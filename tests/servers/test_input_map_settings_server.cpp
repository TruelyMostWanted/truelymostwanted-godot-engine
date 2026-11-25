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
#include "servers/settings/input_map_settings_records.h"
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
	// Round trip of every kind: `[Device<N>:]<Type>:[<Modifiers>+]<Input>`.
	for (const String &text : { String("Keyboard:W"), String("KeyboardLabel:W"), String("KeyboardPrinted:A"), String("KeyboardLabel:Ctrl+S"), String("KeyboardPrinted:Alt+Shift+A"),
				 String("KeyboardLabel:F5"), String("KeyboardLabel:Kp Add"), String("Mouse:Left"), String("Mouse:WheelUp"), String("Mouse:Ctrl+WheelUp"), String("Mouse:Extra2"),
				 String("Gamepad:A"), String("Gamepad:LeftShoulder"), String("Gamepad:LeftStickPress"), String("Gamepad:RightStickPress"), String("Gamepad:30"),
				 String("GamepadAxis:RightTrigger"), String("GamepadAxis:7-"),
				 // Key names, never characters: `:` and `+` stay separators.
				 String("Keyboard:Colon"), String("Keyboard:Shift+Semicolon"), String("Keyboard:Ctrl+Plus"), String("Keyboard:Minus"), String("Keyboard:At"),
				 // Modifiers in the fixed order CmdOrCtrl, Ctrl, Alt, Shift, Meta; a modifier alone is the key.
				 String("Keyboard:Ctrl+Shift+S"), String("Keyboard:CmdOrCtrl+Z"), String("Mouse:Ctrl+Left"), String("Keyboard:Shift"), String("Keyboard:Ctrl+Alt+Shift+Meta+K") }) {
		const Ref<InputEvent> event = InputAction::text_to_event(text);
		REQUIRE_MESSAGE(event.is_valid(), text);
		CHECK_MESSAGE(InputAction::event_to_text(event) == text, text);
		CHECK_MESSAGE(event->get_device() == -1, text);
	}

	// All ten axis directions (Godot's Y+ points down; triggers only have the positive direction).
	struct AxisCase {
		const char *text;
		JoyAxis axis;
		float value;
	};
	const AxisCase axes[] = {
		{ "GamepadAxis:LeftStickLeft", JoyAxis::LEFT_X, -1.0f },
		{ "GamepadAxis:LeftStickRight", JoyAxis::LEFT_X, 1.0f },
		{ "GamepadAxis:LeftStickUp", JoyAxis::LEFT_Y, -1.0f },
		{ "GamepadAxis:LeftStickDown", JoyAxis::LEFT_Y, 1.0f },
		{ "GamepadAxis:RightStickLeft", JoyAxis::RIGHT_X, -1.0f },
		{ "GamepadAxis:RightStickRight", JoyAxis::RIGHT_X, 1.0f },
		{ "GamepadAxis:RightStickUp", JoyAxis::RIGHT_Y, -1.0f },
		{ "GamepadAxis:RightStickDown", JoyAxis::RIGHT_Y, 1.0f },
		{ "GamepadAxis:LeftTrigger", JoyAxis::TRIGGER_LEFT, 1.0f },
		{ "GamepadAxis:RightTrigger", JoyAxis::TRIGGER_RIGHT, 1.0f },
	};
	for (const AxisCase &axis_case : axes) {
		const Ref<InputEventJoypadMotion> motion = InputAction::text_to_event(axis_case.text);
		REQUIRE_MESSAGE(motion.is_valid(), axis_case.text);
		CHECK_MESSAGE(motion->get_axis() == axis_case.axis, axis_case.text);
		CHECK_MESSAGE(motion->get_axis_value() == doctest::Approx(axis_case.value), axis_case.text);
		CHECK_MESSAGE(InputAction::event_to_text(motion) == axis_case.text, axis_case.text);
	}
	// A stick click is a button, a stick direction an axis.
	CHECK(Ref<InputEventJoypadButton>(InputAction::text_to_event("Gamepad:LeftStickPress")).is_valid());
	CHECK(InputAction::text_to_event("Gamepad:LeftStickLeft").is_null());
	CHECK(InputAction::text_to_event("GamepadAxis:LeftStickPress").is_null());

	// Event fields.
	const Ref<InputEventKey> physical = InputAction::text_to_event("Keyboard:W");
	CHECK(physical->get_physical_keycode() == Key::W);
	CHECK(physical->get_keycode() == Key::NONE);
	const Ref<InputEventKey> label = InputAction::text_to_event("KeyboardLabel:W");
	CHECK(label->get_keycode() == Key::W);
	CHECK(label->get_physical_keycode() == Key::NONE);
	const Ref<InputEventKey> printed = InputAction::text_to_event("KeyboardPrinted:A");
	CHECK(printed->get_key_label() == Key::A);
	CHECK(printed->get_keycode() == Key::NONE);
	const Ref<InputEventKey> colon = InputAction::text_to_event("Keyboard:Colon");
	CHECK(colon->get_physical_keycode() == Key::COLON);
	const Ref<InputEventKey> shift_semicolon = InputAction::text_to_event("Keyboard:Shift+Semicolon");
	CHECK(shift_semicolon->get_physical_keycode() == Key::SEMICOLON);
	CHECK(shift_semicolon->is_shift_pressed());
	const Ref<InputEventKey> ctrl_plus = InputAction::text_to_event("Keyboard:Ctrl+Plus");
	CHECK(ctrl_plus->get_physical_keycode() == Key::PLUS);
	CHECK(ctrl_plus->is_ctrl_pressed());
	CHECK(Ref<InputEventKey>(InputAction::text_to_event("Keyboard:At"))->get_physical_keycode() == Key::AT);
	const Ref<InputEventKey> ctrl_shift_s = InputAction::text_to_event("Keyboard:Ctrl+Shift+S");
	CHECK(ctrl_shift_s->is_ctrl_pressed());
	CHECK(ctrl_shift_s->is_shift_pressed());
	CHECK_FALSE(ctrl_shift_s->is_alt_pressed());
	// Any order is read; the fixed order is written.
	const Ref<InputEvent> shift_ctrl_s = InputAction::text_to_event("Keyboard:Shift+Ctrl+S");
	REQUIRE(shift_ctrl_s.is_valid());
	CHECK(InputAction::event_to_text(shift_ctrl_s) == "Keyboard:Ctrl+Shift+S");
	CHECK(shift_ctrl_s->is_match(ctrl_shift_s, true));
	const Ref<InputEventKey> undo = InputAction::text_to_event("Keyboard:CmdOrCtrl+Z");
	CHECK(undo->is_command_or_control_autoremap());
	const Ref<InputEventMouseButton> ctrl_left = InputAction::text_to_event("Mouse:Ctrl+Left");
	CHECK(ctrl_left->get_button_index() == MouseButton::LEFT);
	CHECK(ctrl_left->is_ctrl_pressed());
	CHECK(Ref<InputEventMouseButton>(InputAction::text_to_event("Mouse:WheelUp"))->get_button_index() == MouseButton::WHEEL_UP);
	CHECK(Ref<InputEventJoypadButton>(InputAction::text_to_event("Gamepad:LeftShoulder"))->get_button_index() == JoyButton::LEFT_SHOULDER);
	const Ref<InputEventKey> shift_alone = InputAction::text_to_event("Keyboard:Shift");
	CHECK(shift_alone->get_physical_keycode() == Key::SHIFT);
	CHECK_FALSE(shift_alone->is_shift_pressed());

	// Device prefix: only that pad. Without a prefix (and with `Any:`) all pads; the old `@N` suffix is still read.
	const Ref<InputEventJoypadButton> pad_a = InputAction::text_to_event("Device3:Gamepad:A");
	REQUIRE(pad_a.is_valid());
	CHECK(pad_a->get_device() == 3);
	CHECK(pad_a->get_button_index() == JoyButton::A);
	CHECK(InputAction::event_to_text(pad_a) == "Device3:Gamepad:A");
	const Ref<InputEventJoypadMotion> pad_axis = InputAction::text_to_event("Device3:GamepadAxis:LeftStickLeft");
	REQUIRE(pad_axis.is_valid());
	CHECK(pad_axis->get_device() == 3);
	CHECK(InputAction::event_to_text(pad_axis) == "Device3:GamepadAxis:LeftStickLeft");
	CHECK(InputAction::text_to_event("Device3:GamepadAxis:LeftX-")->get_device() == 3);
	CHECK(InputAction::event_to_text(InputAction::text_to_event("Device3:GamepadAxis:LeftX-")) == "Device3:GamepadAxis:LeftStickLeft");
	CHECK(InputAction::text_to_event("Any:Gamepad:A")->get_device() == -1);
	CHECK(InputAction::event_to_text(InputAction::text_to_event("Any:Gamepad:A")) == "Gamepad:A");
	CHECK(InputAction::text_to_event("Gamepad:A@3")->get_device() == 3);
	CHECK(InputAction::event_to_text(InputAction::text_to_event("Gamepad:A@3")) == "Device3:Gamepad:A");
	CHECK(InputAction::event_to_text(InputAction::text_to_event("Gamepad:A@Any")) == "Gamepad:A");
	CHECK(InputAction::event_to_text(InputEventJoypadButton::create_reference(JoyButton::B, 0)) == "Device0:Gamepad:B");
	// Keys and mouse buttons are bound for all devices: the prefix is read, not kept.
	CHECK(InputAction::text_to_event("Device3:Keyboard:W")->get_device() == -1);

	// Old names are still read; only the new names are written.
	const struct {
		const char *old_text;
		const char *new_text;
	} old_names[] = {
		{ "PhysicalKey:W", "Keyboard:W" },
		{ "Key:Ctrl+S", "KeyboardLabel:Ctrl+S" },
		{ "KeyLabel:A", "KeyboardPrinted:A" },
		{ "MouseButton:Right", "Mouse:Right" },
		{ "JoypadButton:A", "Gamepad:A" },
		{ "JoypadMotion:LeftY-", "GamepadAxis:LeftStickUp" },
		{ "GamepadAxis:LeftX-", "GamepadAxis:LeftStickLeft" },
		{ "GamepadAxis:LeftY+", "GamepadAxis:LeftStickDown" },
		{ "GamepadAxis:TriggerRight+", "GamepadAxis:RightTrigger" },
		{ "GamepadAxis:TriggerLeft+", "GamepadAxis:LeftTrigger" },
		{ "Gamepad:LeftStick", "Gamepad:LeftStickPress" },
		{ "Gamepad:RightStick", "Gamepad:RightStickPress" },
	};
	for (const auto &names : old_names) {
		const Ref<InputEvent> event = InputAction::text_to_event(names.old_text);
		REQUIRE_MESSAGE(event.is_valid(), names.old_text);
		CHECK_MESSAGE(InputAction::event_to_text(event) == names.new_text, names.old_text);
	}

	// A pressed key is stored by its physical code; a modifier key alone has no modifier flags.
	CHECK(InputAction::event_to_text(make_pressed_key(Key::W)) == "Keyboard:W");
	Ref<InputEventKey> shift = make_pressed_key(Key::SHIFT);
	shift->set_shift_pressed(true);
	CHECK(InputAction::event_to_text(shift) == "Keyboard:Shift");

	// Invalid texts and event types that can't be bound.
	for (const String &text : { String(""), String("Key"), String("Keyboard:"), String("Keyboard:Ctrl+"), String("KeyboardLabel:NoSuchKey"), String("KeyboardLabel:Hyper+S"),
				 String("MouseMotion:Left"), String("NotAType:A"), String("Mouse:Sideways"), String("Gamepad:Z"), String("Gamepad:Ctrl+A"), String("Device:Gamepad:A"),
				 String("DeviceX:Gamepad:A"), String("Device3:"), String("GamepadAxis:LeftX"), String("GamepadAxis:LeftStickX-"), String("GamepadAxis:LeftTrigger+"),
				 String("GamepadAxis:Nowhere+") }) {
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
	CHECK(label_server->get_event_label(InputAction::text_to_event("KeyboardLabel:Ctrl+S")) == "Ctrl+S");
	CHECK(label_server->get_event_label(InputAction::text_to_event("Mouse:Right")) == "Right Mouse Button");
	CHECK(label_server->get_event_label(InputAction::text_to_event("Gamepad:A")) == "Joypad A");
	CHECK(label_server->get_event_label(InputAction::text_to_event("GamepadAxis:LeftStickRight")) == "Left Stick Right");
	CHECK(label_server->get_event_label(InputAction::text_to_event("Keyboard:W")) == "W");
}

