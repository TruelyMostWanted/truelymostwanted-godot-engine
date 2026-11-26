/**************************************************************************/
/*  video_stream_webm_resource_format.h                                   */
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

#include "video_stream_webm.h"

// Loads `.webm` files as VideoStreamWebm, also outside the project (`user://`, mod folders, absolute paths).
// Extends the module's ResourceFormatLoaderWebm with the import rule of the other runtime loaders: files with
// an import (a `.import` file whose importer is not "keep" or "skip") are left to ResourceFormatImporter.
// It also accepts every base class of VideoStreamWebm as type hint and checks the EBML header before it
// creates the stream. There is no saver: the engine has no WebM encoder.
class ResourceFormatLoaderVideoStreamWebm : public ResourceFormatLoaderWebm {
	GDSOFTCLASS(ResourceFormatLoaderVideoStreamWebm, ResourceFormatLoaderWebm);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	// Narrows the extension list: imported paths are not recognized, so the importer keeps precedence.
	// The loader still recognizes paths by extension only, so it stays in ResourceLoader's extension index.
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

	// True if `p_path` has a `.import` file whose importer imports it (anything but `keep` and `skip`), or if
	// it is a `res://` file in the editor that an importer for its extension is about to import.
	static bool is_imported_path(const String &p_path);
};
