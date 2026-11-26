/**************************************************************************/
/*  language_settings_server.cpp                                          */
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

#include "language_settings_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation_server.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "servers/settings/language_settings.h"
#include "servers/settings/settings_server_manager.h"

static_assert(Window::LAYOUT_DIRECTION_INHERITED == 0 && Window::LAYOUT_DIRECTION_SYSTEM_LOCALE == 4, "LanguageSettings.layout_direction mirrors Window::LayoutDirection.");
static_assert(Node::AUTO_TRANSLATE_MODE_INHERIT == 0 && Node::AUTO_TRANSLATE_MODE_DISABLED == 2, "LanguageSettings.root_auto_translate_mode mirrors Node::AutoTranslateMode.");

LanguageSettingsServer *LanguageSettingsServer::singleton = nullptr;

LanguageSettingsServer *LanguageSettingsServer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Locale.

String LanguageSettingsServer::_get_test_locale() {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting("internationalization/locale/test")) {
		return String();
	}
	return String(project_settings->get_setting_with_override("internationalization/locale/test")).strip_edges();
}

String LanguageSettingsServer::get_project_locale() {
	const String test = _get_test_locale();
	return test.is_empty() ? OS::get_singleton()->get_locale() : test;
}

String LanguageSettingsServer::get_command_line_locale() {
	// Main::setup() consumes `--language`/`-l`, so it is not in OS::get_cmdline_args().
	return Main::get_locale_override().strip_edges();
}

PackedStringArray LanguageSettingsServer::get_available_locales() {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	if (translation_server == nullptr) {
		return PackedStringArray();
	}
	PackedStringArray locales = LocalizationServer::get_all_available_locales();
	// Skips the tr() adapter of the localization catalogs: its locale is always the active one.
	const TextLocalizationServer *text_server = TextLocalizationServer::get_singleton();
	for (const Ref<Translation> &translation : translation_server->get_main_domain()->get_translations()) {
		if ((text_server == nullptr || !text_server->is_adapter(translation)) && !locales.has(translation->get_locale())) {
			locales.push_back(translation->get_locale());
		}
	}
	locales.sort();
	return locales;
}

bool LanguageSettingsServer::_is_developer_locale(const String &p_locale) {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	const String command_line = get_command_line_locale();
	if (!command_line.is_empty() && translation_server->standardize_locale(command_line) == p_locale) {
		return true;
	}
	const String test = _get_test_locale();
	return !test.is_empty() && translation_server->standardize_locale(test) == p_locale;
}

String LanguageSettingsServer::_resolve_locale(const String &p_locale) {
	const PackedStringArray available = get_available_locales();
	if (available.is_empty()) {
		// No translations: the game may still use the locale for its own texts.
		return p_locale;
	}
	// The same matching rule as the localization servers: the locale, its parents or the closest other locale.
	for (const String &locale : LocalizationServer::build_locale_chain(p_locale, available, false)) {
		if (available.has(locale)) {
			return p_locale;
		}
	}
	// A translation that was removed, or a file from another version of the game: the normal case, no error.
	const String project_locale = get_project_locale();
	print_verbose(vformat("Language settings: no translation is loaded for the locale \"%s\"; \"%s\" is used.", p_locale, project_locale));
	return project_locale;
}

