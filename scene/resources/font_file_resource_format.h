/**************************************************************************/
/*  font_file_resource_format.h                                           */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

// Loads font files that have no import (for example from `user://` or a mod folder) as `FontFile` at runtime:
// dynamic fonts (TrueType, OpenType, WOFF, WOFF2, Type 1) and BMFont. Paths with a `.import` file are left
// to `ResourceFormatImporter`.
class ResourceFormatLoaderFontFile : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderFontFile, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	// Narrows the extension list: paths with a `.import` file are not recognized, so the importer keeps precedence.
	// The loader still recognizes paths by extension only, so it stays in ResourceLoader's extension index.
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

// Writes the font data of a `FontFile` back to a dynamic font file, byte for byte. The data must already be in the
// format of the target extension: no conversion.
class ResourceFormatSaverFontFile : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverFontFile, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	// Only the extensions that fit the font's data (none for a font without data), for save dialogs.
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	// Every dynamic font extension, so that `save()` can explain why a font does not fit the target format.
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};

namespace FontFileResourceFormat {

enum DataKind {
	DATA_UNKNOWN,
	DATA_SFNT, // TrueType or OpenType, one font.
	DATA_COLLECTION, // TrueType or OpenType collection (`ttcf`).
	DATA_WOFF,
	DATA_WOFF2,
	DATA_TYPE1, // PostScript Type 1 (PFB or PFA).
};

// Detects the font format from the first bytes of the data.
DataKind detect_data_kind(const uint8_t *p_data, int64_t p_size);
// True if data of this kind may be stored in a file with this lower-case extension without conversion.
bool is_kind_valid_for_extension(DataKind p_kind, const String &p_extension);
// Extensions read with `FontFile::load_dynamic_font()` (and written by the saver).
bool is_dynamic_font_extension(const String &p_extension);
// Extensions read with `FontFile::load_bitmap_font()` (BMFont).
bool is_bitmap_font_extension(const String &p_extension);

} // namespace FontFileResourceFormat
