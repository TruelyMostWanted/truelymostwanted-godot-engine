/**************************************************************************/
/*  input_map.cpp                                                         */
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

#include "input_map.h"
#include "input_map.compat.inc"

#include "core/config/project_settings.h"
#include "core/core_string_names.h"
#include "core/input/builtin_input_actions.h"
#include "core/input/input.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/variant/typed_array.h"

void InputMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputMap::has_action);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMap::get_actions);
	ClassDB::bind_method(D_METHOD("add_action", "action", "deadzone"), &InputMap::add_action, DEFVAL(DEFAULT_DEADZONE));
	ClassDB::bind_method(D_METHOD("erase_action", "action"), &InputMap::erase_action);

	ClassDB::bind_method(D_METHOD("get_action_description", "action"), &InputMap::get_action_description);

	ClassDB::bind_method(D_METHOD("action_set_deadzone", "action", "deadzone"), &InputMap::action_set_deadzone);
	ClassDB::bind_method(D_METHOD("action_get_deadzone", "action"), &InputMap::action_get_deadzone);
	ClassDB::bind_method(D_METHOD("action_add_event", "action", "event"), &InputMap::action_add_event);
	ClassDB::bind_method(D_METHOD("action_has_event", "action", "event"), &InputMap::action_has_event);
	ClassDB::bind_method(D_METHOD("action_erase_event", "action", "event"), &InputMap::action_erase_event);
	ClassDB::bind_method(D_METHOD("action_erase_events", "action"), &InputMap::action_erase_events);
	ClassDB::bind_method(D_METHOD("action_get_events", "action"), &InputMap::_action_get_events);
	ClassDB::bind_method(D_METHOD("event_is_action", "event", "action", "exact_match"), &InputMap::event_is_action, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_from_project_settings"), &InputMap::load_from_project_settings);

	ClassDB::bind_method(D_METHOD("is_builtin_input_action", "action"), &InputMap::is_builtin_input_action);
	ClassDB::bind_method(D_METHOD("is_custom_input_action", "action"), &InputMap::is_custom_input_action);
	ClassDB::bind_method(D_METHOD("get_input_action_layer", "action"), &InputMap::get_input_action_layer);
	ClassDB::bind_method(D_METHOD("get_input_action", "action"), &InputMap::get_input_action);
	ClassDB::bind_method(D_METHOD("get_input_actions", "layer"), &InputMap::get_input_actions);
	ClassDB::bind_method(D_METHOD("set_input_action", "action"), &InputMap::set_input_action);
	ClassDB::bind_method(D_METHOD("set_input_actions", "actions"), &InputMap::set_input_actions);

	ADD_SIGNAL(MethodInfo("project_settings_loaded"));

	BIND_ENUM_CONSTANT(INPUT_ACTION_LAYER_BUILTIN);
	BIND_ENUM_CONSTANT(INPUT_ACTION_LAYER_CUSTOM);
}

/**
 * Returns an nonexistent action error message with a suggestion of the closest
 * matching action name (if possible).
 */
String InputMap::suggest_actions(const StringName &p_action) const {
	StringName closest_action;
	float closest_similarity = 0.0;

	// Find the most action with the most similar name.
	_ensure_input_map();
	for (const KeyValue<StringName, Action> &kv : input_map) {
		const float similarity = String(kv.key).similarity(p_action);

		if (similarity > closest_similarity) {
			closest_action = kv.key;
			closest_similarity = similarity;
		}
	}

	String error_message = vformat("The InputMap action \"%s\" doesn't exist.", p_action);

	if (closest_similarity >= 0.4) {
		// Only include a suggestion in the error message if it's similar enough.
		error_message += vformat(" Did you mean \"%s\"?", closest_action);
	}
	return error_message;
}

#ifdef TOOLS_ENABLED
void InputMap::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	bool first_argument_is_action = false;
	if (p_idx == 0) {
		first_argument_is_action = (pf == "has_action" || pf == "erase_action" ||
				pf == "action_set_deadzone" || pf == "action_get_deadzone" ||
				pf == "action_has_event" || pf == "action_add_event" || pf == "action_get_events" ||
				pf == "action_erase_event" || pf == "action_erase_events");
	}
	if (first_argument_is_action || (p_idx == 1 && pf == "event_is_action")) {
		// Cannot rely on `get_actions()`, otherwise the actions would be in the context of the Editor (no user-defined actions).
		List<PropertyInfo> pinfo;
		ProjectSettings::get_singleton()->get_property_list(&pinfo);

		for (const PropertyInfo &pi : pinfo) {
			if (!pi.name.begins_with("input/")) {
				continue;
			}

			String name = pi.name.substr(pi.name.find_char('/') + 1);
			r_options->push_back(name.quote());
		}
	}

	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void InputMap::add_action(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(has_action(p_action), vformat("InputMap already has action \"%s\".", String(p_action)));
	// Godot's built-in names go to the built-in layer, all others to the custom layer.
	const Ref<InputAction> action = memnew(InputAction(p_action, p_deadzone));
	_get_layer(builtin_action_names.has(p_action) ? INPUT_ACTION_LAYER_BUILTIN : INPUT_ACTION_LAYER_CUSTOM)->add_action(action);
}

