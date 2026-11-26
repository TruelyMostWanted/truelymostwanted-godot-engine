/**************************************************************************/
/*  toml.cpp                                                              */
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

#include "toml.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/string/char_utils.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"

static constexpr const char *TOML_KEY_TYPE = "$type";
static constexpr int TOML_MAX_DEPTH = 512;

/////////////////////////////////////////////////////////////////////////////
// Reader.

namespace {

struct TOMLReader {
	const char32_t *src = nullptr;
	int len = 0;
	int pos = 0;
	int line = 1;
	int line_start = 0;

	String error;
	int error_line = 0;
	int error_column = 0;

	Dictionary root;
	Dictionary current;
	// Tables defined by a [header] (can't be defined again).
	HashSet<const void *> header_tables;
	// Tables created by dotted keys (can't be defined by a [header], can only be extended by dotted keys).
	HashSet<const void *> dotted_tables;
	// Inline tables and arrays of values: closed, nothing can be added later.
	HashSet<const void *> closed;
	// Arrays created by [[header]].
	HashSet<const void *> table_arrays;

	char32_t peek(int p_offset = 0) const {
		const int at = pos + p_offset;
		return at < len ? src[at] : 0;
	}

	bool at_end() const { return pos >= len; }

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
		if (error.is_empty()) {
			error = p_message;
			error_line = line;
			error_column = pos - line_start + 1;
		}
		return false;
	}

	bool at_newline() const {
		return peek() == '\n' || (peek() == '\r' && peek(1) == '\n');
	}

	// Consumes one line break ("\n" or "\r\n").
	void skip_newline() {
		if (peek() == '\r') {
			advance();
		}
		advance();
	}

	void skip_ws() {
		while (peek() == ' ' || peek() == '\t') {
			advance();
		}
	}

	bool skip_comment() {
		if (peek() != '#') {
			return true;
		}
		while (!at_end() && !at_newline()) {
			const char32_t c = peek();
			if ((c < 0x20 && c != '\t') || c == 0x7F) {
				return fail("Control characters are not allowed in comments.");
			}
			advance();
		}
		return true;
	}

	// Whitespace, comments and line breaks (inside arrays).
	bool skip_ws_comments_newlines() {
		while (true) {
			skip_ws();
			if (peek() == '#') {
				if (!skip_comment()) {
					return false;
				}
			} else if (at_newline()) {
				skip_newline();
			} else {
				return true;
			}
		}
	}

	bool expect_line_end() {
		skip_ws();
		if (!skip_comment()) {
			return false;
		}
		if (at_end()) {
			return true;
		}
		if (at_newline()) {
			skip_newline();
			return true;
		}
		return fail(vformat("Expected a line break, found '%s'.", String::chr(peek())));
	}

	static bool is_bare_key_char(char32_t p_c) {
		return is_ascii_alphanumeric_char(p_c) || p_c == '_' || p_c == '-';
	}

	static bool is_value_end(char32_t p_c) {
		return p_c == 0 || p_c == ' ' || p_c == '\t' || p_c == '\r' || p_c == '\n' || p_c == ',' || p_c == ']' || p_c == '}' || p_c == '#';
	}

	bool parse(const String &p_text);
	bool parse_header();
	bool parse_key(LocalVector<String> &r_parts);
	bool parse_key_value(Dictionary &p_table);
	bool assign(Dictionary &p_table, const LocalVector<String> &p_parts, const Variant &p_value);
	bool parse_value(Variant &r_value, int p_depth);
	bool parse_escape(String &r_text);
	bool parse_basic_string(String &r_text);
	bool parse_literal_string(String &r_text);
	bool parse_multiline_basic_string(String &r_text);
	bool parse_multiline_literal_string(String &r_text);
	bool parse_array(Variant &r_value, int p_depth);
	bool parse_inline_table(Variant &r_value, int p_depth);
	bool parse_number_or_date(Variant &r_value);
	bool parse_date_time(Variant &r_value);
	bool parse_number(const String &p_token, Variant &r_value);
	int read_digits(int p_count);
};

bool TOMLReader::parse(const String &p_text) {
	src = p_text.ptr();
	len = p_text.length();
	current = root;
	if (peek() == 0xFEFF) {
		advance();
	}
	while (!at_end()) {
		skip_ws();
		if (peek() == '#') {
			if (!skip_comment()) {
				return false;
			}
			continue;
		}
		if (at_newline()) {
			skip_newline();
			continue;
		}
		if (at_end()) {
			break;
		}
		if (peek() == '[') {
			if (!parse_header()) {
				return false;
			}
		} else if (!parse_key_value(current)) {
			return false;
		}
		if (!expect_line_end()) {
			return false;
		}
	}
	return true;
}

static String _join_key(const LocalVector<String> &p_parts, uint32_t p_count) {
	String result;
	for (uint32_t i = 0; i < p_count && i < p_parts.size(); i++) {
		if (i > 0) {
			result += ".";
		}
		result += p_parts[i];
	}
	return result;
}

