/**************************************************************************/
/*  input_action.cpp                                                      */
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

#include "input_action.h"

#include "core/object/class_db.h"
#include "core/os/keyboard.h"

/////////////////////////////////////////////////////////////////////////////
// Text form.

static const char *_mouse_button_names[] = { "Left", "Right", "Middle", "WheelUp", "WheelDown", "WheelLeft", "WheelRight", "Extra1", "Extra2" };
static constexpr int MOUSE_BUTTON_NAME_COUNT = 9; // MouseButton::LEFT (1) to MouseButton::MB_XBUTTON2 (9).

// The names of the text form (#193); Godot's JoyButton and JoyAxis enums keep their own names.
static const char *_joy_button_names[] = { "A", "B", "X", "Y", "Back", "Guide", "Start", "LeftStickPress", "RightStickPress", "LeftShoulder", "RightShoulder", "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "Misc1", "Paddle1", "Paddle2", "Paddle3", "Paddle4", "Touchpad", "Misc2", "Misc3", "Misc4", "Misc5", "Misc6" };
static constexpr int JOY_BUTTON_NAME_COUNT = int(JoyButton::SDL_MAX);

// An axis with its direction in the name (Godot's Y+ points down). Triggers only have the positive direction.
struct JoyAxisName {
	const char *name;
	JoyAxis axis;
	float value;
};

static const JoyAxisName _joy_axis_names[] = {
	{ "LeftStickLeft", JoyAxis::LEFT_X, -1.0f },
	{ "LeftStickRight", JoyAxis::LEFT_X, 1.0f },
	{ "LeftStickUp", JoyAxis::LEFT_Y, -1.0f },
	{ "LeftStickDown", JoyAxis::LEFT_Y, 1.0f },
	{ "RightStickLeft", JoyAxis::RIGHT_X, -1.0f },
	{ "RightStickRight", JoyAxis::RIGHT_X, 1.0f },
	{ "RightStickUp", JoyAxis::RIGHT_Y, -1.0f },
	{ "RightStickDown", JoyAxis::RIGHT_Y, 1.0f },
	{ "LeftTrigger", JoyAxis::TRIGGER_LEFT, 1.0f },
	{ "RightTrigger", JoyAxis::TRIGGER_RIGHT, 1.0f },
};

// Names before the final text form; still read, never written.
static const char *_old_joy_axis_names[] = { "LeftX", "LeftY", "RightX", "RightY", "TriggerLeft", "TriggerRight" };
static constexpr int OLD_JOY_AXIS_NAME_COUNT = int(JoyAxis::SDL_MAX);

static bool _is_modifier_key(Key p_key) {
	return p_key == Key::SHIFT || p_key == Key::CTRL || p_key == Key::ALT || p_key == Key::META;
}

// "CmdOrCtrl+Alt+" or "" (with the trailing separator).
static String _modifiers_to_text(const InputEventWithModifiers *p_event) {
	String text;
	if (p_event->is_command_or_control_autoremap()) {
		text += "CmdOrCtrl+";
	} else if (p_event->is_ctrl_pressed()) {
		text += "Ctrl+";
	}
	if (p_event->is_alt_pressed()) {
		text += "Alt+";
	}
	if (p_event->is_shift_pressed()) {
		text += "Shift+";
	}
	if (!p_event->is_command_or_control_autoremap() && p_event->is_meta_pressed()) {
		text += "Meta+";
	}
	return text;
}

// Sets the modifiers of all parts but the last; false if a part is not a modifier.
static bool _parse_modifiers(const Vector<String> &p_parts, InputEventWithModifiers *r_event) {
	bool command_or_control = false;
	for (int i = 0; i < p_parts.size() - 1; i++) {
		const String part = p_parts[i].strip_edges();
		if (part.nocasecmp_to("CmdOrCtrl") == 0) {
			command_or_control = true;
		} else if (part.nocasecmp_to("Ctrl") == 0 || part.nocasecmp_to("Control") == 0) {
			r_event->set_ctrl_pressed(true);
		} else if (part.nocasecmp_to("Alt") == 0 || part.nocasecmp_to("Option") == 0) {
			r_event->set_alt_pressed(true);
		} else if (part.nocasecmp_to("Shift") == 0) {
			r_event->set_shift_pressed(true);
		} else if (part.nocasecmp_to("Meta") == 0 || part.nocasecmp_to("Command") == 0 || part.nocasecmp_to("Cmd") == 0 || part.nocasecmp_to("Windows") == 0) {
			r_event->set_meta_pressed(true);
		} else {
			return false;
		}
	}
	if (command_or_control) {
		// Resolved here: Command on macOS, Ctrl elsewhere.
		r_event->set_ctrl_pressed(false);
		r_event->set_meta_pressed(false);
		r_event->set_command_or_control_autoremap(true);
	}
	return true;
}

static String _key_to_text(Key p_key) {
	if (p_key == Key::META) {
		return "Meta"; // The table name depends on the platform ("Command", "Windows").
	}
	const char *key_name = find_keycode_name(p_key);
	if (key_name != nullptr && key_name[0] != '\0') {
		return String(key_name);
	}
	return "#" + itos(int64_t(p_key));
}

static Key _text_to_key(const String &p_text) {
	if (p_text.begins_with("#")) {
		const String number = p_text.substr(1);
		return number.is_valid_int() ? Key(number.to_int()) : Key::NONE;
	}
	if (p_text.nocasecmp_to("Meta") == 0 || p_text.nocasecmp_to("Command") == 0 || p_text.nocasecmp_to("Windows") == 0) {
		return Key::META;
	}
	Key key = find_keycode(p_text);
	if (key == Key::NONE && p_text.length() == 1) {
		key = static_cast<Key>(char32_t(p_text.to_upper()[0]));
	}
	return key & KeyModifierMask::CODE_MASK;
}

// Name of p_index in p_names, or the number.
static String _index_to_text(int p_index, const char *const *p_names, int p_count) {
	if (p_index >= 0 && p_index < p_count) {
		return String(p_names[p_index]);
	}
	return itos(p_index);
}

static int _text_to_index(const String &p_text, const char *const *p_names, int p_count, int p_max) {
	for (int i = 0; i < p_count; i++) {
		if (p_text.nocasecmp_to(p_names[i]) == 0) {
			return i;
		}
	}
	if (p_text.is_valid_int()) {
		const int index = p_text.to_int();
		if (index >= 0 && index < p_max) {
			return index;
		}
	}
	return -1;
}

String InputAction::event_to_text(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid()) {
		// A pressed key has all three codes; the physical one keeps the binding on the same key on every layout.
		String type;
		Key code = Key::NONE;
		if (key_event->get_physical_keycode() != Key::NONE) {
			type = "Keyboard";
			code = key_event->get_physical_keycode();
		} else if (key_event->get_keycode() != Key::NONE) {
			type = "KeyboardLabel";
			code = key_event->get_keycode();
		} else if (key_event->get_key_label() != Key::NONE) {
			type = "KeyboardPrinted";
			code = key_event->get_key_label();
		} else {
			return String();
		}
		const String modifiers = _is_modifier_key(code) ? String() : _modifiers_to_text(key_event.ptr());
		return type + ":" + modifiers + _key_to_text(code);
	}
	const Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_valid()) {
		if (mouse_event->get_button_index() == MouseButton::NONE) {
			return String();
		}
		const int index = int(mouse_event->get_button_index());
		const String button = (index >= 1 && index <= MOUSE_BUTTON_NAME_COUNT) ? String(_mouse_button_names[index - 1]) : itos(index);
		return "Mouse:" + _modifiers_to_text(mouse_event.ptr()) + button;
	}
	// Only a gamepad event names its device: keys and mouse buttons have fixed device ids.
	const String device = p_event.is_valid() && p_event->get_device() >= 0 ? vformat("Device%d:", p_event->get_device()) : String();
	const Ref<InputEventJoypadButton> button_event = p_event;
	if (button_event.is_valid()) {
		if (button_event->get_button_index() == JoyButton::INVALID) {
			return String();
		}
		return device + "Gamepad:" + _index_to_text(int(button_event->get_button_index()), _joy_button_names, JOY_BUTTON_NAME_COUNT);
	}
	const Ref<InputEventJoypadMotion> motion_event = p_event;
	if (motion_event.is_valid()) {
		if (motion_event->get_axis() == JoyAxis::INVALID) {
			return String();
		}
		const bool negative = motion_event->get_axis_value() < 0.0f;
		for (const JoyAxisName &entry : _joy_axis_names) {
			if (entry.axis == motion_event->get_axis() && (entry.value < 0.0f) == negative) {
				return device + "GamepadAxis:" + entry.name;
			}
		}
		// Axes without a name (and a trigger below zero): the number and the sign.
		return device + "GamepadAxis:" + itos(int(motion_event->get_axis())) + (negative ? "-" : "+");
	}
	return String();
}

