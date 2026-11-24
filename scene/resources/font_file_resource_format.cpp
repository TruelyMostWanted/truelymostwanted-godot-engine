/**************************************************************************/
/*  font_file_resource_format.cpp                                         */
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

#include "font_file_resource_format.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "scene/resources/font.h"

namespace FontFileResourceFormat {

static const char *const dynamic_font_extensions[] = { "ttf", "ttc", "otf", "otc", "woff", "woff2", "pfb", "pfm" };
static const char *const bitmap_font_extensions[] = { "fnt", "font" };

static bool _starts_with(const uint8_t *p_data, int64_t p_size, const char *p_magic, int64_t p_magic_size) {
	if (p_size < p_magic_size) {
		return false;
	}
	for (int64_t i = 0; i < p_magic_size; i++) {
		if (p_data[i] != (uint8_t)p_magic[i]) {
			return false;
		}
	}
	return true;
}

DataKind detect_data_kind(const uint8_t *p_data, int64_t p_size) {
	if (p_data == nullptr || p_size < 4) {
		return DATA_UNKNOWN;
	}
	if (_starts_with(p_data, p_size, "\x00\x01\x00\x00", 4) || _starts_with(p_data, p_size, "OTTO", 4) || _starts_with(p_data, p_size, "true", 4) || _starts_with(p_data, p_size, "typ1", 4)) {
		return DATA_SFNT;
	}
	if (_starts_with(p_data, p_size, "ttcf", 4)) {
		return DATA_COLLECTION;
	}
	if (_starts_with(p_data, p_size, "wOFF", 4)) {
		return DATA_WOFF;
	}
	if (_starts_with(p_data, p_size, "wOF2", 4)) {
		return DATA_WOFF2;
	}
	// PFB segment header (0x80, type 1 = ASCII), or plain PFA text.
	if ((p_data[0] == 0x80 && p_data[1] == 0x01) || _starts_with(p_data, p_size, "%!PS-AdobeFont", 14) || _starts_with(p_data, p_size, "%!FontType1", 11)) {
		return DATA_TYPE1;
	}
	return DATA_UNKNOWN;
}

bool is_kind_valid_for_extension(DataKind p_kind, const String &p_extension) {
	if (p_extension == "pfm") {
		return true; // Printer font metrics have no signature to check.
	}
	switch (p_kind) {
		case DATA_SFNT:
		case DATA_COLLECTION:
			return p_extension == "ttf" || p_extension == "ttc" || p_extension == "otf" || p_extension == "otc";
		case DATA_WOFF:
			return p_extension == "woff";
		case DATA_WOFF2:
			return p_extension == "woff2";
		case DATA_TYPE1:
			return p_extension == "pfb";
		case DATA_UNKNOWN:
			break;
	}
	return false;
}

bool is_dynamic_font_extension(const String &p_extension) {
	for (const char *E : dynamic_font_extensions) {
		if (p_extension == E) {
			return true;
		}
	}
	return false;
}

bool is_bitmap_font_extension(const String &p_extension) {
	for (const char *E : bitmap_font_extensions) {
		if (p_extension == E) {
			return true;
		}
	}
	return false;
}

bool is_imported_path(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		// In the editor, a `res://` file without `.import` is about to be imported (or is read by
		// another importer as a dependency): leave it to the import system.
		return p_path.begins_with("res://") && Engine::get_singleton()->is_editor_hint();
	}
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(import_path) != OK) {
		return true; // Unreadable `.import` file: leave the path to the importer, which reports the error.
	}
	const String importer = config->get_value("remap", "importer", String());
	return importer != "keep" && importer != "skip";
}

} // namespace FontFileResourceFormat

using namespace FontFileResourceFormat;

// ResourceFormatLoaderFontFile

Ref<Resource> ResourceFormatLoaderFontFile::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	const String extension = p_path.get_extension().to_lower();
	Ref<FontFile> font;
	font.instantiate();

	if (is_bitmap_font_extension(extension)) {
		const Error err = font->load_bitmap_font(p_path);
		if (err != OK) {
			if (r_error) {
				*r_error = err;
			}
			ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot load BMFont: %s.", p_path));
		}
	} else {
		Error err = OK;
		const Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path, &err);
		ERR_FAIL_COND_V_MSG(err != OK || data.is_empty(), Ref<Resource>(), vformat("Cannot read font file: %s.", p_path));
		if (extension != "pfm" && detect_data_kind(data.ptr(), data.size()) == DATA_UNKNOWN) {
			if (r_error) {
				*r_error = ERR_FILE_UNRECOGNIZED;
			}
			ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Unrecognized font data (not TrueType, OpenType, WOFF, WOFF2 or Type 1): %s.", p_path));
		}
		font->set_data(data);
	}

	if (r_error) {
		*r_error = OK;
	}
	return font;
}

void ResourceFormatLoaderFontFile::get_recognized_extensions(List<String> *p_extensions) const {
	for (const char *E : dynamic_font_extensions) {
		p_extensions->push_back(E);
	}
	for (const char *E : bitmap_font_extensions) {
		p_extensions->push_back(E);
	}
}

bool ResourceFormatLoaderFontFile::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!ResourceFormatLoader::recognize_path(p_path, p_for_type)) {
		return false;
	}
	// Imported fonts are loaded by ResourceFormatImporter (same rule as the other source formats).
	return !is_imported_path(p_path);
}

bool ResourceFormatLoaderFontFile::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("FontFile", p_type);
}

String ResourceFormatLoaderFontFile::get_resource_type(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (!is_dynamic_font_extension(extension) && !is_bitmap_font_extension(extension)) {
		return String();
	}
	if (is_imported_path(p_path)) {
		return String();
	}
	return "FontFile";
}

// ResourceFormatSaverFontFile

Error ResourceFormatSaverFontFile::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<FontFile> font = p_resource;
	ERR_FAIL_COND_V(font.is_null(), ERR_INVALID_PARAMETER);

	const String extension = p_path.get_extension().to_lower();
	const PackedByteArray data = font->get_data();
	ERR_FAIL_COND_V_MSG(data.is_empty(), ERR_INVALID_DATA, vformat("Cannot save font to %s: the FontFile has no font data (a BMFont or a font built in code can only be saved as .res, .tres or .fontdata).", p_path));

	const DataKind kind = detect_data_kind(data.ptr(), data.size());
	ERR_FAIL_COND_V_MSG(!is_kind_valid_for_extension(kind, extension), ERR_INVALID_DATA, vformat("Cannot save font to %s: the font data is not in the .%s format (fonts are saved without conversion).", p_path, extension));

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open font file for writing: %s.", p_path));

	file->store_buffer(data.ptr(), data.size());
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

void ResourceFormatSaverFontFile::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	const FontFile *font = Object::cast_to<FontFile>(*p_resource);
	if (font == nullptr) {
		return;
	}
	const PackedByteArray data = font->get_data();
	const DataKind kind = detect_data_kind(data.ptr(), data.size());
	if (kind == DATA_UNKNOWN) {
		return;
	}
	for (const char *E : dynamic_font_extensions) {
		const String extension = E;
		if (extension != "pfm" && is_kind_valid_for_extension(kind, extension)) {
			p_extensions->push_back(extension);
		}
	}
}

bool ResourceFormatSaverFontFile::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<FontFile>(*p_resource) != nullptr;
}

bool ResourceFormatSaverFontFile::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	return is_dynamic_font_extension(p_path.get_extension().to_lower());
}
