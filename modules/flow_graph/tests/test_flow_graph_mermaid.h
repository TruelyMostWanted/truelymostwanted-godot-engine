/**************************************************************************/
/*  test_flow_graph_mermaid.h                                             */
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

#include "../flow_graph_mermaid.h"
#include "test_flow_graph.h"

namespace TestFlowGraphMermaid {

using TestFlowGraph::check_same_graph;
using TestFlowGraph::make_graph;

// make_graph() plus a frame, an event node with an awkward title and a disconnected data edge target.
static Ref<FlowGraph> make_full_graph(int &r_event) {
	int start, branch, end, get;
	Ref<FlowGraph> graph = make_graph(start, branch, end, get);
	Ref<FlowEmitEventNode> event;
	event.instantiate();
	event->set_event_name("door_opened");
	event->set_title_override("Say \"hi\" #1\nsecond line");
	event->set_editor_comment("A comment");
	r_event = graph->add_node(event, Vector2(400, 200));
	CHECK(graph->connect_nodes(branch, "false", r_event, "in") == OK);
	Ref<FlowFrame> frame;
	frame.instantiate();
	frame->set_title("Intro");
	frame->set_color(Color(1, 0, 0));
	frame->set_node_ids(PackedInt32Array({ start, branch }));
	TypedArray<FlowFrame> frames;
	frames.push_back(frame);
	graph->set_frames(frames);
	return graph;
}

TEST_CASE("[FlowGraph][Mermaid] Round trip keeps nodes, types, connections, properties and frames") {
	int event_id;
	const Ref<FlowGraph> graph = make_full_graph(event_id);
	const String text = FlowGraphMermaid::serialize(graph);
	CHECK(text.begins_with("flowchart TD\n"));
	CHECK(text.contains("-.->|value:condition|"));
	CHECK(text.contains("-->|true|"));

	Ref<FlowGraphMermaid> mermaid;
	mermaid.instantiate();
	REQUIRE(mermaid->parse(text) == OK);
	const Ref<FlowGraph> loaded = mermaid->get_graph();
	check_same_graph(graph, loaded);
	const Ref<FlowEmitEventNode> event = loaded->get_node(event_id);
	REQUIRE(event.is_valid());
	CHECK(event->get_event_name() == StringName("door_opened"));
	CHECK(event->get_title() == "Say \"hi\" #1\nsecond line");
	CHECK(event->get_editor_comment() == "A comment");
	REQUIRE(loaded->get_frames().size() == 1);
	const Ref<FlowFrame> frame = loaded->get_frames()[0];
	CHECK(frame->get_title() == "Intro");
	CHECK(frame->get_color() == Color(1, 0, 0));
	CHECK(frame->get_node_ids() == Ref<FlowFrame>(graph->get_frames()[0])->get_node_ids());
	CHECK(FlowGraphMermaid::serialize(loaded) == text);
	CHECK(loaded->add_node(memnew(FlowEndNode)) == event_id + 1); // next_node_id is kept.
}

TEST_CASE("[FlowGraph][Mermaid] Options") {
	int event_id;
	const Ref<FlowGraph> graph = make_full_graph(event_id);
	Ref<FlowGraphMermaidOptions> options;
	options.instantiate();
	options->set_direction(FlowGraphMermaidOptions::DIRECTION_LEFT_RIGHT);
	options->set_draw_data_connections(false);
	options->set_include_editor_data(false);
	String text = FlowGraphMermaid::serialize(graph, options);
	CHECK(text.begins_with("flowchart LR\n"));
	CHECK_FALSE(text.contains("-.->"));
	CHECK(text.contains("%% flow:edge"));
	CHECK_FALSE(text.contains("editor_position"));
	Ref<FlowGraph> loaded = FlowGraphMermaid::deserialize(text);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_connections().size() == graph->get_connections().size());
	CHECK(loaded->get_node(event_id)->get_editor_position() == Vector2());

	options->set_include_data_comments(false);
	text = FlowGraphMermaid::serialize(graph, options);
	CHECK_FALSE(text.contains("%%"));
	loaded = FlowGraphMermaid::deserialize(text);
	REQUIRE(loaded.is_valid()); // Plain Mermaid: loads with generic nodes.
	CHECK(loaded->get_node_ids() == graph->get_node_ids());
}

