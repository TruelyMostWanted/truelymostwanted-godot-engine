/**************************************************************************/
/*  flow_graph_mermaid.cpp                                                */
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

#include "flow_graph_mermaid.h"

#include "flow_nodes.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

// FlowGraphMermaidOptions.

void FlowGraphMermaidOptions::set_direction(Direction p_direction) {
	ERR_FAIL_INDEX(p_direction, DIRECTION_MAX);
	direction = p_direction;
	emit_changed();
}

void FlowGraphMermaidOptions::set_include_data_comments(bool p_enabled) {
	include_data_comments = p_enabled;
	emit_changed();
}

void FlowGraphMermaidOptions::set_include_editor_data(bool p_enabled) {
	include_editor_data = p_enabled;
	emit_changed();
}

void FlowGraphMermaidOptions::set_draw_data_connections(bool p_enabled) {
	draw_data_connections = p_enabled;
	emit_changed();
}

void FlowGraphMermaidOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &FlowGraphMermaidOptions::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &FlowGraphMermaidOptions::get_direction);
	ClassDB::bind_method(D_METHOD("set_include_data_comments", "enabled"), &FlowGraphMermaidOptions::set_include_data_comments);
	ClassDB::bind_method(D_METHOD("is_including_data_comments"), &FlowGraphMermaidOptions::is_including_data_comments);
	ClassDB::bind_method(D_METHOD("set_include_editor_data", "enabled"), &FlowGraphMermaidOptions::set_include_editor_data);
	ClassDB::bind_method(D_METHOD("is_including_editor_data"), &FlowGraphMermaidOptions::is_including_editor_data);
	ClassDB::bind_method(D_METHOD("set_draw_data_connections", "enabled"), &FlowGraphMermaidOptions::set_draw_data_connections);
	ClassDB::bind_method(D_METHOD("is_drawing_data_connections"), &FlowGraphMermaidOptions::is_drawing_data_connections);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "direction", PROPERTY_HINT_ENUM, "Top Down,Left Right,Bottom Up,Right Left"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_data_comments"), "set_include_data_comments", "is_including_data_comments");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_editor_data"), "set_include_editor_data", "is_including_editor_data");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_data_connections"), "set_draw_data_connections", "is_drawing_data_connections");

	BIND_ENUM_CONSTANT(DIRECTION_TOP_DOWN);
	BIND_ENUM_CONSTANT(DIRECTION_LEFT_RIGHT);
	BIND_ENUM_CONSTANT(DIRECTION_BOTTOM_UP);
	BIND_ENUM_CONSTANT(DIRECTION_RIGHT_LEFT);
	BIND_ENUM_CONSTANT(DIRECTION_MAX);
}

// Helpers.

namespace {

String _escape_label(const String &p_text) {
	return p_text.replace("#", "#35;").replace("\"", "#quot;").replace("\n", "<br>");
}

String _unescape_label(const String &p_text) {
	String result;
	int i = 0;
	while (i < p_text.length()) {
		if (p_text[i] == '#') {
			const int end = p_text.find_char(';', i + 1);
			const String code = end > i + 1 ? p_text.substr(i + 1, end - i - 1) : String();
			if (code == "quot") {
				result += "\"";
				i = end + 1;
				continue;
			}
			if (code.is_valid_int()) {
				result += String::chr((char32_t)code.to_int());
				i = end + 1;
				continue;
			}
		}
		result += p_text[i];
		i++;
	}
	return result.replace("<br>", "\n");
}

// One-line JSON of the stored properties of a resource, without the given keys.
String _to_json(const Ref<Resource> &p_resource, std::initializer_list<const char *> p_skip_keys) {
	Dictionary tree;
	String error;
	const Error err = ResourceTextSerializer::encode_resource(p_resource, ResourceTextSerializer::Options(), true, tree, error);
	ERR_FAIL_COND_V_MSG(err != OK, "{}", error);
	for (const char *key : p_skip_keys) {
		tree.erase(key);
	}
	return JSON::stringify(tree, "", false, true);
}

bool _is_shape_char(char32_t p_char) {
	return p_char == '[' || p_char == '(' || p_char == '{' || p_char == '>' || p_char == '/' || p_char == '\\';
}

bool _is_shape_end_char(char32_t p_char) {
	return p_char == ']' || p_char == ')' || p_char == '}' || p_char == '/' || p_char == '\\';
}

bool _is_link_char(char32_t p_char) {
	return p_char == '-' || p_char == '=' || p_char == '.';
}

struct MermaidNode {
	String id;
	String label;
	bool has_label = false;
	char32_t shape = '[';
	Dictionary tree;
	int tree_line = 0; // 0: no `%% flow:node` line.
	int graph_id = FlowGraph::NODE_ID_NONE;
	Ref<FlowNode> node;
};

struct MermaidEdge {
	String from;
	String to;
	String label;
	bool data = false;
	bool explicit_ports = false; // From `%% flow:edge`: label is "from_port:to_port".
	int line = 0;
};

struct MermaidFrame {
	String id;
	String title;
	Vector<String> members;
};

// Reads the flowchart subset described in the class reference. All state is local, so readers on several threads never share anything.
struct MermaidReader {
	LocalVector<MermaidNode> nodes;
	HashMap<String, uint32_t> node_index;
	LocalVector<MermaidEdge> edges;
	LocalVector<MermaidFrame> frames;
	HashMap<String, Dictionary> frame_trees;
	HashMap<String, int> frame_tree_lines;
	Dictionary graph_tree;
	int graph_tree_line = 0;
	String error;
	int error_line = 0;

