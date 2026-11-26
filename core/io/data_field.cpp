/**************************************************************************/
/*  data_field.cpp                                                        */
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

#include "data_field.h"

#include "core/core_constants.h"
#include "core/io/data_schema.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/variant/variant_utility.h"

static bool _data_field_is_array_type(Variant::Type p_type) {
	return p_type == Variant::ARRAY || (p_type >= Variant::PACKED_BYTE_ARRAY && p_type < Variant::VARIANT_MAX);
}

void DataField::_update_hint_cache() {
	has_range = false;
	range_or_greater = false;
	range_or_less = false;
	enum_names.clear();
	enum_values.clear();
	const Vector<String> parts = hint_string.split(",", false);
	if (hint == PROPERTY_HINT_RANGE && parts.size() >= 2 && parts[0].strip_edges().is_valid_float() && parts[1].strip_edges().is_valid_float()) {
		has_range = true;
		range_min = parts[0].strip_edges().to_float();
		range_max = parts[1].strip_edges().to_float();
		for (int i = 2; i < parts.size(); i++) {
			const String part = parts[i].strip_edges();
			range_or_greater = range_or_greater || part == "or_greater";
			range_or_less = range_or_less || part == "or_less" || part == "or_lesser";
		}
	} else if (hint == PROPERTY_HINT_ENUM) {
		// "A,B:5,C" like @export_enum: values count on from the last explicit value.
		int64_t next_value = 0;
		for (const String &part : parts) {
			String enum_name = part.strip_edges();
			const int colon = enum_name.rfind(":");
			if (colon >= 0 && enum_name.substr(colon + 1).strip_edges().is_valid_int()) {
				next_value = enum_name.substr(colon + 1).strip_edges().to_int();
				enum_name = enum_name.substr(0, colon).strip_edges();
			}
			enum_names.insert(enum_name);
			enum_values.insert(next_value);
			next_value++;
		}
	}
}

void DataField::set_field_name(const StringName &p_name) {
	field_name = p_name;
	emit_changed();
}

void DataField::set_type(Variant::Type p_type) {
	ERR_FAIL_COND_MSG(!is_data_type(p_type), vformat("%s is not a data type.", Variant::get_type_name(p_type)));
	type = p_type;
	emit_changed();
}

void DataField::set_hint(PropertyHint p_hint) {
	hint = p_hint;
	_update_hint_cache();
	emit_changed();
}

void DataField::set_hint_string(const String &p_hint_string) {
	hint_string = p_hint_string;
	_update_hint_cache();
	emit_changed();
}

void DataField::set_required(bool p_required) {
	required = p_required;
	emit_changed();
}

void DataField::set_unique(bool p_unique) {
	unique = p_unique;
	emit_changed();
}

void DataField::set_allow_empty(bool p_allow_empty) {
	allow_empty = p_allow_empty;
	emit_changed();
}

void DataField::set_default_value(const Variant &p_value) {
	default_value = p_value;
	emit_changed();
}

void DataField::set_pattern(const String &p_pattern) {
	pattern = p_pattern;
	// Compiled once here; the pattern must match the whole value.
	pattern_regex = pattern.is_empty() ? Ref<RegEx>() : RegEx::create_from_string("^(?:" + pattern + ")$");
	emit_changed();
}

void DataField::set_description(const String &p_description) {
	description = p_description;
	emit_changed();
}

void DataField::set_schema(const Ref<DataSchema> &p_schema) {
	schema = p_schema;
	emit_changed();
}

Ref<DataSchema> DataField::get_schema() const {
	return schema;
}

void DataField::set_element_schema(const Ref<DataSchema> &p_schema) {
	element_schema = p_schema;
	emit_changed();
}

Ref<DataSchema> DataField::get_element_schema() const {
	return element_schema;
}

void DataField::set_element_type(Variant::Type p_type) {
	ERR_FAIL_COND_MSG(!is_data_type(p_type), vformat("%s is not a data type.", Variant::get_type_name(p_type)));
	element_type = p_type;
	emit_changed();
}

void DataField::set_range(double p_min, double p_max, double p_step) {
	hint = PROPERTY_HINT_RANGE;
	set_hint_string(vformat("%s,%s,%s", String::num(p_min), String::num(p_max), String::num(p_step)));
}

