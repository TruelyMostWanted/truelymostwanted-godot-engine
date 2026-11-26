/**************************************************************************/
/*  csv_format.h                                                          */
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

#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/variant/array.h"

// A CSV file (RFC 4180) as a Resource: `data` is an Array of rows (PackedStringArray), `header` the column
// names when SerializerOptions.csv_headers is on. Columns are addressed by index; find_column() resolves a
// header name. The static functions also read and write any list Resource as CSV (format FORMAT_CSV of
// ResourceTextSerializer); parse_string() returns rows as Dictionary (with headers) or PackedStringArray.
class CSV : public TextDataFormat {
	GDCLASS(CSV, TextDataFormat);

	PackedStringArray header;

	Array _get_rows() const;

protected:
	static void _bind_methods();

	virtual Error _parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) override;
	virtual Error _encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const override;
	virtual Variant _get_empty_data() const override { return Array(); }
	virtual Variant::Type _get_data_type() const override { return Variant::ARRAY; }
	virtual void _get_extensions(List<String> *r_extensions) const override;
	virtual void _cleared() override { header = PackedStringArray(); }
	virtual Ref<DataValidation> _validate_data(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const override;
	virtual void _resolve_issue_lines(const Ref<DataValidation> &p_validation, const String &p_text) const override;

public:
	// On error, r_line is the 1-based line of the problem.
	static Error parse_text(const String &p_text, const Ref<SerializerOptions> &p_options, Array &r_data, String &r_error, int &r_line);
	// Rows are arrays (PackedStringArray or Array) or dictionaries (columns in order of first appearance).
	static Error write_text(const Array &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error);

	void set_header(const PackedStringArray &p_header);
	PackedStringArray get_header() const { return header; }

	int get_row_count() const;
	int get_column_count() const;
	PackedStringArray get_row(int p_row) const;
	Dictionary get_row_as_dictionary(int p_row) const;
	void append_row(const PackedStringArray &p_values);
	void insert_row(int p_row, const PackedStringArray &p_values);
	void remove_row(int p_row);
	void set_row(int p_row, const PackedStringArray &p_values);
	String get_cell(int p_row, int p_column) const;
	void set_cell(int p_row, int p_column, const String &p_value);
	int find_column(const String &p_name) const;
	PackedStringArray get_column(int p_column) const;
	PackedInt32Array find_rows(int p_column, const String &p_value) const;

	// Typed access through a schema: cells converted to the type of their field (by header name, or by
	// position without a header); empty or unconvertible cells give the field's default_value.
	Dictionary get_typed_row(int p_row, const Ref<DataSchema> &p_schema) const;
	Error append_typed_row(const Dictionary &p_values, const Ref<DataSchema> &p_schema);
	Variant get_typed_column(int p_column, const Ref<DataSchema> &p_schema) const;

	static Array parse_string(const String &p_text, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String stringify(const Array &p_data, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());

	CSV();
};
