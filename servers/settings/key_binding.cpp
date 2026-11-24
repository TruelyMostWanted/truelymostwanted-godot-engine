/**************************************************************************/
/*  key_binding.cpp                                                       */
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

#include "key_binding.h"

#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "servers/display/display_server.h"

/////////////////////////////////////////////////////////////////////////////
// Text form.

static const char *_mouse_button_names[] = { "Left", "Right", "Middle", "WheelUp", "WheelDown", "WheelLeft", "WheelRight", "Extra1", "Extra2" };
static const char *_mouse_button_labels[] = { "Left Mouse Button", "Right Mouse Button", "Middle Mouse Button", "Mouse Wheel Up", "Mouse Wheel Down", "Mouse Wheel Left", "Mouse Wheel Right", "Mouse Thumb Button 1", "Mouse Thumb Button 2" };
static constexpr int MOUSE_BUTTON_NAME_COUNT = 9; // MouseButton::LEFT (1) to MouseButton::MB_XBUTTON2 (9).

static const char *_joy_button_names[] = { "A", "B", "X", "Y", "Back", "Guide", "Start", "LeftStick", "RightStick", "LeftShoulder", "RightShoulder", "DPadUp", "DPadDown", "DPadLeft", "DPadRight", "Misc1", "Paddle1", "Paddle2", "Paddle3", "Paddle4", "Touchpad", "Misc2", "Misc3", "Misc4", "Misc5", "Misc6" };
static const char *_joy_button_labels[] = { "A", "B", "X", "Y", "Back", "Guide", "Start", "Left Stick", "Right Stick", "Left Shoulder", "Right Shoulder", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Misc 1", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad", "Misc 2", "Misc 3", "Misc 4", "Misc 5", "Misc 6" };
static constexpr int JOY_BUTTON_NAME_COUNT = int(JoyButton::SDL_MAX);

static const char *_joy_axis_names[] = { "LeftX", "LeftY", "RightX", "RightY", "TriggerLeft", "TriggerRight" };
// Negative and positive direction per axis (Y is positive downwards).
static const char *_joy_axis_labels[][2] = {
	{ "Left Stick Left", "Left Stick Right" },
	{ "Left Stick Up", "Left Stick Down" },
	{ "Right Stick Left", "Right Stick Right" },
	{ "Right Stick Up", "Right Stick Down" },
	{ "Left Trigger", "Left Trigger" },
	{ "Right Trigger", "Right Trigger" },
};
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

String KeyBinding::event_to_text(const Ref<InputEvent> &p_event) {
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

Ref<InputEvent> KeyBinding::text_to_event(const String &p_text) {
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

KeyBinding::InputDevice KeyBinding::get_event_device(const Ref<InputEvent> &p_event) {
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

// Display servers that implement keyboard_get_label_from_physical(); the others print an error.
static bool _has_keyboard_layout() {
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr) {
		return false;
	}
	const String server_name = display_server->get_name();
	return server_name == "Windows" || server_name == "X11" || server_name == "Wayland" || server_name == "macOS" || server_name == "embedded";
}

String KeyBinding::event_to_label(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid()) {
		Key code = key_event->get_keycode();
		if (code == Key::NONE && key_event->get_physical_keycode() != Key::NONE) {
			code = key_event->get_physical_keycode();
			if (_has_keyboard_layout()) {
				const Key label = DisplayServer::get_singleton()->keyboard_get_label_from_physical(code);
				if (label != Key::NONE) {
					code = label;
				}
			}
		}
		if (code == Key::NONE) {
			code = key_event->get_key_label();
		}
		if (code == Key::NONE) {
			return String();
		}
		if (_is_modifier_key(code)) {
			return keycode_get_string(code);
		}
		return keycode_get_string(code | KeyModifierMask(key_event->get_modifiers_mask()));
	}
	const Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_valid()) {
		const int index = int(mouse_event->get_button_index());
		const String button = (index >= 1 && index <= MOUSE_BUTTON_NAME_COUNT) ? String(_mouse_button_labels[index - 1]) : vformat("Mouse Button %d", index);
		const Key modifiers = Key::NONE | KeyModifierMask(mouse_event->get_modifiers_mask());
		const String prefix = modifiers == Key::NONE ? String() : keycode_get_string(modifiers) + "+";
		return prefix + button;
	}
	const Ref<InputEventJoypadButton> button_event = p_event;
	if (button_event.is_valid()) {
		const int index = int(button_event->get_button_index());
		return index >= 0 && index < JOY_BUTTON_NAME_COUNT ? vformat("Joypad %s", _joy_button_labels[index]) : vformat("Joypad Button %d", index);
	}
	const Ref<InputEventJoypadMotion> motion_event = p_event;
	if (motion_event.is_valid()) {
		const int index = int(motion_event->get_axis());
		const bool positive = motion_event->get_axis_value() >= 0.0f;
		if (index >= 0 && index < JOY_AXIS_NAME_COUNT) {
			return String(_joy_axis_labels[index][positive ? 1 : 0]);
		}
		return vformat("Joypad Axis %d%s", index, positive ? "+" : "-");
	}
	return p_event.is_valid() ? p_event->as_text() : String();
}

/////////////////////////////////////////////////////////////////////////////
// KeyBinding.