bool TOMLReader::parse_header() {
	advance(); // [
	const bool array = peek() == '[';
	if (array) {
		advance();
	}
	skip_ws();
	LocalVector<String> parts;
	if (!parse_key(parts)) {
		return false;
	}
	skip_ws();
	if (peek() != ']') {
		return fail("Expected ']' at the end of the table header.");
	}
	advance();
	if (array) {
		if (peek() != ']') {
			return fail("Expected ']]' at the end of the array of tables header.");
		}
		advance();
	}

	Dictionary table = root;
	for (uint32_t i = 0; i + 1 < parts.size(); i++) {
		Variant *value = table.getptr(parts[i]);
		if (value == nullptr) {
			Dictionary created;
			table[parts[i]] = created;
			table = created;
		} else if (value->get_type() == Variant::DICTIONARY) {
			Dictionary existing = *value;
			if (closed.has(existing.id())) {
				return fail(vformat("Can't add a table to the inline table '%s'.", _join_key(parts, i + 1)));
			}
			table = existing;
		} else if (value->get_type() == Variant::ARRAY) {
			Array existing = *value;
			if (!table_arrays.has(existing.id()) || existing.is_empty()) {
				return fail(vformat("Can't add a table to the array '%s'.", _join_key(parts, i + 1)));
			}
			table = existing[existing.size() - 1];
		} else {
			return fail(vformat("The key '%s' is already defined as a value.", _join_key(parts, i + 1)));
		}
	}

	const String &last = parts[parts.size() - 1];
	Variant *value = table.getptr(last);
	if (!array) {
		if (value == nullptr) {
			Dictionary created;
			table[last] = created;
			header_tables.insert(created.id());
			current = created;
			return true;
		}
		if (value->get_type() == Variant::DICTIONARY) {
			Dictionary existing = *value;
			if (header_tables.has(existing.id()) || dotted_tables.has(existing.id()) || closed.has(existing.id())) {
				return fail(vformat("The table '%s' is already defined.", _join_key(parts, parts.size())));
			}
			header_tables.insert(existing.id());
			current = existing;
			return true;
		}
		return fail(vformat("The key '%s' is already defined as a value.", _join_key(parts, parts.size())));
	}

	Dictionary element;
	if (value == nullptr) {
		Array created;
		table_arrays.insert(created.id());
		created.push_back(element);
		table[last] = created;
	} else if (value->get_type() == Variant::ARRAY && table_arrays.has(Array(*value).id())) {
		Array existing = *value;
		existing.push_back(element);
	} else {
		return fail(vformat("The key '%s' is already defined and is not an array of tables.", _join_key(parts, parts.size())));
	}
	current = element;
	return true;
}

bool TOMLReader::parse_key(LocalVector<String> &r_parts) {
	while (true) {
		skip_ws();
		String part;
		const char32_t c = peek();
		if (c == '"') {
			if (peek(1) == '"' && peek(2) == '"') {
				return fail("Multi-line strings can't be used as keys.");
			}
			if (!parse_basic_string(part)) {
				return false;
			}
		} else if (c == '\'') {
			if (peek(1) == '\'' && peek(2) == '\'') {
				return fail("Multi-line strings can't be used as keys.");
			}
			if (!parse_literal_string(part)) {
				return false;
			}
		} else if (is_bare_key_char(c)) {
			const int start = pos;
			while (is_bare_key_char(peek())) {
				advance();
			}
			part = String::utf32(Span(src + start, pos - start));
		} else {
			return fail("Expected a key.");
		}
		r_parts.push_back(part);
		skip_ws();
		if (peek() != '.') {
			return true;
		}
		advance();
	}
}

bool TOMLReader::assign(Dictionary &p_table, const LocalVector<String> &p_parts, const Variant &p_value) {
	Dictionary table = p_table;
	for (uint32_t i = 0; i + 1 < p_parts.size(); i++) {
		Variant *value = table.getptr(p_parts[i]);
		if (value == nullptr) {
			Dictionary created;
			table[p_parts[i]] = created;
			dotted_tables.insert(created.id());
			table = created;
		} else if (value->get_type() == Variant::DICTIONARY) {
			Dictionary existing = *value;
			if (!dotted_tables.has(existing.id()) || closed.has(existing.id())) {
				return fail(vformat("Can't add keys to the table '%s' here.", _join_key(p_parts, i + 1)));
			}
			table = existing;
		} else {
			return fail(vformat("The key '%s' is already defined as a value.", _join_key(p_parts, i + 1)));
		}
	}
	const String &last = p_parts[p_parts.size() - 1];
	if (table.has(last)) {
		return fail(vformat("The key '%s' is defined twice.", _join_key(p_parts, p_parts.size())));
	}
	table[last] = p_value;
	return true;
}

bool TOMLReader::parse_key_value(Dictionary &p_table) {
	LocalVector<String> parts;
	if (!parse_key(parts)) {
		return false;
	}
	skip_ws();
	if (peek() != '=') {
		return fail(vformat("Expected '=' after the key '%s'.", _join_key(parts, parts.size())));
	}
	advance();
	skip_ws();
	Variant value;
	if (!parse_value(value, 0)) {
		return false;
	}
	return assign(p_table, parts, value);
}

