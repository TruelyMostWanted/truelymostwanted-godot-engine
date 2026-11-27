/**************************************************************************/
/*  flow_graph_editor_plugin.cpp                                          */
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

#include "flow_graph_editor_plugin.h"

#include "../dialogue/dialogue_nodes.h"
#include "../flow_graph_debugger.h"
#include "../flow_nodes.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hash_set.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/graph_frame.h"
#include "scene/gui/graph_node.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"
#include "scene/resources/style_box_flat.h"

enum {
	MENU_ADD_NODE,
	MENU_ADD_FRAME,
	MENU_ADD_NODE_AT_CENTER,
	MENU_DETACH_FROM_FRAME,
};

// Helpers.

Ref<FlowNode> FlowGraphEditor::_instantiate_type(const StringName &p_type) {
	Variant instance;
	if (ScriptServer::is_global_class(p_type)) {
		instance = EditorNode::get_editor_data().script_class_instance(p_type);
	} else {
		instance = ClassDB::instantiate(p_type);
	}
	return Ref<FlowNode>(instance);
}

Ref<FlowPort> FlowGraphEditor::_port_at(const TypedArray<FlowPort> &p_ports, int p_index) {
	return (p_index >= 0 && p_index < p_ports.size()) ? Ref<FlowPort>(p_ports[p_index]) : Ref<FlowPort>();
}

int FlowGraphEditor::_port_index(const TypedArray<FlowPort> &p_ports, const StringName &p_name) {
	for (int i = 0; i < p_ports.size(); i++) {
		const Ref<FlowPort> port = p_ports[i];
		if (port.is_valid() && port->get_port_name() == p_name) {
			return i;
		}
	}
	return -1;
}

int FlowGraphEditor::_find_compatible_port(const Ref<FlowNode> &p_node, const Ref<FlowPort> &p_port, bool p_port_is_output) {
	const TypedArray<FlowPort> ports = p_port_is_output ? p_node->get_input_ports() : p_node->get_output_ports();
	for (int i = 0; i < ports.size(); i++) {
		const Ref<FlowPort> other = ports[i];
		if (other.is_null() || other->get_kind() != p_port->get_kind()) {
			continue;
		}
		const Variant::Type from = p_port_is_output ? p_port->get_port_type() : other->get_port_type();
		const Variant::Type to = p_port_is_output ? other->get_port_type() : p_port->get_port_type();
		if (other->get_kind() == FlowPort::PORT_KIND_FLOW || FlowPort::is_type_compatible(from, to)) {
			return i;
		}
	}
	return -1;
}

// Slot type 0 is flow, 1 + Variant.Type is data.
int FlowGraphEditor::_slot_type(const Ref<FlowPort> &p_port) {
	return (p_port.is_null() || p_port->get_kind() == FlowPort::PORT_KIND_FLOW) ? 0 : 1 + p_port->get_port_type();
}

Color FlowGraphEditor::_slot_color(const Ref<FlowPort> &p_port) {
	if (p_port.is_null() || p_port->get_kind() == FlowPort::PORT_KIND_FLOW) {
		return Color(1, 1, 1);
	}
	if (p_port->get_port_type() == Variant::NIL) {
		return Color(0.6, 0.6, 0.6);
	}
	return Color::from_hsv(float(p_port->get_port_type()) / float(Variant::VARIANT_MAX), 0.55, 0.95);
}

// Graph elements are named "n<node id>" and "f<frame index>".
int FlowGraphEditor::_element_id(const StringName &p_name) {
	return String(p_name).substr(1).to_int();
}

Vector2 FlowGraphEditor::_to_graph_position(const Vector2 &p_local) const {
	return (p_local + graph_edit->get_scroll_offset()) / graph_edit->get_zoom();
}

PackedInt32Array FlowGraphEditor::_get_selected_ids() const {
	PackedInt32Array ids;
	for (int i = 0; i < graph_edit->get_child_count(); i++) {
		GraphNode *graph_node = Object::cast_to<GraphNode>(graph_edit->get_child(i));
		if (graph_node != nullptr && graph_node->is_selected()) {
			ids.push_back(_element_id(graph_node->get_name()));
		}
	}
	return ids;
}

// Display.

void FlowGraphEditor::_queue_rebuild() {
	if (editing_inline || rebuild_queued) {
		return;
	}
	rebuild_queued = true;
	callable_mp(this, &FlowGraphEditor::_rebuild).call_deferred();
}

