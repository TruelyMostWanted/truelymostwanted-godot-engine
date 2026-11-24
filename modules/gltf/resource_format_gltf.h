/**************************************************************************/
/*  resource_format_gltf.h                                                */
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

#include "gltf_document.h"
#include "gltf_state.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

// Loads `.gltf` and `.glb` files as a PackedScene when the editor does not import them:
// `user://`, absolute paths, and `res://` files without an `.import` file or kept as-is ("keep").
// Imported files keep loading through ResourceFormatImporter, which is asked first.
class ResourceFormatLoaderGLTF : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderGLTF, ResourceFormatLoader);

protected:
	virtual void _get_extensions(List<String> *p_extensions) const;
	virtual Ref<GLTFDocument> _create_document() const;
	virtual Ref<GLTFState> _create_state() const;
	virtual uint32_t _get_import_flags() const { return 0; }

	bool _has_extension(const String &p_path) const;

public:
	// Project settings under `filesystem/scene_formats/`, shared by the glTF and FBX loaders.
	static void define_project_settings();
	// True if `p_path` has an `.import` file with an importer that produces a resource (not "keep" or "skip").
	static bool is_handled_by_importer(const String &p_path);

	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const override;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

// Saves a PackedScene as `.gltf` (JSON with an external `.bin` buffer and image files) or `.glb` (one binary file).
class ResourceFormatSaverGLTF : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverGLTF, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};
