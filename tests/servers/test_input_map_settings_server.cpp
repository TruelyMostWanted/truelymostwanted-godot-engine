/**************************************************************************/
/*  test_input_map_settings_server.cpp                                    */
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

TEST_FORCE_LINK(test_input_map_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_action.h"
#include "core/input/input_actions.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_text_serializer.h"
#include "servers/settings/input_map_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/test_utils.h"

namespace TestInputMapSettingsServer {

static constexpr const char *JUMP = "kb_test_jump";
static constexpr const char *FIRE = "kb_test_fire";
static constexpr const char *SETTING_SAVE_MODE = "settings/input_map/save_mode";

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
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		load_project_actions();
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
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

// A built-in action in the InputMap, and the project setting `save_mode` for the lifetime of the object.
struct BuiltinSetup {
	bool added = false;
	int old_mode = 0;

	explicit BuiltinSetup(InputMapSettingsServer::SaveMode p_mode) {
		InputMap *input_map = InputMap::get_singleton();
		if (!input_map->has_action("ui_accept")) {
			input_map->add_action("ui_accept", 0.5f);
			input_map->action_add_event("ui_accept", make_key(Key::ENTER, false));
			added = true;
		}
		old_mode = ProjectSettings::get_singleton()->get_setting(SETTING_SAVE_MODE, 0);
		ProjectSettings::get_singleton()->set_setting(SETTING_SAVE_MODE, int(p_mode));
	}
	~BuiltinSetup() {
		ProjectSettings::get_singleton()->set_setting(SETTING_SAVE_MODE, old_mode);
		if (added) {
			InputMap::get_singleton()->erase_action("ui_accept");
		}
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("input_map_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static bool is_action(const Ref<InputEvent> &p_event, const StringName &p_action) {
	return InputMap::get_singleton()->event_is_action(p_event, p_action);
}

TEST_CASE("[InputMapSettingsServer] The domain is an engine singleton registered with the manager") {
	InputMapSettingsServer *server = InputMapSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("InputMapSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("input_map") == server);
	CHECK(server->get_domain() == StringName("input_map"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_list_property() == StringName("actions"));
	CHECK(server->get_entry_key() == StringName("action"));
	CHECK(ProjectSettings::get_singleton()->has_setting(SETTING_SAVE_MODE));
	CHECK_FALSE(ProjectSettings::get_singleton()->has_setting("settings/input_map/include_builtin_actions"));
}

TEST_CASE("[InputMapSettingsServer] Text form of key, mouse button, joypad button and joypad axis") {
	// Round trip of every kind.
	for (const String &text : { String("Key:Ctrl+S"), String("PhysicalKey:W"), String("KeyLabel:Alt+Shift+A"), String("Key:CmdOrCtrl+Z"),
				 String("Key:F5"), String("Key:Kp Add"), String("MouseButton:Left"), String("MouseButton:Ctrl+WheelUp"), String("MouseButton:Extra2"),
				 String("JoypadButton:A"), String("JoypadButton:LeftShoulder"), String("JoypadButton:30"),
				 String("JoypadMotion:LeftX+"), String("JoypadMotion:LeftY-"), String("JoypadMotion:TriggerRight+"), String("JoypadMotion:7-") }) {
		const Ref<InputEvent> event = InputAction::text_to_event(text);
		REQUIRE_MESSAGE(event.is_valid(), text);
		CHECK_MESSAGE(InputAction::event_to_text(event) == text, text);
		CHECK(event->get_device() == -1);
	}

	const Ref<InputEventKey> ctrl_s = InputAction::text_to_event("Key:Ctrl+S");
	CHECK(ctrl_s->get_keycode() == Key::S);
	CHECK(ctrl_s->get_physical_keycode() == Key::NONE);
	CHECK(ctrl_s->is_ctrl_pressed());
	const Ref<InputEventKey> undo = InputAction::text_to_event("Key:CmdOrCtrl+Z");
	CHECK(undo->is_command_or_control_autoremap());
	const Ref<InputEventJoypadMotion> left_y = InputAction::text_to_event("JoypadMotion:LeftY-");
	CHECK(left_y->get_axis() == JoyAxis::LEFT_Y);
	CHECK(left_y->get_axis_value() == doctest::Approx(-1.0));

	// A pressed key is stored by its physical code; a modifier key alone has no modifier flags.
	CHECK(InputAction::event_to_text(make_pressed_key(Key::W)) == "PhysicalKey:W");
	Ref<InputEventKey> shift = make_pressed_key(Key::SHIFT);
	shift->set_shift_pressed(true);
	CHECK(InputAction::event_to_text(shift) == "PhysicalKey:Shift");

	// Invalid texts and event types that can't be bound.
	for (const String &text : { String(""), String("Key"), String("Key:"), String("Key:NoSuchKey"), String("Key:Hyper+S"), String("Mouse:Left"),
				 String("MouseButton:Sideways"), String("JoypadButton:Z"), String("JoypadMotion:LeftX"), String("JoypadMotion:Nowhere+") }) {
		CHECK_MESSAGE(InputAction::text_to_event(text).is_null(), text);
	}
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(InputAction::event_to_text(touch).is_empty());
	CHECK(InputAction::get_event_device(touch) == InputAction::INPUT_DEVICE_MAX);
	CHECK(InputAction::get_event_device(make_key(Key::A, false)) == InputAction::INPUT_DEVICE_KEYBOARD);
	CHECK(InputAction::get_event_device(make_mouse_button(MouseButton::LEFT)) == InputAction::INPUT_DEVICE_MOUSE);
	CHECK(InputAction::get_event_device(InputEventJoypadButton::create_reference(JoyButton::B)) == InputAction::INPUT_DEVICE_GAMEPAD);

	// Labels for the UI.
	const InputMapSettingsServer *label_server = InputMapSettingsServer::get_singleton();
	REQUIRE(label_server != nullptr);
	CHECK(label_server->get_event_label(InputAction::text_to_event("Key:Ctrl+S")) == "Ctrl+S");
	CHECK(label_server->get_event_label(InputAction::text_to_event("MouseButton:Right")) == "Right Mouse Button");
	CHECK(label_server->get_event_label(InputAction::text_to_event("JoypadButton:A")) == "Joypad A");
	CHECK(label_server->get_event_label(InputAction::text_to_event("JoypadMotion:LeftX+")) == "Left Stick Right");
	CHECK(label_server->get_event_label(InputAction::text_to_event("PhysicalKey:W")) == "W");
}

TEST_CASE("[InputMapSettingsServer] Text form of the events of an action") {
	TypedArray<InputEvent> events;
	events.push_back(InputAction::text_to_event("Key:Ctrl+S"));
	events.push_back(InputAction::text_to_event("PhysicalKey:W"));
	events.push_back(InputAction::text_to_event("MouseButton:Left"));
	events.push_back(InputEventJoypadButton::create_reference(JoyButton::A, 1));
	events.push_back(InputAction::text_to_event("JoypadMotion:LeftX-"));
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	events.push_back(touch); // No text form: skipped.
	const String text = InputMapSettingsServer::events_to_text(events);
	CHECK(text == "Key:Ctrl+S; PhysicalKey:W; MouseButton:Left; JoypadButton:A@1; JoypadMotion:LeftX-");

	const TypedArray<InputEvent> read = InputMapSettingsServer::text_to_events(text);
	REQUIRE(read.size() == 5);
	const Ref<InputEventJoypadButton> pad = read[3];
	REQUIRE(pad.is_valid());
	CHECK(pad->get_device() == 1);
	CHECK(InputMapSettingsServer::events_to_text(read) == text);

	// Invalid items are skipped.
	CHECK(InputMapSettingsServer::text_to_events("Key:Ctrl+S; NotAType:X; ;").size() == 1);
	CHECK(InputMapSettingsServer::text_to_events("").is_empty());
}

TEST_CASE("[SceneTree][InputMapSettingsServer] The file holds one text row per action; version 1 files still load") {
	TestSetup setup;
	{
		// JSON: the storage type and its rows, not InputActions with InputEvent sub-resources.
		const String path = temp_file("input_map_rows.json");
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		CHECK(server->rebind(JUMP, InputAction::text_to_event("Key:Ctrl+J")) == OK);
		CHECK(server->save() == OK);
		memdelete(server);
		const String text = FileAccess::get_file_as_string(path);
		CHECK(text.contains("InputMapSettingsRows"));
		CHECK(text.contains("\"bindings\""));
		CHECK(text.contains("Key:Ctrl+J; JoypadButton:A"));
		CHECK_FALSE(text.contains("\"events\""));
	}
	for (const String &extension : { String("json"), String("csv") }) {
		// A file of the first layout (InputActions with InputAction.bindings, version 1).
		const String path = temp_file("input_map_v1." + extension);
		String text;
		if (extension == "json") {
			text = vformat("{\"$type\": \"InputActions\", \"$format_version\": 1, \"metadata/settings_version\": 1, \"actions\": [{\"action\": \"%s\", \"deadzone\": 0.4, \"bindings\": \"Key:K; JoypadButton:B@2\"}, {\"action\": \"%s\", \"deadzone\": 0.2, \"bindings\": \"MouseButton:Right\"}]}", JUMP, FIRE);
		} else {
			text = vformat("action,deadzone,bindings\n%s,0.4,Key:K; JoypadButton:B@2\n%s,0.2,MouseButton:Right\n", JUMP, FIRE);
		}
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string(text);
		file.unref();

		load_project_actions();
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		CHECK_MESSAGE(server->load() == OK, extension);
		server->apply();
		CHECK_MESSAGE(server->get_action_deadzone(JUMP) == doctest::Approx(0.4), extension);
		CHECK_MESSAGE(InputMapSettingsServer::events_to_text(server->get_events(JUMP)) == "Key:K; JoypadButton:B@2", extension);
		CHECK_MESSAGE(is_action(make_pressed_key(Key::K), JUMP), extension);
		CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
		CHECK_MESSAGE(is_action(make_mouse_button(MouseButton::RIGHT), FIRE), extension);
		memdelete(server);
	}
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Defaults come from InputMap; custom actions only by default") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	CHECK(server->get_save_mode() == InputMapSettingsServer::SAVE_MODE_CUSTOM_ONLY);
	const PackedStringArray actions = server->get_action_names();
	CHECK(actions.has(String(JUMP)));
	CHECK(actions.has(String(FIRE)));
	for (const String &action_name : actions) {
		CHECK_MESSAGE(InputMap::get_singleton()->is_custom_input_action(action_name), action_name);
	}
	CHECK_FALSE(actions.has("ui_accept")); // Built-in actions are left out by default.

	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 1);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_GAMEPAD) == 1);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_MOUSE) == 0);
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD)) == "Key:Space");
	CHECK(server->get_events(JUMP).size() == 2);
	CHECK(server->get_action_deadzone(JUMP) == doctest::Approx(0.3));
	CHECK(server->get_action_deadzone(FIRE) == doctest::Approx(0.2));

	// A copy of the action as a resource: changing it does not change the settings.
	const Ref<InputAction> copy = server->get_input_action(JUMP);
	REQUIRE(copy.is_valid());
	CHECK(copy->get_action() == StringName(JUMP));
	CHECK(copy->get_deadzone() == doctest::Approx(0.3));
	CHECK(copy->get_event_count() == 2);
	CHECK(InputMapSettingsServer::events_to_text(copy->get_events()) == "Key:Space; JoypadButton:A");
	copy->clear_events();
	CHECK(server->get_events(JUMP).size() == 2);
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] SAVE_MODE_ALL adds the built-in actions") {
	TestSetup setup;
	BuiltinSetup builtin(InputMapSettingsServer::SAVE_MODE_ALL);
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	CHECK(server->get_save_mode() == InputMapSettingsServer::SAVE_MODE_ALL);
	CHECK(server->has_action("ui_accept"));
	CHECK(server->has_action(JUMP));
	CHECK(server->has_action(FIRE));
	CHECK(server->get_input_action("ui_accept").is_valid());
	CHECK(server->get_events("ui_accept").size() >= 1);
	CHECK(server->rebind("ui_accept", make_pressed_key(Key::Q)) == OK);
	CHECK(is_action(make_pressed_key(Key::Q), "ui_accept"));
	server->reset_action("ui_accept"); // Gives a real ui_accept of the engine back its events.
	CHECK_FALSE(is_action(make_pressed_key(Key::Q), "ui_accept"));
	memdelete(server);

	// The custom-only default leaves the built-in action out and refuses to change it.
	ProjectSettings::get_singleton()->set_setting(SETTING_SAVE_MODE, int(InputMapSettingsServer::SAVE_MODE_CUSTOM_ONLY));
	InputMapSettingsServer *custom_server = memnew(InputMapSettingsServer);
	CHECK(custom_server->get_save_mode() == InputMapSettingsServer::SAVE_MODE_CUSTOM_ONLY);
	CHECK_FALSE(custom_server->has_action("ui_accept"));
	CHECK(custom_server->has_action(JUMP));
	ERR_PRINT_OFF;
	CHECK(custom_server->rebind("ui_accept", make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	ERR_PRINT_ON;
	memdelete(custom_server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Rebinding reaches InputMap at once; slots replace or append") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);

	// Slot 0 replaced, slot 1 added; the device follows from the event.
	CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::UP), 1) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 2);
	CHECK(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(is_action(make_pressed_key(Key::UP), JUMP));
	CHECK_FALSE(is_action(make_pressed_key(Key::SPACE), JUMP));
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));
	// The pad id of the pressed button is not kept: the binding is for all pads.
	const Ref<InputEventJoypadButton> pad_b = InputEventJoypadButton::create_reference(JoyButton::B, 2);
	CHECK(server->rebind(JUMP, pad_b) == OK);
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::B, 0), JUMP));
	CHECK_FALSE(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_GAMEPAD) == 1);

	// A slot behind the last one appends.
	CHECK(server->rebind(JUMP, make_pressed_key(Key::DOWN), 5) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 3);
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 2)) == "PhysicalKey:Down");
	CHECK(is_action(make_pressed_key(Key::DOWN), JUMP));

	server->set_action_deadzone(JUMP, 0.6f);
	CHECK(InputMap::get_singleton()->action_get_deadzone(JUMP) == doctest::Approx(0.6));
	CHECK(server->get_action_deadzone(JUMP) == doctest::Approx(0.6));
	server->set_action_deadzone(JUMP, 2.0f);
	CHECK(server->get_action_deadzone(JUMP) == doctest::Approx(1.0));

	// Errors.
	ERR_PRINT_OFF;
	CHECK(server->rebind("kb_test_no_such_action", make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(server->rebind(JUMP, touch) == ERR_INVALID_PARAMETER);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::Q), -1) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] clear_event removes the event, later slots move up, and it survives apply") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::UP), 1) == OK);
	CHECK(server->rebind(JUMP, make_pressed_key(Key::DOWN), 2) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 3);

	// Slot 0 is cleared: slot 1 becomes slot 0, slot 2 becomes slot 1.
	server->clear_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 0);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 2);
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 0)) == "PhysicalKey:Up");
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 1)) == "PhysicalKey:Down");
	CHECK(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 2).is_null());
	CHECK_FALSE(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(is_action(make_pressed_key(Key::UP), JUMP));
	CHECK(is_action(make_pressed_key(Key::DOWN), JUMP));
	// The gamepad slot is not touched.
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));

	// Clearing an empty slot does nothing.
	server->clear_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 7);
	server->clear_event(JUMP, InputAction::INPUT_DEVICE_MOUSE, 0);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 2);

	// The only event of an action is cleared: the project binding does not come back when the settings are applied.
	server->clear_event(FIRE, InputAction::INPUT_DEVICE_MOUSE, 0);
	CHECK(server->has_action(FIRE));
	CHECK(server->get_input_action(FIRE)->get_event_count() == 0);
	CHECK_FALSE(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	server->apply();
	CHECK_FALSE(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	CHECK(server->get_slot_count(FIRE, InputAction::INPUT_DEVICE_MOUSE) == 0);
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] add_event and remove_event") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);

	CHECK(server->add_event(JUMP, make_pressed_key(Key::Q)) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 2);
	CHECK(is_action(make_pressed_key(Key::Q), JUMP));
	CHECK(is_action(make_pressed_key(Key::SPACE), JUMP)); // The project binding stays.
	// The same event twice is added once.
	CHECK(server->add_event(JUMP, make_pressed_key(Key::Q)) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 2);
	CHECK(server->add_event(JUMP, make_mouse_button(MouseButton::RIGHT)) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_MOUSE) == 1);
	CHECK(is_action(make_mouse_button(MouseButton::RIGHT), JUMP));

	CHECK(server->remove_event(JUMP, make_pressed_key(Key::Q)) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 1);
	CHECK_FALSE(is_action(make_pressed_key(Key::Q), JUMP));
	// A project binding can be removed by the event of its text form.
	CHECK(server->remove_event(JUMP, make_key(Key::SPACE, false)) == OK);
	CHECK(server->get_slot_count(JUMP, InputAction::INPUT_DEVICE_KEYBOARD) == 0);
	CHECK_FALSE(is_action(make_pressed_key(Key::SPACE), JUMP));

	ERR_PRINT_OFF;
	CHECK(server->remove_event(JUMP, make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	CHECK(server->add_event("kb_test_no_such_action", make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	CHECK(server->remove_event("kb_test_no_such_action", make_pressed_key(Key::Q)) == ERR_DOES_NOT_EXIST);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(server->add_event(JUMP, touch) == ERR_INVALID_PARAMETER);
	CHECK(server->remove_event(JUMP, touch) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Conflicts and reset to the project bindings") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);

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

	// reset_action() restores one action: its events and its deadzone.
	CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
	server->set_action_deadzone(JUMP, 0.9f);
	server->reset_action(JUMP);
	CHECK(is_action(make_pressed_key(Key::SPACE), JUMP));
	CHECK_FALSE(is_action(make_pressed_key(Key::W), JUMP));
	CHECK(InputMap::get_singleton()->action_get_deadzone(JUMP) == doctest::Approx(0.3));
	CHECK(is_action(make_pressed_key(Key::SPACE), FIRE));

	// reset_all() restores every action.
	server->reset_all();
	CHECK_FALSE(is_action(make_pressed_key(Key::SPACE), FIRE));
	CHECK(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	CHECK(SettingsServer::values_equal(server->get_value("actions"), server->get_default_value("actions")));
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Rebound, saved, restarted: the new key works, the old one does not (every text format)") {
	TestSetup setup;
	for (const String &extension : { String("ini"), String("json"), String("csv"), String("xml"), String("toml"), String("yaml") }) {
		if (!ResourceTextSerializer::has_format_implementation(ResourceTextSerializer::get_format_for_extension(extension))) {
			continue; // The module of the format is disabled in this build.
		}
		const String path = temp_file("input_map." + extension);
		load_project_actions();
		{
			InputMapSettingsServer *server = memnew(InputMapSettingsServer);
			server->set_file_path(path);
			CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
			CHECK(server->rebind(JUMP, InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f)) == OK);
			CHECK(server->rebind(FIRE, InputAction::text_to_event("MouseButton:Ctrl+Right")) == OK);
			server->clear_event(FIRE, InputAction::INPUT_DEVICE_MOUSE, 1); // No second slot: nothing happens.
			server->set_action_deadzone(JUMP, 0.45f);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path);
		CHECK_MESSAGE(text.contains("PhysicalKey:W"), extension);
		CHECK_MESSAGE(text.contains("JoypadMotion:LeftY-"), extension);
		CHECK_MESSAGE(text.contains("MouseButton:Ctrl+Right"), extension);
		// The events are one text value per action, never sub-resources.
		CHECK_FALSE_MESSAGE(text.contains("InputEvent"), extension);
		if (extension == "csv") {
			// One row per action with the bindings of the action in one cell.
			int jump_rows = 0;
			int fire_rows = 0;
			for (const String &line : text.split("\n")) {
				if (line.contains(JUMP)) {
					jump_rows++;
					CHECK_MESSAGE(line.contains("Key:"), line);
					CHECK_MESSAGE(line.contains("JoypadMotion:LeftY-"), line);
				}
				if (line.contains(FIRE)) {
					fire_rows++;
					CHECK_MESSAGE(line.contains("MouseButton:Ctrl+Right"), line);
				}
			}
			CHECK(jump_rows == 1);
			CHECK(fire_rows == 1);
		}

		// "Restart": InputMap is back at the project settings.
		load_project_actions();
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK_MESSAGE(is_action(make_pressed_key(Key::W), JUMP), extension);
		CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
		const Ref<InputEventJoypadMotion> stick_up = InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f, 0);
		CHECK_MESSAGE(is_action(stick_up, JUMP), extension);
		CHECK_FALSE(is_action(InputEventJoypadButton::create_reference(JoyButton::A, 0), JUMP));
		CHECK_MESSAGE(InputMap::get_singleton()->action_get_deadzone(JUMP) == doctest::Approx(0.45), extension);
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
		CHECK(InputMap::get_singleton()->action_get_deadzone(JUMP) == doctest::Approx(0.3));
		memdelete(server);
	}
}

TEST_CASE("[SceneTree][InputMapSettingsServer] A cleared action stays cleared after a restart (INI, JSON, CSV)") {
	TestSetup setup;
	for (const String &extension : { String("ini"), String("json"), String("csv") }) {
		const String path = temp_file("input_map_cleared." + extension);
		load_project_actions();
		{
			InputMapSettingsServer *server = memnew(InputMapSettingsServer);
			server->set_file_path(path);
			server->clear_event(FIRE, InputAction::INPUT_DEVICE_MOUSE, 0);
			CHECK(server->save() == OK);
			memdelete(server);
		}

		load_project_actions();
		CHECK(is_action(make_mouse_button(MouseButton::LEFT), FIRE)); // The project binding is back until the file is applied.
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK_FALSE_MESSAGE(is_action(make_mouse_button(MouseButton::LEFT), FIRE), extension);
		CHECK_MESSAGE(server->get_slot_count(FIRE, InputAction::INPUT_DEVICE_MOUSE) == 0, extension);
		CHECK_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension); // Other actions are untouched.
		memdelete(server);
	}
}

TEST_CASE("[SceneTree][InputMapSettingsServer] A saved action that no longer exists is skipped; new project actions keep their bindings") {
	TestSetup setup;
	const String path = temp_file("input_map_old.ini");
	{
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		Ref<InputAction> gone;
		gone.instantiate();
		gone->set_action("kb_test_removed");
		gone->add_event(InputAction::text_to_event("Key:G"));
		ERR_PRINT_OFF; // Applying warns once that the project has no such action.
		server->add_entry(gone);
		ERR_PRINT_ON;
		CHECK(server->rebind(JUMP, make_pressed_key(Key::W)) == OK);
		// The file of an older version has no entry for fire.
		for (int i = server->get_entry_count() - 1; i >= 0; i--) {
			const Ref<InputAction> row = server->get_entry(i);
			if (row->get_action() == StringName(FIRE)) {
				server->remove_entry(i);
			}
		}
		CHECK(server->save() == OK);
		memdelete(server);
	}
	CHECK(FileAccess::get_file_as_string(path).contains("kb_test_removed"));

	load_project_actions();
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
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

TEST_CASE("[SceneTree][InputMapSettingsServer] Other event types of an action are kept") {
	TestSetup setup;
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	touch->set_index(0);
	InputMap::get_singleton()->action_add_event(FIRE, touch);
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	CHECK(server->rebind(FIRE, make_pressed_key(Key::F)) == OK);
	CHECK(is_action(make_pressed_key(Key::F), FIRE));
	bool has_touch = false;
	for (const Ref<InputEvent> &event : *InputMap::get_singleton()->action_get_events(FIRE)) {
		has_touch = has_touch || event == touch;
	}
	CHECK(has_touch);
	memdelete(server);
}

} // namespace TestInputMapSettingsServer
