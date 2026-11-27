/**************************************************************************/
/*  yaml.cpp                                                              */
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

#include "yaml.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/string/char_utils.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

static constexpr const char *YAML_KEY_TYPE = "$type";
// Nesting limit for reading and writing; keeps the recursion within the small stack of the Web export.
static constexpr int YAML_MAX_DEPTH = 256;

namespace {

bool _is_blank(char32_t p_c) {
	return p_c == ' ' || p_c == '\t';
}

// Space, tab, line break or the end of the text (0).
bool _is_ws_or_end(char32_t p_c) {
	return p_c == 0 || p_c == ' ' || p_c == '\t' || p_c == '\n';
}

bool _is_flow_indicator(char32_t p_c) {
	return p_c == ',' || p_c == '[' || p_c == ']' || p_c == '{' || p_c == '}';
}

bool _parse_digits(const String &p_text, int p_from, int p_base, uint64_t &r_value) {
	if (p_from >= p_text.length()) {
		return false;
	}
	uint64_t value = 0;
	for (int i = p_from; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		int digit = -1;
		if (is_digit(c)) {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		}
		if (digit < 0 || digit >= p_base) {
			return false;
		}
		if (value > (UINT64_MAX - uint64_t(digit)) / uint64_t(p_base)) {
			value = UINT64_MAX; // Too large; reported by the caller.
			for (int j = i + 1; j < p_text.length(); j++) {
				const char32_t d = p_text[j];
				if (!(p_base == 16 ? is_hex_digit(d) : (d >= '0' && d < char32_t('0' + p_base)))) {
					return false;
				}
			}
			r_value = value;
			return true;
		}
		value = value * uint64_t(p_base) + uint64_t(digit);
	}
	r_value = value;
	return true;
}

// [-+]? ( \. [0-9]+ | [0-9]+ ( \. [0-9]* )? ) ( [eE] [-+]? [0-9]+ )?
bool _is_core_float(const String &p_text) {
	int i = 0;
	const int length = p_text.length();
	if (i < length && (p_text[i] == '-' || p_text[i] == '+')) {
		i++;
	}
	int digits = 0;
	while (i < length && is_digit(p_text[i])) {
		i++;
		digits++;
	}
	if (i < length && p_text[i] == '.') {
		i++;
		while (i < length && is_digit(p_text[i])) {
			i++;
			digits++;
		}
	}
	if (digits == 0) {
		return false;
	}
	if (i < length && (p_text[i] == 'e' || p_text[i] == 'E')) {
		i++;
		if (i < length && (p_text[i] == '-' || p_text[i] == '+')) {
			i++;
		}
		int exponent_digits = 0;
		while (i < length && is_digit(p_text[i])) {
			i++;
			exponent_digits++;
		}
		if (exponent_digits == 0) {
			return false;
		}
	}
	return i == length;
}

// Types a plain scalar by the YAML 1.2 core schema. Returns false (with r_error) for integers outside 64 bits.
bool _resolve_plain(const String &p_text, Variant &r_value, String &r_error) {
	if (p_text.is_empty() || p_text == "~" || p_text == "null" || p_text == "Null" || p_text == "NULL") {
		r_value = Variant();
		return true;
	}
	if (p_text == "true" || p_text == "True" || p_text == "TRUE") {
		r_value = true;
		return true;
	}
	if (p_text == "false" || p_text == "False" || p_text == "FALSE") {
		r_value = false;
		return true;
	}
	const char32_t first = p_text[0];
	if (first == '.' || first == '-' || first == '+') {
		const String rest = first == '.' ? p_text : p_text.substr(1);
		if (rest == ".inf" || rest == ".Inf" || rest == ".INF") {
			r_value = first == '-' ? -Math::INF : Math::INF;
			return true;
		}
		if (first == '.' && (p_text == ".nan" || p_text == ".NaN" || p_text == ".NAN")) {
			r_value = Math::NaN;
			return true;
		}
	}
	if (first == '0' && p_text.length() > 2 && (p_text[1] == 'x' || p_text[1] == 'o')) {
		uint64_t value = 0;
		if (_parse_digits(p_text, 2, p_text[1] == 'x' ? 16 : 8, value)) {
			if (value > uint64_t(INT64_MAX)) {
				r_error = vformat("The integer '%s' is outside the 64-bit range.", p_text);
				return false;
			}
			r_value = int64_t(value);
			return true;
		}
	}
	{
		const bool negative = first == '-';
		const int from = (first == '-' || first == '+') ? 1 : 0;
		uint64_t value = 0;
		if (_parse_digits(p_text, from, 10, value)) {
			if (negative ? value > uint64_t(INT64_MAX) + 1 : value > uint64_t(INT64_MAX)) {
				r_error = vformat("The integer '%s' is outside the 64-bit range.", p_text);
				return false;
			}
			if (negative) {
				r_value = value == uint64_t(INT64_MAX) + 1 ? INT64_MIN : -int64_t(value);
			} else {
				r_value = int64_t(value);
			}
			return true;
		}
	}
	if (_is_core_float(p_text)) {
		r_value = (first == '+' ? p_text.substr(1) : p_text).to_float();
		return true;
	}
	r_value = p_text;
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Reader.

struct YAMLReader {
	String text; // Line breaks normalized to "\n".
	const char32_t *src = nullptr;
	int len = 0;
	int pos = 0;
	int line = 1;
	int line_start = 0;

	String error;
	int error_line = 0;
	int error_column = 0;

	Array documents; // At least one; an empty text is one null document.

	struct State {
		int pos = 0;
		int line = 1;
		int line_start = 0;
	};

	State save() const {
		State state;
		state.pos = pos;
		state.line = line;
		state.line_start = line_start;
		return state;
	}

	void restore(const State &p_state) {
		pos = p_state.pos;
		line = p_state.line;
		line_start = p_state.line_start;
	}

	char32_t peek(int p_offset = 0) const {
		const int at = pos + p_offset;
		return (at >= 0 && at < len) ? src[at] : 0;
	}

	bool at_end() const { return pos >= len; }

	int column() const { return pos - line_start; }

	void advance() {
		if (pos < len) {
			if (src[pos] == '\n') {
				line++;
				line_start = pos + 1;
			}
			pos++;
		}
	}

	bool fail(const String &p_message) {
		return fail_at(line, column() + 1, p_message);
	}

	bool fail_at(int p_line, int p_column, const String &p_message) {
		if (error.is_empty()) {
			error = p_message;
			error_line = p_line;
			error_column = p_column;
		}
		return false;
	}

	void skip_blanks() {
		while (_is_blank(peek())) {
			advance();
		}
	}

	void skip_comment() {
		if (peek() != '#') {
			return;
		}
		while (!at_end() && peek() != '\n') {
			advance();
		}
	}

	// "---" or "..." at the start of a line, followed by a space or the end of the line.
	bool is_document_marker() const {
		if (column() != 0) {
			return false;
		}
		const char32_t c = peek();
		return (c == '-' || c == '.') && peek(1) == c && peek(2) == c && _is_ws_or_end(peek(3));
	}

	bool is_sequence_entry() const {
		return peek() == '-' && _is_ws_or_end(peek(1));
	}

	// Moves to the first character of the next line with content (skipping empty and comment lines).
	// r_indent is its indentation, or -1 at the end of the text and at a document marker.
	bool next_content_line(int &r_indent) {
		if (pos != line_start && !at_end()) {
			r_indent = column(); // Already at the content of a line.
			return true;
		}
		while (true) {
			if (at_end()) {
				r_indent = -1;
				return true;
			}
			int spaces = 0;
			while (peek(spaces) == ' ') {
				spaces++;
			}
			int blanks = spaces;
			while (_is_blank(peek(blanks))) {
				blanks++;
			}
			const char32_t c = peek(blanks);
			if (c == '\n' || c == 0 || c == '#') {
				while (!at_end() && peek() != '\n') {
					advance();
				}
				advance();
				continue;
			}
			for (int i = 0; i < spaces; i++) {
				advance();
			}
			if (blanks != spaces) {
				return fail("Tabs can't be used for indentation.");
			}
			r_indent = is_document_marker() ? -1 : spaces;
			return true;
		}
	}

	// After a value: blanks, a comment and the line break.
	bool finish_line() {
		skip_blanks();
		if (peek() == '#') {
			if (pos > line_start && !_is_blank(peek(-1))) {
				return fail("A comment needs a space before '#'.");
			}
			skip_comment();
		}
		if (at_end()) {
			return true;
		}
		if (peek() == '\n') {
			advance();
			return true;
		}
		if (peek() == ':') {
			return fail("A mapping value is not allowed here; check the indentation, or quote the value if it contains ': '.");
		}
		return fail(vformat("Unexpected '%s' after the value.", String::chr(peek())));
	}

	bool parse(const String &p_text);
	bool parse_block_node(int p_parent_indent, bool p_allow_sequence_at_parent, Variant &r_value, int p_depth);
	bool parse_node_at(int p_indent, int p_parent_indent, Variant &r_value, int p_depth);
	bool check_mapping_key(bool &r_is_key);
	bool parse_key(String &r_key, bool &r_plain);
	bool parse_block_mapping(int p_indent, Dictionary &r_map, int p_depth);
	bool parse_block_sequence(int p_indent, Array &r_array, int p_depth);
	bool parse_inline_value(int p_parent_indent, Variant &r_value, int p_depth);
	bool read_plain_line(String &r_text);
	bool parse_plain_block(int p_parent_indent, String &r_text);
	bool parse_quoted(String &r_text);
	bool parse_block_scalar(int p_parent_indent, String &r_text);
	bool read_tag(String &r_tag);
	bool apply_tag(const String &p_tag, const String &p_scalar, bool p_plain, int p_line, int p_column, Variant &r_value);
	bool skip_flow_space(int p_parent_indent, int p_open_line, int p_open_column, bool p_map);
	bool parse_flow_collection(int p_parent_indent, Variant &r_value, int p_depth);
	bool parse_flow_node(int p_parent_indent, Variant &r_value, int p_depth);
	bool read_plain_flow(String &r_text);
};

bool YAMLReader::parse(const String &p_text) {
	text = p_text.replace("\r\n", "\n").replace("\r", "\n");
	if (!text.is_empty() && text[0] == 0xFEFF) {
		text = text.substr(1);
	}
	src = text.ptr();
	len = text.length();

	int indent = 0;
	// Directives.
	while (true) {
		if (!next_content_line(indent)) {
			return false;
		}
		if (indent != 0 || peek() != '%') {
			break;
		}
		if (text.substr(pos, 4) == "%TAG") {
			return fail("The %TAG directive is not supported, because tags other than !!str, !!int, !!float, !!bool and !!null are not supported.");
		}
		// %YAML and reserved directives are skipped.
		while (!at_end() && peek() != '\n') {
			advance();
		}
		advance();
	}
	// Documents, each optionally started by "---" and ended by "...".
	documents.clear();
	while (true) {
		bool started = false;
		if (indent == -1 && !at_end() && peek() == '-') {
			// Document start "---".
			for (int i = 0; i < 3; i++) {
				advance();
			}
			skip_blanks();
			skip_comment();
			if (!at_end() && peek() != '\n') {
				return fail("Content on the line of '---' is not supported; start the document on the next line.");
			}
			advance();
			if (!next_content_line(indent)) {
				return false;
			}
			started = true;
		}
		Variant document;
		if (indent >= 0) {
			if (!parse_block_node(-1, false, document, 0)) {
				return false;
			}
			if (!next_content_line(indent)) {
				return false;
			}
			if (indent >= 0) {
				return fail("Unexpected content after the document; check the indentation.");
			}
			started = true;
		}
		if (started || documents.is_empty()) {
			documents.push_back(document);
		}
		if (at_end()) {
			return true;
		}
		if (peek() == '.') {
			// Document end "...".
			for (int i = 0; i < 3; i++) {
				advance();
			}
			if (!finish_line()) {
				return false;
			}
			if (!next_content_line(indent)) {
				return false;
			}
			if (at_end()) {
				return true;
			}
		}
	}
}

bool YAMLReader::parse_block_node(int p_parent_indent, bool p_allow_sequence_at_parent, Variant &r_value, int p_depth) {
	int indent = 0;
	if (!next_content_line(indent)) {
		return false;
	}
	if (indent > p_parent_indent) {
		return parse_node_at(indent, p_parent_indent, r_value, p_depth);
	}
	if (p_allow_sequence_at_parent && indent >= 0 && indent == p_parent_indent && is_sequence_entry()) {
		// "key:" followed by "- item" lines at the indentation of the key.
		Array array;
		if (!parse_block_sequence(indent, array, p_depth + 1)) {
			return false;
		}
		r_value = array;
		return true;
	}
	r_value = Variant(); // Empty node.
	return true;
}

bool YAMLReader::parse_node_at(int p_indent, int p_parent_indent, Variant &r_value, int p_depth) {
	if (p_depth > YAML_MAX_DEPTH) {
		return fail(vformat("The document is nested deeper than %d levels.", YAML_MAX_DEPTH));
	}
	if (is_sequence_entry()) {
		Array array;
		if (!parse_block_sequence(p_indent, array, p_depth + 1)) {
			return false;
		}
		r_value = array;
		return true;
	}
	bool is_key = false;
	if (!check_mapping_key(is_key)) {
		return false;
	}
	if (is_key) {
		Dictionary map;
		if (!parse_block_mapping(p_indent, map, p_depth + 1)) {
			return false;
		}
		r_value = map;
		return true;
	}
	return parse_inline_value(p_parent_indent, r_value, p_depth);
}

bool YAMLReader::check_mapping_key(bool &r_is_key) {
	r_is_key = false;
	const char32_t c = peek();
	if (c == '?' && _is_ws_or_end(peek(1))) {
		return fail("Complex mapping keys ('?') are not supported.");
	}
	if (c == '"' || c == '\'') {
		int i = 1;
		while (peek(i) != 0 && peek(i) != '\n') {
			if (peek(i) == c) {
				if (c == '\'' && peek(i + 1) == '\'') {
					i += 2;
					continue;
				}
				break;
			}
			i += (c == '"' && peek(i) == '\\' && peek(i + 1) != '\n') ? 2 : 1;
		}
		if (peek(i) != c) {
			return true; // A quoted scalar over several lines: not a key.
		}
		i++;
		while (_is_blank(peek(i))) {
			i++;
		}
		r_is_key = peek(i) == ':' && _is_ws_or_end(peek(i + 1));
		return true;
	}
	if (c == '[' || c == '{' || c == '&' || c == '*' || c == '!' || c == '|' || c == '>' || c == '#') {
		return true; // Values; flow collections as keys are not supported.
	}
	for (int i = 0; peek(i) != 0 && peek(i) != '\n'; i++) {
		if (peek(i) == ':' && _is_ws_or_end(peek(i + 1))) {
			r_is_key = true;
			return true;
		}
		if (peek(i) == '#' && i > 0 && _is_blank(peek(i - 1))) {
			break;
		}
	}
	return true;
}

bool YAMLReader::parse_key(String &r_key, bool &r_plain) {
	const char32_t c = peek();
	r_plain = c != '"' && c != '\'';
	if (r_plain) {
		const int from = pos;
		int last = pos;
		while (!at_end() && peek() != '\n' && !(peek() == ':' && _is_ws_or_end(peek(1)))) {
			if (!_is_blank(peek())) {
				last = pos + 1;
			}
			advance();
		}
		r_key = text.substr(from, last - from);
	} else if (!parse_quoted(r_key)) {
		return false;
	}
	skip_blanks();
	if (peek() != ':') {
		return fail("Expected ':' after the mapping key.");
	}
	advance();
	return true;
}

bool YAMLReader::parse_block_mapping(int p_indent, Dictionary &r_map, int p_depth) {
	while (true) {
		const int key_line = line;
		const int key_column = column() + 1;
		String key;
		bool plain = false;
		if (!parse_key(key, plain)) {
			return false;
		}
		if (plain && key == "<<") {
			return fail_at(key_line, key_column, "Merge keys ('<<') are not supported.");
		}
		if (r_map.has(key)) {
			return fail_at(key_line, key_column, vformat("Duplicate key \"%s\".", key));
		}
		skip_blanks();
		Variant value;
		if (at_end() || peek() == '\n' || peek() == '#') {
			skip_comment();
			advance();
			if (!parse_block_node(p_indent, true, value, p_depth)) {
				return false;
			}
		} else if (!parse_inline_value(p_indent, value, p_depth)) {
			return false;
		}
		r_map[key] = value;

		int indent = 0;
		if (!next_content_line(indent)) {
			return false;
		}
		if (indent < p_indent) {
			return true;
		}
		if (indent > p_indent) {
			return fail(vformat("Bad indentation: expected %d spaces (the mapping above) or less, found %d.", p_indent, indent));
		}
		if (is_sequence_entry()) {
			return fail("Expected a mapping key, found a sequence entry '-'; check the indentation.");
		}
		bool is_key = false;
		if (!check_mapping_key(is_key)) {
			return false;
		}
		if (!is_key) {
			return fail("Expected a mapping key ('key: value') at this indentation.");
		}
	}
}

bool YAMLReader::parse_block_sequence(int p_indent, Array &r_array, int p_depth) {
	while (true) {
		advance(); // '-'
		skip_blanks();
		Variant item;
		if (at_end() || peek() == '\n' || peek() == '#') {
			skip_comment();
			advance();
			if (!parse_block_node(p_indent, false, item, p_depth)) {
				return false;
			}
		} else if (!parse_node_at(column(), p_indent, item, p_depth)) {
			// Compact form: "- key: value", "- - item" or "- value".
			return false;
		}
		r_array.push_back(item);

		int indent = 0;
		if (!next_content_line(indent)) {
			return false;
		}
		if (indent < p_indent) {
			return true;
		}
		if (indent > p_indent) {
			return fail(vformat("Bad indentation: expected %d spaces (the sequence above) or less, found %d.", p_indent, indent));
		}
		if (!is_sequence_entry()) {
			return true; // A key of the mapping that contains this sequence.
		}
	}
}

bool YAMLReader::read_tag(String &r_tag) {
	const int from = pos;
	while (!_is_ws_or_end(peek()) && !_is_flow_indicator(peek())) {
		advance();
	}
	r_tag = text.substr(from, pos - from);
	if (r_tag != "!!str" && r_tag != "!!int" && r_tag != "!!float" && r_tag != "!!bool" && r_tag != "!!null") {
		return fail_at(line, from - line_start + 1, vformat("The tag '%s' is not supported; only !!str, !!int, !!float, !!bool and !!null are.", r_tag));
	}
	skip_blanks();
	return true;
}

bool YAMLReader::apply_tag(const String &p_tag, const String &p_scalar, bool p_plain, int p_line, int p_column, Variant &r_value) {
	if (p_tag.is_empty() && !p_plain) {
		r_value = p_scalar;
		return true;
	}
	if (p_tag == "!!str") {
		r_value = p_scalar;
		return true;
	}
	String resolve_error;
	if (!_resolve_plain(p_scalar, r_value, resolve_error)) {
		return fail_at(p_line, p_column, resolve_error);
	}
	if (p_tag.is_empty()) {
		return true;
	}
	bool matches = false;
	if (p_tag == "!!null") {
		matches = r_value.get_type() == Variant::NIL;
	} else if (p_tag == "!!bool") {
		matches = r_value.get_type() == Variant::BOOL;
	} else if (p_tag == "!!int") {
		matches = r_value.get_type() == Variant::INT;
	} else if (p_tag == "!!float") {
		if (r_value.get_type() == Variant::INT) {
			r_value = double(int64_t(r_value));
		}
		matches = r_value.get_type() == Variant::FLOAT;
	}
	if (!matches) {
		return fail_at(p_line, p_column, vformat("'%s' is not a valid %s value.", p_scalar, p_tag));
	}
	return true;
}

bool YAMLReader::parse_inline_value(int p_parent_indent, Variant &r_value, int p_depth) {
	if (p_depth > YAML_MAX_DEPTH) {
		return fail(vformat("The document is nested deeper than %d levels.", YAML_MAX_DEPTH));
	}
	String tag;
	if (peek() == '!') {
		if (!read_tag(tag)) {
			return false;
		}
	}
	const int value_line = line;
	const int value_column = column() + 1;
	const char32_t c = peek();
	if (c == '&') {
		return fail("Anchors ('&') are not supported.");
	}
	if (c == '*') {
		return fail("Aliases ('*') are not supported.");
	}
	if (c == '!') {
		return fail("A value can have only one tag.");
	}
	String scalar;
	bool plain = false;
	if (c == '|' || c == '>') {
		// The block scalar reads its lines up to the start of the next line with content.
		if (!parse_block_scalar(p_parent_indent, scalar)) {
			return false;
		}
		return apply_tag(tag, scalar, false, value_line, value_column, r_value);
	}
	if (c == '[' || c == '{') {
		if (!tag.is_empty()) {
			return fail_at(value_line, value_column, vformat("The tag '%s' can't be used on a collection.", tag));
		}
		if (!parse_flow_collection(p_parent_indent, r_value, p_depth + 1)) {
			return false;
		}
		return finish_line();
	}
	if (c == '"' || c == '\'') {
		if (!parse_quoted(scalar)) {
			return false;
		}
	} else if (c == ',' || c == ']' || c == '}' || c == '@' || c == '`') {
		return fail(vformat("A plain value can't start with '%s'; quote the value.", String::chr(c)));
	} else if (at_end() || c == '\n' || c == '#') {
		plain = true; // Empty value after a tag.
	} else {
		if (!parse_plain_block(p_parent_indent, scalar)) {
			return false;
		}
		plain = true;
	}
	if (!finish_line()) {
		return false;
	}
	return apply_tag(tag, scalar, plain, value_line, value_column, r_value);
}

bool YAMLReader::read_plain_line(String &r_text) {
	const int from = pos;
	int last = pos;
	while (!at_end() && peek() != '\n') {
		const char32_t c = peek();
		if (c == ':' && _is_ws_or_end(peek(1))) {
			return fail("A mapping value is not allowed here; check the indentation, or quote the value if it contains ': '.");
		}
		if (c == '#' && pos > from && _is_blank(peek(-1))) {
			break;
		}
		advance();
		if (!_is_blank(c)) {
			last = pos;
		}
	}
	r_text = text.substr(from, last - from);
	pos = last; // Trailing blanks and a comment are left for finish_line(); still on the same line.
	return true;
}

bool YAMLReader::parse_plain_block(int p_parent_indent, String &r_text) {
	if (!read_plain_line(r_text)) {
		return false;
	}
	while (true) {
		// Continuation lines: indented more than the parent node, folded with a space (empty lines give "\n").
		const State end_of_value = save();
		skip_blanks();
		if (peek() != '\n') {
			restore(end_of_value); // A comment or the end of the text.
			return true;
		}
		advance();
		int empty_lines = 0;
		bool continued = false;
		while (!at_end()) {
			int spaces = 0;
			while (peek(spaces) == ' ') {
				spaces++;
			}
			int blanks = spaces;
			while (_is_blank(peek(blanks))) {
				blanks++;
			}
			const char32_t c = peek(blanks);
			if (c == '\n') {
				empty_lines++;
				for (int i = 0; i <= blanks; i++) {
					advance();
				}
				continue;
			}
			if (c == 0 || c == '#' || spaces <= p_parent_indent || (spaces == 0 && (c == '-' || c == '.') && peek(1) == c && peek(2) == c && _is_ws_or_end(peek(3)))) {
				break;
			}
			for (int i = 0; i < blanks; i++) {
				advance();
			}
			bool is_key = false;
			if (!check_mapping_key(is_key)) {
				return false;
			}
			if (is_key) {
				return fail("Bad indentation: this mapping entry is indented more than the keys before it, so it would continue the value above.");
			}
			String part;
			if (!read_plain_line(part)) {
				return false;
			}
			r_text += empty_lines == 0 ? String(" ") : String("\n").repeat(empty_lines);
			r_text += part;
			continued = true;
			break;
		}
		if (!continued) {
			restore(end_of_value);
			return true;
		}
	}
}

bool YAMLReader::parse_quoted(String &r_text) {
	const char32_t quote = peek();
	const int open_line = line;
	const int open_column = column() + 1;
	advance();
	String result;
	int keep = 0; // Length of `result` without blanks that a line break folds away.
	while (true) {
		if (at_end()) {
			return fail_at(open_line, open_column, quote == '"' ? "The double-quoted string is not closed." : "The single-quoted string is not closed.");
		}
		const char32_t c = peek();
		if (c == quote) {
			advance();
			if (quote == '\'' && peek() == '\'') {
				advance();
				result += '\'';
				keep = result.length();
				continue;
			}
			break;
		}
		if (c == '\n') {
			// Line folding: a single line break becomes a space, each further empty line a "\n".
			result = result.substr(0, keep);
			advance();
			int empty_lines = 0;
			while (true) {
				skip_blanks();
				if (peek() != '\n') {
					break;
				}
				advance();
				empty_lines++;
			}
			if (pos == line_start && is_document_marker()) {
				return fail_at(open_line, open_column, "The quoted string is not closed before the document marker.");
			}
			result += empty_lines == 0 ? String(" ") : String("\n").repeat(empty_lines);
			keep = result.length();
			continue;
		}
		if (quote == '"' && c == '\\') {
			advance();
			const char32_t e = peek();
			if (e == '\n') {
				// Escaped line break: the lines are joined without a space.
				advance();
				skip_blanks();
				keep = result.length();
				continue;
			}
			int hex_digits = 0;
			char32_t value = 0;
			switch (e) {
				case 'a':
					value = 0x07;
					break;
				case 'b':
					value = 0x08;
					break;
				case 't':
				case '\t':
					value = 0x09;
					break;
				case 'n':
					value = 0x0A;
					break;
				case 'v':
					value = 0x0B;
					break;
				case 'f':
					value = 0x0C;
					break;
				case 'r':
					value = 0x0D;
					break;
				case 'e':
					value = 0x1B;
					break;
				case ' ':
				case '"':
				case '/':
				case '\\':
					value = e;
					break;
				case 'N':
					value = 0x85;
					break;
				case '_':
					value = 0xA0;
					break;
				case 'L':
					value = 0x2028;
					break;
				case 'P':
					value = 0x2029;
					break;
				case 'x':
					hex_digits = 2;
					break;
				case 'u':
					hex_digits = 4;
					break;
				case 'U':
					hex_digits = 8;
					break;
				case '0':
					return fail("The escape '\\0' is not supported (strings can't contain the null character).");
				default:
					return fail(vformat("Invalid escape '\\%s'.", String::chr(e)));
			}
			advance();
			for (int i = 0; i < hex_digits; i++) {
				const char32_t h = peek();
				if (!is_hex_digit(h)) {
					return fail(vformat("Expected %d hexadecimal digits after '\\%s'.", hex_digits, String::chr(e)));
				}
				value = value * 16 + (is_digit(h) ? h - '0' : (h >= 'a' ? h - 'a' + 10 : h - 'A' + 10));
				advance();
			}
			if (hex_digits > 0 && (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))) {
				return fail(vformat("The escape gives the invalid character U+%X.", int64_t(value)));
			}
			result += value;
			keep = result.length();
			continue;
		}
		result += c;
		advance();
		if (!_is_blank(c)) {
			keep = result.length();
		}
	}
	r_text = result;
	return true;
}

bool YAMLReader::parse_block_scalar(int p_parent_indent, String &r_text) {
	const bool folded = peek() == '>';
	advance();
	int chomping = 0; // -1 strip, 0 clip, 1 keep.
	int explicit_indent = 0;
	for (int i = 0; i < 2; i++) {
		const char32_t c = peek();
		if ((c == '-' || c == '+') && chomping == 0) {
			chomping = c == '-' ? -1 : 1;
			advance();
		} else if (c >= '1' && c <= '9' && explicit_indent == 0) {
			explicit_indent = c - '0';
			advance();
		} else {
			break;
		}
	}
	const int before_comment = pos;
	skip_blanks();
	if (peek() == '#' && pos == before_comment) {
		return fail("A comment needs a space before '#'.");
	}
	skip_comment();
	if (!at_end() && peek() != '\n') {
		return fail(vformat("Unexpected '%s' in the block scalar header.", String::chr(peek())));
	}
	advance();

	const int base_indent = MAX(p_parent_indent, 0);
	int content_indent = explicit_indent > 0 ? base_indent + explicit_indent : -1;
	int leading_spaces = 0;
	Vector<String> lines; // Empty strings are empty lines.
	while (!at_end()) {
		const State line_begin = save();
		int spaces = 0;
		while (peek(spaces) == ' ') {
			spaces++;
		}
		const char32_t after = peek(spaces);
		if (after == '\n' || after == 0) {
			if (content_indent < 0) {
				leading_spaces = MAX(leading_spaces, spaces);
				lines.push_back(String());
			} else if (spaces > content_indent) {
				lines.push_back(String(" ").repeat(spaces - content_indent));
			} else {
				lines.push_back(String());
			}
			for (int i = 0; i <= spaces; i++) {
				advance();
			}
			continue;
		}
		if (content_indent < 0) {
			if (spaces <= p_parent_indent) {
				break;
			}
			content_indent = spaces;
			if (leading_spaces > content_indent) {
				return fail("A leading empty line of the block scalar has more spaces than its first line.");
			}
		}
		if (spaces < content_indent || (spaces == 0 && is_document_marker())) {
			restore(line_begin);
			break;
		}
		for (int i = 0; i < content_indent; i++) {
			advance();
		}
		const int from = pos;
		while (!at_end() && peek() != '\n') {
			advance();
		}
		lines.push_back(text.substr(from, pos - from));
		advance();
	}

	int last = int(lines.size()) - 1;
	while (last >= 0 && lines[last].is_empty()) {
		last--;
	}
	const int trailing = int(lines.size()) - 1 - last;
	String result;
	if (!folded) {
		for (int i = 0; i <= last; i++) {
			if (i > 0) {
				result += "\n";
			}
			result += lines[i];
		}
	} else {
		bool has_previous = false;
		bool previous_normal = false;
		int empty_lines = 0;
		for (int i = 0; i <= last; i++) {
			const String &part = lines[i];
			if (part.is_empty()) {
				empty_lines++;
				continue;
			}
			const bool normal = !_is_blank(part[0]);
			if (!has_previous) {
				result += String("\n").repeat(empty_lines);
			} else if (previous_normal && normal) {
				result += empty_lines == 0 ? String(" ") : String("\n").repeat(empty_lines);
			} else {
				result += String("\n").repeat(empty_lines + 1);
			}
			result += part;
			has_previous = true;
			previous_normal = normal;
			empty_lines = 0;
		}
	}
	if (chomping == 1) {
		result += String("\n").repeat(last >= 0 ? trailing + 1 : trailing);
	} else if (chomping == 0 && last >= 0) {
		result += "\n";
	}
	r_text = result;
	return true;
}

bool YAMLReader::skip_flow_space(int p_parent_indent, int p_open_line, int p_open_column, bool p_map) {
	const char *kind = p_map ? "mapping" : "sequence";
	const char *close = p_map ? "}" : "]";
	while (true) {
		const char32_t c = peek();
		if (_is_blank(c)) {
			advance();
		} else if (c == '#') {
			skip_comment();
		} else if (c == '\n') {
			advance();
			int spaces = 0;
			while (peek(spaces) == ' ') {
				spaces++;
			}
			int blanks = spaces;
			while (_is_blank(peek(blanks))) {
				blanks++;
			}
			const char32_t next = peek(blanks);
			if (next != '\n' && next != 0 && next != '#') {
				const bool marker = spaces == 0 && (next == '-' || next == '.') && peek(1) == next && peek(2) == next && _is_ws_or_end(peek(3));
				if (spaces <= p_parent_indent || marker) {
					return fail_at(p_open_line, p_open_column, vformat("The flow %s is not closed: expected ',' or '%s' before line %d.", kind, close, line));
				}
			}
		} else if (at_end()) {
			return fail_at(p_open_line, p_open_column, vformat("The flow %s is not closed: expected ',' or '%s' before the end of the text.", kind, close));
		} else {
			return true;
		}
	}
}

bool YAMLReader::read_plain_flow(String &r_text) {
	const int from = pos;
	int last = pos;
	while (!at_end() && peek() != '\n') {
		const char32_t c = peek();
		if (_is_flow_indicator(c)) {
			break;
		}
		if (c == ':' && (_is_ws_or_end(peek(1)) || _is_flow_indicator(peek(1)))) {
			break;
		}
		if (c == '#' && pos > from && _is_blank(peek(-1))) {
			break;
		}
		advance();
		if (!_is_blank(c)) {
			last = pos;
		}
	}
	r_text = text.substr(from, last - from);
	return true;
}

bool YAMLReader::parse_flow_node(int p_parent_indent, Variant &r_value, int p_depth) {
	if (p_depth > YAML_MAX_DEPTH) {
		return fail(vformat("The document is nested deeper than %d levels.", YAML_MAX_DEPTH));
	}
	String tag;
	if (peek() == '!') {
		if (!read_tag(tag)) {
			return false;
		}
	}
	const int value_line = line;
	const int value_column = column() + 1;
	const char32_t c = peek();
	if (c == '&') {
		return fail("Anchors ('&') are not supported.");
	}
	if (c == '*') {
		return fail("Aliases ('*') are not supported.");
	}
	if (c == '[' || c == '{') {
		if (!tag.is_empty()) {
			return fail_at(value_line, value_column, vformat("The tag '%s' can't be used on a collection.", tag));
		}
		return parse_flow_collection(p_parent_indent, r_value, p_depth + 1);
	}
	String scalar;
	bool plain = false;
	if (c == '"' || c == '\'') {
		if (!parse_quoted(scalar)) {
			return false;
		}
	} else if (c == '|' || c == '>' || c == '@' || c == '`' || c == '#' || c == '!' || c == '%') {
		return fail(vformat("A plain value can't start with '%s'; quote the value.", String::chr(c)));
	} else {
		if (!read_plain_flow(scalar)) {
			return false;
		}
		if (scalar.is_empty() && tag.is_empty()) {
			return fail("Expected a value.");
		}
		plain = true;
	}
	return apply_tag(tag, scalar, plain, value_line, value_column, r_value);
}

bool YAMLReader::parse_flow_collection(int p_parent_indent, Variant &r_value, int p_depth) {
	if (p_depth > YAML_MAX_DEPTH) {
		return fail(vformat("The document is nested deeper than %d levels.", YAML_MAX_DEPTH));
	}
	const bool is_map = peek() == '{';
	const char32_t close = is_map ? '}' : ']';
	const int open_line = line;
	const int open_column = column() + 1;
	advance();
	Dictionary map;
	Array array;
	while (true) {
		if (!skip_flow_space(p_parent_indent, open_line, open_column, is_map)) {
			return false;
		}
		if (peek() == close) {
			advance();
			break;
		}
		if (peek() == ',') {
			return fail("Expected a value before ','.");
		}
		if (is_map) {
			const int key_line = line;
			const int key_column = column() + 1;
			const char32_t c = peek();
			String key;
			if (c == '"' || c == '\'') {
				if (!parse_quoted(key)) {
					return false;
				}
			} else if (c == '[' || c == '{') {
				return fail("Flow collections can't be mapping keys.");
			} else if (c == '?' && _is_ws_or_end(peek(1))) {
				return fail("Complex mapping keys ('?') are not supported.");
			} else if (c == '&' || c == '*' || c == '!') {
				return fail("Anchors, aliases and tags are not supported on mapping keys.");
			} else {
				if (!read_plain_flow(key)) {
					return false;
				}
				if (key == "<<") {
					return fail_at(key_line, key_column, "Merge keys ('<<') are not supported.");
				}
			}
			if (map.has(key)) {
				return fail_at(key_line, key_column, vformat("Duplicate key \"%s\".", key));
			}
			if (!skip_flow_space(p_parent_indent, open_line, open_column, is_map)) {
				return false;
			}
			Variant value;
			if (peek() == ':') {
				advance();
				if (!skip_flow_space(p_parent_indent, open_line, open_column, is_map)) {
					return false;
				}
				if (peek() != ',' && peek() != close) {
					if (!parse_flow_node(p_parent_indent, value, p_depth)) {
						return false;
					}
				}
			}
			map[key] = value;
		} else {
			Variant item;
			if (!parse_flow_node(p_parent_indent, item, p_depth)) {
				return false;
			}
			if (!skip_flow_space(p_parent_indent, open_line, open_column, is_map)) {
				return false;
			}
			if (peek() == ':') {
				return fail("Mappings inside flow sequences need braces: write '[{a: 1}]' instead of '[a: 1]'.");
			}
			array.push_back(item);
		}
		if (!skip_flow_space(p_parent_indent, open_line, open_column, is_map)) {
			return false;
		}
		if (peek() == ',') {
			advance();
			continue;
		}
		if (peek() == close) {
			advance();
			break;
		}
		return fail(vformat("Expected ',' or '%s' in the flow %s that starts on line %d, found '%s'.", String::chr(close), is_map ? "mapping" : "sequence", open_line, String::chr(peek())));
	}
	if (is_map) {
		r_value = map;
	} else {
		r_value = array;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Writer.

struct YAMLWriter {
	String error;
	HashSet<const void *> stack;
	int indent_width = 2;
	bool flow = false;
	bool sort_keys = false;

	struct Entry {
		String key;
		Variant value;
		bool operator<(const Entry &p_other) const { return key < p_other.key; }
	};

	bool enter(const void *p_id, const String &p_where) {
		if (stack.has(p_id)) {
			error = vformat("'%s' contains itself; cycles can't be written as YAML.", p_where);
			return false;
		}
		if (stack.size() >= uint32_t(YAML_MAX_DEPTH)) {
			error = vformat("'%s' is nested deeper than %d levels.", p_where, YAML_MAX_DEPTH);
			return false;
		}
		stack.insert(p_id);
		return true;
	}

	// Collections written as blocks over several lines. Empty collections and arrays of only numbers, booleans and
	// nulls (vectors, colors, packed number arrays) stay on one line in flow style: `[1.5, 2.0]`.
	static bool is_container(const Variant &p_value) {
		if (p_value.get_type() == Variant::DICTIONARY) {
			return !Dictionary(p_value).is_empty();
		}
		if (p_value.is_array()) {
			const Array array = p_value;
			for (int i = 0; i < array.size(); i++) {
				const Variant::Type type = array[i].get_type();
				if (type != Variant::NIL && type != Variant::BOOL && type != Variant::INT && type != Variant::FLOAT) {
					return true;
				}
			}
		}
		return false;
	}

	static bool is_string(const Variant &p_value) {
		return p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME || p_value.get_type() == Variant::NODE_PATH;
	}

	// Whether the text can be written without quotes and reads back as the same string.
	static bool is_plain_safe(const String &p_text, bool p_flow) {
		if (p_text.is_empty()) {
			return false;
		}
		Variant resolved;
		String resolve_error;
		if (!_resolve_plain(p_text, resolved, resolve_error) || resolved.get_type() != Variant::STRING) {
			return false;
		}
		const char32_t first = p_text[0];
		if (String("-?:,[]{}#&*!|>'\"%@`").contains_char(first) || _is_blank(first) || _is_blank(p_text[p_text.length() - 1])) {
			return false;
		}
		if (p_text == "<<" || p_text.begins_with("...")) {
			return false;
		}
		for (int i = 0; i < p_text.length(); i++) {
			const char32_t c = p_text[i];
			if (c < 0x20 || c == 0x7F || c == 0x85 || c == 0xFEFF || c == 0x2028 || c == 0x2029) {
				return false;
			}
			const char32_t next = i + 1 < p_text.length() ? p_text[i + 1] : 0;
			if (c == ':' && (_is_ws_or_end(next) || (p_flow && _is_flow_indicator(next)))) {
				return false;
			}
			if (c == '#' && i > 0 && _is_blank(p_text[i - 1])) {
				return false;
			}
			if (p_flow && _is_flow_indicator(c)) {
				return false;
			}
		}
		return true;
	}

	static String quote(const String &p_text) {
		String result = "\"";
		for (int i = 0; i < p_text.length(); i++) {
			const char32_t c = p_text[i];
			switch (c) {
				case '"':
					result += "\\\"";
					break;
				case '\\':
					result += "\\\\";
					break;
				case '\n':
					result += "\\n";
					break;
				case '\t':
					result += "\\t";
					break;
				case '\r':
					result += "\\r";
					break;
				case 0x08:
					result += "\\b";
					break;
				case 0x0C:
					result += "\\f";
					break;
				case 0x1B:
					result += "\\e";
					break;
				case 0x85:
					result += "\\N";
					break;
				case 0x2028:
					result += "\\L";
					break;
				case 0x2029:
					result += "\\P";
					break;
				case 0xFEFF:
					result += "\\uFEFF";
					break;
				default:
					if (c < 0x20 || c == 0x7F) {
						result += "\\x" + String::num_int64(c, 16, true).lpad(2, "0");
					} else {
						result += c;
					}
					break;
			}
		}
		return result + "\"";
	}

	static String text_scalar(const String &p_text, bool p_flow) {
		return is_plain_safe(p_text, p_flow) ? p_text : quote(p_text);
	}

	static String number(double p_value) {
		if (Math::is_nan(p_value)) {
			return ".nan";
		}
		if (Math::is_inf(p_value)) {
			return p_value > 0 ? ".inf" : "-.inf";
		}
		const String result = String::num_scientific(p_value);
		if (result.contains_char('.') || result.contains_char('e') || result.contains_char('E')) {
			return result;
		}
		return result + ".0";
	}

	// Strings with line breaks that a literal block scalar ("|") keeps as they are.
	static bool is_literal_candidate(const String &p_text) {
		if (!p_text.contains_char('\n')) {
			return false;
		}
		for (int i = 0; i < p_text.length(); i++) {
			const char32_t c = p_text[i];
			if ((c < 0x20 && c != '\n' && c != '\t') || c == 0x7F || c == 0x85 || c == 0xFEFF || c == 0x2028 || c == 0x2029) {
				return false;
			}
		}
		// The first line gives the indentation of the block, so it must start with text.
		return !p_text.is_empty() && p_text[0] != '\n' && !_is_blank(p_text[0]);
	}

	static String literal(const String &p_text, int p_column) {
		int trailing = 0;
		while (trailing < p_text.length() && p_text[p_text.length() - 1 - trailing] == '\n') {
			trailing++;
		}
		const String body = p_text.substr(0, p_text.length() - trailing);
		String result = trailing == 0 ? "|-\n" : (trailing == 1 ? "|\n" : "|+\n");
		const String indent = String(" ").repeat(p_column);
		const Vector<String> lines = body.split("\n");
		for (const String &part : lines) {
			if (part.is_empty()) {
				result += "\n";
			} else {
				result += indent + part + "\n";
			}
		}
		if (trailing > 1) {
			result += String("\n").repeat(trailing - 1);
		}
		return result;
	}

	bool collect_entries(const Dictionary &p_map, const String &p_where, Vector<Entry> &r_entries) {
		HashSet<String> seen;
		for (const KeyValue<Variant, Variant> &kv : p_map) {
			Entry entry;
			entry.key = kv.key;
			entry.value = kv.value;
			if (seen.has(entry.key)) {
				error = vformat("'%s' has two keys that are both written as \"%s\".", p_where, entry.key);
				return false;
			}
			seen.insert(entry.key);
			r_entries.push_back(entry);
		}
		if (sort_keys) {
			r_entries.sort();
		}
		return true;
	}

	// A scalar, or an empty or flow collection, on one line.
	bool write_inline(const Variant &p_value, const String &p_where, String &r_text) {
		switch (p_value.get_type()) {
			case Variant::NIL:
				r_text += "null";
				return true;
			case Variant::BOOL:
				r_text += bool(p_value) ? "true" : "false";
				return true;
			case Variant::INT:
				r_text += itos(int64_t(p_value));
				return true;
			case Variant::FLOAT:
				r_text += number(double(p_value));
				return true;
			case Variant::STRING:
			case Variant::STRING_NAME:
			case Variant::NODE_PATH:
				r_text += text_scalar(String(p_value), flow);
				return true;
			case Variant::DICTIONARY: {
				const Dictionary map = p_value;
				if (!enter(map.id(), p_where)) {
					return false;
				}
				Vector<Entry> entries;
				if (!collect_entries(map, p_where, entries)) {
					return false;
				}
				r_text += "{";
				for (int i = 0; i < entries.size(); i++) {
					if (i > 0) {
						r_text += ", ";
					}
					r_text += text_scalar(entries[i].key, true) + ": ";
					if (!write_inline(entries[i].value, p_where + "." + entries[i].key, r_text)) {
						return false;
					}
				}
				r_text += "}";
				stack.erase(map.id());
				return true;
			}
			case Variant::OBJECT:
			case Variant::RID:
			case Variant::CALLABLE:
			case Variant::SIGNAL:
				error = vformat("'%s' has the type %s, which can't be written as YAML.", p_where, Variant::get_type_name(p_value.get_type()));
				return false;
			default:
				break;
		}
		if (p_value.is_array()) {
			const Array array = p_value;
			const bool shared = p_value.get_type() == Variant::ARRAY;
			if (shared && !enter(array.id(), p_where)) {
				return false;
			}
			r_text += "[";
			for (int i = 0; i < array.size(); i++) {
				if (i > 0) {
					r_text += ", ";
				}
				if (!write_inline(array[i], p_where + "[" + itos(i) + "]", r_text)) {
					return false;
				}
			}
			r_text += "]";
			if (shared) {
				stack.erase(array.id());
			}
			return true;
		}
		// Godot types without YAML syntax (Vector2, Color, ...) as strings, like JSON.stringify().
		r_text += text_scalar(String(p_value), flow);
		return true;
	}

	// The value after "key:" or "-": a nested block on the next lines, a literal block scalar or an inline value.
	bool write_value(const Variant &p_value, int p_column, const String &p_where, String &r_text) {
		if (p_value.get_type() == Variant::DICTIONARY && is_container(p_value)) {
			r_text += "\n";
			return write_mapping(p_value, p_column, false, p_where, r_text);
		}
		if (p_value.is_array() && is_container(p_value)) {
			r_text += "\n";
			return write_sequence(p_value, p_column, false, p_where, r_text);
		}
		if (is_string(p_value) && is_literal_candidate(String(p_value))) {
			r_text += " " + literal(String(p_value), p_column);
			return true;
		}
		r_text += " ";
		if (!write_inline(p_value, p_where, r_text)) {
			return false;
		}
		r_text += "\n";
		return true;
	}

	bool write_mapping(const Dictionary &p_map, int p_column, bool p_inline_first, const String &p_where, String &r_text) {
		if (!enter(p_map.id(), p_where)) {
			return false;
		}
		Vector<Entry> entries;
		if (!collect_entries(p_map, p_where, entries)) {
			return false;
		}
		const String indent = String(" ").repeat(p_column);
		for (int i = 0; i < entries.size(); i++) {
			if (i > 0 || !p_inline_first) {
				r_text += indent;
			}
			r_text += text_scalar(entries[i].key, false) + ":";
			if (!write_value(entries[i].value, p_column + indent_width, p_where.is_empty() ? entries[i].key : p_where + "." + entries[i].key, r_text)) {
				return false;
			}
		}
		stack.erase(p_map.id());
		return true;
	}

	bool write_sequence(const Variant &p_array, int p_column, bool p_inline_first, const String &p_where, String &r_text) {
		const Array array = p_array;
		const bool shared = p_array.get_type() == Variant::ARRAY;
		if (shared && !enter(array.id(), p_where)) {
			return false;
		}
		const String indent = String(" ").repeat(p_column);
		for (int i = 0; i < array.size(); i++) {
			if (i > 0 || !p_inline_first) {
				r_text += indent;
			}
			r_text += "-";
			const Variant item = array[i];
			const String where = p_where + "[" + itos(i) + "]";
			// Nested collections start on the line of the "-" (compact form), two columns further in.
			if (item.get_type() == Variant::DICTIONARY && is_container(item)) {
				r_text += " ";
				if (!write_mapping(item, p_column + 2, true, where, r_text)) {
					return false;
				}
			} else if (item.is_array() && is_container(item)) {
				r_text += " ";
				if (!write_sequence(item, p_column + 2, true, where, r_text)) {
					return false;
				}
			} else if (!write_value(item, p_column + 2, where, r_text)) {
				return false;
			}
		}
		if (shared) {
			stack.erase(array.id());
		}
		return true;
	}

	bool write_document(const Variant &p_data, String &r_text) {
		if (!flow) {
			if (p_data.get_type() == Variant::DICTIONARY && is_container(p_data)) {
				return write_mapping(p_data, 0, false, String(), r_text);
			}
			if (p_data.is_array() && is_container(p_data)) {
				return write_sequence(p_data, 0, false, String(), r_text);
			}
			if (is_string(p_data) && is_literal_candidate(String(p_data))) {
				r_text += literal(String(p_data), indent_width);
				return true;
			}
		}
		if (!write_inline(p_data, "data", r_text)) {
			return false;
		}
		r_text += "\n";
		return true;
	}
};

} // namespace

/////////////////////////////////////////////////////////////////////////////
// YAML.

Error YAML::parse_text(const String &p_text, Variant &r_data, String &r_error, int &r_line, int &r_column, Array *r_documents) {
	YAMLReader reader;
	if (!reader.parse(p_text)) {
		r_data = Variant();
		if (r_documents) {
			r_documents->clear();
		}
		r_error = reader.error;
		r_line = reader.error_line;
		r_column = reader.error_column;
		return ERR_PARSE_ERROR;
	}
	r_data = reader.documents[0];
	if (r_documents) {
		*r_documents = reader.documents;
	}
	r_error = String();
	r_line = 0;
	r_column = 0;
	return OK;
}

Error YAML::write_text(const Variant &p_data, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error) {
	YAMLWriter writer;
	writer.flow = p_options.indent.is_empty();
	writer.sort_keys = p_options.sort_keys;
	int width = 0;
	for (int i = 0; i < p_options.indent.length(); i++) {
		width += p_options.indent[i] == '\t' ? 4 : 1; // YAML has no tabs in indentation.
	}
	writer.indent_width = MAX(width, 1);
	String text;
	if (!writer.write_document(p_data, text)) {
		r_error = writer.error;
		return ERR_INVALID_DATA;
	}
	r_text = text;
	return OK;
}

String YAML::get_marker_type(const String &p_text) {
	if (!p_text.contains(YAML_KEY_TYPE)) {
		return String(); // Fast path for plain YAML files.
	}
	Variant tree;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, tree, error, line, column) != OK || tree.get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Variant type = Dictionary(tree).get(YAML_KEY_TYPE, Variant());
	if (type.get_type() != Variant::STRING || !ResourceTextSerializer::is_resource_type(type)) {
		return String();
	}
	return type;
}

Error YAML::write_resource(const Ref<Resource> &p_resource, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error) {
	Dictionary tree;
	const Error err = ResourceTextSerializer::encode_resource(p_resource, p_options, true, tree, r_error);
	if (err != OK) {
		return err;
	}
	return write_text(tree, p_options, r_text, r_error);
}

Error YAML::parse_resource(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	Variant tree;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, tree, error, line, column) != OK) {
		r_error = vformat("YAML parse error on line %d, column %d: %s", line, column, error);
		// Without a type hint and a type marker the file may belong to another loader.
		return (hint.is_empty() && !p_text.contains(YAML_KEY_TYPE)) ? ERR_FILE_UNRECOGNIZED : ERR_PARSE_ERROR;
	}
	if (tree.get_type() != Variant::DICTIONARY) {
		r_error = "The YAML document is not a mapping, so it is not a resource.";
		return hint.is_empty() ? ERR_FILE_UNRECOGNIZED : ERR_INVALID_DATA;
	}
	const Dictionary map = tree;
	if (hint.is_empty()) {
		const Variant type = map.get(YAML_KEY_TYPE, Variant());
		if (type.get_type() != Variant::STRING || !ResourceTextSerializer::is_resource_type(type)) {
			r_error = vformat("The YAML has no key \"%s\" with a Resource type.", YAML_KEY_TYPE);
			return ERR_FILE_UNRECOGNIZED;
		}
	}
	return ResourceTextSerializer::decode_resource(map, hint, r_resource, r_error);
}

