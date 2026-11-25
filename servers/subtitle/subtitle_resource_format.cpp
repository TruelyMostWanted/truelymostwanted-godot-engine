/**************************************************************************/
/*  subtitle_resource_format.cpp                                          */
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

#include "subtitle_resource_format.h"

#include "core/io/file_access.h"
#include "servers/subtitle/subtitle.h"
#include "servers/subtitle/subtitle_format_srt.h"

// ResourceFormatLoaderSubtitle

Ref<Resource> ResourceFormatLoaderSubtitle::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	const Subtitle::Format format = Subtitle::get_format_for_extension(p_path.get_extension());
	if (format == Subtitle::FORMAT_MAX) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Not a subtitle file: '%s'.", p_path));
	}
	Error err = OK;
	const Vector<uint8_t> bytes = FileAccess::get_file_as_bytes(p_path, &err);
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot open subtitle file '%s'.", p_path));
	}

	Vector<Ref<SubtitleSegment>> segments;
	int skipped = 0;
	String language;
	err = Subtitle::parse_text(SubtitleFormatSRT::decode(bytes), format, segments, &skipped, &language);
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot parse subtitle file '%s': no cue could be read.", p_path));
	}
	if (skipped > 0) {
		WARN_PRINT(vformat("Subtitle file '%s': %d cue(s) could not be read and were skipped.", p_path, skipped));
	}

	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	TypedArray<SubtitleSegment> array;
	for (const Ref<SubtitleSegment> &segment : segments) {
		array.push_back(segment);
	}
	subtitle->set_segments(array);
	subtitle->set_language(language);
	if (r_error) {
		*r_error = OK;
	}
	return subtitle;
}

void ResourceFormatLoaderSubtitle::get_recognized_extensions(List<String> *p_extensions) const {
	for (const String &extension : Subtitle::get_recognized_extensions()) {
		p_extensions->push_back(extension);
	}
}

bool ResourceFormatLoaderSubtitle::handles_type(const String &p_type) const {
	return p_type == "Subtitle";
}

String ResourceFormatLoaderSubtitle::get_resource_type(const String &p_path) const {
	return Subtitle::get_format_for_extension(p_path.get_extension()) != Subtitle::FORMAT_MAX ? "Subtitle" : "";
}

// ResourceFormatSaverSubtitle

Error ResourceFormatSaverSubtitle::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	const Ref<Subtitle> subtitle = p_resource;
	ERR_FAIL_COND_V(subtitle.is_null(), ERR_INVALID_PARAMETER);
	return subtitle->save_to_file(p_path);
}

void ResourceFormatSaverSubtitle::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<Subtitle>(*p_resource)) {
		for (const String &extension : Subtitle::get_recognized_extensions()) {
			p_extensions->push_back(extension);
		}
	}
}

bool ResourceFormatSaverSubtitle::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<Subtitle>(*p_resource) != nullptr;
}
