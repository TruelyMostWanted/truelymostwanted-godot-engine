/**************************************************************************/
/*  toml_syntax_highlighter.cpp                                           */
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

#include "toml_syntax_highlighter.h"

#include "editor/settings/editor_settings.h"

void EditorTOMLSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	key_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	section_color = EDITOR_GET("text_editor/theme/highlighting/function_color");

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_function_color(text_color);
	highlighter->set_member_variable_color(text_color);

	// Multi-line strings first; basic and literal strings end with the line.
	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"\"\"", "\"\"\"", string_color);
	highlighter->add_color_region("'''", "'''", string_color);
	highlighter->add_color_region("\"", "\"", string_color, true);
	highlighter->add_color_region("'", "'", string_color, true);

	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	highlighter->add_keyword_color("true", keyword_color);
	highlighter->add_keyword_color("false", keyword_color);
	highlighter->add_keyword_color("inf", keyword_color);
	highlighter->add_keyword_color("nan", keyword_color);

	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("#", "", comment_color, true);
}

int EditorTOMLSyntaxHighlighter::_find_key(const String &p_line, int &r_start, bool &r_section) const {
	int start = 0;
	while (start < p_line.length() && (p_line[start] == ' ' || p_line[start] == '\t')) {
		start++;
	}
	if (start >= p_line.length() || p_line[start] == '#') {
		return -1;
	}
	r_start = start;
	// `[table]` and `[[array.of.tables]]` at the start of a line; otherwise `key = value`.
	r_section = p_line[start] == '[';
	if (r_section) {
		const String close = p_line.substr(start, 2) == "[[" ? "]]" : "]";
		const int end = p_line.find(close, start);
		return end < 0 ? -1 : end + close.length();
	}
	int end = p_line.find("=", start);
	if (end < 0) {
		return -1;
	}
	while (end > start && (p_line[end - 1] == ' ' || p_line[end - 1] == '\t')) {
		end--;
	}
	return end;
}

Ref<EditorSyntaxHighlighter> EditorTOMLSyntaxHighlighter::_create() const {
	Ref<EditorTOMLSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}
