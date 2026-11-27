/**************************************************************************/
/*  resource_format_text_data.cpp                                         */
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

#include "resource_format_text_data.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/text_data_format.h"
#include "core/object/class_db.h"

static Ref<TextDataFormat> _instantiate_format(const StringName &p_class) {
	Object *object = ClassDB::instantiate(p_class);
	TextDataFormat *format = Object::cast_to<TextDataFormat>(object);
	if (format == nullptr) {
		if (object) {
			memdelete(object);
		}
		ERR_FAIL_V_MSG(Ref<TextDataFormat>(), vformat("\"%s\" is not a TextDataFormat class.", p_class));
	}
	return Ref<TextDataFormat>(format);
}

/////////////////////////////////////////////////////////////////////////////
// Loader.

ResourceFormatLoaderTextData::ResourceFormatLoaderTextData(const StringName &p_class, const Vector<String> &p_extensions) {
	format_class = p_class;
	extensions = p_extensions;
}

bool ResourceFormatLoaderTextData::_has_extension(const String &p_path) const {
	const String extension = p_path.get_extension();
	for (const String &candidate : extensions) {
		if (extension.nocasecmp_to(candidate) == 0) {
			return true;
		}
	}
	return false;
}

Ref<Resource> ResourceFormatLoaderTextData::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		return Ref<Resource>();
	}

	Ref<TextDataFormat> format = _instantiate_format(format_class);
	ERR_FAIL_COND_V(format.is_null(), Ref<Resource>());
	const bool editor = Engine::get_singleton()->is_editor_hint();
	// Only the editor keeps the text (comments and formatting survive saving); games do not keep a second copy.
	err = format->parse(text, editor);
	if (err != OK) {
		// The line is 1-based, 0 means unknown.
		const String line = format->get_error_line() > 0 ? vformat(", on line %d", format->get_error_line()) : String();
		const String message = vformat("Error parsing %s file at '%s'%s: %s", format_class, p_path, line, format->get_error_message());
		if (editor) {
			// Still return the resource, so the text editor can open the file and fix it.
			WARN_PRINT(message);
		} else {
			if (r_error) {
				*r_error = err;
			}
			ERR_PRINT(message);
			return Ref<Resource>();
		}
	}
	if (r_error) {
		*r_error = OK;
	}
	return format;
}

void ResourceFormatLoaderTextData::get_recognized_extensions(List<String> *p_extensions) const {
	for (const String &extension : extensions) {
		p_extensions->push_back(extension);
	}
}

bool ResourceFormatLoaderTextData::handles_type(const String &p_type) const {
	return p_type == String(format_class) || ClassDB::is_parent_class(format_class, p_type);
}

String ResourceFormatLoaderTextData::get_resource_type(const String &p_path) const {
	return _has_extension(p_path) ? String(format_class) : String();
}

/////////////////////////////////////////////////////////////////////////////
// Saver.

ResourceFormatSaverTextData::ResourceFormatSaverTextData(const StringName &p_class) {
	format_class = p_class;
}

Error ResourceFormatSaverTextData::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	const Ref<TextDataFormat> format = p_resource;
	ERR_FAIL_COND_V(format.is_null(), ERR_INVALID_PARAMETER);
	String text;
	String message;
	Error err = format->get_save_text(text, message);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't save %s as '%s': %s", format_class, p_path, message));
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't open the file '%s' for writing (in exported projects, res:// is read-only: use user://).", p_path));
	file->store_string(text);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

void ResourceFormatSaverTextData::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	const Ref<TextDataFormat> format = p_resource;
	if (recognize(p_resource) && format.is_valid()) {
		format->get_recognized_extensions_list(p_extensions);
	}
}

bool ResourceFormatSaverTextData::recognize(const Ref<Resource> &p_resource) const {
	// Only this class, not subclasses (like the JSON saver).
	return p_resource.is_valid() && p_resource->get_class_name() == format_class;
}
