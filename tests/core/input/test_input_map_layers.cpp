/**************************************************************************/
/*  test_input_map_layers.cpp                                             */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_input_map_layers)

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_action.h"
#include "core/input/input_actions.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "core/variant/typed_array.h"
#include "tests/test_utils.h"

namespace TestInputMapLayers {

static Ref<InputEventKey> make_key(Key p_keycode, Key p_physical, Key p_label, bool p_pressed = true, int p_device = InputEvent::DEVICE_ID_KEYBOARD) {
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(p_keycode);
	event->set_physical_keycode(p_physical);
	event->set_key_label(p_label);
	event->set_pressed(p_pressed);
	event->set_device(p_device);
	return event;
}

static Ref<InputEventMouseButton> make_mouse_button(MouseButton p_button, bool p_pressed = true) {
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_button_index(p_button);
	event->set_pressed(p_pressed);
	event->set_device(InputEvent::DEVICE_ID_MOUSE);
	return event;
}

static Ref<InputEventJoypadButton> make_joy_button(JoyButton p_button, int p_device, bool p_pressed = true) {
	Ref<InputEventJoypadButton> event = InputEventJoypadButton::create_reference(p_button, p_device);
	event->set_pressed(p_pressed);
	return event;
}

static Ref<InputEventAction> make_action_event(const StringName &p_action, bool p_pressed = true) {
	Ref<InputEventAction> event;
	event.instantiate();
	event->set_action(p_action);
	event->set_pressed(p_pressed);
	return event;
}

// Names of the actions for which event_get_index() is not -1, checking every action (the old way).
static Vector<StringName> full_scan(const Ref<InputEvent> &p_event, bool p_exact) {
	Vector<StringName> result;
	InputMap *input_map = InputMap::get_singleton();
	for (const KeyValue<StringName, InputMap::Action> &E : input_map->get_action_map()) {
		if (input_map->event_get_index(p_event, E.key, p_exact) != -1) {
			result.push_back(E.key);
		}
	}
	return result;
}

// The same, checking only the candidates of the reverse index.
static Vector<StringName> indexed_scan(const Ref<InputEvent> &p_event, bool p_exact) {
	Vector<StringName> result;
	InputMap *input_map = InputMap::get_singleton();
	LocalVector<StringName> candidates;
	input_map->get_event_candidate_actions(p_event, candidates);
	for (const StringName &action : candidates) {
		if (input_map->event_get_index(p_event, action, p_exact) != -1) {
			result.push_back(action);
		}
	}
	return result;
}

static const char *TEST_ACTIONS[] = {
	"layers_keycode", "layers_physical", "layers_label", "layers_ctrl_s", "layers_mouse", "layers_joy_any",
	"layers_joy_pad1", "layers_axis", "layers_action_event", "layers_touch", "layers_mixed", "layers_empty"
};

static void erase_test_actions() {
	InputMap *input_map = InputMap::get_singleton();
	for (const char *action : TEST_ACTIONS) {
		if (input_map->has_action(action)) {
			input_map->erase_action(action);
		}
	}
}

// Actions with every kind of event the index knows, and some it does not.
static void add_test_actions() {
	erase_test_actions();
	InputMap *input_map = InputMap::get_singleton();
	for (const char *action : TEST_ACTIONS) {
		input_map->add_action(action);
	}
	input_map->action_add_event("layers_keycode", InputEventKey::create_reference(Key::W));
	input_map->action_add_event("layers_keycode", InputEventKey::create_reference(Key::UP));
	input_map->action_add_event("layers_physical", InputEventKey::create_reference(Key::A, true));
	Ref<InputEventKey> label;
	label.instantiate();
	label->set_key_label(Key::Q);
	input_map->action_add_event("layers_label", label);
	input_map->action_add_event("layers_ctrl_s", InputEventKey::create_reference(Key::S | KeyModifierMask::CTRL));
	input_map->action_add_event("layers_mouse", make_mouse_button(MouseButton::LEFT, false));
	input_map->action_add_event("layers_joy_any", InputEventJoypadButton::create_reference(JoyButton::A));
	input_map->action_add_event("layers_joy_pad1", InputEventJoypadButton::create_reference(JoyButton::A, 1));
	input_map->action_add_event("layers_axis", InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f));
	input_map->action_add_event("layers_action_event", make_action_event("layers_keycode", false));
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	input_map->action_add_event("layers_touch", touch);
	input_map->action_add_event("layers_mixed", InputEventKey::create_reference(Key::W));
	input_map->action_add_event("layers_mixed", InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, 1.0f));
	input_map->action_add_event("layers_mixed", make_mouse_button(MouseButton::RIGHT, false));
}

