/**************************************************************************/
/*  locale_option_button.h                                                */
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

#include "scene/gui/option_button.h"

// A language menu (#414): an OptionButton filled with the locales of the project (LanguageSettingsServer::
// get_available_locales_for(): catalogs and translations, one list for every channel) under their native names.
// Selecting an entry sets the locale of its `target` channel through LanguageSettingsServer (stored for the next
// start; custom channels directly through TranslationServer), and the selection follows `locale_changed`. The list
// is built from the loaded catalogs, not from the file system. In the editor it stays empty: its items would be
// saved into the scene and a selection must not change the editor's language.
class LocaleOptionButton : public OptionButton {
	GDCLASS(LocaleOptionButton, OptionButton);

	StringName target;
	bool show_native_names = true;
	bool show_region = false;
	bool include_fallback = false;
	bool connected = false;

	void _on_item_selected(int p_idx);
	void _on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale);
	void _select_current();
	void _refresh_if_inside_tree();
	static String _get_native_language_name(const String &p_locale, bool &r_is_full);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	// The name of `p_locale` in its own language (`de_AT` -> `Deutsch`, `pt_BR` -> `Português (Brasil)`), the English
	// name of the engine if the language is not in the table. `p_region` appends the country (not for names that
	// contain it already).
	static String get_native_name(const String &p_locale, bool p_region = false);

	void set_target(const StringName &p_target);
	StringName get_target() const;
	void set_show_native_names(bool p_show);
	bool is_showing_native_names() const;
	void set_show_region(bool p_show);
	bool is_showing_region() const;
	void set_include_fallback(bool p_include);
	bool is_including_fallback() const;

	// Rebuilds the list from the current locales of the project.
	void refresh();
	PackedStringArray get_locales() const;
	// The locale of the selected entry, or an empty string.
	String get_selected_locale() const;
	// Selects the entry of `p_locale` (or the closest one) without changing the language.
	void select_locale(const String &p_locale);

	LocaleOptionButton();
};
