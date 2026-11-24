/**************************************************************************/
/*  test_image_texture_resource_format.cpp                                */
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

TEST_FORCE_LINK(test_image_texture_resource_format)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/image_save_options.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/image_texture_resource_format.h"
#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h" // For bmp, dds, hdr, jpg, ktx, svg, tga, tinyexr, webp.

namespace TestImageTextureResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("image_texture_resource_format");
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

static void _push_u16(Vector<uint8_t> &r_data, uint32_t p_value) {
	r_data.push_back(p_value & 0xff);
	r_data.push_back((p_value >> 8) & 0xff);
}

static void _push_u32(Vector<uint8_t> &r_data, uint32_t p_value) {
	_push_u16(r_data, p_value & 0xffff);
	_push_u16(r_data, p_value >> 16);
}

// 4x4 RGBA8: top half red, bottom half blue.
static Ref<Image> _make_image() {
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			image->set_pixel(x, y, y < 2 ? Color(1, 0, 0) : Color(0, 0, 1));
		}
	}
	return image;
}

// 64x64 RGBA8 with a pattern, so that lossy encoders produce different sizes at different qualities.
static Ref<Image> _make_pattern_image() {
	Ref<Image> image = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < 64; y++) {
		for (int x = 0; x < 64; x++) {
			image->set_pixel(x, y, Color(((x * 37 + y * 11) % 64) / 63.0, ((x * y) % 64) / 63.0, ((x + y * 53) % 64) / 63.0));
		}
	}
	return image;
}

// Uncompressed 2x2 BMP, 24 bits, bottom-up: bottom row green, top row red.
static Vector<uint8_t> _make_bmp() {
	Vector<uint8_t> data;
	data.push_back('B');
	data.push_back('M');
	_push_u32(data, 54 + 16); // File size.
	_push_u32(data, 0); // Reserved.
	_push_u32(data, 54); // Pixel data offset.
	_push_u32(data, 40); // Info header size.
	_push_u32(data, 2); // Width.
	_push_u32(data, 2); // Height (bottom-up).
	_push_u16(data, 1); // Planes.
	_push_u16(data, 24); // Bits per pixel.
	_push_u32(data, 0); // No compression.
	_push_u32(data, 16); // Image size.
	_push_u32(data, 2835);
	_push_u32(data, 2835);
	_push_u32(data, 0);
	_push_u32(data, 0);
	for (int row = 0; row < 2; row++) {
		for (int x = 0; x < 2; x++) {
			// BGR.
			data.push_back(0);
			data.push_back(row == 0 ? 255 : 0);
			data.push_back(row == 0 ? 0 : 255);
		}
		data.push_back(0); // Row padding to 4 bytes.
		data.push_back(0);
	}
	return data;
}

// Uncompressed true-color 2x2 TGA, 32 bits, top-left origin, all pixels red.
static Vector<uint8_t> _make_tga() {
	Vector<uint8_t> data;
	data.push_back(0); // ID length.
	data.push_back(0); // No color map.
	data.push_back(2); // Uncompressed true-color.
	for (int i = 0; i < 5; i++) {
		data.push_back(0); // Color map specification.
	}
	_push_u16(data, 0); // X origin.
	_push_u16(data, 0); // Y origin.
	_push_u16(data, 2); // Width.
	_push_u16(data, 2); // Height.
	data.push_back(32); // Bits per pixel.
	data.push_back(0x28); // 8 alpha bits, top-left origin.
	for (int i = 0; i < 4; i++) {
		// BGRA.
		data.push_back(0);
		data.push_back(0);
		data.push_back(255);
		data.push_back(255);
	}
	return data;
}

// Flat (not run-length encoded) 2x2 Radiance HDR, all pixels (1, 0, 0).
static Vector<uint8_t> _make_hdr() {
	const CharString header = String("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n").ascii();
	Vector<uint8_t> data;
	for (int i = 0; i < header.length(); i++) {
		data.push_back((uint8_t)header[i]);
	}
	for (int i = 0; i < 4; i++) {
		data.push_back(128);
		data.push_back(0);
		data.push_back(0);
		data.push_back(129);
	}
	return data;
}

