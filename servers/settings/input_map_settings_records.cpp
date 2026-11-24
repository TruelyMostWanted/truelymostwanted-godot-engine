/**************************************************************************/
/*  input_map_settings_records.cpp                                        */
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

#include "input_map_settings_records.h"

#include "core/input/input_action.h"
#include "core/object/class_db.h"

static constexpr const char *DEVICE_ID_ANY = "Any";

// `Ctrl+Shift+W` into the modifiers (`Ctrl+Shift`) and the input (`W`).
static void _split_modifiers(const String &p_value, String &r_modifiers, String &r_input) {
	const int plus = p_value.rfind_char('+');
	if (plus > 0 && plus < p_value.length() - 1) {
		r_modifiers = p_value.substr(0, plus);
		r_input = p_value.substr(plus + 1);
	} else {
		r_modifiers = String();
		r_input = p_value;
	}
}

/////////////////////////////////////////////////////////////////////////////
// InputMapSettingsEvent.

Ref<InputMapSettingsEvent> InputMapSettingsEvent::from_event(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		return Ref<InputMapSettingsEvent>();
	}
	// The text form of InputAction has the kind of the event and the key; the record splits it into its fields.
	const String text = InputAction::event_to_text(p_event);
	const int colon = text.find_char(':');
	if (colon <= 0) {
		return Ref<InputMapSettingsEvent>();
	}
	String type = text.substr(0, colon);
	const String value = text.substr(colon + 1);
	// InputAction::event_to_text() writes the names of #193; map them to the cases below.
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

	Ref<InputMapSettingsEvent> record;
	record.instantiate();
	if (type == "PhysicalKey" || type == "Key" || type == "KeyLabel") {
		record->device = DEVICE_KEYBOARD;
		record->key_mode = type == "PhysicalKey" ? KEY_MODE_PHYSICAL : KEY_MODE_LABEL;
		_split_modifiers(value, record->modifiers, record->input);
	} else if (type == "MouseButton") {
		record->device = DEVICE_MOUSE;
		_split_modifiers(value, record->modifiers, record->input);
	} else if (type == "JoypadButton") {
		record->device = DEVICE_GAMEPAD;
		record->input = value;
	} else if (type == "JoypadMotion" && value.length() > 1) {
		record->device = DEVICE_GAMEPAD;
		record->input = value.substr(0, value.length() - 1);
		record->direction = value.substr(value.length() - 1);
	} else {
		return Ref<InputMapSettingsEvent>();
	}
	if (record->device == DEVICE_GAMEPAD && p_event->get_device() >= 0) {
		record->device_id = itos(p_event->get_device());
	}
	return record;
}

Ref<InputMapSettingsEvent> InputMapSettingsEvent::from_text(const String &p_text) {
	String item = p_text.strip_edges();
	String parsed_id = DEVICE_ID_ANY;
	const int at = item.rfind_char('@');
	if (at > 0) {
		const String suffix = item.substr(at + 1).strip_edges();
		if (suffix.nocasecmp_to(DEVICE_ID_ANY) == 0 || suffix.is_valid_int()) {
			parsed_id = suffix.is_valid_int() ? itos(suffix.to_int()) : String(DEVICE_ID_ANY);
			item = item.substr(0, at).strip_edges();
		}
	}
	const int colon = item.find_char(':');
	if (colon <= 0) {
		return Ref<InputMapSettingsEvent>();
	}
	const String type = item.substr(0, colon).strip_edges();
	const String value = item.substr(colon + 1).strip_edges();
	if (value.is_empty()) {
		return Ref<InputMapSettingsEvent>();
	}

	Ref<InputMapSettingsEvent> record;
	if (type == "Keyboard" || type == "KeyboardLabel" || type == "Mouse") {
		record.instantiate();
		record->device = type == "Mouse" ? DEVICE_MOUSE : DEVICE_KEYBOARD;
		record->key_mode = type == "KeyboardLabel" ? KEY_MODE_LABEL : KEY_MODE_PHYSICAL;
		_split_modifiers(value, record->modifiers, record->input);
	} else if (type == "Gamepad") {
		record.instantiate();
		record->device = DEVICE_GAMEPAD;
		record->input = value;
	} else if (type == "GamepadAxis") {
		if (value.length() < 2) {
			return Ref<InputMapSettingsEvent>();
		}
		record.instantiate();
		record->device = DEVICE_GAMEPAD;
		record->input = value.substr(0, value.length() - 1);
		record->direction = value.substr(value.length() - 1);
	} else {
		// A name of a version 2 file: `Key:Ctrl+S`, `PhysicalKey:W`, `MouseButton:Left`, `JoypadMotion:LeftX-`.
		const Ref<InputEvent> event = InputAction::text_to_event(item);
		record = from_event(event);
		if (record.is_null()) {
			return record;
		}
	}
	if (record->device == DEVICE_GAMEPAD) {
		record->device_id = parsed_id;
	}
	// Names that do not exist are found here, not when the event is used.
	if (record->to_event().is_null()) {
		return Ref<InputMapSettingsEvent>();
	}
	return record;
}

