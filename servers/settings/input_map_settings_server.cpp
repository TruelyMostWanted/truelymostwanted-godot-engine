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
#include "core/os/keyboard.h"
#include "core/templates/hash_map.h"
#include "servers/display/display_server.h"

static constexpr const char *SETTING_SAVE_MODE = "settings/input_map/save_mode";

/////////////////////////////////////////////////////////////////////////////
// Labels for the UI (English); the stored text form is events_to_text().

static constexpr int MOUSE_BUTTON_NAME_COUNT = 9; // MouseButton::LEFT (1) to MouseButton::MB_XBUTTON2 (9).
static constexpr int JOY_BUTTON_NAME_COUNT = int(JoyButton::SDL_MAX);
static constexpr int JOY_AXIS_NAME_COUNT = int(JoyAxis::SDL_MAX);

static bool _is_modifier_key(Key p_key) {
	return p_key == Key::SHIFT || p_key == Key::CTRL || p_key == Key::ALT || p_key == Key::META;
}

static const char *_mouse_button_labels[] = { "Left Mouse Button", "Right Mouse Button", "Middle Mouse Button", "Mouse Wheel Up", "Mouse Wheel Down", "Mouse Wheel Left", "Mouse Wheel Right", "Mouse Thumb Button 1", "Mouse Thumb Button 2" };
static const char *_joy_button_labels[] = { "A", "B", "X", "Y", "Back", "Guide", "Start", "Left Stick", "Right Stick", "Left Shoulder", "Right Shoulder", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Misc 1", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad", "Misc 2", "Misc 3", "Misc 4", "Misc 5", "Misc 6" };
// Negative and positive direction per axis (Y is positive downwards).
static const char *_joy_axis_labels[][2] = {
	{ "Left Stick Left", "Left Stick Right" },
	{ "Left Stick Up", "Left Stick Down" },
	{ "Right Stick Left", "Right Stick Right" },
	{ "Right Stick Up", "Right Stick Down" },
	{ "Left Trigger", "Left Trigger" },
	{ "Right Trigger", "Right Trigger" },
};

// Display servers that implement keyboard_get_label_from_physical(); the others print an error.
static bool _has_keyboard_layout() {
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr) {
		return false;
	}
	const String server_name = display_server->get_name();
	return server_name == "Windows" || server_name == "X11" || server_name == "Wayland" || server_name == "macOS" || server_name == "embedded";
}

static String _event_to_label(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid()) {
		Key code = key_event->get_keycode();
		if (code == Key::NONE && key_event->get_physical_keycode() != Key::NONE) {
			code = key_event->get_physical_keycode();
			if (_has_keyboard_layout()) {
				const Key label = DisplayServer::get_singleton()->keyboard_get_label_from_physical(code);
				if (label != Key::NONE) {
					code = label;
				}
			}
		}
		if (code == Key::NONE) {
			code = key_event->get_key_label();
		}
		if (code == Key::NONE) {
			return String();
		}
		if (_is_modifier_key(code)) {
			return keycode_get_string(code);
		}
		return keycode_get_string(code | KeyModifierMask(key_event->get_modifiers_mask()));
	}
	const Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_valid()) {
		const int index = int(mouse_event->get_button_index());
		const String button = (index >= 1 && index <= MOUSE_BUTTON_NAME_COUNT) ? String(_mouse_button_labels[index - 1]) : vformat("Mouse Button %d", index);
		const Key modifiers = Key::NONE | KeyModifierMask(mouse_event->get_modifiers_mask());
		const String prefix = modifiers == Key::NONE ? String() : keycode_get_string(modifiers) + "+";
		return prefix + button;
	}
	const Ref<InputEventJoypadButton> button_event = p_event;
	if (button_event.is_valid()) {
		const int index = int(button_event->get_button_index());
		return index >= 0 && index < JOY_BUTTON_NAME_COUNT ? vformat("Joypad %s", _joy_button_labels[index]) : vformat("Joypad Button %d", index);
	}
	const Ref<InputEventJoypadMotion> motion_event = p_event;
	if (motion_event.is_valid()) {
		const int index = int(motion_event->get_axis());
		const bool positive = motion_event->get_axis_value() >= 0.0f;
		if (index >= 0 && index < JOY_AXIS_NAME_COUNT) {
			return String(_joy_axis_labels[index][positive ? 1 : 0]);
		}
		return vformat("Joypad Axis %d%s", index, positive ? "+" : "-");
	}
	return p_event.is_valid() ? p_event->as_text() : String();
}

