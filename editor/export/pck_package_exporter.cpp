/**************************************************************************/
/*  pck_package_exporter.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "pck_package_exporter.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/pck_package_server.h"
#include "core/io/pck_packer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_platform.h"
#include "editor/file_system/editor_file_system.h"

int PCKPackageExporter::running_exports = 0;
int PCKPackageExporter::building = 0;
PCKPackageExporter::CommandLineRequest PCKPackageExporter::command_line_request;

namespace {

// `EditorExportPlatform::ExportNotifier` is protected. This type is never instantiated; it only names the
// notifier, which calls `_export_begin()`/`_export_end()` of every export plugin like `export_pack()` does.
class PCKPackageExporterPlatformAccess : public EditorExportPlatform {
public:
	using Notifier = EditorExportPlatform::ExportNotifier;
};

struct PackState {
	Ref<PCKPacker> packer;
	// `res://` source paths whose exported files are written: the description's sources and their dependencies.
	HashSet<String> wanted;
	// Sources whose files are encrypted because their entry asks for it.
	HashSet<String> encrypted_sources;
	// Pack paths (without `res://`) of entries copied as they are. They win over exported files.
	HashSet<String> raw_paths;
	HashSet<String> written;
	HashSet<String> exported_sources;
};

Error _save_exported_file(const Ref<EditorExportPreset> &p_preset, void *p_userdata, const EditorExportPlatform::SaveFileInfo &p_info, const Vector<uint8_t> &p_data) {
	PackState *state = static_cast<PackState *>(p_userdata);
	const String source = ResourceUID::ensure_path(p_info.source_path).simplify_path();
	// Autoloads, `project.binary`, the UID and class caches and the plugins' global files belong to the main pack.
	if (!state->wanted.has(source)) {
		return OK;
	}
	const String path = EditorExportPlatform::simplify_path(p_info.path);
	const String pack_path = path.trim_prefix("res://");
	if (state->raw_paths.has(pack_path) || state->written.has(pack_path)) {
		return OK;
	}
	const bool encrypt = PCKPackageExporter::should_encrypt(p_preset, path) || state->encrypted_sources.has(source);
	const Error err = state->packer->add_file_from_buffer(pack_path, p_data, encrypt);
	if (err != OK) {
		return err;
	}
	state->written.insert(pack_path);
	state->exported_sources.insert(source);
	return OK;
}

bool _is_sidecar(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	return extension == "import" || extension == "remap" || extension == "uid";
}

} // namespace

PCKPackageExporter::Plan PCKPackageExporter::make_plan(const Ref<PCKPackage> &p_description) {
	Plan plan;
	if (p_description.is_null()) {
		plan.errors.push_back("The description is null.");
		return plan;
	}

	HashSet<String> sources;
	Vector<String> sidecars;
	for (const KeyValue<String, PCKPackage::Entry> &kv : p_description->get_entry_map()) {
		const PCKPackage::Entry &entry = kv.value;
		if (entry.removal) {
			plan.removals.push_back(kv.key);
			continue;
		}
		if (entry.delta) {
			plan.errors.push_back(vformat("\"%s\" is a delta patch: packs with delta patches can't be rebuilt.", kv.key));
			continue;
		}
		if (entry.source_type != PCKPackage::SOURCE_FILE || !entry.source_path.begins_with("res://")) {
			plan.raw_files.push_back(kv.key);
			continue;
		}
		const String source = entry.source_path.simplify_path();
		if (source.trim_prefix("res://") != kv.key) {
			plan.errors.push_back(vformat("\"%s\": the target must be the source path \"%s\", because the export keeps \"res://\" paths. Rename the target or copy the source.", kv.key, source.trim_prefix("res://")));
			continue;
		}
		if (_is_sidecar(source)) {
			sidecars.push_back(kv.key);
			continue;
		}
		sources.insert(source);
		plan.export_sources.push_back(source);
	}

	// The export writes `.import`, `.remap` and `.uid` files for their source. Listed alone, they would be
	// exported without the file they describe.
	for (const String &sidecar : sidecars) {
		if (sources.has("res://" + sidecar.get_basename())) {
			plan.skipped.push_back(sidecar);
		} else {
			plan.errors.push_back(vformat("\"%s\": add its source \"%s\" instead; the export writes this file for it.", sidecar, sidecar.get_basename()));
		}
	}
	return plan;
}

Dictionary PCKPackageExporter::plan_description(const Ref<PCKPackage> &p_description) {
	const Plan plan = make_plan(p_description);
	Dictionary result;
	result["export_sources"] = plan.export_sources;
	result["raw_files"] = plan.raw_files;
	result["removals"] = plan.removals;
	result["skipped"] = plan.skipped;
	result["errors"] = plan.errors;
	return result;
}

bool PCKPackageExporter::matches_encryption_filters(const String &p_path, const PackedStringArray &p_include_filters, const PackedStringArray &p_exclude_filters) {
	const String local = p_path.trim_prefix("res://");
	bool encrypt = false;
	for (const String &filter : p_include_filters) {
		if (p_path.matchn(filter) || local.matchn(filter)) {
			encrypt = true;
			break;
		}
	}
	if (!encrypt) {
		return false;
	}
	for (const String &filter : p_exclude_filters) {
		if (p_path.matchn(filter) || local.matchn(filter)) {
			return false;
		}
	}
	return true;
}

bool PCKPackageExporter::should_encrypt(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	if (p_preset.is_null() || !p_preset->get_enc_pck()) {
		return false;
	}
	const String path = p_path.begins_with("res://") ? p_path : "res://" + p_path;
	return matches_encryption_filters(path, p_preset->get_enc_in_filters(), p_preset->get_enc_ex_filters());
}

String PCKPackageExporter::export_target_to_res_path(const String &p_export_path, const String &p_project_dir) {
	const String path = p_export_path.strip_edges().replace_char('\\', '/');
	if (path.is_empty()) {
		return String();
	}
	if (path.begins_with("res://")) {
		const String simplified = path.simplify_path();
		return simplified.begins_with("res://") ? simplified : String();
	}
	if (path.contains("://")) {
		return String();
	}
	if (path.is_relative_path()) {
		// Export paths are relative to the project folder.
		const String simplified = path.simplify_path();
		if (simplified.is_empty() || simplified == "." || simplified == ".." || simplified.begins_with("../")) {
			return String();
		}
		return "res://" + simplified;
	}
	String project_dir = p_project_dir.replace_char('\\', '/').simplify_path();
	if (project_dir.is_empty()) {
		return String();
	}
	if (!project_dir.ends_with("/")) {
		project_dir += "/";
	}
	const String simplified = path.simplify_path();
	// Windows paths are case-insensitive; a false match only skips a file that is an export target elsewhere.
	if (!simplified.to_lower().begins_with(project_dir.to_lower())) {
		return String();
	}
	const String relative = simplified.substr(project_dir.length());
	return relative.is_empty() ? String() : "res://" + relative;
}

PackedStringArray PCKPackageExporter::get_export_target_paths() {
	PackedStringArray paths;
	EditorExport *export_singleton = EditorExport::get_singleton();
	if (!export_singleton) {
		return paths;
	}
	const String project_dir = ProjectSettings::get_singleton()->get_resource_path();
	for (int i = 0; i < export_singleton->get_export_preset_count(); i++) {
		const Ref<EditorExportPreset> preset = export_singleton->get_export_preset(i);
		if (preset.is_null()) {
			continue;
		}
		const String target = export_target_to_res_path(preset->get_export_path(), project_dir);
		if (target.is_empty()) {
			continue;
		}
		if (!paths.has(target)) {
			paths.push_back(target);
		}
		// An executable export writes its pack next to it.
		const String extension = target.get_extension().to_lower();
		if (extension != "pck" && extension != "zip") {
			const String pack = target.get_basename() + ".pck";
			if (!paths.has(pack)) {
				paths.push_back(pack);
			}
		}
	}
	return paths;
}

bool PCKPackageExporter::is_export_target(const String &p_path) {
	const String path = p_path.begins_with("res://") ? p_path.simplify_path() : "res://" + p_path.simplify_path();
	return get_export_target_paths().has(path);
}

PackedStringArray PCKPackageExporter::get_preset_names() {
	PackedStringArray names;
	EditorExport *export_singleton = EditorExport::get_singleton();
	if (!export_singleton) {
		return names;
	}
	for (int i = 0; i < export_singleton->get_export_preset_count(); i++) {
		const Ref<EditorExportPreset> preset = export_singleton->get_export_preset(i);
		if (preset.is_valid()) {
			names.push_back(preset->get_name());
		}
	}
	return names;
}

Ref<EditorExportPreset> PCKPackageExporter::find_preset(const String &p_name) {
	EditorExport *export_singleton = EditorExport::get_singleton();
	if (!export_singleton) {
		return Ref<EditorExportPreset>();
	}
	for (int i = 0; i < export_singleton->get_export_preset_count(); i++) {
		const Ref<EditorExportPreset> preset = export_singleton->get_export_preset(i);
		if (preset.is_valid() && preset->get_name() == p_name) {
			return preset;
		}
	}
	return Ref<EditorExportPreset>();
}

bool PCKPackageExporter::is_exporting() {
	return running_exports > 0;
}

bool PCKPackageExporter::is_building() {
	return building > 0;
}

void PCKPackageExporter::notify_export_begin() {
	running_exports++;
}

void PCKPackageExporter::notify_export_end() {
	if (running_exports > 0) {
		running_exports--;
	}
}

Ref<EditorExportPreset> PCKPackageExporter::_make_pack_preset(const Ref<EditorExportPreset> &p_preset, const PackedStringArray &p_sources) {
	Ref<EditorExportPlatform> platform = p_preset->get_platform();
	ERR_FAIL_COND_V(platform.is_null(), Ref<EditorExportPreset>());
	// Never added to EditorExport, so `export_presets.cfg` keeps the original preset only.
	Ref<EditorExportPreset> preset = platform->create_preset();
	ERR_FAIL_COND_V(preset.is_null(), Ref<EditorExportPreset>());

	preset->set_name(p_preset->get_name());
	preset->set_dedicated_server(p_preset->is_dedicated_server());
	// The include filter adds files that are not part of the description; the exclude filter still applies.
	preset->set_include_filter(String());
	preset->set_exclude_filter(p_preset->get_exclude_filter());
	preset->set_custom_features(p_preset->get_custom_features());
	preset->set_export_path(p_preset->get_export_path());
	preset->set_enc_in_filters_str(p_preset->get_enc_in_filters_str());
	preset->set_enc_ex_filters_str(p_preset->get_enc_ex_filters_str());
	preset->set_enc_pck(p_preset->get_enc_pck());
	preset->set_enc_directory(p_preset->get_enc_directory());
	preset->set_seed(p_preset->get_seed());
	preset->set_script_encryption_key(p_preset->get_script_encryption_key());
	preset->set_script_export_mode(p_preset->get_script_export_mode());
	for (const KeyValue<StringName, Variant> &kv : p_preset->get_values()) {
		preset->set(kv.key, kv.value);
	}

	HashSet<String> selected;
	for (const String &source : p_sources) {
		selected.insert(source);
	}
	preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_RESOURCES);
	preset->set_selected_files(selected);
	preset->update_value_overrides();
	return preset;
}

void PCKPackageExporter::_add_dependencies(const String &p_path, HashSet<String> &r_paths) {
	// The same walk as `EditorExportPlatform::_export_find_dependencies()`.
	const String path = ResourceUID::ensure_path(p_path).simplify_path();
	if (r_paths.has(path)) {
		return;
	}
	r_paths.insert(path);
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (!file_system) {
		return;
	}
	int file_index = -1;
	EditorFileSystemDirectory *dir = file_system->find_file(path, &file_index);
	if (!dir) {
		return;
	}
	const Vector<String> deps = dir->get_file_deps(file_index);
	for (const String &dep : deps) {
		_add_dependencies(dep, r_paths);
	}
}

Error PCKPackageExporter::export_description(const Ref<PCKPackage> &p_description, const String &p_preset_name, const String &p_target_path, bool p_debug, bool p_include_dependencies) {
	const Ref<EditorExportPreset> preset = find_preset(p_preset_name);
	ERR_FAIL_COND_V_MSG(preset.is_null(), ERR_DOES_NOT_EXIST, vformat("There is no export preset \"%s\". Presets: %s.", p_preset_name, String(", ").join(get_preset_names())));
	return export_description_with_preset(p_description, preset, p_target_path, p_debug, p_include_dependencies);
}

Error PCKPackageExporter::export_description_with_preset(const Ref<PCKPackage> &p_description, const Ref<EditorExportPreset> &p_preset, const String &p_target_path, bool p_debug, bool p_include_dependencies) {
	ERR_FAIL_COND_V_MSG(p_description.is_null(), ERR_INVALID_PARAMETER, "The pack description is null.");
	ERR_FAIL_COND_V_MSG(p_preset.is_null(), ERR_INVALID_PARAMETER, "The export preset is null.");
	ERR_FAIL_COND_V_MSG(p_target_path.is_empty(), ERR_INVALID_PARAMETER, "The target path of the pack is empty.");
	ERR_FAIL_COND_V_MSG(building > 0, ERR_BUSY, "A pack description is already being exported.");
	ERR_FAIL_NULL_V_MSG(EditorFileSystem::get_singleton(), ERR_UNCONFIGURED, "Pack descriptions can only be exported in the editor.");
	ERR_FAIL_COND_V_MSG(EditorFileSystem::get_singleton()->is_scanning(), ERR_BUSY, "The editor is scanning the project: export the pack description when the scan is done.");
	Ref<EditorExportPlatform> platform = p_preset->get_platform();
	ERR_FAIL_COND_V_MSG(platform.is_null(), ERR_UNCONFIGURED, vformat("Export preset \"%s\" doesn't have a matching platform.", p_preset->get_name()));
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	ERR_FAIL_COND_V_MSG(server && server->is_mounted(p_target_path), ERR_BUSY, vformat("Pack \"%s\" is mounted: unmount it before building over it.", p_target_path));

	const Plan plan = make_plan(p_description);
	ERR_FAIL_COND_V_MSG(!plan.errors.is_empty(), ERR_INVALID_PARAMETER, vformat("Can't export pack description to \"%s\": %s", p_target_path, String(" ").join(plan.errors)));

	const HashMap<String, PCKPackage::Entry> &entries = p_description->get_entry_map();
	PackedStringArray missing;
	for (const String &source : plan.export_sources) {
		if (!FileAccess::exists(source)) {
			missing.push_back(source);
		}
	}
	for (const String &path : plan.raw_files) {
		const PCKPackage::Entry &entry = entries[path];
		if (entry.source_type == PCKPackage::SOURCE_FILE && !FileAccess::exists(entry.source_path)) {
			missing.push_back(entry.source_path);
		}
	}
	ERR_FAIL_COND_V_MSG(!missing.is_empty(), ERR_FILE_NOT_FOUND, vformat("Can't export pack description to \"%s\": %d source files are missing: %s.", p_target_path, missing.size(), String(", ").join(missing)));

	PackState state;
	for (const String &source : plan.export_sources) {
		if (p_include_dependencies) {
			_add_dependencies(source, state.wanted);
		} else {
			state.wanted.insert(source);
		}
		if (entries[source.trim_prefix("res://")].encrypted) {
			state.encrypted_sources.insert(source);
		}
	}
	for (const String &path : plan.raw_files) {
		state.raw_paths.insert(path);
	}

	Vector<uint8_t> key = p_preset->resolve_script_encryption_key();
	if (key.size() != 32) {
		PCKPackage::decode_key(String(), key);
	}
	const String key_hex = String::hex_encode_buffer(key.ptr(), key.size());

	const String base_dir = p_target_path.get_base_dir();
	if (!base_dir.is_empty() && !DirAccess::exists(base_dir)) {
		DirAccess::make_dir_recursive_absolute(base_dir);
	}

	state.packer.instantiate();
	Error err = state.packer->pck_start(p_target_path, p_description->get_alignment(), key_hex, p_preset->get_enc_directory() || p_description->is_encrypted_directory());
	if (err != OK) {
		return err;
	}

	if (!plan.export_sources.is_empty()) {
		const Ref<EditorExportPreset> pack_preset = _make_pack_preset(p_preset, plan.export_sources);
		if (pack_preset.is_null()) {
			err = ERR_CANT_CREATE;
		} else {
			building++;
			if (running_exports > 0) {
				// Inside another export: the plugins already hold the state of that export.
				err = platform->export_project_files(pack_preset, p_debug, &_save_exported_file, nullptr, &state);
			} else {
				PCKPackageExporterPlatformAccess::Notifier notifier(*platform.ptr(), pack_preset, p_debug, p_target_path, 0);
				err = platform->export_project_files(pack_preset, p_debug, &_save_exported_file, nullptr, &state);
			}
			building--;
		}
	}

	PackedStringArray not_exported;
	if (err == OK) {
		for (const String &source : plan.export_sources) {
			if (!state.exported_sources.has(source)) {
				not_exported.push_back(source);
			}
		}
		if (!not_exported.is_empty()) {
			err = ERR_FILE_MISSING_DEPENDENCIES;
		}
	}

	for (int i = 0; err == OK && i < plan.raw_files.size(); i++) {
		const String &path = plan.raw_files[i];
		Vector<uint8_t> data;
		err = p_description->read_entry(path, data);
		if (err == OK) {
			const bool encrypt = entries[path].encrypted || should_encrypt(p_preset, path);
			err = state.packer->add_file_from_buffer(path, data, encrypt);
		}
	}
	for (int i = 0; err == OK && i < plan.removals.size(); i++) {
		err = state.packer->add_file_removal(plan.removals[i]);
	}

	const Error flush_err = state.packer->flush();
	state.packer.unref();
	if (err == OK) {
		err = flush_err;
	}
	if (err != OK) {
		DirAccess::remove_absolute(p_target_path);
	}
	ERR_FAIL_COND_V_MSG(!not_exported.is_empty(), err, vformat("Can't export pack description to \"%s\": the export wrote nothing for %s. Check the preset's exclude filter and the export plugins that skip files.", p_target_path, String(", ").join(not_exported)));
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't export pack description to \"%s\" with preset \"%s\".", p_target_path, p_preset->get_name()));
	return OK;
}

void PCKPackageExporter::request_command_line_export(const String &p_preset, const String &p_description, const String &p_target, bool p_debug) {
	command_line_request.preset = p_preset;
	command_line_request.description = p_description;
	command_line_request.target = p_target;
	command_line_request.debug = p_debug;
	command_line_request.pending = true;
}

bool PCKPackageExporter::has_command_line_request() {
	return command_line_request.pending;
}

Error PCKPackageExporter::run_command_line_request() {
	ERR_FAIL_COND_V(!command_line_request.pending, ERR_UNCONFIGURED);
	const CommandLineRequest request = command_line_request;
	command_line_request = CommandLineRequest();

	ERR_FAIL_COND_V_MSG(request.target.is_empty(), ERR_INVALID_PARAMETER, "--export-pack-description: missing the path of the pack to write.");
	const String description_path = ResourceUID::ensure_path(request.description);
	Ref<PCKPackage> description = ResourceLoader::load(description_path, "PCKPackage");
	ERR_FAIL_COND_V_MSG(description.is_null(), ERR_FILE_CANT_OPEN, vformat("--export-pack-description: can't load the pack description \"%s\".", request.description));
	String target = request.target;
	if (target.is_relative_path() && !target.contains("://")) {
		// Like the other export options, a relative path is relative to the project folder.
		target = ProjectSettings::get_singleton()->get_resource_path().path_join(target);
	}
	const Error err = export_description(description, request.preset, target, request.debug);
	if (err == OK) {
		print_line(vformat("Exported pack description \"%s\" with preset \"%s\" to \"%s\".", request.description, request.preset, target));
	}
	return err;
}

void PCKPackageExporter::_bind_methods() {
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("export_description", "description", "preset_name", "target_path", "debug", "include_dependencies"), &PCKPackageExporter::export_description, DEFVAL(false), DEFVAL(true));
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("export_description_with_preset", "description", "preset", "target_path", "debug", "include_dependencies"), &PCKPackageExporter::export_description_with_preset, DEFVAL(false), DEFVAL(true));
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("plan_description", "description"), &PCKPackageExporter::plan_description);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("matches_encryption_filters", "path", "include_filters", "exclude_filters"), &PCKPackageExporter::matches_encryption_filters);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("should_encrypt", "preset", "path"), &PCKPackageExporter::should_encrypt);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("export_target_to_res_path", "export_path", "project_dir"), &PCKPackageExporter::export_target_to_res_path);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("get_export_target_paths"), &PCKPackageExporter::get_export_target_paths);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("is_export_target", "path"), &PCKPackageExporter::is_export_target);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("get_preset_names"), &PCKPackageExporter::get_preset_names);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("find_preset", "name"), &PCKPackageExporter::find_preset);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("is_exporting"), &PCKPackageExporter::is_exporting);
	ClassDB::bind_static_method("PCKPackageExporter", D_METHOD("is_building"), &PCKPackageExporter::is_building);
}
