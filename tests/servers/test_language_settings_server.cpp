/**************************************************************************/
/*  test_language_settings_server.cpp                                     */
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

TEST_FORCE_LINK(test_language_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/message_queue.h"
#include "core/string/localization_catalog.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/language_settings.h"
#include "servers/settings/language_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestLanguageSettingsServer {

// The game has a French and a Japanese translation. Restores the locale of TranslationServer afterwards.
struct TestSetup {
	TranslationServer *translation_server = TranslationServer::get_singleton();
	String original_locale;
	bool original_pseudolocalization = false;
	Ref<Translation> french;
	Ref<Translation> japanese;

	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		original_locale = translation_server->get_locale();
		original_pseudolocalization = translation_server->is_pseudolocalization_enabled();
		french.instantiate();
		french->set_locale("fr");
		translation_server->add_translation(french);
		japanese.instantiate();
		japanese->set_locale("ja");
		translation_server->add_translation(japanese);
	}
	~TestSetup() {
		translation_server->remove_translation(french);
		translation_server->remove_translation(japanese);
		for (const char *channel : { "interface", "audio", "subtitles" }) {
			translation_server->clear_locale_for(channel);
		}
		translation_server->set_locale(original_locale);
		translation_server->set_pseudolocalization_enabled(original_pseudolocalization);
		ProjectSettings::get_singleton()->set_setting("internationalization/locale/test", "");
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("language_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static String project_locale() {
	return TranslationServer::get_singleton()->standardize_locale(LanguageSettingsServer::get_project_locale());
}

TEST_CASE("[LanguageSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	LanguageSettingsServer *server = LanguageSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("LanguageSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("language") == server);
	CHECK(server->get_domain() == StringName("language"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_file_path() == "user://settings/language.ini");
	CHECK(server->get_setting_names().has("locale"));
	CHECK_FALSE(server->get_setting_names().has("use_system_locale_for_numbers"));

	// The selectable locales come from the loaded translations.
	const PackedStringArray locales = LanguageSettingsServer::get_available_locales();
	CHECK(locales.has("fr"));
	CHECK(locales.has("ja"));
	CHECK(locales.find("fr") < locales.find("ja"));
	CHECK(LanguageSettingsServer::get_command_line_locale().is_empty());
}

TEST_CASE("[LanguageSettingsServer] The locale is applied at once, saved and applied again on startup") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	const String path = temp_file("language.ini");
	{
		LanguageSettingsServer *server = memnew(LanguageSettingsServer);
		server->set_file_path(path);
		// The default follows the OS.
		CHECK(server->get_locale().is_empty());
		CHECK(server->get_default_value("locale") == Variant(String()));

		server->set_locale("fr-FR");
		CHECK(server->get_locale() == "fr_FR");
		CHECK(translation_server->get_locale() == "fr_FR");
		CHECK(server->save() == OK);

		server->set_locale("");
		CHECK(translation_server->get_locale() == project_locale());
		memdelete(server);
	}

	// "Restart": the engine is back at the OS locale.
	translation_server->set_locale(project_locale());
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(translation_server->get_locale() == "fr_FR");
	CHECK(server->get_locale() == "fr_FR");
	CHECK_FALSE(server->has_session_override("locale"));
	CHECK_FALSE(server->is_dirty());
	CHECK_FALSE(server->has_unsaved_changes());
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] A saved locale without a loaded translation falls back without an error") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	const String path = temp_file("missing.json");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"locale\": \"sv\" }");
	}
	translation_server->set_locale("ja");
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(translation_server->get_locale() == project_locale());
	// The stored value is kept, so it works again once the translation exists.
	CHECK(server->get_locale() == "sv");

	// A locale that matches a translation by language is used as it is.
	server->set_locale("fr_CA");
	CHECK(translation_server->get_locale() == "fr_CA");
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Locales of localization catalogs are selectable without translations") {
	TranslationServer *translation_server = TranslationServer::get_singleton();
	TextLocalizationServer *text_server = TextLocalizationServer::get_singleton();
	REQUIRE(text_server != nullptr);
	const String original_locale = translation_server->get_locale();
	ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);

	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_text("HELLO", "en", "Hello");
	catalog->set_text("HELLO", "fr", "Bonjour");
	text_server->add_catalog(catalog);
	translation_server->set_locale("en_US");

	// TranslationServer only knows the tr() adapter, whose locale is the active one.
	CHECK_FALSE(translation_server->get_loaded_locales().has("fr"));
	const PackedStringArray locales = LanguageSettingsServer::get_available_locales();
	CHECK(locales.has("en"));
	CHECK(locales.has("fr"));
	CHECK_FALSE(locales.has("en_US"));
	CHECK(locales.find("en") < locales.find("fr"));

	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_locale("fr");
	CHECK(translation_server->get_locale() == "fr");
	CHECK(text_server->translate("HELLO") == "Bonjour");
	memdelete(server);

	text_server->clear_catalogs();
	translation_server->set_locale(original_locale);
	MessageQueue::get_singleton()->flush();
	ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
}

