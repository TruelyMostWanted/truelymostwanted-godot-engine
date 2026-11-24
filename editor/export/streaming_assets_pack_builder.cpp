/**************************************************************************/
/*  streaming_assets_pack_builder.cpp                                     */
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

#include "streaming_assets_pack_builder.h"

#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "editor/export/pck_package_exporter.h"
#include "editor/export/streaming_assets_export_plugin.h"

bool StreamingAssetsPackBuilder::_is_import_skipped(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		return false;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(import_path) != OK) {
		return false;
	}
	// The export never writes such a file, so the pack build would report it as not exported.
	return String(config->get_value("remap", "importer", String())) == "skip";
}

Ref<PCKPackage> StreamingAssetsPackBuilder::make_description(const String &p_source_folder) {
	String folder = p_source_folder.strip_edges().simplify_path();
	while (folder.ends_with("/") && !folder.ends_with("://")) {
		folder = folder.substr(0, folder.length() - 1);
	}
	ERR_FAIL_COND_V_MSG(!folder.begins_with("res://") || folder.ends_with("://"), Ref<PCKPackage>(), vformat("The streaming assets folder \"%s\" must be a folder below \"res://\".", p_source_folder));

	return make_description_for_files(folder, StreamingAssetsExportPlugin::collect_files(folder));
}

Ref<PCKPackage> StreamingAssetsPackBuilder::make_description_for_files(const String &p_source_folder, const PackedStringArray &p_files) {
	Ref<PCKPackage> description;
	description.instantiate();
	for (const String &file : p_files) {
		const String source = p_source_folder.path_join(file);
		if (_is_import_skipped(source)) {
			continue;
		}
		description->add_file(source.trim_prefix("res://"), source);
	}
	return description;
}

String StreamingAssetsPackBuilder::get_pack_path(const String &p_export_path, const String &p_pack_file) {
	const String pack_file = p_pack_file.strip_edges();
	if (pack_file.is_empty() || !pack_file.is_valid_filename() || p_export_path.is_empty()) {
		return String();
	}
	return p_export_path.replace_char('\\', '/').get_base_dir().path_join(pack_file);
}

Error StreamingAssetsPackBuilder::build(const Ref<EditorExportPreset> &p_preset, const String &p_source_folder, const String &p_target_path, bool p_debug) {
	ERR_FAIL_COND_V_MSG(p_preset.is_null(), ERR_INVALID_PARAMETER, "The export preset is null.");
	ERR_FAIL_COND_V_MSG(p_target_path.is_empty(), ERR_INVALID_PARAMETER, "The target path of the streaming pack is empty.");
	const Ref<PCKPackage> description = make_description(p_source_folder);
	if (description.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	return PCKPackageExporter::export_description_with_preset(description, p_preset, p_target_path, p_debug, false);
}

void StreamingAssetsPackBuilder::_bind_methods() {
	ClassDB::bind_static_method("StreamingAssetsPackBuilder", D_METHOD("make_description", "source_folder"), &StreamingAssetsPackBuilder::make_description);
	ClassDB::bind_static_method("StreamingAssetsPackBuilder", D_METHOD("get_pack_path", "export_path", "pack_file"), &StreamingAssetsPackBuilder::get_pack_path);
	ClassDB::bind_static_method("StreamingAssetsPackBuilder", D_METHOD("build", "preset", "source_folder", "target_path", "debug"), &StreamingAssetsPackBuilder::build, DEFVAL(false));
}
