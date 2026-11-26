/**************************************************************************/
/*  data_field.h                                                          */
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

#pragma once

#include "core/io/data_validation.h"
#include "core/io/resource.h"
#include "core/string/regex.h"
#include "core/templates/hash_set.h"
#include "core/variant/binder_common.h"

class DataSchema;

// One field of a DataSchema: a key, column or attribute of a data file, described like a PropertyInfo
// (type, hint, hint_string) plus the rules of a data file. `hint_string` and `pattern` are parsed once when set.
class DataField : public Resource {
	GDCLASS(DataField, Resource);

	StringName field_name;
	Variant::Type type = Variant::STRING;
	PropertyHint hint = PROPERTY_HINT_NONE;
	String hint_string;
	bool required = false;
	bool unique = false;
	bool allow_empty = true;
	Variant default_value;
	String pattern;
	String description;
	Ref<DataSchema> schema;
	Ref<DataSchema> element_schema;
	Variant::Type element_type = Variant::NIL;

	// Parsed hint_string.
	bool has_range = false;
	double range_min = 0.0;
	double range_max = 0.0;
	bool range_or_greater = false;
	bool range_or_less = false;
	HashSet<String> enum_names;
	HashSet<int64_t> enum_values;
	// `pattern` compiled by set_pattern().
	Ref<RegEx> pattern_regex;

	void _update_hint_cache();

protected:
	static void _bind_methods();

public:
	void set_field_name(const StringName &p_name);
	StringName get_field_name() const { return field_name; }
	void set_type(Variant::Type p_type);
	Variant::Type get_type() const { return type; }
	void set_hint(PropertyHint p_hint);
	PropertyHint get_hint() const { return hint; }
	void set_hint_string(const String &p_hint_string);
	String get_hint_string() const { return hint_string; }
	void set_required(bool p_required);
	bool is_required() const { return required; }
	void set_unique(bool p_unique);
	bool is_unique() const { return unique; }
	void set_allow_empty(bool p_allow_empty);
	bool is_allow_empty() const { return allow_empty; }
	void set_default_value(const Variant &p_value);
	Variant get_default_value() const { return default_value; }
	void set_pattern(const String &p_pattern);
	String get_pattern() const { return pattern; }
	void set_description(const String &p_description);
	String get_description() const { return description; }
	void set_schema(const Ref<DataSchema> &p_schema);
	Ref<DataSchema> get_schema() const;
	void set_element_schema(const Ref<DataSchema> &p_schema);
	Ref<DataSchema> get_element_schema() const;
	void set_element_type(Variant::Type p_type);
	Variant::Type get_element_type() const { return element_type; }

	void set_range(double p_min, double p_max, double p_step = 1.0);
	void set_enum_values(const PackedStringArray &p_values);

	// Checks a value of the field type against the parsed hint and the pattern. Returns ISSUE_KIND_MAX if it
	// passes, otherwise the issue kind and a message.
	DataValidation::IssueKind check_hint(const Variant &p_value, String &r_message) const;
	// Typed value for `p_text` (empty text: default_value). Returns false if the text does not convert.
	bool convert_text(const String &p_text, Variant &r_value) const;
	// The value as cell text: numbers, bools and strings plainly, other types with var_to_str().
	static String value_to_text(const Variant &p_value);
	// Converts `p_value` to `p_type` if it has that type, is text of that type, or is a compatible number or
	// array (an int field takes a float without fraction, as JSON numbers are floats). NIL = any type.
	static bool coerce(Variant::Type p_type, const Variant &p_value, Variant &r_value);
	static bool text_to_type(Variant::Type p_type, const String &p_text, Variant &r_value);
	static bool is_data_type(Variant::Type p_type);

	DataField();
	~DataField();
};
