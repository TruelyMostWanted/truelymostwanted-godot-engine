/**************************************************************************/
/*  text_insertion_template.cpp                                           */
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

#include "text_insertion_template.h"

#include "core/object/class_db.h"

TextInsertionTemplate::ModifierKind TextInsertionTemplate::get_builtin_modifier(const StringName &p_name) {
	if (p_name == "t") {
		return MODIFIER_T;
	} else if (p_name == "upper") {
		return MODIFIER_UPPER;
	} else if (p_name == "lower") {
		return MODIFIER_LOWER;
	} else if (p_name == "capitalize") {
		return MODIFIER_CAPITALIZE;
	} else if (p_name == "number") {
		return MODIFIER_NUMBER;
	} else if (p_name == "f") {
		return MODIFIER_F;
	}
	return MODIFIER_CUSTOM;
}

void TextInsertionTemplate::_add_literal(String &r_literal) {
	if (r_literal.is_empty()) {
		return;
	}
	Segment segment;
	segment.text = r_literal;
	literal_length += r_literal.length();
	segments.push_back(segment);
	r_literal = String();
}

void TextInsertionTemplate::_parse(const String &p_text) {
	source = p_text;
	const char32_t *src = p_text.ptr();
	const int length = p_text.length();
	String literal;
	int i = 0;
	while (i < length) {
		const char32_t c = src[i];
		// `{{` and `}}` are a literal brace.
		if ((c == U'{' || c == U'}') && i + 1 < length && src[i + 1] == c) {
			literal += c;
			i += 2;
			continue;
		}
		if (c == U'{') {
			int end = i + 1;
			while (end < length && src[end] != U'}' && src[end] != U'{') {
				end++;
			}
			const Vector<String> parts = end < length && src[end] == U'}' ? p_text.substr(i + 1, end - i - 1).split(":") : Vector<String>();
			if (!parts.is_empty() && !parts[0].is_empty()) {
				_add_literal(literal);
				Segment segment;
				segment.text = p_text.substr(i, end - i + 1);
				segment.name = parts[0];
				segment.modifier_begin = modifiers.size();
				segment.modifier_count = (uint32_t)(parts.size() - 1);
				for (int part = 1; part < parts.size(); part++) {
					Modifier modifier;
					modifier.name = parts[part];
					modifier.kind = get_builtin_modifier(modifier.name);
					modifiers.push_back(modifier);
				}
				if (!names.has(segment.name)) {
					names.push_back(segment.name);
				}
				segments.push_back(segment);
				i = end + 1;
				continue;
			}
		}
		// A brace without a valid placeholder stays as written.
		literal += c;
		i++;
	}
	_add_literal(literal);
}

TypedArray<StringName> TextInsertionTemplate::get_names() const {
	TypedArray<StringName> result;
	for (const StringName &entry : names) {
		result.push_back(entry);
	}
	return result;
}

bool TextInsertionTemplate::has_name(const StringName &p_name) const {
	return names.has(p_name);
}

void TextInsertionTemplate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_source"), &TextInsertionTemplate::get_source);
	ClassDB::bind_method(D_METHOD("get_names"), &TextInsertionTemplate::get_names);
	ClassDB::bind_method(D_METHOD("has_name", "name"), &TextInsertionTemplate::has_name);
}
