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

#include "core/io/data_schema.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/string/translation_server.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant_utility.h"

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
		// A row wider than the header (set_cell(), set_cells()) gets unnamed columns: the parser rejects
		// rows wider than the header, and a failed re-parse of this text would drop the data.
		for (int i = 0; i < rows.size(); i++) {
			const PackedStringArray values = rows[i];
			cells.resize(MAX(cells.size(), values.size()));
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
	r_extensions->push_back("tsv");
}

Ref<SerializerOptions> CSV::_get_path_options(const String &p_path) const {
	if (p_path.get_extension().nocasecmp_to("tsv") != 0) {
		return Ref<SerializerOptions>();
	}
	// Tab-separated values: the other options keep their defaults.
	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_csv_delimiter("\t");
	return options;
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

Array CSV::get_cells(int p_row, int p_column, int p_row_count, int p_column_count) const {
	ERR_FAIL_COND_V(p_row < 0 || p_column < 0 || p_row_count < 0 || p_column_count < 0, Array());
	const Array rows = _get_rows();
	Array result;
	for (int row = p_row; row < MIN(p_row + p_row_count, rows.size()); row++) {
		const PackedStringArray cells = rows[row];
		PackedStringArray block_row;
		block_row.resize(p_column_count);
		for (int column = 0; column < p_column_count; column++) {
			block_row.set(column, p_column + column < cells.size() ? cells[p_column + column] : String());
		}
		result.push_back(block_row);
	}
	return result;
}

void CSV::set_cells(int p_row, int p_column, const Array &p_cells) {
	Array rows = _get_rows();
	ERR_FAIL_INDEX(p_row, rows.size() + 1);
	ERR_FAIL_COND(p_column < 0);
	for (int i = 0; i < p_cells.size(); i++) {
		const PackedStringArray values = p_cells[i];
		if (p_row + i == rows.size()) {
			rows.push_back(PackedStringArray());
		}
		PackedStringArray cells = rows[p_row + i];
		if (cells.size() < p_column + values.size()) {
			cells.resize(p_column + values.size());
		}
		for (int column = 0; column < values.size(); column++) {
			cells.set(p_column + column, values[column]);
		}
		rows[p_row + i] = cells;
	}
	_mark_data_changed();
}

// Inserts p_value at p_column of a row (or header) wider than p_column; a shorter one gets it only if it is not empty.
// Rows may be shorter than the header, so an empty cell past their end is left out; the header always gets the cell
// (`p_always`), else a column appended without a name would not count.
static PackedStringArray _insert_cell(PackedStringArray p_cells, int p_column, const String &p_value, bool p_always = false) {
	if (p_column < p_cells.size()) {
		p_cells.insert(p_column, p_value);
	} else if (p_always || !p_value.is_empty()) {
		p_cells.resize(p_column);
		p_cells.push_back(p_value);
	}
	return p_cells;
}

void CSV::insert_column(int p_column, const String &p_name, const String &p_value) {
	ERR_FAIL_INDEX(p_column, get_column_count() + 1);
	if (!header.is_empty() || !p_name.is_empty()) {
		header = _insert_cell(header, p_column, p_name, true);
	}
	Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		rows[i] = _insert_cell(rows[i], p_column, p_value);
	}
	_mark_data_changed();
}

void CSV::remove_column(int p_column) {
	ERR_FAIL_INDEX(p_column, get_column_count());
	if (p_column < header.size()) {
		header.remove_at(p_column);
	}
	Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		PackedStringArray cells = rows[i];
		if (p_column < cells.size()) {
			cells.remove_at(p_column);
			rows[i] = cells;
		}
	}
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

