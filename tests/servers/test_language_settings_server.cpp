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
#include "core/string/translation.h"
#include "core/string/translation_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/language_settings.h"
#include "servers/settings/language_settings_server.h"
#include "servers/settings/settings_server_manager.h"
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
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
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
		translation_server->set_locale(original_locale);
		translation_server->set_pseudolocalization_enabled(original_pseudolocalization);
		ProjectSettings::get_singleton()->set_setting("internationalization/locale/test", "");
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
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
	server->retrieve();
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

} // namespace TestLanguageSettingsServer
