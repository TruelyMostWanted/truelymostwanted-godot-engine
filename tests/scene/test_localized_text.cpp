/**************************************************************************/
/*  test_localized_text.cpp                                               */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_localized_text)

#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/string/localization_catalog.h"
#include "core/string/text_insertion_server.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation.h"
#include "core/string/translation_domain.h"
#include "core/string/translation_server.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/localized_text.h"
#include "tests/signal_watcher.h"

namespace TestLocalizedText {

static int ticks = 0;
static Variant _tick_provider() {
	return ++ticks;
}

// An NPC greeting with the player's name and a plural HUD line.
struct State {
	TextLocalizationServer *server = TextLocalizationServer::get_singleton();
	TranslationServer *translation_server = TranslationServer::get_singleton();
	TextInsertionServer *insertion = TextInsertionServer::get_singleton();
	String previous_locale = TranslationServer::get_singleton()->get_locale();

	State() {
		Ref<TextLocalizationCatalog> catalog;
		catalog.instantiate();
		catalog->set_text("NPC_GREETING", "en", "Hello {player_name}!");
		catalog->set_text("NPC_GREETING", "de", "Hallo {player_name}!");
		catalog->set_text("NPC_GREETING", "de", "Na, {player_name}?", "casual");
		catalog->set_plural_rule("en", "nplurals=2; plural=(n != 1);");
		catalog->set_plural_rule("de", "nplurals=2; plural=(n != 1);");
		catalog->set_plural_texts("ENEMIES_LEFT", "en", PackedStringArray({ "{enemy.count} enemy left", "{enemy.count} enemies left" }));
		catalog->set_plural_texts("ENEMIES_LEFT", "de", PackedStringArray({ "{enemy.count} Feind", "{enemy.count} Feinde" }));
		catalog->set_text("TICK", "en", "Tick {tick}");
		server->add_catalog(catalog);
		translation_server->set_locale("en");
		insertion->set_value("player_name", "Ada");
		MessageQueue::get_singleton()->flush();
	}

