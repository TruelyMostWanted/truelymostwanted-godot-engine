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

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/files_manifest.h"
#include "core/io/files_manifest_server.h"
#include "core/io/resource_loader.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/string/localization_catalog.h"
#include "core/string/text_insertion_server.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation_domain.h"
#include "core/string/translation_server.h"
#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
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
	TranslationServer *translation_server = TranslationServer::get_singleton();
	String previous_locale = TranslationServer::get_singleton()->get_locale();

	~ServerState() {
		server->clear_catalogs();
		for (const String &channel : translation_server->get_locale_channels()) {
			translation_server->clear_locale_for(channel);
		}
		translation_server->set_locale(previous_locale);
		MessageQueue::get_singleton()->flush();
	}
};

TEST_CASE("[TextLocalizationServer] Locale chain") {
	const String fallback = TranslationServer::get_singleton()->get_fallback_locale();
	const PackedStringArray chain = LocalizationServer::build_locale_chain("de_DE", PackedStringArray());
	REQUIRE(chain.size() >= 3);
	CHECK(chain[0] == "de_DE");
	CHECK(chain[1] == "de");
	CHECK(chain[2] == fallback);
	CHECK(LocalizationServer::build_locale_chain(fallback, PackedStringArray()) == PackedStringArray({ fallback }));
	// The closest other locale comes before the fallback (one rule for texts, resources and language settings).
	CHECK(LocalizationServer::build_locale_chain("de_DE", PackedStringArray({ "fr", "de_AT" }), false) == PackedStringArray({ "de_DE", "de", "de_AT" }));
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

	state.translation_server->set_locale("de_DE");
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
	CHECK(String(state.translation_server->translate("HELLO")) == "Moin");
	CHECK(String(state.translation_server->translate("HELLO", "menu")) == String(U"Hallo (Menü)"));
	CHECK(String(state.translation_server->translate("UNKNOWN")) == "UNKNOWN");

	state.translation_server->set_locale("en");
	CHECK(server->translate("HELLO") == "Hello");
	CHECK(String(state.translation_server->translate("HELLO")) == "Hello");

	server->remove_catalog(mod);
	CHECK_FALSE(server->has_catalog(mod));
	CHECK(server->translate_for_locale("de", "HELLO") == "Hallo");
}

TEST_CASE("[TextLocalizationServer] The closest other locale before the fallback") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	Ref<TextLocalizationCatalog> austrian;
	austrian.instantiate();
	austrian->set_text("HELLO", "de_AT", "Servus");
	austrian->set_text("HELLO", "en", "Hello");
	server->add_catalog(austrian);
	state.translation_server->set_locale("de_DE");
	CHECK(server->get_locale_chain("de_DE") == LocalizationServer::build_locale_chain("de_DE", PackedStringArray({ "de_AT", "en" })));
	CHECK(server->translate("HELLO") == "Servus"); // Not the fallback: the same rule as resources.
}