void FlowGraphEditor::_rebuild() {
	rebuild_queued = false;
	const Callable changed = callable_mp(this, &FlowGraphEditor::_queue_rebuild);
	for (const Ref<Resource> &resource : watched) {
		resource->disconnect_changed(changed);
	}
	watched.clear();

	HashSet<StringName> selected;
	for (int i = graph_edit->get_child_count() - 1; i >= 0; i--) {
		GraphElement *element = Object::cast_to<GraphElement>(graph_edit->get_child(i));
		if (element != nullptr) {
			if (element->is_selected()) {
				selected.insert(element->get_name());
			}
			graph_edit->remove_child(element);
			element->queue_free();
		}
	}
	graph_edit->clear_connections();
	debug_highlighted = StringName();
	status->set_text(String());
	if (graph.is_null()) {
		_debug_update();
		return;
	}

	const Ref<FlowGraphValidation> validation = graph->validate();
	PackedStringArray graph_issues;
	HashMap<int, PackedStringArray> node_issues;
	for (int i = 0; i < validation->get_issue_count(); i++) {
		const int id = validation->get_issue_node_id(i);
		if (id == FlowGraph::NODE_ID_NONE) {
			graph_issues.push_back(validation->get_issue_message(i));
		} else {
			node_issues[id].push_back(validation->get_issue_message(i));
		}
	}
	status->set_text(String("\n").join(graph_issues));

	const TypedArray<FlowFrame> frames = graph->get_frames();
	for (int i = 0; i < frames.size(); i++) {
		const Ref<FlowFrame> frame = frames[i];
		if (frame.is_null()) {
			continue;
		}
		frame->connect_changed(changed);
		watched.push_back(frame);
		GraphFrame *graph_frame = memnew(GraphFrame);
		graph_frame->set_name("f" + itos(i));
		graph_frame->set_title(frame->get_title());
		graph_frame->set_tint_color_enabled(true);
		graph_frame->set_tint_color(frame->get_color());
		graph_frame->set_position_offset(frame->get_rect().position);
		graph_frame->set_size(frame->get_rect().size);
		graph_frame->set_autoshrink_enabled(!frame->get_node_ids().is_empty());
		graph_frame->connect("dragged", callable_mp(this, &FlowGraphEditor::_element_dragged).bind(graph_frame->get_name()));
		graph_edit->add_child(graph_frame);
		graph_frame->set_selected(selected.has(graph_frame->get_name()));
	}

	for (const int id : graph->get_node_ids()) {
		const Ref<FlowNode> node = graph->get_node(id);
		if (node.is_null()) {
			continue;
		}
		node->connect_changed(changed);
		watched.push_back(node);

		GraphNode *graph_node = memnew(GraphNode);
		graph_node->set_name("n" + itos(id));
		graph_node->set_title(node->get_title());
		graph_node->set_position_offset(node->get_editor_position());
		graph_node->connect("dragged", callable_mp(this, &FlowGraphEditor::_element_dragged).bind(graph_node->get_name()));
		graph_edit->add_child(graph_node);

		// Row i holds input i on the left and output i on the right, so GraphEdit port indices are port list indices.
		const TypedArray<FlowPort> inputs = node->get_input_ports();
		const TypedArray<FlowPort> outputs = node->get_output_ports();
		for (int row = 0; row < MAX(inputs.size(), outputs.size()); row++) {
			const Ref<FlowPort> input = _port_at(inputs, row);
			const Ref<FlowPort> output = _port_at(outputs, row);
			HBoxContainer *row_box = memnew(HBoxContainer);
			Label *input_label = memnew(Label);
			input_label->set_h_size_flags(SIZE_EXPAND_FILL);
			Label *output_label = memnew(Label);
			output_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			if (input.is_valid()) {
				input_label->set_text(input->get_label().is_empty() ? String(input->get_port_name()) : input->get_label());
			}
			if (output.is_valid()) {
				output_label->set_text(output->get_label().is_empty() ? String(output->get_port_name()) : output->get_label());
			}
			row_box->add_child(input_label);
			row_box->add_child(output_label);
			graph_node->add_child(row_box);
			graph_node->set_slot(row, input.is_valid(), _slot_type(input), _slot_color(input), output.is_valid(), _slot_type(output), _slot_color(output));
		}
		_add_inline_properties(graph_node, node, id);
		if (Object::cast_to<DialogueChoiceNode>(node.ptr()) != nullptr) {
			Button *add_option = memnew(Button);
			add_option->set_text(TTR("Add Option"));
			add_option->connect(SceneStringName(pressed), callable_mp(this, &FlowGraphEditor::_add_option).bind(id));
			graph_node->add_child(add_option);
		}

		PackedStringArray warnings;
		if (const PackedStringArray *issues = node_issues.getptr(id)) {
			warnings = *issues;
		}
		const Ref<Script> node_script = node->get_script();
		if (node_script.is_valid() && !node_script->is_tool()) {
			warnings.push_back(TTR("The script of this node is not a tool script (@tool in GDScript, [Tool] in C#), so its ports are unknown in the editor."));
		}
		if (!warnings.is_empty()) {
			TextureRect *icon = memnew(TextureRect);
			icon->set_texture(get_editor_theme_icon(SNAME("NodeWarning")));
			icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
			icon->set_mouse_filter(MOUSE_FILTER_PASS);
			icon->set_tooltip_text(String("\n").join(warnings));
			graph_node->get_titlebar_hbox()->add_child(icon);
		}
		graph_node->set_selected(selected.has(graph_node->get_name()));
	}

	for (int i = 0; i < frames.size(); i++) {
		const Ref<FlowFrame> frame = frames[i];
		if (frame.is_valid()) {
			for (const int id : frame->get_node_ids()) {
				if (graph->has_node(id)) {
					graph_edit->attach_graph_element_to_frame("n" + itos(id), "f" + itos(i));
				}
			}
		}
	}

	const TypedArray<FlowConnection> graph_connections = graph->get_connections();
	for (int i = 0; i < graph_connections.size(); i++) {
		const Ref<FlowConnection> c = graph_connections[i];
		if (!graph->has_node(c->get_from_node_id()) || !graph->has_node(c->get_to_node_id())) {
			continue;
		}
		const int from_port = _port_index(graph->get_node(c->get_from_node_id())->get_output_ports(), c->get_from_port());
		const int to_port = _port_index(graph->get_node(c->get_to_node_id())->get_input_ports(), c->get_to_port());
		if (from_port >= 0 && to_port >= 0) {
			graph_edit->connect_node("n" + itos(c->get_from_node_id()), from_port, "n" + itos(c->get_to_node_id()), to_port);
		}
	}
	if (frame_view_pending) {
		frame_view_pending = false;
		// After the new elements have their sizes.
		callable_mp(this, &FlowGraphEditor::_frame_view).call_deferred();
	}
	if (fit_frame >= 0) {
		callable_mp(this, &FlowGraphEditor::_fit_frame).call_deferred(fit_frame);
		fit_frame = -1;
	}
	_debug_update();
}

// Stores the rect GraphEdit shows for a frame fitted around its nodes (autoshrink), once the nodes have their sizes.
void FlowGraphEditor::_fit_frame(int p_index) {
	GraphFrame *graph_frame = Object::cast_to<GraphFrame>(graph_edit->get_node_or_null(NodePath("f" + itos(p_index))));
	if (graph.is_null() || graph_frame == nullptr || p_index >= graph->get_frames().size()) {
		return;
	}
	const Ref<FlowFrame> frame = graph->get_frames()[p_index];
	graph_frame->emit_signal(SNAME("autoshrink_changed"), graph_frame->get_size());
	const Rect2 shown(graph_frame->get_position_offset(), graph_frame->get_size());
	if (frame.is_valid() && shown != frame->get_rect()) {
		frame->set_rect(shown); // Part of the Add Frame result, not an own undo step.
	}
}

void FlowGraphEditor::_frame_view() {
	if (debug_follow->is_pressed()) {
		return; // Following a playback centers on its node instead.
	}
	Rect2 bounds;
	bool empty = true;
	for (int i = 0; i < graph_edit->get_child_count(); i++) {
		const GraphElement *element = Object::cast_to<GraphElement>(graph_edit->get_child(i));
		if (element != nullptr) {
			const Rect2 rect(element->get_position_offset(), element->get_size());
			bounds = empty ? rect : bounds.merge(rect);
			empty = false;
		}
	}
	if (!empty) {
		// The top left of the contents just below the toolbar, which lies over the graph.
		const Vector2 margin = Vector2(20 * EDSCALE, graph_edit->get_menu_hbox()->get_size().y + 30 * EDSCALE);
		graph_edit->set_scroll_offset(bounds.position * graph_edit->get_zoom() - margin);
	}
}

