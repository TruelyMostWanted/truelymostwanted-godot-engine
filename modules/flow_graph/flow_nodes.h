/**************************************************************************/
/*  flow_nodes.h                                                          */
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

#include "flow_graph.h"

#include "core/math/expression.h"

// Built-in node types. Their runtime behavior is implemented by FlowGraphPlayback.
// Port names: flow input "in", flow output "out".

// Entry point of a graph.
class FlowStartNode : public FlowNode {
	GDCLASS(FlowStartNode, FlowNode);

protected:
	static void _bind_methods() {}
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Flow"; }
};

// Ends the graph (or returns from a subgraph) with an optional end name.
class FlowEndNode : public FlowNode {
	GDCLASS(FlowEndNode, FlowNode);

	StringName end_name;

protected:
	static void _bind_methods();
	TypedArray<FlowPort> _create_input_ports() const override;
	String _get_default_category() const override { return "Flow"; }
	PackedStringArray _get_default_inline_properties() const override;
	FlowGraphPlayback::EnterResult _default_enter(FlowGraphPlayback *p_playback) const override { return FlowGraphPlayback::ENTER_RESULT_END; }

public:
	void set_end_name(const StringName &p_name);
	StringName get_end_name() const { return end_name; }
};

// Continues with the flow output "true" or "false" depending on the data input "condition".
class FlowBranchNode : public FlowNode {
	GDCLASS(FlowBranchNode, FlowNode);

protected:
	static void _bind_methods() {}
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Flow"; }
	StringName _default_choose_exit(FlowGraphPlayback *p_playback) const override;
};

// Evaluates a Godot Expression. Every name in input_names is a data input; the data output "result" holds the value.
class FlowExpressionNode : public FlowNode {
	GDCLASS(FlowExpressionNode, FlowNode);

	String expression;
	PackedStringArray input_names;
	Variant::Type result_type = Variant::NIL;
	mutable Ref<Expression> parsed; // Parsed once, dropped when expression or input_names change.

protected:
	static void _bind_methods();
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Data"; }
	PackedStringArray _get_default_inline_properties() const override;
	PackedStringArray _get_default_configuration_warnings() const override;
	Variant _default_evaluate_output(FlowGraphPlayback *p_playback, const StringName &p_port) const override;

public:
	void set_expression(const String &p_expression);
	String get_expression() const { return expression; }
	void set_input_names(const PackedStringArray &p_names);
	PackedStringArray get_input_names() const { return input_names; }
	void set_result_type(Variant::Type p_type);
	Variant::Type get_result_type() const { return result_type; }
};

// Base of the variable nodes: the name of a variable declared on the graph.
class FlowVariableNode : public FlowNode {
	GDCLASS(FlowVariableNode, FlowNode);

	StringName variable_name;

protected:
	static void _bind_methods();
	String _get_default_category() const override { return "Variables"; }
	PackedStringArray _get_default_inline_properties() const override;
	PackedStringArray _get_default_configuration_warnings() const override;

public:
	void set_variable_name(const StringName &p_name);
	StringName get_variable_name() const { return variable_name; }
};

// Reads a variable of the blackboard into the data output "value".
class FlowGetVariableNode : public FlowVariableNode {
	GDCLASS(FlowGetVariableNode, FlowVariableNode);

protected:
	static void _bind_methods() {}
	TypedArray<FlowPort> _create_output_ports() const override;
	Variant _default_evaluate_output(FlowGraphPlayback *p_playback, const StringName &p_port) const override;
};

// Writes the data input "value" into a variable of the blackboard.
class FlowSetVariableNode : public FlowVariableNode {
	GDCLASS(FlowSetVariableNode, FlowVariableNode);

protected:
	static void _bind_methods() {}
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	FlowGraphPlayback::EnterResult _default_enter(FlowGraphPlayback *p_playback) const override;
};

// Emits an event with arguments through the playback (signal event_emitted).
class FlowEmitEventNode : public FlowNode {
	GDCLASS(FlowEmitEventNode, FlowNode);

	StringName event_name;
	Array arguments;

protected:
	static void _bind_methods();
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Events"; }
	PackedStringArray _get_default_inline_properties() const override;
	PackedStringArray _get_default_configuration_warnings() const override;
	FlowGraphPlayback::EnterResult _default_enter(FlowGraphPlayback *p_playback) const override;

public:
	void set_event_name(const StringName &p_name);
	StringName get_event_name() const { return event_name; }
	void set_arguments(const Array &p_arguments);
	Array get_arguments() const { return arguments; }
};

// Runs another FlowGraph from its start node and continues when it ends: through the output named like the
// end_name of the inner end node if that name is in exit_names, else through "out".
class FlowSubgraphNode : public FlowNode {
	GDCLASS(FlowSubgraphNode, FlowNode);

	Ref<FlowGraph> graph;
	PackedStringArray exit_names;

protected:
	static void _bind_methods();
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Flow"; }
	PackedStringArray _get_default_inline_properties() const override;
	PackedStringArray _get_default_configuration_warnings() const override;

public:
	void set_graph(const Ref<FlowGraph> &p_graph);
	Ref<FlowGraph> get_graph() const { return graph; }
	void set_exit_names(const PackedStringArray &p_names);
	PackedStringArray get_exit_names() const { return exit_names; }
};

// Waits a number of seconds before it continues.
class FlowWaitNode : public FlowNode {
	GDCLASS(FlowWaitNode, FlowNode);

	double seconds = 1.0;

protected:
	static void _bind_methods();
	TypedArray<FlowPort> _create_input_ports() const override;
	TypedArray<FlowPort> _create_output_ports() const override;
	String _get_default_category() const override { return "Flow"; }
	PackedStringArray _get_default_inline_properties() const override;
	FlowGraphPlayback::EnterResult _default_enter(FlowGraphPlayback *p_playback) const override;
	bool _default_process_waiting(FlowGraphPlayback *p_playback, double p_delta) const override;
	bool _has_default_process_waiting() const override { return true; }

public:
	void set_seconds(double p_seconds);
	double get_seconds() const { return seconds; }
};
