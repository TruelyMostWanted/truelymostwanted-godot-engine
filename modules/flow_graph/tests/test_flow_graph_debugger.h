/**************************************************************************/
/*  test_flow_graph_debugger.h                                            */
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

#ifdef DEBUG_ENABLED

#include "../flow_graph.h"
#include "../flow_graph_debugger.h"
#include "../flow_graph_playback.h"
#include "../flow_nodes.h"

#include "tests/test_macros.h"

namespace TestFlowGraphDebugger {

TEST_CASE("[FlowGraphDebugger] Not installed without a debugger") {
	CHECK(FlowGraphPlayback::debug_hook == nullptr);
}

TEST_CASE("[FlowGraphDebugger] Encode the state of a playback inside a subgraph") {
	// Inner: Start -> Wait -> End. Outer: Start -> Subgraph(inner) -> End.
	Ref<FlowGraph> inner;
	inner.instantiate();
	inner->set_path_cache("res://scene.tscn::FlowGraph_inner");
	const int inner_start = inner->add_node(memnew(FlowStartNode));
	Ref<FlowWaitNode> wait;
	wait.instantiate();
	wait->set_seconds(1.0);
	const int wait_id = inner->add_node(wait);
	const int inner_end = inner->add_node(memnew(FlowEndNode));
	CHECK(inner->connect_nodes(inner_start, "out", wait_id, "in") == OK);
	CHECK(inner->connect_nodes(wait_id, "out", inner_end, "in") == OK);

	Ref<FlowGraph> outer;
	outer.instantiate();
	outer->set_path_cache("res://dialogue.tres");
	const int start = outer->add_node(memnew(FlowStartNode));
	Ref<FlowSubgraphNode> subgraph;
	subgraph.instantiate();
	subgraph->set_graph(inner);
	const int sub = outer->add_node(subgraph);
	const int end = outer->add_node(memnew(FlowEndNode));
	CHECK(outer->connect_nodes(start, "out", sub, "in") == OK);
	CHECK(outer->connect_nodes(sub, "out", end, "in") == OK);

	Ref<FlowGraphPlayback> playback;
	playback.instantiate();
	Array data = FlowGraphDebugger::encode(playback.ptr());
	REQUIRE(data.size() == FlowGraphDebugger::FIELD_MAX);
	CHECK((int64_t)data[FlowGraphDebugger::FIELD_ID] == (int64_t)(uint64_t)playback->get_instance_id());
	CHECK(String(data[FlowGraphDebugger::FIELD_GRAPH_PATH]).is_empty());
	CHECK((int)data[FlowGraphDebugger::FIELD_NODE_ID] == FlowGraph::NODE_ID_NONE);
	CHECK_FALSE((bool)data[FlowGraphDebugger::FIELD_PLAYING]);

	CHECK(playback->start(outer) == OK);
	REQUIRE(playback->is_waiting());
	data = FlowGraphDebugger::encode(playback.ptr());
	CHECK(String(data[FlowGraphDebugger::FIELD_GRAPH_PATH]) == "res://dialogue.tres");
	CHECK(String(data[FlowGraphDebugger::FIELD_CURRENT_GRAPH_PATH]) == "res://scene.tscn::FlowGraph_inner");
	CHECK((int)data[FlowGraphDebugger::FIELD_NODE_ID] == wait_id);
	CHECK((bool)data[FlowGraphDebugger::FIELD_PLAYING]);
	CHECK((bool)data[FlowGraphDebugger::FIELD_WAITING]);

	playback->process(1.5);
	REQUIRE_FALSE(playback->is_playing());
	data = FlowGraphDebugger::encode(playback.ptr());
	CHECK(String(data[FlowGraphDebugger::FIELD_CURRENT_GRAPH_PATH]) == "res://dialogue.tres");
	CHECK_FALSE((bool)data[FlowGraphDebugger::FIELD_PLAYING]);
	CHECK_FALSE((bool)data[FlowGraphDebugger::FIELD_WAITING]);
}

} // namespace TestFlowGraphDebugger

#endif // DEBUG_ENABLED
