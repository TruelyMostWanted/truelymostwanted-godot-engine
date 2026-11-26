/**************************************************************************/
/*  syntax_highlighters.cpp                                               */
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

#include "syntax_highlighters.h"

#include "core/config/project_settings.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/text_data_format.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/text_edit.h"
#include "scene/resources/shader.h"
#include "servers/rendering/shader_preprocessor.h"
#include "servers/rendering/shader_types.h"

String EditorSyntaxHighlighter::_get_name() const {
	String ret = "Unnamed";
	GDVIRTUAL_CALL(_get_name, ret);
	return ret;
}

PackedStringArray EditorSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_supported_languages, ret);
	return ret;
}

Ref<EditorSyntaxHighlighter> EditorSyntaxHighlighter::_create() const {
	Ref<EditorSyntaxHighlighter> syntax_highlighter;
	if (GDVIRTUAL_IS_OVERRIDDEN(_create)) {
		GDVIRTUAL_CALL(_create, syntax_highlighter);
	} else {
		syntax_highlighter.instantiate();
		if (get_script_instance()) {
			syntax_highlighter->set_script(get_script_instance()->get_script());
		}
	}
	return syntax_highlighter;
}

void EditorSyntaxHighlighter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_edited_resource"), &EditorSyntaxHighlighter::_get_edited_resource);

	GDVIRTUAL_BIND(_get_name)
	GDVIRTUAL_BIND(_get_supported_languages)
	GDVIRTUAL_BIND(_create)
}

////

void EditorStandardSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	/* Engine types. */
	const Color type_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	LocalVector<StringName> types;
	ClassDB::get_class_list(types);
	for (const StringName &type : types) {
		highlighter->add_keyword_color(type, type_color);
	}

	/* User types. */
	const Color usertype_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");
	LocalVector<StringName> global_classes;
	ScriptServer::get_global_class_list(global_classes);
	for (const StringName &class_name : global_classes) {
		highlighter->add_keyword_color(class_name, usertype_color);
	}

	/* Autoloads. */
	HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads(ProjectSettings::get_singleton()->get_autoload_list());
	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
		const ProjectSettings::AutoloadInfo &info = E.value;
		if (info.is_singleton) {
			highlighter->add_keyword_color(info.name, usertype_color);
		}
	}

	const ScriptLanguage *scr_lang = script_language;
	StringName instance_base;

	if (scr_lang == nullptr) {
		const Ref<Script> scr = _get_edited_resource();
		if (scr.is_valid()) {
			scr_lang = scr->get_language();
			instance_base = scr->get_instance_base_type();
		}
	}

	/* Variant types. */
	const Color basetype_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
	for (int type = 0; type < Variant::Type::VARIANT_MAX; type++) {
		if (type != Variant::Type::NIL && type != Variant::Type::OBJECT) {
			highlighter->add_keyword_color(Variant::get_type_name((Variant::Type)type), basetype_color);
		}
	}

	if (scr_lang != nullptr) {
		/* Reserved words. */
		const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
		const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");
		for (const String &keyword : scr_lang->get_reserved_words()) {
			if (scr_lang->is_control_flow_keyword(keyword)) {
				highlighter->add_keyword_color(keyword, control_flow_keyword_color);
			} else {
				highlighter->add_keyword_color(keyword, keyword_color);
			}
		}

		/* Member types. */
		const Color member_variable_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
		if (instance_base != StringName()) {
			List<PropertyInfo> plist;
			ClassDB::get_property_list(instance_base, &plist);
			for (const PropertyInfo &E : plist) {
				String prop_name = E.name;
				if (E.usage & PROPERTY_USAGE_CATEGORY || E.usage & PROPERTY_USAGE_GROUP || E.usage & PROPERTY_USAGE_SUBGROUP) {
					continue;
				}
				if (prop_name.contains_char('/')) {
					continue;
				}
				highlighter->add_member_keyword_color(prop_name, member_variable_color);
			}

			List<String> clist;
			ClassDB::get_integer_constant_list(instance_base, &clist);
			for (const String &E : clist) {
				highlighter->add_member_keyword_color(E, member_variable_color);
			}
		}

		/* Comments */
		const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
		for (const String &comment : scr_lang->get_comment_delimiters()) {
			String beg = comment.get_slicec(' ', 0);
			String end = comment.get_slice_count(" ") > 1 ? comment.get_slicec(' ', 1) : String();
			highlighter->add_color_region(beg, end, comment_color, end.is_empty());
		}

		/* Doc comments */
		const Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");
		for (const String &doc_comment : scr_lang->get_doc_comment_delimiters()) {
			String beg = doc_comment.get_slicec(' ', 0);
			String end = doc_comment.get_slice_count(" ") > 1 ? doc_comment.get_slicec(' ', 1) : String();
			highlighter->add_color_region(beg, end, doc_comment_color, end.is_empty());
		}

		/* Strings */
		const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
		for (const String &string : scr_lang->get_string_delimiters()) {
			String beg = string.get_slicec(' ', 0);
			String end = string.get_slice_count(" ") > 1 ? string.get_slicec(' ', 1) : String();
			highlighter->add_color_region(beg, end, string_color, end.is_empty());
		}
	}
}

