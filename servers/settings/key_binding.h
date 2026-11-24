/**************************************************************************/
/*  key_binding.h                                                         */
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
#include "core/variant/typed_array.h"

// Data of the `key_bindings` settings domain (#18). One KeyBinding is one slot of an action: the key,
// mouse button or joypad input in a readable text form (`Key:Ctrl+S`, `MouseButton:Left`,
// `JoypadButton:A`, `JoypadMotion:LeftX+`). The text form is stable and English, so the saved file can
// be read and edited by hand and survives JSON, INI and CSV.
class KeyBinding : public Resource {
	GDCLASS(KeyBinding, Resource);

public:
	enum InputDevice {
		INPUT_DEVICE_KEYBOARD,
		INPUT_DEVICE_MOUSE,
		INPUT_DEVICE_GAMEPAD,
		INPUT_DEVICE_MAX,
	};

	static constexpr float DEFAULT_DEADZONE = 0.2f;

private:
	String action;
	InputDevice device = INPUT_DEVICE_KEYBOARD;
	int slot = 0;
	String event;
	int device_id = -1;
	float deadzone = DEFAULT_DEADZONE;

protected:
	static void _bind_methods();

public:
	// Text form of an event; empty for event types that are not bindable (touch, MIDI, ...).
	static String event_to_text(const Ref<InputEvent> &p_event);
	// Event of a text form; null if the text is not valid. The device of the event is -1 (all devices).
	static Ref<InputEvent> text_to_event(const String &p_text);
	// Device of a bindable event; INPUT_DEVICE_MAX for event types that are not bindable.
	static InputDevice get_event_device(const Ref<InputEvent> &p_event);
	// Short text for the UI in English: "Ctrl+S", "Left Mouse Button", "Joypad A", "Left Stick Right".
	// Physical keys show the key of the current keyboard layout where the platform reports it.
	static String event_to_label(const Ref<InputEvent> &p_event);

	void set_action(const String &p_action);
	String get_action() const;
	void set_device(InputDevice p_device);
	InputDevice get_device() const;
	void set_slot(int p_slot);
	int get_slot() const;
	void set_event(const String &p_event);
	String get_event() const;
	void set_device_id(int p_device_id);
	int get_device_id() const;
	void set_deadzone(float p_deadzone);
	float get_deadzone() const;

	// The event of `event` with `device_id`; null if the slot is empty or the text is not valid.
	Ref<InputEvent> get_input_event() const;
	// Sets `event`, `device` and `device_id` from an event.
	void set_input_event(const Ref<InputEvent> &p_event);
	bool is_empty() const;
};

// Settings of the `key_bindings` domain: one KeyBinding per bound slot of all actions. It holds
// nothing but the list, so it can be stored as CSV (one row per event).
class KeyBindingList : public Resource {
	GDCLASS(KeyBindingList, Resource);

	TypedArray<KeyBinding> bindings;

protected:
	static void _bind_methods();

public:
	void set_bindings(const TypedArray<KeyBinding> &p_bindings);
	TypedArray<KeyBinding> get_bindings() const;
};

VARIANT_ENUM_CAST(KeyBinding::InputDevice);