	bool fail(int p_line, const String &p_message) {
		if (error.is_empty()) {
			error = p_message;
			error_line = p_line;
		}
		return false;
	}

	uint32_t get_node(const String &p_id) {
		if (const uint32_t *index = node_index.getptr(p_id)) {
			return *index;
		}
		MermaidNode node;
		node.id = p_id;
		node_index.insert(p_id, nodes.size());
		nodes.push_back(node);
		return nodes.size() - 1;
	}

	static void skip_spaces(const String &p_line, int &r_pos) {
		while (r_pos < p_line.length() && (p_line[r_pos] == ' ' || p_line[r_pos] == '\t')) {
			r_pos++;
		}
	}

	// `id`, `id[label]`, `id{"label"}`, `id([label])`, ... followed by an optional `:::class`.
	bool read_node(const String &p_line, int &r_pos, int p_line_number, int p_frame, String &r_id) {
		skip_spaces(p_line, r_pos);
		const int start = r_pos;
		while (r_pos < p_line.length() && is_ascii_identifier_char(p_line[r_pos])) {
			r_pos++;
		}
		if (r_pos == start) {
			return fail(p_line_number, vformat("Expected a node id at \"%s\".", p_line.substr(start)));
		}
		r_id = p_line.substr(start, r_pos - start);
		const uint32_t index = get_node(r_id);

		if (r_pos < p_line.length() && _is_shape_char(p_line[r_pos]) && p_line[r_pos] != '/' && p_line[r_pos] != '\\') {
			const char32_t shape = p_line[r_pos];
			while (r_pos < p_line.length() && _is_shape_char(p_line[r_pos])) {
				r_pos++;
			}
			String label;
			if (r_pos < p_line.length() && p_line[r_pos] == '"') {
				const int end = p_line.find_char('"', r_pos + 1);
				if (end < 0) {
					return fail(p_line_number, "Unclosed quote in a node label.");
				}
				label = p_line.substr(r_pos + 1, end - r_pos - 1);
				r_pos = end + 1;
			} else {
				const int label_start = r_pos;
				while (r_pos < p_line.length() && p_line[r_pos] != ']' && p_line[r_pos] != ')' && p_line[r_pos] != '}') {
					r_pos++;
				}
				label = p_line.substr(label_start, r_pos - label_start).rstrip("/\\");
			}
			if (r_pos >= p_line.length() || !_is_shape_end_char(p_line[r_pos])) {
				return fail(p_line_number, vformat("Unclosed shape of node \"%s\".", r_id));
			}
			while (r_pos < p_line.length() && _is_shape_end_char(p_line[r_pos])) {
				r_pos++;
			}
			nodes[index].label = _unescape_label(label.strip_edges());
			nodes[index].has_label = true;
			nodes[index].shape = shape;
		}
		if (p_line.substr(r_pos, 3) == ":::") {
			r_pos += 3;
			while (r_pos < p_line.length() && (is_ascii_identifier_char(p_line[r_pos]) || p_line[r_pos] == '-')) {
				r_pos++;
			}
		}
		if (p_frame >= 0 && !frames[p_frame].members.has(r_id)) {
			frames[p_frame].members.push_back(r_id);
		}
		return true;
	}

