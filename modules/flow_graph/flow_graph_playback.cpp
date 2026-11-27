/**************************************************************************/
/*  flow_graph_playback.cpp                                               */
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

#include "flow_graph_playback.h"

#include "flow_graph.h"
#include "flow_nodes.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

// FlowExit.

void FlowExit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_port", "port"), &FlowExit::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &FlowExit::get_port);
	ClassDB::bind_method(D_METHOD("set_label", "label"), &FlowExit::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &FlowExit::get_label);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &FlowExit::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &FlowExit::is_enabled);
	ClassDB::bind_method(D_METHOD("set_node_id", "node_id"), &FlowExit::set_node_id);
	ClassDB::bind_method(D_METHOD("get_node_id"), &FlowExit::get_node_id);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "node_id"), "set_node_id", "get_node_id");
}

// FlowGraphState.

void FlowGraphState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_graph_path", "path"), &FlowGraphState::set_graph_path);
	ClassDB::bind_method(D_METHOD("get_graph_path"), &FlowGraphState::get_graph_path);
	ClassDB::bind_method(D_METHOD("set_node_stack", "stack"), &FlowGraphState::set_node_stack);
	ClassDB::bind_method(D_METHOD("get_node_stack"), &FlowGraphState::get_node_stack);
	ClassDB::bind_method(D_METHOD("set_node_memory", "memory"), &FlowGraphState::set_node_memory);
	ClassDB::bind_method(D_METHOD("get_node_memory"), &FlowGraphState::get_node_memory);
	ClassDB::bind_method(D_METHOD("set_history", "history"), &FlowGraphState::set_history);
	ClassDB::bind_method(D_METHOD("get_history"), &FlowGraphState::get_history);
	ClassDB::bind_method(D_METHOD("set_variables", "variables"), &FlowGraphState::set_variables);
	ClassDB::bind_method(D_METHOD("get_variables"), &FlowGraphState::get_variables);
	ClassDB::bind_method(D_METHOD("set_waiting", "waiting"), &FlowGraphState::set_waiting);
	ClassDB::bind_method(D_METHOD("is_waiting"), &FlowGraphState::is_waiting);
	ClassDB::bind_method(D_METHOD("set_layout_version", "version"), &FlowGraphState::set_layout_version);
	ClassDB::bind_method(D_METHOD("get_layout_version"), &FlowGraphState::get_layout_version);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "graph_path"), "set_graph_path", "get_graph_path");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "node_stack"), "set_node_stack", "get_node_stack");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "node_memory"), "set_node_memory", "get_node_memory");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "history"), "set_history", "get_history");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "variables"), "set_variables", "get_variables");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "waiting"), "set_waiting", "is_waiting");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layout_version"), "set_layout_version", "get_layout_version");

	BIND_CONSTANT(STATE_LAYOUT_VERSION);
}

// FlowGraphPlayback: helpers.

void FlowGraphPlayback::_variable_changed(const StringName &p_name, const Variant &p_value) {
	emit_signal(SNAME("variable_changed"), p_name, p_value);
}

void FlowGraphPlayback::_notify_debugger() {
	if (debug_hook != nullptr) {
		debug_hook(this);
	}
}

void FlowGraphPlayback::_add_declared_variables(const Ref<FlowGraph> &p_graph) {
	const TypedArray<FlowVariable> declared = p_graph->get_variables();
	for (int i = 0; i < declared.size(); i++) {
		const Ref<FlowVariable> variable = declared[i];
		if (variable.is_valid() && !variable->get_variable_name().is_empty() && !blackboard->has_variable(variable->get_variable_name())) {
			blackboard->set_variable(variable->get_variable_name(), variable->get_default_value());
		}
	}
}

void FlowGraphPlayback::_fail(Error p_error, const String &p_message) {
	if (run_error == OK) {
		run_error = p_error;
		ERR_PRINT(p_message);
	}
}

uint64_t FlowGraphPlayback::_begin() {
	run_error = OK;
	return ++run_serial;
}

