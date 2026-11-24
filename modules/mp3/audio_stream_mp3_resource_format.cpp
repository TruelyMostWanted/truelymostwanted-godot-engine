/**************************************************************************/
/*  audio_stream_mp3_resource_format.cpp                                  */
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

#include "audio_stream_mp3_resource_format.h"

#include "audio_stream_mp3.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"

bool ResourceFormatLoaderAudioStreamMP3::is_imported_path(const String &p_path) {
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

bool ResourceFormatLoaderAudioStreamMP3::is_recognized_extension(const String &p_extension) {
#ifndef DR_MP3_ONLY_MP3
	if (p_extension == "mp1" || p_extension == "mp2") {
		return true;
	}
#endif
	return p_extension == "mp3";
}

Ref<Resource> ResourceFormatLoaderAudioStreamMP3::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error err = OK;
	const Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK || data.is_empty(), Ref<Resource>(), vformat("Cannot read MP3 file: %s.", p_path));

	// The compressed data is kept as it is and decoded while playing, like the importer does.
	Ref<AudioStreamMP3> stream = AudioStreamMP3::load_from_buffer(data);
	if (stream.is_null()) {
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot load MP3 file: %s.", p_path));
	}

	if (r_progress) {
		*r_progress = 1.0f;
	}
	if (r_error) {
		*r_error = OK;
	}
	return stream;
}

void ResourceFormatLoaderAudioStreamMP3::get_recognized_extensions(List<String> *p_extensions) const {
	// Same extensions as ResourceImporterMP3.
#ifndef DR_MP3_ONLY_MP3
	p_extensions->push_back("mp1");
	p_extensions->push_back("mp2");
#endif
	p_extensions->push_back("mp3");
}

bool ResourceFormatLoaderAudioStreamMP3::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!ResourceFormatLoader::recognize_path(p_path, p_for_type)) {
		return false;
	}
	// Imported MP3 files are loaded by ResourceFormatImporter (same rule as the other source formats).
	return !is_imported_path(p_path);
}

bool ResourceFormatLoaderAudioStreamMP3::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("AudioStreamMP3", p_type);
}

String ResourceFormatLoaderAudioStreamMP3::get_resource_type(const String &p_path) const {
	if (!is_recognized_extension(p_path.get_extension().to_lower()) || is_imported_path(p_path)) {
		return String();
	}
	return "AudioStreamMP3";
}