Ref<InputEvent> InputAction::text_to_event(const String &p_text) {
	String text = p_text.strip_edges();
	// `@3` and `@Any` at the end: the device suffix of older texts.
	int device = -1;
	const int at = text.rfind_char('@');
	if (at > 0) {
		const String suffix = text.substr(at + 1).strip_edges();
		if (suffix.nocasecmp_to("Any") == 0 || (suffix.is_valid_int() && suffix.to_int() >= 0)) {
			device = suffix.is_valid_int() ? int(suffix.to_int()) : -1;
			text = text.substr(0, at).strip_edges();
		}
	}
	int colon = text.find_char(':');
	if (colon <= 0) {
		return Ref<InputEvent>();
	}
	// `[Device<N>:]<Type>:[<Modifiers>+]<Input>`: split at most twice at `:` from the left.
	String type = text.substr(0, colon).strip_edges();
	if (type.nocasecmp_to("Any") == 0 || type.begins_with("Device")) {
		if (type.begins_with("Device")) {
			const String number = type.substr(6);
			if (!number.is_valid_int() || number.to_int() < 0) {
				return Ref<InputEvent>();
			}
			device = int(number.to_int());
		}
		text = text.substr(colon + 1);
		colon = text.find_char(':');
		if (colon <= 0) {
			return Ref<InputEvent>();
		}
		type = text.substr(0, colon).strip_edges();
	}
	// Names since #193 (Keyboard = physical key, KeyboardLabel = key code of the layout); the older names still load.
	if (type == "Keyboard") {
		type = "PhysicalKey";
	} else if (type == "KeyboardLabel") {
		type = "Key";
	} else if (type == "KeyboardPrinted") {
		type = "KeyLabel";
	} else if (type == "Mouse") {
		type = "MouseButton";
	} else if (type == "Gamepad") {
		type = "JoypadButton";
	} else if (type == "GamepadAxis") {
		type = "JoypadMotion";
	}
	const String value = text.substr(colon + 1).strip_edges();
	if (value.is_empty()) {
		return Ref<InputEvent>();
	}

	if (type == "Key" || type == "PhysicalKey" || type == "KeyLabel") {
		// The last part is the key; key names have no `+` (`Ctrl+Plus`).
		const Vector<String> parts = value.split("+");
		const Key code = _text_to_key(parts[parts.size() - 1].strip_edges());
		if (code == Key::NONE) {
			return Ref<InputEvent>();
		}
		Ref<InputEventKey> key_event;
		key_event.instantiate();
		if (type == "Key") {
			key_event->set_keycode(code);
		} else if (type == "PhysicalKey") {
			key_event->set_physical_keycode(code);
		} else {
			key_event->set_key_label(code);
		}
		if (!_parse_modifiers(parts, key_event.ptr())) {
			return Ref<InputEvent>();
		}
		// Keys are bound for all devices; a device prefix is read but not kept.
		key_event->set_device(-1);
		return key_event;
	}
	if (type == "MouseButton") {
		const Vector<String> parts = value.split("+");
		const String button = parts[parts.size() - 1].strip_edges();
		int index = -1;
		for (int i = 0; i < MOUSE_BUTTON_NAME_COUNT; i++) {
			if (button.nocasecmp_to(_mouse_button_names[i]) == 0) {
				index = i + 1;
				break;
			}
		}
		if (index < 0 && button.is_valid_int() && button.to_int() > 0 && button.to_int() <= 32) {
			index = int(button.to_int());
		}
		if (index < 0) {
			return Ref<InputEvent>();
		}
		Ref<InputEventMouseButton> mouse_event;
		mouse_event.instantiate();
		mouse_event->set_button_index(MouseButton(index));
		if (!_parse_modifiers(parts, mouse_event.ptr())) {
			return Ref<InputEvent>();
		}
		mouse_event->set_device(-1);
		return mouse_event;
	}
	if (type == "JoypadButton") {
		int index = _text_to_index(value, _joy_button_names, JOY_BUTTON_NAME_COUNT, int(JoyButton::MAX));
		if (index < 0 && value.nocasecmp_to("LeftStick") == 0) {
			index = int(JoyButton::LEFT_STICK); // Old name of LeftStickPress.
		} else if (index < 0 && value.nocasecmp_to("RightStick") == 0) {
			index = int(JoyButton::RIGHT_STICK);
		}
		if (index < 0) {
			return Ref<InputEvent>();
		}
		return InputEventJoypadButton::create_reference(JoyButton(index), device);
	}
	if (type == "JoypadMotion") {
		for (const JoyAxisName &entry : _joy_axis_names) {
			if (value.nocasecmp_to(entry.name) == 0) {
				return InputEventJoypadMotion::create_reference(entry.axis, entry.value, device);
			}
		}
		// The older form: axis name or number with the sign (`LeftX-`, `TriggerRight+`, `6+`).
		const char32_t sign = value[value.length() - 1];
		if (sign != '+' && sign != '-') {
			return Ref<InputEvent>();
		}
		const int index = _text_to_index(value.substr(0, value.length() - 1).strip_edges(), _old_joy_axis_names, OLD_JOY_AXIS_NAME_COUNT, int(JoyAxis::MAX));
		if (index < 0) {
			return Ref<InputEvent>();
		}
		return InputEventJoypadMotion::create_reference(JoyAxis(index), sign == '-' ? -1.0f : 1.0f, device);
	}
	return Ref<InputEvent>();
}