// Incoming events: keys, physical keys, labels, modifiers, mouse, joypads on several devices, axes, actions and others.
static Vector<Ref<InputEvent>> make_corpus() {
	Vector<Ref<InputEvent>> corpus;
	for (bool pressed : { true, false }) {
		corpus.push_back(make_key(Key::W, Key::W, Key::W, pressed));
		corpus.push_back(make_key(Key::Z, Key::W, Key::Z, pressed)); // Other layout: physical W.
		corpus.push_back(make_key(Key::NONE, Key::A, Key::NONE, pressed));
		corpus.push_back(make_key(Key::A, Key::Q, Key::A, pressed)); // AZERTY: physical Q gives A.
		corpus.push_back(make_key(Key::Q, Key::A, Key::Q, pressed));
		corpus.push_back(make_key(Key::UP, Key::UP, Key::UP, pressed, 3));
		Ref<InputEventKey> ctrl_s = make_key(Key::S, Key::S, Key::S, pressed);
		ctrl_s->set_ctrl_pressed(true);
		corpus.push_back(ctrl_s);
		Ref<InputEventKey> ctrl_shift_s = make_key(Key::S, Key::S, Key::S, pressed);
		ctrl_shift_s->set_ctrl_pressed(true);
		ctrl_shift_s->set_shift_pressed(true);
		corpus.push_back(ctrl_shift_s);
		corpus.push_back(make_key(Key::S, Key::S, Key::S, pressed));
		corpus.push_back(make_key(Key::ENTER, Key::ENTER, Key::ENTER, pressed));
		corpus.push_back(make_mouse_button(MouseButton::LEFT, pressed));
		corpus.push_back(make_mouse_button(MouseButton::RIGHT, pressed));
		corpus.push_back(make_mouse_button(MouseButton::MIDDLE, pressed));
		for (int device : { 0, 1, 2 }) {
			corpus.push_back(make_joy_button(JoyButton::A, device, pressed));
			corpus.push_back(make_joy_button(JoyButton::B, device, pressed));
		}
		corpus.push_back(make_action_event("layers_keycode", pressed));
		corpus.push_back(make_action_event("layers_touch", pressed));
		corpus.push_back(make_action_event("layers_missing_action", pressed));
	}
	for (float value : { -1.0f, -0.5f, -0.1f, 0.0f, 0.1f, 0.5f, 1.0f }) {
		for (int device : { 0, 1 }) {
			corpus.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, value, device));
			corpus.push_back(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, value, device));
		}
	}
	Ref<InputEventMouseMotion> motion;
	motion.instantiate();
	corpus.push_back(motion);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	touch->set_pressed(true);
	corpus.push_back(touch);
	Ref<InputEventMIDI> midi;
	midi.instantiate();
	corpus.push_back(midi);
	return corpus;
}

static void check_index_equals_full_scan() {
	const Vector<Ref<InputEvent>> corpus = make_corpus();
	for (const Ref<InputEvent> &event : corpus) {
		for (bool exact : { false, true }) {
			const Vector<StringName> expected = full_scan(event, exact);
			const Vector<StringName> actual = indexed_scan(event, exact);
			CHECK_MESSAGE(actual == expected, vformat("%s (exact: %s)", event->as_text(), exact ? "true" : "false"));
		}
	}
}

