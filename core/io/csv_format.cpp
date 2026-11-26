/**************************************************************************/
/*  csv_format.cpp                                                        */
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

#include "csv_format.h"

#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

static String _csv_format_cell_text(const Variant &p_value) {
	return p_value.get_type() == Variant::NIL ? String() : String(p_value);
}

Error CSV::parse_text(const String &p_text, const Ref<SerializerOptions> &p_options, Array &r_data, String &r_error, int &r_line) {
	r_data = Array();
	r_error = String();
	r_line = 0;
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	Vector<Vector<String>> rows;
	const Error err = ResourceTextSerializer::parse_csv_rows(p_text, ResourceTextSerializer::get_csv_delimiter(options), rows, r_error, r_line);
	if (err != OK) {
		return err;
	}

	Array result;
	if (!options.csv_headers) {
		for (const Vector<String> &row : rows) {
			PackedStringArray cells;
			for (const String &cell : row) {
				cells.push_back(cell);
			}
			result.push_back(cells);
		}
		r_data = result;
		return OK;
	}

	if (rows.is_empty()) {
		r_data = result;
		return OK;
	}
	const Vector<String> &columns = rows[0];
	for (int i = 1; i < rows.size(); i++) {
		const Vector<String> &row = rows[i];
		if (row.size() > columns.size()) {
			// The row number is the line number unless a quoted cell contains a line break.
			r_error = vformat("Row %d: %d cells, but the header has %d columns.", i + 1, row.size(), columns.size());
			r_line = i + 1;
			return ERR_PARSE_ERROR;
		}
		Dictionary record;
		for (int column = 0; column < columns.size(); column++) {
			record[columns[column]] = column < row.size() ? row[column] : String();
		}
		result.push_back(record);
	}
	r_data = result;
	return OK;
}

Error CSV::write_text(const Array &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) {
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	const char32_t delimiter = ResourceTextSerializer::get_csv_delimiter(options);
	if (p_data.is_empty()) {
		r_text = String();
		return OK;
	}

	const bool records = p_data[0].get_type() == Variant::DICTIONARY;
	Vector<String> columns;
	if (records) {
		HashSet<String> known;
		for (int i = 0; i < p_data.size(); i++) {
			if (p_data[i].get_type() != Variant::DICTIONARY) {
				r_error = vformat("Row %d: all rows must be dictionaries when the first row is one.", i + 1);
				return ERR_INVALID_DATA;
			}
			const Dictionary record = p_data[i];
			for (const KeyValue<Variant, Variant> &kv : record) {
				const String column = kv.key;
				if (!known.has(column)) {
					known.insert(column);
					columns.push_back(column);
				}
			}
		}
		if (options.sort_keys) {
			columns.sort();
		}
	}

	String text;
	if (records && options.csv_headers) {
		text += ResourceTextSerializer::write_csv_row(columns, delimiter);
	}
	for (int i = 0; i < p_data.size(); i++) {
		const Variant &row = p_data[i];
		Vector<String> cells;
		if (records) {
			const Dictionary record = row;
			for (const String &column : columns) {
				cells.push_back(_csv_format_cell_text(record.get(column, Variant())));
			}
		} else if (row.get_type() == Variant::ARRAY || row.get_type() == Variant::PACKED_STRING_ARRAY) {
			const Array values = row;
			for (int j = 0; j < values.size(); j++) {
				cells.push_back(_csv_format_cell_text(values[j]));
			}
		} else {
			r_error = vformat("Row %d: a row must be an Array, a PackedStringArray or a Dictionary.", i + 1);
			return ERR_INVALID_DATA;
		}
		text += ResourceTextSerializer::write_csv_row(cells, delimiter);
	}
	r_text = text;
	return OK;
}

