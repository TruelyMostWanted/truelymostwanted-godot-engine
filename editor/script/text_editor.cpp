/**************************************************************************/
/*  text_editor.cpp                                                       */
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

#include "text_editor.h"

#include "core/io/json.h"
#include "core/io/text_data_format.h"
#include "core/object/callable_mp.h"
#include "core/templates/hash_set.h"
#include "editor/script/data_schema_lookup.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/scene_string_names.h"

void TextEditor::_validate_script() {
	TextEditorBase::_validate_script();

	// Live errors of data files (JSON, INI, CSV, XML, TOML, YAML): the parse error, or the issues of the
	// DataSchema whose `file_patterns` match the file (project setting `filesystem/data/schema_files`).
	Ref<JSON> json_file = edited_res;
	Ref<TextDataFormat> data_file = edited_res;
	if (json_file.is_null() && data_file.is_null()) {
		return;
	}

	CodeEdit *te = code_editor->get_text_editor();
	for (int line : issue_lines) {
		if (line >= 0 && line < te->get_line_count()) {
			te->set_line_background_color(line, Color(0, 0, 0, 0));
		}
	}
	issue_lines.clear();
	code_editor->set_error("");

	const String text = te->get_text();
	const int64_t mib = 1024 * 1024;
	if (text.length() > int64_t(EDITOR_GET("text_editor/data_files/live_parse_max_size_mb")) * mib) {
		return;
	}

	String error;
	int error_column = 0;
	Ref<DataValidation> validation;
	const Ref<DataSchema> schema = text.length() > int64_t(EDITOR_GET("text_editor/data_files/schema_validation_max_size_mb")) * mib ? Ref<DataSchema>() : EditorDataSchemaLookup::find_schema(edited_res->get_path());
	if (json_file.is_valid()) {
		if (json_file->parse(text, true) != OK) {
			error = json_file->get_error_message();
			issue_lines.push_back(json_file->get_error_line());
		} else if (schema.is_valid()) {
			validation = schema->validate(json_file->get_data(), validation_options);
			validation->resolve_lines(text);
		}
	} else if (data_file->parse(text, true) != OK) {
		error = data_file->get_error_message();
		issue_lines.push_back(MAX(data_file->get_error_line() - 1, 0));
		error_column = MAX(data_file->get_error_column() - 1, 0);
	} else if (schema.is_valid()) {
		validation = data_file->validate(schema, validation_options);
	}

	if (validation.is_valid() && !validation->is_valid()) {
		const int count = validation->get_issue_count();
		HashSet<int> marked;
		for (int i = 0; i < count; i++) {
			const int line = validation->get_issue_line(i) - 1;
			if (line >= 0 && line < te->get_line_count() && !marked.has(line)) {
				marked.insert(line);
				issue_lines.push_back(line);
			}
		}
		issue_lines.sort();
		const int first_line = validation->get_issue_line(0);
		const String first = first_line > 0 ? vformat(TTR("Line %d: %s"), first_line, validation->get_issue_message(0)) : validation->get_issue_message(0);
		error = count == 1 ? first : vformat(TTR("%s issues. %s"), count >= MAX_ISSUES ? itos(count) + "+" : itos(count), first);
	}
	if (error.is_empty()) {
		return;
	}

	code_editor->set_error(error.replace("[", "[lb]"));
	code_editor->set_error_pos(issue_lines.is_empty() ? 0 : issue_lines[0], error_column);
	const Color mark_color = EDITOR_GET("text_editor/theme/highlighting/mark_color");
	for (int line : issue_lines) {
		if (line >= 0 && line < te->get_line_count()) {
			te->set_line_background_color(line, mark_color);
		}
	}
}

void TextEditor::_caret_changed() {
	// A click on the status goes to the error position; moving it to the next issue below the caret lets
	// repeated clicks cycle through all marked lines.
	if (issue_lines.size() < 2) {
		return;
	}
	const int caret_line = code_editor->get_text_editor()->get_caret_line();
	int next = issue_lines[0];
	for (int line : issue_lines) {
		if (line > caret_line) {
			next = line;
			break;
		}
	}
	code_editor->set_error_pos(next, 0);
}

void TextEditor::apply_code() {
	Ref<TextFile> text_file = edited_res;
	if (text_file.is_valid()) {
		text_file->set_text(code_editor->get_text_editor()->get_text());
	}

	Ref<JSON> json_file = edited_res;
	if (json_file.is_valid()) {
		json_file->parse(code_editor->get_text_editor()->get_text(), true);
	}

	Ref<TextDataFormat> data_file = edited_res;
	if (data_file.is_valid()) {
		data_file->parse(code_editor->get_text_editor()->get_text(), true);
	}
	code_editor->get_text_editor()->get_syntax_highlighter()->update_cache();
}

bool TextEditor::_requires_space_indent() const {
	// YAML forbids tabs in indentation, so these files always indent with spaces, whatever the indent type
	// setting is (also when "Convert Indent on Save" runs).
	if (edited_res.is_null()) {
		return false;
	}
	const String extension = edited_res->get_path().get_extension().to_lower();
	return extension == "yaml" || extension == "yml" || edited_res->is_class("YAML");
}

void TextEditor::_apply_space_indent() {
	if (_requires_space_indent()) {
		code_editor->set_indent_using_spaces(true);
	}
}

void TextEditor::set_edited_resource(const Ref<Resource> &p_res) {
	TextEditorBase::set_edited_resource(p_res);
	_apply_space_indent();
	// CodeTextEditor applies the indent type setting again on each theme change: apply the override after it.
	const Callable apply = callable_mp(this, &TextEditor::_apply_space_indent);
	if (!code_editor->is_connected(SceneStringName(theme_changed), apply)) {
		code_editor->connect(SceneStringName(theme_changed), apply, CONNECT_DEFERRED);
	}
}

void TextEditor::update_settings() {
	TextEditorBase::update_settings();
	_apply_space_indent();
}

void TextEditor::convert_indent() {
	_apply_space_indent();
	TextEditorBase::convert_indent();
}

ScriptEditorBase *TextEditor::create_editor(const Ref<Resource> &p_resource) {
	if (Object::cast_to<TextFile>(*p_resource) || Object::cast_to<JSON>(*p_resource) || Object::cast_to<TextDataFormat>(*p_resource)) {
		return memnew(TextEditor);
	}
	return nullptr;
}

void TextEditor::register_editor() {
	ScriptEditor::register_create_script_editor_function(create_editor);
}

TextEditor::TextEditor() {
	add_child(code_editor);
	code_editor->get_text_editor()->connect("caret_changed", callable_mp(this, &TextEditor::_caret_changed));

	validation_options.instantiate();
	validation_options->set_keep_positions(true);
	validation_options->set_max_issues(MAX_ISSUES);
}
