/**************************************************************************/
/*  resource_format_text_tree.cpp                                         */
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

#include "resource_format_text_tree.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/script_language.h"

// ResourceFormatLoader::load() gets no type hint, but ResourceLoader calls recognize_path() with the hint
// right before load() on the same thread. The hint is kept here for that one call (needed by CSV, which has no
// type in the file, and by JSON/INI files without a type marker).
static thread_local String text_tree_hint_path;
static thread_local String text_tree_hint_type;

/////////////////////////////////////////////////////////////////////////////
// Loader base.

String ResourceFormatLoaderTextTree::_get_file_type(const String &p_path, String *r_script_class) const {
	if (p_path.get_extension().nocasecmp_to(_get_extension()) != 0 || !FileAccess::exists(p_path)) {
		return String();
	}
	const String marker = _get_marker_type(FileAccess::get_file_as_string(p_path));
	if (marker.is_empty()) {
		return String();
	}
	if (ScriptServer::is_global_class(marker)) {
		if (r_script_class) {
			*r_script_class = marker;
		}
		return ScriptServer::get_global_class_native_base(marker);
	}
	return marker;
}

Ref<Resource> ResourceFormatLoaderTextTree::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	String type_hint;
	if (text_tree_hint_path == p_path) {
		type_hint = text_tree_hint_type;
	}
	text_tree_hint_path = String();
	text_tree_hint_type = String();

	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Can't open the file '%s'.", p_path));
	}

	Ref<Resource> resource;
	String message;
	err = _parse_text(text, type_hint, resource, message);
	if (err == ERR_FILE_UNRECOGNIZED) {
		// Not a resource file of this format: the next loader (for example the JSON loader) takes it.
		if (r_error) {
			*r_error = err;
		}
		return Ref<Resource>();
	}
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Can't load the resource '%s': %s", p_path, message));
	}

	if (r_error) {
		*r_error = OK;
	}
	return resource;
}

void ResourceFormatLoaderTextTree::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(_get_extension());
}

bool ResourceFormatLoaderTextTree::recognize_path(const String &p_path, const String &p_for_type) const {
	if (p_path.get_extension().nocasecmp_to(_get_extension()) != 0) {
		return false;
	}
	if (!p_for_type.is_empty() && !handles_type(p_for_type)) {
		return false;
	}
	text_tree_hint_path = p_path;
	text_tree_hint_type = p_for_type;
	return true;
}

bool ResourceFormatLoaderTextTree::handles_type(const String &p_type) const {
	return p_type == "Resource" || ResourceTextSerializer::is_resource_type(p_type);
}

String ResourceFormatLoaderTextTree::get_resource_type(const String &p_path) const {
	return _get_file_type(p_path, nullptr);
}

String ResourceFormatLoaderTextTree::get_resource_script_class(const String &p_path) const {
	String script_class;
	[[maybe_unused]] const String type = _get_file_type(p_path, &script_class);
	return script_class;
}

/////////////////////////////////////////////////////////////////////////////
// Saver base.

Error ResourceFormatSaverTextTree::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	String text;
	String message;
	Error err = _write_text(p_resource, text, message);
	if (err != OK) {
		ERR_FAIL_V_MSG(err, vformat("Can't save the resource as '%s': %s", p_path, message));
	}

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't open the file '%s' for writing.", p_path));
	file->store_string(text);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

void ResourceFormatSaverTextTree::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource) && _can_save(p_resource)) {
		p_extensions->push_back(_get_extension());
	}
}

bool ResourceFormatSaverTextTree::recognize(const Ref<Resource> &p_resource) const {
	// JSON resources keep their own saver.
	return p_resource.is_valid() && p_resource->get_class_name() != JSON::get_class_static();
}

bool ResourceFormatSaverTextTree::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	// Also when _can_save() is false, so that save() can report why the resource does not fit.
	return p_path.get_extension().nocasecmp_to(_get_extension()) == 0;
}

/////////////////////////////////////////////////////////////////////////////
// JSON.

Error ResourceFormatLoaderResourceJSON::_parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const {
	return ResourceTextSerializer::parse_json(p_text, p_type_hint, r_resource, r_error);
}

String ResourceFormatLoaderResourceJSON::_get_marker_type(const String &p_text) const {
	if (!p_text.contains("\"$type\"")) {
		return String(); // Fast path for plain JSON files.
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_text) != OK) {
		return String();
	}
	return ResourceTextSerializer::get_json_marker_type(json->get_data());
}

bool ResourceFormatLoaderResourceJSON::handles_type(const String &p_type) const {
	// The JSON class itself stays with ResourceFormatLoaderJSON.
	return p_type != "JSON" && ResourceFormatLoaderTextTree::handles_type(p_type);
}

Error ResourceFormatSaverResourceJSON::_write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const {
	return ResourceTextSerializer::write_json(p_resource, ResourceTextSerializer::get_default_options(), r_text, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// INI.

Error ResourceFormatLoaderResourceINI::_parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const {
	return ResourceTextSerializer::parse_ini(p_text, p_type_hint, r_resource, r_error);
}

String ResourceFormatLoaderResourceINI::_get_marker_type(const String &p_text) const {
	return ResourceTextSerializer::get_ini_marker_type(p_text);
}

Error ResourceFormatSaverResourceINI::_write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const {
	return ResourceTextSerializer::write_ini(p_resource, ResourceTextSerializer::get_default_options(), r_text, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// CSV.

Error ResourceFormatLoaderResourceCSV::_parse_text(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) const {
	return ResourceTextSerializer::parse_csv(p_text, p_type_hint, ResourceTextSerializer::get_default_options(), r_resource, r_error);
}

Error ResourceFormatSaverResourceCSV::_write_text(const Ref<Resource> &p_resource, String &r_text, String &r_error) const {
	return ResourceTextSerializer::write_csv(p_resource, ResourceTextSerializer::get_default_options(), r_text, r_error);
}

bool ResourceFormatSaverResourceCSV::_can_save(const Ref<Resource> &p_resource) const {
	String error;
	return ResourceTextSerializer::check_csv(p_resource, error) == OK;
}
