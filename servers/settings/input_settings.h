/**************************************************************************/
/*  input_settings.h                                                      */
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

#include "core/io/resource.h"

// Data of the `input` settings domain (#19). No engine dependency: InputSettingsServer pushes the three
// Input options and offers helpers for the game-side values (sensitivity, inversion, deadzone, ...).
class InputSettings : public Resource {
	GDCLASS(InputSettings, Resource);

	float keyboard_sensitivity = 1.0;
	bool keyboard_x_inverted = false;
	bool keyboard_y_inverted = false;

	float mouse_sensitivity = 1.0;
	bool mouse_x_inverted = false;
	bool mouse_y_inverted = false;
	float mouse_smoothing = 0.0;
	bool mouse_confined = false;

	float gamepad_sensitivity = 1.0;
	bool gamepad_x_inverted = false;
	bool gamepad_y_inverted = false;
	float gamepad_deadzone = 0.2;
	bool vibration_enabled = true;
	float vibration_strength = 1.0;

	PackedStringArray toggle_actions;
	int first_player_device_id = -1; // -1: any device.
	bool allow_keyboard_for_current_player = true;

	// Input options; the defaults are read from Input when the domain starts.
	bool emulate_mouse_from_touch = true;
	bool emulate_touch_from_mouse = false;
	bool use_accumulated_input = true;

protected:
	static void _bind_methods();

public:
	void set_keyboard_sensitivity(float p_sensitivity);
	float get_keyboard_sensitivity() const;
	void set_keyboard_x_inverted(bool p_inverted);
	bool is_keyboard_x_inverted() const;
	void set_keyboard_y_inverted(bool p_inverted);
	bool is_keyboard_y_inverted() const;

	void set_mouse_sensitivity(float p_sensitivity);
	float get_mouse_sensitivity() const;
	void set_mouse_x_inverted(bool p_inverted);
	bool is_mouse_x_inverted() const;
	void set_mouse_y_inverted(bool p_inverted);
	bool is_mouse_y_inverted() const;
	void set_mouse_smoothing(float p_smoothing);
	float get_mouse_smoothing() const;
	void set_mouse_confined(bool p_confined);
	bool is_mouse_confined() const;

	void set_gamepad_sensitivity(float p_sensitivity);
	float get_gamepad_sensitivity() const;
	void set_gamepad_x_inverted(bool p_inverted);
	bool is_gamepad_x_inverted() const;
	void set_gamepad_y_inverted(bool p_inverted);
	bool is_gamepad_y_inverted() const;
	void set_gamepad_deadzone(float p_deadzone);
	float get_gamepad_deadzone() const;
	void set_vibration_enabled(bool p_enabled);
	bool is_vibration_enabled() const;
	void set_vibration_strength(float p_strength);
	float get_vibration_strength() const;

	void set_toggle_actions(const PackedStringArray &p_actions);
	PackedStringArray get_toggle_actions() const;
	void set_first_player_device_id(int p_device);
	int get_first_player_device_id() const;
	void set_allow_keyboard_for_current_player(bool p_allow);
	bool is_keyboard_allowed_for_current_player() const;

	void set_emulate_mouse_from_touch(bool p_emulate);
	bool is_emulating_mouse_from_touch() const;
	void set_emulate_touch_from_mouse(bool p_emulate);
	bool is_emulating_touch_from_mouse() const;
	void set_use_accumulated_input(bool p_enable);
	bool is_using_accumulated_input() const;
};