Ref<EditorSyntaxHighlighter> EditorStandardSyntaxHighlighter::_create() const {
	Ref<EditorStandardSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

Ref<EditorSyntaxHighlighter> EditorPlainTextSyntaxHighlighter::_create() const {
	Ref<EditorPlainTextSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorJSONSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));

	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color);
}

Ref<EditorSyntaxHighlighter> EditorJSONSyntaxHighlighter::_create() const {
	Ref<EditorJSONSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorMarkdownSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	// Disable automatic symbolic highlights, as these don't make sense for prose.
	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));

	// Headings (any level).
	const Color function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	highlighter->add_color_region("#", "", function_color);

	// Bold.
	highlighter->add_color_region("**", "**", function_color);
	// `__bold__` syntax is not supported as color regions must begin with a symbol,
	// not a character that is valid in an identifier.

	// Code (both inline code and triple-backticks code blocks).
	const Color code_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	highlighter->add_color_region("`", "`", code_color);

	// Link (both references and inline links with URLs). The URL is not highlighted.
	const Color link_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	highlighter->add_color_region("[", "]", link_color);

	// Quote.
	const Color quote_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region(">", "", quote_color, true);

	// HTML comment, which is also supported in Markdown.
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("<!--", "-->", comment_color);
}

Ref<EditorSyntaxHighlighter> EditorMarkdownSyntaxHighlighter::_create() const {
	Ref<EditorMarkdownSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorConfigFileSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	// Assume that all function-style syntax is for types such as `Vector2()` and `PackedStringArray()`.
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/base_type_color"));

	// Disable member variable highlighting as it's not relevant for ConfigFile.
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));

	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color);

	// FIXME: Sections in ConfigFile must be at the beginning of a line. Otherwise, it can be an array within a line.
	const Color function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	highlighter->add_color_region("[", "]", function_color);

	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	highlighter->add_keyword_color("true", keyword_color);
	highlighter->add_keyword_color("false", keyword_color);
	highlighter->add_keyword_color("null", keyword_color);
	highlighter->add_keyword_color("ExtResource", keyword_color);
	highlighter->add_keyword_color("SubResource", keyword_color);

	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region(";", "", comment_color);
}