Error CSV::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	r_column = 0;
	header = PackedStringArray();
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	Vector<Vector<String>> rows;
	const Error err = ResourceTextSerializer::parse_csv_rows(p_text, ResourceTextSerializer::get_csv_delimiter(options), rows, r_error, r_line);
	if (err != OK) {
		return err;
	}
	Array result;
	int first = 0;
	if (options.csv_headers && !rows.is_empty()) {
		for (const String &column : rows[0]) {
			header.push_back(column);
		}
		first = 1;
	}
	for (int i = first; i < rows.size(); i++) {
		const Vector<String> &row = rows[i];
		if (first == 1 && row.size() > header.size()) {
			// The row number is the line number unless a quoted cell contains a line break.
			r_error = vformat("Row %d: %d cells, but the header has %d columns.", i + 1, row.size(), header.size());
			r_line = i + 1;
			header = PackedStringArray();
			return ERR_PARSE_ERROR;
		}
		PackedStringArray cells;
		for (const String &cell : row) {
			cells.push_back(cell);
		}
		result.push_back(cells);
	}
	r_data = result;
	return OK;
}

Error CSV::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	const char32_t delimiter = ResourceTextSerializer::get_csv_delimiter(options);
	const Array rows = p_data;
	String text;
	if (!header.is_empty()) {
		Vector<String> cells;
		for (const String &column : header) {
			cells.push_back(column);
		}
		text += ResourceTextSerializer::write_csv_row(cells, delimiter);
	}
	for (int i = 0; i < rows.size(); i++) {
		const Variant &row = rows[i];
		if (row.get_type() != Variant::PACKED_STRING_ARRAY && row.get_type() != Variant::ARRAY) {
			r_error = vformat("Row %d: a row must be a PackedStringArray.", i + 1);
			return ERR_INVALID_DATA;
		}
		const PackedStringArray values = row;
		Vector<String> cells;
		for (const String &value : values) {
			cells.push_back(value);
		}
		text += ResourceTextSerializer::write_csv_row(cells, delimiter);
	}
	r_text = text;
	return OK;
}

void CSV::_get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("csv");
}

Array CSV::_get_rows() const {
	return get_data();
}

void CSV::set_header(const PackedStringArray &p_header) {
	header = p_header;
	_mark_data_changed();
}

int CSV::get_row_count() const {
	return _get_rows().size();
}

int CSV::get_column_count() const {
	int count = header.size();
	const Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		count = MAX(count, (int)PackedStringArray(rows[i]).size());
	}
	return count;
}

PackedStringArray CSV::get_row(int p_row) const {
	const Array rows = _get_rows();
	ERR_FAIL_INDEX_V(p_row, rows.size(), PackedStringArray());
	return rows[p_row];
}

Dictionary CSV::get_row_as_dictionary(int p_row) const {
	const Array rows = _get_rows();
	ERR_FAIL_INDEX_V(p_row, rows.size(), Dictionary());
	ERR_FAIL_COND_V_MSG(header.is_empty(), Dictionary(), "The CSV has no header: use get_row() instead.");
	const PackedStringArray cells = rows[p_row];
	Dictionary result;
	for (int column = 0; column < header.size(); column++) {
		result[header[column]] = column < cells.size() ? cells[column] : String();
	}
	return result;
}

void CSV::append_row(const PackedStringArray &p_values) {
	Array rows = _get_rows();
	rows.push_back(p_values);
	_mark_data_changed();
}

void CSV::insert_row(int p_row, const PackedStringArray &p_values) {
	Array rows = _get_rows();
	ERR_FAIL_INDEX(p_row, rows.size() + 1);
	rows.insert(p_row, p_values);
	_mark_data_changed();
}

void CSV::remove_row(int p_row) {
	Array rows = _get_rows();
	ERR_FAIL_INDEX(p_row, rows.size());
	rows.remove_at(p_row);
	_mark_data_changed();
}

void CSV::set_row(int p_row, const PackedStringArray &p_values) {
	Array rows = _get_rows();
	ERR_FAIL_INDEX(p_row, rows.size());
	rows[p_row] = p_values;
	_mark_data_changed();
}

String CSV::get_cell(int p_row, int p_column) const {
	const Array rows = _get_rows();
	ERR_FAIL_INDEX_V(p_row, rows.size(), String());
	ERR_FAIL_COND_V(p_column < 0, String());
	const PackedStringArray cells = rows[p_row];
	return p_column < cells.size() ? cells[p_column] : String();
}