int InputMapSettingsEvent::get_device_number() const {
	if (device_id.is_valid_int() && device_id.to_int() >= 0) {
		return device_id.to_int();
	}
	return -1;
}

Ref<InputEvent> InputMapSettingsEvent::to_event() const {
	if (input.is_empty()) {
		return Ref<InputEvent>();
	}
	String text;
	switch (device) {
		case DEVICE_KEYBOARD: {
			text = String(key_mode == KEY_MODE_LABEL ? "Key:" : "PhysicalKey:") + (modifiers.is_empty() ? String() : modifiers + "+") + input;
		} break;
		case DEVICE_MOUSE: {
			text = "MouseButton:" + (modifiers.is_empty() ? String() : modifiers + "+") + input;
		} break;
		case DEVICE_GAMEPAD: {
			if (direction.is_empty()) {
				text = "JoypadButton:" + input;
			} else if (direction == "+" || direction == "-") {
				text = "JoypadMotion:" + input + direction;
			} else {
				return Ref<InputEvent>();
			}
		} break;
	}
	Ref<InputEvent> event = InputAction::text_to_event(text);
	if (event.is_valid() && device == DEVICE_GAMEPAD && get_device_number() >= 0) {
		event->set_device(get_device_number());
	}
	return event;
}

String InputMapSettingsEvent::to_text() const {
	String prefix;
	switch (device) {
		case DEVICE_KEYBOARD:
			prefix = key_mode == KEY_MODE_LABEL ? "KeyboardLabel" : "Keyboard";
			break;
		case DEVICE_MOUSE:
			prefix = "Mouse";
			break;
		case DEVICE_GAMEPAD:
			prefix = direction.is_empty() ? "Gamepad" : "GamepadAxis";
			break;
	}
	String text = prefix + ":";
	if (device != DEVICE_GAMEPAD && !modifiers.is_empty()) {
		text += modifiers + "+";
	}
	text += input;
	if (device == DEVICE_GAMEPAD) {
		text += direction;
		// A button names its pad (`@Any` or `@1`), an axis only if it is one pad.
		if (direction.is_empty()) {
			text += "@" + (get_device_number() >= 0 ? itos(get_device_number()) : String(DEVICE_ID_ANY));
		} else if (get_device_number() >= 0) {
			text += "@" + itos(get_device_number());
		}
	}
	return text;
}

void InputMapSettingsEvent::set_device(Device p_device) {
	ERR_FAIL_INDEX(int(p_device), int(DEVICE_GAMEPAD) + 1);
	device = p_device;
}

InputMapSettingsEvent::Device InputMapSettingsEvent::get_device() const {
	return device;
}

void InputMapSettingsEvent::set_input(const String &p_input) {
	input = p_input;
}

String InputMapSettingsEvent::get_input() const {
	return input;
}

void InputMapSettingsEvent::set_direction(const String &p_direction) {
	direction = p_direction;
}

String InputMapSettingsEvent::get_direction() const {
	return direction;
}

void InputMapSettingsEvent::set_modifiers(const String &p_modifiers) {
	modifiers = p_modifiers;
}

String InputMapSettingsEvent::get_modifiers() const {
	return modifiers;
}