TEST_CASE("[InputAction] Text form and events") {
	Ref<InputAction> action;
	action.instantiate();
	action->set_action("jump");
	const char *texts[] = { "KeyboardLabel:Ctrl+S", "Keyboard:W", "Mouse:Left", "Gamepad:A", "GamepadAxis:LeftStickLeft" };
	for (const char *text : texts) {
		const Ref<InputEvent> event = InputAction::text_to_event(text);
		REQUIRE(event.is_valid());
		CHECK(InputAction::event_to_text(event) == text);
		CHECK(event->get_device() == -1);
		action->add_event(event);
	}
	CHECK(action->get_event_count() == 5);
	CHECK(action->get_device_events(InputAction::INPUT_DEVICE_GAMEPAD).size() == 2);
	CHECK(action->get_device_events(InputAction::INPUT_DEVICE_KEYBOARD).size() == 2);

	// Duplicates are not added; removing matches the event, not the object.
	action->add_event(InputEventKey::create_reference(Key::S | KeyModifierMask::CTRL));
	CHECK(action->get_event_count() == 5);
	CHECK(action->remove_event(InputEventKey::create_reference(Key::S | KeyModifierMask::CTRL)));
	CHECK(action->get_event_count() == 4);
	CHECK_FALSE(action->has_event(InputEventKey::create_reference(Key::S | KeyModifierMask::CTRL)));

	CHECK(InputAction::get_event_device(make_mouse_button(MouseButton::LEFT)) == InputAction::INPUT_DEVICE_MOUSE);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	CHECK(InputAction::get_event_device(touch) == InputAction::INPUT_DEVICE_MAX);
	CHECK(InputAction::event_to_text(touch).is_empty());
	CHECK(InputAction::text_to_event("Nonsense").is_null());

	// `events` is a stored property (sub-resources in `.tres`); there is no text property.
	bool found_events = false;
	List<PropertyInfo> properties;
	action->get_property_list(&properties);
	for (const PropertyInfo &info : properties) {
		CHECK(info.name != "bindings");
		if (info.name == "events") {
			found_events = true;
			CHECK((info.usage & PROPERTY_USAGE_STORAGE) != 0);
			CHECK((info.usage & PROPERTY_USAGE_EDITOR) != 0);
		}
	}
	CHECK(found_events);
}

// The text resource format and the binary format keep the events of every action as sub-resources.
static void check_input_actions_round_trip(const String &p_path) {
	Ref<InputActions> actions;
	actions.instantiate();
	Ref<InputAction> jump = Ref<InputAction>(memnew(InputAction("jump", 0.5f)));
	jump->add_event(InputEventKey::create_reference(Key::SPACE));
	Ref<InputEventKey> physical;
	physical.instantiate();
	physical->set_physical_keycode(Key::W);
	physical->set_ctrl_pressed(true);
	jump->add_event(physical);
	jump->add_event(InputEventJoypadButton::create_reference(JoyButton::A, 1));
	jump->add_event(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f, -1));
	Ref<InputEventMouseButton> mouse;
	mouse.instantiate();
	mouse->set_button_index(MouseButton::RIGHT);
	jump->add_event(mouse);
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	touch->set_index(2);
	jump->add_event(touch); // No text form, but stored as a sub-resource too.
	actions->add_action(jump);
	actions->add_action(Ref<InputAction>(memnew(InputAction("empty"))));

	REQUIRE(ResourceSaver::save(actions, p_path) == OK);
	const Ref<InputActions> loaded = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded != actions);
	REQUIRE(loaded->get_action_count() == 2);
	const Ref<InputAction> loaded_jump = loaded->get_action("jump");
	REQUIRE(loaded_jump.is_valid());
	CHECK(loaded_jump->get_deadzone() == doctest::Approx(0.5f));
	REQUIRE(loaded_jump->get_event_count() == 6);
	for (int i = 0; i < 6; i++) {
		const Ref<InputEvent> saved = jump->get_event_list()[i];
		const Ref<InputEvent> read = loaded_jump->get_event_list()[i];
		CHECK(read->get_class() == saved->get_class());
		CHECK(read->get_device() == saved->get_device());
		if (i < 5) {
			CHECK(read->is_match(saved, true));
		}
	}
	const Ref<InputEventScreenTouch> loaded_touch = loaded_jump->get_event_list()[5];
	REQUIRE(loaded_touch.is_valid());
	CHECK(loaded_touch->get_index() == 2);
	CHECK(loaded->get_action("empty")->get_event_count() == 0);
	CHECK(loaded->get_action("empty")->get_deadzone() == doctest::Approx(InputAction::DEFAULT_DEADZONE));
}

