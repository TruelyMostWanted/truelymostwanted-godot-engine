/**************************************************************************/
/*  syntax_highlighters.h                                                 */
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

#pragma once

#include "core/templates/local_vector.h"
#include "scene/resources/syntax_highlighter.h"

class EditorSyntaxHighlighter : public SyntaxHighlighter {
	GDCLASS(EditorSyntaxHighlighter, SyntaxHighlighter)

private:
	Ref<RefCounted> edited_resource;

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(String, _get_name)
	GDVIRTUAL0RC(PackedStringArray, _get_supported_languages)
	GDVIRTUAL0RC(Ref<EditorSyntaxHighlighter>, _create)

public:
	virtual String _get_name() const;
	virtual PackedStringArray _get_supported_languages() const;

	void _set_edited_resource(const Ref<Resource> &p_res) { edited_resource = p_res; }
	Ref<RefCounted> _get_edited_resource() { return edited_resource; }

	virtual Ref<EditorSyntaxHighlighter> _create() const;
};

class EditorStandardSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorStandardSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;
	ScriptLanguage *script_language = nullptr; // See GH-89610.

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual String _get_name() const override { return TTR("Standard"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	void _set_script_language(ScriptLanguage *p_script_language) { script_language = p_script_language; }

	EditorStandardSyntaxHighlighter() { highlighter.instantiate(); }
};

class EditorPlainTextSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorPlainTextSyntaxHighlighter, EditorSyntaxHighlighter)

public:
	virtual String _get_name() const override { return TTR("Plain Text"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;
};

class EditorJSONSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorJSONSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "json" }; }
	virtual String _get_name() const override { return TTR("JSON"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorJSONSyntaxHighlighter() { highlighter.instantiate(); }
};

class EditorMarkdownSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorMarkdownSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "md", "markdown" }; }
	virtual String _get_name() const override { return TTR("Markdown"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorMarkdownSyntaxHighlighter() { highlighter.instantiate(); }
};

class EditorConfigFileSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorConfigFileSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	// While not explicitly designed for those formats, this highlighter happens
	// to handle TSCN, TRES, `project.godot` well. We can expose it in case the
	// user opens one of these using the script editor (which can be done using
	// the All Files filter).
	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "ini", "cfg", "tscn", "tres", "godot" }; }
	virtual String _get_name() const override { return TTR("ConfigFile"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorConfigFileSyntaxHighlighter() { highlighter.instantiate(); }
};

// Base of highlighters whose lines depend on the end state of the previous line (a quoted CSV cell or an
// XML comment over several lines). End states are computed in line order and dropped from an edited line on.
class EditorLineStateSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorLineStateSyntaxHighlighter, EditorSyntaxHighlighter)

	LocalVector<int> line_end_states;

	void _lines_edited_from(int p_from_line, int p_to_line);

protected:
	// Adds the colors of `p_line`, which starts in `p_state`, and returns the state at its end (0 = none).
	virtual int _highlight_line(const String &p_line, int p_state, Dictionary &r_colors) const { return 0; }
	static void _set_color(Dictionary &r_colors, int p_column, const Color &p_color);

public:
	virtual void _update_cache() override;
	virtual void _clear_highlighting_cache() override { line_end_states.clear(); }
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
};

class EditorCSVSyntaxHighlighter : public EditorLineStateSyntaxHighlighter {
	GDCLASS(EditorCSVSyntaxHighlighter, EditorLineStateSyntaxHighlighter)

	static constexpr int COLUMN_COLOR_COUNT = 6;
	Color column_colors[COLUMN_COLOR_COUNT];
	Color symbol_color;
	char32_t delimiter = ',';

protected:
	virtual int _highlight_line(const String &p_line, int p_state, Dictionary &r_colors) const override;

public:
	virtual void _update_cache() override;

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "csv" }; }
	virtual String _get_name() const override { return TTR("CSV"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;
};

class EditorXMLSyntaxHighlighter : public EditorLineStateSyntaxHighlighter {
	GDCLASS(EditorXMLSyntaxHighlighter, EditorLineStateSyntaxHighlighter)

	enum State {
		STATE_TEXT,
		STATE_TAG,
		STATE_VALUE_DOUBLE_QUOTED,
		STATE_VALUE_SINGLE_QUOTED,
		STATE_COMMENT,
		STATE_CDATA,
		STATE_DECLARATION,
	};

	Color text_color;
	Color symbol_color;
	Color tag_color;
	Color attribute_color;
	Color string_color;
	Color comment_color;
	Color keyword_color;

protected:
	virtual int _highlight_line(const String &p_line, int p_state, Dictionary &r_colors) const override;

public:
	virtual void _update_cache() override;

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "xml" }; }
	virtual String _get_name() const override { return TTR("XML"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;
};

// Base of highlighters that color the key (or section) of a line over a CodeHighlighter (TOML and YAML
// modules).
class EditorKeyValueSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorKeyValueSyntaxHighlighter, EditorSyntaxHighlighter)

protected:
	Ref<CodeHighlighter> highlighter;
	Color text_color;
	Color key_color;
	Color section_color;

	// Returns the end column of the key or section of `p_line` and sets its start, or -1 if there is none.
	virtual int _find_key(const String &p_line, int &r_start, bool &r_section) const { return -1; }

public:
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;

	EditorKeyValueSyntaxHighlighter() { highlighter.instantiate(); }
};

class GDShaderSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(GDShaderSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

	Vector<Point2i> disabled_branch_regions;
	Color disabled_branch_color;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "gdshader", "gdshaderinc" }; }
	virtual String _get_name() const override { return TTR("GDShader"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	GDShaderSyntaxHighlighter() { highlighter.instantiate(); }

	void add_disabled_branch_region(const Point2i &p_region);
	void clear_disabled_branch_regions();
	void set_disabled_branch_color(const Color &p_color);
};