void InputMapSettingsEvent::set_key_mode(KeyMode p_key_mode) {
	ERR_FAIL_INDEX(int(p_key_mode), int(KEY_MODE_LABEL) + 1);
	key_mode = p_key_mode;
}

InputMapSettingsEvent::KeyMode InputMapSettingsEvent::get_key_mode() const {
	return key_mode;
}

void InputMapSettingsEvent::set_device_id(const String &p_device_id) {
	device_id = p_device_id;
}

String InputMapSettingsEvent::get_device_id() const {
	return device_id;
}

void InputMapSettingsEvent::_bind_methods() {
	ClassDB::bind_static_method("InputMapSettingsEvent", D_METHOD("from_event", "event"), &InputMapSettingsEvent::from_event);
	ClassDB::bind_static_method("InputMapSettingsEvent", D_METHOD("from_text", "text"), &InputMapSettingsEvent::from_text);
	ClassDB::bind_method(D_METHOD("to_event"), &InputMapSettingsEvent::to_event);
	ClassDB::bind_method(D_METHOD("to_text"), &InputMapSettingsEvent::to_text);

	ClassDB::bind_method(D_METHOD("set_device", "device"), &InputMapSettingsEvent::set_device);
	ClassDB::bind_method(D_METHOD("get_device"), &InputMapSettingsEvent::get_device);
	ClassDB::bind_method(D_METHOD("set_input", "input"), &InputMapSettingsEvent::set_input);
	ClassDB::bind_method(D_METHOD("get_input"), &InputMapSettingsEvent::get_input);
	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &InputMapSettingsEvent::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &InputMapSettingsEvent::get_direction);
	ClassDB::bind_method(D_METHOD("set_modifiers", "modifiers"), &InputMapSettingsEvent::set_modifiers);
	ClassDB::bind_method(D_METHOD("get_modifiers"), &InputMapSettingsEvent::get_modifiers);
	ClassDB::bind_method(D_METHOD("set_key_mode", "key_mode"), &InputMapSettingsEvent::set_key_mode);
	ClassDB::bind_method(D_METHOD("get_key_mode"), &InputMapSettingsEvent::get_key_mode);
	ClassDB::bind_method(D_METHOD("set_device_id", "device_id"), &InputMapSettingsEvent::set_device_id);
	ClassDB::bind_method(D_METHOD("get_device_id"), &InputMapSettingsEvent::get_device_id);

	// The field names of the file.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "device", PROPERTY_HINT_ENUM, "Keyboard,Mouse,Gamepad"), "set_device", "get_device");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "input"), "set_input", "get_input");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modifiers"), "set_modifiers", "get_modifiers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "key_mode", PROPERTY_HINT_ENUM, "Physical,Label"), "set_key_mode", "get_key_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_id"), "set_device_id", "get_device_id");

	BIND_ENUM_CONSTANT(DEVICE_KEYBOARD);
	BIND_ENUM_CONSTANT(DEVICE_MOUSE);
	BIND_ENUM_CONSTANT(DEVICE_GAMEPAD);
	BIND_ENUM_CONSTANT(KEY_MODE_PHYSICAL);
	BIND_ENUM_CONSTANT(KEY_MODE_LABEL);
}

/////////////////////////////////////////////////////////////////////////////
// InputMapSettingsAction and InputMapSettingsActions.

void InputMapSettingsAction::set_action(const StringName &p_action) {
	action = p_action;
}

StringName InputMapSettingsAction::get_action() const {
	return action;
}

void InputMapSettingsAction::set_deadzone(float p_deadzone) {
	deadzone = p_deadzone;
}

float InputMapSettingsAction::get_deadzone() const {
	return deadzone;
}

void InputMapSettingsAction::set_layer(Layer p_layer) {
	ERR_FAIL_INDEX(int(p_layer), int(LAYER_CUSTOM) + 1);
	layer = p_layer;
}

InputMapSettingsAction::Layer InputMapSettingsAction::get_layer() const {
	return layer;
}

void InputMapSettingsAction::set_input_events(const TypedArray<InputMapSettingsEvent> &p_input_events) {
	input_events = p_input_events;
}

TypedArray<InputMapSettingsEvent> InputMapSettingsAction::get_input_events() const {
	return input_events;
}