	// `-->`, `---`, `==>`, `-.->`, `-.-`, with `|label|` or `-- label -->`. Dotted links are data edges.
	bool read_edge(const String &p_line, int &r_pos, int p_line_number, MermaidEdge &r_edge) {
		skip_spaces(p_line, r_pos);
		if (r_pos < p_line.length() && p_line[r_pos] == '&') {
			return fail(p_line_number, "\"&\" is not supported; write one edge per line.");
		}
		if (r_pos < p_line.length() && p_line[r_pos] == '<') {
			r_pos++;
		}
		const int start = r_pos;
		while (r_pos < p_line.length() && _is_link_char(p_line[r_pos])) {
			r_pos++;
		}
		const String link = p_line.substr(start, r_pos - start);
		if (link.length() < 2) {
			return fail(p_line_number, vformat("Expected an edge such as \"-->\" at \"%s\".", p_line.substr(start)));
		}
		r_edge.data = link.contains_char('.');
		if (r_pos < p_line.length() && p_line[r_pos] == '>') {
			r_pos++;
		} else if (link == "--" || link == "==" || link == "-.") {
			// `A -- label --> B`: the label runs up to the closing part of the link.
			int end = -1;
			for (const char *token : { "-->", "---", "==>", "===", ".->", ".-" }) {
				const int found = p_line.find(token, r_pos);
				if (found >= 0 && (end < 0 || found < end)) {
					end = found;
				}
			}
			if (end < 0) {
				return fail(p_line_number, "Unclosed edge label.");
			}
			r_edge.label = p_line.substr(r_pos, end - r_pos);
			r_pos = end;
			while (r_pos < p_line.length() && (_is_link_char(p_line[r_pos]) || p_line[r_pos] == '>')) {
				r_pos++;
			}
		}
		skip_spaces(p_line, r_pos);
		if (r_pos < p_line.length() && p_line[r_pos] == '|') {
			const int end = p_line.find_char('|', r_pos + 1);
			if (end < 0) {
				return fail(p_line_number, "Unclosed edge label.");
			}
			r_edge.label = p_line.substr(r_pos + 1, end - r_pos - 1);
			r_pos = end + 1;
		}
		String label = r_edge.label.strip_edges();
		if (label.length() >= 2 && label.begins_with("\"") && label.ends_with("\"")) {
			label = label.substr(1, label.length() - 2);
		}
		r_edge.label = _unescape_label(label);
		r_edge.line = p_line_number;
		return true;
	}

	// `node (edge node)*`.
	bool read_statement(const String &p_line, int p_line_number, int p_frame) {
		int pos = 0;
		String from;
		if (!read_node(p_line, pos, p_line_number, p_frame, from)) {
			return false;
		}
		while (true) {
			skip_spaces(p_line, pos);
			if (pos >= p_line.length()) {
				return true;
			}
			MermaidEdge edge;
			if (!read_edge(p_line, pos, p_line_number, edge)) {
				return false;
			}
			if (!read_node(p_line, pos, p_line_number, p_frame, edge.to)) {
				return false;
			}
			edge.from = from;
			from = edge.to;
			edges.push_back(edge);
		}
	}

