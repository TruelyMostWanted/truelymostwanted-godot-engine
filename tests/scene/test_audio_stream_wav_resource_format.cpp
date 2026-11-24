/**************************************************************************/
/*  test_audio_stream_wav_resource_format.cpp                             */
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

TEST_FORCE_LINK(test_audio_stream_wav_resource_format)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "scene/resources/audio/audio_stream_wav_resource_format.h"
#include "tests/test_utils.h"

namespace TestAudioStreamWAVResourceFormat {

static String _temp_dir() {
	const String dir = TestUtils::get_temp_path("audio_stream_wav_resource_format");
	DirAccess::make_dir_recursive_absolute(dir);
	return dir;
}

static void _write_text(const String &p_path, const String &p_text) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_text);
}

// A short 16-bit sawtooth, `p_frames` frames.
static Vector<uint8_t> _pcm16(int p_frames, bool p_stereo) {
	const int channels = p_stereo ? 2 : 1;
	Vector<uint8_t> data;
	data.resize(p_frames * channels * 2);
	uint8_t *w = data.ptrw();
	for (int i = 0; i < p_frames * channels; i++) {
		const int16_t v = (int16_t)((i * 997) % 65536 - 32768);
		encode_uint16((uint16_t)v, &w[i * 2]);
	}
	return data;
}

static Ref<AudioStreamWAV> _make_stream(AudioStreamWAV::Format p_format, bool p_stereo, int p_mix_rate) {
	Ref<AudioStreamWAV> stream;
	stream.instantiate();
	stream->set_format(p_format);
	stream->set_stereo(p_stereo);
	stream->set_mix_rate(p_mix_rate);
	if (p_format == AudioStreamWAV::FORMAT_8_BITS) {
		Vector<uint8_t> data;
		data.resize(64);
		for (int i = 0; i < data.size(); i++) {
			data.write[i] = (uint8_t)(int8_t)(i * 4 - 128);
		}
		stream->set_data(data);
	} else {
		stream->set_data(_pcm16(32, p_stereo));
	}
	return stream;
}

TEST_CASE("[Audio][AudioStreamWAVResourceFormat] Load a WAV file without import through ResourceLoader") {
	const String dir = _temp_dir();
	const String source = dir.path_join("source.wav");
	Ref<AudioStreamWAV> original = _make_stream(AudioStreamWAV::FORMAT_16_BITS, true, 22050);
	REQUIRE(original->save_to_wav(source) == OK);

	CHECK(ResourceLoader::get_resource_type(source) == "AudioStreamWAV");
	CHECK(ResourceLoader::exists(source));
	CHECK(ResourceLoader::exists(source, "AudioStream"));

	Ref<AudioStreamWAV> loaded = ResourceLoader::load(source, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_format() == AudioStreamWAV::FORMAT_16_BITS);
	CHECK(loaded->is_stereo());
	CHECK(loaded->get_mix_rate() == 22050);
	CHECK(loaded->get_data() == original->get_data());

	// Cached on a normal load.
	Ref<Resource> first = ResourceLoader::load(source);
	Ref<Resource> second = ResourceLoader::load(source);
	REQUIRE(first.is_valid());
	CHECK(first == second);
	CHECK_FALSE(first->get_path().is_empty());

	DirAccess::remove_absolute(source);
}

TEST_CASE("[Audio][AudioStreamWAVResourceFormat] Save through ResourceSaver and load again") {
	const String dir = _temp_dir();

	for (const AudioStreamWAV::Format format : { AudioStreamWAV::FORMAT_8_BITS, AudioStreamWAV::FORMAT_16_BITS }) {
		Ref<AudioStreamWAV> original = _make_stream(format, false, 44100);
		const String path = dir.path_join(format == AudioStreamWAV::FORMAT_8_BITS ? "saved8.wav" : "saved16.WAV");

		List<String> extensions;
		ResourceSaver::get_recognized_extensions(original, &extensions);
		CHECK(extensions.find("wav") != nullptr);

		REQUIRE(ResourceSaver::save(original, path) == OK);
		Ref<AudioStreamWAV> loaded = ResourceLoader::load(path, "AudioStreamWAV", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_format() == format);
		CHECK_FALSE(loaded->is_stereo());
		CHECK(loaded->get_mix_rate() == 44100);
		CHECK(loaded->get_data() == original->get_data());

		DirAccess::remove_absolute(path);
	}
}

TEST_CASE("[Audio][AudioStreamWAVResourceFormat] Compressed streams cannot be saved as WAV") {
	const String dir = _temp_dir();
	Ref<AudioStreamWAV> stream = _make_stream(AudioStreamWAV::FORMAT_16_BITS, false, 44100);
	stream->set_format(AudioStreamWAV::FORMAT_QOA);

	List<String> extensions;
	ResourceFormatSaverAudioStreamWAV saver;
	saver.get_recognized_extensions(stream, &extensions);
	CHECK(extensions.is_empty());

	const String path = dir.path_join("qoa.wav");
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(stream, path) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(path));
}

TEST_CASE("[Audio][AudioStreamWAVResourceFormat] Imported paths are left to the importer") {
	const String dir = _temp_dir();
	const String source = dir.path_join("imported.wav");
	REQUIRE(_make_stream(AudioStreamWAV::FORMAT_16_BITS, false, 44100)->save_to_wav(source) == OK);

	ResourceFormatLoaderAudioStreamWAV loader;
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "AudioStreamWAV");

	_write_text(source + ".import", "[remap]\n\nimporter=\"wav\"\ntype=\"AudioStreamWAV\"\n");
	CHECK(AudioStreamWAVResourceFormat::is_imported_path(source));
	CHECK_FALSE(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source).is_empty());

	// "Keep File (exported as is)" and "Skip File" do not import: the runtime loader takes the file.
	_write_text(source + ".import", "[remap]\n\nimporter=\"keep\"\n");
	CHECK_FALSE(AudioStreamWAVResourceFormat::is_imported_path(source));
	CHECK(loader.recognize_path(source));
	_write_text(source + ".import", "[remap]\n\nimporter=\"skip\"\n");
	CHECK(loader.recognize_path(source));
	CHECK(loader.get_resource_type(source) == "AudioStreamWAV");

	DirAccess::remove_absolute(source + ".import");
	DirAccess::remove_absolute(source);
}

TEST_CASE("[Audio][AudioStreamWAVResourceFormat] Invalid WAV files") {
	const String dir = _temp_dir();

	const String not_a_wav = dir.path_join("not_a_wav.wav");
	_write_text(not_a_wav, "This is not a WAV file.");
	Error err = OK;
	ERR_PRINT_OFF;
	Ref<Resource> res = ResourceLoader::load(not_a_wav, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_PRINT_ON;
	CHECK(res.is_null());
	CHECK(err != OK);

	const String empty = dir.path_join("empty.wav");
	_write_text(empty, "");
	ERR_PRINT_OFF;
	res = ResourceLoader::load(empty, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;
	CHECK(res.is_null());

	ResourceFormatLoaderAudioStreamWAV loader;
	CHECK_FALSE(loader.recognize_path(dir.path_join("sound.ogg")));
	CHECK(loader.get_resource_type(dir.path_join("sound.ogg")).is_empty());
	CHECK(loader.handles_type("AudioStream"));
	CHECK(loader.handles_type("AudioStreamWAV"));
	CHECK_FALSE(loader.handles_type("Texture2D"));

	DirAccess::remove_absolute(not_a_wav);
	DirAccess::remove_absolute(empty);
}

} // namespace TestAudioStreamWAVResourceFormat
