/**************************************************************************/
/*  test_locale_option_button.cpp                                         */
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

TEST_FORCE_LINK(test_locale_option_button)

#ifndef ADVANCED_GUI_DISABLED

#include "core/config/project_settings.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"
#include "core/templates/local_vector.h"
#include "scene/gui/locale_option_button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/language_settings_server.h"
#include "tests/signal_watcher.h"

namespace TestLocaleOptionButton {

// The game has French, Japanese and two German translations. Restores the locales afterwards.
struct TestSetup {
	TranslationServer *translation_server = TranslationServer::get_singleton();
	String original_locale = translation_server->get_locale();
	LocalVector<Ref<Translation>> translations;

	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		for (const char *locale : { "fr", "ja", "de_DE", "de_AT" }) {
			Ref<Translation> translation;
			translation.instantiate();
			translation->set_locale(locale);
			translation_server->add_translation(translation);
			translations.push_back(translation);
		}
	}
	~TestSetup() {
		for (const Ref<Translation> &translation : translations) {
			translation_server->remove_translation(translation);
		}
		for (const char *channel : { "interface", "audio", "subtitles" }) {
			LanguageSettingsServer::get_singleton()->set_locale_for(channel, String());
			translation_server->clear_locale_for(channel);
		}
		LanguageSettingsServer::get_singleton()->set_locale(String());
		translation_server->set_locale(original_locale);
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

TEST_CASE("[LocaleOptionButton] Native names") {
	CHECK(LocaleOptionButton::get_native_name("de") == String::utf8("Deutsch"));
	CHECK(LocaleOptionButton::get_native_name("fr") == String::utf8("Français"));
	CHECK(LocaleOptionButton::get_native_name("ja") == String::utf8("日本語"));
	CHECK(LocaleOptionButton::get_native_name("pt_BR") == String::utf8("Português (Brasil)"));
	CHECK(LocaleOptionButton::get_native_name("zh_TW") == String::utf8("繁體中文"));
	// The country is appended on request, but not to names that contain it.
	CHECK(LocaleOptionButton::get_native_name("de_AT") == String::utf8("Deutsch"));
	CHECK(LocaleOptionButton::get_native_name("de_AT", true).begins_with(String::utf8("Deutsch (")));
	CHECK(LocaleOptionButton::get_native_name("pt_BR", true) == String::utf8("Português (Brasil)"));
	// A language without an entry shows the English name of the engine.
	CHECK(LocaleOptionButton::get_native_name("ace") == TranslationServer::get_singleton()->get_locale_name("ace"));
}

TEST_CASE("[SceneTree][LocaleOptionButton] The list shows the locales of the project") {
	TestSetup setup;
	LocaleOptionButton *button = memnew(LocaleOptionButton);
	SceneTree::get_singleton()->get_root()->add_child(button);

	const PackedStringArray locales = button->get_locales();
	CHECK(locales.has("fr"));
	CHECK(locales.has("ja"));
	CHECK(button->get_item_count() == locales.size());
	CHECK(button->get_item_text(locales.find("fr")) == String::utf8("Français"));
	// Locales of one language are told apart by their country.
	CHECK(button->get_item_text(locales.find("de_AT")).begins_with(String::utf8("Deutsch (")));
	CHECK(button->get_item_text(locales.find("de_AT")) != button->get_item_text(locales.find("de_DE")));

	button->set_show_native_names(false);
	CHECK(button->get_item_text(button->get_locales().find("fr")) == setup.translation_server->get_locale_name("fr"));

	memdelete(button);
}

TEST_CASE("[SceneTree][LocaleOptionButton] The fallback locale is listed on request") {
	TestSetup setup;
	LocaleOptionButton *button = memnew(LocaleOptionButton);
	SceneTree::get_singleton()->get_root()->add_child(button);

	const String fallback = setup.translation_server->get_fallback_locale();
	const bool listed = button->get_locales().has(fallback);
	button->set_include_fallback(true);
	CHECK(button->get_locales().has(fallback));
	button->set_include_fallback(false);
	CHECK(button->get_locales().has(fallback) == listed);

	memdelete(button);
}

TEST_CASE("[SceneTree][LocaleOptionButton] Selecting an entry sets the locale of the target") {
	TestSetup setup;
	LocaleOptionButton *general = memnew(LocaleOptionButton);
	LocaleOptionButton *subtitles = memnew(LocaleOptionButton);
	subtitles->set_target("subtitles");
	SceneTree::get_singleton()->get_root()->add_child(general);
	SceneTree::get_singleton()->get_root()->add_child(subtitles);
	setup.translation_server->set_locale("fr");
	CHECK(general->get_selected_locale() == "fr");

	SIGNAL_WATCH(general, "locale_selected");
	general->emit_signal("item_selected", general->get_locales().find("ja"));
	CHECK(setup.translation_server->get_locale() == "ja");
	CHECK(general->get_selected_locale() == "ja");
	Array arguments;
	arguments.push_back(Array({ "ja" }));
	SIGNAL_CHECK("locale_selected", arguments);
	SIGNAL_UNWATCH(general, "locale_selected");

	// A channel changes alone; the button of the general locale keeps its entry.
	subtitles->emit_signal("item_selected", subtitles->get_locales().find("fr"));
	CHECK(setup.translation_server->get_locale_for("subtitles") == "fr");
	CHECK(setup.translation_server->get_locale() == "ja");
	CHECK(subtitles->get_selected_locale() == "fr");
	CHECK(general->get_selected_locale() == "ja");

	// The general locale reaches a channel without an own locale.
	setup.translation_server->clear_locale_for("subtitles");
	CHECK(subtitles->get_selected_locale() == "ja");

	memdelete(general);
	memdelete(subtitles);
}

} // namespace TestLocaleOptionButton

#endif // ADVANCED_GUI_DISABLED