/////////////////////////////////////////////////////////////////////////////
// Storage form.

String InputMapSettingsServer::events_to_text(const TypedArray<InputEvent> &p_events) {
	String result;
	for (int i = 0; i < p_events.size(); i++) {
		const Ref<InputEvent> event = p_events[i];
		const Ref<InputMapSettingsEvent> record = InputMapSettingsEvent::from_event(event);
		if (record.is_null()) {
			continue;
		}
		if (!result.is_empty()) {
			result += "; ";
		}
		result += record->to_text();
	}
	return result;
}

TypedArray<InputEvent> InputMapSettingsServer::_parse_events(const String &p_text, PackedStringArray &r_invalid) {
	TypedArray<InputEvent> result;
	const Vector<String> items = p_text.split(";", false);
	for (const String &raw_item : items) {
		const String item = raw_item.strip_edges();
		if (item.is_empty()) {
			continue;
		}
		const Ref<InputMapSettingsEvent> record = InputMapSettingsEvent::from_text(item);
		const Ref<InputEvent> event = record.is_valid() ? record->to_event() : Ref<InputEvent>();
		if (event.is_null()) {
			r_invalid.push_back(item);
			continue;
		}
		result.push_back(event);
	}
	return result;
}

TypedArray<InputEvent> InputMapSettingsServer::text_to_events(const String &p_text) {
	PackedStringArray invalid;
	return _parse_events(p_text, invalid);
}

TypedArray<InputEvent> InputMapSettingsServer::_canonical_events(const TypedArray<InputEvent> &p_events) {
	PackedStringArray invalid;
	return _parse_events(events_to_text(p_events), invalid);
}

Ref<Resource> InputMapSettingsServer::_settings_to_storage(const Ref<Resource> &p_settings) const {
	const Ref<InputActions> source = p_settings;
	ERR_FAIL_COND_V(source.is_null(), Ref<Resource>());
	const InputMap *input_map = InputMap::get_singleton();
	TypedArray<InputMapSettingsAction> records;
	for (const Ref<InputAction> &action : source->get_action_list()) {
		Ref<InputMapSettingsAction> record;
		record.instantiate();
		record->set_action(action->get_action());
		record->set_deadzone(action->get_deadzone());
		const bool builtin = input_map != nullptr && input_map->is_builtin_input_action(action->get_action());
		record->set_layer(builtin ? InputMapSettingsAction::LAYER_BUILT_IN : InputMapSettingsAction::LAYER_CUSTOM);
		TypedArray<InputMapSettingsEvent> event_records;
		for (const Ref<InputEvent> &event : action->get_event_list()) {
			const Ref<InputMapSettingsEvent> event_record = InputMapSettingsEvent::from_event(event);
			if (event_record.is_valid()) {
				event_records.push_back(event_record);
			}
		}
		record->set_input_events(event_records);
		records.push_back(record);
	}
	Ref<InputMapSettingsActions> storage;
	storage.instantiate();
	storage->set_actions(records);
	return storage;
}

Ref<Resource> InputMapSettingsServer::_settings_to_storage_for_format(const Ref<Resource> &p_settings, ResourceTextSerializer::Format p_format) const {
	if (p_format != ResourceTextSerializer::FORMAT_CSV) {
		return _settings_to_storage(p_settings);
	}
	// CSV: one row per event; an action without events is one row with an empty input.
	const Ref<InputMapSettingsActions> nested = _settings_to_storage(p_settings);
	ERR_FAIL_COND_V(nested.is_null(), Ref<Resource>());
	TypedArray<InputMapSettingsEventRow> rows;
	const TypedArray<InputMapSettingsAction> records = nested->get_actions();
	for (int i = 0; i < records.size(); i++) {
		const Ref<InputMapSettingsAction> record = records[i];
		const TypedArray<InputMapSettingsEvent> event_records = record->get_input_events();
		if (event_records.is_empty()) {
			Ref<InputMapSettingsEventRow> row;
			row.instantiate();
			row->set_action(record->get_action());
			row->set_index(-1);
			row->set_deadzone(record->get_deadzone());
			rows.push_back(row);
			continue;
		}
		for (int j = 0; j < event_records.size(); j++) {
			const Ref<InputMapSettingsEvent> event_record = event_records[j];
			Ref<InputMapSettingsEventRow> row;
			row.instantiate();
			row->set_action(record->get_action());
			row->set_index(j);
			row->set_device(event_record->get_device());
			row->set_input(event_record->get_input());
			row->set_direction(event_record->get_direction());
			row->set_modifiers(event_record->get_modifiers());
			row->set_key_mode(event_record->get_key_mode());
			row->set_device_id(event_record->get_device_id());
			row->set_deadzone(record->get_deadzone());
			rows.push_back(row);
		}
	}
	Ref<InputMapSettingsEventRows> storage;
	storage.instantiate();
	storage->set_actions(rows);
	return storage;
}

