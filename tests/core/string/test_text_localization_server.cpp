/**************************************************************************/
/*  test_text_localization_server.cpp                                     */
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

TEST_FORCE_LINK(test_text_localization_server)

#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/string/localization_catalog.h"
#include "core/string/text_insertion_server.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestTextLocalizationServer {

// Game catalog (priority 0) and a mod (priority 10) that overrides one German text.
static Ref<TextLocalizationCatalog> make_game_catalog() {
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_text("HELLO", "en", "Hello");
	catalog->set_text("HELLO", "de", "Hallo");
	catalog->set_text("HELLO", "de", String(U"Hallo (Menü)"), "menu");
	catalog->set_text("BYE", "en", "Bye");
	catalog->set_text("GREET", "en", "Hello {name}");
	catalog->set_text("GREET", "de", "Hallo {name}");
	catalog->set_plural_rule("de", "nplurals=2; plural=(n != 1);");
	catalog->set_plural_texts("APPLE", "de", PackedStringArray({ "{n} Apfel", String(U"{n} Äpfel") }));
	catalog->set_text("APPLE", "en", "{n} apple(s)");
	return catalog;
}

static Ref<TextLocalizationCatalog> make_mod_catalog() {
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_text("HELLO", "de", "Servus");
	catalog->set_text("MOD_ONLY", "de", "Nur im Mod");
	return catalog;
}

struct ServerState {
	TextLocalizationServer *server = TextLocalizationServer::get_singleton();
	String previous_locale = TranslationServer::get_singleton()->get_locale();

	~ServerState() {
		server->clear_catalogs();
		TranslationServer::get_singleton()->set_locale(previous_locale);
		MessageQueue::get_singleton()->flush();
	}
};

TEST_CASE("[TextLocalizationServer] Locale chain") {
	const String fallback = LocalizationServer::get_fallback_locale();
	const PackedStringArray chain = LocalizationServer::get_locale_chain("de_DE");
	REQUIRE(chain.size() >= 3);
	CHECK(chain[0] == "de_DE");
	CHECK(chain[1] == "de");
	CHECK(chain[2] == fallback);
	CHECK(LocalizationServer::get_locale_chain(fallback) == PackedStringArray({ fallback }));
}

TEST_CASE("[TextLocalizationServer] Layers, chain, context and fallback") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	Ref<TextLocalizationCatalog> game = make_game_catalog();
	Ref<TextLocalizationCatalog> mod = make_mod_catalog();
	server->add_catalog(mod, 10);
	server->add_catalog(game);
	CHECK(server->get_catalogs().size() == 2);
	CHECK(server->get_catalogs()[0] == Variant(game)); // Lowest priority first.

	LocalizationServer::set_locale("de_DE");
	CHECK(LocalizationServer::get_locale() == "de_DE");
	CHECK(server->translate("HELLO") == "Servus"); // The mod wins on the same locale.
	CHECK(server->translate("HELLO", "menu") == String(U"Hallo (Menü)"));
	CHECK(server->translate("BYE") == "Bye"); // Fallback locale.
	CHECK(server->translate("MOD_ONLY") == "Nur im Mod");
	CHECK(server->translate("UNKNOWN") == "UNKNOWN");
	CHECK(server->has_translation("HELLO"));
	CHECK_FALSE(server->has_translation("UNKNOWN"));

	// A text of the exact locale beats a text of the language, whatever the layer.
	game->set_text("HELLO", "de_DE", "Moin");
	MessageQueue::get_singleton()->flush(); // Catalog changes are applied deferred.
	CHECK(server->translate("HELLO") == "Moin");

	// Other locales without switching.
	CHECK(server->translate_for_locale("en", "HELLO") == "Hello");
	CHECK(server->translate_for_locale("en", "MOD_ONLY") == "MOD_ONLY");
	CHECK(server->translate_for_locale("de", "HELLO") == "Servus");

	// The tr() adapter in the main domain.
	CHECK(String(TranslationServer::get_singleton()->translate("HELLO")) == "Moin");
	CHECK(String(TranslationServer::get_singleton()->translate("HELLO", "menu")) == String(U"Hallo (Menü)"));
	CHECK(String(TranslationServer::get_singleton()->translate("UNKNOWN")) == "UNKNOWN");

	LocalizationServer::set_locale("en");
	CHECK(server->translate("HELLO") == "Hello");
	CHECK(String(TranslationServer::get_singleton()->translate("HELLO")) == "Hello");

	server->remove_catalog(mod);
	CHECK_FALSE(server->has_catalog(mod));
	CHECK(server->translate_for_locale("de", "HELLO") == "Hallo");
}