void FlowGraphEditor::_add_option(int p_id) {
	ERR_FAIL_COND(graph.is_null());
	const Ref<DialogueChoiceNode> choice = graph->get_node(p_id);
	ERR_FAIL_COND(choice.is_null());
	const TypedArray<DialogueOption> options = choice->get_options();
	Ref<DialogueOption> option;
	option.instantiate();
	option->set_text("Option " + itos(options.size() + 1));
	TypedArray<DialogueOption> new_options = options.duplicate();
	new_options.push_back(option);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Option"), UndoRedo::MERGE_DISABLE, graph.ptr());
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(choice.ptr(), "set_options", new_options);
	undo_redo->add_undo_method(choice.ptr(), "set_options", options);
	undo_redo->commit_action();
}

void FlowGraphEditor::_add_inline_properties(GraphNode *p_graph_node, const Ref<FlowNode> &p_node, int p_id) {
	const PackedStringArray names = p_node->get_inline_properties();
	if (names.is_empty()) {
		return;
	}
	List<PropertyInfo> properties;
	p_node->get_property_list(&properties);
	for (const String &property_name : names) {
		for (const PropertyInfo &info : properties) {
			if (info.name != property_name) {
				continue;
			}
			EditorProperty *editor = EditorInspector::instantiate_property_editor(p_node.ptr(), info.type, info.name, info.hint, info.hint_string, info.usage, true);
			if (editor != nullptr) {
				editor->set_object_and_property(p_node.ptr(), info.name);
				editor->set_label(info.name.capitalize());
				editor->set_custom_minimum_size(Size2(220, 0) * EDSCALE);
				editor->update_property();
				editor->connect("property_changed", callable_mp(this, &FlowGraphEditor::_inline_property_changed).bind(p_id));
				p_graph_node->add_child(editor);
			}
			break;
		}
	}
}

// Undo/redo. Structural edits change the graph first, then record the storage before and after.

Dictionary FlowGraphEditor::_snapshot() const {
	Dictionary snapshot;
	snapshot["node_ids"] = graph->get_node_ids();
	snapshot["nodes"] = graph->get("nodes");
	snapshot["connections"] = graph->get("connections");
	snapshot["frames"] = graph->get_frames().duplicate();
	snapshot["next_node_id"] = graph->get("next_node_id");
	snapshot["start_node_id"] = graph->get_start_node_id();
	// remove_node() edits the node lists of frames in place.
	Array frame_node_ids;
	const TypedArray<FlowFrame> frames = graph->get_frames();
	for (int i = 0; i < frames.size(); i++) {
		const Ref<FlowFrame> frame = frames[i];
		frame_node_ids.push_back(frame.is_valid() ? frame->get_node_ids() : PackedInt32Array());
	}
	snapshot["frame_node_ids"] = frame_node_ids;
	return snapshot;
}

void FlowGraphEditor::_commit_snapshot(const String &p_action, const Dictionary &p_before) {
	const Dictionary after = _snapshot();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, UndoRedo::MERGE_DISABLE, graph.ptr());
	undo_redo->force_fixed_history(); // Nodes and frames follow the history of the graph.
	static const char *storage[] = { "node_ids", "nodes", "connections", "frames", "next_node_id", "start_node_id" };
	for (const char *property : storage) {
		undo_redo->add_do_property(graph.ptr(), property, after[property]);
		undo_redo->add_undo_property(graph.ptr(), property, p_before[property]);
	}
	const Array after_frames = after["frames"];
	const Array after_ids = after["frame_node_ids"];
	for (int i = 0; i < after_frames.size(); i++) {
		Object *frame = after_frames[i];
		if (frame != nullptr) {
			undo_redo->add_do_property(frame, "node_ids", after_ids[i]);
		}
	}
	const Array before_frames = p_before["frames"];
	const Array before_ids = p_before["frame_node_ids"];
	for (int i = 0; i < before_frames.size(); i++) {
		Object *frame = before_frames[i];
		if (frame != nullptr) {
			undo_redo->add_undo_property(frame, "node_ids", before_ids[i]);
		}
	}
	undo_redo->add_do_method(graph.ptr(), "emit_changed");
	undo_redo->add_undo_method(graph.ptr(), "emit_changed");
	undo_redo->commit_action(false);
}

// A flow output and a data input take one connection: a new connection replaces the old one.
bool FlowGraphEditor::_connect(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) {
	const Ref<FlowPort> output = graph->get_node(p_from_id)->find_output_port(p_from_port);
	ERR_FAIL_COND_V(output.is_null(), false);
	const bool is_flow = output->get_kind() == FlowPort::PORT_KIND_FLOW;
	const TypedArray<FlowConnection> used = is_flow ? graph->get_output_connections(p_from_id) : graph->get_input_connections(p_to_id);
	for (int i = 0; i < used.size(); i++) {
		const Ref<FlowConnection> c = used[i];
		if ((is_flow && c->get_from_port() == p_from_port) || (!is_flow && c->get_to_port() == p_to_port)) {
			graph->disconnect_nodes(c->get_from_node_id(), c->get_from_port(), c->get_to_node_id(), c->get_to_port());
		}
	}
	return graph->connect_nodes(p_from_id, p_from_port, p_to_id, p_to_port) == OK;
}

