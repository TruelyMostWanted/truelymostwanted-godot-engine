/**************************************************************************/
/*  accessibility_settings_server.cpp                                     */
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

#include "accessibility_settings_server.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "scene/theme/theme_db.h"
#include "servers/display/display_server.h"
#include "servers/settings/accessibility_settings.h"
#include "servers/tts/tts_server.h"

AccessibilitySettingsServer *AccessibilitySettingsServer::singleton = nullptr;

AccessibilitySettingsServer *AccessibilitySettingsServer::get_singleton() {
	return singleton;
}

float AccessibilitySettingsServer::get_root_ui_scale() {
	return singleton != nullptr ? singleton->pushed_ui_scale : 1.0f;
}

Window *AccessibilitySettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

int AccessibilitySettingsServer::_get_os_flag(int p_value) {
	// DisplayServer answers 1 (yes), 0 (no) or -1 (unknown: no AccessKit, headless, platform without the query).
	return p_value == 1 ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName AccessibilitySettingsServer::get_domain() const {
	return SNAME("accessibility");
}

SettingsServer::ApplyPhase AccessibilitySettingsServer::get_apply_phase() const {
	// The root window and the theme database exist, the main scene is not loaded yet: the first menu
	// already shows the player's scale, font size and colors.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> AccessibilitySettingsServer::_create_default_settings() const {
	Ref<AccessibilitySettings> default_settings;
	default_settings.instantiate();
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server != nullptr) {
		default_settings->set_reduce_motion(_get_os_flag(display_server->accessibility_should_reduce_animation()) == 1);
		default_settings->set_reduce_transparency(_get_os_flag(display_server->accessibility_should_reduce_transparency()) == 1);
	}
	return default_settings;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets. Without a root window (headless tools, unit tests without a scene tree) the values are
// kept and the window, theme and focus parts are skipped.

void AccessibilitySettingsServer::_push_ui_scale(float p_scale) {
	const float scale = MAX(p_scale, 0.01f);
	Window *root = _get_root();
	if (root != nullptr) {
		// The root window has the factor of the window domain (or the project) times the UI scale.
		const real_t base = root->get_content_scale_factor() / MAX(pushed_ui_scale, 0.01f);
		root->set_content_scale_factor(MAX(base * scale, real_t(0.01)));
	}
	pushed_ui_scale = scale;
}

float AccessibilitySettingsServer::_pull_ui_scale() {
	return pushed_ui_scale;
}

void AccessibilitySettingsServer::_push_font_size_factor(float p_factor) {
	pushed_font_size_factor = p_factor;
	_update_theme();
}

float AccessibilitySettingsServer::_pull_font_size_factor() {
	return pushed_font_size_factor;
}

void AccessibilitySettingsServer::_push_color_scheme(int p_scheme) {
	pushed_color_scheme = CLAMP(p_scheme, 0, int(COLOR_SCHEME_MAX) - 1);
	color_scheme_pushed = true;
	_update_color_scheme();
}

int AccessibilitySettingsServer::_pull_color_scheme() {
	if (color_scheme_pushed) {
		return pushed_color_scheme;
	}
	// Before the first push (when the defaults are captured): the OS asks for more contrast.
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server != nullptr && _get_os_flag(display_server->accessibility_should_increase_contrast()) == 1) {
		return COLOR_SCHEME_HIGH_CONTRAST_DARK;
	}
	return COLOR_SCHEME_DEFAULT;
}

void AccessibilitySettingsServer::_push_custom_color_scheme_path(const String &p_path) {
	pushed_custom_color_scheme_path = p_path;
	custom_color_scheme.unref();
	if (!p_path.is_empty()) {
		const Ref<Resource> resource = ResourceLoader::load(p_path);
		custom_color_scheme = resource;
		if (custom_color_scheme.is_null()) {
			WARN_PRINT(vformat("Accessibility settings: \"%s\" is not an AccessibilityColorScheme; the custom color scheme is not used.", p_path));
		}
	}
	if (pushed_color_scheme == COLOR_SCHEME_CUSTOM) {
		_update_color_scheme();
	}
}

String AccessibilitySettingsServer::_pull_custom_color_scheme_path() {
	return pushed_custom_color_scheme_path;
}

