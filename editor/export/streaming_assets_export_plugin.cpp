/**************************************************************************/
/*  streaming_assets_export_plugin.cpp                                    */
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

#include "streaming_assets_export_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

// Guards against deep or cyclic folder structures (links are skipped, but junctions may not be detected).
static constexpr int STREAMING_ASSETS_MAX_FOLDER_DEPTH = 64;

// Static helpers.

String StreamingAssetsExportPlugin::expand_template(const String &p_template, const String &p_exe_name) {
	return p_template.replace("{exe_name}", p_exe_name);
}

bool StreamingAssetsExportPlugin::is_valid_target_folder(const String &p_folder) {
	String folder = p_folder.replace_char('\\', '/');
	if (folder.ends_with("/")) {
		folder = folder.substr(0, folder.length() - 1);
	}
	if (folder.is_empty() || folder.is_absolute_path() || folder.contains_char(':')) {
		return false;
	}
	const Vector<String> parts = folder.split("/");
	for (const String &part : parts) {
		if (part.is_empty() || part == "." || part == ".." || part.contains_char('{') || part.contains_char('}')) {
			return false;
		}
	}
	return true;
}

bool StreamingAssetsExportPlugin::is_path_in_folder(const String &p_path, const String &p_folder) {
	String folder = p_folder.simplify_path();
	while (folder.ends_with("/") && !folder.ends_with("://")) {
		folder = folder.substr(0, folder.length() - 1);
	}
	if (folder.is_empty() || folder.ends_with("://")) {
		return false; // Never treat a whole prefix as the streaming folder.
	}
	const String path = p_path.simplify_path();
	return path == folder || path.begins_with(folder + "/");
}

bool StreamingAssetsExportPlugin::is_file_excluded(const String &p_name) {
	return p_name.is_empty() || p_name.begins_with(".") || p_name.ends_with(".import") || p_name.ends_with(".uid");
}

void StreamingAssetsExportPlugin::_collect_files(const String &p_root, const String &p_relative, int p_depth, PackedStringArray &r_files) {
	if (p_depth > STREAMING_ASSETS_MAX_FOLDER_DEPTH) {
		return;
	}
	const String dir_path = p_relative.is_empty() ? p_root : p_root.path_join(p_relative);
	Ref<DirAccess> da = DirAccess::open(dir_path);
	if (da.is_null()) {
		return;
	}

	// `get_files()` and `get_directories()` skip hidden entries and return sorted lists.
	const PackedStringArray dir_files = da->get_files();
	for (const String &name : dir_files) {
		if (!is_file_excluded(name)) {
			r_files.push_back(p_relative.is_empty() ? name : p_relative.path_join(name));
		}
	}

	const PackedStringArray dirs = da->get_directories();
	for (const String &name : dirs) {
		if (is_file_excluded(name) || da->is_link(name)) {
			continue;
		}
		_collect_files(p_root, p_relative.is_empty() ? name : p_relative.path_join(name), p_depth + 1, r_files);
	}
}

PackedStringArray StreamingAssetsExportPlugin::collect_files(const String &p_folder) {
	PackedStringArray result;
	String root = p_folder;
	while (root.length() > 1 && root.ends_with("/") && !root.ends_with("://")) {
		root = root.substr(0, root.length() - 1);
	}
	if (root.is_empty() || !DirAccess::dir_exists_absolute(root)) {
		return result;
	}
	_collect_files(root, String(), 0, result);
	result.sort();
	return result;
}

StreamingAssetsExportPlugin::Placement StreamingAssetsExportPlugin::resolve_placement(const PackedStringArray &p_features, const String &p_export_path, Fallback p_fallback) {
	// Windows and Linux/BSD carry the `pc` feature. macOS carries it as well, but needs
	// the bundle resources (#90): until then it uses the fallback like iOS and visionOS.
	const bool desktop = p_features.has("pc") && !p_features.has("macos");
	// A folder next to a `.zip` would not be part of the archive.
	const bool zip = p_export_path.to_lower().ends_with(".zip");
	if (desktop && !zip) {
		return PLACEMENT_EXTERNAL;
	}
	return p_fallback == FALLBACK_NONE ? PLACEMENT_NONE : PLACEMENT_EMBED;
}

// Settings.

StreamingAssetsExportPlugin::Mode StreamingAssetsExportPlugin::_parse_mode(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		return String(p_value).strip_edges().to_lower() == "pack" ? MODE_PACK : MODE_RAW;
	}
	if (p_value.get_type() == Variant::INT) {
		return int(p_value) == MODE_PACK ? MODE_PACK : MODE_RAW;
	}
	return MODE_RAW;
}