void DataField::set_enum_values(const PackedStringArray &p_values) {
	hint = PROPERTY_HINT_ENUM;
	set_hint_string(String(",").join(p_values));
}

DataValidation::IssueKind DataField::check_hint(const Variant &p_value, String &r_message) const {
	const Variant::Type value_type = p_value.get_type();
	if (has_range && (value_type == Variant::INT || value_type == Variant::FLOAT)) {
		const double number = p_value;
		if ((number < range_min && !range_or_less) || (number > range_max && !range_or_greater)) {
			r_message = vformat("%s is outside the range %s to %s.", String(p_value), String::num(range_min), String::num(range_max));
			return DataValidation::ISSUE_KIND_OUT_OF_RANGE;
		}
	}
	if (hint == PROPERTY_HINT_ENUM && !enum_names.is_empty()) {
		if (value_type == Variant::INT ? !enum_values.has((int64_t)p_value) : !enum_names.has(String(p_value))) {
			r_message = vformat("\"%s\" is not one of: %s.", String(p_value), hint_string);
			return DataValidation::ISSUE_KIND_NOT_IN_ENUM;
		}
	}
	if (pattern_regex.is_valid() && pattern_regex->is_valid() && pattern_regex->search(String(p_value)).is_null()) {
		r_message = vformat("\"%s\" does not match the pattern \"%s\".", String(p_value), pattern);
		return DataValidation::ISSUE_KIND_PATTERN_MISMATCH;
	}
	return DataValidation::ISSUE_KIND_MAX;
}

bool DataField::convert_text(const String &p_text, Variant &r_value) const {
	if (p_text.is_empty()) {
		r_value = default_value;
		return true;
	}
	return text_to_type(type, p_text, r_value);
}

String DataField::value_to_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return String();
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			return String(p_value);
		default:
			return VariantUtilityFunctions::var_to_str(p_value);
	}
}

bool DataField::coerce(Variant::Type p_type, const Variant &p_value, Variant &r_value) {
	const Variant::Type value_type = p_value.get_type();
	if (p_type == Variant::NIL || value_type == p_type) {
		r_value = p_value;
		return true;
	}
	if (value_type == Variant::STRING || value_type == Variant::STRING_NAME) {
		return text_to_type(p_type, p_value, r_value);
	}
	if (p_type == Variant::INT && value_type == Variant::FLOAT) {
		const double number = p_value;
		if (Math::is_finite(number) && number == Math::floor(number)) {
			r_value = (int64_t)number;
			return true;
		}
		return false;
	}
	if (p_type == Variant::FLOAT && value_type == Variant::INT) {
		r_value = (double)(int64_t)p_value;
		return true;
	}
	if (_data_field_is_array_type(p_type) && _data_field_is_array_type(value_type)) {
		r_value = VariantUtilityFunctions::type_convert(p_value, p_type);
		return true;
	}
	return false;
}

bool DataField::text_to_type(Variant::Type p_type, const String &p_text, Variant &r_value) {
	const String text = p_text.strip_edges();
	switch (p_type) {
		case Variant::NIL:
		case Variant::STRING:
			r_value = p_text;
			return true;
		case Variant::STRING_NAME:
			r_value = StringName(p_text);
			return true;
		case Variant::NODE_PATH:
			r_value = NodePath(p_text);
			return true;
		case Variant::BOOL: {
			const String lower = text.to_lower();
			if (lower == "true" || lower == "1" || lower == "false" || lower == "0") {
				r_value = lower == "true" || lower == "1";
				return true;
			}
			return false;
		}
		case Variant::INT:
			if (text.is_valid_int()) {
				r_value = text.to_int();
				return true;
			}
			return false;
		case Variant::FLOAT:
			if (text.is_valid_float()) {
				r_value = text.to_float();
				return true;
			}
			return false;
		default: {
			// Other types in Godot syntax ("Vector2(1, 2)"), as written by value_to_text().
			const Variant parsed = VariantUtilityFunctions::str_to_var(text);
			if (parsed.get_type() == Variant::STRING || parsed.get_type() == Variant::STRING_NAME) {
				return false;
			}
			return coerce(p_type, parsed, r_value);
		}
	}
}

