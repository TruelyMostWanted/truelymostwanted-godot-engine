/**************************************************************************/
/*  yaml_syntax_highlighter.cpp                                           */
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

#include "yaml_syntax_highlighter.h"

#include "editor/settings/editor_settings.h"

void EditorYAMLSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	key_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	section_color = key_color;

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_function_color(text_color);
	highlighter->set_member_variable_color(text_color);

	// Quoted strings end with the line, so an apostrophe in a plain value (`title: Don't`) colors one line at most.
	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color, true);
	highlighter->add_color_region("'", "'", string_color, true);

	// YAML 1.2 core schema.
	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	for (const char *keyword : { "true", "True", "TRUE", "false", "False", "FALSE", "null", "Null", "NULL" }) {
		highlighter->add_keyword_color(keyword, keyword_color);
	}

	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("#", "", comment_color, true);
}

int EditorYAMLSyntaxHighlighter::_find_key(const String &p_line, int &r_start, bool &r_section) const {
	// Indentation and sequence items (`- `) before the key.
	const int length = p_line.length();
	int start = 0;
	while (start < length && (p_line[start] == ' ' || p_line[start] == '\t' || (p_line[start] == '-' && (start + 1 == length || p_line[start + 1] == ' ')))) {
		start++;
	}
	if (start >= length || p_line[start] == '#') {
		return -1;
	}
	// `key:` ends at the first colon followed by a space or the end of the line.
	for (int i = start; i < length; i++) {
		if (p_line[i] == '#' && p_line[i - 1] == ' ') {
			return -1;
		}
		if (p_line[i] == ':' && (i + 1 == length || p_line[i + 1] == ' ' || p_line[i + 1] == '\t')) {
			r_start = start;
			r_section = false;
			return i;
		}
	}
	return -1;
}

Ref<EditorSyntaxHighlighter> EditorYAMLSyntaxHighlighter::_create() const {
	Ref<EditorYAMLSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}