void FlowGraphEditor::_connection_request(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {
	const int from_id = _element_id(p_from);
	const int to_id = _element_id(p_to);
	ERR_FAIL_COND(graph.is_null() || !graph->has_node(from_id) || !graph->has_node(to_id));
	const Ref<FlowPort> output = _port_at(graph->get_node(from_id)->get_output_ports(), p_from_port);
	const Ref<FlowPort> input = _port_at(graph->get_node(to_id)->get_input_ports(), p_to_port);
	ERR_FAIL_COND(output.is_null() || input.is_null());
	const Dictionary before = _snapshot();
	if (_connect(from_id, output->get_port_name(), to_id, input->get_port_name())) {
		_commit_snapshot(TTR("Connect Nodes"), before);
	} else {
		// Refused (the reason is printed): put back what _connect() replaced.
		graph->set("connections", before["connections"]);
	}
}

void FlowGraphEditor::_disconnection_request(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {
	const int from_id = _element_id(p_from);
	const int to_id = _element_id(p_to);
	ERR_FAIL_COND(graph.is_null() || !graph->has_node(from_id) || !graph->has_node(to_id));
	const Ref<FlowPort> output = _port_at(graph->get_node(from_id)->get_output_ports(), p_from_port);
	const Ref<FlowPort> input = _port_at(graph->get_node(to_id)->get_input_ports(), p_to_port);
	ERR_FAIL_COND(output.is_null() || input.is_null());
	const Dictionary before = _snapshot();
	graph->disconnect_nodes(from_id, output->get_port_name(), to_id, input->get_port_name());
	_commit_snapshot(TTR("Disconnect Nodes"), before);
}

void FlowGraphEditor::_connection_to_empty(const StringName &p_from, int p_from_port, const Vector2 &p_position) {
	const int id = _element_id(p_from);
	ERR_FAIL_COND(graph.is_null() || !graph->has_node(id));
	pending_node_id = id;
	pending_port = _port_at(graph->get_node(id)->get_output_ports(), p_from_port);
	pending_port_is_output = true;
	_open_add_dialog(p_position);
}

void FlowGraphEditor::_connection_from_empty(const StringName &p_to, int p_to_port, const Vector2 &p_position) {
	const int id = _element_id(p_to);
	ERR_FAIL_COND(graph.is_null() || !graph->has_node(id));
	pending_node_id = id;
	pending_port = _port_at(graph->get_node(id)->get_input_ports(), p_to_port);
	pending_port_is_output = false;
	_open_add_dialog(p_position);
}

void FlowGraphEditor::_delete_request(const TypedArray<StringName> &p_names) {
	PackedInt32Array ids;
	PackedInt32Array frame_indices;
	for (int i = 0; i < p_names.size(); i++) {
		const String element = p_names[i];
		(element.begins_with("f") ? frame_indices : ids).push_back(_element_id(element));
	}
	_delete(ids, frame_indices);
}

void FlowGraphEditor::_delete(const PackedInt32Array &p_ids, const PackedInt32Array &p_frames) {
	if (graph.is_null() || (p_ids.is_empty() && p_frames.is_empty())) {
		return;
	}
	const Dictionary before = _snapshot();
	for (const int id : p_ids) {
		if (graph->has_node(id)) {
			graph->remove_node(id);
		}
	}
	if (!p_frames.is_empty()) {
		const TypedArray<FlowFrame> frames = graph->get_frames();
		TypedArray<FlowFrame> kept;
		for (int i = 0; i < frames.size(); i++) {
			if (!p_frames.has(i)) {
				kept.push_back(frames[i]);
			}
		}
		graph->set_frames(kept);
	}
	_commit_snapshot(TTR("Delete Nodes"), before);
}

void FlowGraphEditor::_element_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_element) {
	drag_buffer.push_back({ p_element, p_from, p_to });
	_queue_drag_commit();
}

void FlowGraphEditor::_linked_to_frame_request(const Array &p_elements, const StringName &p_frame) {
	link_frame = _element_id(p_frame);
	link_buffer.clear();
	for (int i = 0; i < p_elements.size(); i++) {
		const String element = p_elements[i];
		if (element.begins_with("n")) {
			link_buffer.push_back(_element_id(element));
		}
	}
	_queue_drag_commit();
}

void FlowGraphEditor::_queue_drag_commit() {
	if (!drag_queued) {
		drag_queued = true;
		callable_mp(this, &FlowGraphEditor::_commit_drag).call_deferred();
	}
}

void FlowGraphEditor::_commit_drag() {
	drag_queued = false;
	if (graph.is_valid()) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Move Nodes"), UndoRedo::MERGE_DISABLE, graph.ptr());
		undo_redo->force_fixed_history();
		const TypedArray<FlowFrame> frames = graph->get_frames();
		for (const DragOp &op : drag_buffer) {
			const int id = _element_id(op.element);
			const bool is_node = String(op.element).begins_with("n");
			if (is_node && graph->has_node(id)) {
				const Ref<FlowNode> node = graph->get_node(id);
				undo_redo->add_do_property(node.ptr(), "editor_position", op.to);
				undo_redo->add_undo_property(node.ptr(), "editor_position", op.from);
			}
		}
		// Frames keep what GraphEdit shows: moved, or shrunk and grown around their moved nodes.
		for (int i = 0; i < frames.size(); i++) {
			const Ref<FlowFrame> frame = frames[i];
			GraphFrame *graph_frame = Object::cast_to<GraphFrame>(graph_edit->get_node_or_null(NodePath("f" + itos(i))));
			if (frame.is_null() || graph_frame == nullptr) {
				continue;
			}
			// A dragged frame is snapped, its nodes aren't: let GraphEdit fit it around them first (it does so later anyway).
			graph_frame->emit_signal(SNAME("autoshrink_changed"), graph_frame->get_size());
			const Rect2 shown(graph_frame->get_position_offset(), graph_frame->get_size());
			if (shown != frame->get_rect()) {
				undo_redo->add_do_property(frame.ptr(), "rect", shown);
				undo_redo->add_undo_property(frame.ptr(), "rect", frame->get_rect());
			}
		}
		if (link_frame >= 0 && link_frame < frames.size()) {
			const Ref<FlowFrame> frame = frames[link_frame];
			PackedInt32Array ids = frame->get_node_ids();
			for (const int id : link_buffer) {
				if (!ids.has(id)) {
					ids.push_back(id);
				}
			}
			undo_redo->add_do_property(frame.ptr(), "node_ids", ids);
			undo_redo->add_undo_property(frame.ptr(), "node_ids", frame->get_node_ids());
		}
		undo_redo->add_do_method(graph.ptr(), "emit_changed");
		undo_redo->add_undo_method(graph.ptr(), "emit_changed");
		undo_redo->commit_action();
	}
	drag_buffer.clear();
	link_buffer.clear();
	link_frame = -1;
}

void FlowGraphEditor::_element_selected(Node *p_element) {
	ERR_FAIL_COND(graph.is_null());
	const String element = p_element->get_name();
	const int id = _element_id(element);
	Ref<Resource> resource;
	if (element.begins_with("n") && graph->has_node(id)) {
		resource = graph->get_node(id);
	} else if (element.begins_with("f") && id < graph->get_frames().size()) {
		resource = graph->get_frames()[id];
	}
	if (resource.is_valid()) {
		// Inspector only, so the graph stays the edited object. can_auto_hide() keeps the dock open.
		EditorNode::get_singleton()->push_item(resource.ptr(), "", true);
	}
}

void FlowGraphEditor::_element_deselected(Node *p_element) {
	// GraphEdit may emit node_deselected after node_selected of the new element: decide once both are done.
	callable_mp(this, &FlowGraphEditor::_show_graph_if_nothing_selected).call_deferred();
}

void FlowGraphEditor::_show_graph_if_nothing_selected() {
	if (graph.is_null()) {
		return;
	}
	for (int i = 0; i < graph_edit->get_child_count(); i++) {
		GraphElement *element = Object::cast_to<GraphElement>(graph_edit->get_child(i));
		if (element != nullptr && element->is_selected()) {
			return;
		}
	}
	EditorNode::get_singleton()->push_item(graph.ptr(), "", true);
}