void InputMap::erase_action(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!has_action(p_action), suggest_actions(p_action));

	builtin_actions->remove_action(p_action);
	custom_actions->remove_action(p_action);
}

TypedArray<StringName> InputMap::get_actions() {
	_ensure_input_map();
	TypedArray<StringName> ret;

	ret.resize(input_map.size());

	uint32_t i = 0;
	for (const KeyValue<StringName, Action> &kv : input_map) {
		ret[i] = kv.key;
		i++;
	}

	return ret;
}

List<Ref<InputEvent>>::Element *InputMap::_find_event(Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match, bool *r_pressed, float *r_strength, float *r_raw_strength, int *r_event_index) const {
	ERR_FAIL_COND_V(p_event.is_null(), nullptr);

	int i = 0;
	for (List<Ref<InputEvent>>::Element *E = p_action.inputs.front(); E; E = E->next()) {
		int device = E->get()->get_device();
		if (device == ALL_DEVICES || device == p_event->get_device()) {
			if (E->get()->action_match(p_event, p_exact_match, p_action.deadzone, r_pressed, r_strength, r_raw_strength)) {
				if (r_event_index) {
					*r_event_index = i;
				}
				return E;
			}
		}
		i++;
	}

	return nullptr;
}

bool InputMap::has_action(const StringName &p_action) const {
	return builtin_actions->has_action(p_action) || custom_actions->has_action(p_action);
}

String InputMap::get_action_description(const StringName &p_action) const {
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), String(), suggest_actions(p_action));

	String ret;
	for (const Ref<InputEvent> &event : action->get_event_list()) {
		const Ref<InputEventKey> iek = event;
		if (iek.is_valid()) {
			if (!ret.is_empty()) {
				ret += RTR(" or ");
			}
			ret += iek->as_text();
		}
	}
	if (ret.is_empty()) {
		ret = RTR("Action has no bound inputs");
	}
	return ret;
}

float InputMap::action_get_deadzone(const StringName &p_action) {
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), 0.0f, suggest_actions(p_action));

	return action->get_deadzone();
}

void InputMap::action_set_deadzone(const StringName &p_action, float p_deadzone) {
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), suggest_actions(p_action));

	action->set_deadzone(p_deadzone);
}

void InputMap::action_add_event(const StringName &p_action, RequiredParam<InputEvent> p_event) {
	EXTRACT_PARAM_OR_FAIL_MSG(event, p_event, "It's not a reference to a valid InputEvent object.");
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), suggest_actions(p_action));

	_normalize_event_device(event);
	if (_find_layer_event(action, event) >= 0) {
		return; // Already added.
	}
	action->push_event(event);
}

bool InputMap::action_has_event(const StringName &p_action, RequiredParam<InputEvent> p_event) {
	EXTRACT_PARAM_OR_FAIL_V(event, p_event, false);
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), false, suggest_actions(p_action));
	return _find_layer_event(action, event) >= 0;
}

void InputMap::action_erase_event(const StringName &p_action, RequiredParam<InputEvent> p_event) {
	EXTRACT_PARAM_OR_FAIL(event, p_event);
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), suggest_actions(p_action));

	const int index = _find_layer_event(action, event);
	if (index >= 0) {
		action->remove_event_at(index);

		if (Input::get_singleton()->is_action_pressed(p_action)) {
			Input::get_singleton()->action_release(p_action);
		}
	}
}

void InputMap::action_erase_events(const StringName &p_action) {
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_MSG(action.is_null(), suggest_actions(p_action));

	if (Input::get_singleton()->is_action_pressed(p_action)) {
		Input::get_singleton()->action_release(p_action);
	}

	action->clear_events();
}

TypedArray<InputEvent> InputMap::_action_get_events(const StringName &p_action) {
	const Ref<InputAction> action = _get_layer_action(p_action);
	return action.is_valid() ? action->get_events() : TypedArray<InputEvent>();
}

const List<Ref<InputEvent>> *InputMap::action_get_events(const StringName &p_action) {
	_ensure_input_map();
	HashMap<StringName, Action>::Iterator E = input_map.find(p_action);
	if (!E) {
		return nullptr;
	}

	return &E->value.inputs;
}

bool InputMap::event_is_action(RequiredParam<InputEvent> p_event, const StringName &p_action, bool p_exact_match) const {
	EXTRACT_PARAM_OR_FAIL_V(event, p_event, false);
	return event_get_action_status(event, p_action, p_exact_match);
}

