/**************************************************************************/
/*  streaming_assets_pack_builder.h                                       */
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
#include "editor/export/editor_export_preset.h"

// Builds the streaming assets folder into the streaming pack (`filesystem/streaming_assets/mode` = `pack`).
// The folder goes through the export pipeline of the preset like a `PCKPackage` description built by
// `PCKPackageExporter`, so imported resources, remaps and UIDs resolve through `res://` once
// `StreamingAssetsServer` mounts the pack at startup. Editor only, main thread only.
class StreamingAssetsPackBuilder : public Object {
	GDCLASS(StreamingAssetsPackBuilder, Object);

	static bool _is_import_skipped(const String &p_path);

protected:
	static void _bind_methods();

public:
	// A description with one entry per file below `p_source_folder`: the target is the `res://` path without
	// `res://`. Hidden files, `.import` and `.uid` files and files whose importer is "skip" are left out.
	// Null if `p_source_folder` is not a folder below `res://`.
	static Ref<PCKPackage> make_description(const String &p_source_folder);
	// The same for a list of files relative to `p_source_folder` (a `res://` folder). Not bound.
	static Ref<PCKPackage> make_description_for_files(const String &p_source_folder, const PackedStringArray &p_files);
	// `<folder of p_export_path>/<p_pack_file>`, or an empty string if `p_pack_file` is not a plain file name.
	static String get_pack_path(const String &p_export_path, const String &p_pack_file);
	// Builds the folder into `p_target_path` with `PCKPackageExporter::export_description_with_preset()`,
	// without the dependencies outside the folder (the main pack ships them).
	static Error build(const Ref<EditorExportPreset> &p_preset, const String &p_source_folder, const String &p_target_path, bool p_debug = false);
};
