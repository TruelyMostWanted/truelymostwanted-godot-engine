/**************************************************************************/
/*  serializer_options.cpp                                                */
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

#include "serializer_options.h"

#include "core/object/class_db.h"

void SerializerOptions::set_indent(const String &p_indent) {
	indent = p_indent;
	emit_changed();
}

String SerializerOptions::get_indent() const {
	return indent;
}

void SerializerOptions::set_enums_as_names(bool p_enable) {
	enums_as_names = p_enable;
	emit_changed();
}

bool SerializerOptions::is_enums_as_names() const {
	return enums_as_names;
}

void SerializerOptions::set_defaults(const Ref<Resource> &p_defaults) {
	defaults = p_defaults;
	emit_changed();
}

Ref<Resource> SerializerOptions::get_defaults() const {
	return defaults;
}

void SerializerOptions::set_include_type_marker(bool p_enable) {
	include_type_marker = p_enable;
	emit_changed();
}

bool SerializerOptions::is_include_type_marker() const {
	return include_type_marker;
}

void SerializerOptions::set_sort_keys(bool p_enable) {
	sort_keys = p_enable;
	emit_changed();
}

bool SerializerOptions::is_sort_keys() const {
	return sort_keys;
}

void SerializerOptions::set_full_precision_floats(bool p_enable) {
	full_precision_floats = p_enable;
	emit_changed();
}

bool SerializerOptions::is_full_precision_floats() const {
	return full_precision_floats;
}

void SerializerOptions::set_csv_delimiter(const String &p_delimiter) {
	csv_delimiter = p_delimiter;
	emit_changed();
}

String SerializerOptions::get_csv_delimiter() const {
	return csv_delimiter;
}

void SerializerOptions::set_csv_headers(bool p_enable) {
	csv_headers = p_enable;
	emit_changed();
}

bool SerializerOptions::is_csv_headers() const {
	return csv_headers;
}

void SerializerOptions::set_ini_plain_values(bool p_enable) {
	ini_plain_values = p_enable;
	emit_changed();
}

bool SerializerOptions::is_ini_plain_values() const {
	return ini_plain_values;
}

void SerializerOptions::set_xml_declaration(bool p_enable) {
	xml_declaration = p_enable;
	emit_changed();
}

bool SerializerOptions::is_xml_declaration() const {
	return xml_declaration;
}

void SerializerOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_indent", "indent"), &SerializerOptions::set_indent);
	ClassDB::bind_method(D_METHOD("get_indent"), &SerializerOptions::get_indent);
	ClassDB::bind_method(D_METHOD("set_enums_as_names", "enable"), &SerializerOptions::set_enums_as_names);
	ClassDB::bind_method(D_METHOD("is_enums_as_names"), &SerializerOptions::is_enums_as_names);
	ClassDB::bind_method(D_METHOD("set_defaults", "defaults"), &SerializerOptions::set_defaults);
	ClassDB::bind_method(D_METHOD("get_defaults"), &SerializerOptions::get_defaults);
	ClassDB::bind_method(D_METHOD("set_include_type_marker", "enable"), &SerializerOptions::set_include_type_marker);
	ClassDB::bind_method(D_METHOD("is_include_type_marker"), &SerializerOptions::is_include_type_marker);
	ClassDB::bind_method(D_METHOD("set_sort_keys", "enable"), &SerializerOptions::set_sort_keys);
	ClassDB::bind_method(D_METHOD("is_sort_keys"), &SerializerOptions::is_sort_keys);
	ClassDB::bind_method(D_METHOD("set_full_precision_floats", "enable"), &SerializerOptions::set_full_precision_floats);
	ClassDB::bind_method(D_METHOD("is_full_precision_floats"), &SerializerOptions::is_full_precision_floats);
	ClassDB::bind_method(D_METHOD("set_csv_delimiter", "delimiter"), &SerializerOptions::set_csv_delimiter);
	ClassDB::bind_method(D_METHOD("get_csv_delimiter"), &SerializerOptions::get_csv_delimiter);
	ClassDB::bind_method(D_METHOD("set_csv_headers", "enable"), &SerializerOptions::set_csv_headers);
	ClassDB::bind_method(D_METHOD("is_csv_headers"), &SerializerOptions::is_csv_headers);
	ClassDB::bind_method(D_METHOD("set_ini_plain_values", "enable"), &SerializerOptions::set_ini_plain_values);
	ClassDB::bind_method(D_METHOD("is_ini_plain_values"), &SerializerOptions::is_ini_plain_values);
	ClassDB::bind_method(D_METHOD("set_xml_declaration", "enable"), &SerializerOptions::set_xml_declaration);
	ClassDB::bind_method(D_METHOD("is_xml_declaration"), &SerializerOptions::is_xml_declaration);

	ADD_GROUP("Common", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "indent"), "set_indent", "get_indent");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enums_as_names"), "set_enums_as_names", "is_enums_as_names");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "defaults", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), "set_defaults", "get_defaults");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_type_marker"), "set_include_type_marker", "is_include_type_marker");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sort_keys"), "set_sort_keys", "is_sort_keys");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "full_precision_floats"), "set_full_precision_floats", "is_full_precision_floats");
	ADD_GROUP("CSV", "csv_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "csv_delimiter"), "set_csv_delimiter", "get_csv_delimiter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "csv_headers"), "set_csv_headers", "is_csv_headers");
	ADD_GROUP("INI", "ini_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ini_plain_values"), "set_ini_plain_values", "is_ini_plain_values");
	ADD_GROUP("XML", "xml_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "xml_declaration"), "set_xml_declaration", "is_xml_declaration");
}