void InputMapSettingsAction::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InputMapSettingsAction::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InputMapSettingsAction::get_action);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &InputMapSettingsAction::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &InputMapSettingsAction::get_deadzone);
	ClassDB::bind_method(D_METHOD("set_layer", "layer"), &InputMapSettingsAction::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer"), &InputMapSettingsAction::get_layer);
	ClassDB::bind_method(D_METHOD("set_input_events", "input_events"), &InputMapSettingsAction::set_input_events);
	ClassDB::bind_method(D_METHOD("get_input_events"), &InputMapSettingsAction::get_input_events);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer", PROPERTY_HINT_ENUM, "BuiltIn,Custom"), "set_layer", "get_layer");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "input_events", PROPERTY_HINT_ARRAY_TYPE, "InputMapSettingsEvent"), "set_input_events", "get_input_events");

	BIND_ENUM_CONSTANT(LAYER_BUILT_IN);
	BIND_ENUM_CONSTANT(LAYER_CUSTOM);
}

void InputMapSettingsActions::set_actions(const TypedArray<InputMapSettingsAction> &p_actions) {
	actions = p_actions;
}

TypedArray<InputMapSettingsAction> InputMapSettingsActions::get_actions() const {
	return actions;
}

void InputMapSettingsActions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_actions", "actions"), &InputMapSettingsActions::set_actions);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMapSettingsActions::get_actions);

	// The name of InputActions.actions, the list property of the settings.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "actions", PROPERTY_HINT_ARRAY_TYPE, "InputMapSettingsAction"), "set_actions", "get_actions");
}

/////////////////////////////////////////////////////////////////////////////
// InputMapSettingsEventRow and InputMapSettingsEventRows (CSV).

void InputMapSettingsEventRow::set_action(const StringName &p_action) {
	action = p_action;
}

StringName InputMapSettingsEventRow::get_action() const {
	return action;
}

void InputMapSettingsEventRow::set_index(int p_index) {
	index = p_index;
}

int InputMapSettingsEventRow::get_index() const {
	return index;
}

void InputMapSettingsEventRow::set_device(InputMapSettingsEvent::Device p_device) {
	ERR_FAIL_INDEX(int(p_device), int(InputMapSettingsEvent::DEVICE_GAMEPAD) + 1);
	device = p_device;
}

InputMapSettingsEvent::Device InputMapSettingsEventRow::get_device() const {
	return device;
}

void InputMapSettingsEventRow::set_input(const String &p_input) {
	input = p_input;
}

String InputMapSettingsEventRow::get_input() const {
	return input;
}

void InputMapSettingsEventRow::set_direction(const String &p_direction) {
	direction = p_direction;
}

String InputMapSettingsEventRow::get_direction() const {
	return direction;
}

void InputMapSettingsEventRow::set_modifiers(const String &p_modifiers) {
	modifiers = p_modifiers;
}

String InputMapSettingsEventRow::get_modifiers() const {
	return modifiers;
}

void InputMapSettingsEventRow::set_key_mode(InputMapSettingsEvent::KeyMode p_key_mode) {
	ERR_FAIL_INDEX(int(p_key_mode), int(InputMapSettingsEvent::KEY_MODE_LABEL) + 1);
	key_mode = p_key_mode;
}

InputMapSettingsEvent::KeyMode InputMapSettingsEventRow::get_key_mode() const {
	return key_mode;
}

void InputMapSettingsEventRow::set_device_id(const String &p_device_id) {
	device_id = p_device_id;
}

String InputMapSettingsEventRow::get_device_id() const {
	return device_id;
}

void InputMapSettingsEventRow::set_deadzone(float p_deadzone) {
	deadzone = p_deadzone;
}

float InputMapSettingsEventRow::get_deadzone() const {
	return deadzone;
}

