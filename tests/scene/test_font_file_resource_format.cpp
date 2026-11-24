/**************************************************************************/
/*  test_font_file_resource_format.cpp                                    */
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

TEST_FORCE_LINK(test_font_file_resource_format)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/font.h"
#include "scene/resources/font_file_resource_format.h"
#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h" // For freetype.

namespace TestFontFileResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("font_file_resource_format");
	DirAccess::make_dir_recursive_absolute(dir);
	return dir;
}

static void _write_file(const String &p_path, const Vector<uint8_t> &p_data) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_buffer(p_data.ptr(), p_data.size());
}

static void _write_text(const String &p_path, const String &p_text) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_text);
}

// A few bytes with a TrueType signature. Enough for the format checks; not a usable font.
static Vector<uint8_t> _fake_sfnt() {
	Vector<uint8_t> data;
	const uint8_t bytes[] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 };
	for (uint8_t b : bytes) {
		data.push_back(b);
	}
	return data;
}

TEST_CASE("[FontFileResourceFormat] Detect the font data format") {
	using namespace FontFileResourceFormat;

	const uint8_t sfnt[] = { 0x00, 0x01, 0x00, 0x00 };
	const uint8_t otto[] = { 'O', 'T', 'T', 'O' };
	const uint8_t ttcf[] = { 't', 't', 'c', 'f' };
	const uint8_t woff[] = { 'w', 'O', 'F', 'F' };
	const uint8_t woff2[] = { 'w', 'O', 'F', '2' };
	const uint8_t pfb[] = { 0x80, 0x01, 0x10, 0x00 };
	const uint8_t png[] = { 0x89, 'P', 'N', 'G' };

	CHECK(detect_data_kind(sfnt, 4) == DATA_SFNT);
	CHECK(detect_data_kind(otto, 4) == DATA_SFNT);
	CHECK(detect_data_kind(ttcf, 4) == DATA_COLLECTION);
	CHECK(detect_data_kind(woff, 4) == DATA_WOFF);
	CHECK(detect_data_kind(woff2, 4) == DATA_WOFF2);
	CHECK(detect_data_kind(pfb, 4) == DATA_TYPE1);
	CHECK(detect_data_kind(png, 4) == DATA_UNKNOWN);
	CHECK(detect_data_kind(sfnt, 2) == DATA_UNKNOWN);
	CHECK(detect_data_kind(nullptr, 0) == DATA_UNKNOWN);

	CHECK(is_kind_valid_for_extension(DATA_SFNT, "ttf"));
	CHECK(is_kind_valid_for_extension(DATA_SFNT, "otf"));
	CHECK(is_kind_valid_for_extension(DATA_COLLECTION, "ttc"));
	CHECK(is_kind_valid_for_extension(DATA_WOFF2, "woff2"));
	CHECK_FALSE(is_kind_valid_for_extension(DATA_WOFF2, "ttf"));
	CHECK_FALSE(is_kind_valid_for_extension(DATA_SFNT, "woff"));
	CHECK_FALSE(is_kind_valid_for_extension(DATA_UNKNOWN, "ttf"));
	CHECK(is_kind_valid_for_extension(DATA_UNKNOWN, "pfm"));

	CHECK(is_dynamic_font_extension("woff2"));
	CHECK_FALSE(is_dynamic_font_extension("fnt"));
	CHECK(is_bitmap_font_extension("fnt"));
	CHECK(is_bitmap_font_extension("font"));
}

TEST_CASE("[FontFileResourceFormat] Load and save a font file without import") {
	const String dir = _temp_dir();
	const String source = dir.path_join("fake.ttf");
	_write_file(source, _fake_sfnt());

	CHECK(ResourceLoader::get_resource_type(source) == "FontFile");
	CHECK(ResourceLoader::exists(source, "Font"));

	Ref<FontFile> font = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(font.is_valid());
	CHECK(font->get_data() == _fake_sfnt());

	// Same bytes back, also to another extension of the same format.
	const String copy = dir.path_join("copy.otf");
	CHECK(ResourceSaver::save(font, copy) == OK);
	CHECK(FileAccess::get_file_as_bytes(copy) == _fake_sfnt());

	// No conversion: TrueType data does not go into a WOFF2 file.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(font, dir.path_join("copy.woff2")) == ERR_INVALID_DATA);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("copy.woff2")));

	DirAccess::remove_absolute(source);
	DirAccess::remove_absolute(copy);
}

