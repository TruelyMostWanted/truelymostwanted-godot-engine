/**************************************************************************/
/*  test_input_action_rebinder.cpp                                        */
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

TEST_FORCE_LINK(test_input_action_rebinder)

#include "core/config/project_settings.h"
#include "core/input/input_action.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "servers/settings/input_action_rebinder.h"
#include "servers/settings/input_map_settings_server.h"
#include "tests/test_utils.h"

namespace TestInputActionRebinder {

static constexpr const char *JUMP = "rb_test_jump";
static constexpr const char *FIRE = "rb_test_fire";

static Ref<InputEventKey> make_project_key(Key p_keycode) {
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(p_keycode);
	return event;
}

// A key as the player presses (or releases) it: all three codes are set.
static Ref<InputEventKey> make_key(Key p_keycode, bool p_pressed = true) {
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(p_keycode);
	event->set_physical_keycode(p_keycode);
	event->set_key_label(p_keycode);
	event->set_pressed(p_pressed);
	return event;
}

static Ref<InputEventMouseButton> make_mouse_button(MouseButton p_button, bool p_pressed = true) {
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_button_index(p_button);
	event->set_pressed(p_pressed);
	return event;
}

static Ref<InputEventJoypadButton> make_joy_button(JoyButton p_button, int p_device) {
	Ref<InputEventJoypadButton> event = InputEventJoypadButton::create_reference(p_button, p_device);
	event->set_pressed(true);
	return event;
}

static String events_text(InputMapSettingsServer *p_server, const StringName &p_action) {
	return InputMapSettingsServer::events_to_text(p_server->get_events(p_action));
}

static bool is_action(const Ref<InputEvent> &p_event, const StringName &p_action) {
	return InputMap::get_singleton()->event_is_action(p_event, p_action);
}

// The "project": jump on Space and joypad X, fire on the left mouse button.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		InputMap *input_map = InputMap::get_singleton();
		remove_actions();
		input_map->add_action(JUMP, 0.3f);
		input_map->action_add_event(JUMP, make_project_key(Key::SPACE));
		input_map->action_add_event(JUMP, InputEventJoypadButton::create_reference(JoyButton::X));
		input_map->add_action(FIRE, 0.2f);
		Ref<InputEventMouseButton> left;
		left.instantiate();
		left->set_button_index(MouseButton::LEFT);
		input_map->action_add_event(FIRE, left);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
		remove_actions();
	}
	static void remove_actions() {
		InputMap *input_map = InputMap::get_singleton();
		for (const char *action : { JUMP, FIRE }) {
			if (input_map->has_action(action)) {
				input_map->erase_action(action);
			}
		}
	}
};

// The built-in `ui_*` actions of the test InputMap never count, unless a test asks for them.
static Ref<InputActionRebinder> make_rebinder(InputMapSettingsServer *p_server, bool p_ignore_builtin = true) {
	Ref<InputActionRebinder> rebinder;
	rebinder.instantiate();
	rebinder->set_settings_server(p_server);
	if (p_ignore_builtin) {
		PackedStringArray builtin;
		InputMap *input_map = InputMap::get_singleton();
		const TypedArray<StringName> names = input_map->get_actions();
		for (int i = 0; i < names.size(); i++) {
			const StringName action_name = names[i];
			if (input_map->is_builtin_input_action(action_name)) {
				builtin.push_back(action_name);
			}
		}
		rebinder->set_ignored_actions(builtin);
	}
	return rebinder;
}

// Records the signals of the server and the rebinders in the order they come.
class RebindRecorder : public Object {
public:
	PackedStringArray log;

	void on_value_changed(const StringName &p_name, const Variant &) {
		log.push_back(vformat("value_changed:%s", p_name));
	}
	// The events of the action in InputMap when the server announced the change.
	HashMap<StringName, String> applied_events;

