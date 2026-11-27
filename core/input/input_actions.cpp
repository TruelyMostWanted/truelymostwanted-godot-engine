/**************************************************************************/
/*  input_actions.cpp                                                     */
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

#include "input_actions.h"

#include "core/core_string_names.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

int InputActions::_find(const StringName &p_action) const {
	if (lookup_dirty) {
		// An action was renamed or removed: index again. The last action of a name wins, as in set_actions().
		lookup.clear();
		for (uint32_t i = 0; i < actions.size(); i++) {
			lookup.insert(actions[i]->get_action(), i);
		}
		lookup_dirty = false;
	}
	const uint32_t *index = lookup.getptr(p_action);
	return index != nullptr ? int(*index) : -1;
}

Callable InputActions::_get_changed_callable(const Ref<InputAction> &p_action) {
	// Bound to the action (by ID, a Ref would keep it alive), so a change can be checked in O(1).
	return callable_mp(this, &InputActions::_action_changed).bind(uint64_t(p_action->get_instance_id()));
}

void InputActions::_attach(const Ref<InputAction> &p_action) {
	p_action->connect(CoreStringName(changed), _get_changed_callable(p_action), CONNECT_REFERENCE_COUNTED);
}

void InputActions::_detach(const Ref<InputAction> &p_action) {
	const Callable callable = _get_changed_callable(p_action);
	if (p_action->is_connected(CoreStringName(changed), callable)) {
		p_action->disconnect(CoreStringName(changed), callable);
	}
}

void InputActions::_action_changed(uint64_t p_action_id) {
	// Only a rename changes the lookup: then the action is no longer found under its name, and the
	// lookup is rebuilt on the next access. Other changes (events, deadzone) keep it.
	if (!lookup_dirty) {
		const InputAction *action = ObjectDB::get_instance<InputAction>(ObjectID(p_action_id));
		const uint32_t *index = action != nullptr ? lookup.getptr(action->get_action()) : nullptr;
		if (index == nullptr || *index >= actions.size() || actions[*index].ptr() != action) {
			lookup_dirty = true;
		}
	}
	emit_changed();
}

void InputActions::set_actions(const TypedArray<InputAction> &p_actions) {
	for (const Ref<InputAction> &action : actions) {
		_detach(action);
	}
	actions.clear();
	action_slots.clear();
	lookup.clear();
	lookup_dirty = false;
	bool has_empty_slot = false;
	PackedStringArray duplicates;
	for (int i = 0; i < p_actions.size(); i++) {
		const Ref<InputAction> action = p_actions[i];
		if (action.is_null()) {
			has_empty_slot = true;
			continue;
		}
		// All entries are kept, so the Inspector never loses one; the later action of a name wins in the lookup.
		// Actions without a name yet (a new element in the Inspector, a loader that sets the properties later)
		// are kept too; the lookup is rebuilt when they get their name.
		const StringName action_name = action->get_action();
		if (action_name != StringName()) {
			if (lookup.has(action_name) && !duplicates.has(action_name)) {
				duplicates.push_back(action_name);
			}
			lookup.insert(action_name, actions.size());
		}
		actions.push_back(action);
		_attach(action);
	}
	if (has_empty_slot) {
		for (int i = 0; i < p_actions.size(); i++) {
			action_slots.push_back(p_actions[i]);
		}
	}
	if (!duplicates.is_empty()) {
		WARN_PRINT(vformat("InputActions: the action %s is listed more than once; the last entry of a name is used.", String(", ").join(duplicates)));
	}
	emit_changed();
}

TypedArray<InputAction> InputActions::get_actions() const {
	TypedArray<InputAction> result;
	for (const Ref<InputAction> &action : action_slots.is_empty() ? actions : action_slots) {
		result.push_back(action);
	}
	return result;
}

void InputActions::add_action(const Ref<InputAction> &p_action) {
	ERR_FAIL_COND_MSG(p_action.is_null(), "InputActions: the action is null.");
	ERR_FAIL_COND_MSG(p_action->get_action() == StringName(), "InputActions: the action has no name.");
	const int index = _find(p_action->get_action());
	if (index >= 0) {
		if (actions[index] == p_action) {
			return;
		}
		_detach(actions[index]);
		actions[index] = p_action;
	} else {
		lookup.insert(p_action->get_action(), actions.size());
		actions.push_back(p_action);
	}
	action_slots.clear();
	_attach(p_action);
	emit_changed();
}

Ref<InputAction> InputActions::get_action(const StringName &p_action) const {
	const int index = _find(p_action);
	return index >= 0 ? actions[index] : Ref<InputAction>();
}

bool InputActions::has_action(const StringName &p_action) const {
	return _find(p_action) >= 0;
}

bool InputActions::remove_action(const StringName &p_action) {
	const int index = _find(p_action);
	if (index < 0) {
		return false;
	}
	_detach(actions[index]);
	actions.remove_at(index);
	action_slots.clear();
	lookup_dirty = true;
	emit_changed();
	return true;
}

TypedArray<StringName> InputActions::get_action_names() const {
	TypedArray<StringName> result;
	for (const Ref<InputAction> &action : actions) {
		result.push_back(action->get_action());
	}
	return result;
}

int InputActions::get_action_count() const {
	return actions.size();
}

void InputActions::clear() {
	if (actions.is_empty() && action_slots.is_empty()) {
		return;
	}
	for (const Ref<InputAction> &action : actions) {
		_detach(action);
	}
	actions.clear();
	action_slots.clear();
	lookup.clear();
	lookup_dirty = false;
	emit_changed();
}

void InputActions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_actions", "actions"), &InputActions::set_actions);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputActions::get_actions);
	ClassDB::bind_method(D_METHOD("add_action", "action"), &InputActions::add_action);
	ClassDB::bind_method(D_METHOD("get_action", "action"), &InputActions::get_action);
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputActions::has_action);
	ClassDB::bind_method(D_METHOD("remove_action", "action"), &InputActions::remove_action);
	ClassDB::bind_method(D_METHOD("get_action_names"), &InputActions::get_action_names);
	ClassDB::bind_method(D_METHOD("get_action_count"), &InputActions::get_action_count);
	ClassDB::bind_method(D_METHOD("clear"), &InputActions::clear);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "actions", PROPERTY_HINT_ARRAY_TYPE, "InputAction"), "set_actions", "get_actions");
}

InputActions::~InputActions() {
	for (const Ref<InputAction> &action : actions) {
		_detach(action);
	}
}
