/**************************************************************************/
/*  input_map_settings_server.cpp                                         */
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

#include "input_map_settings_server.h"

#include "core/config/project_settings.h"
#include "core/input/input_map.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

static constexpr const char *SETTING_INCLUDE_BUILTIN_ACTIONS = "settings/input_map/include_builtin_actions";

InputMapSettingsServer *InputMapSettingsServer::singleton = nullptr;

InputMapSettingsServer *InputMapSettingsServer::get_singleton() {
	return singleton;
}

void InputMapSettingsServer::register_settings() {
	GLOBAL_DEF(SETTING_INCLUDE_BUILTIN_ACTIONS, false);
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName InputMapSettingsServer::get_domain() const {
	return SNAME("input_map");
}

SettingsServer::ApplyPhase InputMapSettingsServer::get_apply_phase() const {
	// InputMap is loaded from the project settings in Main::setup(), long before this phase.
	return APPLY_PHASE_SERVERS;
}

StringName InputMapSettingsServer::get_list_property() const {
	return SNAME("actions");
}

StringName InputMapSettingsServer::get_entry_key() const {
	return SNAME("action");
}

Ref<Resource> InputMapSettingsServer::_create_default_settings() const {
	Ref<InputMapSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

bool InputMapSettingsServer::_is_managed_event(const Ref<InputEvent> &p_event) {
	return p_event.is_valid() && InputMapAction::get_event_device(p_event) != InputMapAction::INPUT_DEVICE_MAX;
}

bool InputMapSettingsServer::_is_managed_action(const StringName &p_action) const {
	if (include_builtin_actions) {
		return true;
	}
	InputMap *input_map = InputMap::get_singleton();
	return input_map == nullptr || !input_map->get_builtins().has(String(p_action));
}

void InputMapSettingsServer::_add_action_rows(const StringName &p_action, TypedArray<InputMapAction> &r_rows) {
	InputMap *input_map = InputMap::get_singleton();
	const float deadzone = input_map->action_get_deadzone(p_action);
	int slots[InputMapAction::INPUT_DEVICE_MAX] = {};
	bool any = false;
	const List<Ref<InputEvent>> *events = input_map->action_get_events(p_action);
	if (events != nullptr) {
		for (const Ref<InputEvent> &event : *events) {
			if (!_is_managed_event(event)) {
				continue;
			}
			Ref<InputMapAction> row;
			row.instantiate();
			row->set_action(p_action);
			row->set_deadzone(deadzone);
			row->set_input_event(event);
			row->set_slot(slots[row->get_device()]++);
			r_rows.push_back(row);
			any = true;
		}
	}
	if (!any) {
		// An action without bindings keeps one empty row for its deadzone.
		Ref<InputMapAction> row;
		row.instantiate();
		row->set_action(p_action);
		row->set_deadzone(deadzone);
		r_rows.push_back(row);
	}
}

void InputMapSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	InputMap *input_map = InputMap::get_singleton();
	if (input_map == nullptr) {
		return;
	}
	TypedArray<InputMapAction> rows;
	const TypedArray<StringName> actions = input_map->get_actions();
	for (int i = 0; i < actions.size(); i++) {
		const StringName action = actions[i];
		if (_is_managed_action(action)) {
			_add_action_rows(action, rows);
		}
	}
	p_settings->set(get_list_property(), rows);
}

void InputMapSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	const StringName property = get_list_property();
	InputMap *input_map = InputMap::get_singleton();
	if (input_map == nullptr || !p_names.has(String(property))) {
		return;
	}
	const Array rows = p_settings->get(property);

	// Rows grouped per action, in the order of the list.
	Vector<String> order;
	HashMap<String, Vector<Ref<InputMapAction>>> grouped;
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_null() || row->get_action().is_empty()) {
			continue;
		}
		if (!grouped.has(row->get_action())) {
			order.push_back(row->get_action());
			grouped.insert(row->get_action(), Vector<Ref<InputMapAction>>());
		}
		grouped[row->get_action()].push_back(row);
	}

	PackedStringArray missing;
	PackedStringArray invalid;
	for (const String &action : order) {
		if (!input_map->has_action(action)) {
			// Kept in the settings until the next load drops it.
			if (!warned_actions.has(action)) {
				warned_actions.insert(action);
				missing.push_back(action);
			}
			continue;
		}
		const Vector<Ref<InputMapAction>> &action_rows = grouped[action];
		input_map->action_set_deadzone(action, action_rows[0]->get_deadzone());

		// Touch, MIDI and other events are not managed and stay.
		Vector<Ref<InputEvent>> kept;
		const List<Ref<InputEvent>> *events = input_map->action_get_events(action);
		if (events != nullptr) {
			for (const Ref<InputEvent> &event : *events) {
				if (!_is_managed_event(event)) {
					kept.push_back(event);
				}
			}
		}
		input_map->action_erase_events(action);
		for (const Ref<InputEvent> &event : kept) {
			input_map->action_add_event(action, event);
		}

		// Keyboard, mouse, gamepad; each by slot.
		for (int device = 0; device < InputMapAction::INPUT_DEVICE_MAX; device++) {
			int max_slot = -1;
			for (const Ref<InputMapAction> &row : action_rows) {
				if (row->get_device() == device) {
					max_slot = MAX(max_slot, row->get_slot());
				}
			}
			for (int slot = 0; slot <= max_slot; slot++) {
				for (const Ref<InputMapAction> &row : action_rows) {
					if (row->get_device() != device || row->get_slot() != slot || row->is_empty()) {
						continue;
					}
					const Ref<InputEvent> event = row->get_input_event();
					if (event.is_null()) {
						invalid.push_back(vformat("%s: \"%s\"", action, row->get_event()));
						continue;
					}
					input_map->action_add_event(action, event);
				}
			}
		}
	}
	if (!missing.is_empty()) {
		WARN_PRINT(vformat("Input map settings: the project has no action %s; the saved bindings are skipped.", String(", ").join(missing)));
	}
	if (!invalid.is_empty()) {
		WARN_PRINT(vformat("Input map settings: can't read the binding %s; it is skipped.", String(", ").join(invalid)));
	}
}

