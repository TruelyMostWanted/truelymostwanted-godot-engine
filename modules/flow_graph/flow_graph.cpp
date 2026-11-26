/**************************************************************************/
/*  flow_graph.cpp                                                        */
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

#include "flow_graph.h"

#include "flow_nodes.h"

#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

// FlowGraphValidation.

void FlowGraphValidation::add_issue(IssueKind p_kind, int p_node_id, const String &p_message) {
	Issue issue;
	issue.kind = p_kind;
	issue.node_id = p_node_id;
	issue.message = p_message;
	issues.push_back(issue);
}

FlowGraphValidation::IssueKind FlowGraphValidation::get_issue_kind(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), ISSUE_KIND_MAX);
	return issues[p_index].kind;
}

int FlowGraphValidation::get_issue_node_id(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), FlowGraph::NODE_ID_NONE);
	return issues[p_index].node_id;
}

String FlowGraphValidation::get_issue_message(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)issues.size(), String());
	return issues[p_index].message;
}

PackedInt32Array FlowGraphValidation::get_issues_for_node(int p_node_id) const {
	PackedInt32Array result;
	for (uint32_t i = 0; i < issues.size(); i++) {
		if (issues[i].node_id == p_node_id) {
			result.push_back(i);
		}
	}
	return result;
}

void FlowGraphValidation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &FlowGraphValidation::is_valid);
	ClassDB::bind_method(D_METHOD("get_issue_count"), &FlowGraphValidation::get_issue_count);
	ClassDB::bind_method(D_METHOD("get_issue_kind", "index"), &FlowGraphValidation::get_issue_kind);
	ClassDB::bind_method(D_METHOD("get_issue_node_id", "index"), &FlowGraphValidation::get_issue_node_id);
	ClassDB::bind_method(D_METHOD("get_issue_message", "index"), &FlowGraphValidation::get_issue_message);
	ClassDB::bind_method(D_METHOD("get_issues_for_node", "node_id"), &FlowGraphValidation::get_issues_for_node);

	BIND_ENUM_CONSTANT(ISSUE_KIND_MISSING_START);
	BIND_ENUM_CONSTANT(ISSUE_KIND_UNREACHABLE_NODE);
	BIND_ENUM_CONSTANT(ISSUE_KIND_TYPE_MISMATCH);
	BIND_ENUM_CONSTANT(ISSUE_KIND_UNKNOWN_PORT);
	BIND_ENUM_CONSTANT(ISSUE_KIND_MULTIPLE_FLOW_OUTPUTS);
	BIND_ENUM_CONSTANT(ISSUE_KIND_UNDECLARED_VARIABLE);
	BIND_ENUM_CONSTANT(ISSUE_KIND_DATA_CYCLE);
	BIND_ENUM_CONSTANT(ISSUE_KIND_NODE_WARNING);
	BIND_ENUM_CONSTANT(ISSUE_KIND_MAX);
}

// FlowGraph: indexes.

void FlowGraph::_ensure_index() const {
	MutexLock lock(index_mutex);
	if (!index_dirty) {
		return;
	}
	node_index.clear();
	output_index.clear();
	input_index.clear();
	const int count = MIN((int)node_ids.size(), (int)nodes.size());
	for (int i = 0; i < count; i++) {
		if (nodes[i].is_valid()) {
			node_index.insert(node_ids[i], i);
		}
	}
	for (uint32_t i = 0; i < connections.size(); i++) {
		output_index[connections[i].from_id].push_back(i);
		input_index[connections[i].to_id].push_back(i);
	}
	index_dirty = false;
}

void FlowGraph::_invalidate_index() {
	MutexLock lock(index_mutex);
	index_dirty = true;
}

