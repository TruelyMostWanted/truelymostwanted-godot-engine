/**************************************************************************/
/*  input_settings_server.h                                               */
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
#include "core/templates/hash_map.h"
#include "servers/settings/settings_server.h"

// Settings domain `input` (#19): the player's input options (InputSettings). Three values reach Input at
// once (touch/mouse emulation, accumulated input); the others have no engine target and are only stored.
// The game applies them through opt-in helpers (apply_to_mouse_motion(), apply_to_stick(), vibrate(), ...);
// Input itself is never changed, so the GUI and games that scale on their own are not affected.
class InputSettingsServer : public SettingsServer {
	GDCLASS(InputSettingsServer, SettingsServer);

public:
	enum Device {
		DEVICE_KEYBOARD,
		DEVICE_MOUSE,
		DEVICE_GAMEPAD,
		DEVICE_MAX,
	};

private:
	static InputSettingsServer *singleton;

	struct ActionState {
		bool active = false;
		bool physics = false;
		uint64_t frame = UINT64_MAX;
	};

	// Toggle state per action (is_action_active()) and the last output of apply_to_mouse_motion().
	HashMap<StringName, ActionState> action_states;
	Vector2 smoothed_mouse_motion;

	static const char *_get_device_prefix(Device p_device);
	static StringName _get_device_value(Device p_device, const char *p_suffix);

	static void _push_emulate_mouse_from_touch(bool p_emulate);
	static bool _pull_emulate_mouse_from_touch();
	static void _push_emulate_touch_from_mouse(bool p_emulate);
	static bool _pull_emulate_touch_from_mouse();
	static void _push_use_accumulated_input(bool p_enable);
	static bool _pull_use_accumulated_input();

	void _on_value_changed(const StringName &p_name, const Variant &p_value);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static InputSettingsServer *get_singleton();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;

	// Per device.
	void set_sensitivity(Device p_device, float p_sensitivity);
	float get_sensitivity(Device p_device) const;
	void set_x_inverted(Device p_device, bool p_inverted);
	bool is_x_inverted(Device p_device) const;
	void set_y_inverted(Device p_device, bool p_inverted);
	bool is_y_inverted(Device p_device) const;

	void set_mouse_smoothing(float p_smoothing);
	float get_mouse_smoothing() const;
	void set_mouse_confined(bool p_confined);
	bool is_mouse_confined() const;
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

	// Helpers (opt-in).
	Vector2 apply_to_vector(Device p_device, const Vector2 &p_vector) const;
	Vector2 apply_to_mouse_motion(const Vector2 &p_relative);
	void reset_mouse_smoothing();
	Vector2 apply_to_stick(const Vector2 &p_axis) const;
	Vector2 get_look_vector(const Ref<InputEvent> &p_event);
	bool is_player_event(const Ref<InputEvent> &p_event) const;
	void vibrate(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration = 0);
	bool is_toggle_action(const StringName &p_action) const;
	bool is_action_active(const StringName &p_action);
	void reset_action_states();
	void set_mouse_captured(bool p_captured);

	InputSettingsServer();
	~InputSettingsServer();
};

VARIANT_ENUM_CAST(InputSettingsServer::Device);