	void on_action_events_changed(const StringName &p_action) {
		log.push_back(vformat("changed:%s", p_action));
		TypedArray<InputEvent> events;
		const List<Ref<InputEvent>> *list = InputMap::get_singleton()->action_get_events(p_action);
		if (list != nullptr) {
			for (const Ref<InputEvent> &event : *list) {
				events.push_back(event);
			}
		}
		applied_events[p_action] = InputMapSettingsServer::events_to_text(events);
	}
	void on_started(const StringName &p_action, int p_index) {
		log.push_back(vformat("started:%s:%d", p_action, p_index));
	}
	void on_canceled(const StringName &p_action, int p_index) {
		log.push_back(vformat("canceled:%s:%d", p_action, p_index));
	}
	void on_conflict(const StringName &p_source, int p_source_index, const StringName &p_dest, int p_dest_index) {
		log.push_back(vformat("conflict:%s:%d:%s:%d", p_source, p_source_index, p_dest, p_dest_index));
	}
	void on_completed(const StringName &p_action, int p_index, const Ref<InputEvent> &p_event) {
		log.push_back(vformat("completed:%s:%d:%s", p_action, p_index, InputAction::event_to_text(p_event)));
	}

	void watch_server(InputMapSettingsServer *p_server) {
		p_server->connect(SNAME("action_events_changed"), callable_mp(this, &RebindRecorder::on_action_events_changed));
	}
	void watch(const Ref<InputActionRebinder> &p_rebinder) {
		p_rebinder->connect(SNAME("started"), callable_mp(this, &RebindRecorder::on_started));
		p_rebinder->connect(SNAME("canceled"), callable_mp(this, &RebindRecorder::on_canceled));
		p_rebinder->connect(SNAME("conflict"), callable_mp(this, &RebindRecorder::on_conflict));
		p_rebinder->connect(SNAME("completed"), callable_mp(this, &RebindRecorder::on_completed));
	}
	String last() const {
		return log.is_empty() ? String() : log[log.size() - 1];
	}
};

