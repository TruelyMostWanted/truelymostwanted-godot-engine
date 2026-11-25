/**************************************************************************/
/*  test_input_settings_server.cpp                                        */
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

TEST_FORCE_LINK(test_input_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "servers/settings/input_settings.h"
#include "servers/settings/input_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestInputSettingsServer {

struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("input_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[InputSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	InputSettingsServer *server = InputSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("InputSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("input") == server);
	CHECK(server->get_domain() == StringName("input"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_file_path() == "user://settings/input.ini");
	CHECK(server->get_setting_names().has("mouse_sensitivity"));
	CHECK(server->get_setting_names().has("use_accumulated_input"));

	// Game-side values have no engine target; the Input options have one.
	CHECK(server->get_setting_flags("mouse_sensitivity").has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
	CHECK(server->get_setting_flags("toggle_actions").has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
	CHECK(server->is_setting_bound("emulate_touch_from_mouse"));
	CHECK_FALSE(server->get_setting_flags("emulate_touch_from_mouse").has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
}

TEST_CASE("[InputSettingsServer] Defaults and validation") {
	TestSetup setup;
	InputSettingsServer *server = memnew(InputSettingsServer);
	for (int device = 0; device < InputSettingsServer::DEVICE_MAX; device++) {
		CHECK(server->get_sensitivity(InputSettingsServer::Device(device)) == doctest::Approx(1.0));
		CHECK_FALSE(server->is_x_inverted(InputSettingsServer::Device(device)));
		CHECK_FALSE(server->is_y_inverted(InputSettingsServer::Device(device)));
	}
	CHECK(server->get_gamepad_deadzone() == doctest::Approx(0.2));
	CHECK(server->is_vibration_enabled());
	CHECK(server->get_vibration_strength() == doctest::Approx(1.0));
	CHECK(server->get_mouse_smoothing() == doctest::Approx(0.0));
	CHECK(server->get_first_player_device_id() == -1);
	CHECK(server->is_keyboard_allowed_for_current_player());

	// Ranges are clamped.
	server->set_sensitivity(InputSettingsServer::DEVICE_MOUSE, 50.0);
	CHECK(server->get_sensitivity(InputSettingsServer::DEVICE_MOUSE) == doctest::Approx(10.0));
	server->set_gamepad_deadzone(2.0);
	CHECK(server->get_gamepad_deadzone() == doctest::Approx(0.95));
	server->set_vibration_strength(-1.0);
	CHECK(server->get_vibration_strength() == doctest::Approx(0.0));
	server->set_mouse_smoothing(1.0);
	CHECK(server->get_mouse_smoothing() == doctest::Approx(0.95));

	// Toggle actions without empty names and duplicates.
	PackedStringArray actions;
	actions.push_back("crouch");
	actions.push_back(" ");
	actions.push_back("sprint ");
	actions.push_back("crouch");
	server->set_toggle_actions(actions);
	const PackedStringArray stored = server->get_toggle_actions();
	REQUIRE(stored.size() == 2);
	CHECK(stored[0] == "crouch");
	CHECK(stored[1] == "sprint");
	CHECK(server->is_toggle_action("sprint"));
	CHECK_FALSE(server->is_toggle_action("aim"));

	// An invalid device is an error, not a crash.
	ERR_PRINT_OFF;
	CHECK(server->get_sensitivity(InputSettingsServer::DEVICE_MAX) == doctest::Approx(1.0));
	CHECK(server->apply_to_vector(InputSettingsServer::DEVICE_MAX, Vector2(1, 2)) == Vector2(1, 2));
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[InputSettingsServer] Y inversion and mouse sensitivity survive a restart") {
	TestSetup setup;
	const String path = temp_file("input.ini");
	{
		InputSettingsServer *server = memnew(InputSettingsServer);
		server->set_file_path(path);
		server->set_y_inverted(InputSettingsServer::DEVICE_MOUSE, true);
		server->set_sensitivity(InputSettingsServer::DEVICE_MOUSE, 2.5);
		CHECK(server->apply_to_mouse_motion(Vector2(2, 4)) == Vector2(5, -10));
		CHECK(server->save() == OK);
		memdelete(server);
	}
	// Plain INI that a player can edit.
	const String text = FileAccess::get_file_as_string(path);
	CHECK(text.contains("mouse_y_inverted"));
	CHECK(text.contains("mouse_sensitivity"));

	InputSettingsServer *server = memnew(InputSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(server->is_y_inverted(InputSettingsServer::DEVICE_MOUSE));
	CHECK(server->get_sensitivity(InputSettingsServer::DEVICE_MOUSE) == doctest::Approx(2.5));
	CHECK(server->apply_to_mouse_motion(Vector2(2, 4)) == Vector2(5, -10));
	CHECK_FALSE(server->is_dirty());

	Ref<InputEventMouseMotion> motion;
	motion.instantiate();
	motion->set_relative_screen_position(Vector2(2, 4));
	CHECK(server->get_look_vector(motion) == Vector2(5, -10));
	memdelete(server);
}

TEST_CASE("[InputSettingsServer] Helpers apply sensitivity, inversion, deadzone and smoothing") {
	TestSetup setup;
	InputSettingsServer *server = memnew(InputSettingsServer);

	server->set_sensitivity(InputSettingsServer::DEVICE_KEYBOARD, 2.0);
	server->set_x_inverted(InputSettingsServer::DEVICE_KEYBOARD, true);
	CHECK(server->apply_to_vector(InputSettingsServer::DEVICE_KEYBOARD, Vector2(1, 1)) == Vector2(-2, 2));

	// Inside the deadzone: nothing. At full tilt: full length; in between: rescaled.
	CHECK(server->apply_to_stick(Vector2(0.1, 0.1)) == Vector2());
	CHECK(server->apply_to_stick(Vector2(1, 0)).is_equal_approx(Vector2(1, 0)));
	CHECK(server->apply_to_stick(Vector2(0, 0.6)).is_equal_approx(Vector2(0, 0.5)));
	server->set_y_inverted(InputSettingsServer::DEVICE_GAMEPAD, true);
	server->set_sensitivity(InputSettingsServer::DEVICE_GAMEPAD, 3.0);
	CHECK(server->apply_to_stick(Vector2(0, 0.6)).is_equal_approx(Vector2(0, -1.5)));

	// Right stick events are look input, left stick events are not.
	Ref<InputEventJoypadMotion> right;
	right.instantiate();
	right->set_axis(JoyAxis::RIGHT_X);
	right->set_axis_value(1.0);
	CHECK(server->get_look_vector(right).is_equal_approx(Vector2(3, 0)));
	Ref<InputEventJoypadMotion> left;
	left.instantiate();
	left->set_axis(JoyAxis::LEFT_X);
	left->set_axis_value(1.0);
	CHECK(server->get_look_vector(left) == Vector2());

	// Smoothing blends with the previous output.
	server->set_mouse_smoothing(0.5);
	CHECK(server->apply_to_mouse_motion(Vector2(4, 0)).is_equal_approx(Vector2(2, 0)));
	CHECK(server->apply_to_mouse_motion(Vector2(4, 0)).is_equal_approx(Vector2(3, 0)));
	server->reset_mouse_smoothing();
	CHECK(server->apply_to_mouse_motion(Vector2(4, 0)).is_equal_approx(Vector2(2, 0)));
	memdelete(server);
}

TEST_CASE("[InputSettingsServer] Player events follow the first player device") {
	TestSetup setup;
	InputSettingsServer *server = memnew(InputSettingsServer);
	Ref<InputEventJoypadButton> pad;
	pad.instantiate();
	pad->set_device(1);
	Ref<InputEventKey> key;
	key.instantiate();

	CHECK(server->is_player_event(pad));
	CHECK(server->is_player_event(key));
	server->set_first_player_device_id(0);
	CHECK_FALSE(server->is_player_event(pad));
	server->set_first_player_device_id(1);
	CHECK(server->is_player_event(pad));
	server->set_allow_keyboard_for_current_player(false);
	CHECK_FALSE(server->is_player_event(key));
	memdelete(server);
}

TEST_CASE("[SceneTree][InputSettingsServer] The Input options take effect at once") {
	TestSetup setup;
	Input *input = Input::get_singleton();
	REQUIRE(input != nullptr);
	const bool original_accumulated = input->is_using_accumulated_input();
	const bool original_touch = input->is_emulating_touch_from_mouse();
	const bool original_mouse = input->is_emulating_mouse_from_touch();

	InputSettingsServer *server = memnew(InputSettingsServer);
	CHECK(server->is_using_accumulated_input() == original_accumulated);

	server->set_use_accumulated_input(!original_accumulated);
	CHECK(input->is_using_accumulated_input() == !original_accumulated);
	server->set_emulate_touch_from_mouse(!original_touch);
	CHECK(input->is_emulating_touch_from_mouse() == !original_touch);
	server->set_emulate_mouse_from_touch(!original_mouse);
	CHECK(input->is_emulating_mouse_from_touch() == !original_mouse);

	server->reset_to_defaults();
	CHECK(input->is_using_accumulated_input() == original_accumulated);
	CHECK(input->is_emulating_touch_from_mouse() == original_touch);
	CHECK(input->is_emulating_mouse_from_touch() == original_mouse);
	memdelete(server);
}

TEST_CASE("[SceneTree][InputSettingsServer] Toggle actions stay active after the press") {
	TestSetup setup;
	Input *input = Input::get_singleton();
	InputMap::get_singleton()->add_action("test_crouch");
	InputMap::get_singleton()->add_action("test_sprint");

	InputSettingsServer *server = memnew(InputSettingsServer);
	PackedStringArray actions;
	actions.push_back("test_crouch");
	server->set_toggle_actions(actions);

	CHECK_FALSE(server->is_action_active("test_crouch"));
	CHECK_FALSE(server->is_action_active("test_sprint"));
	// The frame counter does not advance in unit tests: forget the frame of the check above.
	server->reset_action_states();

	input->action_press("test_crouch");
	input->action_press("test_sprint");
	CHECK(server->is_action_active("test_crouch"));
	CHECK(server->is_action_active("test_sprint"));

	// Hold: follows the button. Toggle: stays on; a second call in the same frame does not flip it back.
	input->action_release("test_crouch");
	input->action_release("test_sprint");
	CHECK(server->is_action_active("test_crouch"));
	CHECK_FALSE(server->is_action_active("test_sprint"));

	server->reset_action_states();
	server->set_toggle_actions(PackedStringArray());
	CHECK_FALSE(server->is_action_active("test_crouch"));
	memdelete(server);

	InputMap::get_singleton()->erase_action("test_crouch");
	InputMap::get_singleton()->erase_action("test_sprint");
}

TEST_CASE("[SceneTree][InputSettingsServer] Vibration respects the settings") {
	// The mouse mode is not checked here: the headless display server of the tests does not keep it.
	TestSetup setup;
	Input *input = Input::get_singleton();
	InputSettingsServer *server = memnew(InputSettingsServer);

	server->set_vibration_strength(0.5);
	server->vibrate(0, 1.0, 0.8, 0.5);
	CHECK(input->get_joy_vibration_strength(0).is_equal_approx(Vector2(0.5, 0.4)));
	input->stop_joy_vibration(0);
	server->set_vibration_enabled(false);
	server->vibrate(0, 1.0, 1.0, 0.5);
	CHECK(input->get_joy_vibration_strength(0) == Vector2());
	memdelete(server);
}

TEST_CASE("[InputSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	InputSettingsServer *server = memnew(InputSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "mouse_sensitivity_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("mouse_sensitivity", 2.0);
	SIGNAL_CHECK_FALSE("mouse_sensitivity_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 2.0 } };
	SIGNAL_CHECK("mouse_sensitivity_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("mouse_sensitivity_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("mouse_sensitivity");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("mouse_sensitivity_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("mouse_sensitivity_changed", default_args);

	SIGNAL_UNWATCH(server, "mouse_sensitivity_changed");
	memdelete(server);
}

} // namespace TestInputSettingsServer