	// `%% flow:graph {...}`, `%% flow:node <id> {...}`, `%% flow:frame <id> {...}`, `%% flow:edge <from> <port> <to> <port>`.
	bool read_data_comment(const String &p_comment, int p_line_number) {
		const String kind = p_comment.get_slicec(' ', 0).trim_prefix("flow:");
		if (kind == "edge") {
			const Vector<String> parts = p_comment.split(" ", false);
			if (parts.size() != 5) {
				return fail(p_line_number, "Expected \"flow:edge <from> <from_port> <to> <to_port>\".");
			}
			MermaidEdge edge;
			edge.from = parts[1];
			edge.to = parts[3];
			edge.label = parts[2] + ":" + parts[4];
			edge.data = true;
			edge.explicit_ports = true;
			edge.line = p_line_number;
			get_node(edge.from);
			get_node(edge.to);
			edges.push_back(edge);
			return true;
		}
		if (kind != "graph" && kind != "node" && kind != "frame") {
			return true; // Unknown kinds are ignored, so newer files still load.
		}
		String rest = p_comment.substr(p_comment.find_char(' ') + 1).strip_edges();
		String target;
		if (kind != "graph") {
			const int space = rest.find_char(' ');
			if (space < 0) {
				return fail(p_line_number, vformat("Expected \"flow:%s <id> {...}\".", kind));
			}
			target = rest.substr(0, space);
			rest = rest.substr(space + 1);
		}
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(rest) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			return fail(p_line_number, vformat("Invalid JSON in \"flow:%s\": %s", kind, json->get_error_message()));
		}
		const Dictionary data = json->get_data();
		if (kind == "graph") {
			graph_tree = data;
			graph_tree_line = p_line_number;
		} else if (kind == "node") {
			const uint32_t index = get_node(target);
			nodes[index].tree = data;
			nodes[index].tree_line = p_line_number;
		} else {
			frame_trees[target] = data;
			frame_tree_lines[target] = p_line_number;
		}
		return true;
	}

	bool read(const String &p_text) {
		const Vector<String> lines = p_text.split("\n");
		bool header = false;
		LocalVector<int> frame_stack;
		for (int i = 0; i < lines.size(); i++) {
			const int line_number = i + 1;
			String line = lines[i].strip_edges();
			if (line.begins_with("%%")) {
				const String comment = line.substr(2).strip_edges();
				if (comment.begins_with("flow:") && !read_data_comment(comment, line_number)) {
					return false;
				}
				continue;
			}
			if (line.ends_with(";")) {
				line = line.substr(0, line.length() - 1).strip_edges();
			}
			if (line.is_empty()) {
				continue;
			}
			const String word = line.get_slicec(' ', 0);
			if (!header) {
				if (word != "flowchart" && word != "graph") {
					return fail(line_number, "Expected \"flowchart\" or \"graph\": only Mermaid flowcharts are supported.");
				}
				header = true;
				continue;
			}
			if (word == "subgraph") {
				MermaidFrame frame;
				String rest = line.substr(8).strip_edges();
				int pos = 0;
				while (pos < rest.length() && is_ascii_identifier_char(rest[pos])) {
					pos++;
				}
				frame.id = rest.substr(0, pos);
				skip_spaces(rest, pos);
				if (!frame.id.is_empty() && pos < rest.length() && rest[pos] == '[') {
					String title = rest.substr(pos + 1).strip_edges().trim_suffix("]").strip_edges();
					if (title.length() >= 2 && title.begins_with("\"") && title.ends_with("\"")) {
						title = title.substr(1, title.length() - 2);
					}
					frame.title = _unescape_label(title);
				} else {
					frame.id = rest; // `subgraph Some title`: the title is also the id.
					frame.title = rest;
				}
				frames.push_back(frame);
				frame_stack.push_back(frames.size() - 1);
				continue;
			}
			if (word == "end") {
				if (frame_stack.is_empty()) {
					return fail(line_number, "\"end\" without \"subgraph\".");
				}
				frame_stack.remove_at(frame_stack.size() - 1);
				continue;
			}
			if (word == "classDef" || word == "class" || word == "style" || word == "linkStyle" || word == "click" || word == "direction") {
				continue; // Styling and interaction are not part of a FlowGraph.
			}
			if (!read_statement(line, line_number, frame_stack.is_empty() ? -1 : frame_stack[frame_stack.size() - 1])) {
				return false;
			}
		}
		if (!header) {
			return fail(1, "The text is not a Mermaid flowchart.");
		}
		if (!frame_stack.is_empty()) {
			return fail(lines.size(), "\"subgraph\" without \"end\".");
		}
		return true;
	}