bool FlowGraphPlayback::_enter_signal(int p_id, const Ref<FlowNode> &p_node) {
	const uint64_t serial = run_serial;
	stack[stack.size() - 1].node_id = p_id;
	history.push_back(p_id);
	if (history.size() > HISTORY_MAX) {
		history.remove_at(0);
	}
	emit_signal(SNAME("node_entered"), p_id, p_node);
	_notify_debugger();
	return run_serial == serial;
}

StringName FlowGraphPlayback::_choose_current() {
	const Frame &frame = stack[stack.size() - 1];
	const Ref<FlowNode> node = frame.graph->get_node(frame.node_id);
	evaluations.push_back({ frame.graph, frame.node_id });
	const StringName port = node->choose_exit(this);
	evaluations.remove_at(evaluations.size() - 1);
	return port;
}

Variant FlowGraphPlayback::_read_input(const Ref<FlowGraph> &p_graph, int p_node_id, const StringName &p_port) {
	if (run_error != OK) {
		return Variant();
	}
	int from_id = FlowGraph::NODE_ID_NONE;
	StringName from_port;
	if (p_graph->get_data_source(p_node_id, p_port, from_id, from_port)) {
		if ((int)evaluations.size() >= max_data_depth) {
			_fail(ERR_CYCLIC_LINK, vformat("FlowGraphPlayback: a data pull went deeper than max_data_depth (%d) at node %d. Check the graph for a data cycle.", max_data_depth, from_id));
			return Variant();
		}
		const Ref<FlowNode> source = p_graph->get_node(from_id);
		evaluations.push_back({ p_graph, from_id });
		const Variant value = source->evaluate_output(this, from_port);
		evaluations.remove_at(evaluations.size() - 1);
		return value;
	}
	const Ref<FlowNode> node = p_graph->get_node(p_node_id);
	// An unconnected input of an expression reads the variable of the same name.
	if (Object::cast_to<FlowExpressionNode>(node.ptr()) != nullptr && blackboard->has_variable(p_port)) {
		return blackboard->get_variable(p_port);
	}
	const Ref<FlowPort> port = node->find_input_port(p_port);
	ERR_FAIL_COND_V_MSG(port.is_null(), Variant(), vformat("FlowGraphPlayback: node %d has no input '%s'.", p_node_id, p_port));
	return port->get_default_value();
}

// FlowGraphPlayback: run loop. A public call that moves the playback starts a run with a new serial; a run that sees
// a newer serial (a signal handler or node started, stopped or moved the playback) returns without touching the state.

Error FlowGraphPlayback::_run(uint64_t p_serial, int p_next_id) {
	int next = p_next_id;
	int steps = 0;
	while (next != FlowGraph::NODE_ID_NONE && run_serial == p_serial && run_error == OK) {
		if (++steps > max_steps_per_advance) {
			_fail(ERR_CYCLIC_LINK, vformat("FlowGraphPlayback: more than max_steps_per_advance (%d) nodes in one call. Check the graph for a flow loop without a waiting node.", max_steps_per_advance));
			break;
		}
		next = _step(next);
	}
	if (run_serial != p_serial) {
		return OK;
	}
	const Error err = run_error;
	run_error = OK;
	if (err != OK) {
		stop();
	}
	return err;
}

