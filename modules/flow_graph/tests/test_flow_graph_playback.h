/**************************************************************************/
/*  test_flow_graph_playback.h                                            */
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
#include "../flow_graph_player.h"
#include "../flow_nodes.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestFlowGraphPlayback {

[[maybe_unused]] static Ref<FlowVariable> make_variable(const StringName &p_name, const Variant &p_default) {
	Ref<FlowVariable> variable;
	variable.instantiate();
	variable->set_variable_name(p_name);
	variable->set_default_value(p_default);
	return variable;
}

[[maybe_unused]] static Ref<FlowSetVariableNode> make_set(const StringName &p_name) {
	Ref<FlowSetVariableNode> node;
	node.instantiate();
	node->set_variable_name(p_name);
	return node;
}

[[maybe_unused]] static Ref<FlowEndNode> make_end(const StringName &p_name) {
	Ref<FlowEndNode> node;
	node.instantiate();
	node->set_end_name(p_name);
	return node;
}

// Start -> Branch(condition = Get "open") -> true: End "yes" / false: End "no".
[[maybe_unused]] static Ref<FlowGraph> make_branch_graph(int &r_yes, int &r_no) {
	Ref<FlowGraph> graph;
	graph.instantiate();
	TypedArray<FlowVariable> variables;
	variables.push_back(make_variable("open", false));
	graph->set_variables(variables);
	const int start = graph->add_node(memnew(FlowStartNode));
	const int branch = graph->add_node(memnew(FlowBranchNode));
	Ref<FlowGetVariableNode> get;
	get.instantiate();
	get->set_variable_name("open");
	const int get_id = graph->add_node(get);
	r_yes = graph->add_node(make_end("yes"));
	r_no = graph->add_node(make_end("no"));
	CHECK(graph->connect_nodes(start, "out", branch, "in") == OK);
	CHECK(graph->connect_nodes(get_id, "value", branch, "condition") == OK);
	CHECK(graph->connect_nodes(branch, "true", r_yes, "in") == OK);
	CHECK(graph->connect_nodes(branch, "false", r_no, "in") == OK);
	return graph;
}

// Start -> Wait(seconds) -> End.
[[maybe_unused]] static Ref<FlowGraph> make_wait_graph(double p_seconds, int &r_wait, int &r_end) {
	Ref<FlowGraph> graph;
	graph.instantiate();
	const int start = graph->add_node(memnew(FlowStartNode));
	Ref<FlowWaitNode> wait;
	wait.instantiate();
	wait->set_seconds(p_seconds);
	r_wait = graph->add_node(wait);
	r_end = graph->add_node(memnew(FlowEndNode));
	CHECK(graph->connect_nodes(start, "out", r_wait, "in") == OK);
	CHECK(graph->connect_nodes(r_wait, "out", r_end, "in") == OK);
	return graph;
}

TEST_CASE("[FlowGraphPlayback] Branch on a variable and end") {
	int yes, no;
	const Ref<FlowGraph> graph = make_branch_graph(yes, no);
	Ref<FlowGraphPlayback> playback;
	playback.instantiate();

	CHECK(playback->start(graph) == OK);
	CHECK_FALSE(playback->is_playing());
	CHECK(playback->get_history()[playback->get_history().size() - 1] == no);
	CHECK(playback->get_variable("open") == Variant(false)); // Declared default added to the blackboard.

	playback->set_variable("open", true);
	CHECK(playback->start(graph) == OK);
	CHECK(playback->get_history()[playback->get_history().size() - 1] == yes);
	CHECK(playback->get_history().size() == 3); // Start, Branch, End (data nodes are not entered).
}

TEST_CASE("[FlowGraphPlayback] Data pull through expressions") {
	Ref<FlowGraph> graph;
	graph.instantiate();
	TypedArray<FlowVariable> variables;
	variables.push_back(make_variable("gold", 7));
	graph->set_variables(variables);
	const int start = graph->add_node(memnew(FlowStartNode));
	Ref<FlowExpressionNode> twice;
	twice.instantiate();
	twice->set_input_names(PackedStringArray({ "gold" }));
	twice->set_expression("gold * 2"); // "gold" is unconnected: reads the variable.
	const int twice_id = graph->add_node(twice);
	Ref<FlowExpressionNode> plus;
	plus.instantiate();
	plus->set_input_names(PackedStringArray({ "x" }));
	plus->set_expression("x + 1");
	const int plus_id = graph->add_node(plus);
	const int set = graph->add_node(make_set("result"));
	CHECK(graph->connect_nodes(start, "out", set, "in") == OK);
	CHECK(graph->connect_nodes(twice_id, "result", plus_id, "x") == OK);
	CHECK(graph->connect_nodes(plus_id, "result", set, "value") == OK);

	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	CHECK(playback->start(graph) == OK); // Set Variable has no flow output connected: the graph ends there.
	CHECK(playback->get_variable("result") == Variant(15));
	CHECK(playback->get_node_input_value(set, "value") == Variant(15));
}