TEST_CASE("[LanguageSettingsServer] The test locale wins at startup, a runtime change wins over it") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	const String path = temp_file("test_locale.ini");
	{
		LanguageSettingsServer *server = memnew(LanguageSettingsServer);
		server->set_file_path(path);
		server->set_locale("fr");
		CHECK(server->save() == OK);
		memdelete(server);
	}
	ProjectSettings::get_singleton()->set_setting("internationalization/locale/test", "ja");
	translation_server->set_locale("ja");

	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(translation_server->get_locale() == "ja");
	CHECK(server->has_session_override("locale"));
	CHECK(server->get_locale() == "fr");

	server->set_locale("fr");
	CHECK_FALSE(server->has_session_override("locale"));
	CHECK(translation_server->get_locale() == "fr");
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Locale channels are applied, saved and applied again on startup") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	const String path = temp_file("channels.ini");
	{
		LanguageSettingsServer *server = memnew(LanguageSettingsServer);
		server->set_file_path(path);
		CHECK(server->get_setting_names().has("audio_locale"));
		// Empty: same as the language.
		CHECK(server->get_locale_for("audio").is_empty());
		CHECK(server->get_default_value("subtitles_locale") == Variant(String()));

		server->set_locale("fr");
		server->set_locale_for("interface", "ja");
		server->set_locale_for("audio", "ja-JP");
		CHECK(server->get_locale_for(StringName()) == "fr");
		CHECK(server->get_locale_for("audio") == "ja_JP");
		CHECK(translation_server->get_locale() == "fr");
		CHECK(translation_server->get_locale_for("interface") == "ja");
		CHECK(translation_server->get_locale_for("audio") == "ja_JP");
		CHECK(translation_server->get_locale_for("subtitles") == "fr");
		CHECK(server->save() == OK);

		server->set_locale_for("interface", "");
		CHECK(translation_server->get_locale_for("interface") == "fr");
		// Custom channels are not stored.
		ERR_PRINT_OFF;
		server->set_locale_for("custom", "ja");
		ERR_PRINT_ON;
		CHECK(translation_server->get_locale_for("custom") == "fr");
		memdelete(server);
	}

	// "Restart": the engine follows the OS locale on every channel.
	translation_server->clear_locale_for("audio");
	translation_server->set_locale(project_locale());
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(translation_server->get_locale() == "fr");
	CHECK(translation_server->get_locale_for("interface") == "ja");
	CHECK(translation_server->get_locale_for("audio") == "ja_JP");
	CHECK(translation_server->get_locale_for("subtitles") == "fr");
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Files without channels load, the test locale makes the channels follow it") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	const String old_path = temp_file("before_channels.json");
	{
		Ref<FileAccess> file = FileAccess::open(old_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"locale\": \"fr\" }");
	}
	LanguageSettingsServer *old_server = memnew(LanguageSettingsServer);
	old_server->set_file_path(old_path);
	old_server->startup();
	CHECK(old_server->get_locale() == "fr");
	CHECK(old_server->get_locale_for("interface").is_empty());
	CHECK(old_server->get_locale_for("subtitles").is_empty());
	CHECK(translation_server->get_locale_for("audio") == "fr");
	memdelete(old_server);

	const String path = temp_file("channels_test_locale.ini");
	{
		LanguageSettingsServer *server = memnew(LanguageSettingsServer);
		server->set_file_path(path);
		server->set_locale("fr");
		server->set_locale_for("audio", "fr");
		CHECK(server->save() == OK);
		memdelete(server);
	}
	ProjectSettings::get_singleton()->set_setting("internationalization/locale/test", "ja");
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(translation_server->get_locale() == "ja");
	CHECK(translation_server->get_locale_for("audio") == "ja");
	CHECK(server->has_session_override("audio_locale"));
	CHECK(server->get_locale_for("audio") == "fr");

	server->set_locale_for("audio", "fr");
	CHECK_FALSE(server->has_session_override("audio_locale"));
	CHECK(translation_server->get_locale_for("audio") == "fr");
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Channel locales are checked against the catalogs of the channel") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	TextLocalizationServer *text_server = TextLocalizationServer::get_singleton();
	REQUIRE(text_server != nullptr);
	Ref<TextLocalizationCatalog> interface_texts;
	interface_texts.instantiate();
	interface_texts->set_text("HELLO", "en", "Hello");
	interface_texts->set_text("HELLO", "de", "Hallo");
	Ref<TextLocalizationCatalog> subtitles;
	subtitles.instantiate();
	subtitles->set_channel("subtitles");
	subtitles->set_text("LINE_1", "en", "Hi");
	subtitles->set_text("LINE_1", "es", "Hola");
	text_server->add_catalog(interface_texts);
	text_server->add_catalog(subtitles);

	CHECK(text_server->get_available_locales_for("subtitles") == PackedStringArray({ "en", "es" }));
	CHECK(text_server->get_available_locales_for("audio").is_empty());
	CHECK(LanguageSettingsServer::get_available_locales_for("subtitles") == PackedStringArray({ "en", "es" }));
	const PackedStringArray interface_locales = LanguageSettingsServer::get_available_locales_for("interface");
	CHECK(interface_locales.has("de"));
	CHECK(interface_locales.has("fr")); // Godot translations belong to the interface.
	CHECK_FALSE(interface_locales.has("es"));
	// No catalog declares the channel: every locale.
	CHECK(LanguageSettingsServer::get_available_locales_for("audio") == LanguageSettingsServer::get_available_locales());
	CHECK(LanguageSettingsServer::get_available_locales().has("es"));

	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_locale("en");
	server->set_locale_for("subtitles", "es");
	CHECK(translation_server->get_locale_for("subtitles") == "es");
	// No subtitles in German: the channel follows the language, the stored value is kept.
	server->set_locale_for("subtitles", "de");
	CHECK(translation_server->get_locale_for("subtitles") == "en");
	CHECK(server->get_locale_for("subtitles") == "de");
	memdelete(server);

	text_server->clear_catalogs();
	MessageQueue::get_singleton()->flush();
}