bool TOMLReader::parse_value(Variant &r_value, int p_depth) {
	if (p_depth > TOML_MAX_DEPTH) {
		return fail("The value is nested too deeply.");
	}
	const char32_t c = peek();
	switch (c) {
		case '"': {
			String text;
			const bool ok = (peek(1) == '"' && peek(2) == '"') ? parse_multiline_basic_string(text) : parse_basic_string(text);
			r_value = text;
			return ok;
		}
		case '\'': {
			String text;
			const bool ok = (peek(1) == '\'' && peek(2) == '\'') ? parse_multiline_literal_string(text) : parse_literal_string(text);
			r_value = text;
			return ok;
		}
		case '[':
			return parse_array(r_value, p_depth);
		case '{':
			return parse_inline_table(r_value, p_depth);
		case 't':
			if (peek(1) == 'r' && peek(2) == 'u' && peek(3) == 'e' && is_value_end(peek(4))) {
				for (int i = 0; i < 4; i++) {
					advance();
				}
				r_value = true;
				return true;
			}
			break;
		case 'f':
			if (peek(1) == 'a' && peek(2) == 'l' && peek(3) == 's' && peek(4) == 'e' && is_value_end(peek(5))) {
				for (int i = 0; i < 5; i++) {
					advance();
				}
				r_value = false;
				return true;
			}
			break;
		case 0:
			return fail("Expected a value.");
		default:
			break;
	}
	if (is_digit(c) || c == '+' || c == '-' || c == 'i' || c == 'n') {
		return parse_number_or_date(r_value);
	}
	return fail(vformat("Unexpected character '%s' at the start of a value.", String::chr(c)));
}

bool TOMLReader::parse_escape(String &r_text) {
	advance(); // Backslash.
	const char32_t c = peek();
	switch (c) {
		case 'b':
			r_text += '\b';
			break;
		case 't':
			r_text += '\t';
			break;
		case 'n':
			r_text += '\n';
			break;
		case 'f':
			r_text += '\f';
			break;
		case 'r':
			r_text += '\r';
			break;
		case '"':
			r_text += '"';
			break;
		case '\\':
			r_text += '\\';
			break;
		case 'u':
		case 'U': {
			const int count = c == 'u' ? 4 : 8;
			advance();
			uint32_t code = 0;
			for (int i = 0; i < count; i++) {
				const char32_t h = peek();
				if (!is_hex_digit(h)) {
					return fail(vformat("Expected %d hexadecimal digits in the escape sequence.", count));
				}
				code = code * 16 + uint32_t(is_digit(h) ? h - '0' : (h | 0x20) - 'a' + 10);
				advance();
			}
			if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
				return fail("The escape sequence is not a Unicode scalar value.");
			}
			r_text += char32_t(code);
			return true;
		}
		default:
			return fail(vformat("Invalid escape sequence '\\%s'.", String::chr(c)));
	}
	advance();
	return true;
}

bool TOMLReader::parse_basic_string(String &r_text) {
	advance(); // "
	while (true) {
		const char32_t c = peek();
		if (at_end() || c == '\n' || c == '\r') {
			return fail("The string is not closed on the same line.");
		}
		if (c == '"') {
			advance();
			return true;
		}
		if (c == '\\') {
			if (!parse_escape(r_text)) {
				return false;
			}
			continue;
		}
		if ((c < 0x20 && c != '\t') || c == 0x7F) {
			return fail("Control characters must be escaped in strings.");
		}
		r_text += c;
		advance();
	}
}

bool TOMLReader::parse_literal_string(String &r_text) {
	advance(); // '
	while (true) {
		const char32_t c = peek();
		if (at_end() || c == '\n' || c == '\r') {
			return fail("The string is not closed on the same line.");
		}
		if (c == '\'') {
			advance();
			return true;
		}
		if ((c < 0x20 && c != '\t') || c == 0x7F) {
			return fail("Control characters are not allowed in literal strings.");
		}
		r_text += c;
		advance();
	}
}

bool TOMLReader::parse_multiline_basic_string(String &r_text) {
	for (int i = 0; i < 3; i++) {
		advance();
	}
	if (at_newline()) {
		skip_newline(); // A line break right after the delimiter is trimmed.
	}
	while (true) {
		if (at_end()) {
			return fail("The multi-line string is not closed.");
		}
		const char32_t c = peek();
		if (c == '"' && peek(1) == '"' && peek(2) == '"') {
			int quotes = 0;
			while (peek() == '"') {
				quotes++;
				advance();
			}
			if (quotes > 5) {
				return fail("Too many quotes at the end of the multi-line string.");
			}
			for (int i = 3; i < quotes; i++) {
				r_text += '"';
			}
			return true;
		}
		if (c == '\\') {
			// A backslash at the end of a line trims the line break and the whitespace that follows.
			int look = 1;
			while (peek(look) == ' ' || peek(look) == '\t') {
				look++;
			}
			if (peek(look) == '\n' || (peek(look) == '\r' && peek(look + 1) == '\n')) {
				advance();
				while (peek() == ' ' || peek() == '\t' || at_newline()) {
					if (at_newline()) {
						skip_newline();
					} else {
						advance();
					}
				}
				continue;
			}
			if (!parse_escape(r_text)) {
				return false;
			}
			continue;
		}
		if (at_newline()) {
			skip_newline();
			r_text += '\n';
			continue;
		}
		if ((c < 0x20 && c != '\t') || c == 0x7F) {
			return fail("Control characters must be escaped in strings.");
		}
		r_text += c;
		advance();
	}
}