TEST_CASE("[FlowGraphPlayback] Wait, process and choices") {
	int wait, end;
	const Ref<FlowGraph> graph = make_wait_graph(1.0, wait, end);
	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	CHECK(playback->start(graph) == OK);
	CHECK(playback->is_playing());
	CHECK(playback->is_waiting());
	CHECK(playback->is_processing_needed());
	CHECK(playback->get_current_node_id() == wait);

	const TypedArray<FlowExit> exits = playback->get_available_exits();
	REQUIRE(exits.size() == 1);
	CHECK(Ref<FlowExit>(exits[0])->get_port() == StringName("out"));
	CHECK(Ref<FlowExit>(exits[0])->get_node_id() == end);

	playback->process(0.6);
	CHECK(playback->is_waiting());
	playback->process(0.6);
	CHECK_FALSE(playback->is_playing());

	CHECK(playback->start(graph) == OK);
	CHECK(playback->choose(0) == OK);
	CHECK_FALSE(playback->is_playing());
	ERR_PRINT_OFF;
	CHECK(playback->advance() == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
}

TEST_CASE("[FlowGraphPlayback] Travel") {
	Ref<FlowGraph> graph;
	graph.instantiate();
	const int start = graph->add_node(memnew(FlowStartNode));
	Ref<FlowWaitNode> wait_a;
	wait_a.instantiate();
	const int a = graph->add_node(wait_a);
	const int b = graph->add_node(make_set("visited_b"));
	Ref<FlowWaitNode> wait_c;
	wait_c.instantiate();
	const int c = graph->add_node(wait_c);
	CHECK(graph->connect_nodes(start, "out", a, "in") == OK);
	CHECK(graph->connect_nodes(a, "out", b, "in") == OK);
	CHECK(graph->connect_nodes(b, "out", c, "in") == OK);

	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	CHECK(playback->start(graph) == OK);
	CHECK(playback->get_travel_path(c) == PackedInt32Array({ a, b, c }));
	CHECK(playback->travel(c) == OK); // Walk: b runs on the way.
	CHECK(playback->get_current_node_id() == c);
	CHECK(playback->has_variable("visited_b"));

	CHECK(playback->start(graph) == OK);
	playback->get_blackboard()->remove_variable("visited_b");
	CHECK(playback->travel(c, FlowGraphPlayback::TRAVEL_MODE_TELEPORT) == OK);
	CHECK(playback->get_current_node_id() == c);
	CHECK_FALSE(playback->has_variable("visited_b"));
	ERR_PRINT_OFF;
	CHECK(playback->travel(start) == ERR_CANT_RESOLVE); // No flow path back.
	ERR_PRINT_ON;
}

TEST_CASE("[FlowGraphPlayback] Subgraph returns through its exit names") {
	Ref<FlowGraph> inner;
	inner.instantiate();
	const int inner_start = inner->add_node(memnew(FlowStartNode));
	const int inner_end = inner->add_node(make_end("accepted"));
	CHECK(inner->connect_nodes(inner_start, "out", inner_end, "in") == OK);

	Ref<FlowGraph> outer;
	outer.instantiate();
	const int start = outer->add_node(memnew(FlowStartNode));
	Ref<FlowSubgraphNode> subgraph;
	subgraph.instantiate();
	subgraph->set_graph(inner);
	subgraph->set_exit_names(PackedStringArray({ "accepted" }));
	const int sub = outer->add_node(subgraph);
	const int accepted = outer->add_node(make_end("good"));
	const int other = outer->add_node(make_end("bad"));
	CHECK(outer->connect_nodes(start, "out", sub, "in") == OK);
	CHECK(outer->connect_nodes(sub, "accepted", accepted, "in") == OK);
	CHECK(outer->connect_nodes(sub, "out", other, "in") == OK);

	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	CHECK(playback->start(outer) == OK);
	CHECK_FALSE(playback->is_playing());
	CHECK(playback->get_history() == PackedInt32Array({ start, sub, inner_start, inner_end, accepted }));
}

TEST_CASE("[FlowGraphPlayback] Guards stop endless loops with an error") {
	Ref<FlowGraphPlayback> playback;
	playback.instantiate();

	// Flow loop without a waiting node.
	Ref<FlowGraph> loop;
	loop.instantiate();
	const int start = loop->add_node(memnew(FlowStartNode));
	const int first = loop->add_node(make_set("a"));
	const int second = loop->add_node(make_set("b"));
	CHECK(loop->connect_nodes(start, "out", first, "in") == OK);
	CHECK(loop->connect_nodes(first, "out", second, "in") == OK);
	CHECK(loop->connect_nodes(second, "out", first, "in") == OK);
	playback->set_max_steps_per_advance(50);
	ERR_PRINT_OFF;
	CHECK(playback->start(loop) == ERR_CYCLIC_LINK);
	ERR_PRINT_ON;
	CHECK_FALSE(playback->is_playing());

	// Data cycle.
	Ref<FlowGraph> cycle;
	cycle.instantiate();
	const int cycle_start = cycle->add_node(memnew(FlowStartNode));
	Ref<FlowExpressionNode> left;
	left.instantiate();
	left->set_input_names(PackedStringArray({ "x" }));
	left->set_expression("x");
	Ref<FlowExpressionNode> right;
	right.instantiate();
	right->set_input_names(PackedStringArray({ "x" }));
	right->set_expression("x");
	const int left_id = cycle->add_node(left);
	const int right_id = cycle->add_node(right);
	const int set = cycle->add_node(make_set("value"));
	CHECK(cycle->connect_nodes(cycle_start, "out", set, "in") == OK);
	CHECK(cycle->connect_nodes(left_id, "result", right_id, "x") == OK);
	CHECK(cycle->connect_nodes(right_id, "result", left_id, "x") == OK);
	CHECK(cycle->connect_nodes(left_id, "result", set, "value") == OK);
	playback->set_max_data_depth(16);
	ERR_PRINT_OFF;
	CHECK(playback->start(cycle) == ERR_CYCLIC_LINK);
	ERR_PRINT_ON;
	CHECK_FALSE(playback->is_playing());

	// A graph that contains itself.
	Ref<FlowGraph> recursive;
	recursive.instantiate();
	const int recursive_start = recursive->add_node(memnew(FlowStartNode));
	Ref<FlowSubgraphNode> self;
	self.instantiate();
	self->set_graph(recursive);
	const int self_id = recursive->add_node(self);
	CHECK(recursive->connect_nodes(recursive_start, "out", self_id, "in") == OK);
	playback->set_max_subgraph_depth(8);
	ERR_PRINT_OFF;
	CHECK(playback->start(recursive) == ERR_CYCLIC_LINK);
	ERR_PRINT_ON;
	CHECK_FALSE(playback->is_playing());
	self->set_graph(Ref<FlowGraph>()); // Break the reference cycle.
}

TEST_CASE("[FlowGraphPlayback] State round-trips through ResourceSaver") {
	int wait, end;
	const Ref<FlowGraph> graph = make_wait_graph(1.0, wait, end);
	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	CHECK(playback->start(graph) == OK);
	playback->set_variable("coins", 3);
	playback->process(0.25);

	const String path = TestUtils::get_temp_path("flow_graph_state.tres");
	REQUIRE(ResourceSaver::save(playback->get_state(), path) == OK);
	const Ref<FlowGraphState> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_layout_version() == FlowGraphState::STATE_LAYOUT_VERSION);
	CHECK(loaded->get_node_stack() == PackedInt32Array({ wait }));

	Ref<FlowGraphPlayback> restored;
	restored.instantiate();
	ERR_PRINT_OFF;
	CHECK(restored->set_state(loaded) == ERR_CANT_OPEN); // The graph has no path and the playback has no graph.
	ERR_PRINT_ON;
	CHECK(restored->start(graph) == OK);
	CHECK(restored->set_state(loaded) == OK);
	CHECK(restored->is_waiting());
	CHECK(restored->get_current_node_id() == wait);
	CHECK(restored->get_variable("coins") == Variant(3));
	restored->process(0.5);
	CHECK(restored->is_waiting()); // 0.75 s of 1 s: the elapsed time was restored.
	restored->process(0.5);
	CHECK_FALSE(restored->is_playing());
}

