/**************************************************************************/
/*  flow_graph_editor_plugin.h                                            */
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

#include "../flow_graph.h"

#include "editor/debugger/editor_debugger_plugin.h"
#include "editor/docks/editor_dock.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class Button;
class ConfirmationDialog;
class GraphEdit;
class GraphNode;
class Label;
class LineEdit;
class OptionButton;
class PopupMenu;
class Tree;

// Bottom dock that edits a FlowGraph on a GraphEdit. Every change goes through EditorUndoRedoManager.
class FlowGraphEditor : public EditorDock {
	GDCLASS(FlowGraphEditor, EditorDock);

	struct DragOp {
		StringName element;
		Vector2 from;
		Vector2 to;
	};

	struct Clipboard {
		LocalVector<Ref<FlowNode>> nodes; // Deep copies.
		PackedInt32Array ids; // Ids of the copied nodes in the source graph, parallel to nodes.
		Array connections; // from_id, from_port, to_id, to_port, ...
	};

	Ref<FlowGraph> graph;
	LocalVector<Ref<FlowNode>> watched_nodes;

	GraphEdit *graph_edit = nullptr;
	Label *status = nullptr;
	PopupMenu *context_menu = nullptr;

	ConfirmationDialog *add_dialog = nullptr;
	LineEdit *add_search = nullptr;
	Tree *add_tree = nullptr;

	bool rebuild_queued = false;
	bool editing_inline = false;
	bool drag_queued = false;
	Vector2 menu_position;
	Vector2 add_position;

	// Set when a connection is dropped into empty space: the new node connects to this port.
	int pending_node_id = FlowGraph::NODE_ID_NONE;
	Ref<FlowPort> pending_port;
	bool pending_port_is_output = false;

	LocalVector<DragOp> drag_buffer;
	PackedInt32Array link_buffer;
	int link_frame = -1;

	Clipboard clipboard;

	// Playbacks of the running game, reported by the FlowGraph debugger.
	struct DebugPlayback {
		int session = -1;
		int64_t id = 0;
		String graph_path;
		String current_graph_path;
		int node_id = FlowGraph::NODE_ID_NONE;
		bool waiting = false;
	};
	LocalVector<DebugPlayback> debug_playbacks;
	OptionButton *debug_selector = nullptr;
	Button *debug_follow = nullptr;
	int debug_selected_session = -1;
	int64_t debug_selected_id = 0;
	StringName debug_highlighted;

	static Ref<FlowNode> _instantiate_type(const StringName &p_type);
	static Ref<FlowPort> _port_at(const TypedArray<FlowPort> &p_ports, int p_index);
	static int _port_index(const TypedArray<FlowPort> &p_ports, const StringName &p_name);
	static int _find_compatible_port(const Ref<FlowNode> &p_node, const Ref<FlowPort> &p_port, bool p_port_is_output);
	static int _slot_type(const Ref<FlowPort> &p_port);
	static Color _slot_color(const Ref<FlowPort> &p_port);
	static int _element_id(const StringName &p_name);

	void _queue_rebuild();
	void _rebuild();
	void _add_inline_properties(GraphNode *p_graph_node, const Ref<FlowNode> &p_node, int p_id);
	Vector2 _to_graph_position(const Vector2 &p_local) const;
	PackedInt32Array _get_selected_ids() const;

	Dictionary _snapshot() const;
	void _commit_snapshot(const String &p_action, const Dictionary &p_before);
	bool _connect(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port);

	void _connection_request(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port);
	void _disconnection_request(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port);
	void _connection_to_empty(const StringName &p_from, int p_from_port, const Vector2 &p_position);
	void _connection_from_empty(const StringName &p_to, int p_to_port, const Vector2 &p_position);
	void _delete_request(const TypedArray<StringName> &p_names);
	void _delete(const PackedInt32Array &p_ids, const PackedInt32Array &p_frames);
	void _element_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_element);
	void _linked_to_frame_request(const Array &p_elements, const StringName &p_frame);
	void _queue_drag_commit();
	void _commit_drag();
	void _element_selected(Node *p_element);
	void _element_deselected(Node *p_element);
	void _inline_property_changed(const String &p_property, const Variant &p_value, const String &p_field, bool p_changing, int p_id);

	void _copy(bool p_cut);
	void _paste();
	void _duplicate();

	void _popup_request(const Vector2 &p_position);
	void _context_menu_id_pressed(int p_id);

	void _open_add_dialog(const Vector2 &p_position);
	void _update_add_tree();
	void _add_dialog_confirmed();

	void _debug_selected(int p_item);
	void _debug_update();

public:
	void edit(const Ref<FlowGraph> &p_graph);
	Ref<FlowGraph> get_edited_graph() const { return graph; }

	void debug_playback_changed(int p_session, const Array &p_data);
	void debug_session_stopped(int p_session);

	FlowGraphEditor();
};

// Adds an "Open Graph Editor" button to the inspector of a FlowGraph.
class EditorInspectorPluginFlowGraph : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginFlowGraph, EditorInspectorPlugin);

	FlowGraphEditor *editor = nullptr;

	void _open(const Ref<FlowGraph> &p_graph);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;

	EditorInspectorPluginFlowGraph(FlowGraphEditor *p_editor = nullptr) { editor = p_editor; }
};

// Receives the playback states the running game sends (FlowGraphDebugger) and passes them to the editor.
class FlowGraphDebuggerPlugin : public EditorDebuggerPlugin {
	GDCLASS(FlowGraphDebuggerPlugin, EditorDebuggerPlugin);

	FlowGraphEditor *editor = nullptr;

public:
	virtual void setup_session(int p_session) override;
	virtual bool capture(const String &p_message, const Array &p_data, int p_session) override;
	virtual bool has_capture(const String &p_capture) const override;

	FlowGraphDebuggerPlugin(FlowGraphEditor *p_editor = nullptr) { editor = p_editor; }
};

class FlowGraphEditorPlugin : public EditorPlugin {
	GDCLASS(FlowGraphEditorPlugin, EditorPlugin);

	FlowGraphEditor *editor = nullptr;
	Ref<FlowGraphDebuggerPlugin> debugger_plugin;

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "FlowGraph"; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
	virtual bool can_auto_hide() const override;

	FlowGraphEditorPlugin();
};