bool TOMLReader::parse_multiline_literal_string(String &r_text) {
	for (int i = 0; i < 3; i++) {
		advance();
	}
	if (at_newline()) {
		skip_newline();
	}
	while (true) {
		if (at_end()) {
			return fail("The multi-line string is not closed.");
		}
		const char32_t c = peek();
		if (c == '\'' && peek(1) == '\'' && peek(2) == '\'') {
			int quotes = 0;
			while (peek() == '\'') {
				quotes++;
				advance();
			}
			if (quotes > 5) {
				return fail("Too many quotes at the end of the multi-line string.");
			}
			for (int i = 3; i < quotes; i++) {
				r_text += '\'';
			}
			return true;
		}
		if (at_newline()) {
			skip_newline();
			r_text += '\n';
			continue;
		}
		if ((c < 0x20 && c != '\t') || c == 0x7F) {
			return fail("Control characters are not allowed in literal strings.");
		}
		r_text += c;
		advance();
	}
}

bool TOMLReader::parse_array(Variant &r_value, int p_depth) {
	advance(); // [
	Array array;
	while (true) {
		if (!skip_ws_comments_newlines()) {
			return false;
		}
		if (peek() == ']') {
			advance();
			break;
		}
		Variant element;
		if (!parse_value(element, p_depth + 1)) {
			return false;
		}
		array.push_back(element);
		if (!skip_ws_comments_newlines()) {
			return false;
		}
		if (peek() == ',') {
			advance();
			continue;
		}
		if (peek() == ']') {
			advance();
			break;
		}
		return fail("Expected ',' or ']' in the array.");
	}
	closed.insert(array.id());
	r_value = array;
	return true;
}

bool TOMLReader::parse_inline_table(Variant &r_value, int p_depth) {
	advance(); // {
	Dictionary table;
	skip_ws();
	if (peek() == '}') {
		advance();
	} else {
		while (true) {
			skip_ws();
			LocalVector<String> parts;
			if (!parse_key(parts)) {
				return false;
			}
			skip_ws();
			if (peek() != '=') {
				return fail(vformat("Expected '=' after the key '%s'.", _join_key(parts, parts.size())));
			}
			advance();
			skip_ws();
			Variant value;
			if (!parse_value(value, p_depth + 1)) {
				return false;
			}
			if (!assign(table, parts, value)) {
				return false;
			}
			skip_ws();
			if (peek() == ',') {
				advance();
				continue;
			}
			if (peek() == '}') {
				advance();
				break;
			}
			return fail("Expected ',' or '}' in the inline table (inline tables must be on one line).");
		}
	}
	closed.insert(table.id());
	r_value = table;
	return true;
}

// Reads exactly p_count digits; returns -1 if they are missing.
int TOMLReader::read_digits(int p_count) {
	int value = 0;
	for (int i = 0; i < p_count; i++) {
		if (!is_digit(peek())) {
			return -1;
		}
		value = value * 10 + int(peek() - '0');
		advance();
	}
	return value;
}

bool TOMLReader::parse_date_time(Variant &r_value) {
	const int start = pos;
	bool has_date = false;
	bool has_time = false;

	if (is_digit(peek(0)) && is_digit(peek(1)) && is_digit(peek(2)) && is_digit(peek(3)) && peek(4) == '-') {
		read_digits(4);
		advance();
		const int month = read_digits(2);
		if (month < 1 || month > 12 || peek() != '-') {
			return fail("Invalid date.");
		}
		advance();
		const int day = read_digits(2);
		if (day < 1 || day > 31) {
			return fail("Invalid date.");
		}
		has_date = true;
		const char32_t separator = peek();
		if ((separator == 'T' || separator == 't' || separator == ' ') && is_digit(peek(1)) && is_digit(peek(2)) && peek(3) == ':') {
			advance();
		} else {
			r_value = String::utf32(Span(src + start, pos - start));
			return true;
		}
	}

	const int hour = read_digits(2);
	if (hour < 0 || hour > 23 || peek() != ':') {
		return fail("Invalid time.");
	}
	advance();
	const int minute = read_digits(2);
	if (minute < 0 || minute > 59 || peek() != ':') {
		return fail("Invalid time (TOML 1.0 requires seconds).");
	}
	advance();
	const int second = read_digits(2);
	if (second < 0 || second > 60) {
		return fail("Invalid time.");
	}
	has_time = true;
	if (peek() == '.') {
		advance();
		if (!is_digit(peek())) {
			return fail("Expected digits after '.' in the time.");
		}
		while (is_digit(peek())) {
			advance();
		}
	}
	if (has_date && has_time) {
		const char32_t offset = peek();
		if (offset == 'Z' || offset == 'z') {
			advance();
		} else if (offset == '+' || offset == '-') {
			advance();
			const int offset_hour = read_digits(2);
			if (offset_hour < 0 || offset_hour > 23 || peek() != ':') {
				return fail("Invalid time offset.");
			}
			advance();
			const int offset_minute = read_digits(2);
			if (offset_minute < 0 || offset_minute > 59) {
				return fail("Invalid time offset.");
			}
		}
	}
	if (!is_value_end(peek())) {
		return fail("Invalid date or time.");
	}
	r_value = String::utf32(Span(src + start, pos - start));
	return true;
}