int FlowGraphPlayback::_step(int p_id) {
	const uint64_t serial = run_serial;
	const Ref<FlowGraph> current = stack[stack.size() - 1].graph;
	if (!current->has_node(p_id)) {
		_fail(ERR_DOES_NOT_EXIST, vformat("FlowGraphPlayback: unknown node id %d.", p_id));
		return FlowGraph::NODE_ID_NONE;
	}
	const Ref<FlowNode> node = current->get_node(p_id);
	if (!_enter_signal(p_id, node)) {
		return FlowGraph::NODE_ID_NONE;
	}

	const FlowSubgraphNode *subgraph = Object::cast_to<FlowSubgraphNode>(node.ptr());
	if (subgraph != nullptr) {
		const Ref<FlowGraph> inner = subgraph->get_graph();
		if (inner.is_null() || !inner->has_node(inner->get_start_node_id())) {
			_fail(ERR_INVALID_DATA, vformat("FlowGraphPlayback: the subgraph node %d has no graph, or its graph has no start node.", p_id));
			return FlowGraph::NODE_ID_NONE;
		}
		if ((int)stack.size() >= max_subgraph_depth) {
			_fail(ERR_CYCLIC_LINK, vformat("FlowGraphPlayback: subgraphs nested deeper than max_subgraph_depth (%d) at node %d. Check for a graph that contains itself.", max_subgraph_depth, p_id));
			return FlowGraph::NODE_ID_NONE;
		}
		Frame frame;
		frame.graph = inner;
		stack.push_back(frame);
		_add_declared_variables(inner);
		return run_serial == serial ? inner->get_start_node_id() : FlowGraph::NODE_ID_NONE;
	}

	evaluations.push_back({ current, p_id });
	const EnterResult result = node->enter(this);
	const StringName port = (result == ENTER_RESULT_CONTINUE && run_serial == serial) ? node->choose_exit(this) : StringName();
	evaluations.remove_at(evaluations.size() - 1);
	if (run_serial != serial || run_error != OK) {
		return FlowGraph::NODE_ID_NONE;
	}
	if (result == ENTER_RESULT_WAIT) {
		waiting = true;
		emit_signal(SNAME("node_waiting"), p_id, node);
		_notify_debugger();
		return FlowGraph::NODE_ID_NONE;
	}
	if (!_exit_current(port)) {
		return FlowGraph::NODE_ID_NONE;
	}
	if (result == ENTER_RESULT_END) {
		const FlowEndNode *end = Object::cast_to<FlowEndNode>(node.ptr());
		return _end_frame(end != nullptr ? end->get_end_name() : StringName());
	}
	return _follow(port);
}

bool FlowGraphPlayback::_exit_current(const StringName &p_port) {
	const uint64_t serial = run_serial;
	const Frame &frame = stack[stack.size() - 1];
	const int id = frame.node_id;
	const Ref<FlowNode> node = frame.graph->get_node(id);
	evaluations.push_back({ frame.graph, id });
	node->exit(this, p_port);
	evaluations.remove_at(evaluations.size() - 1);
	if (run_serial != serial) {
		return false;
	}
	emit_signal(SNAME("node_exited"), id, node);
	return run_serial == serial;
}

int FlowGraphPlayback::_follow(const StringName &p_port) {
	const Frame &frame = stack[stack.size() - 1];
	const int target = p_port.is_empty() ? FlowGraph::NODE_ID_NONE : frame.graph->get_flow_target(frame.node_id, p_port);
	// A flow output without a connection ends the graph like an end node.
	return target != FlowGraph::NODE_ID_NONE ? target : _end_frame(StringName());
}

int FlowGraphPlayback::_end_frame(const StringName &p_end_name) {
	if (stack.size() == 1) {
		const int end_id = stack[0].node_id;
		playing = false;
		waiting = false;
		stack.clear();
		emit_signal(SNAME("finished"), end_id);
		_notify_debugger();
		return FlowGraph::NODE_ID_NONE;
	}
	// Back at the subgraph node of the outer graph.
	stack.remove_at(stack.size() - 1);
	const Frame &frame = stack[stack.size() - 1];
	const FlowSubgraphNode *subgraph = Object::cast_to<FlowSubgraphNode>(frame.graph->get_node(frame.node_id).ptr());
	const StringName port = (subgraph != nullptr && !p_end_name.is_empty() && subgraph->get_exit_names().has(String(p_end_name))) ? p_end_name : SNAME("out");
	if (!_exit_current(port)) {
		return FlowGraph::NODE_ID_NONE;
	}
	return _follow(port);
}

// FlowGraphPlayback: control.