TEST_CASE("[InputMapSettingsServer] Text form of the events of an action") {
	TypedArray<InputEvent> events;
	events.push_back(InputAction::text_to_event("KeyboardLabel:Ctrl+S"));
	events.push_back(InputAction::text_to_event("Keyboard:W"));
	events.push_back(InputAction::text_to_event("Mouse:Left"));
	events.push_back(InputEventJoypadButton::create_reference(JoyButton::A, 1));
	events.push_back(InputAction::text_to_event("GamepadAxis:LeftStickLeft"));
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	events.push_back(touch); // No text form: skipped.
	const String text = InputMapSettingsServer::events_to_text(events);
	CHECK(text == "KeyboardLabel:Ctrl+S; Keyboard:W; Mouse:Left; Device1:Gamepad:A; GamepadAxis:LeftStickLeft");

	const TypedArray<InputEvent> read = InputMapSettingsServer::text_to_events(text);
	REQUIRE(read.size() == 5);
	const Ref<InputEventJoypadButton> pad = read[3];
	REQUIRE(pad.is_valid());
	CHECK(pad->get_device() == 1);
	CHECK(InputMapSettingsServer::events_to_text(read) == text);
	// Old forms are read, the new ones written.
	CHECK(InputMapSettingsServer::events_to_text(InputMapSettingsServer::text_to_events("JoypadButton:A@1; GamepadAxis:LeftX-; Gamepad:B@Any")) == "Device1:Gamepad:A; GamepadAxis:LeftStickLeft; Gamepad:B");

	// Invalid items are skipped.
	CHECK(InputMapSettingsServer::text_to_events("Keyboard:Ctrl+S; NotAType:X; Keyboard:NoSuchKey; ;").size() == 1);
	CHECK(InputMapSettingsServer::text_to_events("").is_empty());
}