TEST_CASE("[FlowGraphPlayer] Processing is on only while a node waits through process") {
	int wait, end;
	const Ref<FlowGraph> graph = make_wait_graph(1.0, wait, end);
	FlowGraphPlayer *player = memnew(FlowGraphPlayer);
	player->set_graph(graph);
	CHECK_FALSE(player->is_processing_internal());

	CHECK(player->play() == OK);
	CHECK(player->is_playing());
	CHECK(player->is_processing_internal());
	CHECK_FALSE(player->is_physics_processing_internal());

	player->set_callback_mode_process(FlowGraphPlayer::CALLBACK_MODE_PROCESS_PHYSICS);
	CHECK_FALSE(player->is_processing_internal());
	CHECK(player->is_physics_processing_internal());

	player->set_callback_mode_process(FlowGraphPlayer::CALLBACK_MODE_PROCESS_MANUAL);
	CHECK_FALSE(player->is_physics_processing_internal());
	player->get_playback()->process(2.0);
	CHECK_FALSE(player->is_playing());

	player->set_callback_mode_process(FlowGraphPlayer::CALLBACK_MODE_PROCESS_IDLE);
	CHECK(player->play() == OK);
	player->stop();
	CHECK_FALSE(player->is_processing_internal());
	memdelete(player);
}

} // namespace TestFlowGraphPlayback