void AccessibilitySettingsServer::_push_focus_outline_width(int p_width) {
	pushed_focus_outline_width = MAX(p_width, 0);
	_update_theme();
}

int AccessibilitySettingsServer::_pull_focus_outline_width() {
	return pushed_focus_outline_width;
}

void AccessibilitySettingsServer::_push_speak_focused_controls(bool p_enabled) {
	pushed_speak_focused_controls = p_enabled;
	_update_focus_connection();
}

bool AccessibilitySettingsServer::_pull_speak_focused_controls() {
	return pushed_speak_focused_controls;
}

void AccessibilitySettingsServer::_push_speech_persona(const String &p_persona) {
	pushed_speech_persona = p_persona;
}

String AccessibilitySettingsServer::_pull_speech_persona() {
	return pushed_speech_persona;
}

/////////////////////////////////////////////////////////////////////////////
// Overlay theme.

Ref<AccessibilityColorScheme> AccessibilitySettingsServer::_resolve_color_scheme() const {
	switch (pushed_color_scheme) {
		case COLOR_SCHEME_HIGH_CONTRAST_DARK: {
			return AccessibilityColorScheme::create_high_contrast_dark();
		}
		case COLOR_SCHEME_HIGH_CONTRAST_LIGHT: {
			return AccessibilityColorScheme::create_high_contrast_light();
		}
		case COLOR_SCHEME_CUSTOM: {
			return custom_color_scheme;
		}
		default: {
			return Ref<AccessibilityColorScheme>();
		}
	}
}

void AccessibilitySettingsServer::_update_color_scheme() {
	const Ref<AccessibilityColorScheme> scheme = _resolve_color_scheme();
	const bool changed = scheme != active_color_scheme;
	active_color_scheme = scheme;
	_update_theme();
	if (changed) {
		emit_signal(SNAME("color_scheme_changed"), active_color_scheme);
	}
}

void AccessibilitySettingsServer::_fill_overlay_theme(const Ref<Theme> &p_theme) const {
	ThemeDB *theme_db = ThemeDB::get_singleton();
	if (theme_db == nullptr) {
		return;
	}
	const Ref<Theme> default_theme = theme_db->get_default_theme();
	List<StringName> types;
	if (default_theme.is_valid()) {
		default_theme->get_type_list(&types);
	}

	// Font size: the base size of the theme chain times the factor, and the explicit sizes of the built-in types.
	if (!Math::is_equal_approx(pushed_font_size_factor, 1.0f)) {
		int base_size = theme_db->get_fallback_font_size();
		const Ref<Theme> project_theme = theme_db->get_project_theme();
		if (previous_root_theme.is_valid() && previous_root_theme->has_default_font_size()) {
			base_size = previous_root_theme->get_default_font_size();
		} else if (project_theme.is_valid() && project_theme->has_default_font_size()) {
			base_size = project_theme->get_default_font_size();
		}
		p_theme->set_default_font_size(MAX(1, int(Math::round(base_size * pushed_font_size_factor))));
		for (const StringName &type : types) {
			List<StringName> sizes;
			default_theme->get_font_size_list(type, &sizes);
			for (const StringName &size_name : sizes) {
				const int size = default_theme->get_font_size(size_name, type);
				if (size > 0) {
					p_theme->set_font_size(size_name, type, MAX(1, int(Math::round(size * pushed_font_size_factor))));
				}
			}
		}
	}

	const Ref<AccessibilityColorScheme> scheme = active_color_scheme;
	if (scheme.is_valid()) {
		const int border_width = scheme->get_border_width();
		for (const StringName &type : types) {
			const String type_name = type;
			const bool field = type_name == "LineEdit" || type_name == "TextEdit" || type_name == "CodeEdit";
			const Color text = field ? scheme->get_field_text_color() : scheme->get_text_color();

			List<StringName> colors;
			default_theme->get_color_list(type, &colors);
			for (const StringName &color_name : colors) {
				const String item = color_name;
				if (item == "selection_color") {
					p_theme->set_color(color_name, type, scheme->get_selection_color());
				} else if (item == "font_disabled_color" || item == "font_placeholder_color" || item == "font_uneditable_color" || item == "font_readonly_color") {
					p_theme->set_color(color_name, type, Color(text, text.a * 0.6f));
				} else if (item == "caret_color" || (item.begins_with("font_") && item.ends_with("_color") && item != "font_outline_color" && item != "font_shadow_color")) {
					p_theme->set_color(color_name, type, text);
				}
			}

			// Only flat style boxes: empty ones (Label, ...) stay empty and draw nothing.
			List<StringName> styles;
			default_theme->get_stylebox_list(type, &styles);
			for (const StringName &style_name : styles) {
				const String item = style_name;
				if (item == "focus") {
					continue;
				}
				const Ref<StyleBoxFlat> flat = default_theme->get_stylebox(style_name, type);
				if (flat.is_null()) {
					continue;
				}
				Ref<StyleBoxFlat> box;
				box = flat->duplicate();
				const bool accent = item.contains("hover") || item.contains("pressed") || item.contains("selected") || item == "fill" || item.begins_with("grabber_area");
				box->set_bg_color(accent ? scheme->get_accent_color() : scheme->get_surface_color());
				Color border = accent ? scheme->get_accent_border_color() : scheme->get_border_color();
				if (item == "disabled") {
					border.a *= 0.5f;
				}
				box->set_border_color(border);
				box->set_border_width_all(border_width);
				p_theme->set_stylebox(style_name, type, box);
			}
		}
	}

	// Focus outline: the scheme's focus color and/or the player's width.
	if (scheme.is_valid() || pushed_focus_outline_width > 0) {
		for (const StringName &type : types) {
			if (!default_theme->has_stylebox(SNAME("focus"), type)) {
				continue;
			}
			const Ref<StyleBoxFlat> flat = default_theme->get_stylebox(SNAME("focus"), type);
			Ref<StyleBoxFlat> box;
			if (flat.is_valid()) {
				box = flat->duplicate();
			} else {
				box.instantiate();
				box->set_border_color(Color(1, 1, 1, 0.75));
				box->set_border_width_all(1);
			}
			box->set_draw_center(false);
			if (scheme.is_valid()) {
				box->set_border_color(scheme->get_focus_color());
				box->set_border_width_all(MAX(2, scheme->get_border_width()));
			}
			if (pushed_focus_outline_width > 0) {
				box->set_border_width_all(pushed_focus_outline_width);
			}
			p_theme->set_stylebox(SNAME("focus"), type, box);
		}
	}
}