int InputMap::event_get_index(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match) const {
	int index = -1;
	bool valid = event_get_action_status(p_event, p_action, p_exact_match, nullptr, nullptr, nullptr, &index);
	return valid ? index : -1;
}

bool InputMap::event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match, bool *r_pressed, float *r_strength, float *r_raw_strength, int *r_event_index) const {
	_ensure_input_map();
	HashMap<StringName, Action>::Iterator E = input_map.find(p_action);
	ERR_FAIL_COND_V_MSG(!E, false, suggest_actions(p_action));

	Ref<InputEventAction> input_event_action = p_event;
	if (input_event_action.is_valid()) {
		const bool pressed = input_event_action->is_pressed();
		if (r_pressed != nullptr) {
			*r_pressed = pressed;
		}
		const float strength = pressed ? input_event_action->get_strength() : 0.0f;
		if (r_strength != nullptr) {
			*r_strength = strength;
		}
		if (r_raw_strength != nullptr) {
			*r_raw_strength = strength;
		}
		if (r_event_index) {
			if (input_event_action->get_event_index() >= 0) {
				*r_event_index = input_event_action->get_event_index();
			} else {
				*r_event_index = E->value.inputs.size();
			}
		}
		return input_event_action->get_action() == p_action;
	}

	List<Ref<InputEvent>>::Element *event = _find_event(E->value, p_event, p_exact_match, r_pressed, r_strength, r_raw_strength, r_event_index);
	return event != nullptr;
}

const HashMap<StringName, InputMap::Action> &InputMap::get_action_map() const {
	_ensure_input_map();
	return input_map;
}

void InputMap::_add_project_action(const String &p_name, const Dictionary &p_action) {
	if (!p_action.has("events")) {
		return;
	}

	float deadzone = p_action.has("deadzone") ? (float)p_action["deadzone"] : DEFAULT_DEADZONE;
	Array events = p_action["events"];

	add_action(p_name, deadzone);
	for (int i = 0; i < events.size(); i++) {
		Ref<InputEvent> event = events[i];
		if (event.is_null()) {
			continue;
		}
		action_add_event(p_name, event);
	}
}

void InputMap::load_from_project_settings() {
	// The layer resources stay the same objects; only their actions are replaced.
	builtin_actions->clear();
	custom_actions->clear();

	List<PropertyInfo> pinfo;
	ProjectSettings::get_singleton()->get_property_list(&pinfo);

	for (const PropertyInfo &pi : pinfo) {
		if (!pi.name.begins_with("input/")) {
			continue;
		}

		String name = pi.name.substr(pi.name.find_char('/') + 1);
		_add_project_action(name, GLOBAL_GET(pi.name));
	}

	emit_signal("project_settings_loaded");
}

struct _BuiltinActionDisplayName {
	const char *name;
	const char *display_name;
};