TEST_CASE("[InputActions] Round trip through ResourceSaver and ResourceLoader") {
	const String folder = TestUtils::get_temp_path("input_actions_round_trip");
	REQUIRE(DirAccess::make_dir_recursive_absolute(folder) == OK);

	SUBCASE("Text resource (.tres)") {
		const String path = folder.path_join("actions.tres");
		check_input_actions_round_trip(path);
		// The layout of the editor: events as sub-resources, the deadzone only when it is not the default.
		const String text = FileAccess::get_file_as_string(path);
		CHECK(text.begins_with("[gd_resource type=\"InputActions\" format=3"));
		CHECK(text.contains("[sub_resource type=\"InputEventKey\""));
		CHECK(text.contains("action = &\"jump\"\ndeadzone = 0.5\nevents = Array[InputEvent]([SubResource(\""));
		CHECK(text.contains("[sub_resource type=\"InputAction\" id=\"InputAction_"));
		CHECK(text.contains("action = &\"empty\"\n\n"));
		CHECK_FALSE(text.contains("bindings"));
		CHECK(text.contains("actions = Array[InputAction]([SubResource(\""));
	}

	SUBCASE("Binary resource (.res)") {
		check_input_actions_round_trip(folder.path_join("actions.res"));
	}
}

TEST_CASE("[InputActions] Lookup by name, replace, rename and remove") {
	Ref<InputActions> actions;
	actions.instantiate();
	Ref<InputAction> jump = Ref<InputAction>(memnew(InputAction("jump", 0.3f)));
	Ref<InputAction> fire = Ref<InputAction>(memnew(InputAction("fire")));
	actions->add_action(jump);
	actions->add_action(fire);
	CHECK(actions->get_action_count() == 2);
	CHECK(actions->get_action("jump") == jump);

	// The same name replaces the action in place.
	Ref<InputAction> jump2 = Ref<InputAction>(memnew(InputAction("jump", 0.5f)));
	actions->add_action(jump2);
	CHECK(actions->get_action_count() == 2);
	CHECK(actions->get_action("jump") == jump2);
	CHECK(StringName(actions->get_action_names()[0]) == StringName("jump"));

	// Renaming an action updates the lookup.
	fire->set_action("shoot");
	CHECK_FALSE(actions->has_action("fire"));
	CHECK(actions->has_action("shoot"));

	CHECK(actions->remove_action("jump"));
	CHECK_FALSE(actions->has_action("jump"));
	CHECK(actions->get_action("shoot") == fire);

	// set_actions(): all entries are kept (with a warning), the later action of a name wins.
	TypedArray<InputAction> list;
	list.push_back(Ref<InputAction>(memnew(InputAction("a", 0.1f))));
	list.push_back(Ref<InputAction>(memnew(InputAction("b"))));
	list.push_back(Ref<InputAction>(memnew(InputAction("a", 0.9f))));
	ERR_PRINT_OFF;
	actions->set_actions(list);
	ERR_PRINT_ON;
	CHECK(actions->get_action_count() == 3);
	CHECK(actions->get_actions().size() == 3);
	CHECK(actions->get_action("a")->get_deadzone() == doctest::Approx(0.9f));
	CHECK(actions->remove_action("a"));
	CHECK(actions->get_action("a")->get_deadzone() == doctest::Approx(0.1f));
}