int FlowGraph::_find_connection(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const {
	_ensure_index();
	const LocalVector<uint32_t> *outputs = output_index.getptr(p_from_id);
	if (outputs == nullptr) {
		return -1;
	}
	for (uint32_t index : *outputs) {
		const Connection &c = connections[index];
		if (c.from_port == p_from_port && c.to_id == p_to_id && c.to_port == p_to_port) {
			return index;
		}
	}
	return -1;
}

FlowPort::PortKind FlowGraph::_get_connection_kind(const Connection &p_connection) const {
	_ensure_index();
	const int *from = node_index.getptr(p_connection.from_id);
	const Ref<FlowPort> port = from != nullptr ? nodes[*from]->find_output_port(p_connection.from_port) : Ref<FlowPort>();
	return port.is_valid() ? port->get_kind() : FlowPort::PORT_KIND_FLOW;
}

Ref<FlowConnection> FlowGraph::_make_connection(const Connection &p_connection) const {
	return FlowConnection::create(p_connection.from_id, p_connection.from_port, p_connection.to_id, p_connection.to_port, _get_connection_kind(p_connection));
}

TypedArray<FlowConnection> FlowGraph::_make_connections(const LocalVector<uint32_t> *p_indices) const {
	TypedArray<FlowConnection> result;
	if (p_indices != nullptr) {
		for (uint32_t index : *p_indices) {
			result.push_back(_make_connection(connections[index]));
		}
	}
	return result;
}

Error FlowGraph::_check_connection(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port, String &r_reason) const {
	const Ref<FlowNode> from = has_node(p_from_id) ? get_node(p_from_id) : Ref<FlowNode>();
	const Ref<FlowNode> to = has_node(p_to_id) ? get_node(p_to_id) : Ref<FlowNode>();
	if (from.is_null() || to.is_null()) {
		r_reason = vformat("Unknown node id %d or %d.", p_from_id, p_to_id);
		return ERR_DOES_NOT_EXIST;
	}
	if (p_from_id == p_to_id) {
		r_reason = "A node can't be connected to itself.";
		return ERR_INVALID_PARAMETER;
	}
	const Ref<FlowPort> output = from->find_output_port(p_from_port);
	const Ref<FlowPort> input = to->find_input_port(p_to_port);
	if (output.is_null() || input.is_null()) {
		r_reason = vformat("Unknown port: output '%s' of node %d or input '%s' of node %d.", p_from_port, p_from_id, p_to_port, p_to_id);
		return ERR_DOES_NOT_EXIST;
	}
	if (output->get_kind() != input->get_kind()) {
		r_reason = "A flow port can only be connected to a flow port, a data port only to a data port.";
		return ERR_INVALID_PARAMETER;
	}
	if (output->get_kind() == FlowPort::PORT_KIND_DATA && !FlowPort::is_type_compatible(output->get_port_type(), input->get_port_type())) {
		r_reason = vformat("The type %s doesn't fit the type %s.", Variant::get_type_name(output->get_port_type()), Variant::get_type_name(input->get_port_type()));
		return ERR_INVALID_PARAMETER;
	}
	_ensure_index();
	const bool is_flow = output->get_kind() == FlowPort::PORT_KIND_FLOW;
	// Flow leaves an output once; a data input reads one value.
	const LocalVector<uint32_t> *used = is_flow ? output_index.getptr(p_from_id) : input_index.getptr(p_to_id);
	if (used != nullptr) {
		for (uint32_t index : *used) {
			const Connection &c = connections[index];
			if ((is_flow && c.from_port == p_from_port) || (!is_flow && c.to_port == p_to_port)) {
				r_reason = is_flow ? vformat("The flow output '%s' of node %d is already connected.", p_from_port, p_from_id) : vformat("The data input '%s' of node %d is already connected.", p_to_port, p_to_id);
				return ERR_ALREADY_IN_USE;
			}
		}
	}
	return OK;
}

// FlowGraph: storage.

void FlowGraph::_set_node_ids(const PackedInt32Array &p_node_ids) {
	node_ids = p_node_ids;
	_invalidate_index();
}

void FlowGraph::_set_nodes(const TypedArray<FlowNode> &p_nodes) {
	for (const Ref<FlowNode> &node : nodes) {
		if (node.is_valid() && node->graph_id == get_instance_id()) {
			node->graph_id = ObjectID();
		}
	}
	nodes.clear();
	for (int i = 0; i < p_nodes.size(); i++) {
		const Ref<FlowNode> node = p_nodes[i];
		if (node.is_valid() && !node->is_in_graph()) {
			node->graph_id = get_instance_id();
		}
		nodes.push_back(node);
	}
	_invalidate_index();
}

TypedArray<FlowNode> FlowGraph::_get_nodes() const {
	TypedArray<FlowNode> result;
	for (const Ref<FlowNode> &node : nodes) {
		result.push_back(node);
	}
	return result;
}

void FlowGraph::_set_connections(const Array &p_connections) {
	connections.clear();
	ERR_FAIL_COND_MSG(p_connections.size() % 4 != 0, "The connection storage needs 4 values per connection.");
	for (int i = 0; i < p_connections.size(); i += 4) {
		Connection c;
		c.from_id = p_connections[i];
		c.from_port = p_connections[i + 1];
		c.to_id = p_connections[i + 2];
		c.to_port = p_connections[i + 3];
		connections.push_back(c);
	}
	_invalidate_index();
}

Array FlowGraph::_get_connections() const {
	Array result;
	for (const Connection &c : connections) {
		result.push_back(c.from_id);
		result.push_back(c.from_port);
		result.push_back(c.to_id);
		result.push_back(c.to_port);
	}
	return result;
}

void FlowGraph::_set_next_node_id(int p_id) {
	next_node_id = MAX(p_id, 0);
}

// FlowGraph: nodes.

int FlowGraph::add_node(const Ref<FlowNode> &p_node, const Vector2 &p_position) {
	ERR_FAIL_COND_V(p_node.is_null(), NODE_ID_NONE);
	ERR_FAIL_COND_V_MSG(p_node->is_in_graph(), NODE_ID_NONE, "The node already belongs to a graph (ERR_ALREADY_IN_USE). Remove it there first, or add a duplicate.");
	while (has_node(next_node_id)) {
		next_node_id++;
	}
	const int id = next_node_id++;
	p_node->graph_id = get_instance_id();
	p_node->set_editor_position(p_position);
	node_ids.push_back(id);
	nodes.push_back(p_node);
	_invalidate_index();
	if (start_node_id == NODE_ID_NONE && Object::cast_to<FlowStartNode>(p_node.ptr()) != nullptr) {
		start_node_id = id;
	}
	emit_signal(SNAME("node_added"), id);
	emit_changed();
	return id;
}

void FlowGraph::remove_node(int p_id) {
	_ensure_index();
	const int *found = node_index.getptr(p_id);
	ERR_FAIL_NULL_MSG(found, vformat("Unknown node id %d.", p_id));
	const int index = *found;
	if (nodes[index]->graph_id == get_instance_id()) {
		nodes[index]->graph_id = ObjectID();
	}
	node_ids.remove_at(index);
	nodes.remove_at(index);

	const uint32_t connection_count = connections.size();
	for (uint32_t i = connections.size(); i > 0; i--) {
		if (connections[i - 1].from_id == p_id || connections[i - 1].to_id == p_id) {
			connections.remove_at(i - 1);
		}
	}
	if (start_node_id == p_id) {
		start_node_id = NODE_ID_NONE;
	}
	for (int i = 0; i < frames.size(); i++) {
		const Ref<FlowFrame> frame = frames[i];
		if (frame.is_valid() && frame->get_node_ids().has(p_id)) {
			PackedInt32Array ids = frame->get_node_ids();
			ids.erase(p_id);
			frame->set_node_ids(ids);
		}
	}
	_invalidate_index();
	emit_signal(SNAME("node_removed"), p_id);
	if (connections.size() != connection_count) {
		emit_signal(SNAME("connections_changed"));
	}
	emit_changed();
}

bool FlowGraph::has_node(int p_id) const {
	_ensure_index();
	return node_index.has(p_id);
}

Ref<FlowNode> FlowGraph::get_node(int p_id) const {
	_ensure_index();
	const int *found = node_index.getptr(p_id);
	ERR_FAIL_NULL_V_MSG(found, Ref<FlowNode>(), vformat("Unknown node id %d.", p_id));
	return nodes[*found];
}

int FlowGraph::find_node_id(const Ref<FlowNode> &p_node) const {
	const int count = MIN((int)node_ids.size(), (int)nodes.size());
	for (int i = 0; i < count; i++) {
		if (nodes[i] == p_node) {
			return node_ids[i];
		}
	}
	return NODE_ID_NONE;
}

// FlowGraph: connections.

Error FlowGraph::connect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) {
	String reason;
	const Error err = _check_connection(p_from_id, p_from_port, p_to_id, p_to_port, reason);
	ERR_FAIL_COND_V_MSG(err != OK, err, reason);
	Connection c;
	c.from_id = p_from_id;
	c.from_port = p_from_port;
	c.to_id = p_to_id;
	c.to_port = p_to_port;
	connections.push_back(c);
	_invalidate_index();
	emit_signal(SNAME("connections_changed"));
	emit_changed();
	return OK;
}