void AccessibilitySettingsServer::_set_context_overlay(const Ref<Theme> &p_theme) {
	ThemeDB *theme_db = ThemeDB::get_singleton();
	ThemeContext *context = theme_db != nullptr ? theme_db->get_default_theme_context() : nullptr;
	if (context == nullptr || p_theme == context_overlay) {
		return;
	}
	Vector<Ref<Theme>> themes = context->get_themes();
	if (context_overlay.is_valid()) {
		themes.erase(context_overlay);
	}
	if (p_theme.is_valid()) {
		themes.insert(0, p_theme);
	}
	context_overlay = p_theme;
	context->set_themes(themes);
}

void AccessibilitySettingsServer::_update_theme() {
	Window *root = _get_root();
	if (root == nullptr) {
		return;
	}
	const bool needed = !Math::is_equal_approx(pushed_font_size_factor, 1.0f) || active_color_scheme.is_valid() || pushed_focus_outline_width > 0;
	if (!needed) {
		if (overlay_active) {
			if (root->get_theme() == overlay_theme) {
				root->set_theme(previous_root_theme);
			}
			overlay_active = false;
			overlay_theme.unref();
			previous_root_theme.unref();
		}
		_set_context_overlay(Ref<Theme>());
		return;
	}
	if (!overlay_active || root->get_theme() != overlay_theme) {
		// First overlay, or the game replaced the root theme in the meantime: that theme is the new base.
		previous_root_theme = root->get_theme();
		overlay_active = true;
	}
	Ref<Theme> theme;
	theme.instantiate();
	if (previous_root_theme.is_valid()) {
		theme->merge_with(previous_root_theme);
	}
	_fill_overlay_theme(theme);
	overlay_theme = theme;
	root->set_theme(theme);
	_set_context_overlay(theme);
}

/////////////////////////////////////////////////////////////////////////////
// Spoken focus.

