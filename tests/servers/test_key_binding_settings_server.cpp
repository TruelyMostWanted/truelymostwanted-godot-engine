/**************************************************************************/
/*  test_key_binding_settings_server.cpp                                  */
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

TEST_FORCE_LINK(test_key_binding_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "servers/settings/key_binding.h"
#include "servers/settings/key_binding_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/test_utils.h"

namespace TestKeyBindingSettingsServer {

static constexpr const char *JUMP = "kb_test_jump";
static constexpr const char *FIRE = "kb_test_fire";

static Ref<InputEventKey> make_key(Key p_keycode, bool p_physical) {
	Ref<InputEventKey> event;
	event.instantiate();
	if (p_physical) {
		event->set_physical_keycode(p_keycode);
	} else {
		event->set_keycode(p_keycode);
	}
	return event;
}

// A key as the player presses it: all three codes are set, on the keyboard's device id
// (`InputEvent::DEVICE_ID_KEYBOARD`, the default), like the project bindings in InputMap.
static Ref<InputEventKey> make_pressed_key(Key p_keycode) {
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(p_keycode);
	event->set_physical_keycode(p_keycode);
	event->set_key_label(p_keycode);
	event->set_pressed(true);
	return event;
}

static Ref<InputEventMouseButton> make_mouse_button(MouseButton p_button) {
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_button_index(p_button);
	return event;
}

// The InputMap of the "project": jump on Space and joypad A, fire on the left mouse button.
static void load_project_actions() {
	InputMap *input_map = InputMap::get_singleton();
	for (const char *action : { JUMP, FIRE }) {
		if (input_map->has_action(action)) {
			input_map->erase_action(action);
		}
	}
	input_map->add_action(JUMP, 0.3f);
	input_map->action_add_event(JUMP, make_key(Key::SPACE, false));
	input_map->action_add_event(JUMP, InputEventJoypadButton::create_reference(JoyButton::A));
	input_map->add_action(FIRE, 0.2f);
	input_map->action_add_event(FIRE, make_mouse_button(MouseButton::LEFT));

	// The same actions as project settings (`input/<action>`): reset restores from there, like in a game.
	for (const char *action : { JUMP, FIRE }) {
		Dictionary value;
		value["deadzone"] = input_map->action_get_deadzone(action);
		Array events;
		for (const Ref<InputEvent> &event : *input_map->action_get_events(action)) {
			events.push_back(event);
		}
		value["events"] = events;
		ProjectSettings::get_singleton()->set_setting(String("input/") + action, value);
	}
}

struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
		load_project_actions();
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
		for (const char *action : { JUMP, FIRE }) {
			ProjectSettings::get_singleton()->set_setting(String("input/") + action, Variant());
		}
		InputMap *input_map = InputMap::get_singleton();
		for (const char *action : { JUMP, FIRE }) {
			if (input_map->has_action(action)) {
				input_map->erase_action(action);
			}
		}
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("key_binding_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static bool is_action(const Ref<InputEvent> &p_event, const StringName &p_action) {
	return InputMap::get_singleton()->event_is_action(p_event, p_action);
}

TEST_CASE("[KeyBindingSettingsServer] The domain is an engine singleton registered with the manager") {
	KeyBindingSettingsServer *server = KeyBindingSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("KeyBindingSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("key_bindings") == server);
	CHECK(server->get_domain() == StringName("key_bindings"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_list_property() == StringName("bindings"));
	CHECK(server->get_entry_key() == StringName("action"));
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/key_bindings/include_builtin_actions"));
}

TEST_CASE("[KeyBindingSettingsServer] Text form of key, mouse button, joypad button and joypad axis") {
	// Round trip of every kind.
	for (const String &text : { String("Key:Ctrl+S"), String("PhysicalKey:W"), String("KeyLabel:Alt+Shift+A"), String("Key:CmdOrCtrl+Z"),
				 String("Key:F5"), String("Key:Kp Add"), String("MouseButton:Left"), String("MouseButton:Ctrl+WheelUp"), String("MouseButton:Extra2"),
				 String("JoypadButton:A"), String("JoypadButton:LeftShoulder"), String("JoypadButton:30"),
				 String("JoypadMotion:LeftX+"), String("JoypadMotion:LeftY-"), String("JoypadMotion:TriggerRight+"), String("JoypadMotion:7-") }) {
		const Ref<InputEvent> event = KeyBinding::text_to_event(text);
		REQUIRE_MESSAGE(event.is_valid(), text);
		CHECK_MESSAGE(KeyBinding::event_to_text(event) == text, text);
		CHECK(event->get_device() == -1);
	}

	const Ref<InputEventKey> ctrl_s = KeyBinding::text_to_event("Key:Ctrl+S");
	CHECK(ctrl_s->get_keycode() == Key::S);
	CHECK(ctrl_s->get_physical_keycode() == Key::NONE);
	CHECK(ctrl_s->is_ctrl_pressed());
	const Ref<InputEventKey> undo = KeyBinding::text_to_event("Key:CmdOrCtrl+Z");
	CHECK(undo->is_command_or_control_autoremap());
	const Ref<InputEventJoypadMotion> left_y = KeyBinding::text_to_event("JoypadMotion:LeftY-");
	CHECK(left_y->get_axis() == JoyAxis::LEFT_Y);
	CHECK(left_y->get_axis_value() == doctest::Approx(-1.0));

	// A pressed key is stored by its physical code; a modifier key alone has no modifier flags.
	CHECK(KeyBinding::event_to_text(make_pressed_key(Key::W)) == "PhysicalKey:W");
	Ref<InputEventKey> shift = make_pressed_key(Key::SHIFT);
	shift->set_shift_pressed(true);
	CHECK(KeyBinding::event_to_text(shift) == "PhysicalKey:Shift");

	// Invalid texts and event types that can't be bound.
	for (const String &text : { String(""), String("Key"), String("Key:"), String("Key:NoSuchKey"), String("Key:Hyper+S"), String("Mouse:Left"),
				 String("MouseButton:Sideways"), String("JoypadButton:Z"), String("JoypadMotion:LeftX"), String("JoypadMotion:Nowhere+") }) {
		CHECK_MESSAGE(KeyBinding::text_to_event(text).is_null(), text);
	}
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(KeyBinding::event_to_text(touch).is_empty());
	CHECK(KeyBinding::get_event_device(touch) == KeyBinding::INPUT_DEVICE_MAX);
	CHECK(KeyBinding::get_event_device(make_key(Key::A, false)) == KeyBinding::INPUT_DEVICE_KEYBOARD);
	CHECK(KeyBinding::get_event_device(make_mouse_button(MouseButton::LEFT)) == KeyBinding::INPUT_DEVICE_MOUSE);
	CHECK(KeyBinding::get_event_device(InputEventJoypadButton::create_reference(JoyButton::B)) == KeyBinding::INPUT_DEVICE_GAMEPAD);

	// Labels for the UI.
	CHECK(KeyBinding::event_to_label(KeyBinding::text_to_event("Key:Ctrl+S")) == "Ctrl+S");
	CHECK(KeyBinding::event_to_label(KeyBinding::text_to_event("MouseButton:Right")) == "Right Mouse Button");
	CHECK(KeyBinding::event_to_label(KeyBinding::text_to_event("JoypadButton:A")) == "Joypad A");
	CHECK(KeyBinding::event_to_label(KeyBinding::text_to_event("JoypadMotion:LeftX+")) == "Left Stick Right");
	CHECK(KeyBinding::event_to_label(KeyBinding::text_to_event("PhysicalKey:W")) == "W");
}

TEST_CASE("[SceneTree][KeyBindingSettingsServer] Defaults come from InputMap; rebinding reaches InputMap at once") {
	TestSetup setup;
	KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
	const PackedStringArray actions = server->get_action_names();
	CHECK(actions.has(String(JUMP)));
	CHECK(actions.has(String(FIRE)));
	CHECK_FALSE(actions.has("ui_accept")); // Built-in actions are left out by default.

	CHECK(server->get_slot_count(JUMP, KeyBinding::INPUT_DEVICE_KEYBOARD) == 1);
	CHECK(server->get_slot_count(JUMP, KeyBinding::INPUT_DEVICE_GAMEPAD) == 1);
	CHECK(server->get_slot_count(JUMP, KeyBinding::INPUT_DEVICE_MOUSE) == 0);
	CHECK(KeyBinding::event_to_text(server->get_binding(JUMP, KeyBinding::INPUT_DEVICE_KEYBOARD)) == "Key:Space");
	CHECK(server->get_events(JUMP).size() == 2);
	CHECK(server->get_action_deadzone(JUMP) == doctest::Approx(0.3));

	// Slot 0 replaced, slot 1 added; the device follows from the event.
	CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::UP), 1) == OK);
	CHECK(server->get_slot_count(JUMP, KeyBinding::INPUT_DEVICE_KEYBOARD) == 2);
	CHECK(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(is_action(make_pressed_key(Key::UP), JUMP));
	CHECK_FALSE(is_action(make_pressed_key(Key::SPACE), JUMP));
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));
	// The pad id of the pressed button is not kept: the binding is for all pads.
	const Ref<InputEventJoypadButton> pad_b = InputEventJoypadButton::create_reference(JoyButton::B, 2);
	CHECK(server->rebind(JUMP, pad_b) == OK);
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::B, 0), JUMP));
	CHECK_FALSE(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));

	// Cleared slot: the key no longer works.
	server->clear_binding(JUMP, KeyBinding::INPUT_DEVICE_KEYBOARD, 1);
	CHECK_FALSE(is_action(make_pressed_key(Key::UP), JUMP));
	CHECK(server->get_binding(JUMP, KeyBinding::INPUT_DEVICE_KEYBOARD, 1).is_null());

	server->set_action_deadzone(JUMP, 0.6f);
	CHECK(InputMap::get_singleton()->action_get_deadzone(JUMP) == doctest::Approx(0.6));

	// Errors.
	ERR_PRINT_OFF;
	CHECK(server->rebind("kb_test_no_such_action", make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(server->rebind(JUMP, touch) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[SceneTree][KeyBindingSettingsServer] Conflicts and reset to the project bindings") {
	TestSetup setup;
	KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);

	// A pressed Space matches the stored `Key:Space` of jump.
	PackedStringArray conflicts = server->find_conflicts(make_pressed_key(Key::SPACE));
	CHECK(conflicts.size() == 1);
	CHECK(conflicts.has(String(JUMP)));
	CHECK(server->find_conflicts(make_pressed_key(Key::SPACE), JUMP).is_empty());
	CHECK(server->find_conflicts(make_mouse_button(MouseButton::LEFT)).has(String(FIRE)));
	CHECK(server->find_conflicts(make_pressed_key(Key::K)).is_empty());

	CHECK(server->rebind(FIRE, make_pressed_key(Key::SPACE)) == OK);
	conflicts = server->find_conflicts(make_pressed_key(Key::SPACE));
	CHECK(conflicts.size() == 2);

	// reset_action() restores one action.
	CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
	server->reset_action(JUMP);
	CHECK(is_action(make_pressed_key(Key::SPACE), JUMP));
	CHECK_FALSE(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(is_action(make_pressed_key(Key::SPACE), FIRE));

	// reset_all() restores every action.
	server->reset_all();
	CHECK_FALSE(is_action(make_pressed_key(Key::SPACE), FIRE));
	CHECK(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	CHECK(SettingsServer::values_equal(server->get_value("bindings"), server->get_default_value("bindings")));
	memdelete(server);
}

TEST_CASE("[SceneTree][KeyBindingSettingsServer] Rebound, saved, restarted: the new key works, the old one does not (INI, JSON, CSV)") {
	TestSetup setup;
	for (const String &extension : { String("ini"), String("json"), String("csv") }) {
		const String path = temp_file("key_bindings." + extension);
		load_project_actions();
		{
			KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
			server->set_file_path(path);
			CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
			CHECK(server->rebind(JUMP, InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f)) == OK);
			CHECK(server->rebind(FIRE, KeyBinding::text_to_event("MouseButton:Ctrl+Right")) == OK);
			server->clear_binding(FIRE, KeyBinding::INPUT_DEVICE_MOUSE, 1);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path);
		CHECK_MESSAGE(text.contains("PhysicalKey:W"), extension);
		CHECK_MESSAGE(text.contains("JoypadMotion:LeftY-"), extension);
		CHECK_MESSAGE(text.contains("MouseButton:Ctrl+Right"), extension);

		// "Restart": InputMap is back at the project settings.
		load_project_actions();
		KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK_MESSAGE(is_action(make_pressed_key(Key::W), JUMP), extension);
		CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
		const Ref<InputEventJoypadMotion> stick_up = InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f, 0);
		CHECK_MESSAGE(is_action(stick_up, JUMP), extension);
		CHECK_FALSE(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));
		Ref<InputEventMouseButton> ctrl_right = make_mouse_button(MouseButton::RIGHT);
		ctrl_right->set_ctrl_pressed(true);
		CHECK_MESSAGE(is_action(ctrl_right, FIRE), extension);
		CHECK_FALSE(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
		CHECK_FALSE(server->is_dirty());

		// Reset gives back the bindings of the project settings.
		server->reset_all();
		CHECK(is_action(make_pressed_key(Key::SPACE), JUMP));
		CHECK_FALSE(is_action(make_pressed_key(Key::W), JUMP));
		CHECK(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
		memdelete(server);
	}
}

TEST_CASE("[SceneTree][KeyBindingSettingsServer] A saved action that no longer exists is skipped; new project actions keep their bindings") {
	TestSetup setup;
	const String path = temp_file("key_bindings_old.ini");
	{
		KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
		server->set_file_path(path);
		Ref<KeyBinding> gone;
		gone.instantiate();
		gone->set_action("kb_test_removed");
		gone->set_event("Key:G");
		ERR_PRINT_OFF; // Applying warns once that the project has no such action.
		server->add_entry(gone);
		ERR_PRINT_ON;
		CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
		// The file of an older version has no row for fire.
		for (int i = server->get_entry_count() - 1; i >= 0; i--) {
			const Ref<KeyBinding> row = server->get_entry(i);
			if (row->get_action() == String(FIRE)) {
				server->remove_entry(i);
			}
		}
		CHECK(server->save() == OK);
		memdelete(server);
	}
	CHECK(FileAccess::get_file_as_string(path).contains("kb_test_removed"));

	load_project_actions();
	KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
	server->set_file_path(path);
	ERR_PRINT_OFF;
	server->startup();
	ERR_PRINT_ON;
	CHECK_FALSE(server->get_action_names().has("kb_test_removed"));
	CHECK_FALSE(InputMap::get_singleton()->has_action("kb_test_removed"));
	CHECK(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(server->has_action(FIRE));
	CHECK(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	memdelete(server);
}

TEST_CASE("[SceneTree][KeyBindingSettingsServer] Other event types of an action are kept") {
	TestSetup setup;
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	touch->set_index(0);
	InputMap::get_singleton()->action_add_event(FIRE, touch);
	KeyBindingSettingsServer *server = memnew(KeyBindingSettingsServer);
	CHECK(server->rebind(FIRE, make_pressed_key(Key::F)) == OK);
	CHECK(is_action(make_pressed_key(Key::F), FIRE));
	bool has_touch = false;
	for (const Ref<InputEvent> &event : *InputMap::get_singleton()->action_get_events(FIRE)) {
		has_touch = has_touch || event == touch;
	}
	CHECK(has_touch);
	memdelete(server);
}

} // namespace TestKeyBindingSettingsServer