TEST_CASE("[InputMapSettingsServer] Records: physical, label or printed key, device id, axis names, modifiers") {
	// Physical key: the position, a label key: the layout.
	Ref<InputMapSettingsEvent> physical = InputMapSettingsEvent::from_event(make_key(Key::W, true));
	REQUIRE(physical.is_valid());
	CHECK(physical->get_device() == InputMapSettingsEvent::DEVICE_KEYBOARD);
	CHECK(physical->get_input() == "W");
	CHECK(physical->get_key_mode() == InputMapSettingsEvent::KEY_MODE_PHYSICAL);
	CHECK(physical->to_text() == "Keyboard:W");
	const Ref<InputEventKey> physical_event = physical->to_event();
	REQUIRE(physical_event.is_valid());
	CHECK(physical_event->get_physical_keycode() == Key::W);
	CHECK(physical_event->get_keycode() == Key::NONE);

	Ref<InputMapSettingsEvent> label = InputMapSettingsEvent::from_event(make_key(Key::E, false));
	REQUIRE(label.is_valid());
	CHECK(label->get_key_mode() == InputMapSettingsEvent::KEY_MODE_LABEL);
	CHECK(label->to_text() == "KeyboardLabel:E");
	const Ref<InputEventKey> label_event = label->to_event();
	REQUIRE(label_event.is_valid());
	CHECK(label_event->get_keycode() == Key::E);
	CHECK(label_event->get_physical_keycode() == Key::NONE);

	Ref<InputMapSettingsEvent> printed = InputMapSettingsEvent::from_text("KeyboardPrinted:A");
	REQUIRE(printed.is_valid());
	CHECK(printed->get_key_mode() == InputMapSettingsEvent::KEY_MODE_PRINTED);
	CHECK(printed->to_text() == "KeyboardPrinted:A");
	CHECK(Ref<InputEventKey>(printed->to_event())->get_key_label() == Key::A);

	// Modifiers: keyboard and mouse.
	Ref<InputMapSettingsEvent> ctrl_shift = InputMapSettingsEvent::from_text("Keyboard:Shift+Ctrl+S");
	REQUIRE(ctrl_shift.is_valid());
	CHECK(ctrl_shift->get_modifiers() == "Ctrl+Shift");
	CHECK(ctrl_shift->get_input() == "S");
	const Ref<InputEventKey> ctrl_shift_event = ctrl_shift->to_event();
	REQUIRE(ctrl_shift_event.is_valid());
	CHECK(ctrl_shift_event->is_ctrl_pressed());
	CHECK(ctrl_shift_event->is_shift_pressed());
	CHECK_FALSE(ctrl_shift_event->is_alt_pressed());
	const Ref<InputMapSettingsEvent> ctrl_plus = InputMapSettingsEvent::from_text("Keyboard:Ctrl+Plus");
	REQUIRE(ctrl_plus.is_valid());
	CHECK(ctrl_plus->get_modifiers() == "Ctrl");
	CHECK(ctrl_plus->get_input() == "Plus");
	const Ref<InputMapSettingsEvent> mouse = InputMapSettingsEvent::from_text("Mouse:Ctrl+WheelUp");
	REQUIRE(mouse.is_valid());
	CHECK(mouse->get_device() == InputMapSettingsEvent::DEVICE_MOUSE);
	CHECK(mouse->get_modifiers() == "Ctrl");
	CHECK(mouse->get_input() == "WheelUp");

	// Gamepad: Any or one pad; an axis has its direction in the name.
	const Ref<InputMapSettingsEvent> any_button = InputMapSettingsEvent::from_text("Gamepad:A");
	REQUIRE(any_button.is_valid());
	CHECK(any_button->get_device_id() == "Any");
	CHECK(any_button->get_device_number() == -1);
	CHECK(any_button->to_event()->get_device() == -1);
	CHECK(any_button->to_text() == "Gamepad:A");
	const Ref<InputMapSettingsEvent> pad_button = InputMapSettingsEvent::from_text("Device2:Gamepad:B");
	REQUIRE(pad_button.is_valid());
	CHECK(pad_button->get_device_id() == "2");
	CHECK(pad_button->to_event()->get_device() == 2);
	CHECK(pad_button->to_text() == "Device2:Gamepad:B");
	const Ref<InputMapSettingsEvent> axis = InputMapSettingsEvent::from_text("GamepadAxis:LeftStickLeft");
	REQUIRE(axis.is_valid());
	CHECK(axis->get_device() == InputMapSettingsEvent::DEVICE_GAMEPAD);
	CHECK(axis->get_input() == "LeftStickLeft");
	const Ref<InputEventJoypadMotion> axis_event = axis->to_event();
	REQUIRE(axis_event.is_valid());
	CHECK(axis_event->get_axis() == JoyAxis::LEFT_X);
	CHECK(axis_event->get_axis_value() < 0.0f);
	CHECK(axis->to_text() == "GamepadAxis:LeftStickLeft");
	const Ref<InputMapSettingsEvent> trigger = InputMapSettingsEvent::from_event(InputEventJoypadMotion::create_reference(JoyAxis::TRIGGER_RIGHT, 1.0f, 1));
	REQUIRE(trigger.is_valid());
	CHECK(trigger->get_input() == "RightTrigger");
	CHECK(trigger->get_device_id() == "1");
	CHECK(trigger->to_text() == "Device1:GamepadAxis:RightTrigger");
	// A device-specific axis keeps its pad.
	const Ref<InputMapSettingsEvent> pad_axis = InputMapSettingsEvent::from_text("Device3:GamepadAxis:LeftStickLeft");
	REQUIRE(pad_axis.is_valid());
	CHECK(pad_axis->get_device_id() == "3");
	CHECK(pad_axis->to_event()->get_device() == 3);

	// Old names (version 2 files) are still read; an old axis name in `input` too.
	CHECK(InputMapSettingsEvent::from_text("Key:Ctrl+S")->to_text() == "KeyboardLabel:Ctrl+S");
	CHECK(InputMapSettingsEvent::from_text("PhysicalKey:W")->to_text() == "Keyboard:W");
	CHECK(InputMapSettingsEvent::from_text("MouseButton:Left")->to_text() == "Mouse:Left");
	CHECK(InputMapSettingsEvent::from_text("Gamepad:A@3")->to_text() == "Device3:Gamepad:A");
	CHECK(InputMapSettingsEvent::from_text("JoypadMotion:LeftY-")->to_text() == "GamepadAxis:LeftStickUp");
	Ref<InputMapSettingsEvent> old_axis;
	old_axis.instantiate();
	old_axis->set_device(InputMapSettingsEvent::DEVICE_GAMEPAD);
	old_axis->set_input("LeftX-");
	CHECK(old_axis->to_text() == "GamepadAxis:LeftStickLeft");

	// Not valid.
	for (const String &text : { String(""), String("Keyboard"), String("Keyboard:"), String("Keyboard:NoSuchKey"), String("Mouse:Sideways"), String("Gamepad:Z"), String("Gamepad:Ctrl+A"), String("GamepadAxis:LeftX"), String("GamepadAxis:Nowhere+"), String("Touch:1") }) {
		CHECK_MESSAGE(InputMapSettingsEvent::from_text(text).is_null(), text);
	}
	Ref<InputMapSettingsEvent> gamepad_modifiers;
	gamepad_modifiers.instantiate();
	gamepad_modifiers->set_device(InputMapSettingsEvent::DEVICE_GAMEPAD);
	gamepad_modifiers->set_input("A");
	gamepad_modifiers->set_modifiers("Ctrl");
	CHECK(gamepad_modifiers->to_event().is_null());
	// Events without a record.
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(InputMapSettingsEvent::from_event(touch).is_null());
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Every format writes the fields of an event; round trip in all six formats") {
	TestSetup setup;
	for (const String &extension : { String("json"), String("ini"), String("csv"), String("xml"), String("toml"), String("yaml") }) {
		if (!ResourceTextSerializer::has_format_implementation(ResourceTextSerializer::get_format_for_extension(extension))) {
			continue; // The module of the format is disabled in this build.
		}
		const String path = temp_file("input_map_fields." + extension);
		load_project_actions();
		{
			InputMapSettingsServer *server = memnew(InputMapSettingsServer);
			server->set_file_path(path);
			// jump: physical W, left stick left, label E with Ctrl+Shift, button B; fire: Ctrl+right mouse button, no keyboard.
			CHECK(server->rebind(JUMP, make_key(Key::W, true)) == OK);
			CHECK(server->add_event(JUMP, InputAction::text_to_event("KeyboardLabel:Ctrl+Shift+E")) == OK);
			CHECK(server->rebind(JUMP, InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f)) == OK);
			CHECK(server->add_event(JUMP, InputEventJoypadButton::create_reference(JoyButton::B)) == OK);
			CHECK(server->rebind(FIRE, InputAction::text_to_event("Mouse:Ctrl+Right")) == OK);
			server->set_action_deadzone(JUMP, 0.45f);
			CHECK(server->save() == OK);
			memdelete(server);
		}

		const String text = FileAccess::get_file_as_string(path);
		CHECK_FALSE_MESSAGE(text.contains("InputEvent"), extension);
		CHECK_FALSE_MESSAGE(text.contains("bindings"), extension);
		CHECK_MESSAGE(text.contains("Keyboard"), extension);
		CHECK_MESSAGE(text.contains("Gamepad"), extension);
		CHECK_MESSAGE(text.contains("Ctrl+Shift"), extension);
		CHECK_MESSAGE(text.contains("LeftStickLeft"), extension);
		CHECK_MESSAGE(text.contains("Label"), extension);
		CHECK_MESSAGE(text.contains("Ctrl"), extension);
		if (extension == "json") {
			CHECK(text.contains("InputMapSettingsActions"));
			CHECK(text.contains("\"input_events\""));
			CHECK(text.contains("\"key_mode\""));
			CHECK(text.contains("\"device_id\""));
		} else if (extension == "ini") {
			CHECK(text.contains("input_events"));
			CHECK(text.contains("device_id"));
		} else if (extension == "csv") {
			// One row per event: Action, Index, Device, Input, Modifiers, KeyMode, DeviceId, Deadzone.
			const Vector<String> lines = text.strip_edges().split("\n");
			REQUIRE(lines.size() >= 2);
			CHECK(lines[0].strip_edges() == "action,index,device,input,modifiers,key_mode,device_id,deadzone");
			int jump_rows = 0;
			int fire_rows = 0;
			for (const String &line : lines) {
				if (line.begins_with(String(JUMP) + ",")) {
					jump_rows++;
					CHECK_MESSAGE(line.ends_with(",0.45"), line);
				}
				if (line.begins_with(String(FIRE) + ",")) {
					fire_rows++;
				}
			}
			CHECK(jump_rows == 4);
			CHECK(fire_rows == 1);
			CHECK(text.contains(String(JUMP) + ",0,Keyboard,W,,Physical,Any,0.45"));
			CHECK(text.contains(String(JUMP) + ",1,Gamepad,LeftStickLeft,,Physical,Any,0.45"));
			CHECK(text.contains(String(JUMP) + ",2,Keyboard,E,Ctrl+Shift,Label,Any,0.45"));
			CHECK(text.contains(String(JUMP) + ",3,Gamepad,B,,Physical,Any,0.45"));
			CHECK(text.contains(String(FIRE) + ",0,Mouse,Right,Ctrl,Physical,Any,"));
		}

		// "Restart": the file gives back exactly these events.
		load_project_actions();
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK_MESSAGE(InputMapSettingsServer::events_to_text(server->get_events(JUMP)) == "Keyboard:W; GamepadAxis:LeftStickLeft; KeyboardLabel:Ctrl+Shift+E; Gamepad:B", extension);
		CHECK_MESSAGE(InputMapSettingsServer::events_to_text(server->get_events(FIRE)) == "Mouse:Ctrl+Right", extension);
		CHECK_MESSAGE(server->get_action_deadzone(JUMP) == doctest::Approx(0.45), extension);
		CHECK_MESSAGE(is_action(make_pressed_key(Key::W), JUMP), extension);
		CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
		Ref<InputEventKey> ctrl_shift_e = make_pressed_key(Key::E);
		ctrl_shift_e->set_ctrl_pressed(true);
		ctrl_shift_e->set_shift_pressed(true);
		CHECK_MESSAGE(is_action(ctrl_shift_e, JUMP), extension);
		CHECK_MESSAGE(is_action(InputEventJoypadButton::create_reference(JoyButton::B, 2), JUMP), extension);
		CHECK_MESSAGE(is_action(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f, 0), JUMP), extension);
		// Not exact, InputMap matches an axis in both directions (as not pressed); exact, only the bound direction.
		CHECK_FALSE_MESSAGE(InputMap::get_singleton()->event_is_action(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, 1.0f, 0), JUMP, true), extension);
		CHECK_FALSE(server->is_dirty());
		memdelete(server);
	}
}