TEST_CASE("[TextLocalizationServer] Locale channels") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	TranslationServer *translation_server = state.translation_server;
	Ref<TextLocalizationCatalog> ui = make_game_catalog();
	Ref<TextLocalizationCatalog> subtitles;
	subtitles.instantiate();
	subtitles->set_channel("subtitles");
	subtitles->set_text("LINE_1", "en", "Where are we?");
	subtitles->set_text("LINE_1", "de", "Wo sind wir?");
	subtitles->set_text("LINE_1", "ja", String(U"ここはどこ？"));
	server->add_catalog(ui);
	server->add_catalog(subtitles);
	translation_server->set_locale("ja");

	SIGNAL_WATCH(translation_server, SNAME("locale_changed"));
	translation_server->set_locale_for("interface", "de");
	Array interface_changed;
	interface_changed.push_back(Array({ StringName("interface"), "de", "ja" }));
	SIGNAL_CHECK("locale_changed", interface_changed);
	translation_server->set_locale_for("subtitles", "en");
	CHECK(translation_server->get_locale_for("interface") == "de");
	CHECK(translation_server->get_locale_for("audio") == "ja"); // Follows the general locale.
	CHECK(translation_server->get_locale_for(StringName()) == "ja");
	CHECK(translation_server->get_locale_channels().has("subtitles"));

	CHECK(server->translate("HELLO") == "Hallo");
	CHECK(server->translate("LINE_1") == "Where are we?");
	CHECK(String(translation_server->translate("HELLO")) == "Hallo"); // tr() follows `interface`.
	CHECK(translation_server->get_main_domain()->get_locale_override() == "de");
	// A channel is a translation domain: the subtitles catalog answers in the domain "subtitles".
	const Ref<TranslationDomain> subtitles_domain = translation_server->get_or_add_domain("subtitles");
	CHECK(subtitles_domain->get_locale_override() == "en");
	CHECK(String(subtitles_domain->translate("LINE_1", StringName())) == "Where are we?");
	CHECK(String(translation_server->translate("LINE_1")) == "LINE_1"); // Not a key of the main domain.

	// Only the changed channel is rebuilt: interface texts stay, subtitles follow.
	const int hello = server->get_key_id("HELLO");
	translation_server->set_locale_for("subtitles", "de");
	CHECK(server->translate_id(hello) == "Hallo");
	CHECK(server->translate("LINE_1") == "Wo sind wir?");

	// A general change reaches the channels without their own locale.
	translation_server->clear_locale_for("subtitles");
	CHECK(server->translate("LINE_1") == String(U"ここはどこ？"));
	Array general_changed;
	general_changed.push_back(Array({ StringName(), "en", "ja" }));
	SIGNAL_DISCARD("locale_changed");
	translation_server->set_locale("en");
	SIGNAL_CHECK("locale_changed", general_changed);
	CHECK(server->translate("LINE_1") == "Where are we?");
	CHECK(server->translate("HELLO") == "Hallo"); // `interface` keeps its own locale.

	// The fallback locale is a TranslationServer signal too.
	SIGNAL_WATCH(translation_server, SNAME("fallback_locale_changed"));
	const String fallback = translation_server->get_fallback_locale();
	translation_server->set_fallback_locale("de");
	SIGNAL_CHECK("fallback_locale_changed", { { "de" } });
	translation_server->set_fallback_locale(fallback);
	SIGNAL_UNWATCH(translation_server, SNAME("fallback_locale_changed"));
	SIGNAL_UNWATCH(translation_server, SNAME("locale_changed"));
}

TEST_CASE("[SceneTree][TextLocalizationServer] Controls follow their channel") {
	class LocaleLabel : public Label {
	public:
		String get_shaping_locale() const { return _get_locale(); }
	};

	ServerState state;
	TranslationServer *translation_server = state.translation_server;
	Ref<TextLocalizationCatalog> ui;
	ui.instantiate();
	ui->set_text("TITLE", "en", "Hi");
	ui->set_text("TITLE", "de", "Guten Tag und willkommen");
	Ref<TextLocalizationCatalog> subtitles;
	subtitles.instantiate();
	subtitles->set_channel("subtitles");
	subtitles->set_text("LINE", "en", "Go");
	subtitles->set_text("LINE", "ja", String(U"行こう、みんな、早く行こう"));
	state.server->add_catalog(ui);
	state.server->add_catalog(subtitles);
	translation_server->set_locale("en");

	Window *root = SceneTree::get_singleton()->get_root();
	LocaleLabel *label = memnew(LocaleLabel);
	label->set_text("TITLE");
	root->add_child(label);
	// A subtree picks the subtitles channel through Godot's translation domain.
	Control *subtitle_box = memnew(Control);
	subtitle_box->set_translation_domain("subtitles");
	root->add_child(subtitle_box);
	LocaleLabel *subtitle = memnew(LocaleLabel);
	subtitle->set_text("LINE");
	subtitle_box->add_child(subtitle);
	SceneTree::get_singleton()->process(0);
	// The translated text (Label::xl_text) is what the label shows; get_total_character_count() reads it.
	const int length_en = label->get_total_character_count();
	const int subtitle_length_en = subtitle->get_total_character_count();
	CHECK(length_en == String("Hi").length());
	CHECK(label->get_shaping_locale() == "en");

	// Only the interface channel changes: the label translates again and shapes in German, the subtitle stays.
	translation_server->set_locale_for("interface", "de");
	SceneTree::get_singleton()->process(0);
	CHECK(label->get_total_character_count() == String("Guten Tag und willkommen").length());
	CHECK(label->get_shaping_locale() == "de");
	CHECK(subtitle->get_total_character_count() == subtitle_length_en);
	CHECK(subtitle->get_shaping_locale() == "en");

	// Only the subtitles channel changes.
	translation_server->set_locale_for("subtitles", "ja");
	SceneTree::get_singleton()->process(0);
	CHECK(subtitle->get_total_character_count() == String(U"行こう、みんな、早く行こう").length());
	CHECK(subtitle->get_shaping_locale() == "ja");
	CHECK(label->get_shaping_locale() == "de");

	// An audio-only change does not notify controls: a changed catalog text is not picked up.
	translation_server->clear_locale_for("interface");
	SceneTree::get_singleton()->process(0);
	CHECK(label->get_total_character_count() == length_en);
	ui->set_text("TITLE", "en", "Hi there, this is a much longer title");
	MessageQueue::get_singleton()->flush();
	translation_server->set_locale_for("audio", "ja");
	SceneTree::get_singleton()->process(0);
	CHECK(label->get_total_character_count() == length_en);
	CHECK(label->get_shaping_locale() == "en");

	memdelete(subtitle_box);
	memdelete(label);
}

