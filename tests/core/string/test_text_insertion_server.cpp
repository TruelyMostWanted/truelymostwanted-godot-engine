/**************************************************************************/
/*  test_text_insertion_server.cpp                                        */
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

TEST_FORCE_LINK(test_text_insertion_server)

#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/text_insertion_server.h"
#include "core/string/translation_server.h"
#include "tests/signal_watcher.h"

namespace TestTextInsertionServer {

static Variant _gold_provider() {
	return 1250;
}

static Variant _shout(const Variant &p_value) {
	return String(p_value) + "!";
}

TEST_CASE("[TextInsertionServer] Placeholders, escapes and call arguments") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	REQUIRE(server);

	Dictionary args;
	args["name"] = "Mara";
	args[StringName("level")] = 7;
	CHECK(server->insert("Hello {name}, level {level}.", args) == "Hello Mara, level 7.");
	CHECK(server->insert("No placeholders.", args) == "No placeholders.");
	CHECK(server->insert("{{name}} and {name}}}", args) == "{name} and Mara}");
	CHECK(server->insert("Open { and {name", args) == "Open { and {name");
	CHECK(server->insert("{:upper} {}", args) == "{:upper} {}");
}

TEST_CASE("[TextInsertionServer] Sources: args, providers, values, resources by priority") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();

	Ref<TextInsertionValues> low;
	low.instantiate();
	low->set_value("title", "low");
	low->set_value("realm", "North");
	Ref<TextInsertionValues> high;
	high.instantiate();
	high->set_priority(10);
	high->set_value("title", "high");
	Ref<TextInsertionValues> later;
	later.instantiate();
	later->set_value("realm", "South");

	server->add_values(low);
	server->add_values(high);
	server->add_values(later);
	CHECK(server->insert("{title} {realm}") == "high South");

	// A priority change sorts the resources again.
	low->set_priority(20);
	CHECK(server->insert("{title} {realm}") == "low North");

	server->set_value("title", "value");
	CHECK(server->insert("{title}") == "value");
	CHECK(server->has_value("title"));
	CHECK(server->get_value("title") == Variant("value"));

	server->register_provider("title", callable_mp_static(&_gold_provider));
	CHECK(server->has_provider("title"));
	CHECK(server->insert("{title}") == "1250");

	Dictionary args;
	args["title"] = "arg";
	CHECK(server->insert("{title}", args) == "arg");

	server->unregister_provider("title");
	server->remove_value("title");
	server->remove_values(low);
	server->remove_values(high);
	server->remove_values(later);
	CHECK_FALSE(server->has_provider("title"));
	CHECK_FALSE(server->has_value("title"));

	// The inspector edits the values as a typed dictionary.
	Dictionary edited;
	edited["coins"] = 3;
	low->set_values(edited);
	CHECK((Variant::Type)low->get_values().get_typed_key_builtin() == Variant::STRING_NAME);
	CHECK(low->get_value("coins") == Variant(3));
	CHECK_FALSE(low->has_value("title"));
}

TEST_CASE("[TextInsertionServer] Built-in and custom modifiers") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	TranslationServer *translation_server = TranslationServer::get_singleton();
	const String previous_locale = translation_server->get_locale();
	translation_server->set_locale("en");

	Dictionary args;
	args["name"] = "mara STONE";
	args["gold"] = 1234567;
	args["price"] = -1234.5;
	args["text"] = "abc";
	CHECK(server->insert("{name:upper}|{name:lower}|{name:lower:capitalize}", args) == "MARA STONE|mara stone|Mara Stone");
	CHECK(server->insert("{gold:number} {price:number} {text:number}", args) == "1,234,567 -1,234.5 abc");

	translation_server->set_locale("de");
	CHECK(server->insert("{gold:number} {price:number}", args) == "1.234.567 -1.234,5");
	translation_server->set_locale("en");

	Ref<Translation> translation;
	translation.instantiate();
	translation->set_locale("en");
	translation->add_message("ITEM_SWORD", "Sword");
	translation_server->add_translation(translation);
	args["item"] = "ITEM_SWORD";
	CHECK(server->insert("{item:t} {item:t:upper}", args) == "Sword SWORD");
	translation_server->remove_translation(translation);

	server->register_modifier("shout", callable_mp_static(&_shout));
	CHECK(server->has_modifier("shout"));
	CHECK(server->insert("{text:shout:upper}", args) == "ABC!");
	server->unregister_modifier("shout");

	ERR_PRINT_OFF;
	server->register_modifier("upper", callable_mp_static(&_shout));
	CHECK_FALSE(server->has_modifier("upper"));
	CHECK(server->insert("{text:unknown}", args) == "abc");
	ERR_PRINT_ON;

	translation_server->set_locale(previous_locale);
}

