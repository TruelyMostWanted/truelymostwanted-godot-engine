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

static const char *_joy_button_names[] = { "A", "B", "X", "Y", "Back", "Guide", "Start", "LeftStick", "RightStick", "LeftShoulder", "RightShoulder", "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "Misc1", "Paddle1", "Paddle2", "Paddle3", "Paddle4", "Touchpad", "Misc2", "Misc3", "Misc4", "Misc5", "Misc6" };
static constexpr int JOY_BUTTON_NAME_COUNT = int(JoyButton::SDL_MAX);

static const char *_joy_axis_names[] = { "LeftX", "LeftY", "RightX", "RightY", "TriggerLeft", "TriggerRight" };
static constexpr int JOY_AXIS_NAME_COUNT = int(JoyAxis::SDL_MAX);

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
			type = "PhysicalKey";
			code = key_event->get_physical_keycode();
		} else if (key_event->get_keycode() != Key::NONE) {
			type = "Key";
			code = key_event->get_keycode();
		} else if (key_event->get_key_label() != Key::NONE) {
			type = "KeyLabel";
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
		return "MouseButton:" + _modifiers_to_text(mouse_event.ptr()) + button;
	}
	const Ref<InputEventJoypadButton> button_event = p_event;
	if (button_event.is_valid()) {
		if (button_event->get_button_index() == JoyButton::INVALID) {
			return String();
		}
		return "JoypadButton:" + _index_to_text(int(button_event->get_button_index()), _joy_button_names, JOY_BUTTON_NAME_COUNT);
	}
	const Ref<InputEventJoypadMotion> motion_event = p_event;
	if (motion_event.is_valid()) {
		if (motion_event->get_axis() == JoyAxis::INVALID) {
			return String();
		}
		return "JoypadMotion:" + _index_to_text(int(motion_event->get_axis()), _joy_axis_names, JOY_AXIS_NAME_COUNT) + (motion_event->get_axis_value() < 0.0f ? "-" : "+");
	}
	return String();
}

Ref<InputEvent> InputAction::text_to_event(const String &p_text) {
	const int colon = p_text.find_char(':');
	if (colon <= 0) {
		return Ref<InputEvent>();
	}
	const String type = p_text.substr(0, colon).strip_edges();
	const String value = p_text.substr(colon + 1).strip_edges();
	if (value.is_empty()) {
		return Ref<InputEvent>();
	}

	if (type == "Key" || type == "PhysicalKey" || type == "KeyLabel") {
		// The last part is the key; "Ctrl+Plus" has no "+" in the key name.
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
			index = button.to_int();
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
		const int index = _text_to_index(value, _joy_button_names, JOY_BUTTON_NAME_COUNT, int(JoyButton::MAX));
		if (index < 0) {
			return Ref<InputEvent>();
		}
		return InputEventJoypadButton::create_reference(JoyButton(index), -1);
	}
	if (type == "JoypadMotion") {
		const char32_t sign = value[value.length() - 1];
		if (sign != '+' && sign != '-') {
			return Ref<InputEvent>();
		}
		const int index = _text_to_index(value.substr(0, value.length() - 1).strip_edges(), _joy_axis_names, JOY_AXIS_NAME_COUNT, int(JoyAxis::MAX));
		if (index < 0) {
			return Ref<InputEvent>();
		}
		return InputEventJoypadMotion::create_reference(JoyAxis(index), sign == '-' ? -1.0f : 1.0f, -1);
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
	for (int i = 0; i < p_events.size(); i++) {
		const Ref<InputEvent> event = p_events[i];
		if (event.is_valid()) {
			events.push_back(event);
		}
	}
	emit_changed();
}

TypedArray<InputEvent> InputAction::get_events() const {
	TypedArray<InputEvent> result;
	for (const Ref<InputEvent> &event : events) {
		result.push_back(event);
	}
	return result;
}

void InputAction::set_bindings(const String &p_bindings) {
	events.clear();
	PackedStringArray invalid;
	const Vector<String> items = p_bindings.split(";", false);
	for (const String &raw_item : items) {
		String item = raw_item.strip_edges();
		if (item.is_empty()) {
			continue;
		}
		// A joypad binding for one pad ends with "@<device id>".
		int device_id = -1;
		const int at = item.rfind_char('@');
		if (at > 0 && item.substr(at + 1).is_valid_int()) {
			device_id = item.substr(at + 1).to_int();
			item = item.substr(0, at).strip_edges();
		}
		const Ref<InputEvent> event = text_to_event(item);
		if (event.is_null()) {
			invalid.push_back(item);
			continue;
		}
		if (device_id >= 0) {
			event->set_device(device_id);
		}
		events.push_back(event);
	}
	emit_changed();
	if (!invalid.is_empty()) {
		WARN_PRINT(vformat("InputAction \"%s\": can't read the binding %s; it is skipped.", action, String(", ").join(invalid)));
	}
}

String InputAction::get_bindings() const {
	String result;
	for (const Ref<InputEvent> &event : events) {
		String text = event_to_text(event);
		if (text.is_empty()) {
			continue;
		}
		if (get_event_device(event) == INPUT_DEVICE_GAMEPAD && event->get_device() >= 0) {
			text += "@" + itos(event->get_device());
		}
		if (!result.is_empty()) {
			result += "; ";
		}
		result += text;
	}
	return result;
}

void InputAction::add_event(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "InputAction: the event is null.");
	if (has_event(p_event)) {
		return;
	}
	events.push_back(p_event);
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
			emit_changed();
			return true;
		}
	}
	return false;
}

void InputAction::push_event(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "InputAction: the event is null.");
	events.push_back(p_event);
	emit_changed();
}

void InputAction::remove_event_at(int p_index) {
	ERR_FAIL_INDEX(p_index, events.size());
	events.remove_at(p_index);
	emit_changed();
}

void InputAction::clear_events() {
	if (events.is_empty()) {
		return;
	}
	events.clear();
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
	ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &InputAction::set_bindings);
	ClassDB::bind_method(D_METHOD("get_bindings"), &InputAction::get_bindings);

	ClassDB::bind_method(D_METHOD("add_event", "event"), &InputAction::add_event);
	ClassDB::bind_method(D_METHOD("has_event", "event"), &InputAction::has_event);
	ClassDB::bind_method(D_METHOD("remove_event", "event"), &InputAction::remove_event);
	ClassDB::bind_method(D_METHOD("clear_events"), &InputAction::clear_events);
	ClassDB::bind_method(D_METHOD("get_event_count"), &InputAction::get_event_count);
	ClassDB::bind_method(D_METHOD("get_device_events", "device"), &InputAction::get_device_events);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
	// Edited in the Inspector, stored as `bindings`.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "events", PROPERTY_HINT_ARRAY_TYPE, "InputEvent", PROPERTY_USAGE_EDITOR), "set_events", "get_events");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bindings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_bindings", "get_bindings");

	BIND_ENUM_CONSTANT(INPUT_DEVICE_KEYBOARD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MOUSE);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_GAMEPAD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MAX);
}
