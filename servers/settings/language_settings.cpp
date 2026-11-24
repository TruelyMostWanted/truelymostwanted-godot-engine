/**************************************************************************/
/*  language_settings.cpp                                                 */
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

#include "language_settings.h"

#include "core/object/class_db.h"

void LanguageSettings::set_locale(const String &p_locale) {
	locale = p_locale.strip_edges();
	emit_changed();
}

String LanguageSettings::get_locale() const {
	return locale;
}

void LanguageSettings::set_pseudolocalization_enabled(bool p_enabled) {
	pseudolocalization_enabled = p_enabled;
	emit_changed();
}

bool LanguageSettings::is_pseudolocalization_enabled() const {
	return pseudolocalization_enabled;
}

void LanguageSettings::set_layout_direction(int p_direction) {
	layout_direction = p_direction;
	emit_changed();
}

int LanguageSettings::get_layout_direction() const {
	return layout_direction;
}

void LanguageSettings::set_root_auto_translate_mode(int p_mode) {
	root_auto_translate_mode = p_mode;
	emit_changed();
}

int LanguageSettings::get_root_auto_translate_mode() const {
	return root_auto_translate_mode;
}

void LanguageSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &LanguageSettings::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &LanguageSettings::get_locale);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_enabled", "enabled"), &LanguageSettings::set_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_enabled"), &LanguageSettings::is_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("set_layout_direction", "direction"), &LanguageSettings::set_layout_direction);
	ClassDB::bind_method(D_METHOD("get_layout_direction"), &LanguageSettings::get_layout_direction);
	ClassDB::bind_method(D_METHOD("set_root_auto_translate_mode", "mode"), &LanguageSettings::set_root_auto_translate_mode);
	ClassDB::bind_method(D_METHOD("get_root_auto_translate_mode"), &LanguageSettings::get_root_auto_translate_mode);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "locale", PROPERTY_HINT_LOCALE_ID), "set_locale", "get_locale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pseudolocalization_enabled"), "set_pseudolocalization_enabled", "is_pseudolocalization_enabled");
	// The values of Window.LayoutDirection and Node.AutoTranslateMode.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layout_direction", PROPERTY_HINT_ENUM, "Inherited,Application Locale,Left-to-Right,Right-to-Left,System Locale"), "set_layout_direction", "get_layout_direction");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_auto_translate_mode", PROPERTY_HINT_ENUM, "Inherit,Always,Disabled"), "set_root_auto_translate_mode", "get_root_auto_translate_mode");
}