TEST_CASE("[TextInsertionServer] Nested texts only with f, up to the depth limit") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	Dictionary args;
	args["name"] = "Mara";
	args["intro"] = "Hi {name}";
	args["loop"] = "<{loop:f}>";
	CHECK(server->insert("{intro}", args) == "Hi {name}");
	CHECK(server->insert("{intro:f}!", args) == "Hi Mara!");
	CHECK(server->insert("{loop:f}", args) == "<<<<<{loop:f}>>>>>");
}

TEST_CASE("[TextInsertionServer] Missing values keep the placeholder") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	ERR_PRINT_OFF;
	CHECK(server->insert("Hello {nobody:upper}!") == "Hello {nobody:upper}!");
	CHECK(server->insert("Hello {nobody}!") == "Hello {nobody}!");
	ERR_PRINT_ON;
}

TEST_CASE("[TextInsertionServer] BBCode escaping of inserted values") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	Dictionary args;
	args["name"] = "[b]Mara[/b]";
	CHECK(server->insert("[i]{name}[/i]", args, TextInsertionServer::ESCAPE_BBCODE) == "[i][lb]b]Mara[lb]/b][/i]");
	CHECK(server->insert("[i]{name}[/i]", args, TextInsertionServer::ESCAPE_NONE) == "[i][b]Mara[/b][/i]");
	CHECK(server->insert("[i]{name}[/i]", args) == "[i][b]Mara[/b][/i]");
}

TEST_CASE("[TextInsertionServer] Parsed templates") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	const Ref<TextInsertionTemplate> parsed = server->parse("{name} has {gold:number} gold, {name}.");
	REQUIRE(parsed.is_valid());
	CHECK(parsed->get_source() == "{name} has {gold:number} gold, {name}.");
	CHECK(parsed->get_names().size() == 2);
	CHECK(parsed->has_name("gold"));
	CHECK_FALSE(parsed->has_name("number"));

	Dictionary args;
	args["name"] = "Mara";
	args["gold"] = 1500;
	CHECK(server->insert_template(parsed, args) == "Mara has 1,500 gold, Mara.");
}

TEST_CASE("[TextInsertionServer] Signals") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	SIGNAL_WATCH(server, SNAME("value_changed"));
	SIGNAL_WATCH(server, SNAME("values_changed"));

	server->set_value("gold", 5);
	server->notify_changed("gold");
	server->remove_value("gold");
	const Array expected_value_changed = { Array{ StringName("gold") }, Array{ StringName("gold") }, Array{ StringName("gold") } };
	SIGNAL_CHECK("value_changed", expected_value_changed);

	Ref<TextInsertionValues> resource;
	resource.instantiate();
	server->add_values(resource);
	resource->set_value("gold", 6);
	server->remove_values(resource);
	const Array expected_values_changed = { Array(), Array(), Array() };
	SIGNAL_CHECK("values_changed", expected_values_changed);

	SIGNAL_UNWATCH(server, SNAME("value_changed"));
	SIGNAL_UNWATCH(server, SNAME("values_changed"));
}

TEST_CASE("[TextInsertionServer] insert() time of a parsed template compared to String.format()") {
	TextInsertionServer *server = TextInsertionServer::get_singleton();
	const String text = "Hello {name}, you have {gold} gold and {items} items.";
	const Ref<TextInsertionTemplate> parsed = server->parse(text);
	Dictionary args;
	args["name"] = "Mara";
	args["gold"] = "1250";
	args["items"] = "7";
	const int count = 20000;

	String result;
	uint64_t begin = OS::get_singleton()->get_ticks_usec();
	for (int idx = 0; idx < count; idx++) {
		result = server->insert_template(parsed, args);
	}
	const double template_ns = (OS::get_singleton()->get_ticks_usec() - begin) * 1000.0 / count;

	begin = OS::get_singleton()->get_ticks_usec();
	for (int idx = 0; idx < count; idx++) {
		result = server->insert(text, args);
	}
	const double cached_ns = (OS::get_singleton()->get_ticks_usec() - begin) * 1000.0 / count;

	String formatted;
	begin = OS::get_singleton()->get_ticks_usec();
	for (int idx = 0; idx < count; idx++) {
		formatted = text.format(args);
	}
	const double format_ns = (OS::get_singleton()->get_ticks_usec() - begin) * 1000.0 / count;

	CHECK(result == formatted);
	MESSAGE(vformat("TextInsertionServer: insert_template() %d ns, insert() (cached) %d ns, String.format() %d ns per call.", (int)template_ns, (int)cached_ns, (int)format_ns).utf8().get_data());
}

} // namespace TestTextInsertionServer