TEST_CASE("[InputActions] Empty slots of the Inspector are kept (#398)") {
	// "Add Element" in the Inspector sets the array with a null slot; it must stay until it gets a value.
	Ref<InputAction> action = Ref<InputAction>(memnew(InputAction("jump")));
	TypedArray<InputEvent> events;
	events.push_back(InputEventJoypadButton::create_reference(JoyButton::A));
	events.push_back(Variant());
	action->set_events(events);
	CHECK(action->get_events().size() == 2);
	// A typed array keeps the empty slot as a null object.
	CHECK(Ref<InputEvent>(action->get_events()[1]).is_null());
	CHECK(action->get_event_count() == 1);
	CHECK(action->get_device_events(InputAction::INPUT_DEVICE_GAMEPAD).size() == 1);
	CHECK_FALSE(action->has_event(Ref<InputEventKey>(memnew(InputEventKey))));
	// A method that changes the events drops the empty slots.
	action->clear_events();
	CHECK(action->get_events().is_empty());

	Ref<InputActions> actions;
	actions.instantiate();
	TypedArray<InputAction> list;
	list.push_back(Variant());
	list.push_back(action);
	actions->set_actions(list);
	CHECK(actions->get_actions().size() == 2);
	CHECK(actions->get_action_count() == 1);
	CHECK(actions->get_action("jump") == action);
	actions->clear();
	CHECK(actions->get_actions().is_empty());
}

TEST_CASE("[SceneTree][InputMap] Built-in actions come from the generated data") {
	InputMap *input_map = InputMap::get_singleton();
	const HashMap<String, List<Ref<InputEvent>>> &builtins = input_map->get_builtins();

	// The numbers of the old hard-coded get_builtins() (Godot 4.8 beta), compared entry by entry when it was removed (#172).
	CHECK(builtins.size() == 94);
	int event_count = 0;
	for (const KeyValue<String, List<Ref<InputEvent>>> &E : builtins) {
		event_count += E.value.size();
		CHECK(input_map->is_builtin_action_name(E.key));
	}
	CHECK(event_count == 128);
	CHECK(builtins.begin()->key == "ui_accept");

	const List<Ref<InputEvent>> &accept = builtins["ui_accept"];
	REQUIRE(accept.size() == 3);
	CHECK(accept.front()->get()->is_match(InputEventKey::create_reference(Key::ENTER), true));
	CHECK(accept.back()->get()->is_match(InputEventKey::create_reference(Key::SPACE), true));

	const List<Ref<InputEvent>> &left = builtins["ui_left"];
	REQUIRE(left.size() == 3);
	CHECK(Ref<InputEventKey>(left.get(0)).is_valid());
	const Ref<InputEventJoypadButton> dpad = left.get(1);
	REQUIRE(dpad.is_valid());
	CHECK(dpad->get_button_index() == JoyButton::DPAD_LEFT);
	const Ref<InputEventJoypadMotion> stick = left.get(2);
	REQUIRE(stick.is_valid());
	CHECK(stick->get_axis() == JoyAxis::LEFT_X);
	CHECK(stick->get_axis_value() == doctest::Approx(-1.0f));

	const List<Ref<InputEvent>> &close_macos = builtins["ui_close_dialog.macos"];
	REQUIRE(close_macos.size() == 2);
	CHECK(close_macos.front()->get()->is_match(InputEventKey::create_reference(Key::W | KeyModifierMask::META), true));
	CHECK(builtins["ui_accessibility_drag_and_drop"].is_empty());
	CHECK(builtins["ui_text_newline_above"].front()->get()->is_match(InputEventKey::create_reference(Key::ENTER | KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL), true));

	for (const KeyValue<String, List<Ref<InputEvent>>> &E : input_map->get_builtins_with_feature_overrides_applied()) {
		CHECK(E.key.find_char('.') == -1);
	}
}