InputAction::InputDevice InputAction::get_event_device(const Ref<InputEvent> &p_event) {
	if (Ref<InputEventKey>(p_event).is_valid()) {
		return INPUT_DEVICE_KEYBOARD;
	}
	if (Ref<InputEventMouseButton>(p_event).is_valid()) {
		return INPUT_DEVICE_MOUSE;
	}
	if (Ref<InputEventJoypadButton>(p_event).is_valid() || Ref<InputEventJoypadMotion>(p_event).is_valid()) {
		return INPUT_DEVICE_GAMEPAD;
	}
	return INPUT_DEVICE_MAX;
}

/////////////////////////////////////////////////////////////////////////////
// InputAction.

InputAction::InputAction(const StringName &p_action, float p_deadzone) :
		action(p_action),
		deadzone(p_deadzone) {
}

void InputAction::set_action(const StringName &p_action) {
	if (action == p_action) {
		return;
	}
	action = p_action;
	emit_changed();
}

StringName InputAction::get_action() const {
	return action;
}

void InputAction::set_deadzone(float p_deadzone) {
	if (deadzone == p_deadzone) {
		return;
	}
	deadzone = p_deadzone;
	emit_changed();
}

float InputAction::get_deadzone() const {
	return deadzone;
}

void InputAction::set_events(const TypedArray<InputEvent> &p_events) {
	events.clear();
	event_slots.clear();
	bool has_empty_slot = false;
	for (int i = 0; i < p_events.size(); i++) {
		const Ref<InputEvent> event = p_events[i];
		if (event.is_valid()) {
			events.push_back(event);
		} else {
			has_empty_slot = true;
		}
	}
	if (has_empty_slot) {
		// Kept for the Inspector, which sets the whole array again once the slot has an event.
		for (int i = 0; i < p_events.size(); i++) {
			event_slots.push_back(p_events[i]);
		}
	}
	emit_changed();
}