void FlowGraphEditor::_inline_property_changed(const String &p_property, const Variant &p_value, const String &p_field, bool p_changing, int p_id) {
	ERR_FAIL_COND(graph.is_null() || !graph->has_node(p_id));
	const Ref<FlowNode> node = graph->get_node(p_id);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Set %s on Node %d"), p_property, p_id), UndoRedo::MERGE_ENDS, graph.ptr());
	undo_redo->force_fixed_history();
	undo_redo->add_do_property(node.ptr(), p_property, p_value);
	undo_redo->add_undo_property(node.ptr(), p_property, node->get(p_property));
	// The property editor already shows the value; a rebuild would take the focus while typing.
	editing_inline = true;
	undo_redo->commit_action();
	editing_inline = false;
}

// Clipboard.

void FlowGraphEditor::_copy(bool p_cut) {
	const PackedInt32Array ids = _get_selected_ids();
	if (graph.is_null() || ids.is_empty()) {
		return;
	}
	clipboard.nodes.clear();
	clipboard.ids = ids;
	clipboard.connections.clear();
	for (const int id : ids) {
		clipboard.nodes.push_back(Ref<FlowNode>(graph->get_node(id)->duplicate(true)));
	}
	const TypedArray<FlowConnection> graph_connections = graph->get_connections();
	for (int i = 0; i < graph_connections.size(); i++) {
		const Ref<FlowConnection> c = graph_connections[i];
		if (ids.has(c->get_from_node_id()) && ids.has(c->get_to_node_id())) {
			clipboard.connections.push_back(c->get_from_node_id());
			clipboard.connections.push_back(c->get_from_port());
			clipboard.connections.push_back(c->get_to_node_id());
			clipboard.connections.push_back(c->get_to_port());
		}
	}
	if (p_cut) {
		_delete(ids, PackedInt32Array());
	}
}

void FlowGraphEditor::_paste() {
	if (graph.is_null() || clipboard.nodes.is_empty()) {
		return;
	}
	// The copied nodes keep their layout; their top left goes to the mouse, or next to the originals.
	Vector2 origin = clipboard.nodes[0]->get_editor_position();
	for (const Ref<FlowNode> &node : clipboard.nodes) {
		origin = origin.min(node->get_editor_position());
	}
	const Vector2 step = Vector2(40, 40) * EDSCALE;
	const Vector2 mouse = graph_edit->get_local_mouse_position();
	const Vector2 target = Rect2(Vector2(), graph_edit->get_size()).has_point(mouse) ? _to_graph_position(mouse) : origin + step;
	// Pasting again at the same place shifts each copy, so the copies do not hide each other.
	if (target.is_equal_approx(paste_target)) {
		paste_shift += step;
	} else {
		paste_target = target;
		paste_shift = Vector2();
	}
	const Vector2 offset = target + paste_shift - origin;

	const Dictionary before = _snapshot();
	HashMap<int, int> new_ids;
	for (uint32_t i = 0; i < clipboard.nodes.size(); i++) {
		// Copy again, so the clipboard can be pasted more than once.
		const Ref<FlowNode> node = clipboard.nodes[i]->duplicate(true);
		new_ids.insert(clipboard.ids[i], graph->add_node(node, node->get_editor_position() + offset));
	}
	for (int i = 0; i + 3 < clipboard.connections.size(); i += 4) {
		graph->connect_nodes(new_ids[(int)clipboard.connections[i]], (StringName)clipboard.connections[i + 1], new_ids[(int)clipboard.connections[i + 2]], (StringName)clipboard.connections[i + 3]);
	}
	_commit_snapshot(TTR("Paste Nodes"), before);
}

void FlowGraphEditor::_duplicate() {
	_copy(false);
	_paste();
}

// Context menu, frames and the add-node dialog.

void FlowGraphEditor::_popup_request(const Vector2 &p_position) {
	if (graph.is_null()) {
		return;
	}
	menu_position = p_position;
	bool in_frame = false;
	const PackedInt32Array ids = _get_selected_ids();
	const TypedArray<FlowFrame> frames = graph->get_frames();
	for (int i = 0; i < frames.size() && !in_frame; i++) {
		const Ref<FlowFrame> frame = frames[i];
		for (int j = 0; frame.is_valid() && j < ids.size() && !in_frame; j++) {
			in_frame = frame->get_node_ids().has(ids[j]);
		}
	}
	context_menu->set_item_disabled(context_menu->get_item_index(MENU_DETACH_FROM_FRAME), !in_frame);
	context_menu->set_position(graph_edit->get_screen_position() + p_position);
	context_menu->reset_size();
	context_menu->popup();
}

void FlowGraphEditor::_context_menu_id_pressed(int p_id) {
	if (graph.is_null()) {
		return;
	}
	if (p_id == MENU_DETACH_FROM_FRAME) {
		const PackedInt32Array ids = _get_selected_ids();
		const Dictionary before = _snapshot();
		bool detached = false;
		const TypedArray<FlowFrame> frames = graph->get_frames();
		for (int i = 0; i < frames.size(); i++) {
			const Ref<FlowFrame> frame = frames[i];
			if (frame.is_null()) {
				continue;
			}
			PackedInt32Array kept = frame->get_node_ids();
			for (const int id : ids) {
				kept.erase(id);
			}
			if (kept.size() != frame->get_node_ids().size()) {
				frame->set_node_ids(kept);
				detached = true;
			}
		}
		if (detached) {
			_commit_snapshot(TTR("Detach from Frame"), before);
		}
		return;
	}
	if (p_id == MENU_ADD_FRAME) {
		// Around the selected nodes, with room for the title; at the click without a selection.
		Rect2 rect(_to_graph_position(menu_position), Size2(200, 120) * EDSCALE);
		bool first = true;
		for (int i = 0; i < graph_edit->get_child_count(); i++) {
			GraphNode *graph_node = Object::cast_to<GraphNode>(graph_edit->get_child(i));
			if (graph_node != nullptr && graph_node->is_selected()) {
				const Rect2 node_rect(graph_node->get_position_offset(), graph_node->get_size());
				rect = first ? node_rect : rect.merge(node_rect);
				first = false;
			}
		}
		if (!first) {
			const real_t margin = 40 * EDSCALE;
			rect = rect.grow_individual(margin, margin + 30 * EDSCALE, margin, margin);
		}
		const Dictionary before = _snapshot();
		Ref<FlowFrame> frame;
		frame.instantiate();
		frame->set_title(TTR("Frame"));
		frame->set_node_ids(_get_selected_ids());
		frame->set_rect(rect);
		TypedArray<FlowFrame> frames;
		frames.append_array(graph->get_frames());
		frames.push_back(frame);
		graph->set_frames(frames);
		if (!frame->get_node_ids().is_empty()) {
			fit_frame = frames.size() - 1;
		}
		_commit_snapshot(TTR("Add Frame"), before);
		return;
	}
	pending_port.unref();
	_open_add_dialog(p_id == MENU_ADD_NODE ? menu_position : graph_edit->get_size() / 2);
	if (p_id == MENU_ADD_NODE_AT_CENTER) {
		add_position = _free_position(add_position);
	}
}

