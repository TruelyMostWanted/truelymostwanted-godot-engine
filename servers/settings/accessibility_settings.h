/**************************************************************************/
/*  accessibility_settings.h                                              */
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

// Data of the `accessibility` settings domain (#27). No engine dependency: AccessibilitySettingsServer pushes
// the UI values to the root window, its theme and TTSServer, and offers helpers for the game-side values.
class AccessibilitySettings : public Resource {
	GDCLASS(AccessibilitySettings, Resource);

	// Engine-applied.
	float ui_scale = 1.0;
	float font_size_factor = 1.0;
	int color_scheme = 0; // AccessibilitySettingsServer::ColorScheme
	String custom_color_scheme_path;
	int focus_outline_width = 0; // 0: the theme's own focus style box.
	bool speak_focused_controls = false;
	String speech_persona;

	// Game-side.
	bool reduce_motion = false;
	bool reduce_transparency = false;
	int colorblind_mode = 0; // AccessibilitySettingsServer::ColorblindMode
	bool subtitles_enabled = false;
	bool captions_enabled = true;
	float subtitle_scale = 1.0;

protected:
	static void _bind_methods();

public:
	void set_ui_scale(float p_scale);
	float get_ui_scale() const;
	void set_font_size_factor(float p_factor);
	float get_font_size_factor() const;
	void set_color_scheme(int p_scheme);
	int get_color_scheme() const;
	void set_custom_color_scheme_path(const String &p_path);
	String get_custom_color_scheme_path() const;
	void set_focus_outline_width(int p_width);
	int get_focus_outline_width() const;
	void set_speak_focused_controls(bool p_enabled);
	bool is_speaking_focused_controls() const;
	void set_speech_persona(const String &p_persona);
	String get_speech_persona() const;

	void set_reduce_motion(bool p_enabled);
	bool is_reducing_motion() const;
	void set_reduce_transparency(bool p_enabled);
	bool is_reducing_transparency() const;
	void set_colorblind_mode(int p_mode);
	int get_colorblind_mode() const;
	void set_subtitles_enabled(bool p_enabled);
	bool is_subtitles_enabled() const;
	void set_captions_enabled(bool p_enabled);
	bool is_captions_enabled() const;
	void set_subtitle_scale(float p_scale);
	float get_subtitle_scale() const;
};