TEST_CASE("[TextLocalizationServer] Key ids, plurals and arguments") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	Ref<TextLocalizationCatalog> mod = make_mod_catalog();
	server->add_catalog(make_game_catalog());
	server->add_catalog(mod, 10);
	state.translation_server->set_locale("de");

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
	CHECK(String(state.translation_server->translate_plural("APPLE", "APPLES", 5)) == String(U"{n} Äpfel"));
	CHECK(server->translate_for_locale("de", "APPLE", "", 1) == "{n} Apfel");

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
	state.translation_server->set_locale("de");

	CHECK(server->get_translation_keys("Servus") == PackedStringArray({ "HELLO" }));
	CHECK(server->get_translation_keys("nothing").is_empty());
	CHECK(server->get_translation_keys("Bye") == PackedStringArray({ "BYE" })); // Through the fallback.

	const PackedStringArray missing = server->get_missing_keys("de");
	CHECK(missing.has("BYE"));
	CHECK_FALSE(missing.has("HELLO"));
	CHECK(server->get_missing_keys() == missing); // The locale of each key's channel.
	CHECK(server->get_missing_keys("en").has("MOD_ONLY"));
	CHECK(server->get_available_locales().has("de"));
}

TEST_CASE("[TextLocalizationServer] Signals") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	state.translation_server->set_locale("en");

	SIGNAL_WATCH(server, SNAME("catalogs_changed"));
	SIGNAL_WATCH(server, SNAME("locale_loaded"));
	server->add_catalog(make_game_catalog());
	SIGNAL_CHECK("catalogs_changed", { {} });
	SIGNAL_CHECK("locale_loaded", { { "en" } });

	state.translation_server->set_locale("de");
	SIGNAL_CHECK("locale_loaded", { { "de" } });
	CHECK(server->translate("HELLO") == "Hallo");

	// The same locale again: nothing.
	state.translation_server->set_locale("de");
	SIGNAL_CHECK_FALSE("locale_loaded");

#ifdef DEBUG_ENABLED
	SIGNAL_WATCH(server, SNAME("key_missing"));
	(void)server->translate("NOT_THERE");
	(void)server->translate("NOT_THERE");
	Array missing;
	missing.push_back(Array({ StringName("NOT_THERE"), "de" }));
	SIGNAL_CHECK("key_missing", missing); // Once per key.
	SIGNAL_UNWATCH(server, SNAME("key_missing"));
#endif

	SIGNAL_UNWATCH(server, SNAME("catalogs_changed"));
	SIGNAL_UNWATCH(server, SNAME("locale_loaded"));
}

