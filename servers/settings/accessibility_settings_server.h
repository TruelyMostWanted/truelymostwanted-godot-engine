/**************************************************************************/
/*  accessibility_settings_server.h                                       */
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

#include "core/variant/binder_common.h"
#include "scene/resources/theme.h"
#include "servers/settings/accessibility_color_scheme.h"
#include "servers/settings/settings_server.h"

class Control;
class Window;

// Settings domain `accessibility` (#27): UI scale, font size, color scheme, focus outline and spoken focus
// (engine-applied), reduced motion and transparency, colorblind mode and subtitles (game-side).
// Applied in APPLY_PHASE_SCENE, before the main scene is loaded:
// - `ui_scale` multiplies the content scale factor of the root window (VideoWindowSettingsServer, #20, keeps
//   its own factor and multiplies with get_root_ui_scale() when it pushes).
// - `font_size_factor`, `color_scheme` and `focus_outline_width` go into an overlay theme on the root window:
//   a copy of the root window's previous theme plus the changed items. The project theme stays untouched.
// - `speak_focused_controls` speaks the focused Control of the root window with TTSServer (#12), unless a
//   screen reader is active (it would speak the same control).
// - The game-side values are stored and signaled only; helpers like get_animation_duration() apply them.
// Defaults: reduced motion, reduced transparency and high contrast come from the OS where DisplayServer knows them.
class AccessibilitySettingsServer : public SettingsServer {
	GDCLASS(AccessibilitySettingsServer, SettingsServer);

public:
	enum ColorScheme {
		COLOR_SCHEME_DEFAULT,
		COLOR_SCHEME_HIGH_CONTRAST_DARK,
		COLOR_SCHEME_HIGH_CONTRAST_LIGHT,
		COLOR_SCHEME_CUSTOM,
		COLOR_SCHEME_MAX,
	};

	enum ColorblindMode {
		COLORBLIND_MODE_NONE,
		COLORBLIND_MODE_PROTANOPIA,
		COLORBLIND_MODE_DEUTERANOPIA,
		COLORBLIND_MODE_TRITANOPIA,
		COLORBLIND_MODE_ACHROMATOPSIA,
		COLORBLIND_MODE_MAX,
	};

private:
	static AccessibilitySettingsServer *singleton;

	// The values pushed to the engine (what the engine has now).
	float pushed_ui_scale = 1.0;
	float pushed_font_size_factor = 1.0;
	int pushed_color_scheme = COLOR_SCHEME_DEFAULT;
	bool color_scheme_pushed = false;
	String pushed_custom_color_scheme_path;
	int pushed_focus_outline_width = 0;
	bool pushed_speak_focused_controls = false;
	String pushed_speech_persona;

	Ref<AccessibilityColorScheme> custom_color_scheme; // Loaded from pushed_custom_color_scheme_path.
	Ref<AccessibilityColorScheme> active_color_scheme; // The scheme of the overlay theme, or null.

	// The overlay theme on the root window, and the theme the root window had before.
	Ref<Theme> overlay_theme;
	Ref<Theme> previous_root_theme;
	bool overlay_active = false;
	// The overlay as first theme of the default theme context: Controls without a Control or Window parent chain
	// (for example under a Node or Node2D) do not inherit the root window's theme.
	Ref<Theme> context_overlay;
	void _set_context_overlay(const Ref<Theme> &p_theme);
	ObjectID focus_root; // The root window whose `gui_focus_changed` is connected.

	static Window *_get_root();
	static int _get_os_flag(int p_value);

	void _push_ui_scale(float p_scale);
	float _pull_ui_scale();
	void _push_font_size_factor(float p_factor);
	float _pull_font_size_factor();
	void _push_color_scheme(int p_scheme);
	int _pull_color_scheme();
	void _push_custom_color_scheme_path(const String &p_path);
	String _pull_custom_color_scheme_path();
	void _push_focus_outline_width(int p_width);
	int _pull_focus_outline_width();
	void _push_speak_focused_controls(bool p_enabled);
	bool _pull_speak_focused_controls();
	void _push_speech_persona(const String &p_persona);
	String _pull_speech_persona();

	Ref<AccessibilityColorScheme> _resolve_color_scheme() const;
	void _update_color_scheme();
	void _update_theme();
	void _fill_overlay_theme(const Ref<Theme> &p_theme) const;
	void _update_focus_connection();
	void _on_gui_focus_changed(Control *p_control);

protected:
	static void _bind_methods();

	// Emits the typed `<value>_changed` signal of an applied value.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;

public:
	static AccessibilitySettingsServer *get_singleton();
	// The UI scale pushed to the root window, or 1 without the domain. VideoWindowSettingsServer multiplies with it.
	static float get_root_ui_scale();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;

	// Engine-applied.
	void set_ui_scale(float p_scale);
	float get_ui_scale() const;
	void set_font_size_factor(float p_factor);
	float get_font_size_factor() const;
	void set_color_scheme(ColorScheme p_scheme);
	ColorScheme get_color_scheme() const;
	void set_custom_color_scheme_path(const String &p_path);
	String get_custom_color_scheme_path() const;
	void set_focus_outline_width(int p_width);
	int get_focus_outline_width() const;
	void set_speak_focused_controls(bool p_enabled);
	bool is_speaking_focused_controls() const;
	void set_speech_persona(const String &p_persona);
	String get_speech_persona() const;

	// Game-side.
	void set_reduce_motion(bool p_enabled);
	bool is_reducing_motion() const;
	void set_reduce_transparency(bool p_enabled);
	bool is_reducing_transparency() const;
	void set_colorblind_mode(ColorblindMode p_mode);
	ColorblindMode get_colorblind_mode() const;
	void set_subtitles_enabled(bool p_enabled);
	bool is_subtitles_enabled() const;
	void set_subtitle_scale(float p_scale);
	float get_subtitle_scale() const;

	// Helpers.
	float get_motion_scale() const;
	double get_animation_duration(double p_duration) const;
	int scale_font_size(int p_size) const;
	int scale_subtitle_font_size(int p_size) const;
	Ref<AccessibilityColorScheme> get_active_color_scheme() const;
	Color get_scheme_color(const StringName &p_role, const Color &p_default = Color()) const;
	Ref<Theme> get_overlay_theme() const;
	bool is_screen_reader_active() const;
	static String get_control_speech_text(Control *p_control);
	int64_t speak_control(Control *p_control);

	AccessibilitySettingsServer();
	~AccessibilitySettingsServer();
};

VARIANT_ENUM_CAST(AccessibilitySettingsServer::ColorScheme);
VARIANT_ENUM_CAST(AccessibilitySettingsServer::ColorblindMode);
