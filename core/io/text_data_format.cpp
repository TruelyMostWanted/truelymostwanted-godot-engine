/**************************************************************************/
/*  text_data_format.cpp                                                  */
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

#include "text_data_format.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"

Error TextDataFormat::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	r_error = "This format does not implement parsing.";
	return ERR_UNAVAILABLE;
}

Error TextDataFormat::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	r_error = "This format does not implement encoding.";
	return ERR_UNAVAILABLE;
}

void TextDataFormat::_mark_data_changed() {
	text = String();
	emit_changed();
}

void TextDataFormat::set_data(const Variant &p_data) {
	const Variant::Type type = _get_data_type();
	ERR_FAIL_COND_MSG(type != Variant::NIL && p_data.get_type() != type, vformat("%s data must be of type %s, not %s.", get_class(), Variant::get_type_name(type), Variant::get_type_name(p_data.get_type())));
	data = p_data;
	text = String();
	_data_replaced();
	emit_changed();
}

void TextDataFormat::set_options(const Ref<SerializerOptions> &p_options) {
	options = p_options;
	emit_changed();
}

Error TextDataFormat::parse(const String &p_text, bool p_keep_text) {
	Variant parsed;
	String parse_error;
	int line = 0;
	int column = 0;
	const Error err = _parse_data(p_text, options, parsed, parse_error, line, column);
	text = p_keep_text ? p_text : String();
	if (err != OK) {
		data = _get_empty_data();
		error_message = parse_error;
		error_line = line;
		error_column = column;
	} else {
		data = parsed;
		error_message = String();
		error_line = 0;
		error_column = 0;
	}
	_data_replaced();
	emit_changed();
	return err;
}

String TextDataFormat::encode_to_text() const {
	String result;
	String encode_error;
	if (_encode_data(data, options, result, encode_error) != OK) {
		ERR_PRINT(vformat("Can't encode %s data: %s", get_class(), encode_error));
		return String();
	}
	return result;
}

Error TextDataFormat::get_save_text(String &r_text, String &r_error) const {
	if (!text.is_empty()) {
		r_text = text;
		return OK;
	}
	return _encode_data(data, options, r_text, r_error);
}

void TextDataFormat::clear() {
	data = _get_empty_data();
	text = String();
	error_message = String();
	error_line = 0;
	error_column = 0;
	_cleared();
	_data_replaced();
	emit_changed();
}

Error TextDataFormat::load(const String &p_path) {
	Error err = OK;
	const String file_text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		return err;
	}
	return parse(file_text);
}

Error TextDataFormat::save(const String &p_path) {
	String save_text;
	String save_error;
	Error err = get_save_text(save_text, save_error);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't save %s to '%s': %s", get_class(), p_path, save_error));
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't open '%s' for writing (in exported projects, res:// is read-only: use user://).", p_path));
	file->store_string(save_text);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

PackedStringArray TextDataFormat::get_recognized_extensions() const {
	List<String> extensions;
	_get_extensions(&extensions);
	PackedStringArray result;
	for (const String &extension : extensions) {
		result.push_back(extension);
	}
	return result;
}

bool TextDataFormat::is_recognized_path(const String &p_path) const {
	const String extension = p_path.get_extension();
	List<String> extensions;
	_get_extensions(&extensions);
	for (const String &candidate : extensions) {
		if (extension.nocasecmp_to(candidate) == 0) {
			return true;
		}
	}
	return false;
}

void TextDataFormat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &TextDataFormat::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &TextDataFormat::get_data);
	ClassDB::bind_method(D_METHOD("set_options", "options"), &TextDataFormat::set_options);
	ClassDB::bind_method(D_METHOD("get_options"), &TextDataFormat::get_options);
	ClassDB::bind_method(D_METHOD("parse", "text", "keep_text"), &TextDataFormat::parse, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("encode_to_text"), &TextDataFormat::encode_to_text);
	ClassDB::bind_method(D_METHOD("clear"), &TextDataFormat::clear);
	ClassDB::bind_method(D_METHOD("load", "path"), &TextDataFormat::load);
	ClassDB::bind_method(D_METHOD("save", "path"), &TextDataFormat::save);
	ClassDB::bind_method(D_METHOD("get_parsed_text"), &TextDataFormat::get_parsed_text);
	ClassDB::bind_method(D_METHOD("get_error_message"), &TextDataFormat::get_error_message);
	ClassDB::bind_method(D_METHOD("get_error_line"), &TextDataFormat::get_error_line);
	ClassDB::bind_method(D_METHOD("get_error_column"), &TextDataFormat::get_error_column);
	ClassDB::bind_method(D_METHOD("get_recognized_extensions"), &TextDataFormat::get_recognized_extensions);

	ADD_PROPERTY(PropertyInfo(Variant::NIL, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "options", PROPERTY_HINT_RESOURCE_TYPE, "SerializerOptions"), "set_options", "get_options");
}