Error FlowGraphPlayback::start(const Ref<FlowGraph> &p_graph, int p_from_id) {
	ERR_FAIL_COND_V(p_graph.is_null(), ERR_INVALID_PARAMETER);
	const int from_id = p_from_id == START_NODE_DEFAULT ? p_graph->get_start_node_id() : p_from_id;
	ERR_FAIL_COND_V_MSG(!p_graph->has_node(from_id), ERR_INVALID_DATA, vformat("FlowGraphPlayback: the graph has no node %d to start from (start_node_id or from_id).", from_id));
	if (validated_graph != p_graph->get_instance_id()) {
		validated_graph = p_graph->get_instance_id();
		const Ref<FlowGraphValidation> validation = p_graph->validate();
		for (int i = 0; i < validation->get_issue_count(); i++) {
			WARN_PRINT(vformat("FlowGraph %s, node %d: %s", p_graph->get_path(), validation->get_issue_node_id(i), validation->get_issue_message(i)));
		}
	}
	const uint64_t serial = _begin();
	graph = p_graph;
	stack.clear();
	Frame frame;
	frame.graph = p_graph;
	stack.push_back(frame);
	history.clear();
	playing = true;
	waiting = false;
	_add_declared_variables(p_graph);
	if (run_serial == serial) {
		emit_signal(SNAME("started"));
	}
	return run_serial == serial ? _run(serial, from_id) : OK;
}

Error FlowGraphPlayback::advance() {
	ERR_FAIL_COND_V_MSG(!waiting, ERR_UNAVAILABLE, "FlowGraphPlayback: advance() needs a waiting node.");
	const uint64_t serial = _begin();
	waiting = false;
	const StringName port = _choose_current();
	if (run_serial != serial || !_exit_current(port)) {
		return OK;
	}
	return _run(serial, _follow(port));
}

Error FlowGraphPlayback::advance_to_exit(const StringName &p_port) {
	ERR_FAIL_COND_V_MSG(!waiting, ERR_UNAVAILABLE, "FlowGraphPlayback: advance_to_exit() needs a waiting node.");
	const Ref<FlowPort> output = get_current_node()->find_output_port(p_port);
	ERR_FAIL_COND_V_MSG(output.is_null() || output->get_kind() != FlowPort::PORT_KIND_FLOW, ERR_INVALID_PARAMETER, vformat("FlowGraphPlayback: the current node has no flow output '%s'.", p_port));
	const uint64_t serial = _begin();
	waiting = false;
	if (!_exit_current(p_port)) {
		return OK;
	}
	return _run(serial, _follow(p_port));
}

TypedArray<FlowExit> FlowGraphPlayback::get_available_exits() {
	ERR_FAIL_COND_V_MSG(stack.is_empty(), TypedArray<FlowExit>(), "FlowGraphPlayback: not playing.");
	const Ref<FlowGraph> current = stack[stack.size() - 1].graph;
	const int id = stack[stack.size() - 1].node_id;
	evaluations.push_back({ current, id });
	const TypedArray<FlowExit> exits = current->get_node(id)->get_available_exits(this);
	evaluations.remove_at(evaluations.size() - 1);
	for (int i = 0; i < exits.size(); i++) {
		const Ref<FlowExit> flow_exit = exits[i];
		if (flow_exit.is_valid() && flow_exit->get_node_id() == FlowGraph::NODE_ID_NONE) {
			flow_exit->set_node_id(current->get_flow_target(id, flow_exit->get_port()));
		}
	}
	return exits;
}

Error FlowGraphPlayback::choose(int p_exit_index) {
	const TypedArray<FlowExit> exits = get_available_exits();
	ERR_FAIL_INDEX_V(p_exit_index, exits.size(), ERR_PARAMETER_RANGE_ERROR);
	const Ref<FlowExit> chosen = exits[p_exit_index];
	ERR_FAIL_COND_V(chosen.is_null(), ERR_INVALID_DATA);
	ERR_FAIL_COND_V_MSG(!chosen->is_enabled(), ERR_UNAVAILABLE, vformat("FlowGraphPlayback: the exit '%s' is disabled.", chosen->get_port()));
	return advance_to_exit(chosen->get_port());
}

