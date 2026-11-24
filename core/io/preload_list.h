/**************************************************************************/
/*  preload_list.h                                                        */
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

#include "core/io/files_manifest.h"
#include "core/io/resource.h"
#include "core/variant/typed_array.h"
#include "core/variant/typed_dictionary.h"

// Names resources to keep in memory: direct references, paths and manifests, plus optional
// keys (key -> path). `ResourceDatabase.preload_from_list()` loads them once and holds them.
// All properties are stored, so a list can be saved as `.tres` and named in the project
// setting `filesystem/resources/preload_lists`.
class PreloadList : public Resource {
	GDCLASS(PreloadList, Resource);

	TypedArray<Resource> resources;
	PackedStringArray paths;
	TypedArray<FilesManifest> manifests;
	TypedDictionary<String, String> keys;

protected:
	static void _bind_methods();

public:
	void set_resources(const TypedArray<Resource> &p_resources);
	TypedArray<Resource> get_resources() const;

	void set_paths(const PackedStringArray &p_paths);
	PackedStringArray get_paths() const;

	void set_manifests(const TypedArray<FilesManifest> &p_manifests);
	TypedArray<FilesManifest> get_manifests() const;

	void set_keys(const TypedDictionary<String, String> &p_keys);
	TypedDictionary<String, String> get_keys() const;

	// True if the list names nothing.
	bool is_empty() const;
};
