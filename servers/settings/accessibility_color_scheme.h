/**************************************************************************/
/*  accessibility_color_scheme.h                                          */
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

#include "core/io/resource.h"
#include "core/math/color.h"

// The colors of an accessibility color scheme (#27). AccessibilitySettingsServer builds an overlay theme for the
// root window from it (text, field text, selection, focus, surface, accent, borders); the role colors `ok`,
// `problem` and `caution` are for the game (AccessibilitySettingsServer.get_scheme_color()).
class AccessibilityColorScheme : public Resource {
	GDCLASS(AccessibilityColorScheme, Resource);

	Color text_color = Color(1, 1, 1);
	Color field_text_color = Color(1, 1, 1);
	Color ok_color = Color(0.3, 1, 0.3);
	Color problem_color = Color(1, 0.35, 0.35);
	Color caution_color = Color(1, 0.85, 0.2);
	Color selection_color = Color(0, 0.35, 0.9);
	Color focus_color = Color(1, 0.85, 0);
	Color surface_color = Color(0, 0, 0);
	Color accent_color = Color(0, 0.25, 0.6);
	Color border_color = Color(1, 1, 1);
	Color accent_border_color = Color(1, 0.85, 0);
	int border_width = 2;

protected:
	static void _bind_methods();

public:
	void set_text_color(const Color &p_color);
	Color get_text_color() const;
	void set_field_text_color(const Color &p_color);
	Color get_field_text_color() const;
	void set_ok_color(const Color &p_color);
	Color get_ok_color() const;
	void set_problem_color(const Color &p_color);
	Color get_problem_color() const;
	void set_caution_color(const Color &p_color);
	Color get_caution_color() const;
	void set_selection_color(const Color &p_color);
	Color get_selection_color() const;
	void set_focus_color(const Color &p_color);
	Color get_focus_color() const;
	void set_surface_color(const Color &p_color);
	Color get_surface_color() const;
	void set_accent_color(const Color &p_color);
	Color get_accent_color() const;
	void set_border_color(const Color &p_color);
	Color get_border_color() const;
	void set_accent_border_color(const Color &p_color);
	Color get_accent_border_color() const;
	void set_border_width(int p_width);
	int get_border_width() const;

	// The color of a role (`text`, `field_text`, `ok`, `problem`, `caution`, `selection`, `focus`, `surface`,
	// `accent`, `border`, `accent_border`), or `p_default` for an unknown role.
	Color get_role_color(const StringName &p_role, const Color &p_default = Color()) const;

	// The built-in schemes: white on black and black on white, both with strong borders.
	static Ref<AccessibilityColorScheme> create_high_contrast_dark();
	static Ref<AccessibilityColorScheme> create_high_contrast_light();
};