String InputMapSettingsServer::_get_storage_type_for_text(const String &p_text, ResourceTextSerializer::Format p_format) const {
	// Version 2 files: `action,deadzone,bindings` in CSV, the type InputMapSettingsRows in the other formats
	// (JSON and INI files are migrated as a tree, see _migrate_settings()).
	if (p_format == ResourceTextSerializer::FORMAT_CSV) {
		const String header = p_text.get_slicec('\n', 0);
		return header.contains("bindings") ? String(InputMapSettingsRows::get_class_static()) : String(InputMapSettingsEventRows::get_class_static());
	}
	if (p_text.contains(InputMapSettingsRows::get_class_static())) {
		return InputMapSettingsRows::get_class_static();
	}
	return InputMapSettingsActions::get_class_static();
}

Ref<Resource> InputMapSettingsServer::_settings_from_storage(const Ref<Resource> &p_storage) const {
	TypedArray<InputAction> list;
	PackedStringArray invalid;

	const Ref<InputMapSettingsActions> nested = p_storage;
	const Ref<InputMapSettingsEventRows> flat = p_storage;
	const Ref<InputMapSettingsRows> legacy = p_storage;
	ERR_FAIL_COND_V(nested.is_null() && flat.is_null() && legacy.is_null(), Ref<Resource>());

	if (nested.is_valid()) {
		const TypedArray<InputMapSettingsAction> records = nested->get_actions();
		for (int i = 0; i < records.size(); i++) {
			const Ref<InputMapSettingsAction> record = records[i];
			if (record.is_null()) {
				continue;
			}
			Ref<InputAction> action;
			action.instantiate(record->get_action(), record->get_deadzone());
			TypedArray<InputEvent> events;
			const TypedArray<InputMapSettingsEvent> event_records = record->get_input_events();
			for (int j = 0; j < event_records.size(); j++) {
				const Ref<InputMapSettingsEvent> event_record = event_records[j];
				const Ref<InputEvent> event = event_record.is_valid() ? event_record->to_event() : Ref<InputEvent>();
				if (event.is_null()) {
					invalid.push_back(vformat("%d of %s", j, record->get_action()));
					continue;
				}
				events.push_back(event);
			}
			action->set_events(events);
			list.push_back(action);
		}
	} else if (flat.is_valid()) {
		// The rows of one action follow each other; the first row has the deadzone.
		const TypedArray<InputMapSettingsEventRow> rows = flat->get_actions();
		HashMap<StringName, Ref<InputAction>> by_name;
		for (int i = 0; i < rows.size(); i++) {
			const Ref<InputMapSettingsEventRow> row = rows[i];
			if (row.is_null()) {
				continue;
			}
			Ref<InputAction> action;
			HashMap<StringName, Ref<InputAction>>::Iterator found = by_name.find(row->get_action());
			if (found == by_name.end()) {
				action.instantiate(row->get_action(), row->get_deadzone());
				by_name.insert(row->get_action(), action);
				list.push_back(action);
			} else {
				action = found->value;
			}
			if (row->get_input().is_empty()) {
				continue; // An action without events.
			}
			Ref<InputMapSettingsEvent> event_record;
			event_record.instantiate();
			event_record->set_device(row->get_device());
			event_record->set_input(row->get_input());
			event_record->set_direction(row->get_direction());
			event_record->set_modifiers(row->get_modifiers());
			event_record->set_key_mode(row->get_key_mode());
			event_record->set_device_id(row->get_device_id());
			const Ref<InputEvent> event = event_record->to_event();
			if (event.is_null()) {
				invalid.push_back(vformat("%d of %s", row->get_index(), row->get_action()));
				continue;
			}
			action->add_event(event);
		}
	} else {
		const TypedArray<InputMapSettingsRow> rows = legacy->get_actions();
		for (int i = 0; i < rows.size(); i++) {
			const Ref<InputMapSettingsRow> row = rows[i];
			if (row.is_null()) {
				continue;
			}
			PackedStringArray row_invalid;
			Ref<InputAction> action;
			action.instantiate(row->get_action(), row->get_deadzone());
			action->set_events(_parse_events(row->get_bindings(), row_invalid));
			for (const String &item : row_invalid) {
				invalid.push_back(vformat("\"%s\" of %s", item, row->get_action()));
			}
			list.push_back(action);
		}
	}
	if (!invalid.is_empty()) {
		WARN_PRINT(vformat("Input map settings: can't read the binding %s; it is skipped.", String(", ").join(invalid)));
	}
	Ref<InputActions> result;
	result.instantiate();
	result->set_actions(list);
	return result;
}

