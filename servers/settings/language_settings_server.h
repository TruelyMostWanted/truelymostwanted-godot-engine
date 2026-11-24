/**************************************************************************/
/*  language_settings_server.h                                            */
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

#include "core/templates/hash_map.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server.h"

// Settings domain `language` (#17): the locale chosen by the player, pseudolocalization and the layout
// direction and auto-translate mode of the root window (LanguageSettings). Applied in APPLY_PHASE_SERVERS,
// so the locale is set before the autoloads and the main scene; the root window values are pushed as soon
// as the root exists (APPLY_PHASE_SCENE at the latest).
// Precedence of the locale: command line `--language` > `internationalization/locale/test` > stored > OS.
class LanguageSettingsServer : public SettingsServer {
	GDCLASS(LanguageSettingsServer, SettingsServer);

	static LanguageSettingsServer *singleton;

	// Root window values pushed while no SceneTree existed; pushed when the root exists.
	HashMap<StringName, Variant> pending_root_values;
	bool startup_overrides_done = false;

	static Window *_get_root();
	static String _get_test_locale();
	static bool _is_developer_locale(const String &p_locale);
	static String _resolve_locale(const String &p_locale);
	static void _push_locale(const String &p_locale);

	void _push_root_value(const StringName &p_name, int p_value);
	int _pull_root_value(const StringName &p_name, int p_default) const;
	void _push_layout_direction(int p_direction);
	int _pull_layout_direction() const;
	void _push_root_auto_translate_mode(int p_mode);
	int _pull_root_auto_translate_mode() const;
	void _push_pending_root_values();

	void _on_loaded();
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	// Emits the typed `<value>_changed` signal of an applied value.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static LanguageSettingsServer *get_singleton();

	// The locale used for an empty `locale`: `internationalization/locale/test` if set, else the OS locale.
	static String get_project_locale();
	// The locale passed with `--language` or `-l`, or an empty string.
	static String get_command_line_locale();
	// The locales of the loaded translations (TranslationServer.get_loaded_locales()), sorted.
	static PackedStringArray get_available_locales();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;

	void set_locale(const String &p_locale);
	String get_locale() const;
	void set_pseudolocalization_enabled(bool p_enabled);
	bool is_pseudolocalization_enabled() const;
	void set_layout_direction(Window::LayoutDirection p_direction);
	Window::LayoutDirection get_layout_direction() const;
	void set_root_auto_translate_mode(Node::AutoTranslateMode p_mode);
	Node::AutoTranslateMode get_root_auto_translate_mode() const;

	LanguageSettingsServer();
	~LanguageSettingsServer();
};