void InputMapSettingsServer::_merge_project_actions() {
	// The saved rows replace the project bindings action by action. Actions added to the project since
	// the file was saved keep their project bindings; actions that no longer exist are dropped.
	const StringName property = get_list_property();
	const Ref<Resource> pending = get_pending_settings();
	const Array project_rows = get_default_settings()->get(property);
	const Array saved_rows = pending->get(property);

	HashSet<String> project_actions;
	for (int i = 0; i < project_rows.size(); i++) {
		const Ref<InputMapAction> row = project_rows[i];
		if (row.is_valid()) {
			project_actions.insert(row->get_action());
		}
	}
	HashSet<String> saved_actions;
	PackedStringArray dropped;
	for (int i = 0; i < saved_rows.size(); i++) {
		const Ref<InputMapAction> row = saved_rows[i];
		if (row.is_null()) {
			continue;
		}
		if (project_actions.has(row->get_action())) {
			saved_actions.insert(row->get_action());
		} else if (!dropped.has(row->get_action()) && !warned_actions.has(row->get_action())) {
			warned_actions.insert(row->get_action());
			dropped.push_back(row->get_action());
		}
	}

	TypedArray<InputMapAction> merged;
	HashSet<String> done;
	for (int i = 0; i < project_rows.size(); i++) {
		const Ref<InputMapAction> project_row = project_rows[i];
		if (project_row.is_null()) {
			continue;
		}
		const String action = project_row->get_action();
		if (!saved_actions.has(action)) {
			merged.push_back(copy_value(project_row));
			continue;
		}
		if (done.has(action)) {
			continue;
		}
		done.insert(action);
		for (int j = 0; j < saved_rows.size(); j++) {
			const Ref<InputMapAction> saved_row = saved_rows[j];
			if (saved_row.is_valid() && saved_row->get_action() == action) {
				merged.push_back(saved_row);
			}
		}
	}
	if (!values_equal(merged, saved_rows)) {
		pending->set(property, merged);
	}
	if (!dropped.is_empty()) {
		WARN_PRINT(vformat("Input map settings: the project has no action %s; the saved bindings are skipped.", String(", ").join(dropped)));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

Array InputMapSettingsServer::_get_rows() const {
	return get_pending_settings()->get(get_list_property());
}

int InputMapSettingsServer::_find_row(const String &p_action, InputMapAction::InputDevice p_device, int p_slot) const {
	const Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && row->get_action() == p_action && row->get_device() == p_device && row->get_slot() == p_slot) {
			return i;
		}
	}
	return -1;
}

float InputMapSettingsServer::_get_default_deadzone(const String &p_action) const {
	const Array rows = get_default_settings()->get(get_list_property());
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && row->get_action() == p_action) {
			return row->get_deadzone();
		}
	}
	return InputMapAction::DEFAULT_DEADZONE;
}

PackedStringArray InputMapSettingsServer::get_action_names() const {
	PackedStringArray names;
	const Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && !names.has(row->get_action())) {
			names.push_back(row->get_action());
		}
	}
	return names;
}