int InputMapSettingsServer::get_format_version() const {
	// 3: the file holds InputMapSettingsActions, CSV InputMapSettingsEventRows (#193).
	// 2: InputMapSettingsRows, the events of an action as one text cell (#172).
	// 1: InputActions with the property InputAction.bindings (#172).
	return 3;
}

Dictionary InputMapSettingsServer::_migrate_settings(int p_from_version, const Dictionary &p_data) {
	Dictionary data = SettingsServer::_migrate_settings(p_from_version, p_data);
	const String old_type = String(data.get("$type", String()));
	if (p_from_version >= 3 || (old_type != "InputActions" && old_type != String(InputMapSettingsRows::get_class_static()))) {
		return data;
	}
	// Version 1 had the same keys under the type InputActions, version 2 under InputMapSettingsRows. JSON and INI kept
	// `bindings`; XML, TOML and YAML files of version 1 were read through InputActions, which no longer has it:
	// those actions keep their project events.
	data = data.duplicate();
	data["$type"] = String(InputMapSettingsActions::get_class_static());
	const String list_property = get_list_property();
	const Array old_rows = data.get(list_property, Array());
	static const char *device_names[] = { "Keyboard", "Mouse", "Gamepad" };
	static const char *key_mode_names[] = { "Physical", "Label" };
	Array actions;
	int skipped = 0;
	for (int i = 0; i < old_rows.size(); i++) {
		const Dictionary row = old_rows[i];
		if (!row.has("bindings")) {
			skipped++;
			continue;
		}
		Array event_dictionaries;
		PackedStringArray invalid;
		const Vector<String> items = String(row["bindings"]).split(";", false);
		for (const String &item : items) {
			const Ref<InputMapSettingsEvent> record = InputMapSettingsEvent::from_text(item);
			if (record.is_null()) {
				invalid.push_back(item.strip_edges());
				continue;
			}
			Dictionary event_dictionary;
			event_dictionary["device"] = String(device_names[int(record->get_device())]);
			event_dictionary["input"] = record->get_input();
			event_dictionary["direction"] = record->get_direction();
			event_dictionary["modifiers"] = record->get_modifiers();
			event_dictionary["key_mode"] = String(key_mode_names[int(record->get_key_mode())]);
			event_dictionary["device_id"] = record->get_device_id();
			event_dictionaries.push_back(event_dictionary);
		}
		if (!invalid.is_empty()) {
			WARN_PRINT(vformat("Input map settings: can't read the binding %s of %s; it is skipped.", String(", ").join(invalid), String(row.get("action", String()))));
		}
		Dictionary action;
		action["action"] = row.get("action", String());
		if (row.has("deadzone")) {
			action["deadzone"] = row["deadzone"];
		}
		action["input_events"] = event_dictionaries;
		actions.push_back(action);
	}
	data[list_property] = actions;
	if (skipped > 0) {
		WARN_PRINT(vformat("Input map settings: the bindings of %d actions in the old settings file can't be read; they use the project events until the next save.", skipped));
	}
	return data;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

InputMapSettingsServer *InputMapSettingsServer::singleton = nullptr;

InputMapSettingsServer *InputMapSettingsServer::get_singleton() {
	return singleton;
}

void InputMapSettingsServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_SAVE_MODE, PROPERTY_HINT_ENUM, "Custom Only,All"), int(SAVE_MODE_CUSTOM_ONLY));
}

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

void InputMapSettingsServer::set_save_mode(SaveMode p_save_mode) {
	ERR_FAIL_INDEX(int(p_save_mode), int(SAVE_MODE_ALL) + 1);
	save_mode = p_save_mode;
}

InputMapSettingsServer::SaveMode InputMapSettingsServer::get_save_mode() const {
	return save_mode;
}

Ref<Resource> InputMapSettingsServer::_create_default_settings() const {
	Ref<InputActions> default_actions;
	default_actions.instantiate();
	return default_actions;
}