void FlowGraph::disconnect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) {
	const int index = _find_connection(p_from_id, p_from_port, p_to_id, p_to_port);
	if (index < 0) {
		return;
	}
	connections.remove_at(index);
	_invalidate_index();
	emit_signal(SNAME("connections_changed"));
	emit_changed();
}

bool FlowGraph::is_nodes_connected(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const {
	return _find_connection(p_from_id, p_from_port, p_to_id, p_to_port) >= 0;
}

bool FlowGraph::can_connect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const {
	String reason;
	return _check_connection(p_from_id, p_from_port, p_to_id, p_to_port, reason) == OK;
}

TypedArray<FlowConnection> FlowGraph::get_connections() const {
	TypedArray<FlowConnection> result;
	for (const Connection &c : connections) {
		result.push_back(_make_connection(c));
	}
	return result;
}

TypedArray<FlowConnection> FlowGraph::get_output_connections(int p_id) const {
	_ensure_index();
	return _make_connections(output_index.getptr(p_id));
}

TypedArray<FlowConnection> FlowGraph::get_input_connections(int p_id) const {
	_ensure_index();
	return _make_connections(input_index.getptr(p_id));
}

PackedInt32Array FlowGraph::find_flow_path(int p_from_id, int p_to_id) const {
	ERR_FAIL_COND_V(!has_node(p_from_id) || !has_node(p_to_id), PackedInt32Array());
	// Breadth-first search over flow connections; previous[id] is the node the search came from.
	HashMap<int, int> previous;
	LocalVector<int> queue;
	previous.insert(p_from_id, NODE_ID_NONE);
	queue.push_back(p_from_id);
	for (uint32_t head = 0; head < queue.size() && !previous.has(p_to_id); head++) {
		const LocalVector<uint32_t> *outputs = output_index.getptr(queue[head]);
		if (outputs == nullptr) {
			continue;
		}
		for (uint32_t index : *outputs) {
			const Connection &c = connections[index];
			if (!previous.has(c.to_id) && _get_connection_kind(c) == FlowPort::PORT_KIND_FLOW) {
				previous.insert(c.to_id, queue[head]);
				queue.push_back(c.to_id);
			}
		}
	}
	PackedInt32Array path;
	if (!previous.has(p_to_id)) {
		return path;
	}
	for (int id = p_to_id; id != NODE_ID_NONE; id = previous[id]) {
		path.push_back(id);
	}
	path.reverse();
	return path;
}

int FlowGraph::get_flow_target(int p_from_id, const StringName &p_from_port) const {
	_ensure_index();
	const LocalVector<uint32_t> *outputs = output_index.getptr(p_from_id);
	if (outputs != nullptr) {
		for (uint32_t index : *outputs) {
			if (connections[index].from_port == p_from_port) {
				return connections[index].to_id;
			}
		}
	}
	return NODE_ID_NONE;
}

bool FlowGraph::get_data_source(int p_to_id, const StringName &p_to_port, int &r_from_id, StringName &r_from_port) const {
	_ensure_index();
	const LocalVector<uint32_t> *inputs = input_index.getptr(p_to_id);
	if (inputs != nullptr) {
		for (uint32_t index : *inputs) {
			if (connections[index].to_port == p_to_port) {
				r_from_id = connections[index].from_id;
				r_from_port = connections[index].from_port;
				return true;
			}
		}
	}
	return false;
}

void FlowGraph::set_frames(const TypedArray<FlowFrame> &p_frames) {
	frames = p_frames;
	emit_changed();
}

void FlowGraph::set_variables(const TypedArray<FlowVariable> &p_variables) {
	variables = p_variables;
	emit_changed();
}

void FlowGraph::set_start_node_id(int p_id) {
	start_node_id = p_id;
	emit_changed();
}

// FlowGraph: validation.

Ref<FlowGraphValidation> FlowGraph::validate() const {
	Ref<FlowGraphValidation> result;
	result.instantiate();
	_ensure_index();

	if (!node_index.has(start_node_id)) {
		result->add_issue(FlowGraphValidation::ISSUE_KIND_MISSING_START, NODE_ID_NONE, RTR("The graph has no start node (start_node_id is not a node of the graph)."));
	}

	HashSet<StringName> declared;
	for (int i = 0; i < variables.size(); i++) {
		const Ref<FlowVariable> variable = variables[i];
		if (variable.is_valid()) {
			declared.insert(variable->get_variable_name());
		}
	}

	for (const KeyValue<int, int> &kv : node_index) {
		const Ref<FlowNode> &node = nodes[kv.value];
		for (const String &warning : node->get_configuration_warnings()) {
			result->add_issue(FlowGraphValidation::ISSUE_KIND_NODE_WARNING, kv.key, warning);
		}
		const FlowVariableNode *variable_node = Object::cast_to<FlowVariableNode>(node.ptr());
		if (variable_node != nullptr && !variable_node->get_variable_name().is_empty() && !declared.has(variable_node->get_variable_name())) {
			result->add_issue(FlowGraphValidation::ISSUE_KIND_UNDECLARED_VARIABLE, kv.key, vformat(RTR("The variable '%s' is not declared in the graph."), variable_node->get_variable_name()));
		}
	}

	// Connections: ports, types and flow outputs used twice. Data edges are collected for the cycle check.
	HashSet<String> used_flow_outputs;
	LocalVector<const Connection *> data_edges;
	for (const Connection &c : connections) {
		const int *from = node_index.getptr(c.from_id);
		const int *to = node_index.getptr(c.to_id);
		const Ref<FlowPort> output = from != nullptr ? nodes[*from]->find_output_port(c.from_port) : Ref<FlowPort>();
		const Ref<FlowPort> input = to != nullptr ? nodes[*to]->find_input_port(c.to_port) : Ref<FlowPort>();
		if (output.is_null() || input.is_null()) {
			result->add_issue(FlowGraphValidation::ISSUE_KIND_UNKNOWN_PORT, output.is_null() ? c.from_id : c.to_id, vformat(RTR("The connection %d:%s -> %d:%s uses a node or port that does not exist."), c.from_id, c.from_port, c.to_id, c.to_port));
			continue;
		}
		if (output->get_kind() != input->get_kind() || (output->get_kind() == FlowPort::PORT_KIND_DATA && !FlowPort::is_type_compatible(output->get_port_type(), input->get_port_type()))) {
			result->add_issue(FlowGraphValidation::ISSUE_KIND_TYPE_MISMATCH, c.to_id, vformat(RTR("The output '%s' of node %d doesn't fit the input '%s' of node %d."), c.from_port, c.from_id, c.to_port, c.to_id));
		}
		if (output->get_kind() == FlowPort::PORT_KIND_FLOW) {
			const String key = itos(c.from_id) + "/" + String(c.from_port);
			if (used_flow_outputs.has(key)) {
				result->add_issue(FlowGraphValidation::ISSUE_KIND_MULTIPLE_FLOW_OUTPUTS, c.from_id, vformat(RTR("The flow output '%s' of node %d has more than one connection."), c.from_port, c.from_id));
			}
			used_flow_outputs.insert(key);
		} else {
			data_edges.push_back(&c);
		}
	}

	// Reachability: flow from the start node, then the data sources of every reached node.
	if (node_index.has(start_node_id)) {
		HashSet<int> reached;
		LocalVector<int> queue;
		reached.insert(start_node_id);
		queue.push_back(start_node_id);
		for (uint32_t head = 0; head < queue.size(); head++) {
			const LocalVector<uint32_t> *outputs = output_index.getptr(queue[head]);
			if (outputs != nullptr) {
				for (uint32_t index : *outputs) {
					const Connection &c = connections[index];
					if (!reached.has(c.to_id) && node_index.has(c.to_id) && _get_connection_kind(c) == FlowPort::PORT_KIND_FLOW) {
						reached.insert(c.to_id);
						queue.push_back(c.to_id);
					}
				}
			}
		}
		for (uint32_t head = 0; head < queue.size(); head++) {
			const LocalVector<uint32_t> *inputs = input_index.getptr(queue[head]);
			if (inputs != nullptr) {
				for (uint32_t index : *inputs) {
					const Connection &c = connections[index];
					if (!reached.has(c.from_id) && node_index.has(c.from_id) && _get_connection_kind(c) == FlowPort::PORT_KIND_DATA) {
						reached.insert(c.from_id);
						queue.push_back(c.from_id);
					}
				}
			}
		}
		for (const KeyValue<int, int> &kv : node_index) {
			if (!reached.has(kv.key)) {
				result->add_issue(FlowGraphValidation::ISSUE_KIND_UNREACHABLE_NODE, kv.key, vformat(RTR("Node %d can't be reached from the start node."), kv.key));
			}
		}
	}

	// Data cycles: peel off edges whose source has no remaining data input (forward) or whose target has no remaining
	// data output (backward). Nodes left with edges in both passes lie on a cycle.
	HashMap<int, int> remaining[2];
	for (int pass = 0; pass < 2; pass++) {
		HashMap<int, int> &degree = remaining[pass];
		LocalVector<bool> removed;
		removed.resize(data_edges.size());
		for (uint32_t i = 0; i < data_edges.size(); i++) {
			removed[i] = false;
			degree[pass == 0 ? data_edges[i]->to_id : data_edges[i]->from_id]++;
		}
		bool changed = true;
		while (changed) {
			changed = false;
			for (uint32_t i = 0; i < data_edges.size(); i++) {
				const int source = pass == 0 ? data_edges[i]->from_id : data_edges[i]->to_id;
				const int target = pass == 0 ? data_edges[i]->to_id : data_edges[i]->from_id;
				const int *source_degree = degree.getptr(source);
				if (!removed[i] && (source_degree == nullptr || *source_degree == 0)) {
					removed[i] = true;
					degree[target]--;
					changed = true;
				}
			}
		}
	}
	for (const KeyValue<int, int> &kv : remaining[0]) {
		const int *backward = remaining[1].getptr(kv.key);
		if (kv.value > 0 && backward != nullptr && *backward > 0) {
			result->add_issue(FlowGraphValidation::ISSUE_KIND_DATA_CYCLE, kv.key, vformat(RTR("Node %d is part of a cycle of data connections."), kv.key));
		}
	}
	return result;
}

TypedArray<PackedStringArray> FlowGraph::get_translatable_strings() const {
	TypedArray<PackedStringArray> result;
	for (const Ref<FlowNode> &node : nodes) {
		if (node.is_valid()) {
			result.append_array(node->get_translatable_strings());
		}
	}
	return result;
}

void FlowGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_node", "node", "position"), &FlowGraph::add_node, DEFVAL(Vector2()));
	ClassDB::bind_method(D_METHOD("remove_node", "id"), &FlowGraph::remove_node);
	ClassDB::bind_method(D_METHOD("has_node", "id"), &FlowGraph::has_node);
	ClassDB::bind_method(D_METHOD("get_node", "id"), &FlowGraph::get_node);
	ClassDB::bind_method(D_METHOD("find_node_id", "node"), &FlowGraph::find_node_id);
	ClassDB::bind_method(D_METHOD("get_node_ids"), &FlowGraph::get_node_ids);

	ClassDB::bind_method(D_METHOD("connect_nodes", "from_id", "from_port", "to_id", "to_port"), &FlowGraph::connect_nodes);
	ClassDB::bind_method(D_METHOD("disconnect_nodes", "from_id", "from_port", "to_id", "to_port"), &FlowGraph::disconnect_nodes);
	ClassDB::bind_method(D_METHOD("is_nodes_connected", "from_id", "from_port", "to_id", "to_port"), &FlowGraph::is_nodes_connected);
	ClassDB::bind_method(D_METHOD("can_connect_nodes", "from_id", "from_port", "to_id", "to_port"), &FlowGraph::can_connect_nodes);
	ClassDB::bind_method(D_METHOD("get_connections"), &FlowGraph::get_connections);
	ClassDB::bind_method(D_METHOD("get_output_connections", "id"), &FlowGraph::get_output_connections);
	ClassDB::bind_method(D_METHOD("get_input_connections", "id"), &FlowGraph::get_input_connections);
	ClassDB::bind_method(D_METHOD("find_flow_path", "from_id", "to_id"), &FlowGraph::find_flow_path);

	ClassDB::bind_method(D_METHOD("set_frames", "frames"), &FlowGraph::set_frames);
	ClassDB::bind_method(D_METHOD("get_frames"), &FlowGraph::get_frames);
	ClassDB::bind_method(D_METHOD("set_variables", "variables"), &FlowGraph::set_variables);
	ClassDB::bind_method(D_METHOD("get_variables"), &FlowGraph::get_variables);
	ClassDB::bind_method(D_METHOD("set_start_node_id", "id"), &FlowGraph::set_start_node_id);
	ClassDB::bind_method(D_METHOD("get_start_node_id"), &FlowGraph::get_start_node_id);

	ClassDB::bind_method(D_METHOD("validate"), &FlowGraph::validate);
	ClassDB::bind_method(D_METHOD("get_translatable_strings"), &FlowGraph::get_translatable_strings);

	ClassDB::bind_method(D_METHOD("_set_node_ids", "node_ids"), &FlowGraph::_set_node_ids);
	ClassDB::bind_method(D_METHOD("_set_nodes", "nodes"), &FlowGraph::_set_nodes);
	ClassDB::bind_method(D_METHOD("_get_nodes"), &FlowGraph::_get_nodes);
	ClassDB::bind_method(D_METHOD("_set_connections", "connections"), &FlowGraph::_set_connections);
	ClassDB::bind_method(D_METHOD("_get_connections"), &FlowGraph::_get_connections);
	ClassDB::bind_method(D_METHOD("_set_next_node_id", "id"), &FlowGraph::_set_next_node_id);
	ClassDB::bind_method(D_METHOD("_get_next_node_id"), &FlowGraph::_get_next_node_id);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "variables", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("FlowVariable")), "set_variables", "get_variables");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "start_node_id"), "set_start_node_id", "get_start_node_id");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "node_ids", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "_set_node_ids", "get_node_ids");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "nodes", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("FlowNode"), PROPERTY_USAGE_NO_EDITOR), "_set_nodes", "_get_nodes");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "_set_connections", "_get_connections");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "frames", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("FlowFrame"), PROPERTY_USAGE_NO_EDITOR), "set_frames", "get_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "next_node_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "_set_next_node_id", "_get_next_node_id");

	ADD_SIGNAL(MethodInfo("node_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("node_removed", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("connections_changed"));

	BIND_CONSTANT(NODE_ID_NONE);
}