	~State() {
		insertion->remove_value("player_name");
		insertion->remove_value("enemy.count");
		server->clear_catalogs();
		for (const String &channel : translation_server->get_locale_channels()) {
			translation_server->clear_locale_for(channel);
		}
		translation_server->set_locale(previous_locale);
		MessageQueue::get_singleton()->flush();
	}
};

static Ref<LocalizedText> make_text(const StringName &p_key) {
	Ref<LocalizedText> localized;
	localized.instantiate();
	localized->set_key(p_key);
	return localized;
}

TEST_CASE("[LocalizedText] Key, context, values, plural and channel") {
	State state;
	CHECK(make_text(StringName())->get_text().is_empty());

	Ref<LocalizedText> greeting = make_text("NPC_GREETING");
	CHECK(greeting->get_text() == "Hello Ada!");
	Dictionary args;
	args["player_name"] = "Bob";
	greeting->set_args(args); // Arguments win over the server's values.
	CHECK(greeting->get_text() == "Hello Bob!");
	greeting->set_args(Dictionary());
	state.translation_server->set_locale("de");
	CHECK(greeting->get_text() == "Hallo Ada!");
	greeting->set_context("casual");
	CHECK(greeting->get_text() == "Na, Ada?");

	// The plural form comes from a value of the server or from the arguments.
	Ref<LocalizedText> enemies = make_text("ENEMIES_LEFT");
	enemies->set_count_argument("enemy.count");
	state.insertion->set_value("enemy.count", 1);
	CHECK(enemies->get_text() == "1 Feind");
	state.insertion->set_value("enemy.count", 3);
	CHECK(enemies->get_text() == "3 Feinde");
	Dictionary count_args;
	count_args["enemy.count"] = 1;
	enemies->set_args(count_args);
	CHECK(enemies->get_text() == "1 Feind");

	// A channel with its own locale.
	state.translation_server->set_locale_for("subtitles", "en");
	Ref<LocalizedText> subtitle = make_text("NPC_GREETING");
	subtitle->set_channel("subtitles");
	CHECK(subtitle->get_text() == "Hello Ada!");

	// Keys of Godot's Translation resources still work.
	Ref<Translation> legacy;
	legacy.instantiate();
	legacy->set_locale("de");
	legacy->add_message("LEGACY_KEY", "Alt");
	state.translation_server->get_main_domain()->add_translation(legacy);
	CHECK(make_text("LEGACY_KEY")->get_text() == "Alt");
	state.translation_server->get_main_domain()->remove_translation(legacy);
	CHECK(make_text("UNKNOWN_KEY")->get_text() == "UNKNOWN_KEY");
}

TEST_CASE("[SceneTree][LocalizedText] Bound controls refresh on locale, value and property changes") {
	State state;
	Window *root = SceneTree::get_singleton()->get_root();
	Label *label = memnew(Label);
	Button *button = memnew(Button);
	RichTextLabel *rich = memnew(RichTextLabel);
	root->add_child(label);
	root->add_child(button);
	root->add_child(rich);

	Ref<LocalizedText> greeting = make_text("NPC_GREETING");
	greeting->bind(label);
	CHECK(label->get_text() == "Hello Ada!");
	CHECK(greeting->is_bound(label));
	CHECK(label->get_meta("localized_text") == Variant(greeting));
	CHECK(label->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_DISABLED);
	greeting->bind(button);
	greeting->bind(button, "tooltip_text");
	greeting->bind(rich);
	CHECK(button->get_text() == "Hello Ada!");
	CHECK(button->get_tooltip_text() == "Hello Ada!");
	// Two properties: the one metadata key holds a dictionary property -> text.
	const Dictionary button_meta = button->get_meta("localized_text");
	CHECK(button_meta.size() == 2);
	CHECK(button_meta["tooltip_text"] == Variant(greeting));
	CHECK(rich->get_text() == "Hello Ada!");

	SIGNAL_WATCH(greeting.ptr(), SNAME("text_changed"));
	state.insertion->set_value("player_name", "Bob");
	CHECK(label->get_text() == "Hello Ada!"); // Refreshes are deferred to the end of the frame.
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Hello Bob!");
	CHECK(button->get_text() == "Hello Bob!");
	CHECK(rich->get_text() == "Hello Bob!");
	Array expected;
	expected.push_back(Array({ "Hello Bob!" })); // A nested initializer list would be flattened.
	SIGNAL_CHECK("text_changed", expected);
	SIGNAL_UNWATCH(greeting.ptr(), SNAME("text_changed"));

	state.translation_server->set_locale("de");
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Hallo Bob!");
	CHECK(rich->get_text() == "Hallo Bob!");
	greeting->set_context("casual");
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Na, Bob?");

	// Unbinding removes the metadata and gives the node its auto translation back.
	greeting->unbind(label);
	CHECK_FALSE(greeting->is_bound(label));
	CHECK_FALSE(label->has_meta("localized_text"));
	CHECK(label->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_INHERIT);
	state.insertion->set_value("player_name", "Cy");
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Na, Bob?");
	CHECK(button->get_text() == "Na, Cy?");
	greeting->unbind(button);
	CHECK(button->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_DISABLED); // The tooltip is still bound.
	const Dictionary remaining = button->get_meta("localized_text");
	CHECK(remaining.size() == 1);
	greeting->unbind(button, "tooltip_text");
	CHECK_FALSE(button->has_meta("localized_text"));
	CHECK(button->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_INHERIT);

	memdelete(label);
	memdelete(button);
	memdelete(rich);
}

TEST_CASE("[SceneTree][LocalizedText] Metadata bindings follow the tree and the node's channel") {
	State state;
	Window *root = SceneTree::get_singleton()->get_root();
	Ref<LocalizedText> greeting = make_text("NPC_GREETING"); // Connects the registry to this tree.
	Label *label = memnew(Label);
	label->set_meta("localized_text", greeting);
	root->add_child(label);
	CHECK(greeting->is_bound(label));
	CHECK(label->get_text() == "Hello Ada!");
	root->remove_child(label);
	CHECK_FALSE(greeting->is_bound(label));
	CHECK(label->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_INHERIT);

	// Under a node of the `subtitles` domain the text follows the subtitles locale.
	state.translation_server->set_locale_for("subtitles", "de");
	Control *subtitles = memnew(Control);
	subtitles->set_translation_domain("subtitles");
	root->add_child(subtitles);
	subtitles->add_child(label);
	CHECK(label->get_text() == "Hallo Ada!");
	state.translation_server->set_locale("de");
	state.translation_server->set_locale_for("subtitles", "en");
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Hello Ada!");
	CHECK(greeting->get_text() == "Hallo Ada!"); // Its own channel is `interface`.

	memdelete(subtitles);

	// The dictionary form binds several properties of one node.
	Button *button = memnew(Button);
	Dictionary properties;
	properties["text"] = greeting;
	properties["tooltip_text"] = greeting;
	button->set_meta("localized_text", properties);
	root->add_child(button);
	CHECK(button->get_text() == "Hallo Ada!");
	CHECK(button->get_tooltip_text() == "Hallo Ada!");
	CHECK(greeting->is_bound(button, "tooltip_text"));
	root->remove_child(button);
	CHECK_FALSE(greeting->is_bound(button, "tooltip_text"));
	memdelete(button);
}

TEST_CASE("[SceneTree][LocalizedText] Cost of the node_added hook for nodes without bindings") {
	Ref<LocalizedText> connect_registry = make_text("NPC_GREETING"); // Connects the hook to this tree.
	Window *root = SceneTree::get_singleton()->get_root();
	const int count = 10000;
	LocalVector<Node *> nodes;
	for (int idx = 0; idx < count; idx++) {
		nodes.push_back(memnew(Node));
	}
	uint64_t start = OS::get_singleton()->get_ticks_usec();
	for (Node *node : nodes) {
		root->add_child(node);
	}
	const uint64_t add_usec = OS::get_singleton()->get_ticks_usec() - start;
	// The hook alone, through the signal as the tree emits it.
	start = OS::get_singleton()->get_ticks_usec();
	for (Node *node : nodes) {
		SceneTree::get_singleton()->emit_signal(SNAME("node_added"), node);
	}
	const uint64_t hook_usec = OS::get_singleton()->get_ticks_usec() - start;
	CHECK_FALSE(nodes[0]->has_meta("localized_text"));
	MESSAGE(vformat("LocalizedText node_added hook, %d nodes without bindings: emit node_added %d ns per node, add_child %d ns per node.",
			count, (int)(hook_usec * 1000 / count), (int)(add_usec * 1000 / count));
	for (Node *node : nodes) {
		memdelete(node);
	}
}

TEST_CASE("[SceneTree][LocalizedText] A value change refreshes only the texts using it, every frame refreshes") {
	State state;
	Window *root = SceneTree::get_singleton()->get_root();
	ticks = 0;
	state.insertion->register_provider("tick", callable_mp_static(&_tick_provider));
	Label *tick_label = memnew(Label);
	Label *label = memnew(Label);
	root->add_child(tick_label);
	root->add_child(label);
	Ref<LocalizedText> ticker = make_text("TICK");
	ticker->bind(tick_label);
	Ref<LocalizedText> greeting = make_text("NPC_GREETING");
	greeting->bind(label);
	MessageQueue::get_singleton()->flush();

	const int before = ticks;
	state.insertion->set_value("player_name", "Bob");
	MessageQueue::get_singleton()->flush();
	CHECK(label->get_text() == "Hello Bob!");
	CHECK(ticks == before); // The ticker does not use `player_name`.

	ticker->set_refresh_mode(LocalizedText::REFRESH_EVERY_FRAME);
	MessageQueue::get_singleton()->flush();
	const int frame_before = ticks;
	SceneTree::get_singleton()->process(0);
	CHECK(ticks == frame_before + 1);
	CHECK(tick_label->get_text() == vformat("Tick %d", ticks));

	state.insertion->unregister_provider("tick");
	memdelete(tick_label);
	memdelete(label);
}

TEST_CASE("[SceneTree][LocalizedText] Timing with 1 000 bound labels") {
	State state;
	Window *root = SceneTree::get_singleton()->get_root();
	const int count = 1000;
	LocalVector<Label *> labels;
	LocalVector<Ref<LocalizedText>> texts;
	for (int idx = 0; idx < count; idx++) {
		Label *label = memnew(Label);
		root->add_child(label);
		labels.push_back(label);
		texts.push_back(make_text(idx == 0 ? "ENEMIES_LEFT" : "NPC_GREETING"));
	}
	texts[0]->set_count_argument("enemy.count");
	state.insertion->set_value("enemy.count", 2);
	MessageQueue::get_singleton()->flush();

	uint64_t start = OS::get_singleton()->get_ticks_usec();
	for (int idx = 0; idx < count; idx++) {
		texts[idx]->bind(labels[idx]);
	}
	const uint64_t bind_usec = OS::get_singleton()->get_ticks_usec() - start;
	CHECK(labels[1]->get_text() == "Hello Ada!");

	start = OS::get_singleton()->get_ticks_usec();
	state.translation_server->set_locale("de");
	MessageQueue::get_singleton()->flush();
	const uint64_t locale_usec = OS::get_singleton()->get_ticks_usec() - start;
	CHECK(labels[1]->get_text() == "Hallo Ada!");
	CHECK(labels[0]->get_text() == "2 Feinde");

	start = OS::get_singleton()->get_ticks_usec();
	state.insertion->set_value("enemy.count", 1);
	MessageQueue::get_singleton()->flush();
	const uint64_t one_usec = OS::get_singleton()->get_ticks_usec() - start;
	CHECK(labels[0]->get_text() == "1 Feind");

	start = OS::get_singleton()->get_ticks_usec();
	state.insertion->set_value("player_name", "Zed");
	MessageQueue::get_singleton()->flush();
	const uint64_t all_usec = OS::get_singleton()->get_ticks_usec() - start;
	CHECK(labels[count - 1]->get_text() == "Hallo Zed!");

	for (Ref<LocalizedText> &localized : texts) {
		localized->set_refresh_mode(LocalizedText::REFRESH_EVERY_FRAME);
	}
	MessageQueue::get_singleton()->flush();
	start = OS::get_singleton()->get_ticks_usec();
	SceneTree::get_singleton()->emit_signal(SNAME("process_frame"));
	const uint64_t frame_usec = OS::get_singleton()->get_ticks_usec() - start;

	MESSAGE(vformat("LocalizedText, %d bound labels (without shaping): bind all %d us, locale switch %d us, value change for 1 label %d us, for %d labels %d us, EVERY_FRAME (unchanged) %d us per frame.",
			count, (int)bind_usec, (int)locale_usec, (int)one_usec, count - 1, (int)all_usec, (int)frame_usec);

	for (Label *label : labels) {
		memdelete(label);
	}
}

} // namespace TestLocalizedText
