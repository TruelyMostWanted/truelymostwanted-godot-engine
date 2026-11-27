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

#include "core/io/data_schema.h"
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

Ref<DataValidation> TextDataFormat::_validate_data(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const {
	return p_schema->validate(format_data, p_options);
}

void TextDataFormat::_resolve_issue_lines(const Ref<DataValidation> &p_validation, const String &p_text) const {
	p_validation->resolve_lines(p_text);
}

Ref<DataValidation> TextDataFormat::validate(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const {
	ERR_FAIL_COND_V(p_schema.is_null(), Ref<DataValidation>());
	if (!parse_error_message.is_empty()) {
		Ref<DataValidation> result;
		result.instantiate();
		result->add_issue(DataValidation::ISSUE_KIND_PARSE_ERROR, parse_error_message, StringName(), String(), DataValidation::POSITION_NONE, DataValidation::POSITION_NONE, parse_error_line > 0 ? parse_error_line : DataValidation::POSITION_NONE);
		return result;
	}
	Ref<DataValidation> result = _validate_data(p_schema, p_options);
	if (result.is_valid() && p_options.is_valid() && p_options->is_keep_positions() && !parsed_text.is_empty()) {
		_resolve_issue_lines(result, parsed_text);
	}
	return result;
}

void TextDataFormat::_mark_data_changed() {
	parsed_text = String();
	emit_changed();
}

void TextDataFormat::set_data(const Variant &p_data) {
	const Variant::Type type = _get_data_type();
	ERR_FAIL_COND_MSG(type != Variant::NIL && p_data.get_type() != type, vformat("%s data must be of type %s, not %s.", get_class(), Variant::get_type_name(type), Variant::get_type_name(p_data.get_type())));
	format_data = p_data;
	parsed_text = String();
	parse_error_message = String();
	parse_error_line = 0;
	parse_error_column = 0;
	_data_replaced();
	emit_changed();
}

void TextDataFormat::set_options(const Ref<SerializerOptions> &p_options) {
	serializer_options = p_options;
	emit_changed();
}

void TextDataFormat::apply_path_options(const String &p_path) {
	if (serializer_options.is_null()) {
		serializer_options = _get_path_options(p_path);
	}
}

Error TextDataFormat::parse(const String &p_text, bool p_keep_text) {
	Variant parsed;
	String parse_error;
	int line = 0;
	int column = 0;
	const Error err = _parse_data(p_text, serializer_options, parsed, parse_error, line, column);
	parsed_text = p_keep_text ? p_text : String();
	if (err != OK) {
		format_data = _get_empty_data();
		parse_error_message = parse_error;
		parse_error_line = line;
		parse_error_column = column;
	} else {
		format_data = parsed;
		parse_error_message = String();
		parse_error_line = 0;
		parse_error_column = 0;
	}
	_data_replaced();
	emit_changed();
	return err;
}

String TextDataFormat::encode_to_text() const {
	String result;
	String encode_error;
	if (_encode_data(format_data, serializer_options, result, encode_error) != OK) {
		ERR_PRINT(vformat("Can't encode %s data: %s", get_class(), encode_error));
		return String();
	}
	return result;
}

Error TextDataFormat::get_save_text(String &r_text, String &r_error) const {
	if (!parsed_text.is_empty()) {
		r_text = parsed_text;
		return OK;
	}
	return _encode_data(format_data, serializer_options, r_text, r_error);
}

void TextDataFormat::clear() {
	format_data = _get_empty_data();
	parsed_text = String();
	parse_error_message = String();
	parse_error_line = 0;
	parse_error_column = 0;
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
	apply_path_options(p_path);
	return parse(file_text);
}

Error TextDataFormat::save(const String &p_path) {
	apply_path_options(p_path);
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
	ClassDB::bind_method(D_METHOD("validate", "schema", "options"), &TextDataFormat::validate, DEFVAL(Ref<DataValidationOptions>()));
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
