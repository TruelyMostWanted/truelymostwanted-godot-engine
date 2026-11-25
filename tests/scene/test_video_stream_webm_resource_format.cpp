/**************************************************************************/
/*  test_video_stream_webm_resource_format.cpp                            */
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

TEST_FORCE_LINK(test_video_stream_webm_resource_format)

#include "modules/modules_enabled.gen.h" // For webm.

#ifdef MODULE_WEBM_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/video_stream.h"
#include "tests/test_utils.h"

#include "modules/webm/video_stream_webm_resource_format.h"

namespace TestVideoStreamWebmResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("video_stream_webm_resource_format");
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

// One second of a 64x64 test pattern, encoded with libvpx (VP9), no audio (generated for these tests).
static Vector<uint8_t> _video() {
	return FileAccess::get_file_as_bytes(TestUtils::get_data_path("video/testsrc_64x64_1s.webm"));
}

TEST_CASE("[VideoStreamWebmResourceFormat] Load a WebM file without import through ResourceLoader") {
	const Vector<uint8_t> bytes = _video();
	REQUIRE(bytes.size() == 6670);

	const String dir = _temp_dir();
	const String source = dir.path_join("clip.webm");
	_write_bytes(source, bytes);

	CHECK(ResourceLoader::get_resource_type(source) == "VideoStreamWebm");
	CHECK(ResourceLoader::exists(source));
	CHECK(ResourceLoader::exists(source, "VideoStream"));
	CHECK(ResourceLoader::exists(source, "Resource"));

	Ref<VideoStream> stream = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_class() == "VideoStreamWebm");
	CHECK(stream->get_file() == source);

	// A type hint of a base class works too.
	Ref<Resource> as_resource = ResourceLoader::load(source, "Resource", ResourceFormatLoader::CACHE_MODE_IGNORE);
	CHECK(as_resource.is_valid());

	// Cached on a normal load.
	Ref<Resource> first = ResourceLoader::load(source);
	Ref<Resource> second = ResourceLoader::load(source);
	REQUIRE(first.is_valid());
	CHECK(first == second);

	// No encoder in the engine: there is no saver for .webm.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(stream, dir.path_join("copy.webm")) != OK);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("copy.webm")));

	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamWebmResourceFormat] Threaded loading") {
	const String dir = _temp_dir();
	const String source = dir.path_join("threaded.webm");
	_write_bytes(source, _video());

	REQUIRE(ResourceLoader::load_threaded_request(source, "VideoStream", false, ResourceFormatLoader::CACHE_MODE_IGNORE) == OK);
	Ref<VideoStream> stream = ResourceLoader::load_threaded_get(source);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_class() == "VideoStreamWebm");
	CHECK(stream->get_file() == source);

	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamWebmResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String source = dir.path_join("imported.webm");
	_write_bytes(source, _video());

	ResourceFormatLoaderVideoStreamWebm loader;
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "VideoStreamWebm");

	// An import plugin for .webm (none in the engine, but possible from an editor plugin) takes precedence.
	_write_text(source + ".import", "[remap]\n\nimporter=\"custom_webm\"\ntype=\"VideoStreamWebm\"\n");
	CHECK(ResourceFormatLoaderVideoStreamWebm::is_imported_path(source));
	CHECK_FALSE(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source).is_empty());

	// "Keep File (exported as is)" and "Skip File" do not import: the runtime loader takes the file.
	_write_text(source + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK(loader.recognize_path(source));
	_write_text(source + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "VideoStreamWebm");

	DirAccess::remove_absolute(source + ".import");
	DirAccess::remove_absolute(source);
}

TEST_CASE("[VideoStreamWebmResourceFormat] Invalid and missing files") {
	const String dir = _temp_dir();
	const String not_webm = dir.path_join("not_webm.webm");
	_write_text(not_webm, "This is not a WebM file.");

	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(not_webm, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	err = OK;
	ERR_PRINT_OFF;
	res = ResourceLoader::load(dir.path_join("missing.webm"), "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);
	CHECK_FALSE(ResourceLoader::exists(dir.path_join("missing.webm")));

	ResourceFormatLoaderVideoStreamWebm loader;
	CHECK(loader.handles_type("Resource"));
	CHECK(loader.handles_type("VideoStream"));
	CHECK(loader.handles_type("VideoStreamWebm"));
	CHECK_FALSE(loader.handles_type("VideoStreamTheora"));
	CHECK_FALSE(loader.handles_type("AudioStream"));
	CHECK(loader.recognize_path(dir.path_join("CLIP.WEBM")));
	CHECK_FALSE(loader.recognize_path(dir.path_join("clip.mkv")));

	List<String> extensions;
	loader.get_recognized_extensions(&extensions);
	CHECK(extensions.size() == 1);
	CHECK(extensions.find("webm") != nullptr);

	DirAccess::remove_absolute(not_webm);
}

} // namespace TestVideoStreamWebmResourceFormat

#endif // MODULE_WEBM_ENABLED
