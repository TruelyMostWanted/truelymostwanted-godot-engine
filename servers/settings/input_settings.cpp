/**************************************************************************/
/*  input_settings.cpp                                                    */
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

#include "input_settings.h"

#include "core/object/class_db.h"

void InputSettings::set_keyboard_sensitivity(float p_sensitivity) {
	keyboard_sensitivity = p_sensitivity;
	emit_changed();
}

float InputSettings::get_keyboard_sensitivity() const {
	return keyboard_sensitivity;
}

void InputSettings::set_keyboard_x_inverted(bool p_inverted) {
	keyboard_x_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_keyboard_x_inverted() const {
	return keyboard_x_inverted;
}

void InputSettings::set_keyboard_y_inverted(bool p_inverted) {
	keyboard_y_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_keyboard_y_inverted() const {
	return keyboard_y_inverted;
}

void InputSettings::set_mouse_sensitivity(float p_sensitivity) {
	mouse_sensitivity = p_sensitivity;
	emit_changed();
}

float InputSettings::get_mouse_sensitivity() const {
	return mouse_sensitivity;
}

void InputSettings::set_mouse_x_inverted(bool p_inverted) {
	mouse_x_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_mouse_x_inverted() const {
	return mouse_x_inverted;
}

void InputSettings::set_mouse_y_inverted(bool p_inverted) {
	mouse_y_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_mouse_y_inverted() const {
	return mouse_y_inverted;
}

void InputSettings::set_mouse_smoothing(float p_smoothing) {
	mouse_smoothing = p_smoothing;
	emit_changed();
}

float InputSettings::get_mouse_smoothing() const {
	return mouse_smoothing;
}

void InputSettings::set_mouse_confined(bool p_confined) {
	mouse_confined = p_confined;
	emit_changed();
}

bool InputSettings::is_mouse_confined() const {
	return mouse_confined;
}

void InputSettings::set_gamepad_sensitivity(float p_sensitivity) {
	gamepad_sensitivity = p_sensitivity;
	emit_changed();
}

float InputSettings::get_gamepad_sensitivity() const {
	return gamepad_sensitivity;
}

void InputSettings::set_gamepad_x_inverted(bool p_inverted) {
	gamepad_x_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_gamepad_x_inverted() const {
	return gamepad_x_inverted;
}

void InputSettings::set_gamepad_y_inverted(bool p_inverted) {
	gamepad_y_inverted = p_inverted;
	emit_changed();
}

bool InputSettings::is_gamepad_y_inverted() const {
	return gamepad_y_inverted;
}

void InputSettings::set_gamepad_deadzone(float p_deadzone) {
	gamepad_deadzone = p_deadzone;
	emit_changed();
}

float InputSettings::get_gamepad_deadzone() const {
	return gamepad_deadzone;
}

void InputSettings::set_vibration_enabled(bool p_enabled) {
	vibration_enabled = p_enabled;
	emit_changed();
}

bool InputSettings::is_vibration_enabled() const {
	return vibration_enabled;
}

void InputSettings::set_vibration_strength(float p_strength) {
	vibration_strength = p_strength;
	emit_changed();
}

float InputSettings::get_vibration_strength() const {
	return vibration_strength;
}

void InputSettings::set_toggle_actions(const PackedStringArray &p_actions) {
	toggle_actions = p_actions;
	emit_changed();
}

PackedStringArray InputSettings::get_toggle_actions() const {
	return toggle_actions;
}

void InputSettings::set_first_player_device_id(int p_device) {
	first_player_device_id = p_device;
	emit_changed();
}

int InputSettings::get_first_player_device_id() const {
	return first_player_device_id;
}

void InputSettings::set_allow_keyboard_for_current_player(bool p_allow) {
	allow_keyboard_for_current_player = p_allow;
	emit_changed();
}

bool InputSettings::is_keyboard_allowed_for_current_player() const {
	return allow_keyboard_for_current_player;
}

void InputSettings::set_emulate_mouse_from_touch(bool p_emulate) {
	emulate_mouse_from_touch = p_emulate;
	emit_changed();
}

bool InputSettings::is_emulating_mouse_from_touch() const {
	return emulate_mouse_from_touch;
}

void InputSettings::set_emulate_touch_from_mouse(bool p_emulate) {
	emulate_touch_from_mouse = p_emulate;
	emit_changed();
}

bool InputSettings::is_emulating_touch_from_mouse() const {
	return emulate_touch_from_mouse;
}

void InputSettings::set_use_accumulated_input(bool p_enable) {
	use_accumulated_input = p_enable;
	emit_changed();
}

bool InputSettings::is_using_accumulated_input() const {
	return use_accumulated_input;
}

void InputSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_keyboard_sensitivity", "sensitivity"), &InputSettings::set_keyboard_sensitivity);
	ClassDB::bind_method(D_METHOD("get_keyboard_sensitivity"), &InputSettings::get_keyboard_sensitivity);
	ClassDB::bind_method(D_METHOD("set_keyboard_x_inverted", "inverted"), &InputSettings::set_keyboard_x_inverted);
	ClassDB::bind_method(D_METHOD("is_keyboard_x_inverted"), &InputSettings::is_keyboard_x_inverted);
	ClassDB::bind_method(D_METHOD("set_keyboard_y_inverted", "inverted"), &InputSettings::set_keyboard_y_inverted);
	ClassDB::bind_method(D_METHOD("is_keyboard_y_inverted"), &InputSettings::is_keyboard_y_inverted);
	ClassDB::bind_method(D_METHOD("set_mouse_sensitivity", "sensitivity"), &InputSettings::set_mouse_sensitivity);
	ClassDB::bind_method(D_METHOD("get_mouse_sensitivity"), &InputSettings::get_mouse_sensitivity);
	ClassDB::bind_method(D_METHOD("set_mouse_x_inverted", "inverted"), &InputSettings::set_mouse_x_inverted);
	ClassDB::bind_method(D_METHOD("is_mouse_x_inverted"), &InputSettings::is_mouse_x_inverted);
	ClassDB::bind_method(D_METHOD("set_mouse_y_inverted", "inverted"), &InputSettings::set_mouse_y_inverted);
	ClassDB::bind_method(D_METHOD("is_mouse_y_inverted"), &InputSettings::is_mouse_y_inverted);
	ClassDB::bind_method(D_METHOD("set_mouse_smoothing", "smoothing"), &InputSettings::set_mouse_smoothing);
	ClassDB::bind_method(D_METHOD("get_mouse_smoothing"), &InputSettings::get_mouse_smoothing);
	ClassDB::bind_method(D_METHOD("set_mouse_confined", "confined"), &InputSettings::set_mouse_confined);
	ClassDB::bind_method(D_METHOD("is_mouse_confined"), &InputSettings::is_mouse_confined);
	ClassDB::bind_method(D_METHOD("set_gamepad_sensitivity", "sensitivity"), &InputSettings::set_gamepad_sensitivity);
	ClassDB::bind_method(D_METHOD("get_gamepad_sensitivity"), &InputSettings::get_gamepad_sensitivity);
	ClassDB::bind_method(D_METHOD("set_gamepad_x_inverted", "inverted"), &InputSettings::set_gamepad_x_inverted);
	ClassDB::bind_method(D_METHOD("is_gamepad_x_inverted"), &InputSettings::is_gamepad_x_inverted);
	ClassDB::bind_method(D_METHOD("set_gamepad_y_inverted", "inverted"), &InputSettings::set_gamepad_y_inverted);
	ClassDB::bind_method(D_METHOD("is_gamepad_y_inverted"), &InputSettings::is_gamepad_y_inverted);
	ClassDB::bind_method(D_METHOD("set_gamepad_deadzone", "deadzone"), &InputSettings::set_gamepad_deadzone);
	ClassDB::bind_method(D_METHOD("get_gamepad_deadzone"), &InputSettings::get_gamepad_deadzone);
	ClassDB::bind_method(D_METHOD("set_vibration_enabled", "enabled"), &InputSettings::set_vibration_enabled);
	ClassDB::bind_method(D_METHOD("is_vibration_enabled"), &InputSettings::is_vibration_enabled);
	ClassDB::bind_method(D_METHOD("set_vibration_strength", "strength"), &InputSettings::set_vibration_strength);
	ClassDB::bind_method(D_METHOD("get_vibration_strength"), &InputSettings::get_vibration_strength);
	ClassDB::bind_method(D_METHOD("set_toggle_actions", "actions"), &InputSettings::set_toggle_actions);
	ClassDB::bind_method(D_METHOD("get_toggle_actions"), &InputSettings::get_toggle_actions);
	ClassDB::bind_method(D_METHOD("set_first_player_device_id", "device"), &InputSettings::set_first_player_device_id);
	ClassDB::bind_method(D_METHOD("get_first_player_device_id"), &InputSettings::get_first_player_device_id);
	ClassDB::bind_method(D_METHOD("set_allow_keyboard_for_current_player", "allow"), &InputSettings::set_allow_keyboard_for_current_player);
	ClassDB::bind_method(D_METHOD("is_keyboard_allowed_for_current_player"), &InputSettings::is_keyboard_allowed_for_current_player);
	ClassDB::bind_method(D_METHOD("set_emulate_mouse_from_touch", "emulate"), &InputSettings::set_emulate_mouse_from_touch);
	ClassDB::bind_method(D_METHOD("is_emulating_mouse_from_touch"), &InputSettings::is_emulating_mouse_from_touch);
	ClassDB::bind_method(D_METHOD("set_emulate_touch_from_mouse", "emulate"), &InputSettings::set_emulate_touch_from_mouse);
	ClassDB::bind_method(D_METHOD("is_emulating_touch_from_mouse"), &InputSettings::is_emulating_touch_from_mouse);
	ClassDB::bind_method(D_METHOD("set_use_accumulated_input", "enable"), &InputSettings::set_use_accumulated_input);
	ClassDB::bind_method(D_METHOD("is_using_accumulated_input"), &InputSettings::is_using_accumulated_input);

	ADD_GROUP("Keyboard", "keyboard_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "keyboard_sensitivity", PROPERTY_HINT_RANGE, "0.01,10,0.01"), "set_keyboard_sensitivity", "get_keyboard_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keyboard_x_inverted"), "set_keyboard_x_inverted", "is_keyboard_x_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keyboard_y_inverted"), "set_keyboard_y_inverted", "is_keyboard_y_inverted");
	ADD_GROUP("Mouse", "mouse_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mouse_sensitivity", PROPERTY_HINT_RANGE, "0.01,10,0.01"), "set_mouse_sensitivity", "get_mouse_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mouse_x_inverted"), "set_mouse_x_inverted", "is_mouse_x_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mouse_y_inverted"), "set_mouse_y_inverted", "is_mouse_y_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mouse_smoothing", PROPERTY_HINT_RANGE, "0,0.95,0.01"), "set_mouse_smoothing", "get_mouse_smoothing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mouse_confined"), "set_mouse_confined", "is_mouse_confined");
	ADD_GROUP("Gamepad", "gamepad_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gamepad_sensitivity", PROPERTY_HINT_RANGE, "0.01,10,0.01"), "set_gamepad_sensitivity", "get_gamepad_sensitivity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gamepad_x_inverted"), "set_gamepad_x_inverted", "is_gamepad_x_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gamepad_y_inverted"), "set_gamepad_y_inverted", "is_gamepad_y_inverted");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gamepad_deadzone", PROPERTY_HINT_RANGE, "0,0.95,0.01"), "set_gamepad_deadzone", "get_gamepad_deadzone");
	ADD_GROUP("Vibration", "vibration_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vibration_enabled"), "set_vibration_enabled", "is_vibration_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vibration_strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_vibration_strength", "get_vibration_strength");
	ADD_GROUP("Behavior", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "toggle_actions"), "set_toggle_actions", "get_toggle_actions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "first_player_device_id", PROPERTY_HINT_RANGE, "-1,15,1,or_greater"), "set_first_player_device_id", "get_first_player_device_id");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_keyboard_for_current_player"), "set_allow_keyboard_for_current_player", "is_keyboard_allowed_for_current_player");
	ADD_GROUP("Engine", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emulate_mouse_from_touch"), "set_emulate_mouse_from_touch", "is_emulating_mouse_from_touch");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emulate_touch_from_mouse"), "set_emulate_touch_from_mouse", "is_emulating_touch_from_mouse");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_accumulated_input"), "set_use_accumulated_input", "is_using_accumulated_input");
}
