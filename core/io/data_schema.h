/**************************************************************************/
/*  data_schema.h                                                         */
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

#include "core/io/data_field.h"
#include "core/io/data_validation.h"
#include "core/io/resource.h"
#include "core/variant/typed_array.h"

// Describes the fields of a data file independent of its format (CSV, INI, JSON, TOML, YAML, XML) and
// validates parsed data against them. validate() takes a Dictionary (fields by key), an Array of
// Dictionaries (records) or an Array of rows (cells by position, see validate_table()).
class DataSchema : public Resource {
	GDCLASS(DataSchema, Resource);

public:
	enum ExtraFields {
		EXTRA_FIELDS_REJECT,
		EXTRA_FIELDS_ALLOW,
		EXTRA_FIELDS_MATCH_TEMPLATE,
		EXTRA_FIELDS_MAX,
	};

private:
	TypedArray<DataField> fields;
	ExtraFields extra_fields = EXTRA_FIELDS_REJECT;
	Ref<DataField> extra_field_template;
	PackedStringArray file_patterns;

	const DataField *_get_field_ptr(int p_index) const;
	// Checks one value (empty, type, hint, nested schemas). Returns true and the typed value if it passes.
	static bool _check_value(const DataField *p_field, const StringName &p_name, const Variant &p_value, const String &p_parent_path, int p_row, int p_column, DataValidation *r_validation, Variant &r_typed);
	void _validate_dictionary(const Dictionary &p_data, const String &p_path, int p_row, DataValidation *r_validation) const;
	void _validate_records(const Array &p_records, const String &p_path, int p_row, DataValidation *r_validation) const;

protected:
	static void _bind_methods();

public:
	void set_fields(const TypedArray<DataField> &p_fields);
	TypedArray<DataField> get_fields() const { return fields; }
	void set_extra_fields(ExtraFields p_extra_fields);
	ExtraFields get_extra_fields() const { return extra_fields; }
	void set_extra_field_template(const Ref<DataField> &p_template);
	Ref<DataField> get_extra_field_template() const { return extra_field_template; }
	void set_file_patterns(const PackedStringArray &p_patterns);
	PackedStringArray get_file_patterns() const { return file_patterns; }

	Ref<DataField> get_field(const StringName &p_name) const;
	int find_field(const StringName &p_name) const;
	void add_field(const Ref<DataField> &p_field);
	void remove_field(int p_index);

	Ref<DataValidation> validate(const Variant &p_data, const Ref<DataValidationOptions> &p_options = Ref<DataValidationOptions>()) const;
	// Rows of cells (Array of PackedStringArray, as in CSV). With a header, columns are matched to fields by
	// name; without one, column i is field i. Issues carry the row and column index.
	void validate_table(const Array &p_rows, const PackedStringArray &p_header, DataValidation *r_validation) const;
	// Field of a table column (by header name, or by position without a header), the extra field template
	// with EXTRA_FIELDS_MATCH_TEMPLATE, or null.
	const DataField *get_column_field(const PackedStringArray &p_header, int p_column) const;
	Variant convert_value(const StringName &p_field_name, const String &p_text) const;

	static Ref<DataSchema> create_from_dictionary(const Dictionary &p_dictionary);
};

VARIANT_ENUM_CAST(DataSchema::ExtraFields);