bool InputMapSettingsServer::_is_managed_event(const Ref<InputEvent> &p_event) {
	return p_event.is_valid() && InputAction::get_event_device(p_event) != InputAction::INPUT_DEVICE_MAX && !InputAction::event_to_text(p_event).is_empty();
}

bool InputMapSettingsServer::_is_managed_action(const StringName &p_action) const {
	if (save_mode == SAVE_MODE_ALL) {
		return true;
	}
	const InputMap *input_map = InputMap::get_singleton();
	return input_map == nullptr || !input_map->is_builtin_input_action(p_action);
}

Ref<InputAction> InputMapSettingsServer::_make_action(const StringName &p_action) {
	InputMap *input_map = InputMap::get_singleton();
	Ref<InputAction> action;
	action.instantiate(p_action, input_map->action_get_deadzone(p_action));
	const List<Ref<InputEvent>> *events = input_map->action_get_events(p_action);
	if (events != nullptr) {
		TypedArray<InputEvent> managed;
		for (const Ref<InputEvent> &event : *events) {
			if (_is_managed_event(event)) {
				managed.push_back(event);
			}
		}
		// In the text form, like the events read back from the file: an unchanged action equals its saved copy.
		action->set_events(_canonical_events(managed));
	}
	return action;
}

void InputMapSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	InputMap *input_map = InputMap::get_singleton();
	Ref<InputActions> target = p_settings;
	if (input_map == nullptr || target.is_null()) {
		return;
	}
	TypedArray<InputAction> list;
	const TypedArray<StringName> names = input_map->get_actions();
	for (int i = 0; i < names.size(); i++) {
		const StringName action_name = names[i];
		if (_is_managed_action(action_name)) {
			list.push_back(_make_action(action_name));
		}
	}
	target->set_actions(list);
}

void InputMapSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	InputMap *input_map = InputMap::get_singleton();
	const Ref<InputActions> source = p_settings;
	if (input_map == nullptr || source.is_null() || !p_names.has(String(get_list_property()))) {
		return;
	}

	PackedStringArray missing;
	for (const Ref<InputAction> &action : source->get_action_list()) {
		const StringName action_name = action->get_action();
		if (action_name == StringName()) {
			continue;
		}
		if (!input_map->has_action(action_name)) {
			// Kept in the settings until the next load drops it.
			if (!warned_actions.has(action_name)) {
				warned_actions.insert(action_name);
				missing.push_back(action_name);
			}
			continue;
		}
		input_map->action_set_deadzone(action_name, action->get_deadzone());

		// Touch, MIDI and other events are not managed and stay.
		Vector<Ref<InputEvent>> kept;
		const List<Ref<InputEvent>> *events = input_map->action_get_events(action_name);
		if (events != nullptr) {
			for (const Ref<InputEvent> &event : *events) {
				if (!_is_managed_event(event)) {
					kept.push_back(event);
				}
			}
		}
		input_map->action_erase_events(action_name);
		for (const Ref<InputEvent> &event : kept) {
			input_map->action_add_event(action_name, event);
		}
		// Copies: InputMap normalizes the device of the events it gets.
		for (const Ref<InputEvent> &event : action->get_event_list()) {
			const Ref<InputEvent> copy = event->duplicate();
			input_map->action_add_event(action_name, copy);
		}
	}
	if (!missing.is_empty()) {
		WARN_PRINT(vformat("Input map settings: the project has no action %s; the saved bindings are skipped.", String(", ").join(missing)));
	}
}