Error FlowGraphPlayback::travel(int p_to_id, TravelMode p_mode) {
	ERR_FAIL_COND_V_MSG(!playing, ERR_UNCONFIGURED, "FlowGraphPlayback: travel() needs a playing graph.");
	ERR_FAIL_INDEX_V(p_mode, TRAVEL_MODE_MAX, ERR_INVALID_PARAMETER);
	const Ref<FlowGraph> current = stack[stack.size() - 1].graph;
	ERR_FAIL_COND_V_MSG(!current->has_node(p_to_id), ERR_DOES_NOT_EXIST, vformat("FlowGraphPlayback: unknown node id %d.", p_to_id));
	PackedInt32Array path;
	if (p_mode == TRAVEL_MODE_WALK) {
		path = get_travel_path(p_to_id);
		ERR_FAIL_COND_V_MSG(path.is_empty(), ERR_CANT_RESOLVE, vformat("FlowGraphPlayback: no flow path from node %d to node %d.", get_current_node_id(), p_to_id));
	}
	const uint64_t serial = _begin();
	waiting = false;
	if (!_exit_current()) {
		return OK;
	}
	// Walk: every node between the current node and the target is entered and exited; waits are skipped.
	for (int i = 1; i < path.size() - 1; i++) {
		const Ref<FlowNode> node = current->get_node(path[i]);
		if (!_enter_signal(path[i], node)) {
			return OK;
		}
		if (Object::cast_to<FlowSubgraphNode>(node.ptr()) == nullptr) {
			evaluations.push_back({ current, path[i] });
			node->enter(this);
			evaluations.remove_at(evaluations.size() - 1);
		}
		if (run_serial != serial || !_exit_current()) {
			return OK;
		}
	}
	return _run(serial, p_to_id);
}

PackedInt32Array FlowGraphPlayback::get_travel_path(int p_to_id) const {
	ERR_FAIL_COND_V_MSG(stack.is_empty(), PackedInt32Array(), "FlowGraphPlayback: not playing.");
	const Frame &frame = stack[stack.size() - 1];
	return frame.graph->find_flow_path(frame.node_id, p_to_id);
}

void FlowGraphPlayback::stop() {
	if (!playing) {
		return;
	}
	run_serial++;
	playing = false;
	waiting = false;
	stack.clear();
	emit_signal(SNAME("stopped"));
	_notify_debugger();
}

void FlowGraphPlayback::process(double p_delta) {
	if (!is_processing_needed()) {
		return;
	}
	const uint64_t serial = run_serial;
	const Frame &frame = stack[stack.size() - 1];
	const Ref<FlowNode> node = frame.graph->get_node(frame.node_id);
	evaluations.push_back({ frame.graph, frame.node_id });
	const bool done = node->process_waiting(this, p_delta);
	evaluations.remove_at(evaluations.size() - 1);
	if (done && waiting && run_serial == serial) {
		advance();
	}
}

bool FlowGraphPlayback::is_processing_needed() const {
	if (!waiting) {
		return false;
	}
	const Ref<FlowNode> node = get_current_node();
	return node.is_valid() && node->uses_process_waiting();
}

Ref<FlowGraph> FlowGraphPlayback::get_graph() const {
	return graph;
}

Ref<FlowGraph> FlowGraphPlayback::get_current_graph() const {
	return stack.is_empty() ? graph : stack[stack.size() - 1].graph;
}

int FlowGraphPlayback::get_current_node_id() const {
	return stack.is_empty() ? FlowGraph::NODE_ID_NONE : stack[stack.size() - 1].node_id;
}

Ref<FlowNode> FlowGraphPlayback::get_current_node() const {
	if (stack.is_empty()) {
		return Ref<FlowNode>();
	}
	const Frame &frame = stack[stack.size() - 1];
	return frame.graph->has_node(frame.node_id) ? frame.graph->get_node(frame.node_id) : Ref<FlowNode>();
}

// FlowGraphPlayback: for nodes.

Variant FlowGraphPlayback::get_input_value(const StringName &p_port) {
	ERR_FAIL_COND_V_MSG(evaluations.is_empty(), Variant(), "FlowGraphPlayback: get_input_value() works only while a node is entered, processed or evaluated. Use get_node_input_value() otherwise.");
	const Evaluation top = evaluations[evaluations.size() - 1];
	return _read_input(top.graph, top.node_id, p_port);
}

