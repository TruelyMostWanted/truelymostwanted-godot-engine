/**************************************************************************/
/*  test_flow_graph.h                                                     */
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
#include "../flow_nodes.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestFlowGraph {

// Start -> Branch(true) -> End, with Get(condition) feeding the branch condition. Returns the node ids.
static Ref<FlowGraph> make_graph(int &r_start, int &r_branch, int &r_end, int &r_get) {
	Ref<FlowGraph> graph;
	graph.instantiate();
	Ref<FlowVariable> variable;
	variable.instantiate();
	variable->set_variable_name("ready");
	variable->set_type(Variant::BOOL);
	TypedArray<FlowVariable> variables;
	variables.push_back(variable);
	graph->set_variables(variables);

	r_start = graph->add_node(memnew(FlowStartNode), Vector2(0, 0));
	r_branch = graph->add_node(memnew(FlowBranchNode), Vector2(200, 0));
	r_end = graph->add_node(memnew(FlowEndNode), Vector2(400, 0));
	Ref<FlowGetVariableNode> get;
	get.instantiate();
	get->set_variable_name("ready");
	r_get = graph->add_node(get, Vector2(0, 200));

	CHECK(graph->connect_nodes(r_start, "out", r_branch, "in") == OK);
	CHECK(graph->connect_nodes(r_branch, "true", r_end, "in") == OK);
	CHECK(graph->connect_nodes(r_get, "value", r_branch, "condition") == OK);
	return graph;
}

static bool has_issue(const Ref<FlowGraphValidation> &p_validation, FlowGraphValidation::IssueKind p_kind, int p_node_id) {
	for (int i = 0; i < p_validation->get_issue_count(); i++) {
		if (p_validation->get_issue_kind(i) == p_kind && p_validation->get_issue_node_id(i) == p_node_id) {
			return true;
		}
	}
	return false;
}

static void check_same_graph(const Ref<FlowGraph> &p_graph, const Ref<FlowGraph> &p_loaded) {
	REQUIRE(p_loaded.is_valid());
	CHECK(p_loaded->get_node_ids() == p_graph->get_node_ids());
	CHECK(p_loaded->get_start_node_id() == p_graph->get_start_node_id());
	CHECK(p_loaded->get_connections().size() == p_graph->get_connections().size());
	for (const Variant &v : p_graph->get_connections()) {
		const Ref<FlowConnection> c = v;
		CHECK(p_loaded->is_nodes_connected(c->get_from_node_id(), c->get_from_port(), c->get_to_node_id(), c->get_to_port()));
	}
	for (int id : p_graph->get_node_ids()) {
		REQUIRE(p_loaded->get_node(id).is_valid());
		CHECK(p_loaded->get_node(id)->get_class() == p_graph->get_node(id)->get_class());
		CHECK(p_loaded->get_node(id)->get_editor_position() == p_graph->get_node(id)->get_editor_position());
	}
	CHECK(p_loaded->get_variables().size() == p_graph->get_variables().size());
	CHECK(p_loaded->validate()->is_valid());
}

TEST_CASE("[FlowGraph] Ports, titles and categories of built-in nodes") {
	Ref<FlowBranchNode> branch;
	branch.instantiate();
	CHECK(branch->get_title() == "Branch");
	CHECK(branch->get_category() == "Flow");
	REQUIRE(branch->find_input_port("condition").is_valid());
	CHECK(branch->find_input_port("condition")->get_kind() == FlowPort::PORT_KIND_DATA);
	CHECK(branch->find_input_port("condition")->get_type() == Variant::BOOL);
	CHECK(branch->find_output_port("true")->get_kind() == FlowPort::PORT_KIND_FLOW);
	CHECK(branch->get_output_ports().size() == 2);

	Ref<FlowGetVariableNode> get;
	get.instantiate();
	CHECK(get->get_title() == "Get Variable");
	CHECK(get->get_inline_properties() == PackedStringArray({ "variable_name" }));
	CHECK(get->get_configuration_warnings().size() == 1);
	get->set_title_override("Read");
	CHECK(get->get_title() == "Read");

	Ref<FlowExpressionNode> expression;
	expression.instantiate();
	CHECK(expression->get_input_ports().is_empty());
	expression->set_input_names(PackedStringArray({ "a", "b" }));
	expression->set_expression("a + b");
	CHECK(expression->get_input_ports().size() == 2);
	CHECK(expression->get_configuration_warnings().is_empty());
	expression->set_expression("a +");
	CHECK(expression->get_configuration_warnings().size() == 1);
}