void InputMapSettingsServer::_merge_project_actions() {
	// The saved actions replace the project actions one by one. Actions added to the project since the
	// file was saved keep their project events; actions that no longer exist (or are not stored in the
	// current save mode) are dropped.
	const Ref<InputActions> pending = get_pending_settings();
	const Ref<InputActions> default_actions = get_default_settings();
	if (pending.is_null() || default_actions.is_null()) {
		return;
	}

	PackedStringArray dropped;
	for (const Ref<InputAction> &saved : pending->get_action_list()) {
		const StringName action_name = saved->get_action();
		if (!default_actions->has_action(action_name) && !warned_actions.has(action_name)) {
			warned_actions.insert(action_name);
			dropped.push_back(action_name);
		}
	}

	TypedArray<InputAction> merged;
	for (const Ref<InputAction> &project_action : default_actions->get_action_list()) {
		const Ref<InputAction> saved = pending->get_action(project_action->get_action());
		merged.push_back(saved.is_valid() ? saved : Ref<InputAction>(copy_value(project_action)));
	}
	if (!values_equal(merged, pending->get_actions())) {
		pending->set_actions(merged);
	}
	if (!dropped.is_empty()) {
		WARN_PRINT(vformat("Input map settings: the project has no action %s; the saved bindings are skipped.", String(", ").join(dropped)));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

Ref<InputActions> InputMapSettingsServer::_get_actions() const {
	return get_pending_settings();
}

Ref<InputAction> InputMapSettingsServer::_get_action(const StringName &p_action) const {
	const Ref<InputActions> actions = _get_actions();
	return actions.is_valid() ? actions->get_action(p_action) : Ref<InputAction>();
}

Ref<InputAction> InputMapSettingsServer::_ensure_action(const StringName &p_action) {
	Ref<InputAction> action = _get_action(p_action);
	if (action.is_valid()) {
		return action;
	}
	// An action of InputMap that is stored in the current save mode but not in the settings yet
	// (the save mode was changed at runtime, or the action was added by the game).
	const InputMap *input_map = InputMap::get_singleton();
	const Ref<InputActions> actions = _get_actions();
	if (input_map == nullptr || actions.is_null() || !input_map->has_action(p_action) || !_is_managed_action(p_action)) {
		return Ref<InputAction>();
	}
	action = _make_action(p_action);
	actions->add_action(action);
	return action;
}

int InputMapSettingsServer::_find_slot(const Ref<InputAction> &p_action, InputAction::InputDevice p_device, int p_slot) {
	const Vector<Ref<InputEvent>> &events = p_action->get_event_list();
	int slot = 0;
	for (int i = 0; i < events.size(); i++) {
		if (InputAction::get_event_device(events[i]) != p_device) {
			continue;
		}
		if (slot == p_slot) {
			return i;
		}
		slot++;
	}
	return -1;
}

Ref<InputEvent> InputMapSettingsServer::_normalized_event(const Ref<InputEvent> &p_event, Error &r_error) {
	r_error = ERR_INVALID_PARAMETER;
	ERR_FAIL_COND_V(p_event.is_null(), Ref<InputEvent>());
	ERR_FAIL_COND_V_MSG(InputAction::get_event_device(p_event) == InputAction::INPUT_DEVICE_MAX, Ref<InputEvent>(), vformat("Input map settings: a %s can't be bound; use a key, mouse button, joypad button or joypad axis.", p_event->get_class()));
	const String text = InputAction::event_to_text(p_event);
	ERR_FAIL_COND_V_MSG(text.is_empty(), Ref<InputEvent>(), vformat("Input map settings: the event \"%s\" has no key, button or axis.", p_event->as_text()));
	// The stored form: one code, no pressed state, for all devices (a pad's id changes when pads are reconnected).
	r_error = OK;
	return InputAction::text_to_event(text);
}

PackedStringArray InputMapSettingsServer::get_action_names() const {
	PackedStringArray names;
	const Ref<InputActions> actions = _get_actions();
	if (actions.is_valid()) {
		for (const Ref<InputAction> &action : actions->get_action_list()) {
			names.push_back(action->get_action());
		}
	}
	return names;
}

bool InputMapSettingsServer::has_action(const StringName &p_action) const {
	return _get_action(p_action).is_valid();
}

Ref<InputAction> InputMapSettingsServer::get_input_action(const StringName &p_action) const {
	const Ref<InputAction> action = _get_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), Ref<InputAction>(), vformat("Input map settings: there is no action \"%s\".", p_action));
	const Ref<InputAction> copy = copy_value(action);
	return copy;
}

TypedArray<InputEvent> InputMapSettingsServer::get_events(const StringName &p_action) const {
	const Ref<InputAction> action = _get_action(p_action);
	return action.is_valid() ? action->get_events() : TypedArray<InputEvent>();
}

void InputMapSettingsServer::set_action_deadzone(const StringName &p_action, float p_deadzone) {
	const Ref<InputAction> action = _ensure_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), vformat("Input map settings: there is no action \"%s\".", p_action));
	const float deadzone = CLAMP(p_deadzone, 0.0f, 1.0f);
	if (action->get_deadzone() == deadzone) {
		return;
	}
	action->set_deadzone(deadzone);
	notify_value_changed(get_list_property());
}

float InputMapSettingsServer::get_action_deadzone(const StringName &p_action) const {
	const Ref<InputAction> action = _get_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), InputAction::DEFAULT_DEADZONE, vformat("Input map settings: there is no action \"%s\".", p_action));
	return action->get_deadzone();
}

