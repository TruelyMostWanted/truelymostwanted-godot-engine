/**************************************************************************/
/*  flow_nodes.cpp                                                        */
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

#include "flow_nodes.h"

#include "core/math/expression.h"
#include "core/object/class_db.h"

static const StringName &_port_in() {
	static const StringName name = "in";
	return name;
}

static const StringName &_port_out() {
	static const StringName name = "out";
	return name;
}

static TypedArray<FlowPort> _flow_in() {
	TypedArray<FlowPort> ports;
	ports.push_back(FlowPort::create_flow(_port_in()));
	return ports;
}

static TypedArray<FlowPort> _flow_out() {
	TypedArray<FlowPort> ports;
	ports.push_back(FlowPort::create_flow(_port_out()));
	return ports;
}

// FlowStartNode.

TypedArray<FlowPort> FlowStartNode::_create_output_ports() const {
	return _flow_out();
}

// FlowEndNode.

TypedArray<FlowPort> FlowEndNode::_create_input_ports() const {
	return _flow_in();
}

PackedStringArray FlowEndNode::_get_default_inline_properties() const {
	return PackedStringArray({ "end_name" });
}

void FlowEndNode::set_end_name(const StringName &p_name) {
	end_name = p_name;
	emit_changed();
}

void FlowEndNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_end_name", "name"), &FlowEndNode::set_end_name);
	ClassDB::bind_method(D_METHOD("get_end_name"), &FlowEndNode::get_end_name);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "end_name"), "set_end_name", "get_end_name");
}

// FlowBranchNode.

TypedArray<FlowPort> FlowBranchNode::_create_input_ports() const {
	TypedArray<FlowPort> ports = _flow_in();
	ports.push_back(FlowPort::create_data("condition", Variant::BOOL, false));
	return ports;
}

TypedArray<FlowPort> FlowBranchNode::_create_output_ports() const {
	TypedArray<FlowPort> ports;
	ports.push_back(FlowPort::create_flow("true"));
	ports.push_back(FlowPort::create_flow("false"));
	return ports;
}

// FlowExpressionNode.

TypedArray<FlowPort> FlowExpressionNode::_create_input_ports() const {
	TypedArray<FlowPort> ports;
	for (const String &input_name : input_names) {
		ports.push_back(FlowPort::create_data(input_name, Variant::NIL));
	}
	return ports;
}

TypedArray<FlowPort> FlowExpressionNode::_create_output_ports() const {
	TypedArray<FlowPort> ports;
	ports.push_back(FlowPort::create_data("result", result_type));
	return ports;
}

PackedStringArray FlowExpressionNode::_get_default_inline_properties() const {
	return PackedStringArray({ "expression" });
}

PackedStringArray FlowExpressionNode::_get_default_configuration_warnings() const {
	PackedStringArray warnings;
	if (expression.strip_edges().is_empty()) {
		warnings.push_back(RTR("The expression is empty."));
		return warnings;
	}
	Ref<Expression> parser;
	parser.instantiate();
	if (parser->parse(expression, input_names) != OK) {
		warnings.push_back(vformat(RTR("The expression can't be parsed: %s"), parser->get_error_text()));
	}
	return warnings;
}

void FlowExpressionNode::set_expression(const String &p_expression) {
	expression = p_expression;
	emit_changed();
}

void FlowExpressionNode::set_input_names(const PackedStringArray &p_names) {
	input_names = p_names;
	notify_ports_changed();
}

void FlowExpressionNode::set_result_type(Variant::Type p_type) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	result_type = p_type;
	notify_ports_changed();
}

void FlowExpressionNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_expression", "expression"), &FlowExpressionNode::set_expression);
	ClassDB::bind_method(D_METHOD("get_expression"), &FlowExpressionNode::get_expression);
	ClassDB::bind_method(D_METHOD("set_input_names", "names"), &FlowExpressionNode::set_input_names);
	ClassDB::bind_method(D_METHOD("get_input_names"), &FlowExpressionNode::get_input_names);
	ClassDB::bind_method(D_METHOD("set_result_type", "type"), &FlowExpressionNode::set_result_type);
	ClassDB::bind_method(D_METHOD("get_result_type"), &FlowExpressionNode::get_result_type);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "expression", PROPERTY_HINT_EXPRESSION), "set_expression", "get_expression");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "input_names"), "set_input_names", "get_input_names");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "result_type", PROPERTY_HINT_ENUM, FlowPort::get_type_hint_string()), "set_result_type", "get_result_type");
}

// FlowVariableNode.

PackedStringArray FlowVariableNode::_get_default_inline_properties() const {
	return PackedStringArray({ "variable_name" });
}

