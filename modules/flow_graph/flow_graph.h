/**************************************************************************/
/*  flow_graph.h                                                          */
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

#include "flow_graph_data.h"
#include "flow_node.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

// Result of FlowGraph.validate(): a list of issues, each with a kind, a node id and a message.
class FlowGraphValidation : public RefCounted {
	GDCLASS(FlowGraphValidation, RefCounted);

public:
	enum IssueKind {
		ISSUE_KIND_MISSING_START,
		ISSUE_KIND_UNREACHABLE_NODE,
		ISSUE_KIND_TYPE_MISMATCH,
		ISSUE_KIND_UNKNOWN_PORT,
		ISSUE_KIND_MULTIPLE_FLOW_OUTPUTS,
		ISSUE_KIND_UNDECLARED_VARIABLE,
		ISSUE_KIND_DATA_CYCLE,
		ISSUE_KIND_NODE_WARNING,
		ISSUE_KIND_MULTIPLE_START,
		ISSUE_KIND_INVALID_START,
		ISSUE_KIND_MAX,
	};

private:
	struct Issue {
		IssueKind kind = ISSUE_KIND_MISSING_START;
		int node_id = -1;
		String message;
	};
	LocalVector<Issue> issues;

protected:
	static void _bind_methods();

public:
	void add_issue(IssueKind p_kind, int p_node_id, const String &p_message);

	bool is_valid() const { return issues.is_empty(); }
	int get_issue_count() const { return (int)issues.size(); }
	IssueKind get_issue_kind(int p_index) const;
	int get_issue_node_id(int p_index) const;
	String get_issue_message(int p_index) const;
	PackedInt32Array get_issues_for_node(int p_node_id) const;
};

// A graph of FlowNode resources with typed ports and connections. The graph owns the node ids (never reused).
// Storage (node ids, nodes, connections, frames) is hidden in the inspector; edit it through the methods.
class FlowGraph : public Resource {
	GDCLASS(FlowGraph, Resource);

public:
	static constexpr int NODE_ID_NONE = -1;

private:
	struct Connection {
		int from_id = NODE_ID_NONE;
		StringName from_port;
		int to_id = NODE_ID_NONE;
		StringName to_port;
	};

	PackedInt32Array node_ids; // Parallel to nodes.
	LocalVector<Ref<FlowNode>> nodes;
	LocalVector<Connection> connections;
	TypedArray<FlowFrame> frames;
	TypedArray<FlowVariable> variables;
	int start_node_id = NODE_ID_NONE;
	int next_node_id = 0;

	// Runtime indexes, built on first use and dropped on every change.
	mutable Mutex index_mutex;
	mutable bool index_dirty = true;
	mutable HashMap<int, int> node_index;
	mutable HashMap<int, LocalVector<uint32_t>> output_index;
	mutable HashMap<int, LocalVector<uint32_t>> input_index;

	void _ensure_index() const;
	void _invalidate_index();
	int _find_connection(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const;
	FlowPort::PortKind _get_connection_kind(const Connection &p_connection) const;
	Ref<FlowConnection> _make_connection(const Connection &p_connection) const;
	TypedArray<FlowConnection> _make_connections(const LocalVector<uint32_t> *p_indices) const;
	Error _check_connection(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port, String &r_reason) const;

	// Storage.
	void _set_node_ids(const PackedInt32Array &p_node_ids);
	void _set_nodes(const TypedArray<FlowNode> &p_nodes);
	TypedArray<FlowNode> _get_nodes() const;
	void _set_connections(const Array &p_connections);
	Array _get_connections() const;
	void _set_next_node_id(int p_id);
	int _get_next_node_id() const { return next_node_id; }

protected:
	static void _bind_methods();

public:
	int add_node(const Ref<FlowNode> &p_node, const Vector2 &p_position = Vector2());
	void remove_node(int p_id);
	bool has_node(int p_id) const;
	Ref<FlowNode> get_node(int p_id) const;
	int find_node_id(const Ref<FlowNode> &p_node) const;
	PackedInt32Array get_node_ids() const { return node_ids; }

	Error connect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port);
	void disconnect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port);
	bool is_nodes_connected(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const;
	bool can_connect_nodes(int p_from_id, const StringName &p_from_port, int p_to_id, const StringName &p_to_port) const;
	TypedArray<FlowConnection> get_connections() const;
	TypedArray<FlowConnection> get_output_connections(int p_id) const;
	TypedArray<FlowConnection> get_input_connections(int p_id) const;
	PackedInt32Array find_flow_path(int p_from_id, int p_to_id) const;

	// Runtime lookups through the index, no allocations (FlowGraphPlayback).
	int get_flow_target(int p_from_id, const StringName &p_from_port) const;
	bool get_data_source(int p_to_id, const StringName &p_to_port, int &r_from_id, StringName &r_from_port) const;

	void set_frames(const TypedArray<FlowFrame> &p_frames);
	TypedArray<FlowFrame> get_frames() const { return frames; }
	void set_variables(const TypedArray<FlowVariable> &p_variables);
	TypedArray<FlowVariable> get_variables() const { return variables; }
	void set_start_node_id(int p_id);
	int get_start_node_id() const { return start_node_id; }

	Ref<FlowGraphValidation> validate() const;
	TypedArray<PackedStringArray> get_translatable_strings() const;
};

VARIANT_ENUM_CAST(FlowGraphValidation::IssueKind);
