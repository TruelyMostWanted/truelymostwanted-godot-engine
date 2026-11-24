/**************************************************************************/
/*  test_video_stream_theora_resource_format.cpp                          */
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

TEST_FORCE_LINK(test_video_stream_theora_resource_format)

#include "modules/modules_enabled.gen.h" // For theora.

#ifdef MODULE_THEORA_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/video_stream.h"
#include "tests/test_utils.h"

// Only the loader header: `video_stream_theora.h` needs the libtheora include path of the module.
#include "modules/theora/video_stream_theora_resource_format.h"

namespace TestVideoStreamTheoraResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("video_stream_theora_resource_format");
	DirAccess::make_dir_recursive_absolute(dir);
	return dir;
}

static void _write_bytes(const String &p_path, const Vector<uint8_t> &p_data) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_buffer(p_data.ptr(), p_data.size());
}

static void _write_text(const String &p_path, const String &p_text) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_text);
}

// One second of a 64x48 test pattern at 10 FPS, encoded with libtheora, no audio (generated for these tests).
static Vector<uint8_t> _video() {
	return FileAccess::get_file_as_bytes(TestUtils::get_data_path("video/test_64x48_10fps_1s.ogv"));
}

TEST_CASE("[VideoStreamTheoraResourceFormat] Load an Ogg Theora file without import through ResourceLoader") {
	const Vector<uint8_t> bytes = _video();
	REQUIRE(bytes.size() == 4852);

	const String dir = _temp_dir();
	const String source = dir.path_join("clip.ogv");
	_write_bytes(source, bytes);

	CHECK(ResourceLoader::get_resource_type(source) == "VideoStreamTheora");
	CHECK(ResourceLoader::exists(source));
	CHECK(ResourceLoader::exists(source, "VideoStream"));
	CHECK(ResourceLoader::exists(source, "Resource"));

	Ref<VideoStream> stream = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_class() == "VideoStreamTheora");
	CHECK(stream->get_file() == source);

	// A type hint of a base class works too.
	Ref<Resource> as_resource = ResourceLoader::load(source, "Resource", ResourceFormatLoader::CACHE_MODE_IGNORE);
	CHECK(as_resource.is_valid());

	// Cached on a normal load.
	Ref<Resource> first = ResourceLoader::load(source);
	Ref<Resource> second = ResourceLoader::load(source);
	REQUIRE(first.is_valid());
	CHECK(first == second);

	// No encoder in the engine: there is no saver for .ogv.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(stream, dir.path_join("copy.ogv")) != OK);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("copy.ogv")));

	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamTheoraResourceFormat] Threaded loading") {
	const String dir = _temp_dir();
	const String source = dir.path_join("threaded.ogv");
	_write_bytes(source, _video());

	REQUIRE(ResourceLoader::load_threaded_request(source, "VideoStream", false, ResourceFormatLoader::CACHE_MODE_IGNORE) == OK);
	Ref<VideoStream> stream = ResourceLoader::load_threaded_get(source);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_class() == "VideoStreamTheora");
	CHECK(stream->get_file() == source);

	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamTheoraResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String source = dir.path_join("imported.ogv");
	_write_bytes(source, _video());

	ResourceFormatLoaderTheora loader;
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "VideoStreamTheora");

	// An import plugin for .ogv (none in the engine, but possible from an editor plugin) takes precedence.
	_write_text(source + ".import", "[remap]\n\nimporter=\"custom_ogv\"\ntype=\"VideoStreamTheora\"\n");
	CHECK(ResourceFormatLoaderTheora::is_imported_path(source));
	CHECK_FALSE(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source).is_empty());

	// "Keep File (exported as is)" and "Skip File" do not import: the runtime loader takes the file.
	_write_text(source + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK(loader.recognize_path(source));
	_write_text(source + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "VideoStreamTheora");

	DirAccess::remove_absolute(source + ".import");
	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamTheoraResourceFormat] Invalid and missing files") {
	const String dir = _temp_dir();
	const String not_ogg = dir.path_join("not_ogg.ogv");
	_write_text(not_ogg, "This is not an Ogg Theora file.");

	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(not_ogg, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	err = OK;
	ERR_PRINT_OFF;
	res = ResourceLoader::load(dir.path_join("missing.ogv"), "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);
	CHECK_FALSE(ResourceLoader::exists(dir.path_join("missing.ogv")));

	ResourceFormatLoaderTheora loader;
	CHECK(loader.handles_type("Resource"));
	CHECK(loader.handles_type("VideoStream"));
	CHECK(loader.handles_type("VideoStreamTheora"));
	CHECK_FALSE(loader.handles_type("AudioStream"));
	CHECK(loader.recognize_path(dir.path_join("CLIP.OGV")));
	CHECK_FALSE(loader.recognize_path(dir.path_join("clip.ogg")));

	DirAccess::remove_absolute(not_ogg);
}

} // namespace TestVideoStreamTheoraResourceFormat

#endif // MODULE_THEORA_ENABLED