TypedArray<InputEvent> InputAction::get_events() const {
	TypedArray<InputEvent> result;
	for (const Ref<InputEvent> &event : event_slots.is_empty() ? events : event_slots) {
		result.push_back(event);
	}
	return result;
}

void InputAction::add_event(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "InputAction: the event is null.");
	if (has_event(p_event)) {
		return;
	}
	events.push_back(p_event);
	event_slots.clear();
	emit_changed();
}

bool InputAction::has_event(const Ref<InputEvent> &p_event) const {
	if (p_event.is_null()) {
		return false;
	}
	for (const Ref<InputEvent> &event : events) {
		if (event == p_event || event->is_match(p_event, true)) {
			return true;
		}
	}
	return false;
}

bool InputAction::remove_event(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		return false;
	}
	for (int i = 0; i < events.size(); i++) {
		if (events[i] == p_event || events[i]->is_match(p_event, true)) {
			events.remove_at(i);
			event_slots.clear();
			emit_changed();
			return true;
		}
	}
	return false;
}

void InputAction::push_event(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "InputAction: the event is null.");
	events.push_back(p_event);
	event_slots.clear();
	emit_changed();
}

void InputAction::remove_event_at(int p_index) {
	ERR_FAIL_INDEX(p_index, events.size());
	events.remove_at(p_index);
	event_slots.clear();
	emit_changed();
}