static const _BuiltinActionDisplayName _builtin_action_display_names[] = {
	/* clang-format off */
	{ "ui_accept",                                     TTRC("Accept") },
	{ "ui_select",                                     TTRC("Select") },
	{ "ui_cancel",                                     TTRC("Cancel") },
	{ "ui_close_dialog",                               TTRC("Close Dialog") },
	{ "ui_focus_next",                                 TTRC("Focus Next") },
	{ "ui_focus_prev",                                 TTRC("Focus Prev") },
	{ "ui_left",                                       TTRC("Left") },
	{ "ui_right",                                      TTRC("Right") },
	{ "ui_up",                                         TTRC("Up") },
	{ "ui_down",                                       TTRC("Down") },
	{ "ui_page_up",                                    TTRC("Page Up") },
	{ "ui_page_down",                                  TTRC("Page Down") },
	{ "ui_home",                                       TTRC("Home") },
	{ "ui_end",                                        TTRC("End") },
	{ "ui_cut",                                        TTRC("Cut") },
	{ "ui_copy",                                       TTRC("Copy") },
	{ "ui_paste",                                      TTRC("Paste") },
	{ "ui_focus_mode",                                 TTRC("Toggle Tab Focus Mode") },
	{ "ui_undo",                                       TTRC("Undo") },
	{ "ui_redo",                                       TTRC("Redo") },
	{ "ui_text_completion_query",                      TTRC("Completion Query") },
	{ "ui_text_newline",                               TTRC("New Line") },
	{ "ui_text_newline_blank",                         TTRC("New Blank Line") },
	{ "ui_text_newline_above",                         TTRC("New Line Above") },
	{ "ui_text_indent",                                TTRC("Indent") },
	{ "ui_text_dedent",                                TTRC("Dedent") },
	{ "ui_text_backspace",                             TTRC("Backspace") },
	{ "ui_text_backspace_word",                        TTRC("Backspace Word") },
	{ "ui_text_backspace_all_to_left",                 TTRC("Backspace all to Left") },
	{ "ui_text_delete",                                TTRC("Delete") },
	{ "ui_text_delete_word",                           TTRC("Delete Word") },
	{ "ui_text_delete_all_to_right",                   TTRC("Delete all to Right") },
	{ "ui_text_caret_left",                            TTRC("Caret Left") },
	{ "ui_text_caret_word_left",                       TTRC("Caret Word Left") },
	{ "ui_text_caret_right",                           TTRC("Caret Right") },
	{ "ui_text_caret_word_right",                      TTRC("Caret Word Right") },
	{ "ui_text_caret_up",                              TTRC("Caret Up") },
	{ "ui_text_caret_down",                            TTRC("Caret Down") },
	{ "ui_text_caret_line_start",                      TTRC("Caret Line Start") },
	{ "ui_text_caret_line_end",                        TTRC("Caret Line End") },
	{ "ui_text_caret_page_up",                         TTRC("Caret Page Up") },
	{ "ui_text_caret_page_down",                       TTRC("Caret Page Down") },
	{ "ui_text_caret_document_start",                  TTRC("Caret Document Start") },
	{ "ui_text_caret_document_end",                    TTRC("Caret Document End") },
	{ "ui_text_caret_add_below",                       TTRC("Caret Add Below") },
	{ "ui_text_caret_add_above",                       TTRC("Caret Add Above") },
	{ "ui_text_scroll_up",                             TTRC("Scroll Up") },
	{ "ui_text_scroll_down",                           TTRC("Scroll Down") },
	{ "ui_text_select_all",                            TTRC("Select All") },
	{ "ui_text_select_word_under_caret",               TTRC("Select Word Under Caret") },
	{ "ui_text_add_selection_for_next_occurrence",     TTRC("Add Selection for Next Occurrence") },
	{ "ui_text_skip_selection_for_next_occurrence",    TTRC("Skip Selection for Next Occurrence") },
	{ "ui_text_clear_carets_and_selection",            TTRC("Clear Carets and Selection") },
	{ "ui_text_toggle_insert_mode",                    TTRC("Toggle Insert Mode") },
	{ "ui_text_submit",                                TTRC("Submit Text") },
	{ "ui_graph_duplicate",                            TTRC("Duplicate Nodes") },
	{ "ui_graph_delete",                               TTRC("Delete Nodes") },
	{ "ui_graph_follow_left",                          TTRC("Follow Input Port Connection") },
	{ "ui_graph_follow_right",                         TTRC("Follow Output Port Connection") },
	{ "ui_filedialog_delete",                          TTRC("Delete") },
	{ "ui_filedialog_up_one_level",                    TTRC("Go Up One Level") },
	{ "ui_filedialog_refresh",                         TTRC("Refresh") },
	{ "ui_filedialog_show_hidden",                     TTRC("Show Hidden") },
	{ "ui_filedialog_find",                            TTRC("Find") },
	{ "ui_filedialog_focus_path",                      TTRC("Focus Path") },
	{ "ui_swap_input_direction",                       TTRC("Swap Input Direction") },
	{ "ui_unicode_start",                              TTRC("Start Unicode Character Input") },
	{ "ui_colorpicker_delete_preset",                  TTRC("ColorPicker: Delete Preset") },
	{ "ui_accessibility_drag_and_drop",                TTRC("Accessibility: Keyboard Drag and Drop") },
	{ "ui_toggle_fullscreen",                          TTRC("Toggle Fullscreen") },
	{ "",                                              ""}
	/* clang-format on */
};

String InputMap::get_builtin_display_name(const String &p_name) const {
	const String name = p_name.get_slicec('.', 0);
	constexpr int len = std_size(_builtin_action_display_names);
	for (int i = 0; i < len; i++) {
		if (_builtin_action_display_names[i].name == name) {
			return _builtin_action_display_names[i].display_name;
		}
	}

	return p_name;
}

const HashMap<String, List<Ref<InputEvent>>> &InputMap::get_builtins() {
	// Return cache if it has already been built.
	if (default_builtin_cache.size()) {
		return default_builtin_cache;
	}

	// The actions come from `core/input/builtin_input_actions.tres` (#172), compiled into a table at build time.
	for (int i = 0; i < BuiltinInputActions::action_count; i++) {
		const BuiltinInputActions::Action &action = BuiltinInputActions::actions[i];
		List<Ref<InputEvent>> inputs;
		for (int j = 0; j < action.event_count; j++) {
			const BuiltinInputActions::Event &event = action.events[j];
			switch (event.type) {
				case BuiltinInputActions::EVENT_KEY:
					inputs.push_back(InputEventKey::create_reference(event.key));
					break;
				case BuiltinInputActions::EVENT_JOYPAD_BUTTON:
					inputs.push_back(InputEventJoypadButton::create_reference(event.button));
					break;
				case BuiltinInputActions::EVENT_JOYPAD_MOTION:
					inputs.push_back(InputEventJoypadMotion::create_reference(event.axis, event.axis_value));
					break;
			}
		}
		default_builtin_cache.insert(action.name, inputs);
	}

	return default_builtin_cache;
}

