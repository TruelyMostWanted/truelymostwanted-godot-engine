/**************************************************************************/
/*  test_accessibility_settings_server.cpp                                */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_accessibility_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "scene/theme/theme_db.h"
#include "servers/settings/accessibility_color_scheme.h"
#include "servers/settings/accessibility_settings.h"
#include "servers/settings/accessibility_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/tts/tts_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestAccessibilitySettingsServer {

struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("accessibility_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static int focus_connection_count(const Window *p_root) {
	List<Object::Connection> connections;
	p_root->get_signal_connection_list(SNAME("gui_focus_changed"), &connections);
	return connections.size();
}

TEST_CASE("[AccessibilitySettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	AccessibilitySettingsServer *server = AccessibilitySettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("AccessibilitySettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("accessibility") == server);
	CHECK(server->get_domain() == StringName("accessibility"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/accessibility.ini");

	// Engine-applied values are bound; game-side values have no engine target.
	static const char *engine_values[] = { "ui_scale", "font_size_factor", "color_scheme", "custom_color_scheme_path", "focus_outline_width", "speak_focused_controls", "speech_persona", nullptr };
	for (int i = 0; engine_values[i] != nullptr; i++) {
		CHECK(server->is_setting_bound(engine_values[i]));
		CHECK_FALSE(server->get_setting_flags(engine_values[i]).has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
	}
	static const char *game_values[] = { "reduce_motion", "reduce_transparency", "colorblind_mode", "subtitles_enabled", "captions_enabled", "subtitle_scale", nullptr };
	for (int i = 0; game_values[i] != nullptr; i++) {
		CHECK(server->get_setting_names().has(game_values[i]));
		CHECK(server->get_setting_flags(game_values[i]).has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
	}
}

TEST_CASE("[AccessibilitySettingsServer] Defaults and validation") {
	TestSetup setup;
	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	CHECK(server->get_ui_scale() == doctest::Approx(1.0));
	CHECK(server->get_font_size_factor() == doctest::Approx(1.0));
	// The test display server does not know the OS preferences (-1): the off defaults stay.
	CHECK(server->get_color_scheme() == AccessibilitySettingsServer::COLOR_SCHEME_DEFAULT);
	CHECK_FALSE(server->is_reducing_motion());
	CHECK_FALSE(server->is_reducing_transparency());
	CHECK(server->get_focus_outline_width() == 0);
	CHECK_FALSE(server->is_speaking_focused_controls());
	CHECK(server->get_colorblind_mode() == AccessibilitySettingsServer::COLORBLIND_MODE_NONE);
	CHECK_FALSE(server->is_subtitles_enabled());
	CHECK(server->is_captions_enabled());
	CHECK(server->get_subtitle_scale() == doctest::Approx(1.0));

	// Ranges are clamped.
	server->set_ui_scale(10.0);
	CHECK(server->get_ui_scale() == doctest::Approx(4.0));
	server->set_font_size_factor(0.1);
	CHECK(server->get_font_size_factor() == doctest::Approx(0.5));
	server->set_focus_outline_width(20);
	CHECK(server->get_focus_outline_width() == 8);

	// Enum values outside the enum keep the current value.
	server->set_value("color_scheme", 2);
	CHECK(server->get_color_scheme() == AccessibilitySettingsServer::COLOR_SCHEME_HIGH_CONTRAST_LIGHT);
	ERR_PRINT_OFF;
	server->set_value("color_scheme", 42);
	server->set_color_scheme(AccessibilitySettingsServer::COLOR_SCHEME_MAX);
	server->set_colorblind_mode(AccessibilitySettingsServer::COLORBLIND_MODE_MAX);
	ERR_PRINT_ON;
	CHECK(server->get_color_scheme() == AccessibilitySettingsServer::COLOR_SCHEME_HIGH_CONTRAST_LIGHT);
	CHECK(server->get_colorblind_mode() == AccessibilitySettingsServer::COLORBLIND_MODE_NONE);
	server->reset_to_defaults();
	memdelete(server);
}

TEST_CASE("[AccessibilitySettingsServer] Helpers for the game-side values") {
	TestSetup setup;
	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	CHECK(server->get_motion_scale() == doctest::Approx(1.0));
	CHECK(server->get_animation_duration(0.5) == doctest::Approx(0.5));
	server->set_reduce_motion(true);
	CHECK(server->get_motion_scale() == doctest::Approx(0.0));
	CHECK(server->get_animation_duration(0.5) == doctest::Approx(0.0));

	server->set_font_size_factor(1.5);
	CHECK(server->scale_font_size(16) == 24);
	server->set_subtitle_scale(2.0);
	CHECK(server->scale_subtitle_font_size(10) == 30);

	// Without a color scheme the game's own colors are kept.
	CHECK(server->get_active_color_scheme().is_null());
	CHECK(server->get_scheme_color("problem", Color(1, 0, 1)) == Color(1, 0, 1));
	server->reset_to_defaults();
	memdelete(server);
}

TEST_CASE("[AccessibilitySettingsServer] Color schemes") {
	const Ref<AccessibilityColorScheme> dark = AccessibilityColorScheme::create_high_contrast_dark();
	const Ref<AccessibilityColorScheme> light = AccessibilityColorScheme::create_high_contrast_light();
	CHECK(dark->get_text_color() == Color(1, 1, 1));
	CHECK(dark->get_surface_color() == Color(0, 0, 0));
	CHECK(light->get_text_color() == Color(0, 0, 0));
	CHECK(light->get_surface_color() == Color(1, 1, 1));
	CHECK(dark->get_role_color("focus") == dark->get_focus_color());
	CHECK(dark->get_role_color("accent_border") == dark->get_accent_border_color());
	CHECK(dark->get_role_color("unknown", Color(0.5, 0.5, 0.5)) == Color(0.5, 0.5, 0.5));
}

TEST_CASE("[AccessibilitySettingsServer] UI scale and a high-contrast scheme survive a restart") {
	TestSetup setup;
	const String path = temp_file("accessibility.ini");
	{
		AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
		server->set_file_path(path);
		server->set_ui_scale(1.5);
		server->set_color_scheme(AccessibilitySettingsServer::COLOR_SCHEME_HIGH_CONTRAST_DARK);
		server->set_reduce_motion(true);
		server->set_speech_persona("narrator");
		CHECK(server->save() == OK);
		// Back to the defaults before the next instance takes over the engine targets.
		server->reset_to_defaults();
		server->apply();
		memdelete(server);
	}
	// Plain INI that a player can edit.
	const String text = FileAccess::get_file_as_string(path);
	CHECK(text.contains("ui_scale"));
	CHECK(text.contains("color_scheme"));

	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(server->get_ui_scale() == doctest::Approx(1.5));
	CHECK(server->get_color_scheme() == AccessibilitySettingsServer::COLOR_SCHEME_HIGH_CONTRAST_DARK);
	CHECK(server->is_reducing_motion());
	CHECK(server->get_speech_persona() == "narrator");
	CHECK(server->get_active_color_scheme().is_valid());
	CHECK_FALSE(server->is_dirty());
	server->reset_to_defaults();
	server->apply();
	memdelete(server);
}

TEST_CASE("[SceneTree][AccessibilitySettingsServer] UI scale, font size and color scheme reach the root window") {
	TestSetup setup;
	Window *root = SceneTree::get_singleton()->get_root().ptr();
	REQUIRE(root != nullptr);
	const real_t base_factor = root->get_content_scale_factor();
	const Ref<Theme> previous_theme = root->get_theme();

	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	server->set_file_path(temp_file("accessibility_root.ini"));
	server->startup();
	CHECK(server->get_overlay_theme().is_null());

	server->set_ui_scale(1.5);
	CHECK(root->get_content_scale_factor() == doctest::Approx(base_factor * 1.5));
	server->set_ui_scale(2.0);
	CHECK(root->get_content_scale_factor() == doctest::Approx(base_factor * 2.0));

	server->set_font_size_factor(2.0);
	Ref<Theme> overlay = server->get_overlay_theme();
	REQUIRE(overlay.is_valid());
	CHECK(root->get_theme() == overlay);
	CHECK(overlay->get_default_font_size() == ThemeDB::get_singleton()->get_fallback_font_size() * 2);

	server->set_color_scheme(AccessibilitySettingsServer::COLOR_SCHEME_HIGH_CONTRAST_DARK);
	overlay = server->get_overlay_theme();
	REQUIRE(overlay.is_valid());
	CHECK(root->get_theme() == overlay);
	REQUIRE(server->get_active_color_scheme().is_valid());
	CHECK(server->get_scheme_color("problem") == server->get_active_color_scheme()->get_problem_color());
	const Ref<Theme> default_theme = ThemeDB::get_singleton()->get_default_theme();
	if (default_theme.is_valid() && default_theme->has_color("font_color", "Button")) {
		CHECK(overlay->get_color("font_color", "Button") == Color(1, 1, 1));
		const Ref<StyleBoxFlat> focus = overlay->get_stylebox("focus", "Button");
		REQUIRE(focus.is_valid());
		CHECK(focus->get_border_color() == server->get_active_color_scheme()->get_focus_color());
		CHECK_FALSE(focus->is_draw_center_enabled());
	}

	server->set_focus_outline_width(5);
	overlay = server->get_overlay_theme();
	if (default_theme.is_valid() && default_theme->has_stylebox("focus", "Button")) {
		const Ref<StyleBoxFlat> focus = overlay->get_stylebox("focus", "Button");
		REQUIRE(focus.is_valid());
		CHECK(focus->get_border_width_min() == 5);
	}

	// Back to the defaults: the root window gets its previous theme and factor again.
	server->reset_to_defaults();
	server->apply();
	CHECK(server->get_overlay_theme().is_null());
	CHECK(root->get_theme() == previous_theme);
	CHECK(root->get_content_scale_factor() == doctest::Approx(base_factor));
	memdelete(server);
}

TEST_CASE("[SceneTree][AccessibilitySettingsServer] The spoken text of a control") {
	Button *button = memnew(Button);
	button->set_text("Play");
	CHECK(AccessibilitySettingsServer::get_control_speech_text(button) == "Play");
	button->set_accessibility_name("Start the game");
	CHECK(AccessibilitySettingsServer::get_control_speech_text(button) == "Start the game");
	memdelete(button);

	LineEdit *line_edit = memnew(LineEdit);
	line_edit->set_placeholder("Player name");
	CHECK(AccessibilitySettingsServer::get_control_speech_text(line_edit) == "Player name");
	line_edit->set_text("Ada");
	CHECK(AccessibilitySettingsServer::get_control_speech_text(line_edit) == "Ada");
	memdelete(line_edit);

	Control *control = memnew(Control);
	CHECK(AccessibilitySettingsServer::get_control_speech_text(control).is_empty());
	control->set_tooltip_text("Volume");
	CHECK(AccessibilitySettingsServer::get_control_speech_text(control) == "Volume");
	memdelete(control);
}

TEST_CASE("[SceneTree][AccessibilitySettingsServer] Speaking focused controls connects to the root window") {
	TestSetup setup;
	Window *root = SceneTree::get_singleton()->get_root().ptr();
	REQUIRE(root != nullptr);
	const int connections_before = focus_connection_count(root);

	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	server->set_file_path(temp_file("accessibility_speech.ini"));
	server->startup();
	server->set_speak_focused_controls(true);
	CHECK(focus_connection_count(root) == connections_before + 1);
	server->set_speak_focused_controls(false);
	CHECK(focus_connection_count(root) == connections_before);

	// No TTS voice in the test run: nothing is spoken, no error.
	Button *button = memnew(Button);
	button->set_text("Play");
	if (TTSServer::get_singleton() == nullptr || !TTSServer::get_singleton()->is_available()) {
		CHECK(server->speak_control(button) == -1);
	}
	memdelete(button);
	memdelete(server);
}

TEST_CASE("[AccessibilitySettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	AccessibilitySettingsServer *server = memnew(AccessibilitySettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "ui_scale_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("ui_scale", 1.5);
	SIGNAL_CHECK_FALSE("ui_scale_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 1.5 } };
	SIGNAL_CHECK("ui_scale_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("ui_scale_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("ui_scale");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("ui_scale_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("ui_scale_changed", default_args);

	SIGNAL_UNWATCH(server, "ui_scale_changed");
	memdelete(server);
}

} // namespace TestAccessibilitySettingsServer