TEST_CASE("[FlowGraph] Node ids are owned by the graph and never reused") {
	Ref<FlowGraph> graph;
	graph.instantiate();
	CHECK(graph->get_start_node_id() == FlowGraph::NODE_ID_NONE);
	const int start = graph->add_node(memnew(FlowStartNode), Vector2(10, 20));
	const int wait = graph->add_node(memnew(FlowWaitNode));
	CHECK(start == 0);
	CHECK(wait == 1);
	CHECK(graph->get_start_node_id() == start); // The first start node becomes the start.
	CHECK(graph->get_node(start)->get_editor_position() == Vector2(10, 20));
	CHECK(graph->find_node_id(graph->get_node(wait)) == wait);
	CHECK(graph->connect_nodes(start, "out", wait, "in") == OK);

	graph->remove_node(start);
	CHECK_FALSE(graph->has_node(start));
	CHECK(graph->get_start_node_id() == FlowGraph::NODE_ID_NONE);
	CHECK(graph->get_connections().is_empty());
	CHECK(graph->add_node(memnew(FlowStartNode)) == 2);
	CHECK(graph->get_node_ids() == PackedInt32Array({ 1, 2 }));
}

TEST_CASE("[FlowGraph] A node can't be added to two graphs") {
	Ref<FlowGraph> first;
	first.instantiate();
	Ref<FlowGraph> second;
	second.instantiate();
	Ref<FlowEndNode> node;
	node.instantiate();
	const int id = first->add_node(node);
	CHECK(id != FlowGraph::NODE_ID_NONE);

	ERR_PRINT_OFF;
	CHECK(second->add_node(node) == FlowGraph::NODE_ID_NONE);
	CHECK(first->add_node(node) == FlowGraph::NODE_ID_NONE);
	ERR_PRINT_ON;
	CHECK(second->get_node_ids().is_empty());

	first->remove_node(id);
	CHECK(second->add_node(node) != FlowGraph::NODE_ID_NONE);
}

TEST_CASE("[FlowGraph] Storage is not editable in the inspector") {
	Ref<FlowGraph> graph;
	graph.instantiate();
	List<PropertyInfo> properties;
	graph->get_property_list(&properties);
	int found = 0;
	for (const PropertyInfo &info : properties) {
		if (info.name == "node_ids" || info.name == "nodes" || info.name == "connections" || info.name == "frames" || info.name == "next_node_id") {
			found++;
			CHECK((info.usage & PROPERTY_USAGE_STORAGE) != 0);
			CHECK((info.usage & PROPERTY_USAGE_EDITOR) == 0);
		}
		if (info.name == "variables" || info.name == "start_node_id") {
			CHECK((info.usage & PROPERTY_USAGE_EDITOR) != 0);
		}
	}
	CHECK(found == 5);
}

TEST_CASE("[FlowGraph] Connections") {
	int start, branch, end, get;
	Ref<FlowGraph> graph = make_graph(start, branch, end, get);
	CHECK(graph->is_nodes_connected(start, "out", branch, "in"));
	CHECK(graph->get_connections().size() == 3);
	CHECK(graph->get_output_connections(branch).size() == 1);
	CHECK(graph->get_input_connections(branch).size() == 2);
	const Ref<FlowConnection> data = graph->get_input_connections(branch)[1];
	CHECK(data->get_kind() == FlowPort::PORT_KIND_DATA);

	ERR_PRINT_OFF;
	CHECK(graph->connect_nodes(start, "out", end, "in") == ERR_ALREADY_IN_USE); // Flow output used.
	CHECK(graph->connect_nodes(get, "value", branch, "condition") == ERR_ALREADY_IN_USE); // Data input used.
	CHECK(graph->connect_nodes(branch, "false", branch, "in") == ERR_INVALID_PARAMETER); // Itself.
	CHECK(graph->connect_nodes(get, "value", end, "in") == ERR_INVALID_PARAMETER); // Data to flow.
	CHECK(graph->connect_nodes(branch, "maybe", end, "in") == ERR_DOES_NOT_EXIST);
	CHECK(graph->connect_nodes(branch, "false", 99, "in") == ERR_DOES_NOT_EXIST);
	ERR_PRINT_ON;
	CHECK_FALSE(graph->can_connect_nodes(start, "out", end, "in"));
	CHECK(graph->can_connect_nodes(branch, "false", end, "in"));

	Ref<FlowExpressionNode> expression;
	expression.instantiate();
	expression->set_result_type(Variant::STRING);
	const int expression_id = graph->add_node(expression);
	graph->disconnect_nodes(get, "value", branch, "condition");
	CHECK_FALSE(graph->is_nodes_connected(get, "value", branch, "condition"));
	CHECK_FALSE(graph->can_connect_nodes(expression_id, "result", branch, "condition")); // String into bool.

	CHECK(graph->find_flow_path(start, end) == PackedInt32Array({ start, branch, end }));
	CHECK(graph->find_flow_path(end, start).is_empty());
}