Error YAML::parse_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	// Reading has no options yet.
	return parse_resource(p_text, p_type_hint, r_resource, r_error);
}

Error YAML::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	Array documents;
	const Error err = parse_text(p_text, r_data, r_error, r_line, r_column, &documents);
	extra_documents = documents.slice(1);
	return err;
}

Error YAML::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	Error err = write_text(p_data, options, r_text, r_error);
	for (int i = 0; i < extra_documents.size() && err == OK; i++) {
		String document;
		err = write_text(extra_documents[i], options, document, r_error);
		r_text += "---\n" + document;
	}
	return err;
}

Variant YAML::get_document(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, get_document_count(), Variant());
	return p_index == 0 ? get_data() : extra_documents[p_index - 1];
}

void YAML::add_document(const Variant &p_data) {
	extra_documents.push_back(p_data);
	_mark_data_changed();
}

void YAML::remove_document(int p_index) {
	ERR_FAIL_INDEX(p_index, get_document_count());
	if (p_index > 0) {
		extra_documents.remove_at(p_index - 1);
		_mark_data_changed();
	} else if (extra_documents.is_empty()) {
		clear();
	} else {
		set_data(extra_documents.pop_front());
	}
}

void YAML::_get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("yaml");
	r_extensions->push_back("yml");
}