Error InputMapSettingsServer::add_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	const Ref<InputAction> action = _ensure_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), ERR_DOES_NOT_EXIST, vformat("Input map settings: there is no action \"%s\".", p_action));
	Error error = OK;
	const Ref<InputEvent> event = _normalized_event(p_event, error);
	if (error != OK) {
		return error;
	}
	// A pressed key has all codes; a stored binding matches it when one of them is the same.
	if (action->has_event(p_event) || action->has_event(event)) {
		return OK;
	}
	action->add_event(event);
	notify_value_changed(get_list_property());
	return OK;
}

Error InputMapSettingsServer::remove_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	const Ref<InputAction> action = _ensure_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), ERR_DOES_NOT_EXIST, vformat("Input map settings: there is no action \"%s\".", p_action));
	Error error = OK;
	const Ref<InputEvent> event = _normalized_event(p_event, error);
	if (error != OK) {
		return error;
	}
	// The stored binding that matches the pressed event (`Key:W` matches a pressed W), else the same text form.
	if (!action->remove_event(p_event) && !action->remove_event(event)) {
		return ERR_DOES_NOT_EXIST;
	}
	notify_value_changed(get_list_property());
	return OK;
}

Ref<InputEvent> InputMapSettingsServer::get_event(const StringName &p_action, InputAction::InputDevice p_device, int p_slot) const {
	const Ref<InputAction> action = _get_action(p_action);
	if (action.is_null()) {
		return Ref<InputEvent>();
	}
	const int index = _find_slot(action, p_device, p_slot);
	return index >= 0 ? action->get_event_list()[index] : Ref<InputEvent>();
}

int InputMapSettingsServer::get_slot_count(const StringName &p_action, InputAction::InputDevice p_device) const {
	const Ref<InputAction> action = _get_action(p_action);
	if (action.is_null()) {
		return 0;
	}
	int count = 0;
	for (const Ref<InputEvent> &event : action->get_event_list()) {
		if (InputAction::get_event_device(event) == p_device) {
			count++;
		}
	}
	return count;
}

Error InputMapSettingsServer::rebind(const StringName &p_action, const Ref<InputEvent> &p_event, int p_slot) {
	ERR_FAIL_COND_V_MSG(p_slot < 0, ERR_INVALID_PARAMETER, "Input map settings: the slot must be 0 or greater.");
	const Ref<InputAction> action = _ensure_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), ERR_DOES_NOT_EXIST, vformat("Input map settings: there is no action \"%s\".", p_action));
	Error error = OK;
	const Ref<InputEvent> event = _normalized_event(p_event, error);
	if (error != OK) {
		return error;
	}

	TypedArray<InputEvent> events = action->get_events();
	const int index = _find_slot(action, InputAction::get_event_device(event), p_slot);
	if (index >= 0) {
		const Ref<InputEvent> old_event = events[index];
		if (InputAction::event_to_text(old_event) == InputAction::event_to_text(event) && old_event->get_device() == -1) {
			return OK;
		}
		events[index] = event;
	} else {
		// A slot after the last one of the device: the event is added at the end.
		events.push_back(event);
	}
	action->set_events(events);
	notify_value_changed(get_list_property());
	return OK;
}

void InputMapSettingsServer::clear_event(const StringName &p_action, InputAction::InputDevice p_device, int p_slot) {
	const Ref<InputAction> action = _ensure_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), vformat("Input map settings: there is no action \"%s\".", p_action));
	const int index = _find_slot(action, p_device, p_slot);
	if (index < 0) {
		return;
	}
	// The action keeps its other events; it stays in the settings, so the project binding does not come back.
	TypedArray<InputEvent> events = action->get_events();
	events.remove_at(index);
	action->set_events(events);
	notify_value_changed(get_list_property());
}

PackedStringArray InputMapSettingsServer::find_conflicts(const Ref<InputEvent> &p_event, const StringName &p_ignore_action) const {
	PackedStringArray result;
	ERR_FAIL_COND_V(p_event.is_null(), result);
	const String text = InputAction::event_to_text(p_event);
	const Ref<InputActions> actions = _get_actions();
	if (text.is_empty() || actions.is_null()) {
		return result;
	}
	const int device_id = p_event->get_device();
	for (const Ref<InputAction> &action : actions->get_action_list()) {
		if (action->get_action() == p_ignore_action) {
			continue;
		}
		for (const Ref<InputEvent> &bound : action->get_event_list()) {
			if (bound->get_device() != -1 && device_id != -1 && bound->get_device() != device_id) {
				continue;
			}
			// A pressed key has all codes, a stored one only one of them (`Key:W` matches a pressed W).
			if (InputAction::event_to_text(bound) == text || bound->is_match(p_event, true)) {
				result.push_back(action->get_action());
				break;
			}
		}
	}
	return result;
}