TEST_CASE("[TextLocalizationServer] Locale hook after the translation remaps are cleared") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	server->add_catalog(make_game_catalog());
	LocalizationServer::set_locale("en");
	CHECK(server->translate("HELLO") == "Hello");

	// What EditorNode does on startup: the hook is dropped, a locale change no longer reaches the server.
	ResourceLoader::clear_translation_remaps();
	LocalizationServer::set_locale("de");
	CHECK(server->translate("HELLO") == "Hello");

	// The editor init callback: registered again and caught up with the active locale.
	LocalizationServer::restore_locale_hooks();
	CHECK(server->translate("HELLO") == "Hallo");
	LocalizationServer::set_locale("en");
	CHECK(server->translate("HELLO") == "Hello");
}

TEST_CASE("[TextLocalizationServer] Key ids, plurals and arguments") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	Ref<TextLocalizationCatalog> mod = make_mod_catalog();
	server->add_catalog(make_game_catalog());
	server->add_catalog(mod, 10);
	LocalizationServer::set_locale("de");

	const int hello = server->get_key_id("HELLO");
	const int mod_only = server->get_key_id("MOD_ONLY");
	REQUIRE(hello >= 0);
	CHECK(server->get_key_id("HELLO", "menu") != hello);
	CHECK(server->get_key_id("UNKNOWN") == -1);
	CHECK(server->translate_id(hello) == "Servus");

	// Ids stay valid when catalogs change.
	server->remove_catalog(mod);
	CHECK(server->get_key_id("HELLO") == hello);
	CHECK(server->translate_id(hello) == "Hallo");
	CHECK(server->translate_id(mod_only) == "MOD_ONLY");
	server->add_catalog(mod, 10);
	CHECK(server->get_key_id("MOD_ONLY") == mod_only);
	CHECK(server->translate_id(mod_only) == "Nur im Mod");

	Dictionary args;
	args["n"] = 3;
	CHECK(server->translate("APPLE", "", 1) == "{n} Apfel");
	CHECK(server->translate("APPLE", "", 3, args) == String(U"3 Äpfel"));
	CHECK(server->translate_id(server->get_key_id("APPLE"), 3, args) == String(U"3 Äpfel"));
	CHECK(String(TranslationServer::get_singleton()->translate_plural("APPLE", "APPLES", 5)) == String(U"{n} Äpfel"));

	Dictionary name;
	name["name"] = "Ann";
	CHECK(server->translate("GREET", "", -1, name) == "Hallo Ann");
	CHECK(server->translate("GREET", "", -1, name) == "Hallo Ann"); // Cached template.
	CHECK(server->translate("Welcome {name}", "", -1, name) == "Welcome Ann"); // A missing key is the text.
	CHECK(server->translate_for_locale("en", "GREET", "", -1, name) == "Hello Ann");

	// The `t` modifier of TextInsertionServer uses this server.
	Dictionary key;
	key["k"] = "HELLO";
	CHECK(TextInsertionServer::get_singleton()->insert("{k:t}!", key) == "Servus!");
}

TEST_CASE("[TextLocalizationServer] Reverse lookup and missing keys") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	server->add_catalog(make_game_catalog());
	server->add_catalog(make_mod_catalog(), 10);
	LocalizationServer::set_locale("de");

	CHECK(server->get_translation_key("Servus") == "HELLO");
	CHECK(server->get_translation_key("nothing") == StringName());
	CHECK(server->get_translation_key_for_locale("en", "Hello") == "HELLO");
	CHECK(server->get_translation_keys("Bye") == PackedStringArray({ "BYE" })); // Through the fallback.

	const PackedStringArray missing = server->get_missing_keys("de");
	CHECK(missing.has("BYE"));
	CHECK_FALSE(missing.has("HELLO"));
	CHECK(server->get_missing_keys("en").has("MOD_ONLY"));
	CHECK(server->get_available_locales().has("de"));
}