TEST_CASE("[LanguageSettingsServer] Pseudolocalization reaches TranslationServer") {
	TestSetup setup;
	TranslationServer *translation_server = setup.translation_server;
	translation_server->set_pseudolocalization_enabled(false);
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	CHECK_FALSE(server->is_pseudolocalization_enabled());
	server->set_pseudolocalization_enabled(true);
	CHECK(translation_server->is_pseudolocalization_enabled());
	server->reset_to_defaults();
	CHECK_FALSE(translation_server->is_pseudolocalization_enabled());
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Root window values wait until a SceneTree exists") {
	TestSetup setup;
	REQUIRE(SceneTree::get_singleton() == nullptr);
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	CHECK(server->get_layout_direction() == Window::LAYOUT_DIRECTION_INHERITED);
	server->set_layout_direction(Window::LAYOUT_DIRECTION_RTL);
	CHECK(server->get_layout_direction() == Window::LAYOUT_DIRECTION_RTL);
	// Kept for the root window and read back from there.
	server->read_from_engine();
	CHECK(server->get_layout_direction() == Window::LAYOUT_DIRECTION_RTL);

	// Values out of range are rejected.
	ERR_PRINT_OFF;
	server->set_value("layout_direction", 9);
	ERR_PRINT_ON;
	CHECK(server->get_layout_direction() == Window::LAYOUT_DIRECTION_RTL);
	memdelete(server);
}

TEST_CASE("[SceneTree][LanguageSettingsServer] Layout direction and auto-translate mode reach the root window") {
	TestSetup setup;
	Window *root = SceneTree::get_singleton()->get_root().ptr();
	REQUIRE(root != nullptr);
	const Window::LayoutDirection original_direction = root->get_layout_direction();
	const Node::AutoTranslateMode original_mode = root->get_auto_translate_mode();

	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	CHECK(server->get_layout_direction() == original_direction);
	CHECK(server->get_root_auto_translate_mode() == original_mode);

	server->set_layout_direction(Window::LAYOUT_DIRECTION_RTL);
	CHECK(root->get_layout_direction() == Window::LAYOUT_DIRECTION_RTL);
	server->set_root_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
	CHECK(root->get_auto_translate_mode() == Node::AUTO_TRANSLATE_MODE_DISABLED);

	server->reset_to_defaults();
	CHECK(root->get_layout_direction() == original_direction);
	CHECK(root->get_auto_translate_mode() == original_mode);
	memdelete(server);
}

TEST_CASE("[LanguageSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	LanguageSettingsServer *server = memnew(LanguageSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "locale_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("locale", String("fr_FR"));
	SIGNAL_CHECK_FALSE("locale_changed");

	CHECK(server->apply() == OK);
	const Array args = { { String("fr_FR") } };
	SIGNAL_CHECK("locale_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("locale_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("locale");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("locale_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("locale_changed", default_args);

	SIGNAL_UNWATCH(server, "locale_changed");
	memdelete(server);
}

} // namespace TestLanguageSettingsServer
