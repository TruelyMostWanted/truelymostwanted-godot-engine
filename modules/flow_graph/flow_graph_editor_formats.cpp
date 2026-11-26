/**************************************************************************/
/*  flow_graph_editor_formats.cpp                                         */
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

#ifdef TOOLS_ENABLED

#include "flow_graph_editor_formats.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_paths.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"

void FlowGraphExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	const String extension = p_path.get_extension().to_lower();
	if (p_type != "FlowGraph" || extension == "tres" || extension == "res" || !GLOBAL_GET("flow_graph/export/convert_text_to_binary")) {
		return;
	}
	const Ref<Resource> graph = ResourceLoader::load(p_path, "FlowGraph", ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND_MSG(graph.is_null(), vformat("Cannot convert the FlowGraph \"%s\" to binary; it is exported as text.", p_path));
	const String temp_path = EditorPaths::get_singleton()->get_temp_dir().path_join("flow_graph_export.res");
	const Error err = ResourceSaver::save(graph, temp_path);
	ERR_FAIL_COND_MSG(err != OK, vformat("Cannot convert the FlowGraph \"%s\" to binary; it is exported as text.", p_path));
	add_file(p_path + ".res", FileAccess::get_file_as_bytes(temp_path), true);
}

void EditorMermaidSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	const Color text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(text_color);
	highlighter->set_function_color(text_color);
	highlighter->set_member_variable_color(text_color);

	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	for (const char *keyword : { "flowchart", "graph", "subgraph", "end", "direction", "classDef", "class", "style", "linkStyle", "click", "TD", "TB", "BT", "LR", "RL" }) {
		highlighter->add_keyword_color(keyword, keyword_color);
	}
	highlighter->add_color_region("%%", "", EDITOR_GET("text_editor/theme/highlighting/comment_color"), true);
	highlighter->add_color_region("\"", "\"", EDITOR_GET("text_editor/theme/highlighting/string_color"));
	highlighter->add_color_region("|", "|", EDITOR_GET("text_editor/theme/highlighting/function_color"));
}

Ref<EditorSyntaxHighlighter> EditorMermaidSyntaxHighlighter::_create() const {
	Ref<EditorMermaidSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

void EditorContextMenuPluginMermaid::_open_as_text(const Dictionary &p_data) {
	const PackedStringArray selected = p_data.get("selected_files", PackedStringArray());
	if (selected.size() == 1 && ScriptEditor::get_singleton()) {
		EditorInterface::get_singleton()->set_main_screen_editor("Script");
		ScriptEditor::get_singleton()->open_file(selected[0]);
	}
}

void EditorContextMenuPluginMermaid::get_options(const OptionsData &p_data) {
	EditorContextMenuPlugin::get_options(p_data);
	const PackedStringArray selected = p_data.get("selected_files", PackedStringArray());
	if (selected.size() != 1 || selected[0].get_extension().to_lower() != "mmd") {
		return;
	}
	const Ref<Texture2D> icon = EditorNode::get_singleton()->get_gui_base()->get_editor_theme_icon(SNAME("TextFile"));
	add_context_menu_item(TTR("Open as Text"), callable_mp(this, &EditorContextMenuPluginMermaid::_open_as_text), icon);
}

FlowGraphFormatsEditorPlugin::FlowGraphFormatsEditorPlugin() {
	Ref<FlowGraphExportPlugin> export_plugin;
	export_plugin.instantiate();
	add_export_plugin(export_plugin);

	Ref<EditorContextMenuPluginMermaid> context_menu_plugin;
	context_menu_plugin.instantiate();
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, context_menu_plugin);

	if (ScriptEditor::get_singleton()) {
		Ref<EditorMermaidSyntaxHighlighter> highlighter;
		highlighter.instantiate();
		ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
	}
}

#endif // TOOLS_ENABLED