Vector<String> YAML::split_path(const String &p_path) {
	Vector<String> parts;
	for (const String &part : p_path.split("/", false)) {
		parts.push_back(part.replace("~1", "/").replace("~0", "~"));
	}
	return parts;
}

String YAML::escape_key(const String &p_key) {
	return p_key.replace("~", "~0").replace("/", "~1");
}

bool YAML::_get_child(const Variant &p_node, const String &p_part, Variant &r_child) {
	if (p_node.get_type() == Variant::DICTIONARY) {
		const Dictionary map = p_node;
		if (!map.has(p_part)) {
			return false;
		}
		r_child = map[p_part];
		return true;
	}
	if (p_node.get_type() == Variant::ARRAY && p_part.is_valid_int()) {
		const Array list = p_node;
		const int64_t index = p_part.to_int();
		if (index < 0 || index >= list.size()) {
			return false;
		}
		r_child = list[index];
		return true;
	}
	return false;
}

bool YAML::_resolve_parent(const Vector<String> &p_parts, bool p_create, Variant &r_parent) const {
	Variant node = get_data();
	for (int i = 0; i < p_parts.size() - 1; i++) {
		Variant child;
		if (!_get_child(node, p_parts[i], child) || (p_create && child.get_type() == Variant::NIL)) {
			if (!p_create || node.get_type() != Variant::DICTIONARY) {
				return false;
			}
			Dictionary map = node;
			child = Dictionary();
			map[p_parts[i]] = child;
		}
		node = child;
	}
	r_parent = node;
	return true;
}