bool TOMLReader::parse_number_or_date(Variant &r_value) {
	// Dates start with four digits and '-', times with two digits and ':'.
	if ((is_digit(peek(0)) && is_digit(peek(1)) && is_digit(peek(2)) && is_digit(peek(3)) && peek(4) == '-') ||
			(is_digit(peek(0)) && is_digit(peek(1)) && peek(2) == ':')) {
		return parse_date_time(r_value);
	}
	const int start = pos;
	while (is_ascii_alphanumeric_char(peek()) || peek() == '_' || peek() == '.' || peek() == '+' || peek() == '-') {
		advance();
	}
	const String token = String::utf32(Span(src + start, pos - start));
	if (!is_value_end(peek())) {
		return fail(vformat("Invalid value '%s'.", token + String::chr(peek())));
	}
	if (!parse_number(token, r_value)) {
		pos = start; // Report the error at the start of the number.
		error_column = pos - line_start + 1;
		return false;
	}
	return true;
}

// Digits of the given base with single underscores between them. Writes the digits without underscores.
static bool _strip_digits(const String &p_text, int p_base, String &r_digits) {
	if (p_text.is_empty()) {
		return false;
	}
	bool previous_digit = false;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (c == '_') {
			if (!previous_digit || i + 1 >= p_text.length()) {
				return false;
			}
			previous_digit = false;
			continue;
		}
		bool valid = false;
		switch (p_base) {
			case 2:
				valid = c == '0' || c == '1';
				break;
			case 8:
				valid = c >= '0' && c <= '7';
				break;
			case 16:
				valid = is_hex_digit(c);
				break;
			default:
				valid = is_digit(c);
				break;
		}
		if (!valid) {
			return false;
		}
		r_digits += c;
		previous_digit = true;
	}
	return true;
}

bool TOMLReader::parse_number(const String &p_token, Variant &r_value) {
	String text = p_token;
	bool negative = false;
	bool has_sign = false;
	if (text.begins_with("+") || text.begins_with("-")) {
		negative = text[0] == '-';
		has_sign = true;
		text = text.substr(1);
	}
	if (text == "inf") {
		r_value = negative ? -Math::INF : Math::INF;
		return true;
	}
	if (text == "nan") {
		r_value = Math::NaN;
		return true;
	}

	if (text.length() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'o' || text[1] == 'b')) {
		if (has_sign) {
			return fail(vformat("Hexadecimal, octal and binary integers can't have a sign: '%s'.", p_token));
		}
		const int base = text[1] == 'x' ? 16 : (text[1] == 'o' ? 8 : 2);
		String digits;
		if (!_strip_digits(text.substr(2), base, digits)) {
			return fail(vformat("Invalid integer '%s'.", p_token));
		}
		uint64_t value = 0;
		for (int i = 0; i < digits.length(); i++) {
			const char32_t c = digits[i];
			const uint64_t digit = is_digit(c) ? uint64_t(c - '0') : uint64_t((c | 0x20) - 'a' + 10);
			if (value > (uint64_t(INT64_MAX) - digit) / uint64_t(base)) {
				return fail(vformat("The integer '%s' is out of the 64-bit range.", p_token));
			}
			value = value * uint64_t(base) + digit;
		}
		r_value = int64_t(value);
		return true;
	}

	// Decimal: integer part, optional fraction, optional exponent.
	String integer_part = text;
	String fraction_part;
	String exponent_part;
	bool has_fraction = false;
	bool has_exponent = false;
	int exponent_at = -1;
	for (int i = 0; i < text.length(); i++) {
		if (text[i] == 'e' || text[i] == 'E') {
			exponent_at = i;
			break;
		}
	}
	if (exponent_at >= 0) {
		has_exponent = true;
		exponent_part = text.substr(exponent_at + 1);
		integer_part = text.substr(0, exponent_at);
	}
	const int dot = integer_part.find_char('.');
	if (dot >= 0) {
		has_fraction = true;
		fraction_part = integer_part.substr(dot + 1);
		integer_part = integer_part.substr(0, dot);
	}

	String integer_digits;
	if (!_strip_digits(integer_part, 10, integer_digits)) {
		return fail(vformat("Invalid number '%s'.", p_token));
	}
	if (integer_digits.length() > 1 && integer_digits[0] == '0') {
		return fail(vformat("Leading zeros are not allowed: '%s'.", p_token));
	}

	if (!has_fraction && !has_exponent) {
		uint64_t value = 0;
		const uint64_t limit = negative ? uint64_t(INT64_MAX) + 1 : uint64_t(INT64_MAX);
		for (int i = 0; i < integer_digits.length(); i++) {
			const uint64_t digit = uint64_t(integer_digits[i] - '0');
			if (value > (limit - digit) / 10) {
				return fail(vformat("The integer '%s' is out of the 64-bit range.", p_token));
			}
			value = value * 10 + digit;
		}
		r_value = negative ? int64_t(0 - value) : int64_t(value);
		return true;
	}

	String number = integer_digits;
	if (has_fraction) {
		String fraction_digits;
		if (!_strip_digits(fraction_part, 10, fraction_digits)) {
			return fail(vformat("Invalid number '%s': a '.' needs digits on both sides.", p_token));
		}
		number += "." + fraction_digits;
	}
	if (has_exponent) {
		String exponent = exponent_part;
		String sign;
		if (exponent.begins_with("+") || exponent.begins_with("-")) {
			sign = exponent.substr(0, 1);
			exponent = exponent.substr(1);
		}
		String exponent_digits;
		if (!_strip_digits(exponent, 10, exponent_digits)) {
			return fail(vformat("Invalid exponent in '%s'.", p_token));
		}
		number += "e" + sign + exponent_digits;
	}
	const double value = String::to_float(number.ptr());
	r_value = negative ? -value : value;
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Writer.

