/**************************************************************************/
/*  locale_option_button.cpp                                              */
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

#include "locale_option_button.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/localization_server.h"
#include "core/string/translation_server.h"
#include "scene/gui/option_button.h"
#include "servers/settings/language_settings_server.h"

// Names of languages in their own language. Keys with a `_` are whole locales that differ from their language.
static const char *native_names[][2] = {
	{ "af", "Afrikaans" },
	{ "am", "አማርኛ" },
	{ "ar", "العربية" },
	{ "az", "Azərbaycanca" },
	{ "be", "Беларуская" },
	{ "bg", "Български" },
	{ "bn", "বাংলা" },
	{ "bs", "Bosanski" },
	{ "ca", "Català" },
	{ "cs", "Čeština" },
	{ "cy", "Cymraeg" },
	{ "da", "Dansk" },
	{ "de", "Deutsch" },
	{ "el", "Ελληνικά" },
	{ "en", "English" },
	{ "eo", "Esperanto" },
	{ "es", "Español" },
	{ "et", "Eesti" },
	{ "eu", "Euskara" },
	{ "fa", "فارسی" },
	{ "fi", "Suomi" },
	{ "fil", "Filipino" },
	{ "fr", "Français" },
	{ "ga", "Gaeilge" },
	{ "gl", "Galego" },
	{ "gu", "ગુજરાતી" },
	{ "he", "עברית" },
	{ "hi", "हिन्दी" },
	{ "hr", "Hrvatski" },
	{ "hu", "Magyar" },
	{ "hy", "Հայերեն" },
	{ "id", "Bahasa Indonesia" },
	{ "is", "Íslenska" },
	{ "it", "Italiano" },
	{ "ja", "日本語" },
	{ "ka", "ქართული" },
	{ "kk", "Қазақша" },
	{ "km", "ខ្មែរ" },
	{ "kn", "ಕನ್ನಡ" },
	{ "ko", "한국어" },
	{ "ky", "Кыргызча" },
	{ "lb", "Lëtzebuergesch" },
	{ "lo", "ລາວ" },
	{ "lt", "Lietuvių" },
	{ "lv", "Latviešu" },
	{ "mk", "Македонски" },
	{ "ml", "മലയാളം" },
	{ "mn", "Монгол" },
	{ "mr", "मराठी" },
	{ "ms", "Bahasa Melayu" },
	{ "mt", "Malti" },
	{ "my", "မြန်မာ" },
	{ "nb", "Norsk bokmål" },
	{ "ne", "नेपाली" },
	{ "nl", "Nederlands" },
	{ "nn", "Norsk nynorsk" },
	{ "no", "Norsk" },
	{ "pa", "ਪੰਜਾਬੀ" },
	{ "pl", "Polski" },
	{ "ps", "پښتو" },
	{ "pt", "Português" },
	{ "pt_BR", "Português (Brasil)" },
	{ "pt_PT", "Português (Portugal)" },
	{ "ro", "Română" },
	{ "ru", "Русский" },
	{ "si", "සිංහල" },
	{ "sk", "Slovenčina" },
	{ "sl", "Slovenščina" },
	{ "sq", "Shqip" },
	{ "sr", "Српски" },
	{ "sv", "Svenska" },
	{ "sw", "Kiswahili" },
	{ "ta", "தமிழ்" },
	{ "te", "తెలుగు" },
	{ "tg", "Тоҷикӣ" },
	{ "th", "ไทย" },
	{ "tk", "Türkmençe" },
	{ "tl", "Tagalog" },
	{ "tr", "Türkçe" },
	{ "uk", "Українська" },
	{ "ur", "اردو" },
	{ "uz", "Oʻzbekcha" },
	{ "vi", "Tiếng Việt" },
	{ "zh", "中文" },
	{ "zh_CN", "简体中文" },
	{ "zh_Hans", "简体中文" },
	{ "zh_Hans_CN", "简体中文" },
	{ "zh_Hant", "繁體中文" },
	{ "zh_Hant_TW", "繁體中文" },
	{ "zh_HK", "繁體中文 (香港)" },
	{ "zh_TW", "繁體中文" },
	{ "zu", "isiZulu" },
};

