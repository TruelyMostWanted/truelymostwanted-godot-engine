/**************************************************************************/
/*  input_map_settings_records.h                                          */
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

#pragma once

#include "core/input/input_event.h"
#include "core/io/resource.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"

// Storage forms of InputMapSettingsServer (#193), internal: the settings file holds records with named fields
// instead of the InputEvent sub-resources, so every settings text format (JSON, INI, CSV, XML, TOML, YAML)
// stays readable and editable. Not used outside the settings file.

// One bindable event: key, mouse button, joypad button or joypad axis.
class InputMapSettingsEvent : public Resource {
	GDCLASS(InputMapSettingsEvent, Resource);

public:
	enum Device {
		DEVICE_KEYBOARD,
		DEVICE_MOUSE,
		DEVICE_GAMEPAD,
	};

	enum KeyMode {
		KEY_MODE_PHYSICAL,
		KEY_MODE_LABEL,
		KEY_MODE_PRINTED,
	};

private:
	Device device = DEVICE_KEYBOARD;
	String input; // `W`, `Space`, `Left`, `WheelUp`, `A`, `LeftStickLeft`: a gamepad axis has its direction in the name.
	String modifiers; // `Ctrl+Shift` (keyboard and mouse), empty otherwise.
	KeyMode key_mode = KEY_MODE_PHYSICAL;
	String device_id = "Any"; // `Any` or the number of one joypad.

	String _compose_text(const String &p_gamepad_type) const;

protected:
	static void _bind_methods();

public:
	// The record of an event; null for event types without a record (touch, MIDI, ...).
	static Ref<InputMapSettingsEvent> from_event(const Ref<InputEvent> &p_event);
	// The record of a text form (`Keyboard:Ctrl+S`, `Device0:Gamepad:A`, `GamepadAxis:LeftStickLeft`; every form
	// InputAction::text_to_event() reads, the names of version 2 files too); null if not valid.
	static Ref<InputMapSettingsEvent> from_text(const String &p_text);

	// The event; null if the record is not valid. The device of the event is -1 (all devices) unless a joypad is set.
	Ref<InputEvent> to_event() const;
	String to_text() const;
	// The joypad number of `device_id`, -1 for `Any`.
	int get_device_number() const;

	void set_device(Device p_device);
	Device get_device() const;
	void set_input(const String &p_input);
	String get_input() const;
	void set_modifiers(const String &p_modifiers);
	String get_modifiers() const;
	void set_key_mode(KeyMode p_key_mode);
	KeyMode get_key_mode() const;
	void set_device_id(const String &p_device_id);
	String get_device_id() const;
};

VARIANT_ENUM_CAST(InputMapSettingsEvent::Device);
VARIANT_ENUM_CAST(InputMapSettingsEvent::KeyMode);

// One action with its events.
class InputMapSettingsAction : public Resource {
	GDCLASS(InputMapSettingsAction, Resource);

public:
	enum Layer {
		LAYER_BUILT_IN,
		LAYER_CUSTOM,
	};

private:
	StringName action;
	float deadzone = 0.2f;
	Layer layer = LAYER_CUSTOM;
	TypedArray<InputMapSettingsEvent> input_events;

protected:
	static void _bind_methods();

public:
	void set_action(const StringName &p_action);
	StringName get_action() const;
	void set_deadzone(float p_deadzone);
	float get_deadzone() const;
	void set_layer(Layer p_layer);
	Layer get_layer() const;
	void set_input_events(const TypedArray<InputMapSettingsEvent> &p_input_events);
	TypedArray<InputMapSettingsEvent> get_input_events() const;
};

VARIANT_ENUM_CAST(InputMapSettingsAction::Layer);

// The file form of InputActions (JSON, INI, XML, TOML, YAML): one InputMapSettingsAction per action.
class InputMapSettingsActions : public Resource {
	GDCLASS(InputMapSettingsActions, Resource);

	TypedArray<InputMapSettingsAction> actions;

protected:
	static void _bind_methods();

public:
	void set_actions(const TypedArray<InputMapSettingsAction> &p_actions);
	TypedArray<InputMapSettingsAction> get_actions() const;
};

// One event of one action as a flat CSV row. An action without events is one row with an empty `input`.
class InputMapSettingsEventRow : public Resource {
	GDCLASS(InputMapSettingsEventRow, Resource);

	StringName action;
	int index = 0; // Position of the event in the action, -1 for an action without events.
	InputMapSettingsEvent::Device device = InputMapSettingsEvent::DEVICE_KEYBOARD;
	String input;
	String modifiers;
	InputMapSettingsEvent::KeyMode key_mode = InputMapSettingsEvent::KEY_MODE_PHYSICAL;
	String device_id = "Any";
	float deadzone = 0.2f;

protected:
	static void _bind_methods();

public:
	void set_action(const StringName &p_action);
	StringName get_action() const;
	void set_index(int p_index);
	int get_index() const;
	void set_device(InputMapSettingsEvent::Device p_device);
	InputMapSettingsEvent::Device get_device() const;
	void set_input(const String &p_input);
	String get_input() const;
	void set_modifiers(const String &p_modifiers);
	String get_modifiers() const;
	void set_key_mode(InputMapSettingsEvent::KeyMode p_key_mode);
	InputMapSettingsEvent::KeyMode get_key_mode() const;
	void set_device_id(const String &p_device_id);
	String get_device_id() const;
	void set_deadzone(float p_deadzone);
	float get_deadzone() const;
};

// The file form of InputActions in CSV: one InputMapSettingsEventRow per event. The list is called `actions`
// like the list of the settings, which the settings server reads by that name.
class InputMapSettingsEventRows : public Resource {
	GDCLASS(InputMapSettingsEventRows, Resource);

	TypedArray<InputMapSettingsEventRow> actions;

protected:
	static void _bind_methods();

public:
	void set_actions(const TypedArray<InputMapSettingsEventRow> &p_actions);
	TypedArray<InputMapSettingsEventRow> get_actions() const;
};

// Version 2 files (read only): one row per action with the events as one text cell (`Key:Ctrl+S; JoypadButton:A@0`).
class InputMapSettingsRow : public Resource {
	GDCLASS(InputMapSettingsRow, Resource);

	StringName action;
	float deadzone = 0.2f;
	String bindings;

protected:
	static void _bind_methods();

public:
	void set_action(const StringName &p_action);
	StringName get_action() const;
	void set_deadzone(float p_deadzone);
	float get_deadzone() const;
	void set_bindings(const String &p_bindings);
	String get_bindings() const;
};

class InputMapSettingsRows : public Resource {
	GDCLASS(InputMapSettingsRows, Resource);

	TypedArray<InputMapSettingsRow> actions;

protected:
	static void _bind_methods();

public:
	void set_actions(const TypedArray<InputMapSettingsRow> &p_actions);
	TypedArray<InputMapSettingsRow> get_actions() const;
};
