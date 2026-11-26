/**************************************************************************/
/*  data_validation.cpp                                                   */
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

#include "data_validation.h"

#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

void DataValidationOptions::set_max_issues(int p_max_issues) {
	max_issues = p_max_issues;
	emit_changed();
}

void DataValidationOptions::set_stop_on_first(bool p_stop_on_first) {
	stop_on_first = p_stop_on_first;
	emit_changed();
}

void DataValidationOptions::set_keep_positions(bool p_keep_positions) {
	keep_positions = p_keep_positions;
	emit_changed();
}

void DataValidationOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_issues", "max_issues"), &DataValidationOptions::set_max_issues);
	ClassDB::bind_method(D_METHOD("get_max_issues"), &DataValidationOptions::get_max_issues);
	ClassDB::bind_method(D_METHOD("set_stop_on_first", "stop_on_first"), &DataValidationOptions::set_stop_on_first);
	ClassDB::bind_method(D_METHOD("is_stop_on_first"), &DataValidationOptions::is_stop_on_first);
	ClassDB::bind_method(D_METHOD("set_keep_positions", "keep_positions"), &DataValidationOptions::set_keep_positions);
	ClassDB::bind_method(D_METHOD("is_keep_positions"), &DataValidationOptions::is_keep_positions);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_issues", PROPERTY_HINT_RANGE, "-1,100000,1,or_greater"), "set_max_issues", "get_max_issues");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stop_on_first"), "set_stop_on_first", "is_stop_on_first");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_positions"), "set_keep_positions", "is_keep_positions");
}

void DataValidation::apply_options(const Ref<DataValidationOptions> &p_options) {
	if (p_options.is_null()) {
		return;
	}
	max_issues = p_options->is_stop_on_first() ? 1 : p_options->get_max_issues();
}

void DataValidation::add_issue(IssueKind p_kind, const String &p_message, const StringName &p_field, const String &p_path, int p_row, int p_column, int p_line) {
	ERR_FAIL_INDEX(p_kind, ISSUE_KIND_MAX);
	if (is_full()) {
		return;
	}
	Issue issue;
	issue.kind = p_kind;
	issue.message = p_message;
	issue.field = p_field;
	issue.path = p_path;
	issue.row = p_row;
	issue.column = p_column;
	issue.line = p_line;
	issues.push_back(issue);
}

void DataValidation::set_issue_line(int p_index, int p_line) {
	ERR_FAIL_INDEX(p_index, (int)issues.size());
	issues[p_index].line = p_line;
}

static bool _data_validation_is_word_char(char32_t p_char) {
	return is_ascii_alphanumeric_char(p_char) || p_char == '_' || p_char == '-';
}

// Next occurrence of `p_key` at or after `p_from` that is not part of a longer word.
static int _data_validation_find_key(const String &p_text, const String &p_key, int p_from) {
	const int length = p_text.length();
	int at = p_text.find(p_key, p_from);
	while (at >= 0) {
		const int end = at + p_key.length();
		if ((at == 0 || !_data_validation_is_word_char(p_text[at - 1])) && (end >= length || !_data_validation_is_word_char(p_text[end]))) {
			return at;
		}
		at = p_text.find(p_key, at + 1);
	}
	return -1;
}

void DataValidation::resolve_lines(const String &p_text) {
	// Searches and line starts are shared between issues, so many issues cost about one pass over the text:
	// the n-th occurrence of a key continues from the last search of the same key at the same position.
	HashMap<String, Vector2i> searches; // "<from>/<key>" -> (occurrence index, position) of the last search.
	LocalVector<int> line_starts;
	for (Issue &issue : issues) {
		if (issue.line != POSITION_NONE || issue.path.is_empty()) {
			continue;
		}
		// Keys are searched in order; an array index n picks the (n + 1)-th occurrence of the next key.
		int position = 0;
		int found = -1;
		int skip = 0;
		for (const String &segment : issue.path.split("/")) {
			if (segment.is_valid_int()) {
				skip = segment.to_int();
				continue;
			}
			const String key = segment.replace("~1", "/").replace("~0", "~");
			const String search_key = itos(position) + "/" + key;
			const Vector2i *last = searches.getptr(search_key);
			int index = 0;
			int at;
			if (last && last->x <= skip) {
				index = last->x;
				at = last->y;
			} else {
				at = _data_validation_find_key(p_text, key, position);
			}
			for (; at >= 0 && index < skip; index++) {
				at = _data_validation_find_key(p_text, key, at + key.length());
			}
			searches[search_key] = Vector2i(index, at);
			skip = 0;
			if (at < 0) {
				break;
			}
			found = at;
			position = at + key.length();
		}
		if (found < 0) {
			continue;
		}
		if (line_starts.is_empty()) {
			line_starts.push_back(0);
			const char32_t *text = p_text.ptr();
			for (int i = 0; i < p_text.length(); i++) {
				if (text[i] == '\n') {
					line_starts.push_back(i + 1);
				}
			}
		}
		// Number of lines that start at or before the match.
		int low = 0;
		int high = line_starts.size();
		while (low < high) {
			const int middle = (low + high) / 2;
			if (line_starts[middle] <= found) {
				low = middle + 1;
			} else {
				high = middle;
			}
		}
		issue.line = low;
	}
}