	// The first flow output of a node that no earlier edge used. `p_used` holds "<id>:<port>".
	static StringName next_flow_output(const Ref<FlowNode> &p_node, int p_id, const HashSet<String> &p_used) {
		const TypedArray<FlowPort> ports = p_node->get_output_ports();
		for (int i = 0; i < ports.size(); i++) {
			const Ref<FlowPort> port = ports[i];
			if (port.is_valid() && port->get_kind() == FlowPort::PORT_KIND_FLOW && !p_used.has(itos(p_id) + ":" + String(port->get_port_name()))) {
				return port->get_port_name();
			}
		}
		return StringName();
	}

	Ref<FlowGraph> build() {
		Ref<FlowGraph> graph;
		if (graph_tree_line > 0) {
			Ref<Resource> resource;
			if (ResourceTextSerializer::decode_resource(graph_tree, "FlowGraph", resource, error) != OK) {
				error_line = graph_tree_line;
				return Ref<FlowGraph>();
			}
			graph = resource;
		} else {
			graph.instantiate();
		}

		// Ids: `n<number>` keeps its number, any other Mermaid id gets the next free one.
		HashSet<int> used_ids;
		int next_id = 0;
		for (MermaidNode &node : nodes) {
			const String number = node.id.trim_prefix("n");
			if (number != node.id && number.is_valid_int() && !used_ids.has(number.to_int())) {
				node.graph_id = number.to_int();
				used_ids.insert(node.graph_id);
				next_id = MAX(next_id, node.graph_id + 1);
			}
		}
		HashMap<String, int> flow_inputs;
		HashMap<String, int> flow_outputs;
		for (const MermaidEdge &edge : edges) {
			if (!edge.data) {
				flow_outputs[edge.from]++;
				flow_inputs[edge.to]++;
			}
		}

		PackedInt32Array node_ids;
		TypedArray<FlowNode> node_list;
		int start_id = FlowGraph::NODE_ID_NONE;
		for (MermaidNode &node : nodes) {
			if (node.graph_id == FlowGraph::NODE_ID_NONE) {
				node.graph_id = next_id++;
			}
			if (node.tree_line > 0) {
				Ref<Resource> resource;
				if (ResourceTextSerializer::decode_resource(node.tree, "FlowNode", resource, error) != OK) {
					error = vformat("Node \"%s\": %s", node.id, error);
					error_line = node.tree_line;
					return Ref<FlowGraph>();
				}
				node.node = resource;
				if (node.has_label && node.label != node.node->get_title()) {
					node.node->set_title_override(node.label); // The label was edited in the text.
				}
			} else {
				// A hand-written node: the type follows from the shape and the edges.
				const String label = node.has_label ? node.label : node.id;
				if (node.shape == '{') {
					node.node = Ref<FlowNode>(memnew(FlowBranchNode));
				} else if (!flow_inputs.has(node.id)) {
					node.node = Ref<FlowNode>(memnew(FlowStartNode));
					if (start_id == FlowGraph::NODE_ID_NONE) {
						start_id = node.graph_id;
					}
				} else if (!flow_outputs.has(node.id)) {
					node.node = Ref<FlowNode>(memnew(FlowEndNode));
				} else {
					Ref<FlowEmitEventNode> event;
					event.instantiate();
					event->set_event_name(label);
					node.node = event;
				}
				node.node->set_title_override(label);
			}
			node_ids.push_back(node.graph_id);
			node_list.push_back(node.node);
		}
		graph->set("node_ids", node_ids);
		graph->set("nodes", node_list);

		Array connections;
		HashSet<String> used_outputs;
		for (const MermaidEdge &edge : edges) {
			const MermaidNode &from = nodes[node_index[edge.from]];
			const MermaidNode &to = nodes[node_index[edge.to]];
			StringName from_port;
			StringName to_port;
			const int colon = edge.label.find_char(':');
			if (colon >= 0) {
				from_port = edge.label.substr(0, colon).strip_edges();
				to_port = edge.label.substr(colon + 1).strip_edges();
			} else if (edge.data) {
				fail(edge.line, "A dotted (data) edge needs a label \"output:input\".");
				return Ref<FlowGraph>();
			} else {
				to_port = SNAME("in");
				if (!edge.label.is_empty() && from.node->find_output_port(edge.label).is_valid()) {
					from_port = edge.label;
				} else {
					from_port = next_flow_output(from.node, from.graph_id, used_outputs);
					if (from_port == StringName()) {
						fail(edge.line, vformat("Node \"%s\" has more outgoing edges than flow outputs (a flow output leads to one node).", edge.from));
						return Ref<FlowGraph>();
					}
				}
			}
			used_outputs.insert(itos(from.graph_id) + ":" + String(from_port));
			connections.push_back(from.graph_id);
			connections.push_back(from_port);
			connections.push_back(to.graph_id);
			connections.push_back(to_port);
		}
		graph->set("connections", connections);

		TypedArray<FlowFrame> frame_list;
		for (const MermaidFrame &mermaid_frame : frames) {
			Ref<FlowFrame> frame;
			if (const Dictionary *tree = frame_trees.getptr(mermaid_frame.id)) {
				Ref<Resource> resource;
				if (ResourceTextSerializer::decode_resource(*tree, "FlowFrame", resource, error) != OK) {
					error_line = frame_tree_lines[mermaid_frame.id];
					return Ref<FlowGraph>();
				}
				frame = resource;
			} else {
				frame.instantiate();
			}
			frame->set_title(mermaid_frame.title);
			PackedInt32Array members;
			for (const String &member : mermaid_frame.members) {
				members.push_back(nodes[node_index[member]].graph_id);
			}
			frame->set_node_ids(members);
			frame_list.push_back(frame);
		}
		graph->set_frames(frame_list);

		graph->set("next_node_id", MAX((int)graph->get("next_node_id"), next_id));
		if (graph_tree_line == 0) {
			graph->set_start_node_id(start_id);
		}
		return graph;
	}
};

} // namespace

