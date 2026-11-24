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

static String _csv_cell_text(const Variant &p_value) {
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
				cells.push_back(_csv_cell_text(record.get(column, Variant())));
			}
		} else if (row.get_type() == Variant::ARRAY || row.get_type() == Variant::PACKED_STRING_ARRAY) {
			const Array values = row;
			for (int j = 0; j < values.size(); j++) {
				cells.push_back(_csv_cell_text(values[j]));
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

Error CSV::parse(const String &p_text, const Ref<SerializerOptions> &p_options) {
	return parse_text(p_text, p_options, data, error_message, error_line);
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

void CSV::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "text", "options"), &CSV::parse, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_method(D_METHOD("get_data"), &CSV::get_data);
	ClassDB::bind_method(D_METHOD("get_error_message"), &CSV::get_error_message);
	ClassDB::bind_method(D_METHOD("get_error_line"), &CSV::get_error_line);

	ClassDB::bind_static_method("CSV", D_METHOD("parse_string", "text", "options"), &CSV::parse_string, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("stringify", "data", "options"), &CSV::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("serialize", "resource", "options"), &CSV::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("deserialize", "text", "type_hint", "options"), &CSV::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