void InputAction::clear_events() {
	if (events.is_empty() && event_slots.is_empty()) {
		return;
	}
	events.clear();
	event_slots.clear();
	emit_changed();
}

int InputAction::get_event_count() const {
	return events.size();
}

TypedArray<InputEvent> InputAction::get_device_events(InputDevice p_device) const {
	TypedArray<InputEvent> result;
	for (const Ref<InputEvent> &event : events) {
		if (get_event_device(event) == p_device) {
			result.push_back(event);
		}
	}
	return result;
}

void InputAction::_bind_methods() {
	ClassDB::bind_static_method("InputAction", D_METHOD("event_to_text", "event"), &InputAction::event_to_text);
	ClassDB::bind_static_method("InputAction", D_METHOD("text_to_event", "text"), &InputAction::text_to_event);
	ClassDB::bind_static_method("InputAction", D_METHOD("get_event_device", "event"), &InputAction::get_event_device);

	ClassDB::bind_method(D_METHOD("set_action", "action"), &InputAction::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InputAction::get_action);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &InputAction::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &InputAction::get_deadzone);
	ClassDB::bind_method(D_METHOD("set_events", "events"), &InputAction::set_events);
	ClassDB::bind_method(D_METHOD("get_events"), &InputAction::get_events);

	ClassDB::bind_method(D_METHOD("add_event", "event"), &InputAction::add_event);
	ClassDB::bind_method(D_METHOD("has_event", "event"), &InputAction::has_event);
	ClassDB::bind_method(D_METHOD("remove_event", "event"), &InputAction::remove_event);
	ClassDB::bind_method(D_METHOD("clear_events"), &InputAction::clear_events);
	ClassDB::bind_method(D_METHOD("get_event_count"), &InputAction::get_event_count);
	ClassDB::bind_method(D_METHOD("get_device_events", "device"), &InputAction::get_device_events);

	// The Inspector suggests the actions of the project (#398); other names can still be typed.
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action", PROPERTY_HINT_INPUT_NAME, "show_builtin,loose_mode"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "events", PROPERTY_HINT_ARRAY_TYPE, "InputEvent"), "set_events", "get_events");

	BIND_ENUM_CONSTANT(INPUT_DEVICE_KEYBOARD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MOUSE);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_GAMEPAD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MAX);
}