Ref<EditorSyntaxHighlighter> EditorConfigFileSyntaxHighlighter::_create() const {
	Ref<EditorConfigFileSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorLineStateSyntaxHighlighter::_lines_edited_from(int p_from_line, int p_to_line) {
	const int keep = MAX(0, MIN(p_from_line, p_to_line));
	if (keep < (int)line_end_states.size()) {
		line_end_states.resize(keep);
	}
}

void EditorLineStateSyntaxHighlighter::_set_color(Dictionary &r_colors, int p_column, const Color &p_color) {
	Dictionary entry;
	entry["color"] = p_color;
	r_colors[p_column] = entry;
}

void EditorLineStateSyntaxHighlighter::_update_cache() {
	line_end_states.clear();
	const Callable edited = callable_mp(this, &EditorLineStateSyntaxHighlighter::_lines_edited_from);
	if (!text_edit->is_connected("lines_edited_from", edited)) {
		text_edit->connect("lines_edited_from", edited);
	}
}

Dictionary EditorLineStateSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	while ((int)line_end_states.size() < p_line) {
		const int line = line_end_states.size();
		Dictionary unused;
		line_end_states.push_back(_highlight_line(text_edit->get_line(line), line > 0 ? line_end_states[line - 1] : 0, unused));
	}
	Dictionary colors;
	const int end_state = _highlight_line(text_edit->get_line(p_line), p_line > 0 ? line_end_states[p_line - 1] : 0, colors);
	if ((int)line_end_states.size() == p_line) {
		line_end_states.push_back(end_state);
	}
	return colors;
}

////

void EditorCSVSyntaxHighlighter::_update_cache() {
	EditorLineStateSyntaxHighlighter::_update_cache();

	// Columns cycle through the highlighting colors, so the column of a cell is visible at a glance.
	const char *color_names[COLUMN_COLOR_COUNT] = { "text_color", "keyword_color", "function_color", "member_variable_color", "number_color", "base_type_color" };
	for (int i = 0; i < COLUMN_COLOR_COUNT; i++) {
		column_colors[i] = EDITOR_GET(String("text_editor/theme/highlighting/") + color_names[i]);
	}
	symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");

	// The delimiter of the edited file (its `options`, or the project default).
	Ref<TextDataFormat> data_file = _get_edited_resource();
	delimiter = ResourceTextSerializer::get_csv_delimiter(ResourceTextSerializer::get_options(data_file.is_valid() ? data_file->get_options() : Ref<SerializerOptions>()));
}

int EditorCSVSyntaxHighlighter::_highlight_line(const String &p_line, int p_state, Dictionary &r_colors) const {
	// State: column index * 2 + 1 inside a quoted cell that continues on the next line; a line outside
	// quotes starts a new record.
	bool in_quotes = p_state & 1;
	int column = in_quotes ? p_state >> 1 : 0;
	_set_color(r_colors, 0, column_colors[column % COLUMN_COLOR_COUNT]);
	const char32_t *chars = p_line.ptr();
	for (int i = 0; i < p_line.length(); i++) {
		if (chars[i] == '"') {
			in_quotes = !in_quotes; // An escaped quote ("") toggles twice.
		} else if (chars[i] == delimiter && !in_quotes) {
			column++;
			_set_color(r_colors, i, symbol_color);
			_set_color(r_colors, i + 1, column_colors[column % COLUMN_COLOR_COUNT]);
		}
	}
	return in_quotes ? column * 2 + 1 : 0;
}

