/**************************************************************************/
/*  system_font_resource_format.cpp                                       */
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

#include "system_font_resource_format.h"

#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/resources/font.h"

// The platforms resolve these names to a default font on purpose (see `OS::get_system_font_path()`).
static bool _is_generic_family(const String &p_family) {
	static const char *generic_families[] = { "sans-serif", "serif", "monospace", "cursive", "fantasy" };
	for (const char *E : generic_families) {
		if (p_family.nocasecmp_to(E) == 0) {
			return true;
		}
	}
	return false;
}

// True if the resolved face belongs to the requested family. Some platforms return another font for a missing name:
// Android scores `fonts.xml` entries by style only, CoreText may fall back to a default font.
static bool _is_family_match(const Ref<Font> &p_face, const String &p_family) {
	if (_is_generic_family(p_family) || p_face->get_font_name().nocasecmp_to(p_family) == 0) {
		return true;
	}
	// Typographic family ("Segoe UI" of "Segoe UI Semibold") and localized family names, which DirectWrite accepts too.
	const Array languages = p_face->get_ot_name_strings().values();
	for (const Variant &language : languages) {
		const Dictionary strings = language;
		if (String(strings.get("family_name", String())).nocasecmp_to(p_family) == 0 || String(strings.get("typographic_family_name", String())).nocasecmp_to(p_family) == 0) {
			return true;
		}
	}
	// Platform aliases (Android's "sans-serif-condensed") resolve to a file with another family name.
	for (const String &E : OS::get_singleton()->get_system_fonts()) {
		if (E.nocasecmp_to(p_family) == 0) {
			return true;
		}
	}
	return false;
}

Ref<SystemFont> SystemFont::find(const String &p_name, int p_weight, bool p_italic, int p_stretch) {
	return find_first(PackedStringArray({ p_name }), p_weight, p_italic, p_stretch);
}

Ref<SystemFont> SystemFont::find_first(const PackedStringArray &p_names, int p_weight, bool p_italic, int p_stretch) {
	for (const String &family : p_names) {
		if (family.is_empty()) {
			continue;
		}
		Ref<SystemFont> font;
		font.instantiate();
		font->set_font_weight(p_weight);
		font->set_font_italic(p_italic);
		font->set_font_stretch(p_stretch);
		font->set_font_names(PackedStringArray({ family })); // Loads the file, after the style is set.
		const Ref<Font> face = font->get_base_font();
		if (face.is_valid() && _is_family_match(face, family)) {
			return font;
		}
	}
	return Ref<SystemFont>();
}

namespace SystemFontResourceFormat {

Error parse_path(const String &p_path, PackedStringArray &r_names, int &r_weight, bool &r_italic, int &r_stretch) {
	ERR_FAIL_COND_V_MSG(!p_path.begins_with(PREFIX), ERR_INVALID_PARAMETER, vformat("\"%s\" is not a \"%s\" path.", p_path, PREFIX));
	r_names.clear();
	r_weight = 400;
	r_italic = false;
	r_stretch = 100;

	const String spec = p_path.trim_prefix(PREFIX);
	const int query = spec.find_char('?');
	for (const String &family : (query < 0 ? spec : spec.substr(0, query)).split(",", false)) {
		const String decoded = family.uri_decode().strip_edges();
		if (!decoded.is_empty()) {
			r_names.push_back(decoded);
		}
	}
	ERR_FAIL_COND_V_MSG(r_names.is_empty(), ERR_INVALID_PARAMETER, vformat("No font family in \"%s\".", p_path));
	if (query < 0) {
		return OK;
	}

	for (const String &option : spec.substr(query + 1).split("&", false)) {
		const String key = option.get_slice("=", 0);
		const String value = option.get_slice("=", 1);
		const int number = value.is_valid_int() ? value.to_int() : -1;
		if (key == "weight" && number >= 100 && number <= 999) {
			r_weight = number;
		} else if (key == "stretch" && number >= 50 && number <= 200) {
			r_stretch = number;
		} else if (key == "italic" && (value == "true" || value == "1" || value == "false" || value == "0")) {
			r_italic = value == "true" || value == "1";
		} else {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, vformat("Invalid option \"%s\" in \"%s\". Valid options: weight=100-999, italic=true/false, stretch=50-200.", option, p_path));
		}
	}
	return OK;
}

} // namespace SystemFontResourceFormat

Ref<Resource> ResourceFormatLoaderSystemFont::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	PackedStringArray families;
	int weight = 400;
	bool italic = false;
	int stretch = 100;
	Error err = SystemFontResourceFormat::parse_path(p_path, families, weight, italic, stretch);
	Ref<SystemFont> font;
	if (err == OK) {
		font = SystemFont::find_first(families, weight, italic, stretch);
		err = font.is_valid() ? OK : ERR_FILE_NOT_FOUND;
	}
	if (r_error) {
		*r_error = err;
	}
	return font;
}

bool ResourceFormatLoaderSystemFont::exists(const String &p_path) const {
	PackedStringArray families;
	int weight = 400;
	bool italic = false;
	int stretch = 100;
	return SystemFontResourceFormat::parse_path(p_path, families, weight, italic, stretch) == OK && SystemFont::find_first(families, weight, italic, stretch).is_valid();
}

bool ResourceFormatLoaderSystemFont::recognize_path(const String &p_path, const String &p_for_type) const {
	return p_path.begins_with(SystemFontResourceFormat::PREFIX) && (p_for_type.is_empty() || handles_type(p_for_type));
}

bool ResourceFormatLoaderSystemFont::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("SystemFont", p_type);
}

String ResourceFormatLoaderSystemFont::get_resource_type(const String &p_path) const {
	return p_path.begins_with(SystemFontResourceFormat::PREFIX) ? "SystemFont" : String();
}