void LanguageSettingsServer::_push_locale(const String &p_locale) {
	TranslationServer *translation_server = TranslationServer::get_singleton();
	if (translation_server == nullptr) {
		return;
	}
	String locale;
	if (p_locale.is_empty()) {
		locale = get_project_locale();
	} else if (_is_developer_locale(p_locale)) {
		// `--language` and the test locale are used as they are, also to test a missing translation.
		locale = p_locale;
	} else {
		locale = _resolve_locale(p_locale);
	}
	if (!locale.is_empty()) {
		translation_server->set_locale(locale);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Root window. Without a SceneTree (before Main::start(), headless tools) the values wait.

Window *LanguageSettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

void LanguageSettingsServer::_push_root_value(const StringName &p_name, int p_value) {
	Window *root = _get_root();
	if (root == nullptr) {
		pending_root_values[p_name] = p_value;
		return;
	}
	pending_root_values.erase(p_name);
	if (p_name == SNAME("layout_direction")) {
		root->set_layout_direction(Window::LayoutDirection(CLAMP(p_value, 0, int(Window::LAYOUT_DIRECTION_MAX) - 1)));
	} else {
		root->set_auto_translate_mode(Node::AutoTranslateMode(CLAMP(p_value, 0, int(Node::AUTO_TRANSLATE_MODE_DISABLED))));
	}
}

int LanguageSettingsServer::_pull_root_value(const StringName &p_name, int p_default) const {
	const Window *root = _get_root();
	if (root != nullptr) {
		return p_name == SNAME("layout_direction") ? int(root->get_layout_direction()) : int(root->get_auto_translate_mode());
	}
	const Variant *pending = pending_root_values.getptr(p_name);
	return pending != nullptr ? int(*pending) : p_default;
}

void LanguageSettingsServer::_push_layout_direction(int p_direction) {
	_push_root_value(SNAME("layout_direction"), p_direction);
}

int LanguageSettingsServer::_pull_layout_direction() const {
	// The root starts as LAYOUT_DIRECTION_INHERITED, which follows `internationalization/rendering/root_node_layout_direction`.
	return _pull_root_value(SNAME("layout_direction"), Window::LAYOUT_DIRECTION_INHERITED);
}

void LanguageSettingsServer::_push_root_auto_translate_mode(int p_mode) {
	_push_root_value(SNAME("root_auto_translate_mode"), p_mode);
}

int LanguageSettingsServer::_pull_root_auto_translate_mode() const {
	// What SceneTree sets on the root it creates.
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const bool auto_translate = project_settings == nullptr || !project_settings->has_setting("internationalization/rendering/root_node_auto_translate") || bool(project_settings->get_setting_with_override("internationalization/rendering/root_node_auto_translate"));
	return _pull_root_value(SNAME("root_auto_translate_mode"), auto_translate ? Node::AUTO_TRANSLATE_MODE_ALWAYS : Node::AUTO_TRANSLATE_MODE_DISABLED);
}

void LanguageSettingsServer::_push_pending_root_values() {
	if (pending_root_values.is_empty() || _get_root() == nullptr) {
		return;
	}
	const HashMap<StringName, Variant> values(pending_root_values);
	for (const KeyValue<StringName, Variant> &kv : values) {
		_push_root_value(kv.key, kv.value);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Startup.

void LanguageSettingsServer::_on_loaded() {
	// Only the load of startup(), after the player's file is read and before it is applied.
	if (!is_started() || startup_overrides_done) {
		return;
	}
	startup_overrides_done = true;
	String locale = get_command_line_locale();
	if (locale.is_empty()) {
		locale = _get_test_locale();
	}
	if (!locale.is_empty()) {
		// Pushed now, never saved; a later set_locale() replaces it.
		set_session_override(SNAME("locale"), locale);
	}
}

void LanguageSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		_push_pending_root_values();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName LanguageSettingsServer::get_domain() const {
	return SNAME("language");
}

SettingsServer::ApplyPhase LanguageSettingsServer::get_apply_phase() const {
	// After TranslationServer::setup(), the project translations and `--language`; before the main loop exists.
	return APPLY_PHASE_SERVERS;
}

Ref<Resource> LanguageSettingsServer::_create_default_settings() const {
	Ref<LanguageSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

Variant LanguageSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (p_name == SNAME("locale")) {
		const String locale = String(value).strip_edges();
		const TranslationServer *translation_server = TranslationServer::get_singleton();
		if (locale.is_empty() || translation_server == nullptr) {
			return locale;
		}
		return translation_server->standardize_locale(locale);
	}
	return value;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void LanguageSettingsServer::set_locale(const String &p_locale) {
	set_value(SNAME("locale"), p_locale);
}

String LanguageSettingsServer::get_locale() const {
	return get_value(SNAME("locale"));
}

void LanguageSettingsServer::set_pseudolocalization_enabled(bool p_enabled) {
	set_value(SNAME("pseudolocalization_enabled"), p_enabled);
}

bool LanguageSettingsServer::is_pseudolocalization_enabled() const {
	return get_value(SNAME("pseudolocalization_enabled"));
}

void LanguageSettingsServer::set_layout_direction(Window::LayoutDirection p_direction) {
	set_value(SNAME("layout_direction"), int(p_direction));
}

Window::LayoutDirection LanguageSettingsServer::get_layout_direction() const {
	return Window::LayoutDirection(int(get_value(SNAME("layout_direction"))));
}

void LanguageSettingsServer::set_root_auto_translate_mode(Node::AutoTranslateMode p_mode) {
	set_value(SNAME("root_auto_translate_mode"), int(p_mode));
}

Node::AutoTranslateMode LanguageSettingsServer::get_root_auto_translate_mode() const {
	return Node::AutoTranslateMode(int(get_value(SNAME("root_auto_translate_mode"))));
}

void LanguageSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"locale",
		"pseudolocalization_enabled",
		"layout_direction",
		"root_auto_translate_mode",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void LanguageSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("locale_changed", PropertyInfo(Variant::STRING, "locale")));
	ADD_SIGNAL(MethodInfo("pseudolocalization_enabled_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("layout_direction_changed", PropertyInfo(Variant::INT, "direction", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Window.LayoutDirection")));
	ADD_SIGNAL(MethodInfo("root_auto_translate_mode_changed", PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Node.AutoTranslateMode")));

	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &LanguageSettingsServer::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &LanguageSettingsServer::get_locale);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_enabled", "enabled"), &LanguageSettingsServer::set_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_enabled"), &LanguageSettingsServer::is_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("set_layout_direction", "direction"), &LanguageSettingsServer::set_layout_direction);
	ClassDB::bind_method(D_METHOD("get_layout_direction"), &LanguageSettingsServer::get_layout_direction);
	ClassDB::bind_method(D_METHOD("set_root_auto_translate_mode", "mode"), &LanguageSettingsServer::set_root_auto_translate_mode);
	ClassDB::bind_method(D_METHOD("get_root_auto_translate_mode"), &LanguageSettingsServer::get_root_auto_translate_mode);
	ClassDB::bind_static_method("LanguageSettingsServer", D_METHOD("get_project_locale"), &LanguageSettingsServer::get_project_locale);
	ClassDB::bind_static_method("LanguageSettingsServer", D_METHOD("get_command_line_locale"), &LanguageSettingsServer::get_command_line_locale);
	ClassDB::bind_static_method("LanguageSettingsServer", D_METHOD("get_available_locales"), &LanguageSettingsServer::get_available_locales);
}

LanguageSettingsServer::LanguageSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	// No getter: an empty locale (follow the OS) is the default, not the locale the engine has now.
	bind_setting_callbacks(SNAME("locale"), callable_mp_static(&LanguageSettingsServer::_push_locale), Callable());
	bind_setting_singleton_property(SNAME("pseudolocalization_enabled"), SNAME("TranslationServer"), SNAME("pseudolocalization_enabled"));
	bind_setting_callbacks(SNAME("layout_direction"), callable_mp(this, &LanguageSettingsServer::_push_layout_direction), callable_mp(this, &LanguageSettingsServer::_pull_layout_direction));
	bind_setting_callbacks(SNAME("root_auto_translate_mode"), callable_mp(this, &LanguageSettingsServer::_push_root_auto_translate_mode), callable_mp(this, &LanguageSettingsServer::_pull_root_auto_translate_mode));

	connect(SNAME("loaded"), callable_mp(this, &LanguageSettingsServer::_on_loaded));
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &LanguageSettingsServer::_on_startup_applied));
	}
}

LanguageSettingsServer::~LanguageSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