void AccessibilitySettingsServer::_update_focus_connection() {
	const Callable callback = callable_mp(this, &AccessibilitySettingsServer::_on_gui_focus_changed);
	Window *root = _get_root();
	Object *connected = ObjectDB::get_instance(focus_root);
	if (connected != nullptr && (!pushed_speak_focused_controls || connected != root)) {
		if (connected->is_connected(SNAME("gui_focus_changed"), callback)) {
			connected->disconnect(SNAME("gui_focus_changed"), callback);
		}
		focus_root = ObjectID();
	}
	if (pushed_speak_focused_controls && root != nullptr && !root->is_connected(SNAME("gui_focus_changed"), callback)) {
		root->connect(SNAME("gui_focus_changed"), callback);
		focus_root = root->get_instance_id();
	}
}

void AccessibilitySettingsServer::_on_gui_focus_changed(Control *p_control) {
	// A screen reader speaks the focused control itself; speaking it twice would garble both.
	if (!pushed_speak_focused_controls || is_screen_reader_active()) {
		return;
	}
	speak_control(p_control);
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void AccessibilitySettingsServer::set_ui_scale(float p_scale) {
	set_value(SNAME("ui_scale"), p_scale);
}

float AccessibilitySettingsServer::get_ui_scale() const {
	return get_value(SNAME("ui_scale"));
}

void AccessibilitySettingsServer::set_font_size_factor(float p_factor) {
	set_value(SNAME("font_size_factor"), p_factor);
}

float AccessibilitySettingsServer::get_font_size_factor() const {
	return get_value(SNAME("font_size_factor"));
}

void AccessibilitySettingsServer::set_color_scheme(ColorScheme p_scheme) {
	ERR_FAIL_INDEX(int(p_scheme), int(COLOR_SCHEME_MAX));
	set_value(SNAME("color_scheme"), int(p_scheme));
}

AccessibilitySettingsServer::ColorScheme AccessibilitySettingsServer::get_color_scheme() const {
	return ColorScheme(int(get_value(SNAME("color_scheme"))));
}

void AccessibilitySettingsServer::set_custom_color_scheme_path(const String &p_path) {
	set_value(SNAME("custom_color_scheme_path"), p_path);
}

String AccessibilitySettingsServer::get_custom_color_scheme_path() const {
	return get_value(SNAME("custom_color_scheme_path"));
}

void AccessibilitySettingsServer::set_focus_outline_width(int p_width) {
	set_value(SNAME("focus_outline_width"), p_width);
}

int AccessibilitySettingsServer::get_focus_outline_width() const {
	return get_value(SNAME("focus_outline_width"));
}

void AccessibilitySettingsServer::set_speak_focused_controls(bool p_enabled) {
	set_value(SNAME("speak_focused_controls"), p_enabled);
}

bool AccessibilitySettingsServer::is_speaking_focused_controls() const {
	return get_value(SNAME("speak_focused_controls"));
}

void AccessibilitySettingsServer::set_speech_persona(const String &p_persona) {
	set_value(SNAME("speech_persona"), p_persona);
}

String AccessibilitySettingsServer::get_speech_persona() const {
	return get_value(SNAME("speech_persona"));
}

void AccessibilitySettingsServer::set_reduce_motion(bool p_enabled) {
	set_value(SNAME("reduce_motion"), p_enabled);
}

bool AccessibilitySettingsServer::is_reducing_motion() const {
	return get_value(SNAME("reduce_motion"));
}

void AccessibilitySettingsServer::set_reduce_transparency(bool p_enabled) {
	set_value(SNAME("reduce_transparency"), p_enabled);
}

bool AccessibilitySettingsServer::is_reducing_transparency() const {
	return get_value(SNAME("reduce_transparency"));
}

void AccessibilitySettingsServer::set_colorblind_mode(ColorblindMode p_mode) {
	ERR_FAIL_INDEX(int(p_mode), int(COLORBLIND_MODE_MAX));
	set_value(SNAME("colorblind_mode"), int(p_mode));
}

AccessibilitySettingsServer::ColorblindMode AccessibilitySettingsServer::get_colorblind_mode() const {
	return ColorblindMode(int(get_value(SNAME("colorblind_mode"))));
}

void AccessibilitySettingsServer::set_subtitles_enabled(bool p_enabled) {
	set_value(SNAME("subtitles_enabled"), p_enabled);
}

bool AccessibilitySettingsServer::is_subtitles_enabled() const {
	return get_value(SNAME("subtitles_enabled"));
}

void AccessibilitySettingsServer::set_subtitle_scale(float p_scale) {
	set_value(SNAME("subtitle_scale"), p_scale);
}

float AccessibilitySettingsServer::get_subtitle_scale() const {
	return get_value(SNAME("subtitle_scale"));
}

/////////////////////////////////////////////////////////////////////////////
// Helpers.

float AccessibilitySettingsServer::get_motion_scale() const {
	return is_reducing_motion() ? 0.0f : 1.0f;
}

double AccessibilitySettingsServer::get_animation_duration(double p_duration) const {
	return is_reducing_motion() ? 0.0 : p_duration;
}

int AccessibilitySettingsServer::scale_font_size(int p_size) const {
	return MAX(1, int(Math::round(p_size * get_font_size_factor())));
}

int AccessibilitySettingsServer::scale_subtitle_font_size(int p_size) const {
	return MAX(1, int(Math::round(p_size * get_font_size_factor() * get_subtitle_scale())));
}

Ref<AccessibilityColorScheme> AccessibilitySettingsServer::get_active_color_scheme() const {
	return active_color_scheme;
}

Color AccessibilitySettingsServer::get_scheme_color(const StringName &p_role, const Color &p_default) const {
	if (active_color_scheme.is_null()) {
		return p_default;
	}
	return active_color_scheme->get_role_color(p_role, p_default);
}

Ref<Theme> AccessibilitySettingsServer::get_overlay_theme() const {
	return overlay_active ? overlay_theme : Ref<Theme>();
}

bool AccessibilitySettingsServer::is_screen_reader_active() const {
	const DisplayServer *display_server = DisplayServer::get_singleton();
	return display_server != nullptr && display_server->accessibility_screen_reader_active() == 1;
}

String AccessibilitySettingsServer::get_control_speech_text(Control *p_control) {
	ERR_FAIL_NULL_V(p_control, String());
	const String accessibility_name = p_control->get_accessibility_name().strip_edges();
	if (!accessibility_name.is_empty()) {
		return accessibility_name;
	}
	static const char *text_properties[] = { "text", "placeholder_text", nullptr };
	for (int i = 0; text_properties[i] != nullptr; i++) {
		bool valid = false;
		const Variant value = p_control->get(StringName(text_properties[i]), &valid);
		if (valid && value.get_type() == Variant::STRING) {
			const String text = String(value).strip_edges();
			if (!text.is_empty()) {
				return text;
			}
		}
	}
	return p_control->get_tooltip_text().strip_edges();
}

int64_t AccessibilitySettingsServer::speak_control(Control *p_control) {
	const String text = get_control_speech_text(p_control);
	TTSServer *tts_server = TTSServer::get_singleton();
	if (text.is_empty() || tts_server == nullptr || !tts_server->is_available()) {
		return -1;
	}
	Ref<TTSPersona> persona;
	if (!pushed_speech_persona.is_empty() && tts_server->has_persona(pushed_speech_persona)) {
		persona = tts_server->get_persona(pushed_speech_persona);
	}
	// The newly focused control replaces what is still spoken.
	tts_server->stop();
	return tts_server->speak(text, persona);
}

void AccessibilitySettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"ui_scale",
		"font_size_factor",
		"focus_outline_width",
		"speak_focused_controls",
		"reduce_motion",
		"reduce_transparency",
		"colorblind_mode",
		"subtitles_enabled",
		"subtitle_scale",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void AccessibilitySettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("ui_scale_changed", PropertyInfo(Variant::FLOAT, "scale")));
	ADD_SIGNAL(MethodInfo("font_size_factor_changed", PropertyInfo(Variant::FLOAT, "factor")));
	ADD_SIGNAL(MethodInfo("focus_outline_width_changed", PropertyInfo(Variant::INT, "width")));
	ADD_SIGNAL(MethodInfo("speak_focused_controls_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("reduce_motion_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("reduce_transparency_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("colorblind_mode_changed", PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "AccessibilitySettingsServer.ColorblindMode")));
	ADD_SIGNAL(MethodInfo("subtitles_enabled_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("subtitle_scale_changed", PropertyInfo(Variant::FLOAT, "scale")));

	ClassDB::bind_method(D_METHOD("set_ui_scale", "scale"), &AccessibilitySettingsServer::set_ui_scale);
	ClassDB::bind_method(D_METHOD("get_ui_scale"), &AccessibilitySettingsServer::get_ui_scale);
	ClassDB::bind_method(D_METHOD("set_font_size_factor", "factor"), &AccessibilitySettingsServer::set_font_size_factor);
	ClassDB::bind_method(D_METHOD("get_font_size_factor"), &AccessibilitySettingsServer::get_font_size_factor);
	ClassDB::bind_method(D_METHOD("set_color_scheme", "scheme"), &AccessibilitySettingsServer::set_color_scheme);
	ClassDB::bind_method(D_METHOD("get_color_scheme"), &AccessibilitySettingsServer::get_color_scheme);
	ClassDB::bind_method(D_METHOD("set_custom_color_scheme_path", "path"), &AccessibilitySettingsServer::set_custom_color_scheme_path);
	ClassDB::bind_method(D_METHOD("get_custom_color_scheme_path"), &AccessibilitySettingsServer::get_custom_color_scheme_path);
	ClassDB::bind_method(D_METHOD("set_focus_outline_width", "width"), &AccessibilitySettingsServer::set_focus_outline_width);
	ClassDB::bind_method(D_METHOD("get_focus_outline_width"), &AccessibilitySettingsServer::get_focus_outline_width);
	ClassDB::bind_method(D_METHOD("set_speak_focused_controls", "enabled"), &AccessibilitySettingsServer::set_speak_focused_controls);
	ClassDB::bind_method(D_METHOD("is_speaking_focused_controls"), &AccessibilitySettingsServer::is_speaking_focused_controls);
	ClassDB::bind_method(D_METHOD("set_speech_persona", "persona"), &AccessibilitySettingsServer::set_speech_persona);
	ClassDB::bind_method(D_METHOD("get_speech_persona"), &AccessibilitySettingsServer::get_speech_persona);

	ClassDB::bind_method(D_METHOD("set_reduce_motion", "enabled"), &AccessibilitySettingsServer::set_reduce_motion);
	ClassDB::bind_method(D_METHOD("is_reducing_motion"), &AccessibilitySettingsServer::is_reducing_motion);
	ClassDB::bind_method(D_METHOD("set_reduce_transparency", "enabled"), &AccessibilitySettingsServer::set_reduce_transparency);
	ClassDB::bind_method(D_METHOD("is_reducing_transparency"), &AccessibilitySettingsServer::is_reducing_transparency);
	ClassDB::bind_method(D_METHOD("set_colorblind_mode", "mode"), &AccessibilitySettingsServer::set_colorblind_mode);
	ClassDB::bind_method(D_METHOD("get_colorblind_mode"), &AccessibilitySettingsServer::get_colorblind_mode);
	ClassDB::bind_method(D_METHOD("set_subtitles_enabled", "enabled"), &AccessibilitySettingsServer::set_subtitles_enabled);
	ClassDB::bind_method(D_METHOD("is_subtitles_enabled"), &AccessibilitySettingsServer::is_subtitles_enabled);
	ClassDB::bind_method(D_METHOD("set_subtitle_scale", "scale"), &AccessibilitySettingsServer::set_subtitle_scale);
	ClassDB::bind_method(D_METHOD("get_subtitle_scale"), &AccessibilitySettingsServer::get_subtitle_scale);

	ClassDB::bind_method(D_METHOD("get_motion_scale"), &AccessibilitySettingsServer::get_motion_scale);
	ClassDB::bind_method(D_METHOD("get_animation_duration", "duration"), &AccessibilitySettingsServer::get_animation_duration);
	ClassDB::bind_method(D_METHOD("scale_font_size", "size"), &AccessibilitySettingsServer::scale_font_size);
	ClassDB::bind_method(D_METHOD("scale_subtitle_font_size", "size"), &AccessibilitySettingsServer::scale_subtitle_font_size);
	ClassDB::bind_method(D_METHOD("get_active_color_scheme"), &AccessibilitySettingsServer::get_active_color_scheme);
	ClassDB::bind_method(D_METHOD("get_scheme_color", "role", "default"), &AccessibilitySettingsServer::get_scheme_color, DEFVAL(Color()));
	ClassDB::bind_method(D_METHOD("get_overlay_theme"), &AccessibilitySettingsServer::get_overlay_theme);
	ClassDB::bind_method(D_METHOD("is_screen_reader_active"), &AccessibilitySettingsServer::is_screen_reader_active);
	ClassDB::bind_static_method("AccessibilitySettingsServer", D_METHOD("get_control_speech_text", "control"), &AccessibilitySettingsServer::get_control_speech_text);
	ClassDB::bind_method(D_METHOD("speak_control", "control"), &AccessibilitySettingsServer::speak_control);

	ADD_SIGNAL(MethodInfo("color_scheme_changed", PropertyInfo(Variant::OBJECT, "scheme", PROPERTY_HINT_RESOURCE_TYPE, AccessibilityColorScheme::get_class_static())));

	BIND_ENUM_CONSTANT(COLOR_SCHEME_DEFAULT);
	BIND_ENUM_CONSTANT(COLOR_SCHEME_HIGH_CONTRAST_DARK);
	BIND_ENUM_CONSTANT(COLOR_SCHEME_HIGH_CONTRAST_LIGHT);
	BIND_ENUM_CONSTANT(COLOR_SCHEME_CUSTOM);
	BIND_ENUM_CONSTANT(COLOR_SCHEME_MAX);

	BIND_ENUM_CONSTANT(COLORBLIND_MODE_NONE);
	BIND_ENUM_CONSTANT(COLORBLIND_MODE_PROTANOPIA);
	BIND_ENUM_CONSTANT(COLORBLIND_MODE_DEUTERANOPIA);
	BIND_ENUM_CONSTANT(COLORBLIND_MODE_TRITANOPIA);
	BIND_ENUM_CONSTANT(COLORBLIND_MODE_ACHROMATOPSIA);
	BIND_ENUM_CONSTANT(COLORBLIND_MODE_MAX);
}

AccessibilitySettingsServer::AccessibilitySettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	bind_setting_callbacks(SNAME("ui_scale"), callable_mp(this, &AccessibilitySettingsServer::_push_ui_scale), callable_mp(this, &AccessibilitySettingsServer::_pull_ui_scale));
	bind_setting_callbacks(SNAME("font_size_factor"), callable_mp(this, &AccessibilitySettingsServer::_push_font_size_factor), callable_mp(this, &AccessibilitySettingsServer::_pull_font_size_factor));
	bind_setting_callbacks(SNAME("color_scheme"), callable_mp(this, &AccessibilitySettingsServer::_push_color_scheme), callable_mp(this, &AccessibilitySettingsServer::_pull_color_scheme));
	bind_setting_callbacks(SNAME("custom_color_scheme_path"), callable_mp(this, &AccessibilitySettingsServer::_push_custom_color_scheme_path), callable_mp(this, &AccessibilitySettingsServer::_pull_custom_color_scheme_path));
	bind_setting_callbacks(SNAME("focus_outline_width"), callable_mp(this, &AccessibilitySettingsServer::_push_focus_outline_width), callable_mp(this, &AccessibilitySettingsServer::_pull_focus_outline_width));
	bind_setting_callbacks(SNAME("speak_focused_controls"), callable_mp(this, &AccessibilitySettingsServer::_push_speak_focused_controls), callable_mp(this, &AccessibilitySettingsServer::_pull_speak_focused_controls));
	bind_setting_callbacks(SNAME("speech_persona"), callable_mp(this, &AccessibilitySettingsServer::_push_speech_persona), callable_mp(this, &AccessibilitySettingsServer::_pull_speech_persona));

	// Game-side values: stored, validated and signaled, never pushed to the engine.
	static const char *game_values[] = {
		"reduce_motion", "reduce_transparency", "colorblind_mode", "subtitles_enabled", "subtitle_scale", nullptr
	};
	for (int i = 0; game_values[i] != nullptr; i++) {
		set_setting_flags(StringName(game_values[i]), SETTING_FLAG_NO_ENGINE_TARGET);
	}
}

AccessibilitySettingsServer::~AccessibilitySettingsServer() {
	Object *connected = ObjectDB::get_instance(focus_root);
	const Callable callback = callable_mp(this, &AccessibilitySettingsServer::_on_gui_focus_changed);
	if (connected != nullptr && connected->is_connected(SNAME("gui_focus_changed"), callback)) {
		connected->disconnect(SNAME("gui_focus_changed"), callback);
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}
