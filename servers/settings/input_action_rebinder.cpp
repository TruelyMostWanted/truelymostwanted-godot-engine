/**************************************************************************/
/*  input_action_rebinder.cpp                                             */
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

#include "input_action_rebinder.h"

#include "core/input/input_action.h"
#include "core/input/input_map.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "servers/settings/input_map_settings_server.h"

static bool _is_modifier_key(Key p_key) {
	return p_key == Key::SHIFT || p_key == Key::CTRL || p_key == Key::ALT || p_key == Key::META;
}

static bool _has_event_text(const TypedArray<InputEvent> &p_events, const String &p_text) {
	for (int i = 0; i < p_events.size(); i++) {
		if (InputAction::event_to_text(p_events[i]) == p_text) {
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// Properties.

void InputActionRebinder::set_device_filter(DeviceFilter p_filter) {
	ERR_FAIL_INDEX(int(p_filter), int(DEVICE_GAMEPAD) + 1);
	device_filter = p_filter;
}

InputActionRebinder::DeviceFilter InputActionRebinder::get_device_filter() const {
	return device_filter;
}

void InputActionRebinder::set_gamepad_device(int p_device) {
	gamepad_device = MAX(-1, p_device);
}

int InputActionRebinder::get_gamepad_device() const {
	return gamepad_device;
}

void InputActionRebinder::set_conflict_mode(ConflictMode p_mode) {
	ERR_FAIL_INDEX(int(p_mode), int(CONFLICT_ALLOW) + 1);
	conflict_mode = p_mode;
}

InputActionRebinder::ConflictMode InputActionRebinder::get_conflict_mode() const {
	return conflict_mode;
}

void InputActionRebinder::set_axis_threshold(float p_threshold) {
	axis_threshold = CLAMP(p_threshold, 0.0f, 1.0f);
}

float InputActionRebinder::get_axis_threshold() const {
	return axis_threshold;
}

void InputActionRebinder::set_ignored_actions(const PackedStringArray &p_actions) {
	ignored_actions = p_actions;
}

PackedStringArray InputActionRebinder::get_ignored_actions() const {
	return ignored_actions;
}

void InputActionRebinder::set_settings_server(InputMapSettingsServer *p_server) {
	server_id = p_server != nullptr ? p_server->get_instance_id() : ObjectID();
}

InputMapSettingsServer *InputActionRebinder::_get_server() const {
	if (server_id.is_valid()) {
		return Object::cast_to<InputMapSettingsServer>(ObjectDB::get_instance(server_id));
	}
	return InputMapSettingsServer::get_singleton();
}

/////////////////////////////////////////////////////////////////////////////
// Helpers.

bool InputActionRebinder::_is_in_device_class(const Ref<InputEvent> &p_event) const {
	const InputAction::InputDevice device = InputAction::get_event_device(p_event);
	switch (device_filter) {
		case DEVICE_KEYBOARD_MOUSE:
			return device == InputAction::INPUT_DEVICE_KEYBOARD || device == InputAction::INPUT_DEVICE_MOUSE;
		case DEVICE_GAMEPAD:
			return device == InputAction::INPUT_DEVICE_GAMEPAD;
		default:
			return device != InputAction::INPUT_DEVICE_MAX;
	}
}

int InputActionRebinder::_get_list_index(const TypedArray<InputEvent> &p_events, int p_index) const {
	int index = 0;
	for (int i = 0; i < p_events.size(); i++) {
		if (!_is_in_device_class(p_events[i])) {
			continue;
		}
		if (index == p_index) {
			return i;
		}
		index++;
	}
	return -1;
}

int InputActionRebinder::_get_class_index(const TypedArray<InputEvent> &p_events, int p_list_index) const {
	int index = 0;
	for (int i = 0; i < p_list_index && i < p_events.size(); i++) {
		if (_is_in_device_class(p_events[i])) {
			index++;
		}
	}
	return index;
}

bool InputActionRebinder::_events_match(const Ref<InputEvent> &p_bound, const Ref<InputEvent> &p_event, const String &p_text) {
	if (p_bound.is_null()) {
		return false;
	}
	if (p_bound->get_device() != -1 && p_event->get_device() != -1 && p_bound->get_device() != p_event->get_device()) {
		return false;
	}
	// A pressed key has all codes, a project binding only one of them (`Key:Space` matches a pressed Space).
	return InputAction::event_to_text(p_bound) == p_text || p_bound->is_match(p_event, true);
}

void InputActionRebinder::_find_conflicts(InputMapSettingsServer *p_server, const Ref<InputEvent> &p_event, const Ref<InputEvent> &p_pressed) {
	conflicts.clear();
	InputMap *input_map = InputMap::get_singleton();
	if (input_map == nullptr) {
		return;
	}
	const String text = InputAction::event_to_text(p_event);
	// The merged InputMap: built-in and custom actions, with the pending events of the actions the server stores.
	const TypedArray<StringName> names = input_map->get_actions();
	for (int i = 0; i < names.size(); i++) {
		const StringName other = names[i];
		if (other == action || ignored_actions.has(String(other))) {
			continue;
		}
		bool exists = false;
		bool managed = false;
		const TypedArray<InputEvent> events = p_server->_get_merged_events(other, exists, managed);
		int index = 0;
		for (int j = 0; j < events.size(); j++) {
			const Ref<InputEvent> bound = events[j];
			if (!_is_in_device_class(bound)) {
				continue;
			}
			if (_events_match(bound, p_pressed, text)) {
				Conflict found;
				found.action = other;
				found.event_index = index;
				conflicts.push_back(found);
				break;
			}
			index++;
		}
	}
}

void InputActionRebinder::_emit_conflicts() {
	// Copies: a connected method may cancel() the session.
	const StringName source_action = action;
	const int source_index = event_index;
	const LocalVector<Conflict> list(conflicts);
	for (const Conflict &item : list) {
		emit_signal(SNAME("conflict"), source_action, source_index, item.action, item.event_index);
	}
}

void InputActionRebinder::_reset_session() {
	active = false;
	action = StringName();
	event_index = -1;
	pending_modifier = Key::NONE;
	pending_modifier_event.unref();
	conflict_pending = false;
	conflict_event.unref();
	conflicts.clear();
}

/////////////////////////////////////////////////////////////////////////////
// Flow.

Error InputActionRebinder::begin(const StringName &p_action, int p_event_index) {
	ERR_FAIL_COND_V_MSG(p_event_index < 0, ERR_INVALID_PARAMETER, "InputActionRebinder: the event index must be 0 or greater.");
	InputMapSettingsServer *server = _get_server();
	ERR_FAIL_NULL_V_MSG(server, ERR_UNCONFIGURED, "InputActionRebinder: there is no InputMapSettingsServer.");
	bool exists = false;
	bool managed = false;
	server->_get_merged_events(p_action, exists, managed);
	ERR_FAIL_COND_V_MSG(!exists, ERR_DOES_NOT_EXIST, vformat("InputActionRebinder: there is no action \"%s\".", p_action));
	ERR_FAIL_COND_V_MSG(!managed, ERR_UNAVAILABLE, vformat("InputActionRebinder: InputMapSettingsServer does not store the action \"%s\" in its save mode.", p_action));
	if (active) {
		cancel();
	}
	active = true;
	action = p_action;
	event_index = p_event_index;
	emit_signal(SNAME("started"), action, event_index);
	return OK;
}

bool InputActionRebinder::update(const Ref<InputEvent> &p_event) {
	if (!active || conflict_pending || p_event.is_null()) {
		return false;
	}
	// Mouse motion, touch, MIDI and actions have no device class; keys or pads of the other class are skipped.
	if (!_is_in_device_class(p_event) || InputAction::event_to_text(p_event).is_empty()) {
		return false;
	}

	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid()) {
		if (key_event->is_echo()) {
			return false;
		}
		const Key code = key_event->get_physical_keycode() != Key::NONE ? key_event->get_physical_keycode() : key_event->get_keycode();
		if (_is_modifier_key(code)) {
			if (key_event->is_pressed()) {
				// Maybe the start of a combination such as Ctrl+S: bound only if it is released alone.
				pending_modifier = code;
				pending_modifier_event = key_event->duplicate();
				return true;
			}
			if (code != pending_modifier) {
				return false;
			}
			const Ref<InputEvent> modifier = pending_modifier_event;
			pending_modifier = Key::NONE;
			pending_modifier_event.unref();
			_accept(modifier);
			return true;
		}
		if (!key_event->is_pressed()) {
			return false;
		}
	} else {
		if (gamepad_device >= 0 && InputAction::get_event_device(p_event) == InputAction::INPUT_DEVICE_GAMEPAD && p_event->get_device() != gamepad_device) {
			return false;
		}
		const Ref<InputEventJoypadMotion> motion_event = p_event;
		if (motion_event.is_valid()) {
			if (Math::abs(motion_event->get_axis_value()) < axis_threshold) {
				return false;
			}
		} else if (!p_event->is_pressed()) {
			return false;
		}
	}
	pending_modifier = Key::NONE;
	pending_modifier_event.unref();
	_accept(p_event);
	return true;
}

void InputActionRebinder::_accept(const Ref<InputEvent> &p_pressed) {
	InputMapSettingsServer *server = _get_server();
	ERR_FAIL_NULL_MSG(server, "InputActionRebinder: there is no InputMapSettingsServer.");
	// The stored form of the server (one code, all devices): what a save and load of the settings gives back.
	const Ref<InputEvent> event = InputAction::text_to_event(InputAction::event_to_text(p_pressed));
	ERR_FAIL_COND(event.is_null());
	_find_conflicts(server, event, p_pressed);
	conflict_event = event;
	if (conflicts.is_empty()) {
		_commit(event, false);
		return;
	}
	switch (conflict_mode) {
		case CONFLICT_ASK: {
			conflict_pending = true;
			_emit_conflicts();
		} break;
		case CONFLICT_REJECT: {
			_emit_conflicts();
			conflicts.clear();
		} break;
		case CONFLICT_SWAP: {
			if (_commit(event, true) != OK) {
				// An action the server does not store can't be changed: rejected like CONFLICT_REJECT.
				_emit_conflicts();
				conflicts.clear();
			}
		} break;
		case CONFLICT_ALLOW: {
			_commit(event, false);
		} break;
	}
}

Error InputActionRebinder::_commit(const Ref<InputEvent> &p_event, bool p_swap) {
	InputMapSettingsServer *server = _get_server();
	ERR_FAIL_NULL_V(server, ERR_UNCONFIGURED);
	const String text = InputAction::event_to_text(p_event);
	bool exists = false;
	bool managed = false;
	const TypedArray<InputEvent> own = server->_get_merged_events(action, exists, managed);
	ERR_FAIL_COND_V_MSG(!exists || !managed, ERR_UNAVAILABLE, vformat("InputActionRebinder: the action \"%s\" can't be changed.", action));

	// The slot gets the event; the same event in another slot of the action is removed (no duplicates).
	const int own_index = _get_list_index(own, event_index);
	const Ref<InputEvent> old_event = own_index >= 0 ? Ref<InputEvent>(own[own_index]) : Ref<InputEvent>();
	TypedArray<InputEvent> new_own;
	for (int i = 0; i < own.size(); i++) {
		if (i == own_index) {
			new_own.push_back(p_event);
		} else if (InputAction::event_to_text(own[i]) != text) {
			new_own.push_back(own[i]);
		}
	}
	if (own_index < 0) {
		new_own.push_back(p_event);
	}

	LocalVector<StringName> names;
	LocalVector<TypedArray<InputEvent>> lists;
	names.push_back(action);
	lists.push_back(new_own);
	if (p_swap) {
		const String old_text = old_event.is_valid() ? InputAction::event_to_text(old_event) : String();
		bool first = true;
		for (const Conflict &item : conflicts) {
			bool dest_exists = false;
			bool dest_managed = false;
			TypedArray<InputEvent> dest = server->_get_merged_events(item.action, dest_exists, dest_managed);
			if (!dest_managed) {
				return ERR_UNAVAILABLE;
			}
			const int dest_index = _get_list_index(dest, item.event_index);
			if (dest_index < 0) {
				continue;
			}
			// The first other action gets the old event of the slot; further ones only lose the event.
			if (first && old_event.is_valid() && !_has_event_text(dest, old_text)) {
				dest[dest_index] = old_event;
			} else {
				dest.remove_at(dest_index);
			}
			first = false;
			names.push_back(item.action);
			lists.push_back(dest);
		}
	}

	// The server emits `action_events_changed` for every changed action before it returns.
	const Error err = server->_set_events_of_actions(names, lists);
	if (err != OK) {
		return err;
	}
	const TypedArray<InputEvent> committed = server->get_events(action);
	int committed_index = event_index;
	for (int i = 0; i < committed.size(); i++) {
		if (InputAction::event_to_text(committed[i]) == text) {
			committed_index = _get_class_index(committed, i);
			break;
		}
	}
	const StringName done_action = action;
	const Ref<InputEvent> done_event = InputAction::text_to_event(text);
	_reset_session();
	emit_signal(SNAME("completed"), done_action, committed_index, done_event);
	return OK;
}

void InputActionRebinder::cancel() {
	if (!active) {
		return;
	}
	const StringName canceled_action = action;
	const int canceled_index = event_index;
	_reset_session();
	emit_signal(SNAME("canceled"), canceled_action, canceled_index);
}

Error InputActionRebinder::resolve_conflict_swap() {
	ERR_FAIL_COND_V_MSG(!conflict_pending, ERR_UNCONFIGURED, "InputActionRebinder: no conflict is waiting for a decision.");
	InputMapSettingsServer *server = _get_server();
	ERR_FAIL_NULL_V(server, ERR_UNCONFIGURED);
	// The bindings may have changed while the player decided.
	_find_conflicts(server, conflict_event, conflict_event);
	return _commit(conflict_event, true);
}

Error InputActionRebinder::resolve_conflict_allow() {
	ERR_FAIL_COND_V_MSG(!conflict_pending, ERR_UNCONFIGURED, "InputActionRebinder: no conflict is waiting for a decision.");
	return _commit(conflict_event, false);
}

bool InputActionRebinder::is_active() const {
	return active;
}

bool InputActionRebinder::is_conflict_pending() const {
	return conflict_pending;
}

StringName InputActionRebinder::get_action() const {
	return action;
}

int InputActionRebinder::get_event_index() const {
	return event_index;
}

Ref<InputEvent> InputActionRebinder::get_conflict_event() const {
	return conflict_event;
}

void InputActionRebinder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_device_filter", "filter"), &InputActionRebinder::set_device_filter);
	ClassDB::bind_method(D_METHOD("get_device_filter"), &InputActionRebinder::get_device_filter);
	ClassDB::bind_method(D_METHOD("set_gamepad_device", "device"), &InputActionRebinder::set_gamepad_device);
	ClassDB::bind_method(D_METHOD("get_gamepad_device"), &InputActionRebinder::get_gamepad_device);
	ClassDB::bind_method(D_METHOD("set_conflict_mode", "mode"), &InputActionRebinder::set_conflict_mode);
	ClassDB::bind_method(D_METHOD("get_conflict_mode"), &InputActionRebinder::get_conflict_mode);
	ClassDB::bind_method(D_METHOD("set_axis_threshold", "threshold"), &InputActionRebinder::set_axis_threshold);
	ClassDB::bind_method(D_METHOD("get_axis_threshold"), &InputActionRebinder::get_axis_threshold);
	ClassDB::bind_method(D_METHOD("set_ignored_actions", "actions"), &InputActionRebinder::set_ignored_actions);
	ClassDB::bind_method(D_METHOD("get_ignored_actions"), &InputActionRebinder::get_ignored_actions);

	ClassDB::bind_method(D_METHOD("begin", "action", "event_index"), &InputActionRebinder::begin);
	ClassDB::bind_method(D_METHOD("update", "event"), &InputActionRebinder::update);
	ClassDB::bind_method(D_METHOD("cancel"), &InputActionRebinder::cancel);
	ClassDB::bind_method(D_METHOD("resolve_conflict_swap"), &InputActionRebinder::resolve_conflict_swap);
	ClassDB::bind_method(D_METHOD("resolve_conflict_allow"), &InputActionRebinder::resolve_conflict_allow);
	ClassDB::bind_method(D_METHOD("is_active"), &InputActionRebinder::is_active);
	ClassDB::bind_method(D_METHOD("is_conflict_pending"), &InputActionRebinder::is_conflict_pending);
	ClassDB::bind_method(D_METHOD("get_action"), &InputActionRebinder::get_action);
	ClassDB::bind_method(D_METHOD("get_event_index"), &InputActionRebinder::get_event_index);
	ClassDB::bind_method(D_METHOD("get_conflict_event"), &InputActionRebinder::get_conflict_event);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "device_filter", PROPERTY_HINT_ENUM, "Any,Keyboard Mouse,Gamepad"), "set_device_filter", "get_device_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gamepad_device", PROPERTY_HINT_RANGE, "-1,15,1"), "set_gamepad_device", "get_gamepad_device");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "conflict_mode", PROPERTY_HINT_ENUM, "Ask,Reject,Swap,Allow"), "set_conflict_mode", "get_conflict_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "axis_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_axis_threshold", "get_axis_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "ignored_actions"), "set_ignored_actions", "get_ignored_actions");

	ADD_SIGNAL(MethodInfo("started", PropertyInfo(Variant::STRING_NAME, "action"), PropertyInfo(Variant::INT, "event_index")));
	ADD_SIGNAL(MethodInfo("canceled", PropertyInfo(Variant::STRING_NAME, "action"), PropertyInfo(Variant::INT, "event_index")));
	ADD_SIGNAL(MethodInfo("conflict", PropertyInfo(Variant::STRING_NAME, "source_action"), PropertyInfo(Variant::INT, "source_event_index"), PropertyInfo(Variant::STRING_NAME, "dest_action"), PropertyInfo(Variant::INT, "dest_event_index")));
	ADD_SIGNAL(MethodInfo("completed", PropertyInfo(Variant::STRING_NAME, "action"), PropertyInfo(Variant::INT, "event_index"), PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEvent")));

	BIND_ENUM_CONSTANT(DEVICE_ANY);
	BIND_ENUM_CONSTANT(DEVICE_KEYBOARD_MOUSE);
	BIND_ENUM_CONSTANT(DEVICE_GAMEPAD);

	BIND_ENUM_CONSTANT(CONFLICT_ASK);
	BIND_ENUM_CONSTANT(CONFLICT_REJECT);
	BIND_ENUM_CONSTANT(CONFLICT_SWAP);
	BIND_ENUM_CONSTANT(CONFLICT_ALLOW);
}
