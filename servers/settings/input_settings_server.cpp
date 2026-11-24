/**************************************************************************/
/*  input_settings_server.cpp                                             */
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

#include "input_settings_server.h"

#include "core/config/engine.h"
#include "core/input/input.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/settings/input_settings.h"

InputSettingsServer *InputSettingsServer::singleton = nullptr;

InputSettingsServer *InputSettingsServer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets. Without Input (headless tools, unit tests) nothing is pushed and the project defaults are read.

void InputSettingsServer::_push_emulate_mouse_from_touch(bool p_emulate) {
	Input *input = Input::get_singleton();
	if (input != nullptr) {
		input->set_emulate_mouse_from_touch(p_emulate);
	}
}

bool InputSettingsServer::_pull_emulate_mouse_from_touch() {
	const Input *input = Input::get_singleton();
	return input != nullptr ? input->is_emulating_mouse_from_touch() : true;
}

void InputSettingsServer::_push_emulate_touch_from_mouse(bool p_emulate) {
	Input *input = Input::get_singleton();
	if (input != nullptr) {
		input->set_emulate_touch_from_mouse(p_emulate);
	}
}

bool InputSettingsServer::_pull_emulate_touch_from_mouse() {
	const Input *input = Input::get_singleton();
	return input != nullptr ? input->is_emulating_touch_from_mouse() : false;
}

void InputSettingsServer::_push_use_accumulated_input(bool p_enable) {
	Input *input = Input::get_singleton();
	if (input != nullptr) {
		input->set_use_accumulated_input(p_enable);
	}
}

