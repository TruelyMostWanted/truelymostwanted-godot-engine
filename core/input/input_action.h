/**************************************************************************/
/*  input_action.h                                                        */
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

// One input action of InputMap as a resource (#172): its name, deadzone and events.
// `events` is the list for scripts and the Inspector; `bindings` is its stored text form
// (`Key:Ctrl+S; JoypadButton:A`), so the resource can be saved as JSON, INI, CSV and `.tres`.
// Only keys, mouse buttons, joypad buttons and joypad axes have a text form.
class InputAction : public Resource {
	GDCLASS(InputAction, Resource);

public:
	enum InputDevice {
		INPUT_DEVICE_KEYBOARD,
		INPUT_DEVICE_MOUSE,
		INPUT_DEVICE_GAMEPAD,
		INPUT_DEVICE_MAX,
	};

	static constexpr float DEFAULT_DEADZONE = 0.2f;

private:
	StringName action;
	float deadzone = DEFAULT_DEADZONE;
	Vector<Ref<InputEvent>> events;

protected:
	static void _bind_methods();

public:
	// Text form of an event (`Key:Ctrl+S`, `PhysicalKey:W`, `MouseButton:Left`, `JoypadButton:A`,
	// `JoypadMotion:LeftX+`); empty for event types without a text form (touch, MIDI, ...).
	static String event_to_text(const Ref<InputEvent> &p_event);
	// Event of a text form; null if the text is not valid. The device of the event is -1 (all devices).
	static Ref<InputEvent> text_to_event(const String &p_text);
	// Device of a bindable event; INPUT_DEVICE_MAX for event types that are not bindable.
	static InputDevice get_event_device(const Ref<InputEvent> &p_event);

	void set_action(const StringName &p_action);
	StringName get_action() const;

	void set_deadzone(float p_deadzone);
	float get_deadzone() const;

	void set_events(const TypedArray<InputEvent> &p_events);
	TypedArray<InputEvent> get_events() const;
	const Vector<Ref<InputEvent>> &get_event_list() const { return events; }

	void set_bindings(const String &p_bindings);
	String get_bindings() const;

	void add_event(const Ref<InputEvent> &p_event);
	bool has_event(const Ref<InputEvent> &p_event) const;
	bool remove_event(const Ref<InputEvent> &p_event);
	void clear_events();
	int get_event_count() const;

	TypedArray<InputEvent> get_device_events(InputDevice p_device) const;

	// For InputMap, which checks duplicates itself (with the device of the event): no check here.
	void push_event(const Ref<InputEvent> &p_event);
	void remove_event_at(int p_index);

	InputAction() {}
	InputAction(const StringName &p_action, float p_deadzone = DEFAULT_DEADZONE);
};

VARIANT_ENUM_CAST(InputAction::InputDevice);
