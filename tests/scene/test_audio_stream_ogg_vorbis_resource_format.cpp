/**************************************************************************/
/*  test_audio_stream_ogg_vorbis_resource_format.cpp                      */
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

TEST_FORCE_LINK(test_audio_stream_ogg_vorbis_resource_format)

#include "modules/modules_enabled.gen.h" // For vorbis.

#ifdef MODULE_VORBIS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/audio/audio_stream.h"
#include "tests/test_utils.h"

// Only the loader header: `audio_stream_ogg_vorbis.h` needs the libvorbis include path of the module.
#include "modules/vorbis/audio_stream_ogg_vorbis_resource_format.h"

namespace TestAudioStreamOggVorbisResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("audio_stream_ogg_vorbis_resource_format");
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

// 0.5 seconds of a 440 Hz sine, 22050 Hz mono, encoded with libvorbis (generated for these tests).
static Vector<uint8_t> _tone() {
	return FileAccess::get_file_as_bytes(TestUtils::get_data_path("audio/tone_440hz_22050_mono.ogg"));
}

TEST_CASE("[Audio][AudioStreamOggVorbisResourceFormat] Load an Ogg Vorbis file without import through ResourceLoader") {
	const Vector<uint8_t> bytes = _tone();
	REQUIRE(bytes.size() == 3985);

	const String dir = _temp_dir();
	const String source = dir.path_join("tone.ogg");
	_write_bytes(source, bytes);

	CHECK(ResourceLoader::get_resource_type(source) == "AudioStreamOggVorbis");
	CHECK(ResourceLoader::exists(source));
	CHECK(ResourceLoader::exists(source, "AudioStream"));

	Ref<AudioStream> stream = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_class() == "AudioStreamOggVorbis");
	CHECK(stream->get_length() == doctest::Approx(0.5).epsilon(0.05));
	CHECK_FALSE(stream->has_loop());
	CHECK(stream->instantiate_playback().is_valid());

	// Cached on a normal load.
	Ref<Resource> first = ResourceLoader::load(source);
	Ref<Resource> second = ResourceLoader::load(source);
	REQUIRE(first.is_valid());
	CHECK(first == second);

	// No encoder in the engine: there is no saver for .ogg.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(stream, dir.path_join("copy.ogg")) != OK);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(dir.path_join("copy.ogg")));

	DirAccess::remove_absolute(source);
}

TEST_CASE("[Audio][AudioStreamOggVorbisResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String source = dir.path_join("imported.ogg");
	_write_bytes(source, _tone());

	ResourceFormatLoaderAudioStreamOggVorbis loader;
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "AudioStreamOggVorbis");

	_write_text(source + ".import", "[remap]\n\nimporter=\"oggvorbisstr\"\ntype=\"AudioStreamOggVorbis\"\n");
	CHECK(ResourceFormatLoaderAudioStreamOggVorbis::is_imported_path(source));
	CHECK_FALSE(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source).is_empty());

	// "Keep File (exported as is)" and "Skip File" do not import: the runtime loader takes the file.
	_write_text(source + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK(loader.recognize_path(source));
	_write_text(source + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "AudioStreamOggVorbis");

	DirAccess::remove_absolute(source + ".import");
	DirAccess::remove_absolute(source);
}

TEST_CASE("[Audio][AudioStreamOggVorbisResourceFormat] Invalid Ogg Vorbis files") {
	const String dir = _temp_dir();
	const String not_ogg = dir.path_join("not_ogg.ogg");
	_write_text(not_ogg, "This is not an Ogg Vorbis file.");

	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(not_ogg, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	ResourceFormatLoaderAudioStreamOggVorbis loader;
	CHECK(loader.handles_type("AudioStream"));
	CHECK(loader.handles_type("AudioStreamOggVorbis"));
	CHECK_FALSE(loader.handles_type("AudioStreamWAV"));
	CHECK_FALSE(loader.recognize_path(dir.path_join("sound.wav")));

	DirAccess::remove_absolute(not_ogg);
}

} // namespace TestAudioStreamOggVorbisResourceFormat

#endif // MODULE_VORBIS_ENABLED