Variant FlowGraphPlayback::get_node_input_value(int p_node_id, const StringName &p_port) {
	const Ref<FlowGraph> current = get_current_graph();
	ERR_FAIL_COND_V(current.is_null(), Variant());
	ERR_FAIL_COND_V_MSG(!current->has_node(p_node_id), Variant(), vformat("FlowGraphPlayback: unknown node id %d.", p_node_id));
	const bool outermost = evaluations.is_empty();
	evaluations.push_back({ current, p_node_id });
	const Variant value = _read_input(current, p_node_id, p_port);
	evaluations.remove_at(evaluations.size() - 1);
	if (outermost) {
		run_error = OK;
	}
	return value;
}

Variant FlowGraphPlayback::get_node_memory() const {
	ERR_FAIL_COND_V_MSG(evaluations.is_empty() || stack.is_empty(), Variant(), "FlowGraphPlayback: get_node_memory() works only while a node is entered, processed or evaluated.");
	return stack[stack.size() - 1].memory.get(evaluations[evaluations.size() - 1].node_id, Variant());
}

void FlowGraphPlayback::set_node_memory(const Variant &p_value) {
	ERR_FAIL_COND_MSG(evaluations.is_empty() || stack.is_empty(), "FlowGraphPlayback: set_node_memory() works only while a node is entered, processed or evaluated.");
	stack[stack.size() - 1].memory[evaluations[evaluations.size() - 1].node_id] = p_value;
}

void FlowGraphPlayback::emit_event(const StringName &p_name, const Array &p_arguments) {
	emit_signal(SNAME("event_emitted"), p_name, p_arguments);
}

void FlowGraphPlayback::set_blackboard(const Ref<FlowBlackboard> &p_blackboard) {
	const Callable callback = callable_mp(this, &FlowGraphPlayback::_variable_changed);
	if (blackboard.is_valid() && blackboard->is_connected(SNAME("variable_changed"), callback)) {
		blackboard->disconnect(SNAME("variable_changed"), callback);
	}
	blackboard = p_blackboard;
	if (blackboard.is_null()) {
		blackboard.instantiate();
	}
	blackboard->connect(SNAME("variable_changed"), callback);
}

Ref<FlowBlackboard> FlowGraphPlayback::get_blackboard() const {
	return blackboard;
}

void FlowGraphPlayback::set_variable(const StringName &p_name, const Variant &p_value) {
	blackboard->set_variable(p_name, p_value);
}

Variant FlowGraphPlayback::get_variable(const StringName &p_name) const {
	return blackboard->get_variable(p_name);
}

bool FlowGraphPlayback::has_variable(const StringName &p_name) const {
	return blackboard->has_variable(p_name);
}

// FlowGraphPlayback: state.

Ref<FlowGraphState> FlowGraphPlayback::get_state() const {
	Ref<FlowGraphState> state;
	state.instantiate();
	if (graph.is_valid() && !graph->get_path().is_empty()) {
		// uid:// survives moving the graph; only when the UID is registered, else it could not be loaded back.
		const ResourceUID::ID uid = ResourceLoader::get_resource_uid(graph->get_path());
		state->set_graph_path(uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid) ? ResourceUID::get_singleton()->id_to_text(uid) : graph->get_path());
	}
	PackedInt32Array node_stack;
	Array memory;
	for (const Frame &frame : stack) {
		node_stack.push_back(frame.node_id);
		memory.push_back(frame.memory.duplicate(true));
	}
	state->set_node_stack(node_stack);
	state->set_node_memory(memory);
	state->set_history(history);
	state->set_variables(blackboard->get_variables().duplicate(true));
	state->set_waiting(waiting);
	return state;
}