// FlowGraphMermaid.

Error FlowGraphMermaid::_fail(int p_line, const String &p_message) {
	graph.unref();
	error_line = p_line;
	error_message = p_message;
	return ERR_PARSE_ERROR;
}

Error FlowGraphMermaid::parse(const String &p_text) {
	graph.unref();
	error_line = 0;
	error_message = String();
	MermaidReader reader;
	if (!reader.read(p_text)) {
		return _fail(reader.error_line, reader.error);
	}
	graph = reader.build();
	if (graph.is_null()) {
		return _fail(reader.error_line, reader.error);
	}
	return OK;
}

String FlowGraphMermaid::serialize(const Ref<FlowGraph> &p_graph, const Ref<FlowGraphMermaidOptions> &p_options) {
	ERR_FAIL_COND_V(p_graph.is_null(), String());
	Ref<FlowGraphMermaidOptions> options = p_options;
	if (options.is_null()) {
		options.instantiate();
	}
	const bool comments = options->is_including_data_comments();
	const bool editor_data = options->is_including_editor_data();
	static const char *const directions[FlowGraphMermaidOptions::DIRECTION_MAX] = { "TD", "LR", "BT", "RL" };

	String text = String("flowchart ") + directions[options->get_direction()] + "\n";
	if (comments) {
		text += "%% flow:graph " + _to_json(p_graph, { "node_ids", "nodes", "connections", "frames" }) + "\n";
	}

	for (int id : p_graph->get_node_ids()) {
		const Ref<FlowNode> node = p_graph->get_node(id);
		if (node.is_null()) {
			continue;
		}
		const String mermaid_id = "n" + itos(id);
		const char *open = "[";
		const char *close = "]";
		if (Object::cast_to<FlowStartNode>(node.ptr()) || Object::cast_to<FlowEndNode>(node.ptr())) {
			open = "([";
			close = "])";
		} else if (Object::cast_to<FlowBranchNode>(node.ptr())) {
			open = "{";
			close = "}";
		}
		text += "\t" + mermaid_id + open + "\"" + _escape_label(node->get_title()) + "\"" + close + "\n";
		if (comments) {
			const String json = editor_data ? _to_json(node, {}) : _to_json(node, { "editor_position", "editor_size", "editor_comment" });
			text += "\t%% flow:node " + mermaid_id + " " + json + "\n";
		}
	}

	const TypedArray<FlowConnection> connections = p_graph->get_connections();
	for (int i = 0; i < connections.size(); i++) {
		const Ref<FlowConnection> connection = connections[i];
		const String from = "n" + itos(connection->get_from_node_id());
		const String to = "n" + itos(connection->get_to_node_id());
		const String from_port = connection->get_from_port();
		const String to_port = connection->get_to_port();
		if (connection->get_kind() == FlowPort::PORT_KIND_DATA) {
			if (options->is_drawing_data_connections()) {
				text += "\t" + from + " -.->|" + from_port + ":" + to_port + "| " + to + "\n";
			} else if (comments) {
				text += "\t%% flow:edge " + from + " " + from_port + " " + to + " " + to_port + "\n";
			}
		} else {
			const String label = to_port == "in" ? from_port : from_port + ":" + to_port;
			text += "\t" + from + " -->|" + label + "| " + to + "\n";
		}
	}

	const TypedArray<FlowFrame> frames = p_graph->get_frames();
	for (int i = 0; i < frames.size(); i++) {
		const Ref<FlowFrame> frame = frames[i];
		if (frame.is_null()) {
			continue;
		}
		const String mermaid_id = "f" + itos(i);
		text += "\tsubgraph " + mermaid_id + "[\"" + _escape_label(frame->get_title()) + "\"]\n";
		for (int id : frame->get_node_ids()) {
			if (p_graph->has_node(id)) {
				text += "\t\tn" + itos(id) + "\n";
			}
		}
		text += "\tend\n";
		if (comments) {
			const String json = editor_data ? _to_json(frame, { "node_ids", "title" }) : _to_json(frame, { "node_ids", "title", "rect", "color" });
			text += "\t%% flow:frame " + mermaid_id + " " + json + "\n";
		}
	}
	return text;
}