Variant YAML::get_value(const String &p_path, const Variant &p_default) const {
	Variant node = get_data();
	for (const String &part : split_path(p_path)) {
		Variant child;
		if (!_get_child(node, part, child)) {
			return p_default;
		}
		node = child;
	}
	return node;
}

bool YAML::has_value(const String &p_path) const {
	Variant node = get_data();
	for (const String &part : split_path(p_path)) {
		Variant child;
		if (!_get_child(node, part, child)) {
			return false;
		}
		node = child;
	}
	return true;
}

void YAML::set_value(const String &p_path, const Variant &p_value) {
	const Vector<String> parts = split_path(p_path);
	if (parts.is_empty()) {
		set_data(p_value);
		return;
	}
	if (get_data().get_type() == Variant::NIL) {
		_get_data_ref() = Dictionary();
	}
	Variant parent;
	ERR_FAIL_COND_MSG(!_resolve_parent(parts, true, parent), vformat("Can't set \"%s\": a part of the path is not a mapping.", p_path));
	const String &last = parts[parts.size() - 1];
	if (parent.get_type() == Variant::DICTIONARY) {
		Dictionary map = parent;
		map[last] = p_value;
	} else if (parent.get_type() == Variant::ARRAY) {
		Array list = parent;
		ERR_FAIL_COND_MSG(!last.is_valid_int(), vformat("Can't set \"%s\": \"%s\" is not an array index.", p_path, last));
		const int64_t index = last.to_int();
		ERR_FAIL_COND_MSG(index < 0 || index > list.size(), vformat("Can't set \"%s\": index %d is out of range.", p_path, index));
		if (index == list.size()) {
			list.push_back(p_value);
		} else {
			list[index] = p_value;
		}
	} else {
		ERR_FAIL_MSG(vformat("Can't set \"%s\": the parent is not a mapping or a sequence.", p_path));
	}
	_mark_data_changed();
}