static bool _is_builtin_channel(const StringName &p_channel) {
	return p_channel == StringName() || p_channel == SNAME("interface") || p_channel == SNAME("audio") || p_channel == SNAME("subtitles");
}

// The country part of a locale (`de_AT`, `zh_Hans_CN`), or an empty string.
static String _get_country_code(const String &p_locale) {
	const PackedStringArray parts = p_locale.split("_");
	for (int idx = 1; idx < parts.size(); idx++) {
		const String &part = parts[idx];
		if ((part.length() == 2 && part.to_upper() == part) || (part.length() == 3 && part.is_numeric())) {
			return part;
		}
	}
	return String();
}

String LocaleOptionButton::_get_native_language_name(const String &p_locale, bool &r_is_full) {
	const String language = p_locale.get_slicec('_', 0);
	String found;
	r_is_full = false;
	for (const auto &entry : native_names) {
		if (p_locale == entry[0]) {
			r_is_full = true;
			return String::utf8(entry[1]);
		}
		if (language == entry[0]) {
			found = String::utf8(entry[1]);
		}
	}
	return found;
}

String LocaleOptionButton::get_native_name(const String &p_locale, bool p_region) {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	ERR_FAIL_NULL_V(translation_server, p_locale);
	const String locale = translation_server->standardize_locale(p_locale);
	bool is_full = false;
	String name = _get_native_language_name(locale, is_full);
	if (name.is_empty()) {
		name = translation_server->get_locale_name(locale);
		return name.is_empty() ? locale : name;
	}
	const String country = _get_country_code(locale);
	if (p_region && !is_full && !country.is_empty()) {
		const String country_name = translation_server->get_country_name(country);
		name += " (" + (country_name.is_empty() ? country : country_name) + ")";
	}
	return name;
}

void LocaleOptionButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			TranslationServer *translation_server = TranslationServer::get_singleton();
			if (translation_server != nullptr && !connected && !Engine::get_singleton()->is_editor_hint()) {
				translation_server->connect(SNAME("locale_changed"), callable_mp(this, &LocaleOptionButton::_on_locale_changed));
				connected = true;
			}
			refresh();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			TranslationServer *translation_server = TranslationServer::get_singleton();
			if (translation_server != nullptr && connected) {
				translation_server->disconnect(SNAME("locale_changed"), callable_mp(this, &LocaleOptionButton::_on_locale_changed));
			}
			connected = false;
		} break;
	}
}

void LocaleOptionButton::_on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale) {
	// The general locale reaches every channel without an own locale.
	if (p_channel == target || p_channel == StringName()) {
		_select_current();
	}
}

void LocaleOptionButton::_on_item_selected(int p_idx) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const String locale = get_item_metadata(p_idx);
	LanguageSettingsServer *language_settings = LanguageSettingsServer::get_singleton();
	if (language_settings != nullptr && _is_builtin_channel(target)) {
		language_settings->set_locale_for(target, locale);
	} else if (TranslationServer *translation_server = TranslationServer::get_singleton()) {
		translation_server->set_locale_for(target, locale);
	}
	// A locale that the settings refused leaves the selection on the locale that is active.
	_select_current();
	emit_signal(SNAME("locale_selected"), locale);
}

void LocaleOptionButton::_select_current() {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	if (translation_server != nullptr) {
		select_locale(translation_server->get_locale_for(target));
	}
}

void LocaleOptionButton::_refresh_if_inside_tree() {
	if (is_inside_tree()) {
		refresh();
	}
}

