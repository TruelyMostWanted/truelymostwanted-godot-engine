/**************************************************************************/
/*  input_actions.h                                                       */
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

#include "core/input/input_action.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

// A collection of InputAction resources with O(1) lookup by action name (#172).
// InputMap references two of them as its layers (InputMap.get_input_actions()); it is also the settings
// resource of InputMapSettingsServer. It emits `changed` when the list or one of its actions changes.
// The list is the typed array `actions` for the Inspector. add_action() with a name that is already used
// replaces the old action; set_actions() keeps all entries (the Inspector's "Duplicate Element", a name typed twice)
// and warns, the last action of a name wins.
class InputActions : public Resource {
	GDCLASS(InputActions, Resource);

	LocalVector<Ref<InputAction>> actions; // Never null; may hold a name more than once (set_actions()).
	// `actions` with its empty (null) slots, as the Inspector sets it ("Add Element" adds a null slot first);
	// only while it has one. Changing the list through a method drops the empty slots.
	LocalVector<Ref<InputAction>> action_slots;
	mutable HashMap<StringName, uint32_t> lookup;
	mutable bool lookup_dirty = false;

	int _find(const StringName &p_action) const;
	Callable _get_changed_callable(const Ref<InputAction> &p_action);
	void _attach(const Ref<InputAction> &p_action);
	void _detach(const Ref<InputAction> &p_action);
	void _action_changed(uint64_t p_action_id);

protected:
	static void _bind_methods();

public:
	void set_actions(const TypedArray<InputAction> &p_actions);
	TypedArray<InputAction> get_actions() const;
	const LocalVector<Ref<InputAction>> &get_action_list() const { return actions; }

	void add_action(const Ref<InputAction> &p_action);
	Ref<InputAction> get_action(const StringName &p_action) const;
	bool has_action(const StringName &p_action) const;
	bool remove_action(const StringName &p_action);
	TypedArray<StringName> get_action_names() const;
	int get_action_count() const;
	void clear();

	~InputActions();
};