TEST_CASE("[SceneTree][InputMap] Layers: built-in and custom actions") {
	InputMap *input_map = InputMap::get_singleton();
	erase_test_actions();
	const bool had_accept = input_map->has_action("ui_accept");
	if (!had_accept) {
		input_map->add_action("ui_accept");
		input_map->action_add_event("ui_accept", InputEventKey::create_reference(Key::ENTER));
	}
	input_map->add_action("layers_keycode", 0.4f);
	input_map->action_add_event("layers_keycode", InputEventKey::create_reference(Key::W));

	CHECK(input_map->is_builtin_input_action("ui_accept"));
	// A lookup in the built-in layer: a platform variant is a built-in name, but not an action of the map.
	CHECK(input_map->is_builtin_action_name("ui_text_backspace_word.macos"));
	CHECK_FALSE(input_map->is_builtin_input_action("ui_text_backspace_word.macos"));
	CHECK_FALSE(input_map->is_custom_input_action("ui_accept"));
	CHECK(input_map->is_custom_input_action("layers_keycode"));
	CHECK_FALSE(input_map->is_builtin_input_action("layers_keycode"));
	CHECK_FALSE(input_map->is_custom_input_action("layers_missing_action"));
	CHECK(input_map->get_input_action_layer("ui_accept") == InputMap::INPUT_ACTION_LAYER_BUILTIN);
	CHECK(input_map->get_input_action_layer("layers_keycode") == InputMap::INPUT_ACTION_LAYER_CUSTOM);

	const Ref<InputActions> builtin_layer = input_map->get_input_actions(InputMap::INPUT_ACTION_LAYER_BUILTIN);
	const Ref<InputActions> custom_layer = input_map->get_input_actions(InputMap::INPUT_ACTION_LAYER_CUSTOM);
	CHECK(builtin_layer->has_action("ui_accept"));
	CHECK_FALSE(builtin_layer->has_action("layers_keycode"));
	CHECK(custom_layer->has_action("layers_keycode"));
	CHECK_FALSE(custom_layer->has_action("ui_accept"));
	CHECK(builtin_layer->get_action_count() + custom_layer->get_action_count() == input_map->get_actions().size());

	// InputMap references the layers: get_input_actions() returns the same objects every time.
	CHECK(input_map->get_input_actions(InputMap::INPUT_ACTION_LAYER_BUILTIN) == builtin_layer);
	CHECK(input_map->get_input_actions(InputMap::INPUT_ACTION_LAYER_CUSTOM) == custom_layer);

	// get_input_action() is the action of the layer: changing it changes the action map.
	Ref<InputAction> action = input_map->get_input_action("layers_keycode");
	REQUIRE(action.is_valid());
	CHECK(action == custom_layer->get_action("layers_keycode"));
	CHECK(action->get_deadzone() == doctest::Approx(0.4f));
	REQUIRE(action->get_event_count() == 1);
	CHECK(InputAction::event_to_text(action->get_event_list()[0]) == "KeyboardLabel:W");
	CHECK(input_map->event_is_action(make_key(Key::W, Key::W, Key::W), "layers_keycode"));
	action->clear_events();
	action->add_event(InputEventKey::create_reference(Key::E));
	action->set_deadzone(0.45f);
	CHECK(input_map->action_get_events("layers_keycode")->size() == 1);
	CHECK(input_map->get_action_map()["layers_keycode"].deadzone == doctest::Approx(0.45f));
	CHECK_FALSE(input_map->event_is_action(make_key(Key::W, Key::W, Key::W), "layers_keycode"));
	CHECK(input_map->event_is_action(make_key(Key::E, Key::E, Key::E), "layers_keycode"));

	// set_input_action() puts the resource itself into the layer, in the place of the old one.
	Ref<InputAction> replacement = Ref<InputAction>(memnew(InputAction("layers_keycode", 0.6f)));
	replacement->add_event(InputEventKey::create_reference(Key::R));
	input_map->set_input_action(replacement);
	CHECK(input_map->get_input_action("layers_keycode") == replacement);
	CHECK(custom_layer->get_action("layers_keycode") == replacement);
	CHECK(input_map->action_get_deadzone("layers_keycode") == doctest::Approx(0.6f));
	CHECK_FALSE(input_map->event_is_action(make_key(Key::E, Key::E, Key::E), "layers_keycode"));
	CHECK(input_map->event_is_action(make_key(Key::R, Key::R, Key::R), "layers_keycode"));

	// The InputMap API writes into the layer of the action.
	input_map->action_add_event("layers_keycode", InputEventKey::create_reference(Key::T));
	CHECK(replacement->get_event_count() == 2);
	input_map->action_erase_event("layers_keycode", InputEventKey::create_reference(Key::R));
	REQUIRE(replacement->get_event_count() == 1);
	CHECK(InputAction::event_to_text(replacement->get_event_list()[0]) == "KeyboardLabel:T");
	input_map->action_set_deadzone("layers_keycode", 0.7f);
	CHECK(replacement->get_deadzone() == doctest::Approx(0.7f));
	CHECK(builtin_layer->has_action("ui_accept"));

	// Direct edits of a layer: the action map follows on the next access.
	Ref<InputAction> direct = Ref<InputAction>(memnew(InputAction("layers_label")));
	direct->add_event(InputEventKey::create_reference(Key::Y));
	custom_layer->add_action(direct);
	CHECK(input_map->has_action("layers_label"));
	CHECK(input_map->is_custom_input_action("layers_label"));
	CHECK(input_map->get_action_map().has("layers_label"));
	CHECK(input_map->event_is_action(make_key(Key::Y, Key::Y, Key::Y), "layers_label"));
	custom_layer->remove_action("layers_label");
	CHECK_FALSE(input_map->has_action("layers_label"));
	CHECK_FALSE(input_map->get_action_map().has("layers_label"));

	// set_input_actions() adds missing actions.
	Ref<InputActions> added;
	added.instantiate();
	Ref<InputAction> new_action = Ref<InputAction>(memnew(InputAction("layers_physical", 0.25f)));
	new_action->add_event(InputEventKey::create_reference(Key::A, true));
	added->add_action(new_action);
	input_map->set_input_actions(added);
	CHECK(input_map->is_custom_input_action("layers_physical"));
	CHECK(input_map->action_get_deadzone("layers_physical") == doctest::Approx(0.25f));

	erase_test_actions();
	if (!had_accept) {
		input_map->erase_action("ui_accept");
	}
}

