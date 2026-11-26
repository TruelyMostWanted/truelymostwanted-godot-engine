/**************************************************************************/
/*  flow_graph_playback.h                                                 */
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

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/binder_common.h"

class FlowGraph;
class FlowNode;

// One way to leave the current node, for choice menus (FlowGraphPlayback.get_available_exits()).
class FlowExit : public RefCounted {
	GDCLASS(FlowExit, RefCounted);

	StringName port;
	String label;
	bool enabled = true;
	int node_id = -1;

protected:
	static void _bind_methods();

public:
	void set_port(const StringName &p_port) { port = p_port; }
	StringName get_port() const { return port; }
	void set_label(const String &p_label) { label = p_label; }
	String get_label() const { return label; }
	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool is_enabled() const { return enabled; }
	void set_node_id(int p_node_id) { node_id = p_node_id; }
	int get_node_id() const { return node_id; }
};

// Saved state of a FlowGraphPlayback, for savegames.
class FlowGraphState : public Resource {
	GDCLASS(FlowGraphState, Resource);

public:
	static constexpr int STATE_LAYOUT_VERSION = 1;

private:
	String graph_path;
	PackedInt32Array node_stack;
	Array node_memory;
	PackedInt32Array history;
	Dictionary variables;
	bool waiting = false;
	int layout_version = STATE_LAYOUT_VERSION;

protected:
	static void _bind_methods();

public:
	void set_graph_path(const String &p_path) { graph_path = p_path; }
	String get_graph_path() const { return graph_path; }
	void set_node_stack(const PackedInt32Array &p_stack) { node_stack = p_stack; }
	PackedInt32Array get_node_stack() const { return node_stack; }
	void set_node_memory(const Array &p_memory) { node_memory = p_memory; }
	Array get_node_memory() const { return node_memory; }
	void set_history(const PackedInt32Array &p_history) { history = p_history; }
	PackedInt32Array get_history() const { return history; }
	void set_variables(const Dictionary &p_variables) { variables = p_variables; }
	Dictionary get_variables() const { return variables; }
	void set_waiting(bool p_waiting) { waiting = p_waiting; }
	bool is_waiting() const { return waiting; }
	void set_layout_version(int p_version) { layout_version = p_version; }
	int get_layout_version() const { return layout_version; }
};

// Walks a FlowGraph like a state machine: current node, exits, travel, data pulls, subgraphs, variables, save and restore.
// Synchronous and single-threaded; FlowGraphPlayer drives it from the scene tree.
class FlowGraphPlayback : public RefCounted {
	GDCLASS(FlowGraphPlayback, RefCounted);

public:
	static constexpr int START_NODE_DEFAULT = -1;
	static constexpr int HISTORY_MAX = 256;

	enum EnterResult {
		ENTER_RESULT_CONTINUE,
		ENTER_RESULT_WAIT,
		ENTER_RESULT_END,
		ENTER_RESULT_MAX,
	};

	enum TravelMode {
		TRAVEL_MODE_WALK,
		TRAVEL_MODE_TELEPORT,
		TRAVEL_MODE_MAX,
	};

	// Called when the current node, the waiting flag or the playing state changes, and before the playback is freed.
	// Set by the runtime debugger (#276); nullptr costs nothing.
	static inline void (*debug_hook)(FlowGraphPlayback *p_playback) = nullptr;

private:
	struct Frame {
		Ref<FlowGraph> graph;
		int node_id = -1;
		Dictionary memory; // node id -> Variant, touched nodes only.
	};
	struct Evaluation {
		Ref<FlowGraph> graph;
		int node_id = -1;
	};

	Ref<FlowGraph> graph;
	Ref<FlowBlackboard> blackboard;
	LocalVector<Frame> stack;
	LocalVector<Evaluation> evaluations;
	PackedInt32Array history;
	ObjectID validated_graph;
	bool playing = false;
	bool waiting = false;
	uint64_t run_serial = 0;
	Error run_error = OK;

	int max_steps_per_advance = 1024;
	int max_data_depth = 256;
	int max_subgraph_depth = 64;

	void _variable_changed(const StringName &p_name, const Variant &p_value);
	void _notify_debugger();
	void _add_declared_variables(const Ref<FlowGraph> &p_graph);
	void _fail(Error p_error, const String &p_message);
	uint64_t _begin();
	bool _enter_signal(int p_id, const Ref<FlowNode> &p_node);
	StringName _choose_current();
	Variant _read_input(const Ref<FlowGraph> &p_graph, int p_node_id, const StringName &p_port);

	// Run loop. _step(), _follow() and _end_frame() return the next node id to enter, or -1 when the run stops there.
	Error _run(uint64_t p_serial, int p_next_id);
	int _step(int p_id);
	bool _exit_current();
	int _follow(const StringName &p_port);
	int _end_frame(const StringName &p_end_name);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error start(const Ref<FlowGraph> &p_graph, int p_from_id = START_NODE_DEFAULT);
	Error advance();
	Error advance_to_exit(const StringName &p_port);
	TypedArray<FlowExit> get_available_exits();
	Error choose(int p_exit_index);
	Error travel(int p_to_id, TravelMode p_mode = TRAVEL_MODE_WALK);
	PackedInt32Array get_travel_path(int p_to_id) const;
	void stop();
	void process(double p_delta);

	bool is_playing() const { return playing; }
	bool is_waiting() const { return waiting; }
	bool is_processing_needed() const;
	Ref<FlowGraph> get_graph() const;
	Ref<FlowGraph> get_current_graph() const;
	int get_current_node_id() const;
	Ref<FlowNode> get_current_node() const;
	int get_graph_stack_depth() const { return (int)stack.size(); }
	PackedInt32Array get_history() const { return history; }

	// For nodes: the node that is being entered, processed or evaluated.
	Variant get_input_value(const StringName &p_port);
	Variant get_node_input_value(int p_node_id, const StringName &p_port);
	Variant get_node_memory() const;
	void set_node_memory(const Variant &p_value);
	void emit_event(const StringName &p_name, const Array &p_arguments = Array());

	void set_blackboard(const Ref<FlowBlackboard> &p_blackboard);
	Ref<FlowBlackboard> get_blackboard() const;
	void set_variable(const StringName &p_name, const Variant &p_value);
	Variant get_variable(const StringName &p_name) const;
	bool has_variable(const StringName &p_name) const;

	void set_max_steps_per_advance(int p_max) { max_steps_per_advance = MAX(p_max, 1); }
	int get_max_steps_per_advance() const { return max_steps_per_advance; }
	void set_max_data_depth(int p_max) { max_data_depth = MAX(p_max, 1); }
	int get_max_data_depth() const { return max_data_depth; }
	void set_max_subgraph_depth(int p_max) { max_subgraph_depth = MAX(p_max, 1); }
	int get_max_subgraph_depth() const { return max_subgraph_depth; }

	Ref<FlowGraphState> get_state() const;
	Error set_state(const Ref<FlowGraphState> &p_state);

	FlowGraphPlayback();
	~FlowGraphPlayback();
};

VARIANT_ENUM_CAST(FlowGraphPlayback::EnterResult);
VARIANT_ENUM_CAST(FlowGraphPlayback::TravelMode);