TEST_CASE("[TextLocalizationServer] Signals") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	LocalizationServer::set_locale("en");

	SIGNAL_WATCH(server, SNAME("catalogs_changed"));
	SIGNAL_WATCH(server, SNAME("locale_changed"));
	SIGNAL_WATCH(server, SNAME("locale_loaded"));
	server->add_catalog(make_game_catalog());
	SIGNAL_CHECK("catalogs_changed", { {} });
	SIGNAL_CHECK("locale_loaded", { { "en" } });

	LocalizationServer::set_locale("de");
	Array changed;
	changed.push_back(Array({ "de", "en" }));
	SIGNAL_CHECK("locale_changed", changed);
	SIGNAL_CHECK("locale_loaded", { { "de" } });

	// The same locale again: nothing.
	LocalizationServer::set_locale("de");
	SIGNAL_CHECK_FALSE("locale_changed");

	// TranslationServer is the single locale source: its changes arrive too.
	TranslationServer::get_singleton()->set_locale("en");
	Array changed_back;
	changed_back.push_back(Array({ "en", "de" }));
	SIGNAL_CHECK("locale_changed", changed_back);
	CHECK(server->translate("HELLO") == "Hello");

#ifdef DEBUG_ENABLED
	SIGNAL_WATCH(server, SNAME("key_missing"));
	(void)server->translate("NOT_THERE");
	(void)server->translate("NOT_THERE");
	Array missing;
	missing.push_back(Array({ StringName("NOT_THERE"), "en" }));
	SIGNAL_CHECK("key_missing", missing); // Once per key.
	SIGNAL_UNWATCH(server, SNAME("key_missing"));
#endif

	SIGNAL_UNWATCH(server, SNAME("catalogs_changed"));
	SIGNAL_UNWATCH(server, SNAME("locale_changed"));
	SIGNAL_UNWATCH(server, SNAME("locale_loaded"));
}

TEST_CASE("[TextLocalizationServer] Catalog lists and on-demand locales") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	LocalizationServer::set_locale("en");

	const String folder = TestUtils::get_temp_path("text_localization_server");
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	REQUIRE(dir->make_dir_recursive(folder.path_join("mods/a/translations")) == OK);
	Ref<TextLocalizationCatalog> game = make_game_catalog();
	game->set_text("HELLO", "fr", "Bonjour");
	REQUIRE(game->save_to_file(folder.path_join("game.lcat")) == OK);
	REQUIRE(make_mod_catalog()->save_to_file(folder.path_join("mods/a/translations/mod.lcat")) == OK);

	Ref<LocalizationCatalogList> list;
	list.instantiate();
	list->add_entry(folder.path_join("*.lcat"));
	list->add_entry(folder.path_join("mods/*/translations/*.lcat"), 100);
	list->add_entry(folder.path_join("missing/*/translations/*.lcat"), 100);
	CHECK(server->add_catalog_list(list) == 2);

	const Ref<LocalizationCatalog> loaded = server->get_catalogs()[0];
	REQUIRE(loaded.is_valid());
	CHECK(loaded->is_locale_loaded("en"));
	CHECK_FALSE(loaded->is_locale_loaded("fr")); // Not in the chain: never read.

	LocalizationServer::set_locale("de");
	CHECK(server->translate("HELLO") == "Servus");
	CHECK(loaded->is_locale_loaded("de"));
	CHECK(server->translate_for_locale("fr", "HELLO") == "Bonjour"); // Read on demand.

	LocalizationServer::set_locale("fr");
	CHECK(server->translate("HELLO") == "Bonjour");
	CHECK_FALSE(loaded->is_locale_loaded("de")); // Unloaded with the locale change.
}

} // namespace TestTextLocalizationServer