TEST_CASE("[SceneTree][InputMap] Merge order and project overrides of ui_* actions") {
	InputMap *input_map = InputMap::get_singleton();
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const bool had_accept_setting = project_settings->has_setting("input/ui_accept");
	const Variant old_accept_setting = had_accept_setting ? project_settings->get_setting("input/ui_accept") : Variant();

	// A custom action defined before the project's change of ui_accept.
	Dictionary custom;
	custom["deadzone"] = 0.2f;
	Array custom_events;
	custom_events.push_back(InputEventKey::create_reference(Key::J));
	custom["events"] = custom_events;
	project_settings->set_setting("input/layers_merge_custom", custom);

	Dictionary accept;
	accept["deadzone"] = 0.5f;
	Array accept_events;
	accept_events.push_back(InputEventKey::create_reference(Key::K)); // The project rebinds ui_accept to K.
	accept["events"] = accept_events;
	project_settings->set_setting("input/ui_accept", accept);

	input_map->load_from_project_settings();

	// ui_accept is still built-in, with the project's events.
	CHECK(input_map->is_builtin_input_action("ui_accept"));
	CHECK(input_map->event_is_action(make_key(Key::K, Key::K, Key::K), "ui_accept"));
	CHECK(input_map->is_custom_input_action("layers_merge_custom"));

	// All built-in actions come before all custom actions.
	const TypedArray<StringName> actions = input_map->get_actions();
	bool seen_custom = false;
	for (int i = 0; i < actions.size(); i++) {
		const bool builtin = input_map->is_builtin_input_action(actions[i]);
		if (!builtin) {
			seen_custom = true;
		}
		CHECK_FALSE((builtin && seen_custom));
	}

	project_settings->set_setting("input/layers_merge_custom", Variant());
	project_settings->set_setting("input/ui_accept", old_accept_setting);
	input_map->load_from_project_settings();
}