DataValidation::IssueKind DataValidation::get_issue_kind(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), ISSUE_KIND_MAX);
	return issues[p_index].kind;
}

String DataValidation::get_issue_message(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), String());
	return issues[p_index].message;
}

StringName DataValidation::get_issue_field(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), StringName());
	return issues[p_index].field;
}

String DataValidation::get_issue_path(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), String());
	return issues[p_index].path;
}

int DataValidation::get_issue_row(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), POSITION_NONE);
	return issues[p_index].row;
}

int DataValidation::get_issue_column(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), POSITION_NONE);
	return issues[p_index].column;
}

int DataValidation::get_issue_line(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), POSITION_NONE);
	return issues[p_index].line;
}

PackedInt32Array DataValidation::get_invalid_rows() const {
	PackedInt32Array rows;
	HashSet<int> seen;
	for (const Issue &issue : issues) {
		if (issue.row != POSITION_NONE && !seen.has(issue.row)) {
			seen.insert(issue.row);
			rows.push_back(issue.row);
		}
	}
	rows.sort();
	return rows;
}

String DataValidation::join_path(const String &p_path, const String &p_key) {
	const String key = p_key.replace("~", "~0").replace("/", "~1");
	return p_path.is_empty() ? key : p_path + "/" + key;
}

void DataValidation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_issue", "kind", "message", "field", "path", "row", "column", "line"), &DataValidation::add_issue, DEFVAL(StringName()), DEFVAL(String()), DEFVAL(POSITION_NONE), DEFVAL(POSITION_NONE), DEFVAL(POSITION_NONE));
	ClassDB::bind_method(D_METHOD("resolve_lines", "text"), &DataValidation::resolve_lines);
	ClassDB::bind_method(D_METHOD("is_valid"), &DataValidation::is_valid);
	ClassDB::bind_method(D_METHOD("get_issue_count"), &DataValidation::get_issue_count);
	ClassDB::bind_method(D_METHOD("get_issue_kind", "index"), &DataValidation::get_issue_kind);
	ClassDB::bind_method(D_METHOD("get_issue_message", "index"), &DataValidation::get_issue_message);
	ClassDB::bind_method(D_METHOD("get_issue_field", "index"), &DataValidation::get_issue_field);
	ClassDB::bind_method(D_METHOD("get_issue_path", "index"), &DataValidation::get_issue_path);
	ClassDB::bind_method(D_METHOD("get_issue_row", "index"), &DataValidation::get_issue_row);
	ClassDB::bind_method(D_METHOD("get_issue_column", "index"), &DataValidation::get_issue_column);
	ClassDB::bind_method(D_METHOD("get_issue_line", "index"), &DataValidation::get_issue_line);
	ClassDB::bind_method(D_METHOD("get_invalid_rows"), &DataValidation::get_invalid_rows);

	BIND_ENUM_CONSTANT(ISSUE_KIND_MISSING_FIELD);
	BIND_ENUM_CONSTANT(ISSUE_KIND_UNKNOWN_FIELD);
	BIND_ENUM_CONSTANT(ISSUE_KIND_TYPE_MISMATCH);
	BIND_ENUM_CONSTANT(ISSUE_KIND_OUT_OF_RANGE);
	BIND_ENUM_CONSTANT(ISSUE_KIND_NOT_IN_ENUM);
	BIND_ENUM_CONSTANT(ISSUE_KIND_PATTERN_MISMATCH);
	BIND_ENUM_CONSTANT(ISSUE_KIND_EMPTY_REQUIRED);
	BIND_ENUM_CONSTANT(ISSUE_KIND_DUPLICATE_KEY);
	BIND_ENUM_CONSTANT(ISSUE_KIND_PARSE_ERROR);
	BIND_ENUM_CONSTANT(ISSUE_KIND_INVALID_LOCALE);
	BIND_ENUM_CONSTANT(ISSUE_KIND_MAX);
	BIND_CONSTANT(POSITION_NONE);
}