Ref<EditorSyntaxHighlighter> EditorCSVSyntaxHighlighter::_create() const {
	Ref<EditorCSVSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorXMLSyntaxHighlighter::_update_cache() {
	EditorLineStateSyntaxHighlighter::_update_cache();

	text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");
	tag_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	attribute_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
}

static bool _xml_is_name_char(char32_t p_char) {
	return is_unicode_identifier_continue(p_char) || p_char == ':' || p_char == '-' || p_char == '.';
}

int EditorXMLSyntaxHighlighter::_highlight_line(const String &p_line, int p_state, Dictionary &r_colors) const {
	int state = p_state;
	const Color state_colors[] = { text_color, text_color, string_color, string_color, comment_color, string_color, keyword_color };
	_set_color(r_colors, 0, state_colors[state]);
	const int length = p_line.length();
	for (int i = 0; i < length; i++) {
		const char32_t c = p_line[i];
		switch (state) {
			case STATE_TEXT: {
				if (c != '<') {
					break;
				}
				if (p_line.substr(i, 4) == "<!--") {
					state = STATE_COMMENT;
					_set_color(r_colors, i, comment_color);
					i += 3;
				} else if (p_line.substr(i, 9) == "<![CDATA[") {
					state = STATE_CDATA;
					_set_color(r_colors, i, string_color);
					i += 8;
				} else if (i + 1 < length && (p_line[i + 1] == '?' || p_line[i + 1] == '!')) {
					state = STATE_DECLARATION;
					_set_color(r_colors, i, keyword_color);
				} else {
					// `<name` or `</name`.
					state = STATE_TAG;
					_set_color(r_colors, i, symbol_color);
					int end = i + 1;
					if (end < length && p_line[end] == '/') {
						end++;
					}
					_set_color(r_colors, end, tag_color);
					while (end < length && _xml_is_name_char(p_line[end])) {
						end++;
					}
					i = end - 1;
				}
			} break;
			case STATE_TAG: {
				if (c == '"' || c == '\'') {
					state = c == '"' ? STATE_VALUE_DOUBLE_QUOTED : STATE_VALUE_SINGLE_QUOTED;
					_set_color(r_colors, i, string_color);
				} else if (c == '>') {
					state = STATE_TEXT;
					_set_color(r_colors, i, symbol_color);
					_set_color(r_colors, i + 1, text_color);
				} else if (c == '/' || c == '=') {
					_set_color(r_colors, i, symbol_color);
				} else if (_xml_is_name_char(c) && (i == 0 || !_xml_is_name_char(p_line[i - 1]))) {
					_set_color(r_colors, i, attribute_color);
				}
			} break;
			case STATE_VALUE_DOUBLE_QUOTED:
			case STATE_VALUE_SINGLE_QUOTED: {
				if (c == (state == STATE_VALUE_DOUBLE_QUOTED ? '"' : '\'')) {
					state = STATE_TAG;
					_set_color(r_colors, i + 1, text_color);
				}
			} break;
			case STATE_COMMENT:
			case STATE_CDATA: {
				if (p_line.substr(i, 3) == (state == STATE_COMMENT ? "-->" : "]]>")) {
					state = STATE_TEXT;
					i += 2;
					_set_color(r_colors, i + 1, text_color);
				}
			} break;
			case STATE_DECLARATION: {
				if (c == '>') {
					state = STATE_TEXT;
					_set_color(r_colors, i + 1, text_color);
				}
			} break;
		}
	}
	return state;
}

Ref<EditorSyntaxHighlighter> EditorXMLSyntaxHighlighter::_create() const {
	Ref<EditorXMLSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

Dictionary EditorKeyValueSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	const Dictionary colors = highlighter->get_line_syntax_highlighting(p_line);
	int start = 0;
	bool section = false;
	const int end = _find_key(text_edit->get_line(p_line), start, section);
	if (end <= start) {
		return colors;
	}
	// Rebuilt in column order: the colors before the key, the key, then the color in effect at its end.
	Dictionary result;
	Variant after;
	for (const Variant *key = colors.next(nullptr); key; key = colors.next(key)) {
		const int column = *key;
		if (column <= end) {
			after = colors[*key];
		}
		if (column < start) {
			result[*key] = colors[*key];
		}
	}
	Dictionary key_entry;
	key_entry["color"] = section ? section_color : key_color;
	result[start] = key_entry;
	if (after.get_type() == Variant::NIL) {
		Dictionary text_entry;
		text_entry["color"] = text_color;
		after = text_entry;
	}
	result[end] = after;
	for (const Variant *key = colors.next(nullptr); key; key = colors.next(key)) {
		if (int(*key) > end) {
			result[*key] = colors[*key];
		}
	}
	return result;
}

////

void GDShaderSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");

	List<String> keywords;
	ShaderLanguage::get_keyword_list(&keywords);

	for (const String &E : keywords) {
		if (ShaderLanguage::is_control_flow_keyword(E)) {
			highlighter->add_keyword_color(E, control_flow_keyword_color);
		} else {
			highlighter->add_keyword_color(E, keyword_color);
		}
	}

	List<String> pp_keywords;
	ShaderPreprocessor::get_keyword_list(&pp_keywords, false);

	for (const String &E : pp_keywords) {
		highlighter->add_keyword_color(E, control_flow_keyword_color);
	}

	// Colorize comments.
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("/*", "*/", comment_color, false);
	highlighter->add_color_region("//", "", comment_color, true);

	const Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");
	highlighter->add_color_region("/**", "*/", doc_comment_color, false);
	// "/**/" will be treated as the start of the "/**" region, this line is guaranteed to end the color_region.
	highlighter->add_color_region("/**/", "", comment_color, true);

	// Disabled preprocessor branches use translucent text color to be easier to distinguish from comments.
	set_disabled_branch_color(Color(EDITOR_GET("text_editor/theme/highlighting/text_color")) * Color(1, 1, 1, 0.5));

	// Colorize preprocessor include strings.
	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color, false);
	highlighter->set_uint_suffix_enabled(true);

	// Colorize built-ins like `COLOR` differently to make them easier
	// to distinguish from keywords at a quick glance.

	Ref<ShaderInclude> shader_inc = _get_edited_resource();
	Ref<Shader> shader = _get_edited_resource();

	LocalVector<Vector<ShaderLanguage::ModeInfo>> modes_list;
	LocalVector<String> built_ins;

	ShaderTypes *st = ShaderTypes::get_singleton();
	if (shader_inc.is_valid() || shader.is_valid()) {
		for (int i = 0; i < RSE::SHADER_MAX; i++) {
			if (shader_inc.is_null() && shader->get_mode() != i) {
				continue;
			}

			for (const KeyValue<StringName, ShaderLanguage::FunctionInfo> &E : st->get_functions(RSE::ShaderMode(i))) {
				for (const KeyValue<StringName, ShaderLanguage::BuiltInInfo> &F : E.value.built_ins) {
					built_ins.push_back(F.key);
				}
			}
			modes_list.push_back(st->get_modes(RSE::ShaderMode(i)));
			modes_list.push_back(st->get_stencil_modes(RSE::ShaderMode(i)));
		}
	}

	for (const Vector<ShaderLanguage::ModeInfo> &modes : modes_list) {
		for (const ShaderLanguage::ModeInfo &mode_info : modes) {
			if (!mode_info.options.is_empty()) {
				for (const StringName &option : mode_info.options) {
					built_ins.push_back(String(mode_info.name) + "_" + String(option));
				}
			} else {
				built_ins.push_back(String(mode_info.name));
			}
		}
	}

	const Color user_type_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");

	for (const String &E : built_ins) {
		highlighter->add_keyword_color(E, user_type_color);
	}
}

