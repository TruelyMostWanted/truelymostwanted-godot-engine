/**************************************************************************/
/*  video_stream_webm_resource_format.cpp                                 */
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

#include "video_stream_webm_resource_format.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/resource_importer.h"
#include "core/object/class_db.h"

bool ResourceFormatLoaderVideoStreamWebm::is_imported_path(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		// In the editor, a `res://` file without `.import` is about to be imported if an importer handles its
		// extension. The engine has no `.webm` importer, so without an import plugin the file is loaded here.
		if (!p_path.begins_with("res://") || !Engine::get_singleton()->is_editor_hint()) {
			return false;
		}
		const ResourceFormatImporter *format_importer = ResourceFormatImporter::get_singleton();
		return format_importer && format_importer->get_importer_by_file(p_path).is_valid();
	}
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(import_path) != OK) {
		return true; // Unreadable `.import` file: leave the path to the importer, which reports the error.
	}
	const String importer = config->get_value("remap", "importer", String());
	return importer != "keep" && importer != "skip";
}

Ref<Resource> ResourceFormatLoaderVideoStreamWebm::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), Ref<Resource>(), vformat("Cannot open WebM file: %s.", p_path));

	// Every WebM file is an EBML document and starts with the EBML header ID 1A 45 DF A3. The VP9 and audio
	// headers are parsed when playback starts, so only the container is checked here.
	uint8_t magic[4] = {};
	const bool is_ebml = f->get_buffer(magic, 4) == 4 && magic[0] == 0x1A && magic[1] == 0x45 && magic[2] == 0xDF && magic[3] == 0xA3;
	f.unref();
	if (!is_ebml) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Not a WebM file: %s.", p_path));
	}

	// The stream only stores the path: each playback opens the file and streams it, nothing is kept in memory.
	Ref<Resource> stream = ResourceFormatLoaderWebm::load(p_path, p_original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);
	if (stream.is_valid() && r_progress) {
		*r_progress = 1.0f;
	}
	return stream;
}

bool ResourceFormatLoaderVideoStreamWebm::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!ResourceFormatLoader::recognize_path(p_path, p_for_type)) {
		return false;
	}
	// Imported `.webm` files (custom import plugins) are loaded by ResourceFormatImporter.
	return !is_imported_path(p_path);
}

bool ResourceFormatLoaderVideoStreamWebm::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("VideoStreamWebm", p_type);
}

String ResourceFormatLoaderVideoStreamWebm::get_resource_type(const String &p_path) const {
	if (!p_path.has_extension("webm") || is_imported_path(p_path)) {
		return String();
	}
	return "VideoStreamWebm";
}