void YAML::erase_value(const String &p_path) {
	const Vector<String> parts = split_path(p_path);
	if (parts.is_empty()) {
		clear();
		return;
	}
	Variant parent;
	ERR_FAIL_COND_MSG(!_resolve_parent(parts, false, parent), vformat("Cannot erase nonexistent value \"%s\".", p_path));
	const String &last = parts[parts.size() - 1];
	if (parent.get_type() == Variant::DICTIONARY) {
		Dictionary map = parent;
		ERR_FAIL_COND_MSG(!map.erase(last), vformat("Cannot erase nonexistent value \"%s\".", p_path));
	} else if (parent.get_type() == Variant::ARRAY && last.is_valid_int()) {
		Array list = parent;
		const int64_t index = last.to_int();
		ERR_FAIL_INDEX_MSG(index, list.size(), vformat("Cannot erase nonexistent value \"%s\".", p_path));
		list.remove_at(index);
	} else {
		ERR_FAIL_MSG(vformat("Cannot erase nonexistent value \"%s\".", p_path));
	}
	_mark_data_changed();
}

Variant YAML::parse_string(const String &p_text) {
	Variant result;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, result, error, line, column) != OK) {
		ERR_PRINT(vformat("YAML parse error on line %d, column %d: %s", line, column, error));
		return Variant();
	}
	return result;
}