Dictionary GDShaderSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;

	for (const Point2i &region : disabled_branch_regions) {
		if (p_line >= region.x && p_line <= region.y) {
			// When "color_regions[0].p_start_key.length() > 2",
			// disabled_branch_region causes color_region to break.
			// This should be seen as a temporary solution.
			highlighter->get_line_syntax_highlighting(p_line);

			Dictionary highlighter_info;
			highlighter_info["color"] = disabled_branch_color;

			color_map[0] = highlighter_info;
			return color_map;
		}
	}

	return highlighter->get_line_syntax_highlighting(p_line);
}

void GDShaderSyntaxHighlighter::add_disabled_branch_region(const Point2i &p_region) {
	ERR_FAIL_COND(p_region.x < 0);
	ERR_FAIL_COND(p_region.y < 0);

	for (const Point2i &disabled_branch_region : disabled_branch_regions) {
		ERR_FAIL_COND_MSG(disabled_branch_region.x == p_region.x, "Branch region with a start line '" + itos(p_region.x) + "' already exists.");
	}

	Point2i disabled_branch_region;
	disabled_branch_region.x = p_region.x;
	disabled_branch_region.y = p_region.y;
	disabled_branch_regions.push_back(disabled_branch_region);

	clear_highlighting_cache();
}

void GDShaderSyntaxHighlighter::clear_disabled_branch_regions() {
	disabled_branch_regions.clear();
	clear_highlighting_cache();
}

void GDShaderSyntaxHighlighter::set_disabled_branch_color(const Color &p_color) {
	disabled_branch_color = p_color;
	clear_highlighting_cache();
}

Ref<EditorSyntaxHighlighter> GDShaderSyntaxHighlighter::_create() const {
	Ref<GDShaderSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}