TEST_CASE("[TextLocalizationServer] Catalog manifests, mods and on-demand locales") {
	ServerState state;
	TextLocalizationServer *server = state.server;
	state.translation_server->set_locale("en");

	const String folder = TestUtils::get_temp_path("text_localization_server");
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	REQUIRE(dir->make_dir_recursive(folder.path_join("mods/a/translations")) == OK);
	REQUIRE(dir->make_dir_recursive(folder.path_join("mods/b/translations")) == OK);
	Ref<TextLocalizationCatalog> game = make_game_catalog();
	game->set_text("HELLO", "fr", "Bonjour");
	REQUIRE(game->save_to_file(folder.path_join("game.lcat")) == OK);
	REQUIRE(make_mod_catalog()->save_to_file(folder.path_join("mods/a/translations/mod.json")) == OK);

	Ref<FilesManifest> base;
	base.instantiate();
	base->set_file_paths({ folder.path_join("game.lcat") });
	Ref<FilesManifest> mods;
	mods.instantiate();
	mods->set_type_hint("TextLocalizationCatalog");
	mods->set_folder_paths({ folder.path_join("mods/*/translations") });
	CHECK(server->add_catalog_manifest(base) == 1);
	CHECK(server->add_catalog_manifest(mods, 10) == 1);

	const Ref<LocalizationCatalog> loaded = server->get_catalogs()[0];
	REQUIRE(loaded.is_valid());
	CHECK(loaded->is_locale_loaded("en"));
	CHECK_FALSE(loaded->is_locale_loaded("fr")); // Not in the chain: never read.

	state.translation_server->set_locale("de");
	CHECK(server->translate("HELLO") == "Servus");
	CHECK(loaded->is_locale_loaded("de"));
	CHECK(server->translate_for_locale("fr", "HELLO") == "Bonjour"); // Read on demand, under the catalog lock.

	state.translation_server->set_locale("fr");
	CHECK(server->translate("HELLO") == "Bonjour");
	CHECK_FALSE(loaded->is_locale_loaded("de")); // Unloaded with the locale change.
	server->clear_catalogs();

	// The project setting: the list order is the priority. Loading twice gives the same layers (ResourceLoader cache).
	const Variant previous_manifests = ProjectSettings::get_singleton()->get_setting(LocalizationServer::SETTING_CATALOG_MANIFESTS, Array());
	ProjectSettings::get_singleton()->set_setting(LocalizationServer::SETTING_CATALOG_MANIFESTS, Array({ base, mods }));
	state.translation_server->set_locale("de");
	CHECK(server->load_project_catalogs() == 2);
	CHECK(server->load_project_catalogs() == 2);
	CHECK(server->get_catalogs().size() == 2);
	CHECK(server->translate("HELLO") == "Servus");

	// A new mod file in a watched manifest reloads the project catalogs.
	FilesManifestServer *manifest_server = FilesManifestServer::get_singleton();
	REQUIRE(manifest_server->has_manifest("localization_catalogs_1"));
	manifest_server->set_watching("localization_catalogs_1", true);
	Ref<TextLocalizationCatalog> second_mod;
	second_mod.instantiate();
	second_mod->set_text("MOD_B", "de", "Zweiter Mod");
	REQUIRE(second_mod->save_to_file(folder.path_join("mods/b/translations/mod_b.json")) == OK);
	manifest_server->poll();
	MessageQueue::get_singleton()->flush();
	CHECK(server->get_catalogs().size() == 3);
	CHECK(server->translate("MOD_B") == "Zweiter Mod");
	manifest_server->set_watching("localization_catalogs_1", false);

	ProjectSettings::get_singleton()->set_setting(LocalizationServer::SETTING_CATALOG_MANIFESTS, previous_manifests);
	for (const char *path : { "game.lcat", "mods/a/translations/mod.json", "mods/b/translations/mod_b.json" }) {
		DirAccess::remove_absolute(folder.path_join(path));
	}
}

} // namespace TestTextLocalizationServer