TEST_CASE("[FlowGraph][Mermaid] A hand-written flowchart loads") {
	const String text = R"(graph TD
	%% A door.
	A([Start]) --> B{Door open?}
	B -->|yes| C[Walk in];
	B -- no --> D((Stop))
	C --> D
	subgraph room [The room]
		C
	end
	classDef red fill:#f00
)";
	Ref<FlowGraphMermaid> mermaid;
	mermaid.instantiate();
	REQUIRE_MESSAGE(mermaid->parse(text) == OK, mermaid->get_error_message().utf8().get_data());
	const Ref<FlowGraph> graph = mermaid->get_graph();
	CHECK(graph->get_node_ids() == PackedInt32Array({ 0, 1, 2, 3 }));
	CHECK(Object::cast_to<FlowStartNode>(graph->get_node(0).ptr()) != nullptr);
	CHECK(Object::cast_to<FlowBranchNode>(graph->get_node(1).ptr()) != nullptr);
	const Ref<FlowEmitEventNode> event = graph->get_node(2);
	REQUIRE(event.is_valid());
	CHECK(event->get_event_name() == StringName("Walk in"));
	CHECK(Object::cast_to<FlowEndNode>(graph->get_node(3).ptr()) != nullptr);
	CHECK(graph->get_node(1)->get_title() == "Door open?");
	CHECK(graph->get_start_node_id() == 0);
	CHECK(graph->is_nodes_connected(0, "out", 1, "in"));
	CHECK(graph->is_nodes_connected(1, "true", 2, "in"));
	CHECK(graph->is_nodes_connected(1, "false", 3, "in"));
	CHECK(graph->is_nodes_connected(2, "out", 3, "in"));
	REQUIRE(graph->get_frames().size() == 1);
	CHECK(Ref<FlowFrame>(graph->get_frames()[0])->get_title() == "The room");
	CHECK(Ref<FlowFrame>(graph->get_frames()[0])->get_node_ids() == PackedInt32Array({ 2 }));
	CHECK(graph->add_node(memnew(FlowEndNode)) == 4);
}

TEST_CASE("[FlowGraph][Mermaid] Errors are kept per instance") {
	Ref<FlowGraphMermaid> first;
	first.instantiate();
	Ref<FlowGraphMermaid> second;
	second.instantiate();
	ERR_PRINT_OFF;
	CHECK(first->parse("sequenceDiagram\n  A->>B: hi\n") == ERR_PARSE_ERROR);
	CHECK(second->parse("flowchart TD\n  A --> B\n  B -.-> C\n") == ERR_PARSE_ERROR);
	CHECK(FlowGraphMermaid::deserialize("flowchart TD\n  A & B --> C\n").is_null());
	ERR_PRINT_ON;
	CHECK(first->get_error_line() == 1);
	CHECK(first->get_graph().is_null());
	CHECK(second->get_error_line() == 3);
	CHECK(second->get_error_message().contains("output:input"));

	CHECK(first->parse("flowchart TD\n  A --> B\n") == OK);
	CHECK(first->get_error_line() == 0);
	CHECK(first->get_error_message().is_empty());
	CHECK(second->get_error_line() == 3);
}

TEST_CASE("[FlowGraph][Mermaid] Saves and loads as .mmd") {
	int event_id;
	const Ref<FlowGraph> graph = make_full_graph(event_id);
	const String path = TestUtils::get_temp_path("flow_graph.mmd");
	REQUIRE(ResourceSaver::save(Ref<Resource>(graph), path) == OK);
	CHECK(ResourceLoader::get_resource_type(path) == "FlowGraph");
	const Ref<FlowGraph> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	check_same_graph(graph, loaded);
}

TEST_CASE("[FlowGraph] Round trip through ResourceTextSerializer (JSON, YAML, TOML)") {
	int event_id;
	const Ref<FlowGraph> graph = make_full_graph(event_id);
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	for (ResourceTextSerializer::Format format : { ResourceTextSerializer::FORMAT_JSON, ResourceTextSerializer::FORMAT_YAML, ResourceTextSerializer::FORMAT_TOML }) {
		if (!ResourceTextSerializer::has_format_implementation(format)) {
			continue; // The module of the format is not built.
		}
		const String text = serializer->serialize(graph, format);
		CHECK_FALSE(text.is_empty());
		const Ref<FlowGraph> loaded = serializer->deserialize(text, format);
		check_same_graph(graph, loaded);
		REQUIRE(loaded->get_frames().size() == 1);
		const Ref<FlowEmitEventNode> event = loaded->get_node(event_id);
		REQUIRE(event.is_valid());
		CHECK(event->get_title() == "Say \"hi\" #1\nsecond line");
		CHECK(loaded->add_node(memnew(FlowEndNode)) == event_id + 1);
	}
}

} // namespace TestFlowGraphMermaid