void LocaleOptionButton::refresh() {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	if (translation_server == nullptr || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	PackedStringArray locales = LanguageSettingsServer::get_available_locales_for(target);
	const String fallback = translation_server->get_fallback_locale();
	if (include_fallback && !fallback.is_empty() && !locales.has(fallback)) {
		locales.push_back(fallback);
		locales.sort();
	}
	// Locales of one language (`de_DE`, `de_AT`) are told apart by their country.
	HashMap<String, int> languages;
	for (const String &locale : locales) {
		languages[locale.get_slicec('_', 0)]++;
	}
	clear();
	for (const String &locale : locales) {
		const bool region = show_region || languages[locale.get_slicec('_', 0)] > 1;
		add_item(show_native_names ? get_native_name(locale, region) : translation_server->get_locale_name(locale));
		set_item_metadata(get_item_count() - 1, locale);
	}
	_select_current();
}

PackedStringArray LocaleOptionButton::get_locales() const {
	PackedStringArray locales;
	for (int idx = 0; idx < get_item_count(); idx++) {
		locales.push_back(get_item_metadata(idx));
	}
	return locales;
}

String LocaleOptionButton::get_selected_locale() const {
	const int selected = get_selected();
	return selected < 0 ? String() : String(get_item_metadata(selected));
}

void LocaleOptionButton::select_locale(const String &p_locale) {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	const PackedStringArray locales = get_locales();
	const String locale = translation_server != nullptr ? translation_server->standardize_locale(p_locale) : p_locale;
	int found = locales.find(locale);
	if (found < 0) {
		// The same matching rule as the localization servers: the parents and the closest other locale.
		for (const String &candidate : LocalizationServer::build_locale_chain(locale, locales, false)) {
			found = locales.find(candidate);
			if (found >= 0) {
				break;
			}
		}
	}
	select(found);
}

void LocaleOptionButton::set_target(const StringName &p_target) {
	if (target == p_target) {
		return;
	}
	target = p_target;
	_refresh_if_inside_tree();
}

StringName LocaleOptionButton::get_target() const {
	return target;
}

void LocaleOptionButton::set_show_native_names(bool p_show) {
	if (show_native_names != p_show) {
		show_native_names = p_show;
		_refresh_if_inside_tree();
	}
}

bool LocaleOptionButton::is_showing_native_names() const {
	return show_native_names;
}

void LocaleOptionButton::set_show_region(bool p_show) {
	if (show_region != p_show) {
		show_region = p_show;
		_refresh_if_inside_tree();
	}
}

bool LocaleOptionButton::is_showing_region() const {
	return show_region;
}

void LocaleOptionButton::set_include_fallback(bool p_include) {
	if (include_fallback != p_include) {
		include_fallback = p_include;
		_refresh_if_inside_tree();
	}
}

bool LocaleOptionButton::is_including_fallback() const {
	return include_fallback;
}

void LocaleOptionButton::_bind_methods() {
	ClassDB::bind_static_method("LocaleOptionButton", D_METHOD("get_native_name", "locale", "include_region"), &LocaleOptionButton::get_native_name, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_target", "target"), &LocaleOptionButton::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &LocaleOptionButton::get_target);
	ClassDB::bind_method(D_METHOD("set_show_native_names", "show"), &LocaleOptionButton::set_show_native_names);
	ClassDB::bind_method(D_METHOD("is_showing_native_names"), &LocaleOptionButton::is_showing_native_names);
	ClassDB::bind_method(D_METHOD("set_show_region", "show"), &LocaleOptionButton::set_show_region);
	ClassDB::bind_method(D_METHOD("is_showing_region"), &LocaleOptionButton::is_showing_region);
	ClassDB::bind_method(D_METHOD("set_include_fallback", "include"), &LocaleOptionButton::set_include_fallback);
	ClassDB::bind_method(D_METHOD("is_including_fallback"), &LocaleOptionButton::is_including_fallback);

	ClassDB::bind_method(D_METHOD("refresh"), &LocaleOptionButton::refresh);
	ClassDB::bind_method(D_METHOD("get_locales"), &LocaleOptionButton::get_locales);
	ClassDB::bind_method(D_METHOD("get_selected_locale"), &LocaleOptionButton::get_selected_locale);
	ClassDB::bind_method(D_METHOD("select_locale", "locale"), &LocaleOptionButton::select_locale);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "target", PROPERTY_HINT_ENUM_SUGGESTION, "interface,audio,subtitles"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_native_names"), "set_show_native_names", "is_showing_native_names");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_region"), "set_show_region", "is_showing_region");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_fallback"), "set_include_fallback", "is_including_fallback");

	ADD_SIGNAL(MethodInfo("locale_selected", PropertyInfo(Variant::STRING, "locale")));
}

LocaleOptionButton::LocaleOptionButton() {
	// Native names must not go through the translation of the interface.
	set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	connect(SNAME("item_selected"), callable_mp(this, &LocaleOptionButton::_on_item_selected));
}