void CSV::set_cell(int p_row, int p_column, const String &p_value) {
	Array rows = _get_rows();
	ERR_FAIL_INDEX(p_row, rows.size());
	ERR_FAIL_COND(p_column < 0);
	PackedStringArray cells = rows[p_row];
	while (cells.size() <= p_column) {
		cells.push_back(String());
	}
	cells.set(p_column, p_value);
	rows[p_row] = cells;
	_mark_data_changed();
}

int CSV::find_column(const String &p_name) const {
	return (int)header.find(p_name);
}

PackedStringArray CSV::get_column(int p_column) const {
	ERR_FAIL_COND_V(p_column < 0, PackedStringArray());
	const Array rows = _get_rows();
	PackedStringArray result;
	for (int i = 0; i < rows.size(); i++) {
		const PackedStringArray cells = rows[i];
		result.push_back(p_column < cells.size() ? cells[p_column] : String());
	}
	return result;
}

PackedInt32Array CSV::find_rows(int p_column, const String &p_value) const {
	ERR_FAIL_COND_V(p_column < 0, PackedInt32Array());
	const Array rows = _get_rows();
	PackedInt32Array result;
	for (int i = 0; i < rows.size(); i++) {
		const PackedStringArray cells = rows[i];
		if (p_column < cells.size() && cells[p_column] == p_value) {
			result.push_back(i);
		}
	}
	return result;
}

Array CSV::parse_string(const String &p_text, const Ref<SerializerOptions> &p_options) {
	Array result;
	String error;
	int line = 0;
	if (parse_text(p_text, p_options, result, error, line) != OK) {
		ERR_PRINT("CSV parse error: " + error);
		return Array();
	}
	return result;
}

String CSV::stringify(const Array &p_data, const Ref<SerializerOptions> &p_options) {
	String text;
	String error;
	if (write_text(p_data, p_options, text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

String CSV::serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, String());
	return serializer->serialize(p_resource, ResourceTextSerializer::FORMAT_CSV, p_options);
}

Ref<Resource> CSV::deserialize(const String &p_text, const StringName &p_type_hint, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, Ref<Resource>());
	return serializer->deserialize(p_text, ResourceTextSerializer::FORMAT_CSV, p_type_hint, p_options);
}

CSV::CSV() {
	_get_data_ref() = Array();
}

void CSV::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_header", "header"), &CSV::set_header);
	ClassDB::bind_method(D_METHOD("get_header"), &CSV::get_header);
	ClassDB::bind_method(D_METHOD("get_row_count"), &CSV::get_row_count);
	ClassDB::bind_method(D_METHOD("get_column_count"), &CSV::get_column_count);
	ClassDB::bind_method(D_METHOD("get_row", "row"), &CSV::get_row);
	ClassDB::bind_method(D_METHOD("get_row_as_dictionary", "row"), &CSV::get_row_as_dictionary);
	ClassDB::bind_method(D_METHOD("append_row", "values"), &CSV::append_row);
	ClassDB::bind_method(D_METHOD("insert_row", "row", "values"), &CSV::insert_row);
	ClassDB::bind_method(D_METHOD("remove_row", "row"), &CSV::remove_row);
	ClassDB::bind_method(D_METHOD("set_row", "row", "values"), &CSV::set_row);
	ClassDB::bind_method(D_METHOD("get_cell", "row", "column"), &CSV::get_cell);
	ClassDB::bind_method(D_METHOD("set_cell", "row", "column", "value"), &CSV::set_cell);
	ClassDB::bind_method(D_METHOD("find_column", "name"), &CSV::find_column);
	ClassDB::bind_method(D_METHOD("get_column", "column"), &CSV::get_column);
	ClassDB::bind_method(D_METHOD("find_rows", "column", "value"), &CSV::find_rows);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "header"), "set_header", "get_header");

	ClassDB::bind_static_method("CSV", D_METHOD("parse_string", "text", "options"), &CSV::parse_string, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("stringify", "data", "options"), &CSV::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("serialize", "resource", "options"), &CSV::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("deserialize", "text", "type_hint", "options"), &CSV::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
