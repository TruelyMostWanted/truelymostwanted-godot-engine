/**************************************************************************/
/*  test_system_font_resource_format.cpp                                  */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_system_font_resource_format)

#include "core/io/resource_loader.h"
#include "scene/resources/font.h"
#include "scene/resources/system_font_resource_format.h"

#include "modules/modules_enabled.gen.h" // For freetype.

namespace TestSystemFontResourceFormat {

static const char *MISSING_FAMILY = "Truely Missing Font 365";

TEST_CASE("[SystemFont] Parse sysfont paths") {
	using namespace SystemFontResourceFormat;
	PackedStringArray families;
	int weight = 0;
	bool italic = true;
	int stretch = 0;

	CHECK(parse_path("sysfont://Arial", families, weight, italic, stretch) == OK);
	CHECK(families == PackedStringArray({ "Arial" }));
	CHECK(weight == 400);
	CHECK_FALSE(italic);
	CHECK(stretch == 100);

	CHECK(parse_path("sysfont://Noto%20Sans,DejaVu+Sans?weight=700&italic=true&stretch=75", families, weight, italic, stretch) == OK);
	CHECK(families == PackedStringArray({ "Noto Sans", "DejaVu Sans" }));
	CHECK(weight == 700);
	CHECK(italic);
	CHECK(stretch == 75);

	CHECK(parse_path("sysfont://Arial?italic=0", families, weight, italic, stretch) == OK);
	CHECK_FALSE(italic);

	ERR_PRINT_OFF;
	CHECK(parse_path("sysfont://", families, weight, italic, stretch) == ERR_INVALID_PARAMETER);
	CHECK(parse_path("sysfont://Arial?weight=bold", families, weight, italic, stretch) == ERR_INVALID_PARAMETER);
	CHECK(parse_path("sysfont://Arial?weight=1200", families, weight, italic, stretch) == ERR_INVALID_PARAMETER);
	CHECK(parse_path("sysfont://Arial?size=16", families, weight, italic, stretch) == ERR_INVALID_PARAMETER);
	CHECK(parse_path("res://Arial", families, weight, italic, stretch) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;

	CHECK(ResourceLoader::get_resource_type("sysfont://Arial") == "SystemFont");
}

TEST_CASE("[SystemFont] A missing font is null") {
	ERR_PRINT_OFF; // Linux without fontconfig prints an error for every lookup.
	CHECK(SystemFont::find(MISSING_FAMILY).is_null());
	CHECK(SystemFont::find_first(PackedStringArray({ MISSING_FAMILY, "" })).is_null());
	CHECK(SystemFont::find_first(PackedStringArray()).is_null());
	CHECK_FALSE(ResourceLoader::exists("sysfont://Truely%20Missing%20Font%20365"));
	CHECK(ResourceLoader::load("sysfont://Truely%20Missing%20Font%20365").is_null());
	ERR_PRINT_ON;
}

#if defined(WINDOWS_ENABLED) && defined(MODULE_FREETYPE_ENABLED)
TEST_CASE("[SystemFont] Find an installed font (Windows)") {
	Ref<SystemFont> font = SystemFont::find("Arial");
	REQUIRE(font.is_valid());
	CHECK(font->get_font_names() == PackedStringArray({ "Arial" }));
	CHECK(font->get_base_font().is_valid());
	CHECK(SystemFont::find("arial").is_valid());
	CHECK(SystemFont::find("sans-serif").is_valid());

	Ref<SystemFont> bold_italic = SystemFont::find("Arial", 700, true);
	REQUIRE(bold_italic.is_valid());
	CHECK(bold_italic->get_font_weight() == 700);
	CHECK(bold_italic->get_font_italic());
	CHECK(bold_italic->get_base_font()->get_font_weight() == 700);
	CHECK(bold_italic->get_base_font()->get_font_style().has_flag(TextServer::FONT_ITALIC));

	Ref<SystemFont> first = SystemFont::find_first(PackedStringArray({ MISSING_FAMILY, "Arial" }));
	REQUIRE(first.is_valid());
	CHECK(first->get_font_names() == PackedStringArray({ "Arial" }));
}

TEST_CASE("[SystemFont] Load sysfont paths (Windows)") {
	const String path = "sysfont://Truely%20Missing,Courier%20New?weight=700";
	CHECK(ResourceLoader::exists("sysfont://Arial"));
	CHECK(ResourceLoader::exists("sysfont://Arial", "Font"));

	Ref<SystemFont> font = ResourceLoader::load(path);
	REQUIRE(font.is_valid());
	CHECK(font->get_font_names() == PackedStringArray({ "Courier New" }));
	CHECK(font->get_font_weight() == 700);
	CHECK(font->get_path() == path);

	// Cached by the path.
	CHECK(ResourceLoader::load(path).ptr() == font.ptr());
	CHECK(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE).ptr() != font.ptr());
}
#endif // WINDOWS_ENABLED && MODULE_FREETYPE_ENABLED

} // namespace TestSystemFontResourceFormat