const HashMap<String, List<Ref<InputEvent>>> &InputMap::get_builtins_with_feature_overrides_applied() {
	if (default_builtin_with_overrides_cache.size() > 0) {
		return default_builtin_with_overrides_cache;
	}

	const HashMap<String, List<Ref<InputEvent>>> &builtins = get_builtins();

	// Get a list of all built in inputs which are valid overrides for the OS
	// Key = builtin name (e.g. ui_accept)
	// Value = override/feature names (e.g. macos, if it was defined as "ui_accept.macos" and the platform supports that feature)
	HashMap<String, Vector<String>> builtins_with_overrides;
	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String fullname = E.key;

		const String &name = fullname.get_slicec('.', 0);
		String override_for = fullname.get_slice_count(".") > 1 ? fullname.get_slicec('.', 1) : String();

		if (!override_for.is_empty() && OS::get_singleton()->has_feature(override_for)) {
			builtins_with_overrides[name].push_back(override_for);
		}
	}

	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String fullname = E.key;

		const String &name = fullname.get_slicec('.', 0);
		String override_for = fullname.get_slice_count(".") > 1 ? fullname.get_slicec('.', 1) : String();

		if (builtins_with_overrides.has(name) && override_for.is_empty()) {
			// Builtin has an override but this particular one is not an override, so skip.
			continue;
		}

		if (!override_for.is_empty() && !OS::get_singleton()->has_feature(override_for)) {
			// OS does not support this override - skip.
			continue;
		}

		default_builtin_with_overrides_cache.insert(name, E.value);
	}

	return default_builtin_with_overrides_cache;
}