String YAML::stringify(const Variant &p_data, const Ref<SerializerOptions> &p_options) {
	String text;
	String error;
	if (write_text(p_data, ResourceTextSerializer::get_options(p_options), text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

String YAML::serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, String());
	return serializer->serialize(p_resource, ResourceTextSerializer::FORMAT_YAML, p_options);
}

Ref<Resource> YAML::deserialize(const String &p_text, const StringName &p_type_hint, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, Ref<Resource>());
	return serializer->deserialize(p_text, ResourceTextSerializer::FORMAT_YAML, p_type_hint, p_options);
}

YAML::YAML() {
	// A new YAML document is an empty mapping ("{}"), not null.
	_get_data_ref() = Dictionary();
}

void YAML::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value", "path", "default"), &YAML::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("set_value", "path", "value"), &YAML::set_value);
	ClassDB::bind_method(D_METHOD("has_value", "path"), &YAML::has_value);
	ClassDB::bind_method(D_METHOD("erase_value", "path"), &YAML::erase_value);
	ClassDB::bind_method(D_METHOD("get_document_count"), &YAML::get_document_count);
	ClassDB::bind_method(D_METHOD("get_document", "index"), &YAML::get_document);
	ClassDB::bind_method(D_METHOD("add_document", "data"), &YAML::add_document);
	ClassDB::bind_method(D_METHOD("remove_document", "index"), &YAML::remove_document);

	ClassDB::bind_static_method("YAML", D_METHOD("parse_string", "text"), &YAML::parse_string);
	ClassDB::bind_static_method("YAML", D_METHOD("stringify", "data", "options"), &YAML::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("YAML", D_METHOD("serialize", "resource", "options"), &YAML::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("YAML", D_METHOD("deserialize", "text", "type_hint", "options"), &YAML::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