TEST_CASE("[SceneTree][InputMapSettingsServer] A hand-written CSV file: pad numbers, axes, an action without events") {
	TestSetup setup;
	const String path = temp_file("input_map_hand.csv");
	const String text = vformat("action,index,device,input,modifiers,key_mode,device_id,deadzone\n%s,0,Gamepad,B,,Physical,2,0.3\n%s,1,Gamepad,RightStickRight,,Physical,Any,0.3\n%s,2,Keyboard,Q,Alt,Label,Any,0.3\n%s,-1,Keyboard,,,Physical,Any,0.5\n%s,3,Gamepad,LeftStickLeft,,Physical,3,0.3\n", JUMP, JUMP, JUMP, FIRE, JUMP);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(text);
	file.unref();

	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	server->set_file_path(path);
	CHECK(server->load() == OK);
	server->apply();
	CHECK(InputMapSettingsServer::events_to_text(server->get_events(JUMP)) == "Device2:Gamepad:B; GamepadAxis:RightStickRight; KeyboardLabel:Alt+Q; Device3:GamepadAxis:LeftStickLeft");
	CHECK(server->get_action_deadzone(FIRE) == doctest::Approx(0.5));
	CHECK(server->get_events(FIRE).is_empty());
	CHECK(is_action(InputEventJoypadButton::create_reference(JoyButton::B, 2), JUMP));
	CHECK_FALSE(is_action(InputEventJoypadButton::create_reference(JoyButton::B, 1), JUMP));
	CHECK(is_action(InputEventJoypadMotion::create_reference(JoyAxis::RIGHT_X, 1.0f, 0), JUMP));
	// A device-specific axis matches only its pad.
	CHECK(InputMap::get_singleton()->event_is_action(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f, 3), JUMP, true));
	CHECK_FALSE(InputMap::get_singleton()->event_is_action(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f, 0), JUMP, true));
	CHECK_FALSE(is_action(make_mouse_button(MouseButton::LEFT), FIRE));
	memdelete(server);
}