PackedStringArray FlowVariableNode::_get_default_configuration_warnings() const {
	PackedStringArray warnings;
	if (variable_name.is_empty()) {
		warnings.push_back(RTR("No variable name is set."));
	}
	return warnings;
}

void FlowVariableNode::set_variable_name(const StringName &p_name) {
	variable_name = p_name;
	emit_changed();
}

void FlowVariableNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_variable_name", "name"), &FlowVariableNode::set_variable_name);
	ClassDB::bind_method(D_METHOD("get_variable_name"), &FlowVariableNode::get_variable_name);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "variable_name"), "set_variable_name", "get_variable_name");
}

// FlowGetVariableNode.

TypedArray<FlowPort> FlowGetVariableNode::_create_output_ports() const {
	TypedArray<FlowPort> ports;
	ports.push_back(FlowPort::create_data("value", Variant::NIL));
	return ports;
}

// FlowSetVariableNode.

TypedArray<FlowPort> FlowSetVariableNode::_create_input_ports() const {
	TypedArray<FlowPort> ports = _flow_in();
	ports.push_back(FlowPort::create_data("value", Variant::NIL));
	return ports;
}

TypedArray<FlowPort> FlowSetVariableNode::_create_output_ports() const {
	return _flow_out();
}

// FlowEmitEventNode.

TypedArray<FlowPort> FlowEmitEventNode::_create_input_ports() const {
	return _flow_in();
}

TypedArray<FlowPort> FlowEmitEventNode::_create_output_ports() const {
	return _flow_out();
}

PackedStringArray FlowEmitEventNode::_get_default_inline_properties() const {
	return PackedStringArray({ "event_name" });
}

PackedStringArray FlowEmitEventNode::_get_default_configuration_warnings() const {
	PackedStringArray warnings;
	if (event_name.is_empty()) {
		warnings.push_back(RTR("No event name is set."));
	}
	return warnings;
}

void FlowEmitEventNode::set_event_name(const StringName &p_name) {
	event_name = p_name;
	emit_changed();
}

void FlowEmitEventNode::set_arguments(const Array &p_arguments) {
	arguments = p_arguments;
	emit_changed();
}

void FlowEmitEventNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_event_name", "name"), &FlowEmitEventNode::set_event_name);
	ClassDB::bind_method(D_METHOD("get_event_name"), &FlowEmitEventNode::get_event_name);
	ClassDB::bind_method(D_METHOD("set_arguments", "arguments"), &FlowEmitEventNode::set_arguments);
	ClassDB::bind_method(D_METHOD("get_arguments"), &FlowEmitEventNode::get_arguments);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "event_name"), "set_event_name", "get_event_name");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "arguments"), "set_arguments", "get_arguments");
}

// FlowSubgraphNode.

TypedArray<FlowPort> FlowSubgraphNode::_create_input_ports() const {
	return _flow_in();
}

TypedArray<FlowPort> FlowSubgraphNode::_create_output_ports() const {
	return _flow_out();
}

PackedStringArray FlowSubgraphNode::_get_default_inline_properties() const {
	return PackedStringArray({ "graph" });
}

PackedStringArray FlowSubgraphNode::_get_default_configuration_warnings() const {
	PackedStringArray warnings;
	if (graph.is_null()) {
		warnings.push_back(RTR("No graph is set."));
	}
	return warnings;
}

void FlowSubgraphNode::set_graph(const Ref<FlowGraph> &p_graph) {
	graph = p_graph;
	emit_changed();
}

void FlowSubgraphNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_graph", "graph"), &FlowSubgraphNode::set_graph);
	ClassDB::bind_method(D_METHOD("get_graph"), &FlowSubgraphNode::get_graph);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "graph", PROPERTY_HINT_RESOURCE_TYPE, "FlowGraph"), "set_graph", "get_graph");
}

// FlowWaitNode.

TypedArray<FlowPort> FlowWaitNode::_create_input_ports() const {
	return _flow_in();
}

TypedArray<FlowPort> FlowWaitNode::_create_output_ports() const {
	return _flow_out();
}

PackedStringArray FlowWaitNode::_get_default_inline_properties() const {
	return PackedStringArray({ "seconds" });
}

void FlowWaitNode::set_seconds(double p_seconds) {
	seconds = MAX(p_seconds, 0.0);
	emit_changed();
}

void FlowWaitNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_seconds", "seconds"), &FlowWaitNode::set_seconds);
	ClassDB::bind_method(D_METHOD("get_seconds"), &FlowWaitNode::get_seconds);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "seconds", PROPERTY_HINT_RANGE, "0,3600,0.01,or_greater,suffix:s"), "set_seconds", "get_seconds");
}