TEST_CASE("[SceneTree][InputActionRebinder] Filtering: motion, release, echo, weak axis, modifier alone, device filter") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	Ref<InputActionRebinder> rebinder = make_rebinder(server);
	recorder->watch(rebinder);
	rebinder->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);

	CHECK_FALSE(rebinder->update(make_key(Key::W))); // No session.
	CHECK(rebinder->begin(JUMP, 0) == OK);
	CHECK(rebinder->is_active());
	CHECK(recorder->last() == vformat("started:%s:0", JUMP));

	Ref<InputEventMouseMotion> motion;
	motion.instantiate();
	motion->set_relative(Vector2(10, 0));
	CHECK_FALSE(rebinder->update(motion));
	CHECK_FALSE(rebinder->update(make_key(Key::W, false))); // Released.
	Ref<InputEventKey> echo = make_key(Key::W);
	echo->set_echo(true);
	CHECK_FALSE(rebinder->update(echo));
	CHECK_FALSE(rebinder->update(make_mouse_button(MouseButton::RIGHT, false)));
	CHECK_FALSE(rebinder->update(make_joy_button(JoyButton::B, 0))); // Not in the device filter.
	Ref<InputEventScreenTouch> touch;
	touch.instantiate();
	touch->set_pressed(true);
	CHECK_FALSE(rebinder->update(touch));

	// A modifier alone is used but not bound; with the next key it is a combination.
	Ref<InputEventKey> ctrl = make_key(Key::CTRL);
	ctrl->set_ctrl_pressed(true);
	CHECK(rebinder->update(ctrl));
	CHECK(rebinder->is_active());
	CHECK(events_text(server, JUMP) == "KeyboardLabel:Space; Gamepad:X");
	Ref<InputEventKey> ctrl_s = make_key(Key::S);
	ctrl_s->set_ctrl_pressed(true);
	CHECK(rebinder->update(ctrl_s));
	CHECK_FALSE(rebinder->is_active());
	CHECK(recorder->last() == vformat("completed:%s:0:Keyboard:Ctrl+S", JUMP));
	CHECK(events_text(server, JUMP) == "Keyboard:Ctrl+S; Gamepad:X");

	// A modifier released before another key is bound alone (Shift to sprint).
	CHECK(rebinder->begin(JUMP, 1) == OK);
	Ref<InputEventKey> shift = make_key(Key::SHIFT);
	shift->set_shift_pressed(true);
	CHECK(rebinder->update(shift));
	CHECK(rebinder->is_active());
	CHECK(rebinder->update(make_key(Key::SHIFT, false)));
	CHECK_FALSE(rebinder->is_active());
	CHECK(recorder->last() == vformat("completed:%s:1:Keyboard:Shift", JUMP));
	CHECK(events_text(server, JUMP) == "Keyboard:Ctrl+S; Gamepad:X; Keyboard:Shift");

	// Gamepad: a weak stick move is ignored, a strong one keeps its direction; another pad is ignored.
	rebinder->set_device_filter(InputActionRebinder::DEVICE_GAMEPAD);
	rebinder->set_gamepad_device(1);
	CHECK(rebinder->begin(JUMP, 0) == OK);
	CHECK_FALSE(rebinder->update(make_key(Key::Q)));
	CHECK_FALSE(rebinder->update(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -0.3f, 1)));
	CHECK_FALSE(rebinder->update(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -0.9f, 0)));
	CHECK(rebinder->update(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -0.9f, 1)));
	CHECK(recorder->last() == vformat("completed:%s:0:GamepadAxis:LeftStickUp", JUMP));
	// Stored for all pads, so the binding still works after the pad is reconnected with another id.
	CHECK(events_text(server, JUMP) == "Keyboard:Ctrl+S; GamepadAxis:LeftStickUp; Keyboard:Shift");
	CHECK(is_action(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_Y, -1.0f, 3), JUMP));

	// Errors of begin().
	ERR_PRINT_OFF;
	CHECK(rebinder->begin("rb_test_no_such_action", 0) == ERR_DOES_NOT_EXIST);
	CHECK(rebinder->begin(JUMP, -1) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK_FALSE(rebinder->is_active());

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] CONFLICT_REJECT emits conflict and waits for another input") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	recorder->watch_server(server);
	Ref<InputActionRebinder> rebinder = make_rebinder(server);
	recorder->watch(rebinder);
	rebinder->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);
	rebinder->set_conflict_mode(InputActionRebinder::CONFLICT_REJECT);

	CHECK(rebinder->begin(FIRE, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(recorder->last() == vformat("conflict:%s:0:%s:0", FIRE, JUMP));
	CHECK(rebinder->is_active());
	CHECK_FALSE(rebinder->is_conflict_pending());
	CHECK(InputAction::event_to_text(rebinder->get_conflict_event()) == "Keyboard:Space");
	CHECK(events_text(server, FIRE) == "Mouse:Left");
	CHECK(events_text(server, JUMP) == "KeyboardLabel:Space; Gamepad:X");

	CHECK(rebinder->update(make_key(Key::F)));
	CHECK(recorder->last() == vformat("completed:%s:0:Keyboard:F", FIRE));
	CHECK(events_text(server, FIRE) == "Keyboard:F");
	CHECK(is_action(make_key(Key::F), FIRE));

	// An ignored action is no conflict.
	// Keep the built-in actions ignored (Space is also ui_select) and add JUMP.
	PackedStringArray ignored = rebinder->get_ignored_actions();
	ignored.push_back(JUMP);
	rebinder->set_ignored_actions(ignored);
	CHECK(rebinder->begin(FIRE, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(recorder->last() == vformat("completed:%s:0:Keyboard:Space", FIRE));

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] CONFLICT_SWAP exchanges both events in one step; the server signals come before completed") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	recorder->watch_server(server);
	Ref<InputActionRebinder> rebinder = make_rebinder(server);
	recorder->watch(rebinder);
	rebinder->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);
	rebinder->set_conflict_mode(InputActionRebinder::CONFLICT_SWAP);
	server->connect(SNAME("value_changed"), callable_mp(recorder, &RebindRecorder::on_value_changed));

	CHECK(rebinder->begin(FIRE, 0) == OK);
	recorder->log.clear();
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(events_text(server, FIRE) == "Keyboard:Space");
	CHECK(events_text(server, JUMP) == "Mouse:Left; Gamepad:X");
	CHECK(is_action(make_key(Key::SPACE), FIRE));
	CHECK_FALSE(is_action(make_key(Key::SPACE), JUMP));
	CHECK(is_action(make_mouse_button(MouseButton::LEFT), JUMP));

	// Order: one change of the server (one value_changed, so one apply and one autosave), the server announces
	// both actions, then the rebinder completes. No conflict signal.
	REQUIRE(recorder->log.size() == 4);
	CHECK(recorder->log[0] == "value_changed:actions");
	CHECK(recorder->log.has(vformat("changed:%s", FIRE)));
	CHECK(recorder->log.has(vformat("changed:%s", JUMP)));
	CHECK(recorder->log[3] == vformat("completed:%s:0:Keyboard:Space", FIRE));
	// `action_events_changed` comes after the apply: InputMap already has the new events.
	CHECK(recorder->applied_events[FIRE] == "Keyboard:Space");
	CHECK(recorder->applied_events[JUMP] == "Mouse:Left; Gamepad:X");

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] CONFLICT_ALLOW binds the event to both actions") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	Ref<InputActionRebinder> rebinder = make_rebinder(server);
	recorder->watch(rebinder);
	rebinder->set_conflict_mode(InputActionRebinder::CONFLICT_ALLOW);

	// DEVICE_ANY: index 2 is behind the last event of fire, so the event is added.
	CHECK(rebinder->begin(FIRE, 2) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(recorder->last() == vformat("completed:%s:1:Keyboard:Space", FIRE));
	CHECK(events_text(server, FIRE) == "Mouse:Left; Keyboard:Space");
	CHECK(events_text(server, JUMP) == "KeyboardLabel:Space; Gamepad:X");
	CHECK(is_action(make_key(Key::SPACE), FIRE));
	CHECK(is_action(make_key(Key::SPACE), JUMP));

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] CONFLICT_ASK waits for swap, allow or cancel") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	recorder->watch_server(server);
	Ref<InputActionRebinder> rebinder = make_rebinder(server);
	recorder->watch(rebinder);
	rebinder->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);
	CHECK(rebinder->get_conflict_mode() == InputActionRebinder::CONFLICT_ASK);

	ERR_PRINT_OFF;
	CHECK(rebinder->resolve_conflict_allow() == ERR_UNCONFIGURED);
	CHECK(rebinder->resolve_conflict_swap() == ERR_UNCONFIGURED);
	ERR_PRINT_ON;

	// The key of the slot itself is no conflict; the slot gets the stored form of the pressed key.
	CHECK(rebinder->begin(JUMP, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(recorder->last() == vformat("completed:%s:0:Keyboard:Space", JUMP));
	CHECK(events_text(server, JUMP) == "Keyboard:Space; Gamepad:X");

	// Cancel: nothing changes.
	CHECK(rebinder->begin(FIRE, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(recorder->last() == vformat("conflict:%s:0:%s:0", FIRE, JUMP));
	CHECK(rebinder->is_conflict_pending());
	CHECK_FALSE(rebinder->update(make_key(Key::G))); // No input while the menu asks.
	recorder->log.clear();
	rebinder->cancel();
	CHECK(recorder->log == PackedStringArray({ vformat("canceled:%s:0", FIRE) }));
	CHECK_FALSE(rebinder->is_active());
	CHECK_FALSE(rebinder->is_conflict_pending());
	CHECK(events_text(server, FIRE) == "Mouse:Left");
	CHECK(events_text(server, JUMP) == "Keyboard:Space; Gamepad:X");

	// Allow.
	CHECK(rebinder->begin(FIRE, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(rebinder->resolve_conflict_allow() == OK);
	CHECK_FALSE(rebinder->is_active());
	CHECK(recorder->last() == vformat("completed:%s:0:Keyboard:Space", FIRE));
	CHECK(events_text(server, FIRE) == "Keyboard:Space");
	CHECK(events_text(server, JUMP) == "Keyboard:Space; Gamepad:X");

	// Swap: jump takes the old event of fire's slot; the server signals come before completed.
	server->reset_action(FIRE);
	CHECK(events_text(server, FIRE) == "Mouse:Left");
	CHECK(rebinder->begin(FIRE, 0) == OK);
	CHECK(rebinder->update(make_key(Key::SPACE)));
	CHECK(rebinder->is_conflict_pending());
	recorder->log.clear();
	CHECK(rebinder->resolve_conflict_swap() == OK);
	CHECK(events_text(server, FIRE) == "Keyboard:Space");
	CHECK(events_text(server, JUMP) == "Mouse:Left; Gamepad:X");
	REQUIRE(recorder->log.size() == 3);
	CHECK(recorder->log[0].begins_with("changed:"));
	CHECK(recorder->log[1].begins_with("changed:"));
	CHECK(recorder->log[2] == vformat("completed:%s:0:Keyboard:Space", FIRE));

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] Cancel without an event changes nothing; two rebinders are independent") {
	TestSetup setup;
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	RebindRecorder *recorder = memnew(RebindRecorder);
	recorder->watch_server(server);
	Ref<InputActionRebinder> keyboard = make_rebinder(server);
	Ref<InputActionRebinder> pad = make_rebinder(server);
	recorder->watch(keyboard);
	recorder->watch(pad);
	keyboard->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);
	pad->set_device_filter(InputActionRebinder::DEVICE_GAMEPAD);

	// Cancel before any input.
	CHECK(keyboard->begin(JUMP, 0) == OK);
	keyboard->cancel();
	CHECK(recorder->last() == vformat("canceled:%s:0", JUMP));
	CHECK_FALSE(keyboard->is_active());
	CHECK(keyboard->get_action() == StringName());
	CHECK(keyboard->get_event_index() == -1);
	recorder->log.clear();
	keyboard->cancel(); // No session: no signal.
	CHECK(recorder->log.is_empty());
	CHECK(events_text(server, JUMP) == "KeyboardLabel:Space; Gamepad:X");

	// begin() during a session cancels it first.
	CHECK(keyboard->begin(JUMP, 0) == OK);
	CHECK(keyboard->begin(FIRE, 0) == OK);
	CHECK(recorder->log == PackedStringArray({ vformat("started:%s:0", JUMP), vformat("canceled:%s:0", JUMP), vformat("started:%s:0", FIRE) }));
	keyboard->cancel();

	// Two sessions at once: each takes only its own device class.
	CHECK(keyboard->begin(JUMP, 0) == OK);
	CHECK(pad->begin(JUMP, 0) == OK);
	CHECK(keyboard->get_action() == StringName(JUMP));
	CHECK(pad->get_event_index() == 0);
	CHECK_FALSE(keyboard->update(make_joy_button(JoyButton::Y, 0)));
	CHECK(pad->update(make_joy_button(JoyButton::Y, 0)));
	CHECK_FALSE(pad->is_active());
	CHECK(keyboard->is_active());
	CHECK_FALSE(pad->update(make_key(Key::W)));
	CHECK(keyboard->update(make_key(Key::W)));
	CHECK_FALSE(keyboard->is_active());
	CHECK(events_text(server, JUMP) == "Keyboard:W; Gamepad:Y");

	keyboard.unref();
	pad.unref();
	memdelete(server);
	memdelete(recorder);
}

TEST_CASE("[SceneTree][InputActionRebinder] Built-in actions count as conflicts; they can't be swapped in SAVE_MODE_CUSTOM_ONLY") {
	TestSetup setup;
	InputMap *input_map = InputMap::get_singleton();
	bool added = false;
	if (!input_map->has_action("ui_accept")) {
		input_map->add_action("ui_accept", 0.5f);
		input_map->action_add_event("ui_accept", make_project_key(Key::ENTER));
		added = true;
	}
	if (!input_map->is_builtin_input_action("ui_accept")) {
		// The test InputMap has no built-in actions: nothing to check.
		if (added) {
			input_map->erase_action("ui_accept");
		}
		return;
	}
	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	REQUIRE(server->get_save_mode() == InputMapSettingsServer::SAVE_MODE_CUSTOM_ONLY);
	RebindRecorder *recorder = memnew(RebindRecorder);
	Ref<InputActionRebinder> rebinder = make_rebinder(server, false);
	recorder->watch(rebinder);
	rebinder->set_device_filter(InputActionRebinder::DEVICE_KEYBOARD_MOUSE);
	rebinder->set_conflict_mode(InputActionRebinder::CONFLICT_SWAP);

	ERR_PRINT_OFF;
	CHECK(rebinder->begin("ui_accept", 0) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;

	CHECK(rebinder->begin(JUMP, 0) == OK);
	CHECK(rebinder->update(make_key(Key::ENTER)));
	bool accept_conflict = false;
	for (const String &entry : recorder->log) {
		accept_conflict = accept_conflict || entry.begins_with(vformat("conflict:%s:0:ui_accept:", JUMP));
	}
	CHECK(accept_conflict);
	CHECK(rebinder->is_active()); // Handled like CONFLICT_REJECT.
	CHECK(events_text(server, JUMP) == "KeyboardLabel:Space; Gamepad:X");
	rebinder->cancel();

	rebinder.unref();
	memdelete(server);
	memdelete(recorder);
	if (added) {
		input_map->erase_action("ui_accept");
	}
}

TEST_CASE("[SceneTree][InputActionRebinder] The committed event is the event a save and load gives back") {
	TestSetup setup;
	const String path = TestUtils::get_temp_path("input_action_rebinder").path_join("input_map.ini");
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	String committed_text;
	String stored_text;
	{
		InputMapSettingsServer *server = memnew(InputMapSettingsServer);
		server->set_file_path(path);
		RebindRecorder *recorder = memnew(RebindRecorder);
		Ref<InputActionRebinder> rebinder = make_rebinder(server);
		recorder->watch(rebinder);
		CHECK(rebinder->begin(JUMP, 0) == OK);
		Ref<InputEventKey> ctrl_w = make_key(Key::W);
		ctrl_w->set_ctrl_pressed(true);
		CHECK(rebinder->update(ctrl_w));
		CHECK(rebinder->begin(JUMP, 1) == OK);
		CHECK(rebinder->update(InputEventJoypadMotion::create_reference(JoyAxis::TRIGGER_RIGHT, 0.8f, 2)));
		committed_text = recorder->last();
		stored_text = events_text(server, JUMP);
		CHECK(stored_text == "Keyboard:Ctrl+W; GamepadAxis:RightTrigger");
		CHECK(server->save() == OK);
		rebinder.unref();
		memdelete(server);
		memdelete(recorder);
	}
	CHECK(committed_text == vformat("completed:%s:1:GamepadAxis:RightTrigger", JUMP));

	InputMapSettingsServer *server = memnew(InputMapSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(events_text(server, JUMP) == stored_text);
	CHECK_FALSE(server->is_dirty());
	Ref<InputEventKey> ctrl_w = make_key(Key::W);
	ctrl_w->set_ctrl_pressed(true);
	CHECK(is_action(ctrl_w, JUMP));
	memdelete(server);
}

} // namespace TestInputActionRebinder