TEST_CASE("[SceneTree][InputMap] Reverse index gives the same actions as the full scan") {
	add_test_actions();
	check_index_equals_full_scan();

	// The index follows changes of the map and of events changed in place.
	InputMap *input_map = InputMap::get_singleton();
	const Ref<InputEventKey> up = input_map->action_get_events("layers_keycode")->back()->get();
	up->set_keycode(Key::S);
	check_index_equals_full_scan();
	input_map->action_erase_events("layers_mixed");
	input_map->erase_action("layers_label");
	input_map->action_add_event("layers_empty", make_joy_button(JoyButton::B, 2, false));
	check_index_equals_full_scan();

	erase_test_actions();
}

TEST_CASE("[SceneTree][InputMap] Input::parse_input_event() uses the index") {
	InputMap *input_map = InputMap::get_singleton();
	add_test_actions();
	Input *input = Input::get_singleton();

	input->parse_input_event(make_key(Key::W, Key::W, Key::W, true));
	input->flush_buffered_events();
	CHECK(input->is_action_pressed("layers_keycode"));
	CHECK(input->is_action_pressed("layers_mixed"));
	CHECK_FALSE(input->is_action_pressed("layers_physical"));

	input->parse_input_event(make_key(Key::W, Key::W, Key::W, false));
	input->flush_buffered_events();
	CHECK_FALSE(input->is_action_pressed("layers_keycode"));

	input->parse_input_event(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -0.8f, 0));
	input->flush_buffered_events();
	CHECK(input->is_action_pressed("layers_axis"));
	CHECK(input->get_action_raw_strength("layers_axis") == doctest::Approx(0.8f));
	input->parse_input_event(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, 0.0f, 0));
	input->flush_buffered_events();
	CHECK_FALSE(input->is_action_pressed("layers_axis"));

	erase_test_actions();
	CHECK_FALSE(input_map->has_action("layers_keycode"));
}

TEST_CASE("[SceneTree][InputMap][Benchmark] Event lookup with 100 actions: full scan and reverse index") {
	InputMap *input_map = InputMap::get_singleton();
	static const Key keys[] = { Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H, Key::I, Key::J };
	Vector<String> bench_actions;
	for (int i = 0; i < 100; i++) {
		const String action = vformat("layers_bench_%d", i);
		bench_actions.push_back(action);
		input_map->add_action(action);
		Key key = keys[i % 10];
		if (i >= 50) {
			key = key | KeyModifierMask::SHIFT;
		}
		input_map->action_add_event(action, InputEventKey::create_reference(key));
		input_map->action_add_event(action, InputEventJoypadButton::create_reference(JoyButton(i % 20)));
	}

	const Vector<Ref<InputEvent>> events = {
		make_key(Key::A, Key::A, Key::A),
		make_key(Key::F12, Key::F12, Key::F12),
		make_joy_button(JoyButton::X, 0),
		InputEventJoypadMotion::create_reference(JoyAxis::RIGHT_Y, 0.3f, 0),
	};
	constexpr int ROUNDS = 2000;

	int full_matches = 0;
	uint64_t start = OS::get_singleton()->get_ticks_usec();
	for (int round = 0; round < ROUNDS; round++) {
		for (const Ref<InputEvent> &event : events) {
			full_matches += int(full_scan(event, false).size());
		}
	}
	const uint64_t full_usec = OS::get_singleton()->get_ticks_usec() - start;

	int indexed_matches = 0;
	start = OS::get_singleton()->get_ticks_usec();
	for (int round = 0; round < ROUNDS; round++) {
		for (const Ref<InputEvent> &event : events) {
			indexed_matches += int(indexed_scan(event, false).size());
		}
	}
	const uint64_t indexed_usec = OS::get_singleton()->get_ticks_usec() - start;

	CHECK(full_matches == indexed_matches);
	const int lookups = ROUNDS * int(events.size());
	print_line(vformat("InputMap event lookup, %d actions (+ project actions), %d events: full scan %.3f us/event, reverse index %.3f us/event.",
			input_map->get_actions().size(), lookups, double(full_usec) / lookups, double(indexed_usec) / lookups));

	for (const String &action : bench_actions) {
		input_map->erase_action(action);
	}
}

} // namespace TestInputMapLayers