struct TOMLWriter {
	String error;
	HashSet<const void *> stack;

	static bool is_bare_key(const String &p_key) {
		if (p_key.is_empty()) {
			return false;
		}
		for (int i = 0; i < p_key.length(); i++) {
			if (!TOMLReader::is_bare_key_char(p_key[i])) {
				return false;
			}
		}
		return true;
	}

	static String escape_char(char32_t p_c) {
		return "\\u" + String::num_int64(p_c, 16, true).lpad(4, "0");
	}

	// Strings with line breaks become multi-line strings, except keys (p_allow_multiline false).
	static String quote(const String &p_text, bool p_allow_multiline = true) {
		const bool multiline = p_allow_multiline && p_text.contains_char('\n');
		String result = multiline ? "\"\"\"\n" : "\"";
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
					result += multiline ? "\n" : "\\n";
					break;
				case '\t':
					result += multiline ? "\t" : "\\t";
					break;
				case '\r':
					result += "\\r";
					break;
				case '\b':
					result += "\\b";
					break;
				case '\f':
					result += "\\f";
					break;
				default:
					if (c < 0x20 || c == 0x7F) {
						result += escape_char(c);
					} else {
						result += c;
					}
					break;
			}
		}
		result += multiline ? "\"\"\"" : "\"";
		return result;
	}

	static String key(const Variant &p_key) {
		const String text = p_key;
		return is_bare_key(text) ? text : quote(text, false);
	}

	static String number(double p_value) {
		if (Math::is_nan(p_value)) {
			return "nan";
		}
		if (Math::is_inf(p_value)) {
			return p_value > 0 ? "inf" : "-inf";
		}
		const String text = String::num_scientific(p_value);
		if (text.contains_char('.') || text.contains_char('e') || text.contains_char('E')) {
			return text;
		}
		return text + ".0";
	}

	static bool is_table(const Variant &p_value) {
		return p_value.get_type() == Variant::DICTIONARY;
	}

	static bool is_table_array(const Variant &p_value) {
		if (p_value.get_type() != Variant::ARRAY) {
			return false;
		}
		const Array array = p_value;
		if (array.is_empty()) {
			return false;
		}
		for (int i = 0; i < array.size(); i++) {
			if (array[i].get_type() != Variant::DICTIONARY) {
				return false;
			}
		}
		return true;
	}

	bool enter(const void *p_id, const String &p_where) {
		if (stack.has(p_id)) {
			error = vformat("'%s' contains itself; cycles can't be written as TOML.", p_where);
			return false;
		}
		if (stack.size() >= uint32_t(TOML_MAX_DEPTH)) {
			error = vformat("'%s' is nested too deeply.", p_where);
			return false;
		}
		stack.insert(p_id);
		return true;
	}

	bool write_value(const Variant &p_value, const String &p_where, String &r_text);
	bool write_table(const Dictionary &p_table, const String &p_path, String &r_text);
};

bool TOMLWriter::write_value(const Variant &p_value, const String &p_where, String &r_text) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			// TOML has no null.
			r_text += "\"\"";
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
			r_text += quote(String(p_value));
			return true;
		case Variant::DICTIONARY: {
			const Dictionary table = p_value;
			if (!enter(table.id(), p_where)) {
				return false;
			}
			if (table.is_empty()) {
				r_text += "{}";
			} else {
				r_text += "{ ";
				bool first = true;
				for (const KeyValue<Variant, Variant> &kv : table) {
					if (!first) {
						r_text += ", ";
					}
					first = false;
					r_text += key(kv.key) + " = ";
					if (!write_value(kv.value, p_where + "." + String(kv.key), r_text)) {
						return false;
					}
				}
				r_text += " }";
			}
			stack.erase(table.id());
			return true;
		}
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
			if (!write_value(array[i], p_where + "[" + itos(i) + "]", r_text)) {
				return false;
			}
		}
		r_text += "]";
		if (shared) {
			stack.erase(array.id());
		}
		return true;
	}
	error = vformat("'%s' has the type %s, which can't be written as TOML.", p_where, Variant::get_type_name(p_value.get_type()));
	return false;
}