bool InputSettingsServer::_pull_use_accumulated_input() {
	const Input *input = Input::get_singleton();
	return input != nullptr ? input->is_using_accumulated_input() : true;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName InputSettingsServer::get_domain() const {
	return SNAME("input");
}

SettingsServer::ApplyPhase InputSettingsServer::get_apply_phase() const {
	// After Main::setup2() has set the touch emulation from the project settings, so those are the defaults.
	return APPLY_PHASE_SERVERS;
}

Ref<Resource> InputSettingsServer::_create_default_settings() const {
	Ref<InputSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

Variant InputSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (p_name == SNAME("toggle_actions")) {
		// Without empty names and duplicates; the order is kept.
		const PackedStringArray actions = value;
		PackedStringArray result;
		for (const String &action : actions) {
			const String name = action.strip_edges();
			if (!name.is_empty() && !result.has(name)) {
				result.push_back(name);
			}
		}
		return result;
	}
	return value;
}

void InputSettingsServer::_on_value_changed(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("toggle_actions")) {
		action_states.clear();
	} else if (p_name == SNAME("mouse_smoothing")) {
		smoothed_mouse_motion = Vector2();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

const char *InputSettingsServer::_get_device_prefix(Device p_device) {
	switch (p_device) {
		case DEVICE_KEYBOARD:
			return "keyboard";
		case DEVICE_MOUSE:
			return "mouse";
		case DEVICE_GAMEPAD:
			return "gamepad";
		default:
			return nullptr;
	}
}

StringName InputSettingsServer::_get_device_value(Device p_device, const char *p_suffix) {
	const char *prefix = _get_device_prefix(p_device);
	ERR_FAIL_NULL_V_MSG(prefix, StringName(), vformat("Invalid input settings device: %d.", int(p_device)));
	return StringName(String(prefix) + "_" + p_suffix);
}

void InputSettingsServer::set_sensitivity(Device p_device, float p_sensitivity) {
	const StringName name = _get_device_value(p_device, "sensitivity");
	if (name != StringName()) {
		set_value(name, p_sensitivity);
	}
}

float InputSettingsServer::get_sensitivity(Device p_device) const {
	const StringName name = _get_device_value(p_device, "sensitivity");
	return name != StringName() ? float(get_value(name)) : 1.0f;
}

void InputSettingsServer::set_x_inverted(Device p_device, bool p_inverted) {
	const StringName name = _get_device_value(p_device, "x_inverted");
	if (name != StringName()) {
		set_value(name, p_inverted);
	}
}

bool InputSettingsServer::is_x_inverted(Device p_device) const {
	const StringName name = _get_device_value(p_device, "x_inverted");
	return name != StringName() && bool(get_value(name));
}

void InputSettingsServer::set_y_inverted(Device p_device, bool p_inverted) {
	const StringName name = _get_device_value(p_device, "y_inverted");
	if (name != StringName()) {
		set_value(name, p_inverted);
	}
}

bool InputSettingsServer::is_y_inverted(Device p_device) const {
	const StringName name = _get_device_value(p_device, "y_inverted");
	return name != StringName() && bool(get_value(name));
}

void InputSettingsServer::set_mouse_smoothing(float p_smoothing) {
	set_value(SNAME("mouse_smoothing"), p_smoothing);
}

float InputSettingsServer::get_mouse_smoothing() const {
	return get_value(SNAME("mouse_smoothing"));
}

void InputSettingsServer::set_mouse_confined(bool p_confined) {
	set_value(SNAME("mouse_confined"), p_confined);
}

bool InputSettingsServer::is_mouse_confined() const {
	return get_value(SNAME("mouse_confined"));
}

void InputSettingsServer::set_gamepad_deadzone(float p_deadzone) {
	set_value(SNAME("gamepad_deadzone"), p_deadzone);
}

float InputSettingsServer::get_gamepad_deadzone() const {
	return get_value(SNAME("gamepad_deadzone"));
}

void InputSettingsServer::set_vibration_enabled(bool p_enabled) {
	set_value(SNAME("vibration_enabled"), p_enabled);
}

bool InputSettingsServer::is_vibration_enabled() const {
	return get_value(SNAME("vibration_enabled"));
}

void InputSettingsServer::set_vibration_strength(float p_strength) {
	set_value(SNAME("vibration_strength"), p_strength);
}

float InputSettingsServer::get_vibration_strength() const {
	return get_value(SNAME("vibration_strength"));
}

void InputSettingsServer::set_toggle_actions(const PackedStringArray &p_actions) {
	set_value(SNAME("toggle_actions"), p_actions);
}

PackedStringArray InputSettingsServer::get_toggle_actions() const {
	return get_value(SNAME("toggle_actions"));
}

void InputSettingsServer::set_first_player_device_id(int p_device) {
	set_value(SNAME("first_player_device_id"), p_device);
}

int InputSettingsServer::get_first_player_device_id() const {
	return get_value(SNAME("first_player_device_id"));
}

void InputSettingsServer::set_allow_keyboard_for_current_player(bool p_allow) {
	set_value(SNAME("allow_keyboard_for_current_player"), p_allow);
}

bool InputSettingsServer::is_keyboard_allowed_for_current_player() const {
	return get_value(SNAME("allow_keyboard_for_current_player"));
}

void InputSettingsServer::set_emulate_mouse_from_touch(bool p_emulate) {
	set_value(SNAME("emulate_mouse_from_touch"), p_emulate);
}

bool InputSettingsServer::is_emulating_mouse_from_touch() const {
	return get_value(SNAME("emulate_mouse_from_touch"));
}

void InputSettingsServer::set_emulate_touch_from_mouse(bool p_emulate) {
	set_value(SNAME("emulate_touch_from_mouse"), p_emulate);
}

bool InputSettingsServer::is_emulating_touch_from_mouse() const {
	return get_value(SNAME("emulate_touch_from_mouse"));
}

void InputSettingsServer::set_use_accumulated_input(bool p_enable) {
	set_value(SNAME("use_accumulated_input"), p_enable);
}

bool InputSettingsServer::is_using_accumulated_input() const {
	return get_value(SNAME("use_accumulated_input"));
}

/////////////////////////////////////////////////////////////////////////////
// Helpers. They read the pending values, so a slider in an options menu takes effect while it is dragged.

Vector2 InputSettingsServer::apply_to_vector(Device p_device, const Vector2 &p_vector) const {
	ERR_FAIL_INDEX_V(int(p_device), int(DEVICE_MAX), p_vector);
	Vector2 result = p_vector * get_sensitivity(p_device);
	if (is_x_inverted(p_device)) {
		result.x = -result.x;
	}
	if (is_y_inverted(p_device)) {
		result.y = -result.y;
	}
	return result;
}

Vector2 InputSettingsServer::apply_to_mouse_motion(const Vector2 &p_relative) {
	const Vector2 motion = apply_to_vector(DEVICE_MOUSE, p_relative);
	const float smoothing = get_mouse_smoothing();
	if (smoothing <= 0.0f) {
		smoothed_mouse_motion = motion;
		return motion;
	}
	// Exponential smoothing over the calls: 0 is off, 0.95 is very smooth.
	smoothed_mouse_motion = motion.lerp(smoothed_mouse_motion, smoothing);
	return smoothed_mouse_motion;
}

void InputSettingsServer::reset_mouse_smoothing() {
	smoothed_mouse_motion = Vector2();
}

Vector2 InputSettingsServer::apply_to_stick(const Vector2 &p_axis) const {
	// Radial deadzone, rescaled so that the output starts at 0 at the edge of the deadzone and reaches 1.
	const float deadzone = CLAMP(get_gamepad_deadzone(), 0.0f, 0.95f);
	const float length = p_axis.length();
	if (length <= deadzone || length <= CMP_EPSILON) {
		return Vector2();
	}
	const float scaled = MIN((length - deadzone) / (1.0f - deadzone), 1.0f);
	return apply_to_vector(DEVICE_GAMEPAD, p_axis / length * scaled);
}

Vector2 InputSettingsServer::get_look_vector(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid()) {
		// The motion on the screen: independent of the stretch settings of the window.
		return apply_to_mouse_motion(mouse_motion->get_relative_screen_position());
	}
	const Ref<InputEventJoypadMotion> joypad_motion = p_event;
	if (joypad_motion.is_valid() && (joypad_motion->get_axis() == JoyAxis::RIGHT_X || joypad_motion->get_axis() == JoyAxis::RIGHT_Y)) {
		// The whole right stick, so the deadzone is radial.
		Vector2 axis;
		const Input *input = Input::get_singleton();
		if (input != nullptr) {
			axis = Vector2(input->get_joy_axis(joypad_motion->get_device(), JoyAxis::RIGHT_X), input->get_joy_axis(joypad_motion->get_device(), JoyAxis::RIGHT_Y));
		}
		if (joypad_motion->get_axis() == JoyAxis::RIGHT_X) {
			axis.x = joypad_motion->get_axis_value();
		} else {
			axis.y = joypad_motion->get_axis_value();
		}
		return apply_to_stick(axis);
	}
	return Vector2();
}

bool InputSettingsServer::is_player_event(const Ref<InputEvent> &p_event) const {
	ERR_FAIL_COND_V(p_event.is_null(), false);
	if (Object::cast_to<InputEventJoypadButton>(p_event.ptr()) != nullptr || Object::cast_to<InputEventJoypadMotion>(p_event.ptr()) != nullptr) {
		const int device = get_first_player_device_id();
		return device < 0 || p_event->get_device() == device;
	}
	if (Object::cast_to<InputEventAction>(p_event.ptr()) != nullptr) {
		return true;
	}
	// Keyboard, mouse, touch and gestures.
	return is_keyboard_allowed_for_current_player();
}

void InputSettingsServer::vibrate(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration) {
	if (!is_vibration_enabled()) {
		return;
	}
	Input *input = Input::get_singleton();
	if (input == nullptr) {
		return;
	}
	const float strength = get_vibration_strength();
	if (strength <= 0.0f) {
		return;
	}
	input->start_joy_vibration(p_device, CLAMP(p_weak_magnitude * strength, 0.0f, 1.0f), CLAMP(p_strong_magnitude * strength, 0.0f, 1.0f), p_duration);
}

bool InputSettingsServer::is_toggle_action(const StringName &p_action) const {
	return get_toggle_actions().has(String(p_action));
}

bool InputSettingsServer::is_action_active(const StringName &p_action) {
	const Input *input = Input::get_singleton();
	if (input == nullptr) {
		return false;
	}
	if (!is_toggle_action(p_action)) {
		return input->is_action_pressed(p_action);
	}
	// A toggle flips once per press. The frame is remembered, so several calls in one frame see the same state.
	const Engine *engine = Engine::get_singleton();
	const bool physics = engine->is_in_physics_frame();
	const uint64_t frame = physics ? engine->get_physics_frames() : engine->get_process_frames();
	ActionState &state = action_states[p_action];
	if (state.frame != frame || state.physics != physics) {
		state.frame = frame;
		state.physics = physics;
		if (input->is_action_just_pressed(p_action)) {
			state.active = !state.active;
		}
	}
	return state.active;
}

void InputSettingsServer::reset_action_states() {
	action_states.clear();
}

void InputSettingsServer::set_mouse_captured(bool p_captured) {
	Input *input = Input::get_singleton();
	if (input == nullptr) {
		return;
	}
	if (p_captured) {
		input->set_mouse_mode(Input::MouseMode::MOUSE_MODE_CAPTURED);
	} else {
		input->set_mouse_mode(is_mouse_confined() ? Input::MouseMode::MOUSE_MODE_CONFINED : Input::MouseMode::MOUSE_MODE_VISIBLE);
	}
}

void InputSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sensitivity", "device", "sensitivity"), &InputSettingsServer::set_sensitivity);
	ClassDB::bind_method(D_METHOD("get_sensitivity", "device"), &InputSettingsServer::get_sensitivity);
	ClassDB::bind_method(D_METHOD("set_x_inverted", "device", "inverted"), &InputSettingsServer::set_x_inverted);
	ClassDB::bind_method(D_METHOD("is_x_inverted", "device"), &InputSettingsServer::is_x_inverted);
	ClassDB::bind_method(D_METHOD("set_y_inverted", "device", "inverted"), &InputSettingsServer::set_y_inverted);
	ClassDB::bind_method(D_METHOD("is_y_inverted", "device"), &InputSettingsServer::is_y_inverted);

	ClassDB::bind_method(D_METHOD("set_mouse_smoothing", "smoothing"), &InputSettingsServer::set_mouse_smoothing);
	ClassDB::bind_method(D_METHOD("get_mouse_smoothing"), &InputSettingsServer::get_mouse_smoothing);
	ClassDB::bind_method(D_METHOD("set_mouse_confined", "confined"), &InputSettingsServer::set_mouse_confined);
	ClassDB::bind_method(D_METHOD("is_mouse_confined"), &InputSettingsServer::is_mouse_confined);
	ClassDB::bind_method(D_METHOD("set_gamepad_deadzone", "deadzone"), &InputSettingsServer::set_gamepad_deadzone);
	ClassDB::bind_method(D_METHOD("get_gamepad_deadzone"), &InputSettingsServer::get_gamepad_deadzone);
	ClassDB::bind_method(D_METHOD("set_vibration_enabled", "enabled"), &InputSettingsServer::set_vibration_enabled);
	ClassDB::bind_method(D_METHOD("is_vibration_enabled"), &InputSettingsServer::is_vibration_enabled);
	ClassDB::bind_method(D_METHOD("set_vibration_strength", "strength"), &InputSettingsServer::set_vibration_strength);
	ClassDB::bind_method(D_METHOD("get_vibration_strength"), &InputSettingsServer::get_vibration_strength);

	ClassDB::bind_method(D_METHOD("set_toggle_actions", "actions"), &InputSettingsServer::set_toggle_actions);
	ClassDB::bind_method(D_METHOD("get_toggle_actions"), &InputSettingsServer::get_toggle_actions);
	ClassDB::bind_method(D_METHOD("set_first_player_device_id", "device"), &InputSettingsServer::set_first_player_device_id);
	ClassDB::bind_method(D_METHOD("get_first_player_device_id"), &InputSettingsServer::get_first_player_device_id);
	ClassDB::bind_method(D_METHOD("set_allow_keyboard_for_current_player", "allow"), &InputSettingsServer::set_allow_keyboard_for_current_player);
	ClassDB::bind_method(D_METHOD("is_keyboard_allowed_for_current_player"), &InputSettingsServer::is_keyboard_allowed_for_current_player);

	ClassDB::bind_method(D_METHOD("set_emulate_mouse_from_touch", "emulate"), &InputSettingsServer::set_emulate_mouse_from_touch);
	ClassDB::bind_method(D_METHOD("is_emulating_mouse_from_touch"), &InputSettingsServer::is_emulating_mouse_from_touch);
	ClassDB::bind_method(D_METHOD("set_emulate_touch_from_mouse", "emulate"), &InputSettingsServer::set_emulate_touch_from_mouse);
	ClassDB::bind_method(D_METHOD("is_emulating_touch_from_mouse"), &InputSettingsServer::is_emulating_touch_from_mouse);
	ClassDB::bind_method(D_METHOD("set_use_accumulated_input", "enable"), &InputSettingsServer::set_use_accumulated_input);
	ClassDB::bind_method(D_METHOD("is_using_accumulated_input"), &InputSettingsServer::is_using_accumulated_input);

	ClassDB::bind_method(D_METHOD("apply_to_vector", "device", "vector"), &InputSettingsServer::apply_to_vector);
	ClassDB::bind_method(D_METHOD("apply_to_mouse_motion", "relative"), &InputSettingsServer::apply_to_mouse_motion);
	ClassDB::bind_method(D_METHOD("reset_mouse_smoothing"), &InputSettingsServer::reset_mouse_smoothing);
	ClassDB::bind_method(D_METHOD("apply_to_stick", "axis"), &InputSettingsServer::apply_to_stick);
	ClassDB::bind_method(D_METHOD("get_look_vector", "event"), &InputSettingsServer::get_look_vector);
	ClassDB::bind_method(D_METHOD("is_player_event", "event"), &InputSettingsServer::is_player_event);
	ClassDB::bind_method(D_METHOD("vibrate", "device", "weak_magnitude", "strong_magnitude", "duration"), &InputSettingsServer::vibrate, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_toggle_action", "action"), &InputSettingsServer::is_toggle_action);
	ClassDB::bind_method(D_METHOD("is_action_active", "action"), &InputSettingsServer::is_action_active);
	ClassDB::bind_method(D_METHOD("reset_action_states"), &InputSettingsServer::reset_action_states);
	ClassDB::bind_method(D_METHOD("set_mouse_captured", "captured"), &InputSettingsServer::set_mouse_captured);

	BIND_ENUM_CONSTANT(DEVICE_KEYBOARD);
	BIND_ENUM_CONSTANT(DEVICE_MOUSE);
	BIND_ENUM_CONSTANT(DEVICE_GAMEPAD);
	BIND_ENUM_CONSTANT(DEVICE_MAX);
}

InputSettingsServer::InputSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	bind_setting_callbacks(SNAME("emulate_mouse_from_touch"), callable_mp_static(&InputSettingsServer::_push_emulate_mouse_from_touch), callable_mp_static(&InputSettingsServer::_pull_emulate_mouse_from_touch));
	bind_setting_callbacks(SNAME("emulate_touch_from_mouse"), callable_mp_static(&InputSettingsServer::_push_emulate_touch_from_mouse), callable_mp_static(&InputSettingsServer::_pull_emulate_touch_from_mouse));
	bind_setting_callbacks(SNAME("use_accumulated_input"), callable_mp_static(&InputSettingsServer::_push_use_accumulated_input), callable_mp_static(&InputSettingsServer::_pull_use_accumulated_input));

	// Game-side values: stored, validated and signaled, never pushed to the engine.
	static const char *game_values[] = {
		"keyboard_sensitivity", "keyboard_x_inverted", "keyboard_y_inverted",
		"mouse_sensitivity", "mouse_x_inverted", "mouse_y_inverted", "mouse_smoothing", "mouse_confined",
		"gamepad_sensitivity", "gamepad_x_inverted", "gamepad_y_inverted", "gamepad_deadzone",
		"vibration_enabled", "vibration_strength",
		"toggle_actions", "first_player_device_id", "allow_keyboard_for_current_player",
		nullptr
	};
	for (int i = 0; game_values[i] != nullptr; i++) {
		set_setting_flags(StringName(game_values[i]), SETTING_FLAG_NO_ENGINE_TARGET);
	}

	connect(SNAME("value_changed"), callable_mp(this, &InputSettingsServer::_on_value_changed));
}

InputSettingsServer::~InputSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
