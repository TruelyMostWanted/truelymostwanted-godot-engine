/**************************************************************************/
/*  video_stream_theora_resource_format.cpp                               */
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

#include "video_stream_theora_resource_format.h"

#include "video_stream_theora.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"

bool ResourceFormatLoaderTheora::is_imported_path(const String &p_path) {
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

Ref<Resource> ResourceFormatLoaderTheora::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), Ref<Resource>(), vformat("Cannot open Ogg Theora file: %s.", p_path));

	// Every Ogg file starts with the capture pattern "OggS". The Theora headers are parsed when playback starts,
	// so only the container is checked here (cheap, and keeps the file closed until it is played).
	uint8_t magic[4] = {};
	const bool is_ogg = f->get_buffer(magic, 4) == 4 && magic[0] == 'O' && magic[1] == 'g' && magic[2] == 'g' && magic[3] == 'S';
	if (!is_ogg) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Not an Ogg file: %s.", p_path));
	}
	f.unref();

	// The stream only stores the path: each playback opens the file and streams it, nothing is kept in memory.
	Ref<VideoStreamTheora> stream;
	stream.instantiate();
	stream->set_file(p_path);

	if (r_progress) {
		*r_progress = 1.0f;
	}
	if (r_error) {
		*r_error = OK;
	}
	return stream;
}

void ResourceFormatLoaderTheora::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("ogv");
}

bool ResourceFormatLoaderTheora::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!ResourceFormatLoader::recognize_path(p_path, p_for_type)) {
		return false;
	}
	// Imported `.ogv` files (custom import plugins) are loaded by ResourceFormatImporter.
	return !is_imported_path(p_path);
}

bool ResourceFormatLoaderTheora::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("VideoStreamTheora", p_type);
}

String ResourceFormatLoaderTheora::get_resource_type(const String &p_path) const {
	if (!p_path.has_extension("ogv") || is_imported_path(p_path)) {
		return String();
	}
	return "VideoStreamTheora";
}