// KTX 1.1, 2x2 GL_RGBA8, one level, all pixels red.
static Vector<uint8_t> _make_ktx() {
	const uint8_t identifier[] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };
	Vector<uint8_t> data;
	for (uint8_t b : identifier) {
		data.push_back(b);
	}
	_push_u32(data, 0x04030201); // Endianness.
	_push_u32(data, 0x1401); // glType: GL_UNSIGNED_BYTE.
	_push_u32(data, 1); // glTypeSize.
	_push_u32(data, 0x1908); // glFormat: GL_RGBA.
	_push_u32(data, 0x8058); // glInternalFormat: GL_RGBA8.
	_push_u32(data, 0x1908); // glBaseInternalFormat: GL_RGBA.
	_push_u32(data, 2); // Width.
	_push_u32(data, 2); // Height.
	_push_u32(data, 0); // Depth.
	_push_u32(data, 0); // Array elements.
	_push_u32(data, 1); // Faces.
	_push_u32(data, 1); // Mipmap levels.
	_push_u32(data, 0); // Key/value data.
	_push_u32(data, 16); // Image size.
	for (int i = 0; i < 4; i++) {
		data.push_back(255);
		data.push_back(0);
		data.push_back(0);
		data.push_back(255);
	}
	return data;
}

TEST_CASE("[ImageTextureResourceFormat] Extension lists") {
	using namespace ImageTextureResourceFormat;

	for (const char *E : { "png", "jpg", "jpeg", "webp", "bmp", "tga", "svg", "exr", "hdr" }) {
		CHECK(is_decoded_extension(E));
		CHECK_FALSE(is_container_extension(E));
	}
	for (const char *E : { "ktx", "ktx2", "dds" }) {
		CHECK(is_container_extension(E));
		CHECK_FALSE(is_decoded_extension(E));
	}
	for (const char *E : { "png", "jpg", "jpeg", "webp", "exr", "dds" }) {
		CHECK(is_saved_extension(E));
	}
	for (const char *E : { "bmp", "tga", "svg", "hdr", "ktx", "tres" }) {
		CHECK_FALSE(is_saved_extension(E));
	}
	CHECK(is_decoder_available("png"));
	CHECK(is_encoder_available("png"));
	CHECK_FALSE(is_encoder_available("bmp"));
}

