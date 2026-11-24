/**************************************************************************/
/*  pck_build_plan.h                                                      */
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

#include "core/object/ref_counted.h"

// The result of `PCKPackageExporter::plan_description()`: how the entries of a `PCKPackage` description are
// built through the export pipeline. Read-only after creation.
class PCKBuildPlan : public RefCounted {
	GDCLASS(PCKBuildPlan, RefCounted);

	friend class PCKPackageExporter;

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

protected:
	static void _bind_methods();

public:
	bool is_valid() const { return errors.is_empty(); }
	PackedStringArray get_export_sources() const { return export_sources; }
	PackedStringArray get_raw_files() const { return raw_files; }
	PackedStringArray get_removals() const { return removals; }
	PackedStringArray get_skipped() const { return skipped; }
	PackedStringArray get_errors() const { return errors; }
};
