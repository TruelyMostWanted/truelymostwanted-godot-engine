/**************************************************************************/
/*  flow_graph_editor_formats.h                                           */
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

#ifdef TOOLS_ENABLED

#include "editor/export/editor_export_plugin.h"
#include "editor/inspector/editor_context_menu_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/script/syntax_highlighters.h"

// Exports FlowGraph files in text formats (`.mmd`, JSON, YAML, TOML) as binary `.res` with a remap.
class FlowGraphExportPlugin : public EditorExportPlugin {
	GDCLASS(FlowGraphExportPlugin, EditorExportPlugin);

protected:
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;

public:
	virtual String get_name() const override { return "FlowGraph"; }
};

// Highlighting of Mermaid flowcharts (`.mmd`) in the script editor.
class EditorMermaidSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorMermaidSyntaxHighlighter, EditorSyntaxHighlighter);

	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }
	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "mmd" }; }
	virtual String _get_name() const override { return "Mermaid"; }
	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorMermaidSyntaxHighlighter() { highlighter.instantiate(); }
};

// "Open as Text" in the context menu of a single `.mmd` file in the FileSystem dock (double-click opens the graph).
class EditorContextMenuPluginMermaid : public EditorContextMenuPlugin {
	GDCLASS(EditorContextMenuPluginMermaid, EditorContextMenuPlugin);

	void _open_as_text(const Dictionary &p_data);

public:
	using EditorContextMenuPlugin::get_options;
	virtual void get_options(const OptionsData &p_data) override;
};

class FlowGraphFormatsEditorPlugin : public EditorPlugin {
	GDCLASS(FlowGraphFormatsEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "FlowGraphFormats"; }

	FlowGraphFormatsEditorPlugin();
};

#endif // TOOLS_ENABLED