TEST_CASE("[SceneTree][ImageTextureResourceFormat] Save and load PNG as ImageTexture and as Image") {
	const String dir = _temp_dir();
	const String path = dir.path_join("round_trip.png");

	Ref<ImageTexture> texture = ImageTexture::create_from_image(_make_image());
	CHECK(ResourceSaver::save(texture, path) == OK);
	REQUIRE(FileAccess::exists(path));

	CHECK(ResourceLoader::get_resource_type(path) == "ImageTexture");
	CHECK(ResourceLoader::exists(path));
	CHECK(ResourceLoader::exists(path, "Texture2D"));
	CHECK(ResourceLoader::exists(path, "Image"));

	Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_width() == 4);
	CHECK(loaded->get_height() == 4);
	Ref<Image> loaded_image = loaded->get_image();
	REQUIRE(loaded_image.is_valid());
	CHECK(loaded_image->get_pixel(0, 0).is_equal_approx(Color(1, 0, 0)));
	CHECK(loaded_image->get_pixel(3, 3).is_equal_approx(Color(0, 0, 1)));

	Ref<Resource> as_image = ResourceLoader::load(path, "Image", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(as_image.is_valid());
	CHECK(as_image->get_class() == "Image");
	Ref<Image> image = as_image;
	CHECK(image->get_width() == 4);

	// The same file again through the cache.
	Ref<Resource> cached_first = ResourceLoader::load(path);
	Ref<Resource> cached_second = ResourceLoader::load(path);
	CHECK(cached_first.is_valid());
	CHECK(cached_first == cached_second);

	// An Image can be saved directly.
	const String image_path = dir.path_join("from_image.png");
	CHECK(ResourceSaver::save(_make_image(), image_path) == OK);
	Ref<Image> reloaded = ResourceLoader::load(image_path, "Image", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(reloaded.is_valid());
	CHECK(reloaded->get_pixel(1, 1).is_equal_approx(Color(1, 0, 0)));

	DirAccess::remove_absolute(path);
	DirAccess::remove_absolute(image_path);
}

TEST_CASE("[SceneTree][ImageTextureResourceFormat] Save with encoders and options") {
	const String dir = _temp_dir();
	const Ref<Image> pattern = _make_pattern_image();

#ifdef MODULE_JPG_ENABLED
	{
		Ref<ImageTexture> texture = ImageTexture::create_from_image(pattern);
		Ref<ImageSaveOptions> options;
		options.instantiate();
		options->set_jpg_quality(0.1f);
		options->attach_to(texture);
		CHECK(ImageSaveOptions::get_attached(texture) == options);

		const String low = dir.path_join("low.jpg");
		const String high = dir.path_join("high.jpeg");
		CHECK(ResourceSaver::save(texture, low) == OK);
		options->set_jpg_quality(1.0f);
		CHECK(ResourceSaver::save(texture, high) == OK);
		CHECK(FileAccess::get_size(low) < FileAccess::get_size(high));

		ImageSaveOptions::detach_from(texture);
		CHECK(ImageSaveOptions::get_attached(texture).is_null());

		Ref<ImageTexture> loaded = ResourceLoader::load(high, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 64);
		DirAccess::remove_absolute(low);
		DirAccess::remove_absolute(high);
	}
#endif // MODULE_JPG_ENABLED

#ifdef MODULE_WEBP_ENABLED
	{
		Ref<ImageTexture> texture = ImageTexture::create_from_image(pattern);
		const String lossless = dir.path_join("lossless.webp");
		const String lossy = dir.path_join("lossy.webp");
		CHECK(ResourceSaver::save(texture, lossless) == OK);

		Ref<ImageSaveOptions> options;
		options.instantiate();
		options->set_webp_lossy(true);
		options->set_webp_quality(0.1f);
		options->attach_to(texture);
		CHECK(ResourceSaver::save(texture, lossy) == OK);
		CHECK(FileAccess::get_size(lossy) < FileAccess::get_size(lossless));

		Ref<ImageTexture> loaded = ResourceLoader::load(lossless, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_image()->get_pixel(5, 7).is_equal_approx(pattern->get_pixel(5, 7)));
		DirAccess::remove_absolute(lossless);
		DirAccess::remove_absolute(lossy);
	}
#endif // MODULE_WEBP_ENABLED

#ifdef MODULE_TINYEXR_ENABLED
	{
		const String path = dir.path_join("float.exr");
		CHECK(ResourceSaver::save(ImageTexture::create_from_image(_make_image()), path) == OK);
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 4);
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_TINYEXR_ENABLED

#ifdef MODULE_DDS_ENABLED
	{
		const String path = dir.path_join("texture.dds");
		CHECK(ResourceSaver::save(_make_image(), path) == OK);
		// Without hint: ResourceFormatDDS.
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 4);
		// With hint Image: this loader.
		Ref<Image> image = ResourceLoader::load(path, "Image", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(image.is_valid());
		CHECK(image->get_height() == 4);
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_DDS_ENABLED

	// Formats without an encoder are not written.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(ImageTexture::create_from_image(_make_image()), dir.path_join("nope.bmp")) != OK);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("nope.bmp")));
}

TEST_CASE("[SceneTree][ImageTextureResourceFormat] Load formats without an encoder") {
	const String dir = _temp_dir();

#ifdef MODULE_BMP_ENABLED
	{
		const String path = dir.path_join("image.bmp");
		_write_file(path, _make_bmp());
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 2);
		Ref<Image> image = loaded->get_image();
		CHECK(image->get_pixel(0, 0).is_equal_approx(Color(1, 0, 0)));
		CHECK(image->get_pixel(0, 1).is_equal_approx(Color(0, 1, 0)));
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_BMP_ENABLED

#ifdef MODULE_TGA_ENABLED
	{
		const String path = dir.path_join("image.tga");
		_write_file(path, _make_tga());
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_height() == 2);
		CHECK(loaded->get_image()->get_pixel(1, 1).is_equal_approx(Color(1, 0, 0)));
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_TGA_ENABLED

#ifdef MODULE_HDR_ENABLED
	{
		const String path = dir.path_join("image.hdr");
		_write_file(path, _make_hdr());
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 2);
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_HDR_ENABLED

#ifdef MODULE_SVG_ENABLED
	{
		const String path = dir.path_join("image.svg");
		_write_text(path, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"6\"><rect width=\"8\" height=\"6\" fill=\"#ff0000\"/></svg>");
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 8);
		CHECK(loaded->get_height() == 6);
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_SVG_ENABLED

#ifdef MODULE_KTX_ENABLED
	{
		const String path = dir.path_join("image.ktx");
		_write_file(path, _make_ktx());
		// Without hint: ResourceFormatKTX.
		Ref<ImageTexture> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_width() == 2);
		// With hint Image: this loader.
		Ref<Image> image = ResourceLoader::load(path, "Image", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(image.is_valid());
		CHECK(image->get_pixel(0, 0).is_equal_approx(Color(1, 0, 0)));
		DirAccess::remove_absolute(path);
	}
#endif // MODULE_KTX_ENABLED
}

TEST_CASE("[SceneTree][ImageTextureResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String path = dir.path_join("imported.png");
	CHECK(_make_image()->save_png(path) == OK);

	ResourceFormatLoaderImageTexture loader;
	CHECK(loader.recognize_path(path));
	CHECK(loader.get_resource_type(path) == "ImageTexture");

	_write_text(path + ".import", "[remap]\n\nimporter=\"texture\"\ntype=\"CompressedTexture2D\"\n");
	CHECK_FALSE(loader.recognize_path(path));
	CHECK(loader.get_resource_type(path).is_empty());

	// "keep" and "skip" files have no imported resource: the runtime loader takes them.
	_write_text(path + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK(loader.recognize_path(path));
	_write_text(path + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK(loader.recognize_path(path));

	DirAccess::remove_absolute(path + ".import");
	DirAccess::remove_absolute(path);
}

TEST_CASE("[SceneTree][ImageTextureResourceFormat] Type hints and invalid files") {
	const String dir = _temp_dir();
	ResourceFormatLoaderImageTexture loader;

	CHECK(loader.handles_type("ImageTexture"));
	CHECK(loader.handles_type("Texture2D"));
	CHECK(loader.handles_type("Image"));
	CHECK(loader.handles_type("Resource"));
	CHECK_FALSE(loader.handles_type("AudioStream"));

	// KTX and DDS only for the hint Image.
	CHECK_FALSE(loader.recognize_path("user://a.dds"));
	CHECK_FALSE(loader.recognize_path("user://a.ktx", "Texture2D"));
	CHECK_FALSE(loader.recognize_path("user://a.png", "AudioStream"));
	CHECK_FALSE(loader.recognize_path("user://a.tres"));

	const String broken = dir.path_join("broken.png");
	_write_text(broken, "This is not a PNG file.");
	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(broken, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	// An empty ImageTexture cannot be saved.
	Ref<ImageTexture> empty;
	empty.instantiate();
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(empty, dir.path_join("empty.png")) != OK);
	ERR_PRINT_ON;

	DirAccess::remove_absolute(broken);
}

TEST_CASE("[ImageSaveOptions] Defaults and ranges") {
	Ref<ImageSaveOptions> options;
	options.instantiate();
	CHECK(options->get_jpg_quality() == doctest::Approx(0.75));
	CHECK_FALSE(options->is_webp_lossy());
	CHECK(options->get_webp_quality() == doctest::Approx(0.75));
	CHECK_FALSE(options->is_exr_grayscale());

	ERR_PRINT_OFF;
	options->set_jpg_quality(1.5f);
	options->set_webp_quality(-0.1f);
	ERR_PRINT_ON;
	CHECK(options->get_jpg_quality() == doctest::Approx(0.75));
	CHECK(options->get_webp_quality() == doctest::Approx(0.75));

	CHECK(ImageSaveOptions::get_attached(Ref<Resource>()).is_null());
}

} // namespace TestImageTextureResourceFormat
