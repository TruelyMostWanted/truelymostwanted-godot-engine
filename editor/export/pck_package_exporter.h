/**************************************************************************/
/*  pck_package_exporter.h                                                */
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

#pragma once

#include "core/io/pck_package.h"
#include "core/object/object.h"
#include "core/templates/hash_set.h"
#include "editor/export/editor_export_preset.h"

// Builds a `PCKPackage` description through the export pipeline of an export preset. Entries with a `res://`
// source are packed in exported form for the preset's platform (`.import` files, imported artifacts, `.remap`
// files, converted scenes and resources, tokenized scripts), so `load()` works after the pack is mounted in an
// exported game. All other entries are copied as they are, like `PCKPackageServer::build()` does.
// Editor only, main thread only.
class PCKPackageExporter : public Object {
	GDCLASS(PCKPackageExporter, Object);

	struct CommandLineRequest {
		String preset;
		String description;
		String target;
		bool debug = false;
		bool pending = false;
	};

	// Exports started by an `ExportNotifier` (counted by `PCKExportPlugin`), and description builds.
	static int running_exports;
	static int building;
	static CommandLineRequest command_line_request;

	static Ref<EditorExportPreset> _make_pack_preset(const Ref<EditorExportPreset> &p_preset, const PackedStringArray &p_sources);
	static void _add_dependencies(const String &p_path, HashSet<String> &r_paths);

protected:
	static void _bind_methods();

public:
	// How the entries of a description are built.
	struct Plan {
		// `res://` sources that go through the export pipeline (the target equals the source path).
		PackedStringArray export_sources;
		// Pack paths copied as they are: data, files outside `res://`, and files of the base pack.
		PackedStringArray raw_files;
		// Pack paths written as removal entries.
		PackedStringArray removals;
		// Pack paths that are not written because the export writes them for their source (`.import`, `.remap`, `.uid`).
		PackedStringArray skipped;
		// Entries that can't be built; nothing is written if there is one.
		PackedStringArray errors;
	};

	static Plan make_plan(const Ref<PCKPackage> &p_description);
	static Dictionary plan_description(const Ref<PCKPackage> &p_description);

	// The encryption filters of an export preset, applied like the export does (`*` and `?`, case-insensitive,
	// with or without `res://`). An exclude filter wins over an include filter.
	static bool matches_encryption_filters(const String &p_path, const PackedStringArray &p_include_filters, const PackedStringArray &p_exclude_filters);
	// True if the preset encrypts the file at `p_path` in its packs.
	static bool should_encrypt(const Ref<EditorExportPreset> &p_preset, const String &p_path);

	// The `res://` path of an export target (relative to the project folder, `res://` or absolute), or an empty
	// string if the target is outside the project folder.
	static String export_target_to_res_path(const String &p_export_path, const String &p_project_dir);
	// The export targets of all presets inside the project folder, and the `.pck` next to each executable target.
	static PackedStringArray get_export_target_paths();
	static bool is_export_target(const String &p_path);

	static PackedStringArray get_preset_names();
	static Ref<EditorExportPreset> find_preset(const String &p_name);

	// True while an export runs (its `_export_begin()` was called and its `_export_end()` not yet).
	static bool is_exporting();
	// True while a description is built.
	static bool is_building();

	static Error export_description(const Ref<PCKPackage> &p_description, const String &p_preset_name, const String &p_target_path, bool p_debug = false, bool p_include_dependencies = true);
	// The entry point for other editor code. It may be called during another export (for example from an export
	// plugin): the other plugins' `_export_begin()`/`_export_end()` are then not called a second time.
	static Error export_description_with_preset(const Ref<PCKPackage> &p_description, const Ref<EditorExportPreset> &p_preset, const String &p_target_path, bool p_debug = false, bool p_include_dependencies = true);

	// Called by `PCKExportPlugin`.
	static void notify_export_begin();
	static void notify_export_end();

	// `--export-pack-description <preset> <description> <target>`: run by `PCKPackageExporterEditorPlugin` after the first scan.
	static void request_command_line_export(const String &p_preset, const String &p_description, const String &p_target, bool p_debug);
	static bool has_command_line_request();
	static Error run_command_line_request();
};