StreamingAssetsExportPlugin::Fallback StreamingAssetsExportPlugin::_parse_fallback(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		return String(p_value).strip_edges().to_lower() == "none" ? FALLBACK_NONE : FALLBACK_EMBED;
	}
	if (p_value.get_type() == Variant::INT) {
		return int(p_value) == FALLBACK_NONE ? FALLBACK_NONE : FALLBACK_EMBED;
	}
	return FALLBACK_EMBED;
}

Variant StreamingAssetsExportPlugin::_get_setting(const char *p_name, const Variant &p_default) const {
	// The settings are registered by StreamingAssetsServer. Reading an unknown setting would warn.
	if (!ProjectSettings::get_singleton()->has_setting(p_name)) {
		return p_default;
	}
	const Ref<EditorExportPreset> preset = get_export_preset();
	const Variant value = EditorExportPlatform::get_project_setting(preset, p_name);
	return value.get_type() == Variant::NIL ? p_default : value;
}

void StreamingAssetsExportPlugin::_add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const {
	const Ref<EditorExportPlatform> platform = get_export_platform();
	if (platform.is_valid()) {
		platform->add_message(p_type, TTR("Streaming Assets"), p_message);
	} else if (p_type == EditorExportPlatform::EXPORT_MESSAGE_ERROR) {
		ERR_PRINT("Streaming Assets: " + p_message);
	} else if (p_type == EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
		WARN_PRINT("Streaming Assets: " + p_message);
	}
}

void StreamingAssetsExportPlugin::_reset() {
	active = false;
	placement = PLACEMENT_NONE;
	source_folder = String();
	target_folder = String();
	export_path = String();
	files.clear();
}

// Export callbacks.

void StreamingAssetsExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	_reset();

	if (!bool(_get_setting(SETTING_ENABLED, false))) {
		return;
	}

	String source = String(_get_setting(SETTING_SOURCE_FOLDER, DEFAULT_SOURCE_FOLDER)).strip_edges().simplify_path();
	while (source.ends_with("/") && !source.ends_with("://")) {
		source = source.substr(0, source.length() - 1);
	}
	if (!source.begins_with("res://") || source.ends_with("://")) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, vformat(TTR("The source folder \"%s\" must be a folder below \"res://\". Streaming assets are not handled."), source));
		return;
	}
	if (!DirAccess::exists(source)) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, vformat(TTR("The source folder \"%s\" does not exist. Nothing to ship."), source));
		return;
	}

	const Mode mode = _parse_mode(_get_setting(SETTING_MODE, "raw"));
	const Fallback fallback = _parse_fallback(_get_setting(SETTING_FALLBACK, "embed"));
	if (mode == MODE_PACK) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, TTR("The \"pack\" mode is not available yet. The folder is exported in \"raw\" mode."));
	}

	PackedStringArray features;
	for (const String &feature : p_features) {
		features.push_back(feature);
	}

	String path = p_path;
	if (path.begins_with("res://") || path.begins_with("user://")) {
		path = ProjectSettings::get_singleton()->globalize_path(path);
	} else if (path.is_relative_path()) {
		path = ProjectSettings::get_singleton()->get_resource_path().path_join(path);
	}
	path = path.simplify_path();

	Placement new_placement = resolve_placement(features, path, fallback);
	if (features.has("macos") || features.has("ios") || features.has("visionos")) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, TTR("App bundles are not supported yet. The fallback is used."));
	} else if (features.has("pc") && path.to_lower().ends_with(".zip")) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, TTR("ZIP exports cannot hold an external folder. The fallback is used."));
	}

	if (new_placement == PLACEMENT_EXTERNAL) {
		const String exe_name = path.get_file().get_basename();
		const String target = expand_template(String(_get_setting(SETTING_TARGET_FOLDER, DEFAULT_TARGET_FOLDER)).strip_edges(), exe_name).replace_char('\\', '/');
		if (!is_valid_target_folder(target)) {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, vformat(TTR("The target folder \"%s\" must be a relative folder without \".\", \"..\" or unknown placeholders. The folder is exported like any other folder."), target));
			return;
		}
		target_folder = target.trim_suffix("/");
	}

	source_folder = source;
	export_path = path;
	placement = new_placement;
	files = collect_files(source_folder);
	active = true;

	uint64_t total_size = 0;
	for (const String &file : files) {
		const int64_t size = FileAccess::get_size(source_folder.path_join(file));
		if (size > 0) {
			total_size += uint64_t(size);
		}
	}
	const String summary = vformat(TTR("%d files, %s"), files.size(), String::humanize_size(total_size));
	switch (placement) {
		case PLACEMENT_EXTERNAL: {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, vformat(TTR("raw: %s are copied to \"%s\" next to the export."), summary, target_folder));
		} break;
		case PLACEMENT_EMBED: {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, vformat(TTR("embed: %s are embedded as raw files into the main pack."), summary));
		} break;
		case PLACEMENT_NONE: {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, vformat(TTR("none: %s are not shipped with this export."), summary));
		} break;
	}
}

void StreamingAssetsExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	// Drops the file, its `.import` remap and its imported artifacts from the pack.
	if (active && is_path_in_folder(p_path, source_folder)) {
		skip();
	}
}

bool StreamingAssetsExportPlugin::_begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	// Runs at the start of every `export_project_files()` call, right before the extra files
	// of all plugins are written. Files added in `_export_begin()` would only reach the first call.
	if (active && placement == PLACEMENT_EMBED) {
		for (const String &file : files) {
			const String res_path = source_folder.path_join(file);
			Error err = OK;
			const Vector<uint8_t> data = FileAccess::get_file_as_bytes(res_path, &err);
			if (err != OK) {
				_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, vformat(TTR("Could not read \"%s\". The file is not embedded."), res_path));
				continue;
			}
			add_file(res_path, data, false);
		}
	}
	return false; // No resource customization.
}

void StreamingAssetsExportPlugin::_copy_external() {
	// `_export_end()` also runs after a failed export.
	if (!FileAccess::exists(export_path)) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, vformat(TTR("The export \"%s\" was not created. Streaming assets are not copied."), export_path));
		return;
	}

	const String target_root = export_path.get_base_dir().path_join(target_folder);
	const String source_root = ProjectSettings::get_singleton()->globalize_path(source_folder);

	// Remove a stale copy. `is_valid_target_folder()` guarantees that the target is below the export folder.
	if (DirAccess::dir_exists_absolute(target_root)) {
		Ref<DirAccess> stale = DirAccess::open(target_root);
		if (stale.is_null() || stale->erase_contents_recursive() != OK) {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, vformat(TTR("Could not remove the old content of \"%s\"."), target_root));
		}
	}

	Error err = DirAccess::make_dir_recursive_absolute(target_root);
	if (err != OK) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, vformat(TTR("Could not create the folder \"%s\"."), target_root));
		return;
	}

	int failed = 0;
	for (const String &file : files) {
		const String to = target_root.path_join(file);
		if (DirAccess::make_dir_recursive_absolute(to.get_base_dir()) != OK || DirAccess::copy_absolute(source_root.path_join(file), to) != OK) {
			failed++;
		}
	}
	if (failed > 0) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, vformat(TTR("%d of %d files could not be copied to \"%s\"."), failed, files.size(), target_root));
	}
}

void StreamingAssetsExportPlugin::_export_end() {
	if (active && placement == PLACEMENT_EXTERNAL) {
		_copy_external();
	}
	_reset();
}

void StreamingAssetsExportPlugin::_bind_methods() {
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("expand_template", "template", "exe_name"), &StreamingAssetsExportPlugin::expand_template);
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("is_valid_target_folder", "folder"), &StreamingAssetsExportPlugin::is_valid_target_folder);
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("is_path_in_folder", "path", "folder"), &StreamingAssetsExportPlugin::is_path_in_folder);
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("is_file_excluded", "name"), &StreamingAssetsExportPlugin::is_file_excluded);
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("collect_files", "folder"), &StreamingAssetsExportPlugin::collect_files);
	ClassDB::bind_static_method("StreamingAssetsExportPlugin", D_METHOD("resolve_placement", "features", "export_path", "fallback"), &StreamingAssetsExportPlugin::resolve_placement);

	BIND_ENUM_CONSTANT(MODE_RAW);
	BIND_ENUM_CONSTANT(MODE_PACK);

	BIND_ENUM_CONSTANT(FALLBACK_EMBED);
	BIND_ENUM_CONSTANT(FALLBACK_NONE);

	BIND_ENUM_CONSTANT(PLACEMENT_EXTERNAL);
	BIND_ENUM_CONSTANT(PLACEMENT_EMBED);
	BIND_ENUM_CONSTANT(PLACEMENT_NONE);
}