void KeyBinding::set_action(const String &p_action) {
	action = p_action;
	emit_changed();
}

String KeyBinding::get_action() const {
	return action;
}

void KeyBinding::set_device(InputDevice p_device) {
	ERR_FAIL_INDEX(int(p_device), int(INPUT_DEVICE_MAX));
	device = p_device;
	emit_changed();
}

KeyBinding::InputDevice KeyBinding::get_device() const {
	return device;
}

void KeyBinding::set_slot(int p_slot) {
	slot = MAX(p_slot, 0);
	emit_changed();
}

int KeyBinding::get_slot() const {
	return slot;
}

void KeyBinding::set_event(const String &p_event) {
	event = p_event;
	emit_changed();
}

String KeyBinding::get_event() const {
	return event;
}

void KeyBinding::set_device_id(int p_device_id) {
	device_id = MAX(p_device_id, -1);
	emit_changed();
}

int KeyBinding::get_device_id() const {
	return device_id;
}

void KeyBinding::set_deadzone(float p_deadzone) {
	deadzone = CLAMP(p_deadzone, 0.0f, 1.0f);
	emit_changed();
}

float KeyBinding::get_deadzone() const {
	return deadzone;
}

Ref<InputEvent> KeyBinding::get_input_event() const {
	if (event.is_empty()) {
		return Ref<InputEvent>();
	}
	Ref<InputEvent> input_event = text_to_event(event);
	if (input_event.is_valid()) {
		input_event->set_device(device_id);
	}
	return input_event;
}

void KeyBinding::set_input_event(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		set_event(String());
		return;
	}
	const InputDevice event_device = get_event_device(p_event);
	ERR_FAIL_COND_MSG(event_device == INPUT_DEVICE_MAX, vformat("KeyBinding: a %s can't be bound; use a key, mouse button, joypad button or joypad axis.", p_event->get_class()));
	device = event_device;
	device_id = MAX(p_event->get_device(), -1);
	set_event(event_to_text(p_event));
}

bool KeyBinding::is_empty() const {
	return event.is_empty();
}

void KeyBinding::_bind_methods() {
	ClassDB::bind_static_method("KeyBinding", D_METHOD("event_to_text", "event"), &KeyBinding::event_to_text);
	ClassDB::bind_static_method("KeyBinding", D_METHOD("text_to_event", "text"), &KeyBinding::text_to_event);
	ClassDB::bind_static_method("KeyBinding", D_METHOD("get_event_device", "event"), &KeyBinding::get_event_device);
	ClassDB::bind_static_method("KeyBinding", D_METHOD("event_to_label", "event"), &KeyBinding::event_to_label);

	ClassDB::bind_method(D_METHOD("set_action", "action"), &KeyBinding::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &KeyBinding::get_action);
	ClassDB::bind_method(D_METHOD("set_device", "device"), &KeyBinding::set_device);
	ClassDB::bind_method(D_METHOD("get_device"), &KeyBinding::get_device);
	ClassDB::bind_method(D_METHOD("set_slot", "slot"), &KeyBinding::set_slot);
	ClassDB::bind_method(D_METHOD("get_slot"), &KeyBinding::get_slot);
	ClassDB::bind_method(D_METHOD("set_event", "event"), &KeyBinding::set_event);
	ClassDB::bind_method(D_METHOD("get_event"), &KeyBinding::get_event);
	ClassDB::bind_method(D_METHOD("set_device_id", "device_id"), &KeyBinding::set_device_id);
	ClassDB::bind_method(D_METHOD("get_device_id"), &KeyBinding::get_device_id);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &KeyBinding::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &KeyBinding::get_deadzone);
	ClassDB::bind_method(D_METHOD("get_input_event"), &KeyBinding::get_input_event);
	ClassDB::bind_method(D_METHOD("set_input_event", "event"), &KeyBinding::set_input_event);
	ClassDB::bind_method(D_METHOD("is_empty"), &KeyBinding::is_empty);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "device", PROPERTY_HINT_ENUM, "Keyboard,Mouse,Gamepad"), "set_device", "get_device");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "slot", PROPERTY_HINT_RANGE, "0,16,1,or_greater"), "set_slot", "get_slot");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "event", PROPERTY_HINT_PLACEHOLDER_TEXT, "Key:Ctrl+S"), "set_event", "get_event");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "device_id", PROPERTY_HINT_RANGE, "-1,16,1,or_greater"), "set_device_id", "get_device_id");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");

	BIND_ENUM_CONSTANT(INPUT_DEVICE_KEYBOARD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MOUSE);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_GAMEPAD);
	BIND_ENUM_CONSTANT(INPUT_DEVICE_MAX);
}

/////////////////////////////////////////////////////////////////////////////
// KeyBindingList.

void KeyBindingList::set_bindings(const TypedArray<KeyBinding> &p_bindings) {
	bindings = p_bindings;
	emit_changed();
}

TypedArray<KeyBinding> KeyBindingList::get_bindings() const {
	return bindings;
}

void KeyBindingList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &KeyBindingList::set_bindings);
	ClassDB::bind_method(D_METHOD("get_bindings"), &KeyBindingList::get_bindings);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bindings", PROPERTY_HINT_ARRAY_TYPE, "KeyBinding"), "set_bindings", "get_bindings");
}