Ref<DataValidation> CSV::_validate_data(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const {
	Ref<DataValidation> result;
	result.instantiate();
	result->apply_options(p_options);
	p_schema->validate_table(_get_rows(), header, result.ptr());
	return result;
}

void CSV::_resolve_issue_lines(const Ref<DataValidation> &p_validation, const String &p_text) const {
	// The line on which each record starts, with the quote and line break rules of parse_csv_rows():
	// a quoted cell can span lines, empty lines are no records.
	const char32_t delimiter = ResourceTextSerializer::get_csv_delimiter(ResourceTextSerializer::get_options(get_options()));
	LocalVector<int> record_lines;
	bool in_quotes = false;
	bool in_record = false;
	bool cell_start = true;
	int line = 1;
	const int length = p_text.length();
	for (int i = (length > 0 && p_text[0] == 0xFEFF) ? 1 : 0; i < length; i++) {
		const char32_t c = p_text[i];
		if (in_quotes) {
			if (c == '"') {
				if (i + 1 < length && p_text[i + 1] == '"') {
					i++;
				} else {
					in_quotes = false;
				}
			} else if (c == '\n') {
				line++;
			}
			continue;
		}
		if (c == '\n' || c == '\r') {
			if (c == '\r' && i + 1 < length && p_text[i + 1] == '\n') {
				i++;
			}
			if (c == '\n' || p_text[i] == '\n') {
				line++;
			}
			in_record = false;
			cell_start = true;
			continue;
		}
		if (!in_record) {
			in_record = true;
			record_lines.push_back(line);
		}
		in_quotes = c == '"' && cell_start;
		cell_start = c == delimiter;
	}

	// Data row n is record n + 1 when the first record is the header; issues without a row belong to the header.
	const int offset = header.is_empty() ? 0 : 1;
	for (int i = 0; i < p_validation->get_issue_count(); i++) {
		const int row = p_validation->get_issue_row(i);
		if (p_validation->get_issue_line(i) != DataValidation::POSITION_NONE || (row == DataValidation::POSITION_NONE && offset == 0)) {
			continue;
		}
		const int record = row == DataValidation::POSITION_NONE ? 0 : row + offset;
		if (record < (int)record_lines.size()) {
			p_validation->set_issue_line(i, record_lines[record]);
		}
	}
}

Dictionary CSV::get_typed_row(int p_row, const Ref<DataSchema> &p_schema) const {
	ERR_FAIL_COND_V(p_schema.is_null(), Dictionary());
	const PackedStringArray cells = get_row(p_row);
	const DataField *extra_template = p_schema->get_extra_field_template().ptr();
	Dictionary result;
	const int count = header.is_empty() ? cells.size() : header.size();
	for (int column = 0; column < count; column++) {
		const DataField *field = p_schema->get_column_field(header, column);
		Variant key = column;
		if (!header.is_empty()) {
			key = header[column];
		} else if (field && field != extra_template) {
			key = field->get_field_name();
		}
		const String cell = column < cells.size() ? cells[column] : String();
		Variant value = cell;
		if (field && !field->convert_text(cell, value)) {
			value = field->get_default_value();
		}
		result[key] = value;
	}
	return result;
}

Error CSV::append_typed_row(const Dictionary &p_values, const Ref<DataSchema> &p_schema) {
	ERR_FAIL_COND_V(p_schema.is_null(), ERR_INVALID_PARAMETER);
	PackedStringArray cells;
	const int count = header.is_empty() ? p_schema->get_fields().size() : header.size();
	for (int column = 0; column < count; column++) {
		const DataField *field = p_schema->get_column_field(header, column);
		const StringName key = !header.is_empty() ? StringName(header[column]) : (field ? field->get_field_name() : StringName());
		const Variant *value = p_values.getptr(String(key));
		Variant typed;
		if (!value) {
			typed = field ? field->get_default_value() : Variant();
		} else if (!field) {
			typed = *value;
		} else if (!DataField::coerce(field->get_value_type(), *value, typed)) {
			return ERR_INVALID_DATA;
		}
		cells.push_back(DataField::value_to_text(typed));
	}
	append_row(cells);
	return OK;
}

Variant CSV::get_typed_column(int p_column, const Ref<DataSchema> &p_schema) const {
	ERR_FAIL_COND_V(p_schema.is_null(), Variant());
	const DataField *field = p_schema->get_column_field(header, p_column);
	const Array rows = _get_rows();
	Array values;
	values.resize(rows.size());
	for (int i = 0; i < rows.size(); i++) {
		const PackedStringArray cells = rows[i];
		const String cell = p_column < cells.size() ? cells[p_column] : String();
		Variant value = cell;
		if (field && !field->convert_text(cell, value)) {
			value = field->get_default_value();
		}
		values[i] = value;
	}
	// Packed arrays by field type (one allocation, no Variant per value), an Array for other types.
	Variant::Type packed_type = Variant::ARRAY;
	switch (field ? field->get_value_type() : Variant::STRING) {
		case Variant::INT:
			packed_type = Variant::PACKED_INT64_ARRAY;
			break;
		case Variant::FLOAT:
			packed_type = Variant::PACKED_FLOAT64_ARRAY;
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			packed_type = Variant::PACKED_STRING_ARRAY;
			break;
		case Variant::VECTOR2:
			packed_type = Variant::PACKED_VECTOR2_ARRAY;
			break;
		case Variant::VECTOR3:
			packed_type = Variant::PACKED_VECTOR3_ARRAY;
			break;
		case Variant::VECTOR4:
			packed_type = Variant::PACKED_VECTOR4_ARRAY;
			break;
		case Variant::COLOR:
			packed_type = Variant::PACKED_COLOR_ARRAY;
			break;
		default:
			return values;
	}
	return VariantUtilityFunctions::type_convert(values, packed_type);
}

CSV::CSV() {
	_get_data_ref() = Array();
}

void CSV::check_translation_header(const Vector<String> &p_header, TranslationHeader &r_header, DataValidation *r_validation) {
	ERR_FAIL_NULL(r_validation);
	TranslationServer *ts = TranslationServer::get_singleton();
	HashSet<String> known_languages;
	for (const String &language : ts->get_all_languages()) {
		known_languages.insert(language);
	}

	for (int i = 1; i < p_header.size(); i++) {
		const String &column = p_header[i];
		if (column.begins_with("_")) {
			continue;
		}
		const String lower = column.to_lower();
		if (lower == "?context" || lower == "?plural") {
			int &special_column = lower == "?context" ? r_header.context_column : r_header.plural_column;
			if (special_column != -1) {
				r_validation->add_issue(DataValidation::ISSUE_KIND_DUPLICATE_KEY, vformat("Multiple '%s' columns found. Only one is allowed. Subsequent ones are ignored.", lower), lower, String(), DataValidation::POSITION_NONE, i);
			} else {
				special_column = i;
			}
			continue;
		}

		const String locale = ts->standardize_locale(column);
		if (locale.is_empty()) {
			r_header.invalid_columns.push_back(i);
			r_validation->add_issue(DataValidation::ISSUE_KIND_INVALID_LOCALE, vformat("Invalid locale format '%s', should be 'language_Script_COUNTRY_VARIANT@extra'. The column is ignored.", column), column, String(), DataValidation::POSITION_NONE, i);
			continue;
		}
		if (known_languages.has(locale.get_slicec('_', 0))) {
			r_header.has_known_language = true;
		} else {
			r_header.unknown_columns.push_back(i);
			r_validation->add_issue(DataValidation::ISSUE_KIND_INVALID_LOCALE, vformat("Unknown language code '%s'. The column is used as a locale anyway; prefix its name with '_' to ignore it.", column), column, String(), DataValidation::POSITION_NONE, i);
		}
		r_header.locale_columns[i] = locale;
	}

	if (!r_header.has_known_language) {
		r_validation->add_issue(DataValidation::ISSUE_KIND_INVALID_LOCALE, "No column of the header starts with a known language code: this is not a translation table.");
	}
}

Ref<DataValidation> CSV::validate_translation(const Ref<DataValidationOptions> &p_options) const {
	Ref<DataValidation> result;
	result.instantiate();
	result->apply_options(p_options);
	if (header.is_empty()) {
		result->add_issue(DataValidation::ISSUE_KIND_MISSING_FIELD, "A translation CSV needs a header row: the keys, then one column per locale.");
		return result;
	}

	TranslationHeader columns;
	check_translation_header(header, columns, result.ptr());
	if (columns.has_known_language) {
		const Array rows = _get_rows();
		HashSet<String> keys;
		for (int row = 0; row < rows.size() && !result->is_full(); row++) {
			const PackedStringArray cells = rows[row];
			const auto cell = [&cells](int p_column) {
				return p_column >= 0 && p_column < cells.size() ? cells[p_column] : String();
			};
			// Rows without a key are empty or further plural forms (a locale may have fewer forms).
			const String key = cell(0);
			if (key.is_empty() || key.to_lower() == "?pluralrule") {
				continue;
			}
			// The importer adds messages by key and context: a second row replaces the first.
			const String context = cell(columns.context_column);
			const String id = context.is_empty() ? key : context + String::chr(4) + key;
			if (keys.has(id)) {
				result->add_issue(DataValidation::ISSUE_KIND_DUPLICATE_KEY, vformat("Key '%s' is used twice%s.", key, context.is_empty() ? String() : vformat(" in context '%s'", context)), header[0], String(), row, 0);
			} else {
				keys.insert(id);
			}
			for (const KeyValue<int, String> &E : columns.locale_columns) {
				if (cell(E.key).is_empty()) {
					result->add_issue(DataValidation::ISSUE_KIND_EMPTY_REQUIRED, vformat("Missing translation of '%s' for locale '%s'.", key, E.value), header[E.key], String(), row, E.key);
				}
			}
		}
	}

	if (p_options.is_valid() && p_options->is_keep_positions() && !get_parsed_text().is_empty()) {
		_resolve_issue_lines(result, get_parsed_text());
	}
	return result;
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
	ClassDB::bind_method(D_METHOD("get_cells", "row", "column", "row_count", "column_count"), &CSV::get_cells);
	ClassDB::bind_method(D_METHOD("set_cells", "row", "column", "cells"), &CSV::set_cells);
	ClassDB::bind_method(D_METHOD("insert_column", "column", "name", "value"), &CSV::insert_column, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("remove_column", "column"), &CSV::remove_column);
	ClassDB::bind_method(D_METHOD("find_column", "name"), &CSV::find_column);
	ClassDB::bind_method(D_METHOD("get_column", "column"), &CSV::get_column);
	ClassDB::bind_method(D_METHOD("find_rows", "column", "value"), &CSV::find_rows);
	ClassDB::bind_method(D_METHOD("get_typed_row", "row", "schema"), &CSV::get_typed_row);
	ClassDB::bind_method(D_METHOD("append_typed_row", "values", "schema"), &CSV::append_typed_row);
	ClassDB::bind_method(D_METHOD("get_typed_column", "column", "schema"), &CSV::get_typed_column);
	ClassDB::bind_method(D_METHOD("validate_translation", "options"), &CSV::validate_translation, DEFVAL(Ref<DataValidationOptions>()));

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "header"), "set_header", "get_header");

	ClassDB::bind_static_method("CSV", D_METHOD("parse_string", "text", "options"), &CSV::parse_string, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("stringify", "data", "options"), &CSV::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("serialize", "resource", "options"), &CSV::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("CSV", D_METHOD("deserialize", "text", "type_hint", "options"), &CSV::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