void InputMapSettingsServer::reset_action(const StringName &p_action) {
	const Ref<InputAction> action = _get_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), vformat("Input map settings: there is no action \"%s\".", p_action));
	const Ref<InputActions> default_actions = get_default_settings();
	const Ref<InputAction> project_action = default_actions.is_valid() ? default_actions->get_action(p_action) : Ref<InputAction>();
	ERR_FAIL_COND_MSG(project_action.is_null(), vformat("Input map settings: the project has no action \"%s\".", p_action));
	if (values_equal(action, project_action)) {
		return;
	}
	// Replaces the entry in place, so the order of the list stays.
	const Ref<InputAction> copy = copy_value(project_action);
	_get_actions()->add_action(copy);
	notify_value_changed(get_list_property());
}

void InputMapSettingsServer::reset_all() {
	reset_to_defaults();
}

String InputMapSettingsServer::get_event_label(const Ref<InputEvent> &p_event) const {
	return _event_to_label(p_event);
}

void InputMapSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_save_mode", "save_mode"), &InputMapSettingsServer::set_save_mode);
	ClassDB::bind_method(D_METHOD("get_save_mode"), &InputMapSettingsServer::get_save_mode);

	ClassDB::bind_method(D_METHOD("get_action_names"), &InputMapSettingsServer::get_action_names);
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputMapSettingsServer::has_action);
	ClassDB::bind_method(D_METHOD("get_input_action", "action"), &InputMapSettingsServer::get_input_action);
	ClassDB::bind_method(D_METHOD("get_events", "action"), &InputMapSettingsServer::get_events);
	ClassDB::bind_method(D_METHOD("set_action_deadzone", "action", "deadzone"), &InputMapSettingsServer::set_action_deadzone);
	ClassDB::bind_method(D_METHOD("get_action_deadzone", "action"), &InputMapSettingsServer::get_action_deadzone);
	ClassDB::bind_method(D_METHOD("add_event", "action", "event"), &InputMapSettingsServer::add_event);
	ClassDB::bind_method(D_METHOD("remove_event", "action", "event"), &InputMapSettingsServer::remove_event);
	ClassDB::bind_method(D_METHOD("get_event", "action", "device", "slot"), &InputMapSettingsServer::get_event, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_slot_count", "action", "device"), &InputMapSettingsServer::get_slot_count);
	ClassDB::bind_method(D_METHOD("rebind", "action", "event", "slot"), &InputMapSettingsServer::rebind, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("clear_event", "action", "device", "slot"), &InputMapSettingsServer::clear_event, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("find_conflicts", "event", "ignore_action"), &InputMapSettingsServer::find_conflicts, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("reset_action", "action"), &InputMapSettingsServer::reset_action);
	ClassDB::bind_method(D_METHOD("reset_all"), &InputMapSettingsServer::reset_all);
	ClassDB::bind_method(D_METHOD("get_event_label", "event"), &InputMapSettingsServer::get_event_label);
	ClassDB::bind_static_method("InputMapSettingsServer", D_METHOD("events_to_text", "events"), &InputMapSettingsServer::events_to_text);
	ClassDB::bind_static_method("InputMapSettingsServer", D_METHOD("text_to_events", "text"), &InputMapSettingsServer::text_to_events);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "save_mode", PROPERTY_HINT_ENUM, "Custom Only,All"), "set_save_mode", "get_save_mode");

	BIND_ENUM_CONSTANT(SAVE_MODE_CUSTOM_ONLY);
	BIND_ENUM_CONSTANT(SAVE_MODE_ALL);
}

InputMapSettingsServer::InputMapSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings != nullptr && project_settings->has_setting(SETTING_SAVE_MODE)) {
		const int mode = project_settings->get_setting_with_override(SETTING_SAVE_MODE);
		save_mode = mode == SAVE_MODE_ALL ? SAVE_MODE_ALL : SAVE_MODE_CUSTOM_ONLY;
	}
	connect(SNAME("loaded"), callable_mp(this, &InputMapSettingsServer::_merge_project_actions));
}

InputMapSettingsServer::~InputMapSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
