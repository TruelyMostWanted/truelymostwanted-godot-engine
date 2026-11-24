/**************************************************************************/
/*  accessibility_color_scheme.cpp                                        */
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

#include "accessibility_color_scheme.h"

#include "core/object/class_db.h"

#define ACCESSIBILITY_COLOR_ACCESSORS(m_name) \
	void AccessibilityColorScheme::set_##m_name(const Color &p_color) { \
		m_name = p_color; \
		emit_changed(); \
	} \
	Color AccessibilityColorScheme::get_##m_name() const { \
		return m_name; \
	}

ACCESSIBILITY_COLOR_ACCESSORS(text_color)
ACCESSIBILITY_COLOR_ACCESSORS(field_text_color)
ACCESSIBILITY_COLOR_ACCESSORS(ok_color)
ACCESSIBILITY_COLOR_ACCESSORS(problem_color)
ACCESSIBILITY_COLOR_ACCESSORS(caution_color)
ACCESSIBILITY_COLOR_ACCESSORS(selection_color)
ACCESSIBILITY_COLOR_ACCESSORS(focus_color)
ACCESSIBILITY_COLOR_ACCESSORS(surface_color)
ACCESSIBILITY_COLOR_ACCESSORS(accent_color)
ACCESSIBILITY_COLOR_ACCESSORS(border_color)
ACCESSIBILITY_COLOR_ACCESSORS(accent_border_color)

#undef ACCESSIBILITY_COLOR_ACCESSORS

void AccessibilityColorScheme::set_border_width(int p_width) {
	border_width = CLAMP(p_width, 0, 16);
	emit_changed();
}

int AccessibilityColorScheme::get_border_width() const {
	return border_width;
}

Color AccessibilityColorScheme::get_role_color(const StringName &p_role, const Color &p_default) const {
	bool valid = false;
	const Variant value = get(StringName(String(p_role) + "_color"), &valid);
	if (!valid || value.get_type() != Variant::COLOR) {
		return p_default;
	}
	return value;
}

Ref<AccessibilityColorScheme> AccessibilityColorScheme::create_high_contrast_dark() {
	// The member defaults are the dark scheme.
	Ref<AccessibilityColorScheme> scheme;
	scheme.instantiate();
	return scheme;
}

Ref<AccessibilityColorScheme> AccessibilityColorScheme::create_high_contrast_light() {
	Ref<AccessibilityColorScheme> scheme;
	scheme.instantiate();
	scheme->text_color = Color(0, 0, 0);
	scheme->field_text_color = Color(0, 0, 0);
	scheme->ok_color = Color(0, 0.45, 0);
	scheme->problem_color = Color(0.75, 0, 0);
	scheme->caution_color = Color(0.55, 0.35, 0);
	scheme->selection_color = Color(0.55, 0.75, 1);
	scheme->focus_color = Color(0, 0.3, 0.9);
	scheme->surface_color = Color(1, 1, 1);
	scheme->accent_color = Color(0.8, 0.88, 1);
	scheme->border_color = Color(0, 0, 0);
	scheme->accent_border_color = Color(0, 0.3, 0.9);
	scheme->border_width = 2;
	return scheme;
}

void AccessibilityColorScheme::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text_color", "color"), &AccessibilityColorScheme::set_text_color);
	ClassDB::bind_method(D_METHOD("get_text_color"), &AccessibilityColorScheme::get_text_color);
	ClassDB::bind_method(D_METHOD("set_field_text_color", "color"), &AccessibilityColorScheme::set_field_text_color);
	ClassDB::bind_method(D_METHOD("get_field_text_color"), &AccessibilityColorScheme::get_field_text_color);
	ClassDB::bind_method(D_METHOD("set_ok_color", "color"), &AccessibilityColorScheme::set_ok_color);
	ClassDB::bind_method(D_METHOD("get_ok_color"), &AccessibilityColorScheme::get_ok_color);
	ClassDB::bind_method(D_METHOD("set_problem_color", "color"), &AccessibilityColorScheme::set_problem_color);
	ClassDB::bind_method(D_METHOD("get_problem_color"), &AccessibilityColorScheme::get_problem_color);
	ClassDB::bind_method(D_METHOD("set_caution_color", "color"), &AccessibilityColorScheme::set_caution_color);
	ClassDB::bind_method(D_METHOD("get_caution_color"), &AccessibilityColorScheme::get_caution_color);
	ClassDB::bind_method(D_METHOD("set_selection_color", "color"), &AccessibilityColorScheme::set_selection_color);
	ClassDB::bind_method(D_METHOD("get_selection_color"), &AccessibilityColorScheme::get_selection_color);
	ClassDB::bind_method(D_METHOD("set_focus_color", "color"), &AccessibilityColorScheme::set_focus_color);
	ClassDB::bind_method(D_METHOD("get_focus_color"), &AccessibilityColorScheme::get_focus_color);
	ClassDB::bind_method(D_METHOD("set_surface_color", "color"), &AccessibilityColorScheme::set_surface_color);
	ClassDB::bind_method(D_METHOD("get_surface_color"), &AccessibilityColorScheme::get_surface_color);
	ClassDB::bind_method(D_METHOD("set_accent_color", "color"), &AccessibilityColorScheme::set_accent_color);
	ClassDB::bind_method(D_METHOD("get_accent_color"), &AccessibilityColorScheme::get_accent_color);
	ClassDB::bind_method(D_METHOD("set_border_color", "color"), &AccessibilityColorScheme::set_border_color);
	ClassDB::bind_method(D_METHOD("get_border_color"), &AccessibilityColorScheme::get_border_color);
	ClassDB::bind_method(D_METHOD("set_accent_border_color", "color"), &AccessibilityColorScheme::set_accent_border_color);
	ClassDB::bind_method(D_METHOD("get_accent_border_color"), &AccessibilityColorScheme::get_accent_border_color);
	ClassDB::bind_method(D_METHOD("set_border_width", "width"), &AccessibilityColorScheme::set_border_width);
	ClassDB::bind_method(D_METHOD("get_border_width"), &AccessibilityColorScheme::get_border_width);

	ClassDB::bind_method(D_METHOD("get_role_color", "role", "default"), &AccessibilityColorScheme::get_role_color, DEFVAL(Color()));
	ClassDB::bind_static_method("AccessibilityColorScheme", D_METHOD("create_high_contrast_dark"), &AccessibilityColorScheme::create_high_contrast_dark);
	ClassDB::bind_static_method("AccessibilityColorScheme", D_METHOD("create_high_contrast_light"), &AccessibilityColorScheme::create_high_contrast_light);

	ADD_GROUP("Text", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "text_color"), "set_text_color", "get_text_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "field_text_color"), "set_field_text_color", "get_field_text_color");
	ADD_GROUP("Roles", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ok_color"), "set_ok_color", "get_ok_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "problem_color"), "set_problem_color", "get_problem_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "caution_color"), "set_caution_color", "get_caution_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "selection_color"), "set_selection_color", "get_selection_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "focus_color"), "set_focus_color", "get_focus_color");
	ADD_GROUP("Surfaces", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "surface_color"), "set_surface_color", "get_surface_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "accent_color"), "set_accent_color", "get_accent_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "border_color"), "set_border_color", "get_border_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "accent_border_color"), "set_accent_border_color", "get_accent_border_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "border_width", PROPERTY_HINT_RANGE, "0,16,1,suffix:px"), "set_border_width", "get_border_width");
}