// Moves the position down below the nodes that would cover a new node there.
Vector2 FlowGraphEditor::_free_position(Vector2 p_position) const {
	const Size2 new_size = Size2(160, 80) * EDSCALE;
	for (int pass = 0; pass < graph_edit->get_child_count(); pass++) {
		bool moved = false;
		for (int i = 0; i < graph_edit->get_child_count(); i++) {
			GraphNode *graph_node = Object::cast_to<GraphNode>(graph_edit->get_child(i));
			if (graph_node == nullptr) {
				continue;
			}
			const Rect2 rect(graph_node->get_position_offset(), graph_node->get_size());
			if (rect.intersects(Rect2(p_position, new_size))) {
				p_position.y = rect.get_end().y + 20 * EDSCALE;
				moved = true;
			}
		}
		if (!moved) {
			break;
		}
	}
	return p_position;
}

void FlowGraphEditor::_open_add_dialog(const Vector2 &p_position) {
	add_position = _to_graph_position(p_position);
	add_search->clear();
	_update_add_tree();
	add_dialog->popup_centered_clamped(Size2(400, 500) * EDSCALE);
	add_search->grab_focus();
}

void FlowGraphEditor::_update_add_tree() {
	add_tree->clear();
	TreeItem *root = add_tree->create_item();

	// C++ node types, and script classes (GDScript class_name, C# [GlobalClass]) whose script chain ends in a FlowNode type.
	LocalVector<StringName> types;
	ClassDB::get_inheriters_from_class(FlowNode::get_class_static(), types);
	LocalVector<StringName> script_classes;
	ScriptServer::get_global_class_list(script_classes);
	for (const StringName &script_class : script_classes) {
		if (!ScriptServer::is_global_class_abstract(script_class) && ClassDB::is_parent_class(ScriptServer::get_global_class_native_base(script_class), FlowNode::get_class_static())) {
			types.push_back(script_class);
		}
	}
	types.sort_custom<StringName::AlphCompare>();

	const String filter = add_search->get_text().strip_edges();
	HashMap<String, TreeItem *> categories;
	TreeItem *first = nullptr;
	for (const StringName &type : types) {
		const bool is_script = ScriptServer::is_global_class(type);
		if (!is_script && !ClassDB::can_instantiate(type)) {
			continue;
		}
		const Ref<FlowNode> node = _instantiate_type(type);
		if (node.is_null()) {
			continue;
		}
		const String node_title = node->get_title();
		if (!filter.is_empty() && !node_title.containsn(filter) && !String(type).containsn(filter)) {
			continue;
		}
		if (pending_port.is_valid() && _find_compatible_port(node, pending_port, pending_port_is_output) < 0) {
			continue;
		}
		String category = node->get_category();
		if (category.is_empty()) {
			category = is_script ? TTR("Scripts") : TTR("Other");
		}
		TreeItem **parent = categories.getptr(category);
		if (parent == nullptr) {
			TreeItem *category_item = add_tree->create_item(root);
			category_item->set_text(0, category);
			category_item->set_selectable(0, false);
			parent = &categories.insert(category, category_item)->value;
		}
		TreeItem *item = add_tree->create_item(*parent);
		item->set_text(0, node_title);
		item->set_icon(0, EditorNode::get_singleton()->get_class_icon(type, "Object"));
		item->set_tooltip_text(0, type);
		item->set_metadata(0, type);
		if (first == nullptr) {
			first = item;
		}
	}
	if (first != nullptr) {
		first->select(0);
	}
}

void FlowGraphEditor::_add_dialog_confirmed() {
	add_dialog->hide();
	TreeItem *item = add_tree->get_selected();
	if (graph.is_null() || item == nullptr || item->get_metadata(0).get_type() == Variant::NIL) {
		return;
	}
	const Ref<FlowNode> node = _instantiate_type(item->get_metadata(0));
	ERR_FAIL_COND(node.is_null());
	const Dictionary before = _snapshot();
	const int id = graph->add_node(node, add_position);
	if (pending_port.is_valid() && graph->has_node(pending_node_id)) {
		const int index = _find_compatible_port(node, pending_port, pending_port_is_output);
		const Ref<FlowPort> port = _port_at(pending_port_is_output ? node->get_input_ports() : node->get_output_ports(), index);
		if (port.is_valid()) {
			if (pending_port_is_output) {
				_connect(pending_node_id, pending_port->get_port_name(), id, port->get_port_name());
			} else {
				_connect(id, port->get_port_name(), pending_node_id, pending_port->get_port_name());
			}
		}
	}
	pending_port.unref();
	_commit_snapshot(vformat(TTR("Add %s"), node->get_title()), before);
}

void FlowGraphEditor::edit(const Ref<FlowGraph> &p_graph) {
	if (graph == p_graph) {
		return;
	}
	const Callable changed = callable_mp(this, &FlowGraphEditor::_queue_rebuild);
	if (graph.is_valid()) {
		graph->disconnect_changed(changed);
	}
	graph = p_graph;
	if (graph.is_valid()) {
		graph->connect_changed(changed);
	}
	frame_view_pending = graph.is_valid();
	paste_target = Vector2(Math::INF, Math::INF);
	_queue_rebuild();
}