Ref<FlowGraph> FlowGraphMermaid::deserialize(const String &p_text) {
	Ref<FlowGraphMermaid> mermaid;
	mermaid.instantiate();
	const Error err = mermaid->parse(p_text);
	ERR_FAIL_COND_V_MSG(err != OK, Ref<FlowGraph>(), vformat("Mermaid parse error on line %d: %s", mermaid->get_error_line(), mermaid->get_error_message()));
	return mermaid->get_graph();
}

void FlowGraphMermaid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "text"), &FlowGraphMermaid::parse);
	ClassDB::bind_method(D_METHOD("get_graph"), &FlowGraphMermaid::get_graph);
	ClassDB::bind_method(D_METHOD("get_error_line"), &FlowGraphMermaid::get_error_line);
	ClassDB::bind_method(D_METHOD("get_error_message"), &FlowGraphMermaid::get_error_message);
	ClassDB::bind_static_method("FlowGraphMermaid", D_METHOD("serialize", "graph", "options"), &FlowGraphMermaid::serialize, DEFVAL(Variant()));
	ClassDB::bind_static_method("FlowGraphMermaid", D_METHOD("deserialize", "text"), &FlowGraphMermaid::deserialize);
}

// Loader and saver.

Ref<Resource> ResourceFormatLoaderFlowGraphMermaid::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	if (r_error) {
		*r_error = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("Cannot read the Mermaid file \"%s\".", p_path));
	Ref<FlowGraphMermaid> mermaid;
	mermaid.instantiate();
	err = mermaid->parse(text);
	if (r_error) {
		*r_error = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("%s:%d - %s", p_path, mermaid->get_error_line(), mermaid->get_error_message()));
	return mermaid->get_graph();
}

void ResourceFormatLoaderFlowGraphMermaid::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("mmd");
}

bool ResourceFormatLoaderFlowGraphMermaid::handles_type(const String &p_type) const {
	return p_type == "FlowGraph";
}

String ResourceFormatLoaderFlowGraphMermaid::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "mmd" ? "FlowGraph" : "";
}

Error ResourceFormatSaverFlowGraphMermaid::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	const Ref<FlowGraph> graph = p_resource;
	ERR_FAIL_COND_V(graph.is_null(), ERR_INVALID_PARAMETER);
	Error err = OK;
	const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot save the Mermaid file \"%s\".", p_path));
	file->store_string(FlowGraphMermaid::serialize(graph));
	return OK;
}

bool ResourceFormatSaverFlowGraphMermaid::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<FlowGraph>(p_resource.ptr()) != nullptr;
}

void ResourceFormatSaverFlowGraphMermaid::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource)) {
		p_extensions->push_back("mmd");
	}
}