void InputMapSettingsEventRow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InputMapSettingsEventRow::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InputMapSettingsEventRow::get_action);
	ClassDB::bind_method(D_METHOD("set_index", "index"), &InputMapSettingsEventRow::set_index);
	ClassDB::bind_method(D_METHOD("get_index"), &InputMapSettingsEventRow::get_index);
	ClassDB::bind_method(D_METHOD("set_device", "device"), &InputMapSettingsEventRow::set_device);
	ClassDB::bind_method(D_METHOD("get_device"), &InputMapSettingsEventRow::get_device);
	ClassDB::bind_method(D_METHOD("set_input", "input"), &InputMapSettingsEventRow::set_input);
	ClassDB::bind_method(D_METHOD("get_input"), &InputMapSettingsEventRow::get_input);
	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &InputMapSettingsEventRow::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &InputMapSettingsEventRow::get_direction);
	ClassDB::bind_method(D_METHOD("set_modifiers", "modifiers"), &InputMapSettingsEventRow::set_modifiers);
	ClassDB::bind_method(D_METHOD("get_modifiers"), &InputMapSettingsEventRow::get_modifiers);
	ClassDB::bind_method(D_METHOD("set_key_mode", "key_mode"), &InputMapSettingsEventRow::set_key_mode);
	ClassDB::bind_method(D_METHOD("get_key_mode"), &InputMapSettingsEventRow::get_key_mode);
	ClassDB::bind_method(D_METHOD("set_device_id", "device_id"), &InputMapSettingsEventRow::set_device_id);
	ClassDB::bind_method(D_METHOD("get_device_id"), &InputMapSettingsEventRow::get_device_id);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &InputMapSettingsEventRow::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &InputMapSettingsEventRow::get_deadzone);

	// The column names of the CSV file, in this order.
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "index"), "set_index", "get_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "device", PROPERTY_HINT_ENUM, "Keyboard,Mouse,Gamepad"), "set_device", "get_device");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "input"), "set_input", "get_input");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modifiers"), "set_modifiers", "get_modifiers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "key_mode", PROPERTY_HINT_ENUM, "Physical,Label"), "set_key_mode", "get_key_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_id"), "set_device_id", "get_device_id");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
}

void InputMapSettingsEventRows::set_actions(const TypedArray<InputMapSettingsEventRow> &p_actions) {
	actions = p_actions;
}

TypedArray<InputMapSettingsEventRow> InputMapSettingsEventRows::get_actions() const {
	return actions;
}

void InputMapSettingsEventRows::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_actions", "actions"), &InputMapSettingsEventRows::set_actions);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMapSettingsEventRows::get_actions);

	// The name of InputActions.actions, the list property of the settings.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "actions", PROPERTY_HINT_ARRAY_TYPE, "InputMapSettingsEventRow"), "set_actions", "get_actions");
}

/////////////////////////////////////////////////////////////////////////////
// InputMapSettingsRow and InputMapSettingsRows (version 2 files, read only).

void InputMapSettingsRow::set_action(const StringName &p_action) {
	action = p_action;
}

StringName InputMapSettingsRow::get_action() const {
	return action;
}

void InputMapSettingsRow::set_deadzone(float p_deadzone) {
	deadzone = p_deadzone;
}

float InputMapSettingsRow::get_deadzone() const {
	return deadzone;
}

void InputMapSettingsRow::set_bindings(const String &p_bindings) {
	bindings = p_bindings;
}

String InputMapSettingsRow::get_bindings() const {
	return bindings;
}

void InputMapSettingsRow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InputMapSettingsRow::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InputMapSettingsRow::get_action);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &InputMapSettingsRow::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &InputMapSettingsRow::get_deadzone);
	ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &InputMapSettingsRow::set_bindings);
	ClassDB::bind_method(D_METHOD("get_bindings"), &InputMapSettingsRow::get_bindings);

	// The column names of the version 2 CSV file.
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bindings"), "set_bindings", "get_bindings");
}

void InputMapSettingsRows::set_actions(const TypedArray<InputMapSettingsRow> &p_actions) {
	actions = p_actions;
}

TypedArray<InputMapSettingsRow> InputMapSettingsRows::get_actions() const {
	return actions;
}

void InputMapSettingsRows::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_actions", "actions"), &InputMapSettingsRows::set_actions);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMapSettingsRows::get_actions);

	// The name of InputActions.actions, the list property of the settings.
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "actions", PROPERTY_HINT_ARRAY_TYPE, "InputMapSettingsRow"), "set_actions", "get_actions");
}
