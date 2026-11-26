/**************************************************************************/
/*  flow_node.cpp                                                         */
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

#include "flow_node.h"

#include "core/object/class_db.h"
#include "core/object/script_language.h"

void FlowNode::_update_ports() const {
	if (!ports_dirty) {
		return;
	}
	TypedArray<FlowPort> inputs;
	if (!GDVIRTUAL_CALL(_get_input_ports, inputs)) {
		inputs = _create_input_ports();
	}
	TypedArray<FlowPort> outputs;
	if (!GDVIRTUAL_CALL(_get_output_ports, outputs)) {
		outputs = _create_output_ports();
	}
	input_ports.clear();
	output_ports.clear();
	for (int i = 0; i < inputs.size(); i++) {
		const Ref<FlowPort> port = inputs[i];
		if (port.is_valid()) {
			input_ports.push_back(port);
		}
	}
	for (int i = 0; i < outputs.size(); i++) {
		const Ref<FlowPort> port = outputs[i];
		if (port.is_valid()) {
			output_ports.push_back(port);
		}
	}
	ports_dirty = false;
}

Ref<FlowPort> FlowNode::_find_port(const LocalVector<Ref<FlowPort>> &p_ports, const StringName &p_name) {
	for (const Ref<FlowPort> &port : p_ports) {
		if (port->get_port_name() == p_name) {
			return port;
		}
	}
	return Ref<FlowPort>();
}

TypedArray<FlowPort> FlowNode::_to_array(const LocalVector<Ref<FlowPort>> &p_ports) {
	TypedArray<FlowPort> result;
	for (const Ref<FlowPort> &port : p_ports) {
		result.push_back(port);
	}
	return result;
}

void FlowNode::set_title_override(const String &p_title) {
	title_override = p_title;
	emit_changed();
}

void FlowNode::set_editor_position(const Vector2 &p_position) {
	editor_position = p_position;
	emit_changed();
}

void FlowNode::set_editor_size(const Vector2 &p_size) {
	editor_size = p_size;
	emit_changed();
}

void FlowNode::set_editor_comment(const String &p_comment) {
	editor_comment = p_comment;
	emit_changed();
}

TypedArray<FlowPort> FlowNode::get_input_ports() const {
	_update_ports();
	return _to_array(input_ports);
}

TypedArray<FlowPort> FlowNode::get_output_ports() const {
	_update_ports();
	return _to_array(output_ports);
}

Ref<FlowPort> FlowNode::find_input_port(const StringName &p_name) const {
	_update_ports();
	return _find_port(input_ports, p_name);
}

Ref<FlowPort> FlowNode::find_output_port(const StringName &p_name) const {
	_update_ports();
	return _find_port(output_ports, p_name);
}

void FlowNode::notify_ports_changed() {
	ports_dirty = true;
	emit_changed();
}

bool FlowNode::has_flow_ports() const {
	_update_ports();
	for (const Ref<FlowPort> &port : input_ports) {
		if (port->get_kind() == FlowPort::PORT_KIND_FLOW) {
			return true;
		}
	}
	for (const Ref<FlowPort> &port : output_ports) {
		if (port->get_kind() == FlowPort::PORT_KIND_FLOW) {
			return true;
		}
	}
	return false;
}

String FlowNode::get_title() const {
	if (!title_override.is_empty()) {
		return title_override;
	}
	String title;
	if (GDVIRTUAL_CALL(_get_title, title) && !title.is_empty()) {
		return title;
	}
	const Ref<Script> node_script = get_script();
	if (node_script.is_valid() && !node_script->get_global_name().is_empty()) {
		title = node_script->get_global_name();
	} else {
		title = get_class();
	}
	if (title.begins_with("Flow") && title.length() > 4) {
		title = title.substr(4);
	}
	if (title.ends_with("Node") && title.length() > 4) {
		title = title.substr(0, title.length() - 4);
	}
	return title.capitalize();
}