bool DataField::is_data_type(Variant::Type p_type) {
	return p_type >= Variant::NIL && p_type < Variant::VARIANT_MAX && p_type != Variant::OBJECT && p_type != Variant::RID && p_type != Variant::CALLABLE && p_type != Variant::SIGNAL;
}

void DataField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_field_name", "name"), &DataField::set_field_name);
	ClassDB::bind_method(D_METHOD("get_field_name"), &DataField::get_field_name);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &DataField::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &DataField::get_type);
	ClassDB::bind_method(D_METHOD("set_hint", "hint"), &DataField::set_hint);
	ClassDB::bind_method(D_METHOD("get_hint"), &DataField::get_hint);
	ClassDB::bind_method(D_METHOD("set_hint_string", "hint_string"), &DataField::set_hint_string);
	ClassDB::bind_method(D_METHOD("get_hint_string"), &DataField::get_hint_string);
	ClassDB::bind_method(D_METHOD("set_required", "required"), &DataField::set_required);
	ClassDB::bind_method(D_METHOD("is_required"), &DataField::is_required);
	ClassDB::bind_method(D_METHOD("set_unique", "unique"), &DataField::set_unique);
	ClassDB::bind_method(D_METHOD("is_unique"), &DataField::is_unique);
	ClassDB::bind_method(D_METHOD("set_allow_empty", "allow_empty"), &DataField::set_allow_empty);
	ClassDB::bind_method(D_METHOD("is_allow_empty"), &DataField::is_allow_empty);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &DataField::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &DataField::get_default_value);
	ClassDB::bind_method(D_METHOD("set_pattern", "pattern"), &DataField::set_pattern);
	ClassDB::bind_method(D_METHOD("get_pattern"), &DataField::get_pattern);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &DataField::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &DataField::get_description);
	ClassDB::bind_method(D_METHOD("set_schema", "schema"), &DataField::set_schema);
	ClassDB::bind_method(D_METHOD("get_schema"), &DataField::get_schema);
	ClassDB::bind_method(D_METHOD("set_element_schema", "schema"), &DataField::set_element_schema);
	ClassDB::bind_method(D_METHOD("get_element_schema"), &DataField::get_element_schema);
	ClassDB::bind_method(D_METHOD("set_element_type", "type"), &DataField::set_element_type);
	ClassDB::bind_method(D_METHOD("get_element_type"), &DataField::get_element_type);
	ClassDB::bind_method(D_METHOD("set_range", "min", "max", "step"), &DataField::set_range, DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("set_enum_values", "values"), &DataField::set_enum_values);

	// Inspector lists: data types only, and the PropertyHint values.
	String types;
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (is_data_type(Variant::Type(i))) {
			types += vformat("%s%s:%d", types.is_empty() ? "" : ",", i == Variant::NIL ? String("Any") : Variant::get_type_name(Variant::Type(i)), i);
		}
	}
	String hints;
	for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
		if (CoreConstants::get_global_constant_enum(i) == StringName("PropertyHint") && CoreConstants::get_global_constant_value(i) != PROPERTY_HINT_MAX) {
			const String hint_name = String(CoreConstants::get_global_constant_name(i)).trim_prefix("PROPERTY_HINT_").capitalize();
			hints += vformat("%s%s:%d", hints.is_empty() ? "" : ",", hint_name, CoreConstants::get_global_constant_value(i));
		}
	}

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name"), "set_field_name", "get_field_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, types), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hint", PROPERTY_HINT_ENUM, hints), "set_hint", "get_hint");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "hint_string"), "set_hint_string", "get_hint_string");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "required"), "set_required", "is_required");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unique"), "set_unique", "is_unique");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_empty"), "set_allow_empty", "is_allow_empty");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "default_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_default_value", "get_default_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pattern"), "set_pattern", "get_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "schema", PROPERTY_HINT_RESOURCE_TYPE, "DataSchema"), "set_schema", "get_schema");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "element_schema", PROPERTY_HINT_RESOURCE_TYPE, "DataSchema"), "set_element_schema", "get_element_schema");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "element_type", PROPERTY_HINT_ENUM, types), "set_element_type", "get_element_type");
}

DataField::DataField() {
}

DataField::~DataField() {
}