TEST_CASE("[FlowGraph] Validation") {
	int start, branch, end, get;
	Ref<FlowGraph> graph = make_graph(start, branch, end, get);
	CHECK(graph->validate()->is_valid());

	// Missing start and undeclared variable.
	graph->set_start_node_id(FlowGraph::NODE_ID_NONE);
	graph->set_variables(TypedArray<FlowVariable>());
	Ref<FlowGraphValidation> validation = graph->validate();
	CHECK_FALSE(validation->is_valid());
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_MISSING_START, FlowGraph::NODE_ID_NONE));
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_UNDECLARED_VARIABLE, get));
	CHECK(validation->get_issues_for_node(get).size() == 1);

	// Unreachable nodes, node warnings, and (written past connect_nodes) type mismatch, unknown port and double flow output.
	graph = make_graph(start, branch, end, get);
	const int lonely = graph->add_node(memnew(FlowEmitEventNode));
	Ref<FlowExpressionNode> expression;
	expression.instantiate();
	expression->set_expression("1");
	expression->set_result_type(Variant::STRING);
	const int expression_id = graph->add_node(expression);
	Array storage = graph->get("connections");
	storage.append_array(Array({ expression_id, "result", branch, "condition" }));
	storage.append_array(Array({ branch, "true", lonely, "in" }));
	storage.append_array(Array({ branch, "nope", end, "in" }));
	graph->set("connections", storage);
	validation = graph->validate();
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_NODE_WARNING, lonely));
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_TYPE_MISMATCH, branch));
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_UNKNOWN_PORT, branch));
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_MULTIPLE_FLOW_OUTPUTS, branch));
	CHECK_FALSE(has_issue(validation, FlowGraphValidation::ISSUE_KIND_UNREACHABLE_NODE, lonely));
	CHECK_FALSE(has_issue(validation, FlowGraphValidation::ISSUE_KIND_UNREACHABLE_NODE, expression_id));
	graph->disconnect_nodes(branch, "true", lonely, "in");
	CHECK(has_issue(graph->validate(), FlowGraphValidation::ISSUE_KIND_UNREACHABLE_NODE, lonely));
	CHECK_FALSE(has_issue(graph->validate(), FlowGraphValidation::ISSUE_KIND_UNREACHABLE_NODE, get));

	// Data cycle between two expressions; a node fed by the cycle is not part of it.
	Ref<FlowExpressionNode> a;
	a.instantiate();
	a->set_expression("x");
	a->set_input_names(PackedStringArray({ "x" }));
	Ref<FlowExpressionNode> b = a->duplicate();
	Ref<FlowExpressionNode> c = a->duplicate();
	const int a_id = graph->add_node(a);
	const int b_id = graph->add_node(b);
	const int c_id = graph->add_node(c);
	CHECK(graph->connect_nodes(a_id, "result", b_id, "x") == OK);
	CHECK(graph->connect_nodes(b_id, "result", a_id, "x") == OK);
	CHECK(graph->connect_nodes(b_id, "result", c_id, "x") == OK);
	validation = graph->validate();
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_DATA_CYCLE, a_id));
	CHECK(has_issue(validation, FlowGraphValidation::ISSUE_KIND_DATA_CYCLE, b_id));
	CHECK_FALSE(has_issue(validation, FlowGraphValidation::ISSUE_KIND_DATA_CYCLE, c_id));
}

TEST_CASE("[FlowGraph] Saves and loads as .tres") {
	int start, branch, end, get;
	Ref<FlowGraph> graph = make_graph(start, branch, end, get);
	Ref<FlowFrame> frame;
	frame.instantiate();
	frame->set_node_ids(PackedInt32Array({ start, branch }));
	TypedArray<FlowFrame> frames;
	frames.push_back(frame);
	graph->set_frames(frames);

	const String path = TestUtils::get_temp_path("flow_graph.tres");
	REQUIRE(ResourceSaver::save(Ref<Resource>(graph), path) == OK);
	const Ref<FlowGraph> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	check_same_graph(graph, loaded);
	REQUIRE(loaded->get_frames().size() == 1);
	CHECK(Ref<FlowFrame>(loaded->get_frames()[0])->get_node_ids() == PackedInt32Array({ start, branch }));
	CHECK(loaded->add_node(memnew(FlowEndNode)) == 4); // next_node_id is stored.
}

TEST_CASE("[FlowGraph] Round trip through ResourceTextSerializer (JSON)") {
	int start, branch, end, get;
	Ref<FlowGraph> graph = make_graph(start, branch, end, get);
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	const String text = serializer->serialize(graph, ResourceTextSerializer::FORMAT_JSON);
	CHECK_FALSE(text.is_empty());
	const Ref<FlowGraph> loaded = serializer->deserialize(text, ResourceTextSerializer::FORMAT_JSON);
	check_same_graph(graph, loaded);
}

TEST_CASE("[FlowGraph] A graph with 1000 connections saves and loads") {
	Ref<FlowGraph> graph;
	graph.instantiate();
	int previous = graph->add_node(memnew(FlowStartNode));
	for (int i = 0; i < 1000; i++) {
		const int id = graph->add_node(memnew(FlowWaitNode), Vector2(i * 10, 0));
		CHECK(graph->connect_nodes(previous, "out", id, "in") == OK);
		previous = id;
	}
	const String path = TestUtils::get_temp_path("flow_graph_1000.res");
	REQUIRE(ResourceSaver::save(Ref<Resource>(graph), path) == OK);
	const Ref<FlowGraph> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_connections().size() == 1000);
	CHECK(loaded->find_flow_path(0, 1000).size() == 1001);
	CHECK(loaded->validate()->is_valid());
}

} // namespace TestFlowGraph