String FlowNode::get_category() const {
	String category;
	if (GDVIRTUAL_CALL(_get_category, category)) {
		return category;
	}
	return _get_default_category();
}

TypedArray<PackedStringArray> FlowNode::get_translatable_strings() const {
	TypedArray<PackedStringArray> strings;
	GDVIRTUAL_CALL(_get_translatable_strings, strings);
	return strings;
}

PackedStringArray FlowNode::get_configuration_warnings() const {
	PackedStringArray warnings = _get_default_configuration_warnings();
	PackedStringArray script_warnings;
	if (GDVIRTUAL_CALL(_get_configuration_warnings, script_warnings)) {
		warnings.append_array(script_warnings);
	}
	return warnings;
}

PackedStringArray FlowNode::get_inline_properties() const {
	PackedStringArray properties;
	if (GDVIRTUAL_CALL(_get_inline_properties, properties)) {
		return properties;
	}
	return _get_default_inline_properties();
}

bool FlowNode::is_in_graph() const {
	return graph_id.is_valid() && ObjectDB::get_instance(graph_id) != nullptr;
}

void FlowNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title_override", "title"), &FlowNode::set_title_override);
	ClassDB::bind_method(D_METHOD("get_title_override"), &FlowNode::get_title_override);
	ClassDB::bind_method(D_METHOD("set_editor_position", "position"), &FlowNode::set_editor_position);
	ClassDB::bind_method(D_METHOD("get_editor_position"), &FlowNode::get_editor_position);
	ClassDB::bind_method(D_METHOD("set_editor_size", "size"), &FlowNode::set_editor_size);
	ClassDB::bind_method(D_METHOD("get_editor_size"), &FlowNode::get_editor_size);
	ClassDB::bind_method(D_METHOD("set_editor_comment", "comment"), &FlowNode::set_editor_comment);
	ClassDB::bind_method(D_METHOD("get_editor_comment"), &FlowNode::get_editor_comment);

	ClassDB::bind_method(D_METHOD("get_input_ports"), &FlowNode::get_input_ports);
	ClassDB::bind_method(D_METHOD("get_output_ports"), &FlowNode::get_output_ports);
	ClassDB::bind_method(D_METHOD("find_input_port", "name"), &FlowNode::find_input_port);
	ClassDB::bind_method(D_METHOD("find_output_port", "name"), &FlowNode::find_output_port);
	ClassDB::bind_method(D_METHOD("notify_ports_changed"), &FlowNode::notify_ports_changed);
	ClassDB::bind_method(D_METHOD("get_title"), &FlowNode::get_title);
	ClassDB::bind_method(D_METHOD("get_category"), &FlowNode::get_category);
	ClassDB::bind_method(D_METHOD("get_translatable_strings"), &FlowNode::get_translatable_strings);
	ClassDB::bind_method(D_METHOD("get_configuration_warnings"), &FlowNode::get_configuration_warnings);
	ClassDB::bind_method(D_METHOD("get_inline_properties"), &FlowNode::get_inline_properties);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title_override"), "set_title_override", "get_title_override");
	ADD_GROUP("Editor", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "editor_position", PROPERTY_HINT_NONE, "suffix:px"), "set_editor_position", "get_editor_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "editor_size", PROPERTY_HINT_NONE, "suffix:px"), "set_editor_size", "get_editor_size");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "editor_comment", PROPERTY_HINT_MULTILINE_TEXT), "set_editor_comment", "get_editor_comment");

	GDVIRTUAL_BIND(_get_input_ports);
	GDVIRTUAL_BIND(_get_output_ports);
	GDVIRTUAL_BIND(_get_title);
	GDVIRTUAL_BIND(_get_category);
	GDVIRTUAL_BIND(_get_translatable_strings);
	GDVIRTUAL_BIND(_get_configuration_warnings);
	GDVIRTUAL_BIND(_get_inline_properties);
}
