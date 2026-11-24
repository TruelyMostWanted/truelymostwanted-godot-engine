/**************************************************************************/
/*  key_binding_settings_server.h                                         */
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

#include "core/templates/hash_set.h"
#include "servers/settings/key_binding.h"
#include "servers/settings/list_settings_server.h"

// Settings domain `key_bindings` (#18): the player's key bindings of the InputMap actions (KeyBindingList).
// A binding is addressed by action, device (keyboard, mouse, gamepad) and slot. Applying replaces the key,
// mouse button and joypad events of each action in InputMap; other event types stay. The defaults are the
// InputMap of the project settings at startup, so reset_action() and reset_all() give back the project bindings.
class KeyBindingSettingsServer : public ListSettingsServer {
	GDCLASS(KeyBindingSettingsServer, ListSettingsServer);

	static KeyBindingSettingsServer *singleton;

	bool include_builtin_actions = false; // Project setting `settings/key_bindings/include_builtin_actions`.
	HashSet<String> warned_actions; // Saved actions missing in the project that were reported once.

	static bool _is_managed_event(const Ref<InputEvent> &p_event);
	bool _is_managed_action(const StringName &p_action) const;
	static void _add_action_rows(const StringName &p_action, TypedArray<KeyBinding> &r_rows);
	Array _get_rows() const;
	int _find_row(const String &p_action, KeyBinding::InputDevice p_device, int p_slot) const;
	float _get_default_deadzone(const String &p_action) const;
	void _merge_project_actions();

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;

public:
	static KeyBindingSettingsServer *get_singleton();
	static void register_settings();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual StringName get_list_property() const override;
	virtual StringName get_entry_key() const override;

	// Actions.
	PackedStringArray get_action_names() const;
	bool has_action(const StringName &p_action) const;
	TypedArray<InputEvent> get_events(const StringName &p_action) const;
	void set_action_deadzone(const StringName &p_action, float p_deadzone);
	float get_action_deadzone(const StringName &p_action) const;

	// Slots.
	Ref<InputEvent> get_binding(const StringName &p_action, KeyBinding::InputDevice p_device, int p_slot = 0) const;
	int get_slot_count(const StringName &p_action, KeyBinding::InputDevice p_device) const;
	Error rebind(const StringName &p_action, const Ref<InputEvent> &p_event, int p_slot = 0);
	void clear_binding(const StringName &p_action, KeyBinding::InputDevice p_device, int p_slot = 0);
	PackedStringArray find_conflicts(const Ref<InputEvent> &p_event, const StringName &p_ignore_action = StringName()) const;

	// Reset to the project bindings.
	void reset_action(const StringName &p_action);
	void reset_all();

	String get_event_label(const Ref<InputEvent> &p_event) const;

	KeyBindingSettingsServer();
	~KeyBindingSettingsServer();
};