TEST_CASE("[SceneTree][InputMapSettingsServer] SAVE_MODE_ALL writes the layer of the actions") {
	TestSetup setup;
	BuiltinSetup builtin(InputMapSettingsServer::SAVE_MODE_ALL);
	const String path = temp_file("input_map_layer.json");
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	server->set_file_path(path);
	// Only changed values are written; one changed action writes the whole list, built-in actions included.
	CHECK(server->rebind(JUMP, make_key(Key::W, true)) == OK);
	CHECK(server->save() == OK);
	memdelete(server);
	const String text = FileAccess::get_file_as_string(path);
	CHECK(text.contains("ui_accept"));
	CHECK(text.contains("Custom"));
	if (InputMap::get_singleton()->is_builtin_input_action("ui_accept")) {
		CHECK(text.contains("BuiltIn"));
	}
}

TEST_CASE("[SceneTree][InputMapSettingsServer] Version 2 and version 1 files still load") {
	TestSetup setup;
	for (const String &extension : { String("json"), String("ini"), String("csv"), String("xml"), String("toml"), String("yaml") }) {
		if (!ResourceTextSerializer::has_format_implementation(ResourceTextSerializer::get_format_for_extension(extension))) {
			continue;
		}
		// A version 2 file: InputMapSettingsRows with one `bindings` text per action, written by the same engine one version earlier.
		Ref<InputMapSettingsRow> jump_row;
		jump_row.instantiate();
		jump_row->set_action(JUMP);
		jump_row->set_deadzone(0.4f);
		jump_row->set_bindings("Key:K; PhysicalKey:Ctrl+J; JoypadButton:B@2; JoypadMotion:LeftY-");
		Ref<InputMapSettingsRow> fire_row;
		fire_row.instantiate();
		fire_row->set_action(FIRE);
		fire_row->set_bindings("MouseButton:Right");
		TypedArray<InputMapSettingsRow> rows;
		rows.push_back(jump_row);
		rows.push_back(fire_row);
		Ref<InputMapSettingsRows> old_file;
		old_file.instantiate();
		old_file->set_actions(rows);

		ResourceTextSerializer::Options options = ResourceTextSerializer::get_default_options();
		String text;
		String error;
		Error err = OK;
		const ResourceTextSerializer::Format format = ResourceTextSerializer::get_format_for_extension(extension);
		if (format == ResourceTextSerializer::FORMAT_CSV) {
			err = ResourceTextSerializer::write_csv(old_file, options, text, error);
		} else {
			old_file->set_meta("settings_version", 2);
			if (format == ResourceTextSerializer::FORMAT_JSON) {
				err = ResourceTextSerializer::write_json(old_file, options, text, error);
			} else if (format == ResourceTextSerializer::FORMAT_INI) {
				options.ini_plain_values = true;
				err = ResourceTextSerializer::write_ini(old_file, options, text, error);
			} else {
				err = ResourceTextSerializer::write_format(format, old_file, options, text, error);
			}
		}
		REQUIRE_MESSAGE(err == OK, extension);
		CHECK_MESSAGE(text.contains("bindings"), extension);

		const String path = temp_file("input_map_v2." + extension);
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
		CHECK_MESSAGE(InputMapSettingsServer::events_to_text(server->get_events(JUMP)) == "KeyboardLabel:K; Keyboard:Ctrl+J; Device2:Gamepad:B; GamepadAxis:LeftStickUp", extension);
		CHECK_MESSAGE(is_action(make_pressed_key(Key::K), JUMP), extension);
		CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
		CHECK_MESSAGE(is_action(make_mouse_button(MouseButton::RIGHT), FIRE), extension);
		CHECK_FALSE_MESSAGE(is_action(make_mouse_button(MouseButton::LEFT), FIRE), extension);

		// The next save writes version 3.
		CHECK_MESSAGE(server->save() == OK, extension);
		memdelete(server);
		const String saved = FileAccess::get_file_as_string(path);
		CHECK_FALSE_MESSAGE(saved.contains("bindings"), extension);
		CHECK_MESSAGE(saved.contains("Keyboard"), extension);
	}
	{
		// A file of the first layout (InputActions with InputAction.bindings, version 1).
		for (const String &extension : { String("json"), String("csv") }) {
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
			CHECK_MESSAGE(InputMapSettingsServer::events_to_text(server->get_events(JUMP)) == "KeyboardLabel:K; Device2:Gamepad:B", extension);
			CHECK_MESSAGE(is_action(make_pressed_key(Key::K), JUMP), extension);
			CHECK_FALSE_MESSAGE(is_action(make_pressed_key(Key::SPACE), JUMP), extension);
			CHECK_MESSAGE(is_action(make_mouse_button(MouseButton::RIGHT), FIRE), extension);
			memdelete(server);
		}
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
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD)) == "KeyboardLabel:Space");
	CHECK(server->get_events(JUMP).size() == 2);
	CHECK(server->get_action_deadzone(JUMP) == doctest::Approx(0.3));
	CHECK(server->get_action_deadzone(FIRE) == doctest::Approx(0.2));

	// A copy of the action as a resource: changing it does not change the settings.
	const Ref<InputAction> copy = server->get_input_action(JUMP);
	REQUIRE(copy.is_valid());
	CHECK(copy->get_action() == StringName(JUMP));
	CHECK(copy->get_deadzone() == doctest::Approx(0.3));
	CHECK(copy->get_event_count() == 2);
	CHECK(InputMapSettingsServer::events_to_text(copy->get_events()) == "KeyboardLabel:Space; Gamepad:A");
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
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 2)) == "Keyboard:Down");
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
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 0)) == "Keyboard:Up");
	CHECK(InputAction::event_to_text(server->get_event(JUMP, InputAction::INPUT_DEVICE_KEYBOARD, 1)) == "Keyboard:Down");
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
			CHECK(server->rebind(FIRE, InputAction::text_to_event("Mouse:Ctrl+Right")) == OK);
			server->clear_event(FIRE, InputAction::INPUT_DEVICE_MOUSE, 1); // No second slot: nothing happens.
			server->set_action_deadzone(JUMP, 0.45f);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path);
		CHECK_MESSAGE(text.contains("Keyboard"), extension);
		CHECK_MESSAGE(text.contains("LeftStickUp"), extension);
		CHECK_MESSAGE(text.contains("Ctrl"), extension);
		// The events are fields, never sub-resources.
		CHECK_FALSE_MESSAGE(text.contains("InputEvent"), extension);
		if (extension == "csv") {
			// One row per event: jump has the key W and the axis (the pad button was replaced), fire one mouse button.
			int jump_rows = 0;
			int fire_rows = 0;
			for (const String &line : text.split("\n")) {
				if (line.begins_with(String(JUMP) + ",")) {
					jump_rows++;
				}
				if (line.begins_with(String(FIRE) + ",")) {
					fire_rows++;
					CHECK_MESSAGE(line.contains(",Mouse,Right,Ctrl,"), line);
				}
			}
			CHECK(jump_rows == 2);
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
		gone->add_event(InputAction::text_to_event("KeyboardLabel:G"));
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