bool InputMapSettingsServer::has_action(const StringName &p_action) const {
	return find_entry(String(p_action)) >= 0;
}

TypedArray<InputEvent> InputMapSettingsServer::get_events(const StringName &p_action) const {
	TypedArray<InputEvent> events;
	for (int device = 0; device < InputMapAction::INPUT_DEVICE_MAX; device++) {
		const int count = get_slot_count(p_action, InputMapAction::InputDevice(device));
		for (int slot = 0; slot < count; slot++) {
			const Ref<InputEvent> event = get_event(p_action, InputMapAction::InputDevice(device), slot);
			if (event.is_valid()) {
				events.push_back(event);
			}
		}
	}
	return events;
}

void InputMapSettingsServer::set_action_deadzone(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(!has_action(p_action), vformat("Input map settings: there is no action \"%s\".", p_action));
	const float deadzone = CLAMP(p_deadzone, 0.0f, 1.0f);
	const Array rows = _get_rows();
	bool changed = false;
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && row->get_action() == String(p_action) && row->get_deadzone() != deadzone) {
			row->set_deadzone(deadzone);
			changed = true;
		}
	}
	if (changed) {
		notify_value_changed(get_list_property());
	}
}

float InputMapSettingsServer::get_action_deadzone(const StringName &p_action) const {
	const int index = find_entry(String(p_action));
	ERR_FAIL_COND_V_MSG(index < 0, InputMapAction::DEFAULT_DEADZONE, vformat("Input map settings: there is no action \"%s\".", p_action));
	const Ref<InputMapAction> row = get_entry(index);
	return row.is_valid() ? row->get_deadzone() : InputMapAction::DEFAULT_DEADZONE;
}

Ref<InputEvent> InputMapSettingsServer::get_event(const StringName &p_action, InputMapAction::InputDevice p_device, int p_slot) const {
	const int index = _find_row(p_action, p_device, p_slot);
	if (index < 0) {
		return Ref<InputEvent>();
	}
	const Ref<InputMapAction> row = get_entry(index);
	return row.is_valid() ? row->get_input_event() : Ref<InputEvent>();
}

int InputMapSettingsServer::get_slot_count(const StringName &p_action, InputMapAction::InputDevice p_device) const {
	int count = 0;
	const Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && row->get_action() == String(p_action) && row->get_device() == p_device && !row->is_empty()) {
			count = MAX(count, row->get_slot() + 1);
		}
	}
	return count;
}

Error InputMapSettingsServer::rebind(const StringName &p_action, const Ref<InputEvent> &p_event, int p_slot) {
	ERR_FAIL_COND_V(p_event.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_slot < 0, ERR_INVALID_PARAMETER, "Input map settings: the slot must be 0 or greater.");
	ERR_FAIL_COND_V_MSG(!has_action(p_action), ERR_DOES_NOT_EXIST, vformat("Input map settings: there is no action \"%s\".", p_action));
	const InputMapAction::InputDevice device = InputMapAction::get_event_device(p_event);
	ERR_FAIL_COND_V_MSG(device == InputMapAction::INPUT_DEVICE_MAX, ERR_INVALID_PARAMETER, vformat("Input map settings: a %s can't be bound; use a key, mouse button, joypad button or joypad axis.", p_event->get_class()));
	const String text = InputMapAction::event_to_text(p_event);
	ERR_FAIL_COND_V_MSG(text.is_empty(), ERR_INVALID_PARAMETER, vformat("Input map settings: the event \"%s\" has no key, button or axis.", p_event->as_text()));

	const int index = _find_row(p_action, device, p_slot);
	if (index >= 0) {
		const Ref<InputMapAction> row = get_entry(index);
		if (row->get_event() == text && row->get_device_id() == -1) {
			return OK;
		}
		// The pressed pad's id changes when pads are reconnected: the binding is for all devices.
		row->set_event(text);
		row->set_device_id(-1);
		notify_value_changed(get_list_property());
		return OK;
	}
	Ref<InputMapAction> row;
	row.instantiate();
	row->set_action(p_action);
	row->set_device(device);
	row->set_slot(p_slot);
	row->set_event(text);
	row->set_device_id(-1);
	row->set_deadzone(get_action_deadzone(p_action));
	// After the last row of the action, so the file stays grouped by action.
	const Array rows = _get_rows();
	int at_index = -1;
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> other = rows[i];
		if (other.is_valid() && other->get_action() == String(p_action)) {
			at_index = i + 1;
		}
	}
	add_entry(row, at_index);
	return OK;
}