FlowGraphEditor::FlowGraphEditor() {
	set_name(TTRC("FlowGraph"));
	set_icon_name("GraphEdit");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_flow_graph_bottom_panel", TTRC("Toggle FlowGraph Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);

	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);

	graph_edit = memnew(GraphEdit);
	graph_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	graph_edit->set_show_zoom_label(true);
	// Dragging a connection end away from an input removes the connection.
	graph_edit->set_right_disconnects(true);
	vbox->add_child(graph_edit);
	// Same rules as FlowPort.is_type_compatible(): "Any" (NIL) fits every type, int fits float. Pairs are (output, input).
	for (int type = 0; type < Variant::VARIANT_MAX; type++) {
		graph_edit->add_valid_connection_type(1 + Variant::NIL, 1 + type);
		graph_edit->add_valid_connection_type(1 + type, 1 + Variant::NIL);
	}
	graph_edit->add_valid_connection_type(1 + Variant::INT, 1 + Variant::FLOAT);
	graph_edit->connect("connection_request", callable_mp(this, &FlowGraphEditor::_connection_request), CONNECT_DEFERRED);
	graph_edit->connect("disconnection_request", callable_mp(this, &FlowGraphEditor::_disconnection_request), CONNECT_DEFERRED);
	graph_edit->connect("connection_to_empty", callable_mp(this, &FlowGraphEditor::_connection_to_empty));
	graph_edit->connect("connection_from_empty", callable_mp(this, &FlowGraphEditor::_connection_from_empty));
	graph_edit->connect("delete_nodes_request", callable_mp(this, &FlowGraphEditor::_delete_request));
	graph_edit->connect("copy_nodes_request", callable_mp(this, &FlowGraphEditor::_copy).bind(false));
	graph_edit->connect("cut_nodes_request", callable_mp(this, &FlowGraphEditor::_copy).bind(true));
	graph_edit->connect("paste_nodes_request", callable_mp(this, &FlowGraphEditor::_paste));
	graph_edit->connect("duplicate_nodes_request", callable_mp(this, &FlowGraphEditor::_duplicate));
	graph_edit->connect("node_selected", callable_mp(this, &FlowGraphEditor::_element_selected));
	graph_edit->connect("node_deselected", callable_mp(this, &FlowGraphEditor::_element_deselected));
	graph_edit->connect("popup_request", callable_mp(this, &FlowGraphEditor::_popup_request));
	graph_edit->connect("graph_elements_linked_to_frame_request", callable_mp(this, &FlowGraphEditor::_linked_to_frame_request));

	Button *add_button = memnew(Button);
	add_button->set_text(TTR("Add Node..."));
	add_button->set_flat(true);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &FlowGraphEditor::_context_menu_id_pressed).bind(MENU_ADD_NODE_AT_CENTER));
	graph_edit->get_menu_hbox()->add_child(add_button);

	debug_selector = memnew(OptionButton);
	debug_selector->set_tooltip_text(TTR("Running playback to show. The game must run from the editor with a debug export or build."));
	debug_selector->set_accessibility_name(TTRC("Running Playback"));
	debug_selector->hide();
	debug_selector->connect(SceneStringName(item_selected), callable_mp(this, &FlowGraphEditor::_debug_selected));
	graph_edit->get_menu_hbox()->add_child(debug_selector);
	debug_follow = memnew(Button);
	debug_follow->set_text(TTR("Follow"));
	debug_follow->set_tooltip_text(TTR("Follow the running playback: open the graph its current node is in (also subgraphs) and center the view on that node."));
	debug_follow->set_toggle_mode(true);
	debug_follow->set_flat(true);
	debug_follow->hide();
	debug_follow->connect(SceneStringName(toggled), callable_mp(this, &FlowGraphEditor::_debug_update).unbind(1));
	graph_edit->get_menu_hbox()->add_child(debug_follow);

	status = memnew(Label);
	status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	vbox->add_child(status);

	context_menu = memnew(PopupMenu);
	context_menu->add_item(TTR("Add Node..."), MENU_ADD_NODE);
	context_menu->add_item(TTR("Add Frame Around Selection"), MENU_ADD_FRAME);
	context_menu->add_item(TTR("Detach from Frame"), MENU_DETACH_FROM_FRAME);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &FlowGraphEditor::_context_menu_id_pressed));
	add_child(context_menu);

	add_dialog = memnew(ConfirmationDialog);
	add_dialog->set_title(TTR("Add FlowNode"));
	add_dialog->set_ok_button_text(TTR("Create"));
	VBoxContainer *dialog_vbox = memnew(VBoxContainer);
	add_dialog->add_child(dialog_vbox);
	add_search = memnew(LineEdit);
	add_search->set_placeholder(TTR("Search"));
	add_search->set_clear_button_enabled(true);
	add_search->connect(SceneStringName(text_changed), callable_mp(this, &FlowGraphEditor::_update_add_tree).unbind(1));
	dialog_vbox->add_child(add_search);
	add_dialog->register_text_enter(add_search);
	add_tree = memnew(Tree);
	add_tree->set_hide_root(true);
	add_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_tree->connect("item_activated", callable_mp(this, &FlowGraphEditor::_add_dialog_confirmed));
	dialog_vbox->add_child(add_tree);
	add_dialog->connect(SceneStringName(confirmed), callable_mp(this, &FlowGraphEditor::_add_dialog_confirmed));
	add_child(add_dialog);
}

// Debugger: highlights the current node of a playback in the running game.

void FlowGraphEditor::_debug_selected(int p_item) {
	const DebugPlayback &playback = debug_playbacks[debug_selector->get_item_id(p_item)];
	debug_selected_session = playback.session;
	debug_selected_id = playback.id;
	_debug_update();
}

