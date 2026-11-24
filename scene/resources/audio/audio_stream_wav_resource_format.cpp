/**************************************************************************/
/*  audio_stream_wav_resource_format.cpp                                  */
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

#include "audio_stream_wav_resource_format.h"

#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "scene/resources/audio/audio_stream_wav.h"

namespace AudioStreamWAVResourceFormat {

bool is_imported_path(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		return false;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(import_path) != OK) {
		return true; // Unreadable `.import` file: leave the path to the importer, which reports the error.
	}
	const String importer = config->get_value("remap", "importer", String());
	return importer != "keep" && importer != "skip";
}

} // namespace AudioStreamWAVResourceFormat

using namespace AudioStreamWAVResourceFormat;

// ResourceFormatLoaderAudioStreamWAV

Ref<Resource> ResourceFormatLoaderAudioStreamWAV::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error err = OK;
	const Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK || data.is_empty(), Ref<Resource>(), vformat("Cannot read WAV file: %s.", p_path));

	// Default options of the importer: no compression, loop mode from the file, no trim or normalize.
	Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(data, Dictionary());
	if (stream.is_null()) {
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot load WAV file: %s.", p_path));
	}

	if (r_progress) {
		*r_progress = 1.0f;
	}
	if (r_error) {
		*r_error = OK;
	}
	return stream;
}

void ResourceFormatLoaderAudioStreamWAV::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("wav");
}

bool ResourceFormatLoaderAudioStreamWAV::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!ResourceFormatLoader::recognize_path(p_path, p_for_type)) {
		return false;
	}
	// Imported WAV files are loaded by ResourceFormatImporter (same rule as the other source formats).
	return !is_imported_path(p_path);
}

bool ResourceFormatLoaderAudioStreamWAV::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("AudioStreamWAV", p_type);
}

String ResourceFormatLoaderAudioStreamWAV::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() != "wav" || is_imported_path(p_path)) {
		return String();
	}
	return "AudioStreamWAV";
}

// ResourceFormatSaverAudioStreamWAV

static bool _can_save_format(const AudioStreamWAV *p_stream) {
	const AudioStreamWAV::Format format = p_stream->get_format();
	return format == AudioStreamWAV::FORMAT_8_BITS || format == AudioStreamWAV::FORMAT_16_BITS;
}

Error ResourceFormatSaverAudioStreamWAV::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<AudioStreamWAV> stream = p_resource;
	ERR_FAIL_COND_V(stream.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_path.get_extension().to_lower() != "wav", ERR_FILE_UNRECOGNIZED, vformat("Cannot save AudioStreamWAV to %s: only .wav files are supported.", p_path));
	ERR_FAIL_COND_V_MSG(!_can_save_format(stream.ptr()), ERR_UNAVAILABLE, vformat("Cannot save AudioStreamWAV to %s: only 8-bit and 16-bit PCM data can be written (not IMA ADPCM or QOA).", p_path));

	return stream->save_to_wav(p_path);
}

void ResourceFormatSaverAudioStreamWAV::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	const AudioStreamWAV *stream = Object::cast_to<AudioStreamWAV>(*p_resource);
	if (stream != nullptr && _can_save_format(stream)) {
		p_extensions->push_back("wav");
	}
}

bool ResourceFormatSaverAudioStreamWAV::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<AudioStreamWAV>(*p_resource) != nullptr;
}

bool ResourceFormatSaverAudioStreamWAV::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	return p_path.get_extension().to_lower() == "wav";
}
