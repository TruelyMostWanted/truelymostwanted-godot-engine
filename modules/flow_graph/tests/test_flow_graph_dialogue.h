/**************************************************************************/
/*  test_flow_graph_dialogue.h                                            */
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

#include "../dialogue/dialogue_nodes.h"

#include "core/io/resource_saver.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#ifdef TOOLS_ENABLED
#include "editor/translations/resource_translation_parser_plugin.h"
#endif

namespace TestFlowGraphDialogue {

[[maybe_unused]] static Ref<DialogueOption> make_option(const StringName &p_port, const String &p_text, const String &p_condition = String(), bool p_once = false) {
	Ref<DialogueOption> option;
	option.instantiate();
	option->set_port(p_port);
	option->set_text(p_text);
	option->set_condition(p_condition);
	option->set_once(p_once);
	return option;
}

// Start -> Line -> Choice [buy (gold >= 10) -> End "bought", chat (once) -> News line -> back to Choice, bye -> End "bye"].
[[maybe_unused]] static Ref<FlowGraph> make_shop_graph(int &r_line, int &r_choice) {
	Ref<FlowGraph> graph;
	graph.instantiate();
	Ref<FlowVariable> gold;
	gold.instantiate();
	gold->set_variable_name("gold");
	gold->set_default_value(5);
	TypedArray<FlowVariable> variables;
	variables.push_back(gold);
	graph->set_variables(variables);

	const int start = graph->add_node(memnew(FlowStartNode));
	Ref<DialogueLineNode> line;
	line.instantiate();
	line->set_speaker("merchant");
	line->set_speaker_name("Merchant");
	line->set_text("DLG_SHOP_HELLO");
	r_line = graph->add_node(line);
	Ref<DialogueChoiceNode> choice;
	choice.instantiate();
	TypedArray<DialogueOption> options;
	options.push_back(make_option("buy", "Buy a sword", "gold >= 10"));
	options.push_back(make_option("chat", "Any news?", String(), true));
	options.push_back(make_option(StringName(), "Bye"));
	choice->set_options(options);
	r_choice = graph->add_node(choice);
	Ref<DialogueLineNode> news;
	news.instantiate();
	news->set_speaker("merchant");
	news->set_text("DLG_SHOP_NEWS");
	const int news_id = graph->add_node(news);
	Ref<FlowEndNode> bought;
	bought.instantiate();
	bought->set_end_name("bought");
	const int bought_id = graph->add_node(bought);
	Ref<FlowEndNode> bye;
	bye.instantiate();
	bye->set_end_name("bye");
	const int bye_id = graph->add_node(bye);

	CHECK(graph->connect_nodes(start, "out", r_line, "in") == OK);
	CHECK(graph->connect_nodes(r_line, "out", r_choice, "in") == OK);
	CHECK(graph->connect_nodes(r_choice, "buy", bought_id, "in") == OK);
	CHECK(graph->connect_nodes(r_choice, "chat", news_id, "in") == OK);
	CHECK(graph->connect_nodes(news_id, "out", r_choice, "in") == OK);
	CHECK(graph->connect_nodes(r_choice, "option_2", bye_id, "in") == OK);
	return graph;
}

TEST_CASE("[FlowGraphDialogue] Ports, titles and translated text") {
	Ref<DialogueLineNode> line;
	line.instantiate();
	CHECK(line->get_title() == "Dialogue Line");
	CHECK(line->get_category() == "Dialogue");
	CHECK(line->get_input_ports().size() == 1);
	CHECK(line->get_output_ports().size() == 1);
	CHECK(line->get_inline_properties() == PackedStringArray({ "speaker", "text" }));
	CHECK(line->get_configuration_warnings().size() == 1); // No text.

	line->set_speaker("hero");
	CHECK(line->get_translated_speaker_name() == "hero"); // Falls back to the speaker id.
	line->set_text("DLG_TEST_HELLO");
	line->set_translation_context("tests");

	Ref<Translation> translation;
	translation.instantiate();
	translation->set_locale("en");
	translation->add_message("DLG_TEST_HELLO", "Hello there!", "tests");
	const String old_locale = TranslationServer::get_singleton()->get_locale();
	TranslationServer::get_singleton()->set_locale("en");
	TranslationServer::get_singleton()->add_translation(translation);
	CHECK(line->get_translated_text() == "Hello there!");
	TranslationServer::get_singleton()->remove_translation(translation);
	TranslationServer::get_singleton()->set_locale(old_locale);
	CHECK(line->get_translated_text() == "DLG_TEST_HELLO"); // Untranslated key.

	Ref<DialogueChoiceNode> choice;
	choice.instantiate();
	CHECK(choice->get_output_ports().is_empty());
	TypedArray<DialogueOption> options;
	options.push_back(make_option("yes", "Yes"));
	options.push_back(make_option(StringName(), "No"));
	choice->set_options(options);
	CHECK(choice->get_option_port(1) == StringName("option_1"));
	REQUIRE(choice->get_output_ports().size() == 2);
	const Ref<DialogueOption> first = options[0];
	first->set_port("accept"); // Changing an option refreshes the ports of its choice.
	CHECK(choice->find_output_port("accept").is_valid());
	CHECK(choice->find_output_port("yes").is_null());
}

TEST_CASE("[FlowGraphDialogue] Lines and choices with conditions and once") {
	int line_id, choice_id;
	const Ref<FlowGraph> graph = make_shop_graph(line_id, choice_id);
	Ref<FlowGraphPlayback> playback;
	playback.instantiate();

	CHECK(playback->start(graph) == OK);
	CHECK(playback->is_waiting());
	CHECK(playback->get_current_node_id() == line_id);
	CHECK(playback->advance() == OK);
	CHECK(playback->get_current_node_id() == choice_id);

	TypedArray<FlowExit> exits = playback->get_available_exits();
	REQUIRE(exits.size() == 3);
	CHECK_FALSE(Ref<FlowExit>(exits[0])->is_enabled()); // gold 5 < 10.
	CHECK(Ref<FlowExit>(exits[1])->is_enabled());
	CHECK(Ref<FlowExit>(exits[1])->get_label() == "Any news?");
	CHECK(Ref<FlowExit>(exits[2])->get_port() == StringName("option_2"));

	ERR_PRINT_OFF;
	CHECK(playback->choose(0) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;

	// "chat" shows a line, loops back to the choice and is gone afterwards.
	CHECK(playback->choose(1) == OK);
	CHECK(Object::cast_to<DialogueLineNode>(playback->get_current_node().ptr()) != nullptr);
	CHECK(playback->advance() == OK);
	CHECK(playback->get_current_node_id() == choice_id);
	exits = playback->get_available_exits();
	CHECK_FALSE(Ref<FlowExit>(exits[1])->is_enabled());

	// The state keeps the taken option and the waiting choice.
	const Ref<FlowGraphState> state = playback->get_state();
	Ref<FlowGraphPlayback> restored;
	restored.instantiate();
	restored->start(graph);
	CHECK(restored->set_state(state) == OK);
	CHECK(restored->is_waiting());
	CHECK(restored->get_current_node_id() == choice_id);
	CHECK_FALSE(Ref<FlowExit>(restored->get_available_exits()[1])->is_enabled());

	restored->set_variable("gold", 12);
	CHECK(Ref<FlowExit>(restored->get_available_exits()[0])->is_enabled());
	CHECK(restored->advance() == OK); // advance() takes the first available option.
	CHECK_FALSE(restored->is_playing());
	CHECK(Ref<FlowEndNode>(graph->get_node(restored->get_history()[restored->get_history().size() - 1]))->get_end_name() == StringName("bought"));

	// A new run forgets the taken options.
	CHECK(playback->start(graph) == OK);
	CHECK(playback->advance() == OK);
	CHECK(Ref<FlowExit>(playback->get_available_exits()[1])->is_enabled());
	CHECK(playback->choose(2) == OK);
	CHECK_FALSE(playback->is_playing());
}

TEST_CASE("[FlowGraphDialogue] Translatable strings") {
	int line_id, choice_id;
	const Ref<FlowGraph> graph = make_shop_graph(line_id, choice_id);
	const TypedArray<PackedStringArray> strings = graph->get_translatable_strings();
	PackedStringArray ids;
	for (int i = 0; i < strings.size(); i++) {
		const PackedStringArray entry = strings[i];
		REQUIRE(entry.size() == 4);
		ids.push_back(entry[0]);
	}
	CHECK(ids.size() == 6);
	CHECK(ids.has("DLG_SHOP_NEWS"));
	CHECK(ids.has("DLG_SHOP_HELLO"));
	CHECK(ids.has("Merchant"));
	CHECK(ids.has("Buy a sword"));
	CHECK(ids.has("Any news?"));
	CHECK(ids.has("Bye"));
}

#ifdef TOOLS_ENABLED
TEST_CASE("[FlowGraphDialogue] POT parser reads the strings of a saved graph") {
	int line_id, choice_id;
	const Ref<FlowGraph> graph = make_shop_graph(line_id, choice_id);
	const String path = TestUtils::get_temp_path("flow_graph_dialogue.tres");
	REQUIRE(ResourceSaver::save(graph, path) == OK);

	Ref<ResourceTranslationParserPlugin> parser;
	parser.instantiate();
	List<String> extensions;
	parser->get_recognized_extensions(&extensions);
	CHECK(extensions.find("tres") != nullptr);
	CHECK(extensions.find("res") != nullptr);
	CHECK(extensions.find("mmd") != nullptr);

	Vector<Vector<String>> strings;
	CHECK(parser->parse_file(path, &strings) == OK);
	REQUIRE(strings.size() == 6);
	CHECK(strings.has(Vector<String>({ "DLG_SHOP_HELLO", "", "", "Speaker: merchant" })));
	CHECK(strings.has(Vector<String>({ "Bye", "", "", "Dialogue option" })));
}
#endif // TOOLS_ENABLED

} // namespace TestFlowGraphDialogue
