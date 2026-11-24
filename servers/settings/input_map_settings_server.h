/**************************************************************************/
/*  input_map_settings_server.h                                           */
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
#include "core/templates/hash_set.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "servers/settings/input_map_settings_records.h"
#include "servers/settings/list_settings_server.h"

// Settings domain `input_map` (#18, layers #172): the player's events of the InputMap actions.
// The settings resource is an InputActions with one InputAction per action. The file stores it as
// InputMapSettingsActions (one InputMapSettingsAction per action with one InputMapSettingsEvent per event;
// CSV: one InputMapSettingsEventRow per event). The events of the settings are always in their canonical form
// (one code, device -1 or one joypad), so a value read back from the file equals the value that was saved.
// Applying replaces the key, mouse button and joypad events of each
// action in InputMap; other event types stay. The defaults are the InputMap of the project settings at
// startup, so reset_action() and reset_all() give back the project events.
// `save_mode` decides which layer is stored: the custom actions only (default) or the built-in `ui_*`
// actions too.
class InputMapSettingsServer : public ListSettingsServer {
	GDCLASS(InputMapSettingsServer, ListSettingsServer);

public:
	enum SaveMode {
		SAVE_MODE_CUSTOM_ONLY,
		SAVE_MODE_ALL,
	};

private:
	static InputMapSettingsServer *singleton;

	SaveMode save_mode = SAVE_MODE_CUSTOM_ONLY; // Project setting `settings/input_map/save_mode`.
	HashSet<String> warned_actions; // Saved actions missing in the project that were reported once.

	static bool _is_managed_event(const Ref<InputEvent> &p_event);
	bool _is_managed_action(const StringName &p_action) const;
	static Ref<InputAction> _make_action(const StringName &p_action);
	Ref<InputActions> _get_actions() const;
	Ref<InputAction> _get_action(const StringName &p_action) const;
	Ref<InputAction> _ensure_action(const StringName &p_action);
	static int _find_slot(const Ref<InputAction> &p_action, InputAction::InputDevice p_device, int p_slot);
	static Ref<InputEvent> _normalized_event(const Ref<InputEvent> &p_event, Error &r_error);
	void _merge_project_actions();
	static TypedArray<InputEvent> _parse_events(const String &p_text, PackedStringArray &r_invalid);
	static TypedArray<InputEvent> _canonical_events(const TypedArray<InputEvent> &p_events);

protected:
	static void _bind_methods();

	virtual Dictionary _migrate_settings(int p_from_version, const Dictionary &p_data) override;
	virtual Ref<Resource> _settings_to_storage(const Ref<Resource> &p_settings) const override;
	virtual Ref<Resource> _settings_from_storage(const Ref<Resource> &p_storage) const override;
	virtual Ref<Resource> _settings_to_storage_for_format(const Ref<Resource> &p_settings, ResourceTextSerializer::Format p_format) const override;
	virtual String _get_storage_type_for_text(const String &p_text, ResourceTextSerializer::Format p_format) const override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;

public:
	static InputMapSettingsServer *get_singleton();
	static void register_settings();

	// The text form of events: `Keyboard:Ctrl+S; KeyboardLabel:E; Mouse:Left; Gamepad:A@Any; GamepadAxis:LeftX-`
	// (InputMapSettingsEvent::to_text()). Events without a text form are skipped, and so are invalid items of the
	// text; the names of version 2 (`Key:`, `PhysicalKey:`, `JoypadButton:`, ...) are read, too.
	static String events_to_text(const TypedArray<InputEvent> &p_events);
	static TypedArray<InputEvent> text_to_events(const String &p_text);

	virtual StringName get_domain() const override;
	virtual int get_format_version() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual StringName get_list_property() const override;
	virtual StringName get_entry_key() const override;

	void set_save_mode(SaveMode p_save_mode);
	SaveMode get_save_mode() const;

	// Actions.
	PackedStringArray get_action_names() const;
	bool has_action(const StringName &p_action) const;
	Ref<InputAction> get_input_action(const StringName &p_action) const;
	TypedArray<InputEvent> get_events(const StringName &p_action) const;
	void set_action_deadzone(const StringName &p_action, float p_deadzone);
	float get_action_deadzone(const StringName &p_action) const;

	// Events.
	Error add_event(const StringName &p_action, const Ref<InputEvent> &p_event);
	Error remove_event(const StringName &p_action, const Ref<InputEvent> &p_event);

	// Slots: slot n of a device is the n-th event of that device in the action.
	Ref<InputEvent> get_event(const StringName &p_action, InputAction::InputDevice p_device, int p_slot = 0) const;
	int get_slot_count(const StringName &p_action, InputAction::InputDevice p_device) const;
	Error rebind(const StringName &p_action, const Ref<InputEvent> &p_event, int p_slot = 0);
	void clear_event(const StringName &p_action, InputAction::InputDevice p_device, int p_slot = 0);
	PackedStringArray find_conflicts(const Ref<InputEvent> &p_event, const StringName &p_ignore_action = StringName()) const;

	// Reset to the project bindings.
	void reset_action(const StringName &p_action);
	void reset_all();

	String get_event_label(const Ref<InputEvent> &p_event) const;

	InputMapSettingsServer();
	~InputMapSettingsServer();
};

VARIANT_ENUM_CAST(InputMapSettingsServer::SaveMode);
