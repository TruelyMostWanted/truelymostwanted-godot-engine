/**************************************************************************/
/*  input_action_rebinder.h                                               */
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
#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/os/keyboard.h"
#include "core/templates/local_vector.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"

class InputMapSettingsServer;

// The "press a key now" flow of a rebinding menu (#191): begin() picks an action and an event index, the UI
// forwards its input events to update(), which filters them and commits the first usable one through
// InputMapSettingsServer. Conflicts with other actions are handled by `conflict_mode`.
// RefCounted, not a singleton: several rebinders can run at once (local co-op). Main thread only.
class InputActionRebinder : public RefCounted {
	GDCLASS(InputActionRebinder, RefCounted);

public:
	enum DeviceFilter {
		DEVICE_ANY,
		DEVICE_KEYBOARD_MOUSE,
		DEVICE_GAMEPAD,
	};

	enum ConflictMode {
		CONFLICT_ASK,
		CONFLICT_REJECT,
		CONFLICT_SWAP,
		CONFLICT_ALLOW,
	};

	static constexpr float DEFAULT_AXIS_THRESHOLD = 0.5f;

private:
	struct Conflict {
		StringName action;
		int event_index = -1; // Index within the device class of the filter.
	};

	DeviceFilter device_filter = DEVICE_ANY;
	int gamepad_device = -1;
	ConflictMode conflict_mode = CONFLICT_ASK;
	float axis_threshold = DEFAULT_AXIS_THRESHOLD;
	PackedStringArray ignored_actions;
	ObjectID server_id; // C++ only (tests): another InputMapSettingsServer than the singleton.

	bool active = false;
	StringName action;
	int event_index = -1;
	Key pending_modifier = Key::NONE; // A modifier key pressed alone; bound if it is released before another key.
	Ref<InputEvent> pending_modifier_event;
	bool conflict_pending = false; // CONFLICT_ASK: waiting for resolve_conflict_swap(), resolve_conflict_allow() or cancel().
	Ref<InputEvent> conflict_event; // The normalized event of the last conflict.
	LocalVector<Conflict> conflicts;

	InputMapSettingsServer *_get_server() const;
	bool _is_in_device_class(const Ref<InputEvent> &p_event) const;
	// Index in the event list of the `p_index`-th event of the device class; -1 if there is none.
	int _get_list_index(const TypedArray<InputEvent> &p_events, int p_index) const;
	int _get_class_index(const TypedArray<InputEvent> &p_events, int p_list_index) const;
	static bool _events_match(const Ref<InputEvent> &p_bound, const Ref<InputEvent> &p_event, const String &p_text);
	void _find_conflicts(InputMapSettingsServer *p_server, const Ref<InputEvent> &p_event, const Ref<InputEvent> &p_pressed);
	void _accept(const Ref<InputEvent> &p_pressed);
	Error _commit(const Ref<InputEvent> &p_event, bool p_swap);
	void _emit_conflicts();
	void _reset_session();

protected:
	static void _bind_methods();

public:
	void set_device_filter(DeviceFilter p_filter);
	DeviceFilter get_device_filter() const;
	void set_gamepad_device(int p_device);
	int get_gamepad_device() const;
	void set_conflict_mode(ConflictMode p_mode);
	ConflictMode get_conflict_mode() const;
	void set_axis_threshold(float p_threshold);
	float get_axis_threshold() const;
	void set_ignored_actions(const PackedStringArray &p_actions);
	PackedStringArray get_ignored_actions() const;

	Error begin(const StringName &p_action, int p_event_index);
	bool update(const Ref<InputEvent> &p_event);
	void cancel();
	Error resolve_conflict_swap();
	Error resolve_conflict_allow();

	bool is_active() const;
	bool is_conflict_pending() const;
	StringName get_action() const;
	int get_event_index() const;
	Ref<InputEvent> get_conflict_event() const;

	// C++ only: commit through `p_server` instead of the InputMapSettingsServer singleton (tests).
	void set_settings_server(InputMapSettingsServer *p_server);

	InputActionRebinder() {}
};

VARIANT_ENUM_CAST(InputActionRebinder::DeviceFilter);
VARIANT_ENUM_CAST(InputActionRebinder::ConflictMode);