void InputMapSettingsServer::clear_event(const StringName &p_action, InputMapAction::InputDevice p_device, int p_slot) {
	ERR_FAIL_COND_MSG(!has_action(p_action), vformat("Input map settings: there is no action \"%s\".", p_action));
	const int index = _find_row(p_action, p_device, p_slot);
	if (index < 0) {
		return;
	}
	const Ref<InputMapAction> row = get_entry(index);
	if (row->is_empty()) {
		return;
	}
	// The empty row stays, so the cleared slot is stored and the project binding does not come back.
	row->set_event(String());
	notify_value_changed(get_list_property());
}

PackedStringArray InputMapSettingsServer::find_conflicts(const Ref<InputEvent> &p_event, const StringName &p_ignore_action) const {
	PackedStringArray actions;
	ERR_FAIL_COND_V(p_event.is_null(), actions);
	const String text = InputMapAction::event_to_text(p_event);
	if (text.is_empty()) {
		return actions;
	}
	const int device_id = p_event->get_device();
	const Array rows = _get_rows();
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_null() || row->is_empty() || row->get_action() == String(p_ignore_action) || actions.has(row->get_action())) {
			continue;
		}
		if (row->get_device_id() != -1 && device_id != -1 && row->get_device_id() != device_id) {
			continue;
		}
		bool same = row->get_event() == text;
		if (!same) {
			// A pressed key has all codes, a stored one only one of them (`Key:W` matches a pressed W).
			const Ref<InputEvent> bound = row->get_input_event();
			same = bound.is_valid() && bound->is_match(p_event, true);
		}
		if (same) {
			actions.push_back(row->get_action());
		}
	}
	return actions;
}

void InputMapSettingsServer::reset_action(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!has_action(p_action), vformat("Input map settings: there is no action \"%s\".", p_action));
	const String action = p_action;
	const Array project_rows = get_default_settings()->get(get_list_property());
	const Array rows = _get_rows();
	TypedArray<InputMapAction> result;
	bool inserted = false;
	for (int i = 0; i < rows.size(); i++) {
		const Ref<InputMapAction> row = rows[i];
		if (row.is_valid() && row->get_action() == action) {
			if (!inserted) {
				inserted = true;
				for (int j = 0; j < project_rows.size(); j++) {
					const Ref<InputMapAction> project_row = project_rows[j];
					if (project_row.is_valid() && project_row->get_action() == action) {
						result.push_back(copy_value(project_row));
					}
				}
			}
			continue;
		}
		result.push_back(rows[i]);
	}
	if (values_equal(result, rows)) {
		return;
	}
	get_pending_settings()->set(get_list_property(), result);
	notify_value_changed(get_list_property());
}

void InputMapSettingsServer::reset_all() {
	reset_to_defaults();
}

String InputMapSettingsServer::get_event_label(const Ref<InputEvent> &p_event) const {
	return InputMapAction::event_to_label(p_event);
}

void InputMapSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_action_names"), &InputMapSettingsServer::get_action_names);
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputMapSettingsServer::has_action);
	ClassDB::bind_method(D_METHOD("get_events", "action"), &InputMapSettingsServer::get_events);
	ClassDB::bind_method(D_METHOD("set_action_deadzone", "action", "deadzone"), &InputMapSettingsServer::set_action_deadzone);
	ClassDB::bind_method(D_METHOD("get_action_deadzone", "action"), &InputMapSettingsServer::get_action_deadzone);
	ClassDB::bind_method(D_METHOD("get_event", "action", "device", "slot"), &InputMapSettingsServer::get_event, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_slot_count", "action", "device"), &InputMapSettingsServer::get_slot_count);
	ClassDB::bind_method(D_METHOD("rebind", "action", "event", "slot"), &InputMapSettingsServer::rebind, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("clear_event", "action", "device", "slot"), &InputMapSettingsServer::clear_event, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("find_conflicts", "event", "ignore_action"), &InputMapSettingsServer::find_conflicts, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("reset_action", "action"), &InputMapSettingsServer::reset_action);
	ClassDB::bind_method(D_METHOD("reset_all"), &InputMapSettingsServer::reset_all);
	ClassDB::bind_method(D_METHOD("get_event_label", "event"), &InputMapSettingsServer::get_event_label);
}

InputMapSettingsServer::InputMapSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings != nullptr && project_settings->has_setting(SETTING_INCLUDE_BUILTIN_ACTIONS)) {
		include_builtin_actions = project_settings->get_setting_with_override(SETTING_INCLUDE_BUILTIN_ACTIONS);
	}
	connect(SNAME("loaded"), callable_mp(this, &InputMapSettingsServer::_merge_project_actions));
}

InputMapSettingsServer::~InputMapSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