TEST_CASE("[FontFileResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String source = dir.path_join("imported.ttf");
	_write_file(source, _fake_sfnt());
	_write_text(source + ".import", "[remap]\n\nimporter=\"font_data_dynamic\"\ntype=\"FontFile\"\n");

	CHECK(FontFileResourceFormat::is_imported_path(source));
	ResourceFormatLoaderFontFile loader;
	CHECK_FALSE(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source).is_empty());

	// "Keep File (exported as is)" and "Skip File" do not import: the runtime loader takes the file.
	_write_text(source + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK_FALSE(FontFileResourceFormat::is_imported_path(source));
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "FontFile");

	_write_text(source + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK_FALSE(FontFileResourceFormat::is_imported_path(source));
	CHECK(loader.recognize_path(source));

	DirAccess::remove_absolute(source + ".import");
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "FontFile");

	DirAccess::remove_absolute(source);
}

TEST_CASE("[FontFileResourceFormat] Invalid font files and fonts without data") {
	const String dir = _temp_dir();

	// Not a font.
	const String not_a_font = dir.path_join("not_a_font.ttf");
	_write_text(not_a_font, "This is not a font.");
	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(not_a_font, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	// Empty file.
	const String empty = dir.path_join("empty.otf");
	_write_file(empty, Vector<uint8_t>());
	ERR_PRINT_OFF;
	res = ResourceLoader::load(empty, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;
	CHECK(res.is_null());

	// A FontFile without font data cannot be saved as a font file.
	Ref<FontFile> font;
	font.instantiate();
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(font, dir.path_join("no_data.ttf")) == ERR_INVALID_DATA);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("no_data.ttf")));

	DirAccess::remove_absolute(not_a_font);
	DirAccess::remove_absolute(empty);
}

TEST_CASE("[FontFileResourceFormat] Load a BMFont") {
	const String dir = _temp_dir();
	const String source = dir.path_join("bitmap.fnt");
	_write_text(source,
			"info face=\"Test\" size=16 bold=0 italic=0 charset=\"\" unicode=1\n"
			"common lineHeight=16 base=13 scaleW=64 scaleH=64 pages=0 packed=0\n"
			"chars count=0\n");

	Ref<FontFile> font = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(font.is_valid());
	CHECK(font->get_fixed_size() == 16);
	CHECK(font->get_data().is_empty());

	// BMFont has no font data to write back.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(font, dir.path_join("bitmap.ttf")) == ERR_INVALID_DATA);
	ERR_PRINT_ON;

	DirAccess::remove_absolute(source);
}

#ifdef MODULE_FREETYPE_ENABLED
TEST_CASE("[FontFileResourceFormat] Load and save a WOFF2 font through ResourceLoader and ResourceSaver") {
	const Vector<uint8_t> bytes = FileAccess::get_file_as_bytes("thirdparty/fonts/Inter_Regular.woff2");
	REQUIRE(bytes.size() == 111268);

	const String dir = _temp_dir();
	const String source = dir.path_join("Inter_Regular.woff2");
	_write_file(source, bytes);

	Ref<FontFile> font = ResourceLoader::load(source, "FontFile", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(font.is_valid());
	CHECK(font->get_font_name() == "Inter");
	CHECK(font->get_glyph_index(16, 'a', 0) != 0);

	const String copy = dir.path_join("copy.woff2");
	CHECK(ResourceSaver::save(font, copy) == OK);
	CHECK(FileAccess::get_file_as_bytes(copy) == bytes);

	List<String> extensions;
	ResourceSaver::get_recognized_extensions(font, &extensions);
	CHECK(extensions.find("woff2") != nullptr);
	CHECK(extensions.find("ttf") == nullptr);

	DirAccess::remove_absolute(source);
	DirAccess::remove_absolute(copy);
}
#endif // MODULE_FREETYPE_ENABLED

} // namespace TestFontFileResourceFormat