bool TOMLWriter::write_table(const Dictionary &p_table, const String &p_path, String &r_text) {
	const String where = p_path.is_empty() ? String("root") : p_path;
	if (!enter(p_table.id(), where)) {
		return false;
	}
	// Plain keys first: TOML does not allow keys of a table after the header of one of its sub-tables.
	for (const KeyValue<Variant, Variant> &kv : p_table) {
		if (is_table(kv.value) || is_table_array(kv.value)) {
			continue;
		}
		r_text += key(kv.key) + " = ";
		if (!write_value(kv.value, p_path.is_empty() ? String(kv.key) : p_path + "." + String(kv.key), r_text)) {
			return false;
		}
		r_text += "\n";
	}
	for (const KeyValue<Variant, Variant> &kv : p_table) {
		const String path = p_path.is_empty() ? key(kv.key) : p_path + "." + key(kv.key);
		if (is_table(kv.value)) {
			if (!r_text.is_empty()) {
				r_text += "\n";
			}
			r_text += "[" + path + "]\n";
			if (!write_table(kv.value, path, r_text)) {
				return false;
			}
		} else if (is_table_array(kv.value)) {
			const Array array = kv.value;
			if (!enter(array.id(), path)) {
				return false;
			}
			for (int i = 0; i < array.size(); i++) {
				if (!r_text.is_empty()) {
					r_text += "\n";
				}
				r_text += "[[" + path + "]]\n";
				if (!write_table(array[i], path, r_text)) {
					return false;
				}
			}
			stack.erase(array.id());
		}
	}
	stack.erase(p_table.id());
	return true;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// TOML.

Error TOML::parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line, int &r_column) {
	TOMLReader reader;
	if (!reader.parse(p_text)) {
		r_data = Dictionary();
		r_error = reader.error;
		r_line = reader.error_line;
		r_column = reader.error_column;
		return ERR_PARSE_ERROR;
	}
	r_data = reader.root;
	r_error = String();
	r_line = 0;
	r_column = 0;
	return OK;
}

Error TOML::write_text(const Dictionary &p_data, String &r_text, String &r_error) {
	TOMLWriter writer;
	String text;
	if (!writer.write_table(p_data, String(), text)) {
		r_error = writer.error;
		return ERR_INVALID_DATA;
	}
	r_text = text;
	return OK;
}

String TOML::get_marker_type(const String &p_text) {
	if (!p_text.contains(TOML_KEY_TYPE)) {
		return String(); // Fast path for plain TOML files.
	}
	Dictionary tree;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, tree, error, line, column) != OK) {
		return String();
	}
	const Variant type = tree.get(TOML_KEY_TYPE, Variant());
	if (type.get_type() != Variant::STRING || !ResourceTextSerializer::is_resource_type(type)) {
		return String();
	}
	return type;
}

Error TOML::write_resource(const Ref<Resource> &p_resource, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error) {
	Dictionary tree;
	const Error err = ResourceTextSerializer::encode_resource(p_resource, p_options, true, tree, r_error);
	if (err != OK) {
		return err;
	}
	return write_text(tree, r_text, r_error);
}

Error TOML::parse_resource(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	Dictionary tree;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, tree, error, line, column) != OK) {
		r_error = vformat("TOML parse error on line %d, column %d: %s", line, column, error);
		// Without a type hint and a type marker the file may belong to another loader.
		return (hint.is_empty() && !p_text.contains(TOML_KEY_TYPE)) ? ERR_FILE_UNRECOGNIZED : ERR_PARSE_ERROR;
	}
	if (hint.is_empty()) {
		const Variant type = tree.get(TOML_KEY_TYPE, Variant());
		if (type.get_type() != Variant::STRING || !ResourceTextSerializer::is_resource_type(type)) {
			r_error = vformat("The TOML has no key \"%s\" with a Resource type.", TOML_KEY_TYPE);
			return ERR_FILE_UNRECOGNIZED;
		}
	}
	return ResourceTextSerializer::decode_resource(tree, hint, r_resource, r_error);
}

Error TOML::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	Dictionary parsed;
	const Error err = parse_text(p_text, parsed, r_error, r_line, r_column);
	r_data = parsed;
	return err;
}

Error TOML::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	return write_text(p_data, r_text, r_error);
}

Vector<String> TOML::split_table_path(const String &p_path) {
	Vector<String> parts;
	if (p_path.is_empty()) {
		return parts;
	}
	String current;
	for (int i = 0; i < p_path.length(); i++) {
		const char32_t c = p_path[i];
		if (c == '\\' && i + 1 < p_path.length()) {
			current += p_path[i + 1];
			i++;
		} else if (c == '.') {
			parts.push_back(current);
			current = String();
		} else {
			current += c;
		}
	}
	parts.push_back(current);
	return parts;
}

String TOML::escape_table_name(const String &p_name) {
	return p_name.replace("\\", "\\\\").replace(".", "\\.");
}

bool TOML::_resolve_table(Dictionary p_root, const String &p_path, bool p_create, Dictionary &r_table) {
	Dictionary table = p_root;
	for (const String &part : split_table_path(p_path)) {
		Variant next = table.get(part, Variant());
		if (next.get_type() != Variant::DICTIONARY) {
			if (!p_create || next.get_type() != Variant::NIL) {
				return false;
			}
			next = Dictionary();
			table[part] = next;
		}
		table = next;
	}
	r_table = table;
	return true;
}

