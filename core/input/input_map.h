/**************************************************************************/
/*  input_map.h                                                           */
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

#include "core/input/input_actions.h"
#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/binder_common.h"

template <typename T>
class TypedArray;

class InputMap : public Object {
	GDCLASS(InputMap, Object);

public:
	/**
	 * A special value used to signify that a given Action can be triggered by any device
	 */
	static constexpr int ALL_DEVICES = -1;

	struct Action {
		int id;
		float deadzone;
		List<Ref<InputEvent>> inputs;
	};

	// The two layers of actions (#172): Godot's built-in `ui_*` actions (also when the project changed
	// them) and every action the project or the game adds.
	enum InputActionLayer {
		INPUT_ACTION_LAYER_BUILTIN,
		INPUT_ACTION_LAYER_CUSTOM,
	};

	static constexpr float DEFAULT_DEADZONE = 0.2f;
	// Keep bigger deadzone for toggle actions (default `ui_*` actions, axis `pressed`) (GH-103360).
	static constexpr float DEFAULT_TOGGLE_DEADZONE = 0.5f;

private:
	static inline InputMap *singleton = nullptr;

	mutable HashMap<StringName, Action> input_map;
	HashMap<String, List<Ref<InputEvent>>> default_builtin_cache;
	HashMap<String, List<Ref<InputEvent>>> default_builtin_with_overrides_cache;

	// Names of Godot's built-in actions, with and without feature suffix (`ui_text_backspace_word.macos`).
	HashSet<StringName> builtin_action_names;

	// Reverse index (#172): event key (type and keycode, button or axis) to the actions that have an event
	// with that key, as ordinals into `event_index_actions` (the actions in the order of `input_map`).
	// Rebuilt on the next lookup after any change of the map or of one of its events.
	mutable Mutex event_index_mutex;
	mutable SafeFlag event_index_dirty;
	mutable HashMap<uint64_t, LocalVector<uint32_t>> event_index;
	mutable LocalVector<StringName> event_index_actions;
	mutable LocalVector<uint32_t> event_index_unindexed; // Actions with event types the index doesn't know.

	void _rebuild_event_index() const;
	void _mark_event_index_dirty();
	void _watch_event(const Ref<InputEvent> &p_event);
	void _unwatch_event(const Ref<InputEvent> &p_event);
	void _add_project_action(const String &p_name, const Dictionary &p_action);

	List<Ref<InputEvent>>::Element *_find_event(Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match = false, bool *r_pressed = nullptr, float *r_strength = nullptr, float *r_raw_strength = nullptr, int *r_event_index = nullptr) const;

	TypedArray<InputEvent> _action_get_events(const StringName &p_action);

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _add_action_bind_compat_97281(const StringName &p_action, float p_deadzone = 0.5);
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	static _FORCE_INLINE_ InputMap *get_singleton() { return singleton; }

	bool has_action(const StringName &p_action) const;
	TypedArray<StringName> get_actions();
	void add_action(const StringName &p_action, float p_deadzone = DEFAULT_DEADZONE);
	void erase_action(const StringName &p_action);

	String get_action_description(const StringName &p_action) const;

	float action_get_deadzone(const StringName &p_action);
	void action_set_deadzone(const StringName &p_action, float p_deadzone);
	void action_add_event(const StringName &p_action, RequiredParam<InputEvent> p_event);
	bool action_has_event(const StringName &p_action, RequiredParam<InputEvent> p_event);
	void action_erase_event(const StringName &p_action, RequiredParam<InputEvent> p_event);
	void action_erase_events(const StringName &p_action);

	const List<Ref<InputEvent>> *action_get_events(const StringName &p_action);
	bool event_is_action(RequiredParam<InputEvent> p_event, const StringName &p_action, bool p_exact_match = false) const;
	int event_get_index(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match = false) const;
	bool event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match = false, bool *r_pressed = nullptr, float *r_strength = nullptr, float *r_raw_strength = nullptr, int *r_event_index = nullptr) const;

	const HashMap<StringName, Action> &get_action_map() const;
	void load_from_project_settings();
	void load_default();

	String suggest_actions(const StringName &p_action) const;

	// Layers (#172).
	bool is_builtin_input_action(const StringName &p_action) const;
	bool is_custom_input_action(const StringName &p_action) const;
	InputActionLayer get_input_action_layer(const StringName &p_action) const;
	Ref<InputAction> get_input_action(const StringName &p_action) const;
	Ref<InputActions> get_input_actions(InputActionLayer p_layer) const;
	void set_input_action(const Ref<InputAction> &p_action);
	void set_input_actions(const Ref<InputActions> &p_actions);

	// The actions that can match p_event, in the order of the action map: a superset of the actions for which
	// event_get_index() is not -1. Used by Input::parse_input_event() instead of checking every action.
	void get_event_candidate_actions(const Ref<InputEvent> &p_event, LocalVector<StringName> &r_actions) const;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	String get_builtin_display_name(const String &p_name) const;
	// Use an Ordered Map so insertion order is preserved. We want the elements to be 'grouped' somewhat.
	const HashMap<String, List<Ref<InputEvent>>> &get_builtins();
	const HashMap<String, List<Ref<InputEvent>>> &get_builtins_with_feature_overrides_applied();

	InputMap();
	~InputMap();
};

VARIANT_ENUM_CAST(InputMap::InputActionLayer);