Error FlowGraphPlayback::set_state(const Ref<FlowGraphState> &p_state) {
	ERR_FAIL_COND_V(p_state.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_state->get_layout_version() > FlowGraphState::STATE_LAYOUT_VERSION, ERR_FILE_UNRECOGNIZED, "FlowGraphPlayback: the state was saved by a newer engine version.");
	Ref<FlowGraph> root = graph;
	const String path = p_state->get_graph_path();
	if (!path.is_empty() && (root.is_null() || root->get_path() != ResourceUID::ensure_path(path))) {
		root = ResourceLoader::load(path, "FlowGraph");
	}
	ERR_FAIL_COND_V_MSG(root.is_null(), ERR_CANT_OPEN, vformat("FlowGraphPlayback: can't get the graph '%s' of the state.", path));

	// Inner graphs follow from the subgraph nodes of the outer levels.
	const PackedInt32Array ids = p_state->get_node_stack();
	const Array memory = p_state->get_node_memory();
	LocalVector<Frame> frames;
	Ref<FlowGraph> level = root;
	for (int i = 0; i < ids.size(); i++) {
		ERR_FAIL_COND_V_MSG(level.is_null() || !level->has_node(ids[i]), ERR_INVALID_DATA, vformat("FlowGraphPlayback: the state names node %d, which its graph doesn't have.", ids[i]));
		Frame frame;
		frame.graph = level;
		frame.node_id = ids[i];
		if (i < memory.size() && memory[i].get_type() == Variant::DICTIONARY) {
			frame.memory = Dictionary(memory[i]).duplicate(true);
		}
		frames.push_back(frame);
		const Ref<FlowSubgraphNode> subgraph = level->get_node(ids[i]);
		level = subgraph.is_valid() ? subgraph->get_graph() : Ref<FlowGraph>();
	}

	const uint64_t serial = _begin();
	graph = root;
	stack = std::move(frames);
	history = p_state->get_history();
	blackboard->set_variables(p_state->get_variables());
	playing = !stack.is_empty();
	waiting = playing && p_state->is_waiting();
	_notify_debugger();
	if (waiting) {
		emit_signal(SNAME("node_waiting"), get_current_node_id(), get_current_node());
		return OK;
	}
	// Saved between two nodes (inside a signal handler): enter the current node again.
	return playing && run_serial == serial ? _run(serial, get_current_node_id()) : OK;
}

// FlowGraphPlayback: setup.

void FlowGraphPlayback::_notification(int p_what) {
	if (p_what == NOTIFICATION_PREDELETE && debug_hook != nullptr) {
		playing = false;
		waiting = false;
		debug_hook(this);
	}
}

void FlowGraphPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "graph", "from_id"), &FlowGraphPlayback::start, DEFVAL(START_NODE_DEFAULT));
	ClassDB::bind_method(D_METHOD("advance"), &FlowGraphPlayback::advance);
	ClassDB::bind_method(D_METHOD("advance_to_exit", "port"), &FlowGraphPlayback::advance_to_exit);
	ClassDB::bind_method(D_METHOD("get_available_exits"), &FlowGraphPlayback::get_available_exits);
	ClassDB::bind_method(D_METHOD("choose", "exit_index"), &FlowGraphPlayback::choose);
	ClassDB::bind_method(D_METHOD("travel", "to_id", "mode"), &FlowGraphPlayback::travel, DEFVAL(TRAVEL_MODE_WALK));
	ClassDB::bind_method(D_METHOD("get_travel_path", "to_id"), &FlowGraphPlayback::get_travel_path);
	ClassDB::bind_method(D_METHOD("stop"), &FlowGraphPlayback::stop);
	ClassDB::bind_method(D_METHOD("process", "delta"), &FlowGraphPlayback::process);

	ClassDB::bind_method(D_METHOD("is_playing"), &FlowGraphPlayback::is_playing);
	ClassDB::bind_method(D_METHOD("is_waiting"), &FlowGraphPlayback::is_waiting);
	ClassDB::bind_method(D_METHOD("get_graph"), &FlowGraphPlayback::get_graph);
	ClassDB::bind_method(D_METHOD("get_current_graph"), &FlowGraphPlayback::get_current_graph);
	ClassDB::bind_method(D_METHOD("get_current_node_id"), &FlowGraphPlayback::get_current_node_id);
	ClassDB::bind_method(D_METHOD("get_current_node"), &FlowGraphPlayback::get_current_node);
	ClassDB::bind_method(D_METHOD("get_graph_stack_depth"), &FlowGraphPlayback::get_graph_stack_depth);
	ClassDB::bind_method(D_METHOD("get_history"), &FlowGraphPlayback::get_history);

	ClassDB::bind_method(D_METHOD("get_input_value", "port"), &FlowGraphPlayback::get_input_value);
	ClassDB::bind_method(D_METHOD("get_node_input_value", "node_id", "port"), &FlowGraphPlayback::get_node_input_value);
	ClassDB::bind_method(D_METHOD("get_node_memory"), &FlowGraphPlayback::get_node_memory);
	ClassDB::bind_method(D_METHOD("set_node_memory", "value"), &FlowGraphPlayback::set_node_memory);
	ClassDB::bind_method(D_METHOD("emit_event", "name", "arguments"), &FlowGraphPlayback::emit_event, DEFVAL(Array()));

	ClassDB::bind_method(D_METHOD("set_blackboard", "blackboard"), &FlowGraphPlayback::set_blackboard);
	ClassDB::bind_method(D_METHOD("get_blackboard"), &FlowGraphPlayback::get_blackboard);
	ClassDB::bind_method(D_METHOD("set_variable", "name", "value"), &FlowGraphPlayback::set_variable);
	ClassDB::bind_method(D_METHOD("get_variable", "name"), &FlowGraphPlayback::get_variable);
	ClassDB::bind_method(D_METHOD("has_variable", "name"), &FlowGraphPlayback::has_variable);

	ClassDB::bind_method(D_METHOD("set_max_steps_per_advance", "max"), &FlowGraphPlayback::set_max_steps_per_advance);
	ClassDB::bind_method(D_METHOD("get_max_steps_per_advance"), &FlowGraphPlayback::get_max_steps_per_advance);
	ClassDB::bind_method(D_METHOD("set_max_data_depth", "max"), &FlowGraphPlayback::set_max_data_depth);
	ClassDB::bind_method(D_METHOD("get_max_data_depth"), &FlowGraphPlayback::get_max_data_depth);
	ClassDB::bind_method(D_METHOD("set_max_subgraph_depth", "max"), &FlowGraphPlayback::set_max_subgraph_depth);
	ClassDB::bind_method(D_METHOD("get_max_subgraph_depth"), &FlowGraphPlayback::get_max_subgraph_depth);

	ClassDB::bind_method(D_METHOD("get_state"), &FlowGraphPlayback::get_state);
	ClassDB::bind_method(D_METHOD("set_state", "state"), &FlowGraphPlayback::set_state);

	// Never null and not stored (like Node.multiplayer): no default-value comparison.
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "blackboard", PROPERTY_HINT_RESOURCE_TYPE, "FlowBlackboard", PROPERTY_USAGE_NONE), "set_blackboard", "get_blackboard");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_steps_per_advance", PROPERTY_HINT_RANGE, "1,65536,1,or_greater"), "set_max_steps_per_advance", "get_max_steps_per_advance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_data_depth", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), "set_max_data_depth", "get_max_data_depth");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_subgraph_depth", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_max_subgraph_depth", "get_max_subgraph_depth");

	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("node_entered", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("node_exited", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("node_waiting", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("event_emitted", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::ARRAY, "arguments")));
	ADD_SIGNAL(MethodInfo("variable_changed", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::INT, "end_node_id")));
	ADD_SIGNAL(MethodInfo("stopped"));

	BIND_CONSTANT(START_NODE_DEFAULT);

	BIND_ENUM_CONSTANT(ENTER_RESULT_CONTINUE);
	BIND_ENUM_CONSTANT(ENTER_RESULT_WAIT);
	BIND_ENUM_CONSTANT(ENTER_RESULT_END);
	BIND_ENUM_CONSTANT(ENTER_RESULT_MAX);

	BIND_ENUM_CONSTANT(TRAVEL_MODE_WALK);
	BIND_ENUM_CONSTANT(TRAVEL_MODE_TELEPORT);
	BIND_ENUM_CONSTANT(TRAVEL_MODE_MAX);
}

FlowGraphPlayback::FlowGraphPlayback() {
	set_blackboard(Ref<FlowBlackboard>());
}

FlowGraphPlayback::~FlowGraphPlayback() {
}