void FlowGraphEditor::_debug_update() {
	const DebugPlayback *selected = nullptr;
	for (const DebugPlayback &playback : debug_playbacks) {
		if (playback.session == debug_selected_session && playback.id == debug_selected_id) {
			selected = &playback;
		}
	}

	// Follow the selected playback into subgraphs and back. Built-in graphs ("::" paths) only resolve while their scene is open.
	const bool follow = debug_follow->is_pressed();
	if (selected != nullptr && follow && !selected->current_graph_path.is_empty() && (graph.is_null() || graph->get_path() != selected->current_graph_path)) {
		Ref<FlowGraph> target = ResourceCache::get_ref(selected->current_graph_path);
		if (target.is_null() && !selected->current_graph_path.contains("::")) {
			target = ResourceLoader::load(selected->current_graph_path);
		}
		if (target.is_valid()) {
			edit(target); // Rebuilds, then comes back here.
			return;
		}
	}

	// List the playbacks that run in the edited graph (and the selected one), to pick one when several run.
	const String path = graph.is_valid() ? graph->get_path() : String();
	debug_selector->clear();
	for (uint32_t i = 0; i < debug_playbacks.size(); i++) {
		const DebugPlayback &playback = debug_playbacks[i];
		if (&playback != selected && (path.is_empty() || (playback.graph_path != path && playback.current_graph_path != path))) {
			continue;
		}
		const String label = vformat(TTR("Playback %d: %s"), debug_selector->get_item_count() + 1, playback.graph_path.get_file());
		debug_selector->add_item(playback.waiting ? vformat(TTR("%s (waiting)"), label) : label, i);
		debug_selector->set_item_tooltip(debug_selector->get_item_count() - 1, vformat(TTR("FlowGraphPlayback %s in %s"), String::num_uint64((uint64_t)playback.id), playback.graph_path));
		if (&playback == selected) {
			debug_selector->select(debug_selector->get_item_count() - 1);
		}
	}
	if (selected == nullptr && debug_selector->get_item_count() > 0) {
		selected = &debug_playbacks[debug_selector->get_item_id(0)];
		debug_selected_session = selected->session;
		debug_selected_id = selected->id;
		debug_selector->select(0);
	}
	debug_selector->set_visible(debug_selector->get_item_count() > 0);
	debug_follow->set_visible(debug_selector->get_item_count() > 0);
	if (!debug_follow->is_visible()) {
		debug_follow->set_pressed_no_signal(false); // The next run starts unfollowed, and opened graphs frame their view again.
	}

	StringName active;
	if (selected != nullptr && !path.is_empty() && selected->current_graph_path == path && selected->node_id != FlowGraph::NODE_ID_NONE) {
		active = "n" + itos(selected->node_id);
	} else if (selected != nullptr && !path.is_empty() && selected->graph_path == path) {
		// Inside a subgraph: highlight the Subgraph node that leads there.
		for (const int id : graph->get_node_ids()) {
			const Ref<FlowSubgraphNode> subgraph = graph->get_node(id);
			if (subgraph.is_valid() && subgraph->get_graph().is_valid() && subgraph->get_graph()->get_path() == selected->current_graph_path) {
				active = "n" + itos(id);
				break;
			}
		}
	}
	if (active == debug_highlighted) {
		return;
	}
	if (GraphNode *previous = Object::cast_to<GraphNode>(graph_edit->get_node_or_null(NodePath(debug_highlighted)))) {
		previous->remove_theme_style_override(SNAME("titlebar"));
		previous->remove_theme_style_override(SNAME("titlebar_selected"));
	}
	debug_highlighted = active;
	GraphNode *current = Object::cast_to<GraphNode>(graph_edit->get_node_or_null(NodePath(active)));
	if (current == nullptr) {
		return;
	}
	const Ref<StyleBox> titlebar = current->get_theme_stylebox(SNAME("titlebar"));
	Ref<StyleBoxFlat> style;
	if (titlebar.is_valid()) {
		style = titlebar->duplicate();
	}
	if (style.is_null()) {
		style.instantiate();
	}
	style->set_bg_color(get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
	current->add_theme_style_override(SNAME("titlebar"), style);
	current->add_theme_style_override(SNAME("titlebar_selected"), style);
	if (follow) {
		graph_edit->set_scroll_offset((current->get_position_offset() + current->get_size() * 0.5) * graph_edit->get_zoom() - graph_edit->get_size() * 0.5);
	}
}

void FlowGraphEditor::debug_playback_changed(int p_session, const Array &p_data) {
	ERR_FAIL_COND(p_data.size() != FlowGraphDebugger::FIELD_MAX);
	const int64_t id = p_data[FlowGraphDebugger::FIELD_ID];
	int index = -1;
	for (uint32_t i = 0; i < debug_playbacks.size(); i++) {
		if (debug_playbacks[i].session == p_session && debug_playbacks[i].id == id) {
			index = i;
		}
	}
	if (!(bool)p_data[FlowGraphDebugger::FIELD_PLAYING]) {
		if (index >= 0) {
			debug_playbacks.remove_at(index);
		}
	} else {
		if (index < 0) {
			index = debug_playbacks.size();
			debug_playbacks.push_back(DebugPlayback());
		}
		DebugPlayback &playback = debug_playbacks[index];
		playback.session = p_session;
		playback.id = id;
		playback.graph_path = p_data[FlowGraphDebugger::FIELD_GRAPH_PATH];
		playback.current_graph_path = p_data[FlowGraphDebugger::FIELD_CURRENT_GRAPH_PATH];
		playback.node_id = p_data[FlowGraphDebugger::FIELD_NODE_ID];
		playback.waiting = p_data[FlowGraphDebugger::FIELD_WAITING];
	}
	_debug_update();
}

void FlowGraphEditor::debug_session_stopped(int p_session) {
	for (int i = (int)debug_playbacks.size() - 1; i >= 0; i--) {
		if (debug_playbacks[i].session == p_session) {
			debug_playbacks.remove_at(i);
		}
	}
	_debug_update();
}

void FlowGraphDebuggerPlugin::setup_session(int p_session) {
	get_session(p_session)->connect("stopped", callable_mp(editor, &FlowGraphEditor::debug_session_stopped).bind(p_session));
}

bool FlowGraphDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session) {
	if (p_message != FlowGraphDebugger::MESSAGE) {
		return false;
	}
	editor->debug_playback_changed(p_session, p_data);
	return true;
}

bool FlowGraphDebuggerPlugin::has_capture(const String &p_capture) const {
	return p_capture == FlowGraphDebugger::CAPTURE;
}

// Inspector button.

void EditorInspectorPluginFlowGraph::_open(const Ref<FlowGraph> &p_graph) {
	editor->edit(p_graph);
	editor->make_visible();
}

bool EditorInspectorPluginFlowGraph::can_handle(Object *p_object) {
	return Object::cast_to<FlowGraph>(p_object) != nullptr;
}

void EditorInspectorPluginFlowGraph::parse_begin(Object *p_object) {
	Button *button = memnew(Button);
	button->set_text(TTR("Open Graph Editor"));
	button->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorPluginFlowGraph::_open).bind(Ref<FlowGraph>(Object::cast_to<FlowGraph>(p_object))));
	add_custom_control(button);
}

// Plugin.

void FlowGraphEditorPlugin::edit(Object *p_object) {
	editor->edit(Ref<FlowGraph>(Object::cast_to<FlowGraph>(p_object)));
}

bool FlowGraphEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<FlowGraph>(p_object) != nullptr;
}

void FlowGraphEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_debugger_plugin(debugger_plugin);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_debugger_plugin(debugger_plugin);
		} break;
	}
}

void FlowGraphEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		editor->make_visible();
	} else {
		editor->close();
	}
}

bool FlowGraphEditorPlugin::can_auto_hide() const {
	// While a graph is open, the inspector shows its nodes; that must not close the dock (same as ThemeEditorPlugin).
	return editor->get_edited_graph().is_null();
}

FlowGraphEditorPlugin::FlowGraphEditorPlugin() {
	editor = memnew(FlowGraphEditor);
	editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	EditorDockManager::get_singleton()->add_dock(editor);
	editor->close();

	Ref<EditorInspectorPluginFlowGraph> inspector_plugin;
	inspector_plugin.instantiate(editor);
	add_inspector_plugin(inspector_plugin);

	debugger_plugin.instantiate(editor);
}