void TOML::_collect_tables(const Dictionary &p_table, const String &p_prefix, PackedStringArray &r_paths) {
	for (const KeyValue<Variant, Variant> &kv : p_table) {
		if (kv.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const String path = p_prefix.is_empty() ? escape_table_name(kv.key) : p_prefix + "." + escape_table_name(kv.key);
		r_paths.push_back(path);
		_collect_tables(kv.value, path, r_paths);
	}
}

void TOML::set_value(const String &p_table_path, const String &p_key, const Variant &p_value) {
	Dictionary table;
	if (p_value.get_type() == Variant::NIL) {
		// TOML has no null: null erases the key.
		if (_resolve_table(get_data(), p_table_path, false, table) && table.erase(p_key)) {
			_mark_data_changed();
		}
		return;
	}
	ERR_FAIL_COND_MSG(!_resolve_table(get_data(), p_table_path, true, table), vformat("\"%s\" is not a table path: a part of it is a value.", p_table_path));
	table[p_key] = p_value;
	_mark_data_changed();
}

Variant TOML::get_value(const String &p_table_path, const String &p_key, const Variant &p_default) const {
	Dictionary table;
	if (!_resolve_table(get_data(), p_table_path, false, table)) {
		return p_default;
	}
	return table.get(p_key, p_default);
}

bool TOML::has_table(const String &p_table_path) const {
	Dictionary table;
	return _resolve_table(get_data(), p_table_path, false, table);
}

bool TOML::has_table_key(const String &p_table_path, const String &p_key) const {
	Dictionary table;
	return _resolve_table(get_data(), p_table_path, false, table) && table.has(p_key);
}

PackedStringArray TOML::get_tables() const {
	PackedStringArray paths;
	_collect_tables(get_data(), String(), paths);
	return paths;
}

PackedStringArray TOML::get_table_keys(const String &p_table_path) const {
	PackedStringArray keys;
	Dictionary table;
	ERR_FAIL_COND_V_MSG(!_resolve_table(get_data(), p_table_path, false, table), keys, vformat("Cannot get keys from nonexistent table \"%s\".", p_table_path));
	for (const KeyValue<Variant, Variant> &kv : table) {
		keys.push_back(kv.key);
	}
	return keys;
}

void TOML::erase_table(const String &p_table_path) {
	const Vector<String> parts = split_table_path(p_table_path);
	ERR_FAIL_COND_MSG(parts.is_empty(), "The root table can't be erased; use clear() instead.");
	String parent_path;
	for (int i = 0; i < parts.size() - 1; i++) {
		parent_path += (i == 0 ? "" : ".") + escape_table_name(parts[i]);
	}
	Dictionary parent;
	const String &name = parts[parts.size() - 1];
	ERR_FAIL_COND_MSG(!_resolve_table(get_data(), parent_path, false, parent) || parent.get(name, Variant()).get_type() != Variant::DICTIONARY, vformat("Cannot erase nonexistent table \"%s\".", p_table_path));
	parent.erase(name);
	_mark_data_changed();
}

void TOML::erase_table_key(const String &p_table_path, const String &p_key) {
	Dictionary table;
	ERR_FAIL_COND_MSG(!_resolve_table(get_data(), p_table_path, false, table) || !table.has(p_key), vformat("Cannot erase nonexistent key \"%s\" from table \"%s\".", p_key, p_table_path));
	table.erase(p_key);
	_mark_data_changed();
}

TOML::TOML() {
	_get_data_ref() = Dictionary();
}

Dictionary TOML::parse_string(const String &p_text) {
	Dictionary result;
	String error;
	int line = 0;
	int column = 0;
	if (parse_text(p_text, result, error, line, column) != OK) {
		ERR_PRINT(vformat("TOML parse error on line %d, column %d: %s", line, column, error));
		return Dictionary();
	}
	return result;
}

String TOML::stringify(const Dictionary &p_data) {
	String text;
	String error;
	if (write_text(p_data, text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

String TOML::serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	String text;
	String error;
	if (write_resource(p_resource, ResourceTextSerializer::get_options(p_options), text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

Ref<Resource> TOML::deserialize(const String &p_text, const StringName &p_type_hint, const Ref<SerializerOptions> &p_options) {
	// Reading has no options yet; the parameter matches ResourceTextSerializer.deserialize().
	Ref<Resource> resource;
	String error;
	if (parse_resource(p_text, p_type_hint, resource, error) != OK) {
		ERR_PRINT(error);
		return Ref<Resource>();
	}
	return resource;
}

void TOML::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "table_path", "key", "value"), &TOML::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "table_path", "key", "default"), &TOML::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_table", "table_path"), &TOML::has_table);
	ClassDB::bind_method(D_METHOD("has_table_key", "table_path", "key"), &TOML::has_table_key);
	ClassDB::bind_method(D_METHOD("get_tables"), &TOML::get_tables);
	ClassDB::bind_method(D_METHOD("get_table_keys", "table_path"), &TOML::get_table_keys);
	ClassDB::bind_method(D_METHOD("erase_table", "table_path"), &TOML::erase_table);
	ClassDB::bind_method(D_METHOD("erase_table_key", "table_path", "key"), &TOML::erase_table_key);

	ClassDB::bind_static_method("TOML", D_METHOD("parse_string", "text"), &TOML::parse_string);
	ClassDB::bind_static_method("TOML", D_METHOD("stringify", "data"), &TOML::stringify);
	ClassDB::bind_static_method("TOML", D_METHOD("serialize", "resource", "options"), &TOML::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("TOML", D_METHOD("deserialize", "text", "type_hint", "options"), &TOML::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