void InputMap::load_default() {
	HashMap<String, List<Ref<InputEvent>>> builtins(get_builtins_with_feature_overrides_applied());

	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		String name = E.key;

		add_action(name);

		const List<Ref<InputEvent>> &inputs = E.value;
		for (const List<Ref<InputEvent>>::Element *I = inputs.front(); I; I = I->next()) {
			Ref<InputEventKey> iek = I->get();

			// For the editor, only add keyboard actions.
			if (iek.is_valid()) {
				action_add_event(name, I->get());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Layers (#172).

InputActions *InputMap::_get_layer(InputActionLayer p_layer) const {
	return p_layer == INPUT_ACTION_LAYER_BUILTIN ? builtin_actions.ptr() : custom_actions.ptr();
}

Ref<InputAction> InputMap::_get_layer_action(const StringName &p_action) const {
	// A custom action replaces a built-in action of the same name, as in the action map.
	const Ref<InputAction> action = custom_actions->get_action(p_action);
	return action.is_valid() ? action : builtin_actions->get_action(p_action);
}

int InputMap::_find_layer_event(const Ref<InputAction> &p_action, const Ref<InputEvent> &p_event) {
	// The exact match of _find_event(), on the events of the layer. A keyboard or mouse event with the
	// legacy device 0 counts as the event with the normalized device.
	const int event_device = _get_normalized_device(p_event);
	const Vector<Ref<InputEvent>> &events = p_action->get_event_list();
	for (int i = 0; i < events.size(); i++) {
		const int device = events[i]->get_device();
		if ((device == ALL_DEVICES || device == event_device) && events[i]->action_match(p_event, true, p_action->get_deadzone(), nullptr, nullptr, nullptr)) {
			return i;
		}
	}
	return -1;
}

bool InputMap::_needs_device_normalization(const Ref<InputEvent> &p_event) {
	// Before the device ID change, keyboard and mouse events defaulted to device 0.
	if (p_event.is_null() || p_event->get_device() != 0) {
		return false;
	}
	switch (p_event->get_type()) {
		case InputEventType::KEY:
		case InputEventType::MOUSE_BUTTON:
		case InputEventType::MOUSE_MOTION:
			return true;
		default:
			return false;
	}
}

int InputMap::_get_normalized_device(const Ref<InputEvent> &p_event) {
	if (!_needs_device_normalization(p_event)) {
		return p_event->get_device();
	}
	return p_event->get_type() == InputEventType::KEY ? InputEvent::DEVICE_ID_KEYBOARD : InputEvent::DEVICE_ID_MOUSE;
}

void InputMap::_normalize_event_device(const Ref<InputEvent> &p_event) {
	if (_needs_device_normalization(p_event)) {
		p_event->set_device(_get_normalized_device(p_event));
	}
}

bool InputMap::is_builtin_input_action(const StringName &p_action) const {
	return builtin_actions->has_action(p_action);
}

bool InputMap::is_custom_input_action(const StringName &p_action) const {
	return custom_actions->has_action(p_action);
}

bool InputMap::is_builtin_action_name(const StringName &p_action) const {
	return builtin_action_names.has(p_action);
}

InputMap::InputActionLayer InputMap::get_input_action_layer(const StringName &p_action) const {
	if (custom_actions->has_action(p_action)) {
		return INPUT_ACTION_LAYER_CUSTOM;
	}
	if (builtin_actions->has_action(p_action) || builtin_action_names.has(p_action)) {
		return INPUT_ACTION_LAYER_BUILTIN;
	}
	return INPUT_ACTION_LAYER_CUSTOM;
}

Ref<InputAction> InputMap::get_input_action(const StringName &p_action) const {
	const Ref<InputAction> action = _get_layer_action(p_action);
	ERR_FAIL_COND_V_MSG(action.is_null(), Ref<InputAction>(), suggest_actions(p_action));
	return action;
}

Ref<InputActions> InputMap::get_input_actions(InputActionLayer p_layer) const {
	ERR_FAIL_INDEX_V(int(p_layer), 2, Ref<InputActions>());
	return Ref<InputActions>(_get_layer(p_layer));
}

void InputMap::set_input_action(const Ref<InputAction> &p_action) {
	ERR_FAIL_COND_MSG(p_action.is_null(), "InputMap: the action is null.");
	const StringName action = p_action->get_action();
	ERR_FAIL_COND_MSG(action == StringName(), "InputMap: the action has no name.");

	const Ref<InputAction> old_action = _get_layer_action(action);
	if (old_action == p_action) {
		return;
	}
	if (old_action.is_valid() && Input::get_singleton() != nullptr && Input::get_singleton()->is_action_pressed(action)) {
		Input::get_singleton()->action_release(action);
	}
	for (const Ref<InputEvent> &event : p_action->get_event_list()) {
		_normalize_event_device(event);
	}
	// The layer keeps the resource itself; an action of that name is replaced in its place.
	_get_layer(get_input_action_layer(action))->add_action(p_action);
}

void InputMap::set_input_actions(const Ref<InputActions> &p_actions) {
	ERR_FAIL_COND_MSG(p_actions.is_null(), "InputMap: the actions are null.");
	const LocalVector<Ref<InputAction>> list(p_actions->get_action_list());
	for (const Ref<InputAction> &action : list) {
		set_input_action(action);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Action map, derived from the layers (#172).

void InputMap::_mark_input_map_dirty() {
	input_map_dirty.set();
}

void InputMap::_ensure_input_map() const {
	if (!input_map_dirty.is_set()) {
		return;
	}
	LocalVector<Ref<InputEvent>> normalize;
	{
		MutexLock lock(input_map_mutex);
		if (input_map_dirty.is_set()) {
			_rebuild_input_map(normalize);
		}
	}
	// After releasing the lock: set_device() emits `changed`, which runs other code.
	for (const Ref<InputEvent> &event : normalize) {
		_normalize_event_device(event);
	}
}

void InputMap::_rebuild_input_map(LocalVector<Ref<InputEvent>> &r_normalize) const {
	// Called with input_map_mutex held.
	input_map_dirty.clear();

	// An event changed in place (for example `event.keycode = KEY_B`) may change its key in the reverse index.
	const Callable on_event_changed = callable_mp(const_cast<InputMap *>(this), &InputMap::_mark_event_index_dirty);
	for (const Ref<InputEvent> &event : watched_events) {
		if (event->is_connected(CoreStringName(changed), on_event_changed)) {
			event->disconnect(CoreStringName(changed), on_event_changed);
		}
	}
	watched_events.clear();

	// The built-in layer first, then the custom layer, each in its order. Actions keep their ID.
	HashMap<StringName, Action> new_map;
	const InputActions *layers[2] = { builtin_actions.ptr(), custom_actions.ptr() };
	for (const InputActions *layer : layers) {
		for (const Ref<InputAction> &layer_action : layer->get_action_list()) {
			const StringName action_name = layer_action->get_action();
			if (action_name == StringName()) {
				continue;
			}
			Action *action = new_map.getptr(action_name);
			if (action == nullptr) {
				const Action *old_action = input_map.getptr(action_name);
				Action created;
				created.id = old_action != nullptr ? old_action->id : next_action_id++;
				created.deadzone = DEFAULT_DEADZONE;
				action = &new_map.insert(action_name, created)->value;
			}
			action->deadzone = layer_action->get_deadzone();
			action->inputs.clear();
			for (const Ref<InputEvent> &event : layer_action->get_event_list()) {
				if (event.is_null()) {
					continue;
				}
				action->inputs.push_back(event);
				watched_events.push_back(event);
				event->connect(CoreStringName(changed), on_event_changed, CONNECT_REFERENCE_COUNTED);
				if (_needs_device_normalization(event)) {
					r_normalize.push_back(event);
				}
			}
		}
	}
	input_map = std::move(new_map);
	event_index_dirty.set();
}

/////////////////////////////////////////////////////////////////////////////
// Reverse index (#172).

namespace {

enum EventIndexKind : uint64_t {
	EVENT_INDEX_KEYCODE = 1,
	EVENT_INDEX_PHYSICAL_KEYCODE,
	EVENT_INDEX_KEY_LABEL,
	EVENT_INDEX_MOUSE_BUTTON,
	EVENT_INDEX_JOY_BUTTON,
	EVENT_INDEX_JOY_AXIS,
};

inline uint64_t _event_index_key(EventIndexKind p_kind, int64_t p_code) {
	return (uint64_t(p_kind) << 32) | uint64_t(uint32_t(p_code));
}

enum ActionEventIndexing {
	ACTION_EVENT_NEVER_MATCHES, // Can't match any event through action_match() in event_get_action_status().
	ACTION_EVENT_KEYED,
	ACTION_EVENT_UNINDEXED, // Unknown type: checked for every event.
};

// The key under which an event of an action can match, following the rules of the action_match() overrides.
ActionEventIndexing _get_action_event_key(const Ref<InputEvent> &p_event, uint64_t &r_key) {
	if (const InputEventKey *key = Object::cast_to<InputEventKey>(p_event.ptr())) {
		// Same priority as InputEventKey::action_match(): label only, else keycode, else physical keycode.
		if (key->get_keycode() == Key::NONE && key->get_physical_keycode() == Key::NONE && key->get_key_label() != Key::NONE) {
			r_key = _event_index_key(EVENT_INDEX_KEY_LABEL, int64_t(key->get_key_label()));
		} else if (key->get_keycode() != Key::NONE) {
			r_key = _event_index_key(EVENT_INDEX_KEYCODE, int64_t(key->get_keycode()));
		} else if (key->get_physical_keycode() != Key::NONE) {
			r_key = _event_index_key(EVENT_INDEX_PHYSICAL_KEYCODE, int64_t(key->get_physical_keycode()));
		} else {
			return ACTION_EVENT_NEVER_MATCHES;
		}
		return ACTION_EVENT_KEYED;
	}
	if (const InputEventMouseButton *mouse_button = Object::cast_to<InputEventMouseButton>(p_event.ptr())) {
		r_key = _event_index_key(EVENT_INDEX_MOUSE_BUTTON, int64_t(mouse_button->get_button_index()));
		return ACTION_EVENT_KEYED;
	}
	if (const InputEventJoypadButton *joy_button = Object::cast_to<InputEventJoypadButton>(p_event.ptr())) {
		r_key = _event_index_key(EVENT_INDEX_JOY_BUTTON, int64_t(joy_button->get_button_index()));
		return ACTION_EVENT_KEYED;
	}
	if (const InputEventJoypadMotion *joy_motion = Object::cast_to<InputEventJoypadMotion>(p_event.ptr())) {
		// The direction is not part of the key: the non-exact match ignores it.
		r_key = _event_index_key(EVENT_INDEX_JOY_AXIS, int64_t(joy_motion->get_axis()));
		return ACTION_EVENT_KEYED;
	}
	if (Object::cast_to<InputEventAction>(p_event.ptr())) {
		// An incoming InputEventAction is matched by name before action_match() is called, and
		// InputEventAction::action_match() only matches InputEventAction.
		return ACTION_EVENT_NEVER_MATCHES;
	}
	return ACTION_EVENT_UNINDEXED;
}

} // namespace

void InputMap::_mark_event_index_dirty() {
	event_index_dirty.set();
}

void InputMap::_rebuild_event_index() const {
	// Called with event_index_mutex held.
	event_index_dirty.clear();
	event_index.clear();
	event_index_actions.clear();
	event_index_unindexed.clear();

	uint32_t ordinal = 0;
	for (const KeyValue<StringName, Action> &E : input_map) {
		event_index_actions.push_back(E.key);
		bool unindexed = false;
		for (const Ref<InputEvent> &event : E.value.inputs) {
			uint64_t key = 0;
			switch (_get_action_event_key(event, key)) {
				case ACTION_EVENT_KEYED: {
					LocalVector<uint32_t> &list = event_index[key];
					if (list.is_empty() || list[list.size() - 1] != ordinal) {
						list.push_back(ordinal);
					}
				} break;
				case ACTION_EVENT_UNINDEXED: {
					unindexed = true;
				} break;
				case ACTION_EVENT_NEVER_MATCHES: {
				} break;
			}
		}
		if (unindexed) {
			event_index_unindexed.push_back(ordinal);
		}
		ordinal++;
	}
}

void InputMap::get_event_candidate_actions(const Ref<InputEvent> &p_event, LocalVector<StringName> &r_actions) const {
	r_actions.clear();
	if (p_event.is_null()) {
		return;
	}
	_ensure_input_map();

	// event_get_action_status() matches an InputEventAction by its action name only.
	if (const InputEventAction *event_action = Object::cast_to<InputEventAction>(p_event.ptr())) {
		if (input_map.has(event_action->get_action())) {
			r_actions.push_back(event_action->get_action());
		}
		return;
	}

	// The keys an incoming event can match. Other event types (mouse motion, touch, MIDI, gestures, ...)
	// can't match a key, mouse button or joypad event of an action; only unindexed actions are candidates.
	uint64_t keys[3];
	int key_count = 0;
	if (const InputEventKey *key = Object::cast_to<InputEventKey>(p_event.ptr())) {
		if (key->get_keycode() != Key::NONE) {
			keys[key_count++] = _event_index_key(EVENT_INDEX_KEYCODE, int64_t(key->get_keycode()));
		}
		if (key->get_physical_keycode() != Key::NONE) {
			keys[key_count++] = _event_index_key(EVENT_INDEX_PHYSICAL_KEYCODE, int64_t(key->get_physical_keycode()));
		}
		if (key->get_key_label() != Key::NONE) {
			keys[key_count++] = _event_index_key(EVENT_INDEX_KEY_LABEL, int64_t(key->get_key_label()));
		}
	} else if (const InputEventMouseButton *mouse_button = Object::cast_to<InputEventMouseButton>(p_event.ptr())) {
		keys[key_count++] = _event_index_key(EVENT_INDEX_MOUSE_BUTTON, int64_t(mouse_button->get_button_index()));
	} else if (const InputEventJoypadButton *joy_button = Object::cast_to<InputEventJoypadButton>(p_event.ptr())) {
		keys[key_count++] = _event_index_key(EVENT_INDEX_JOY_BUTTON, int64_t(joy_button->get_button_index()));
	} else if (const InputEventJoypadMotion *joy_motion = Object::cast_to<InputEventJoypadMotion>(p_event.ptr())) {
		keys[key_count++] = _event_index_key(EVENT_INDEX_JOY_AXIS, int64_t(joy_motion->get_axis()));
	}

	MutexLock lock(event_index_mutex);
	if (event_index_dirty.is_set()) {
		_rebuild_event_index();
	}

	const LocalVector<uint32_t> *lists[4];
	int list_count = 0;
	for (int i = 0; i < key_count; i++) {
		const LocalVector<uint32_t> *list = event_index.getptr(keys[i]);
		if (list != nullptr) {
			lists[list_count++] = list;
		}
	}
	if (!event_index_unindexed.is_empty()) {
		lists[list_count++] = &event_index_unindexed;
	}

	if (list_count == 1) {
		for (const uint32_t ordinal : *lists[0]) {
			r_actions.push_back(event_index_actions[ordinal]);
		}
		return;
	}

	// Several lists: merge them in the order of the action map, without duplicates.
	LocalVector<uint32_t> ordinals;
	for (int i = 0; i < list_count; i++) {
		for (const uint32_t ordinal : *lists[i]) {
			ordinals.push_back(ordinal);
		}
	}
	ordinals.sort();
	for (uint32_t i = 0; i < ordinals.size(); i++) {
		if (i == 0 || ordinals[i] != ordinals[i - 1]) {
			r_actions.push_back(event_index_actions[ordinals[i]]);
		}
	}
}

InputMap::InputMap() {
	ERR_FAIL_COND_MSG(singleton, "Singleton in InputMap already exists.");
	singleton = this;

	builtin_actions.instantiate();
	custom_actions.instantiate();
	// Any change of a layer or of one of its actions: the action map is rebuilt on the next access.
	builtin_actions->connect(CoreStringName(changed), callable_mp(this, &InputMap::_mark_input_map_dirty));
	custom_actions->connect(CoreStringName(changed), callable_mp(this, &InputMap::_mark_input_map_dirty));

	for (int i = 0; i < BuiltinInputActions::action_count; i++) {
		const String action_name = BuiltinInputActions::actions[i].name;
		builtin_action_names.insert(action_name);
		builtin_action_names.insert(action_name.get_slicec('.', 0));
	}
	input_map_dirty.set();
	event_index_dirty.set();
}

InputMap::~InputMap() {
	const Callable on_event_changed = callable_mp(this, &InputMap::_mark_event_index_dirty);
	for (const Ref<InputEvent> &event : watched_events) {
		if (event->is_connected(CoreStringName(changed), on_event_changed)) {
			event->disconnect(CoreStringName(changed), on_event_changed);
		}
	}
	singleton = nullptr;
}
