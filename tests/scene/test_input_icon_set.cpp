/**************************************************************************/
/*  test_input_icon_set.cpp                                               */
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

TEST_FORCE_LINK(test_input_icon_set)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input_action.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/main/input_icon_server.h"
#include "scene/resources/input_icon_set.h"
#include "scene/resources/placeholder_textures.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestInputIconSet {

// A texture without RenderingServer; the size tells the icons apart.
static Ref<Texture2D> make_icon(int p_size) {
	Ref<PlaceholderTexture2D> icon;
	icon.instantiate();
	icon->set_size(Size2(p_size, p_size));
	return icon;
}

static Ref<InputEventKey> make_key(Key p_physical_keycode) {
	Ref<InputEventKey> key;
	key.instantiate();
	key->set_physical_keycode(p_physical_keycode);
	return key;
}

static Ref<InputEventMouseButton> make_mouse_button(MouseButton p_button) {
	Ref<InputEventMouseButton> button;
	button.instantiate();
	button->set_button_index(p_button);
	return button;
}

// Restores the server after a test case, also when a REQUIRE fails.
struct ServerReset {
	~ServerReset() {
		InputIconServer *server = InputIconServer::get_singleton();
		if (server) {
			for (int i = 0; i < InputIconServer::INPUT_ICON_FAMILY_MAX; i++) {
				server->reset_icon_set(InputIconServer::InputIconFamily(i));
			}
			server->set_gamepad_family(InputIconServer::INPUT_ICON_FAMILY_AUTO);
		}
	}
};

TEST_CASE("[InputIconSet] Lookup per key, mouse button and gamepad button") {
	Ref<InputIconSet> set;
	set.instantiate();
	const Ref<Texture2D> space = make_icon(1);
	const Ref<Texture2D> left = make_icon(2);
	const Ref<Texture2D> a = make_icon(3);
	set->set_key_icon(Key::SPACE, space);
	set->set_mouse_button_icon(MouseButton::LEFT, left);
	set->set_joy_button_icon(JoyButton::A, a);

	CHECK(set->get_key_icon(Key::SPACE) == space);
	CHECK(set->get_mouse_button_icon(MouseButton::LEFT) == left);
	CHECK(set->get_joy_button_icon(JoyButton::A) == a);
	CHECK(set->get_joy_button_icon(JoyButton::B).is_null());

	CHECK(set->get_icon(make_key(Key::SPACE)) == space);
	CHECK(set->get_icon(make_mouse_button(MouseButton::LEFT)) == left);
	CHECK(set->get_icon(InputEventJoypadButton::create_reference(JoyButton::A, 2)) == a);
	CHECK(set->has_icon(make_key(Key::SPACE)));
	CHECK_FALSE(set->has_icon(make_key(Key::ENTER)));

	// Modifiers and the kind of key code are ignored.
	Ref<InputEventKey> ctrl_space = make_key(Key::SPACE);
	ctrl_space->set_ctrl_pressed(true);
	CHECK(set->get_icon(ctrl_space) == space);
	Ref<InputEventKey> label_space;
	label_space.instantiate();
	label_space->set_keycode(Key::SPACE);
	CHECK(set->get_icon(label_space) == space);
	CHECK(set->get_key_icon(Key::SPACE | KeyModifierMask::SHIFT) == space);

	// Null removes an icon.
	set->set_key_icon(Key::SPACE, Ref<Texture2D>());
	CHECK(set->get_key_icon(Key::SPACE).is_null());

	// Events without an icon.
	Ref<InputEventMouseMotion> motion;
	motion.instantiate();
	CHECK(set->get_icon(motion).is_null());
	CHECK(set->get_icon(Ref<InputEvent>()).is_null());
}

TEST_CASE("[InputIconSet] Axis directions") {
	Ref<InputIconSet> set;
	set.instantiate();
	const Ref<Texture2D> stick_left = make_icon(1);
	const Ref<Texture2D> stick_right = make_icon(2);
	const Ref<Texture2D> trigger = make_icon(3);
	set->set_joy_axis_icon(JoyAxis::LEFT_X, -1.0f, stick_left);
	set->set_joy_axis_icon(JoyAxis::LEFT_X, 0.5f, stick_right);
	set->set_joy_axis_icon(JoyAxis::TRIGGER_RIGHT, 1.0f, trigger);

	CHECK(set->get_joy_axis_icon(JoyAxis::LEFT_X, -0.3f) == stick_left);
	CHECK(set->get_joy_axis_icon(JoyAxis::LEFT_X, 1.0f) == stick_right);
	CHECK(set->get_joy_axis_icon(JoyAxis::LEFT_Y, -1.0f).is_null());
	CHECK(set->get_icon(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, -1.0f)) == stick_left);
	CHECK(set->get_icon(InputEventJoypadMotion::create_reference(JoyAxis::LEFT_X, 1.0f)) == stick_right);
	CHECK(set->get_icon(InputEventJoypadMotion::create_reference(JoyAxis::TRIGGER_RIGHT, 1.0f)) == trigger);

	const Dictionary axes = set->get_joy_axis_icons();
	CHECK(axes.size() == 3);
	CHECK(Ref<Texture2D>(axes["LeftStickLeft"]) == stick_left);
	CHECK(Ref<Texture2D>(axes["LeftStickRight"]) == stick_right);
	CHECK(Ref<Texture2D>(axes["RightTrigger"]) == trigger);
}

TEST_CASE("[InputIconSet] Fallback icons per event type") {
	Ref<InputIconSet> set;
	set.instantiate();
	const Ref<Texture2D> specific = make_icon(1);
	const Ref<Texture2D> key_fallback = make_icon(2);
	const Ref<Texture2D> mouse_fallback = make_icon(3);
	const Ref<Texture2D> button_fallback = make_icon(4);
	const Ref<Texture2D> axis_fallback = make_icon(5);
	set->set_key_icon(Key::A, specific);
	set->set_fallback_key_icon(key_fallback);
	set->set_fallback_mouse_button_icon(mouse_fallback);
	set->set_fallback_joy_button_icon(button_fallback);
	set->set_fallback_joy_axis_icon(axis_fallback);

	CHECK(set->get_icon(make_key(Key::A)) == specific);
	CHECK(set->get_icon(make_key(Key::B)) == key_fallback);
	CHECK(set->get_icon(make_key(Key::B), false).is_null());
	CHECK(set->get_icon(make_mouse_button(MouseButton::RIGHT)) == mouse_fallback);
	CHECK(set->get_icon(InputEventJoypadButton::create_reference(JoyButton::X)) == button_fallback);
	CHECK(set->get_icon(InputEventJoypadMotion::create_reference(JoyAxis::RIGHT_Y, 1.0f)) == axis_fallback);
	CHECK(set->get_fallback_icon(make_key(Key::A)) == key_fallback);

	set->clear();
	CHECK(set->get_icon(make_key(Key::A)).is_null());
	CHECK(set->get_fallback_key_icon().is_null());
}

TEST_CASE("[InputIconSet] Text form, dictionaries and Inspector properties") {
	Ref<InputIconSet> set;
	set.instantiate();
	const Ref<Texture2D> a = make_icon(1);
	const Ref<Texture2D> press = make_icon(2);
	const Ref<Texture2D> space = make_icon(3);
	const Ref<Texture2D> wheel = make_icon(4);
	const Ref<Texture2D> up = make_icon(5);

	Dictionary buttons;
	buttons["A"] = a;
	buttons[int(JoyButton::LEFT_STICK)] = press;
	set->set_joy_button_icons(buttons);
	CHECK(set->get_joy_button_icon(JoyButton::A) == a);
	CHECK(set->get_joy_button_icon(JoyButton::LEFT_STICK) == press);
	CHECK(set->get_icon_for_text("Gamepad:A") == a);
	CHECK(set->get_icon_for_text("Device3:Gamepad:LeftStickPress") == press);
	CHECK(set->get_icon_for_text("not an event").is_null());

	// Saved with the text names of InputAction.
	const Dictionary saved = set->get_joy_button_icons();
	CHECK(saved.has("A"));
	CHECK(saved.has("LeftStickPress"));

	ERR_PRINT_OFF;
	Dictionary keys;
	keys["Space"] = space;
	keys["NoSuchKey"] = a;
	set->set_key_icons(keys);
	ERR_PRINT_ON;
	CHECK(set->get_key_icon(Key::SPACE) == space);
	CHECK(set->get_key_icons().size() == 1);
	CHECK(set->get_icon_for_text("KeyboardLabel:Ctrl+Space") == space);

	// One Inspector property per value.
	bool valid = false;
	set->set("mouse/WheelUp", wheel, &valid);
	CHECK(valid);
	CHECK(set->get_mouse_button_icon(MouseButton::WHEEL_UP) == wheel);
	valid = false;
	set->set("gamepad_axes/LeftStickUp", up, &valid);
	CHECK(valid);
	CHECK(set->get_joy_axis_icon(JoyAxis::LEFT_Y, -1.0f) == up);
	CHECK(Ref<Texture2D>(set->get("keyboard/Space")) == space);

	List<PropertyInfo> properties;
	set->get_property_list(&properties);
	bool found_key = false;
	bool found_button = false;
	bool found_axis = false;
	for (const PropertyInfo &property : properties) {
		found_key = found_key || property.name == "keyboard/Space";
		found_button = found_button || property.name == "gamepad_buttons/LeftStickPress";
		found_axis = found_axis || property.name == "gamepad_axes/RightTrigger";
	}
	CHECK(found_key);
	CHECK(found_button);
	CHECK(found_axis);
	CHECK(set->property_can_revert("keyboard/Space"));
}

TEST_CASE("[InputIconSet] Saves and loads as .tres") {
	Ref<InputIconSet> set;
	set.instantiate();
	set->set_key_icon(Key::ESCAPE, make_icon(7));
	set->set_joy_axis_icon(JoyAxis::RIGHT_X, 1.0f, make_icon(8));
	set->set_fallback_joy_button_icon(make_icon(9));

	const String path = TestUtils::get_temp_path("input_icon_set.tres");
	REQUIRE(ResourceSaver::save(set, path) == OK);
	Ref<InputIconSet> loaded = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	REQUIRE(loaded->get_key_icon(Key::ESCAPE).is_valid());
	CHECK(loaded->get_key_icon(Key::ESCAPE)->get_size() == Size2(7, 7));
	REQUIRE(loaded->get_joy_axis_icon(JoyAxis::RIGHT_X, 1.0f).is_valid());
	CHECK(loaded->get_joy_axis_icon(JoyAxis::RIGHT_X, 1.0f)->get_size() == Size2(8, 8));
	REQUIRE(loaded->get_fallback_joy_button_icon().is_valid());
	CHECK(loaded->get_fallback_joy_button_icon()->get_size() == Size2(9, 9));
	CHECK(loaded->get_mouse_button_icons().is_empty());
}

TEST_CASE("[InputIconServer] Singleton and family of joypad names") {
	REQUIRE(InputIconServer::get_singleton() != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("InputIconServer"));

	CHECK(InputIconServer::get_family_for_joy_name("Xbox Series Controller") == InputIconServer::INPUT_ICON_FAMILY_XBOX);
	CHECK(InputIconServer::get_family_for_joy_name("XInput Controller") == InputIconServer::INPUT_ICON_FAMILY_XBOX);
	CHECK(InputIconServer::get_family_for_joy_name("PS5 Controller") == InputIconServer::INPUT_ICON_FAMILY_PLAYSTATION);
	CHECK(InputIconServer::get_family_for_joy_name("DualSense Wireless Controller") == InputIconServer::INPUT_ICON_FAMILY_PLAYSTATION);
	CHECK(InputIconServer::get_family_for_joy_name("Nintendo Switch Pro Controller") == InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	CHECK(InputIconServer::get_family_for_joy_name("Wireless Controller", 0x054C) == InputIconServer::INPUT_ICON_FAMILY_PLAYSTATION);
	// Gamepad.id of the browsers.
	CHECK(InputIconServer::get_family_for_joy_name("054c-0ce6-Wireless Controller") == InputIconServer::INPUT_ICON_FAMILY_PLAYSTATION);
	CHECK(InputIconServer::get_family_for_joy_name("Wireless Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)") == InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	CHECK(InputIconServer::get_family_for_joy_name("Generic USB Joystick") == InputIconServer::INPUT_ICON_FAMILY_GENERIC_GAMEPAD);
	CHECK(InputIconServer::get_singleton()->get_device_family(-1) == InputIconServer::INPUT_ICON_FAMILY_GENERIC_GAMEPAD);
}

TEST_CASE("[InputIconServer] Family selection and fallback order") {
	ServerReset reset;
	InputIconServer *server = InputIconServer::get_singleton();
	REQUIRE(server != nullptr);

	Ref<InputIconSet> keyboard_mouse;
	keyboard_mouse.instantiate();
	Ref<InputIconSet> generic;
	generic.instantiate();
	Ref<InputIconSet> xbox;
	xbox.instantiate();
	const Ref<Texture2D> key_w = make_icon(1);
	const Ref<Texture2D> generic_a = make_icon(2);
	const Ref<Texture2D> generic_b = make_icon(3);
	const Ref<Texture2D> xbox_a = make_icon(4);
	const Ref<Texture2D> xbox_fallback = make_icon(5);
	const Ref<Texture2D> generic_fallback = make_icon(6);
	keyboard_mouse->set_key_icon(Key::W, key_w);
	generic->set_joy_button_icon(JoyButton::A, generic_a);
	generic->set_joy_button_icon(JoyButton::B, generic_b);
	generic->set_fallback_joy_button_icon(generic_fallback);
	xbox->set_joy_button_icon(JoyButton::A, xbox_a);
	xbox->set_fallback_joy_button_icon(xbox_fallback);

	server->set_icon_set(InputIconServer::INPUT_ICON_FAMILY_KEYBOARD_MOUSE, keyboard_mouse);
	server->set_icon_set(InputIconServer::INPUT_ICON_FAMILY_GENERIC_GAMEPAD, generic);
	server->set_icon_set(InputIconServer::INPUT_ICON_FAMILY_XBOX, xbox);
	CHECK(server->get_icon_set(InputIconServer::INPUT_ICON_FAMILY_XBOX) == xbox);
	CHECK(server->has_icon_set_override(InputIconServer::INPUT_ICON_FAMILY_XBOX));

	// Keys use KEYBOARD_MOUSE.
	CHECK(server->get_event_family(make_key(Key::W)) == InputIconServer::INPUT_ICON_FAMILY_KEYBOARD_MOUSE);
	CHECK(server->get_icon(make_key(Key::W)) == key_w);
	CHECK(server->get_icon_for_text("Keyboard:W") == key_w);

	// No connected gamepad: GENERIC_GAMEPAD.
	CHECK(server->get_icon(InputEventJoypadButton::create_reference(JoyButton::A)) == generic_a);

	// Explicit family: specific icon of the family, of GENERIC_GAMEPAD, then the fallbacks in the same order.
	const Ref<InputEvent> button_a = InputEventJoypadButton::create_reference(JoyButton::A);
	const Ref<InputEvent> button_b = InputEventJoypadButton::create_reference(JoyButton::B);
	const Ref<InputEvent> button_x = InputEventJoypadButton::create_reference(JoyButton::X);
	CHECK(server->get_icon(button_a, InputIconServer::INPUT_ICON_FAMILY_XBOX) == xbox_a);
	CHECK(server->get_icon(button_b, InputIconServer::INPUT_ICON_FAMILY_XBOX) == generic_b);
	CHECK(server->get_icon(button_x, InputIconServer::INPUT_ICON_FAMILY_XBOX) == xbox_fallback);
	CHECK(server->get_icon(button_x, InputIconServer::INPUT_ICON_FAMILY_PLAYSTATION) == generic_fallback);
	CHECK(server->get_icon_for_text("Gamepad:A", InputIconServer::INPUT_ICON_FAMILY_XBOX) == xbox_a);

	// gamepad_family forces a family for all gamepad events.
	server->set_gamepad_family(InputIconServer::INPUT_ICON_FAMILY_XBOX);
	CHECK(server->get_event_family(button_a) == InputIconServer::INPUT_ICON_FAMILY_XBOX);
	CHECK(server->get_icon(button_a) == xbox_a);
	CHECK(server->get_event_family(make_key(Key::W)) == InputIconServer::INPUT_ICON_FAMILY_KEYBOARD_MOUSE);
	server->set_gamepad_family(InputIconServer::INPUT_ICON_FAMILY_AUTO);

	// Without any set: null.
	server->set_icon_set(InputIconServer::INPUT_ICON_FAMILY_GENERIC_GAMEPAD, Ref<InputIconSet>());
	CHECK(server->get_icon(button_b, InputIconServer::INPUT_ICON_FAMILY_GENERIC_GAMEPAD).is_null());
	CHECK(server->get_icon(Ref<InputEvent>()).is_null());
}

TEST_CASE("[InputIconServer] Signal and project setting") {
	ServerReset reset;
	InputIconServer *server = InputIconServer::get_singleton();
	REQUIRE(server != nullptr);

	Ref<InputIconSet> set;
	set.instantiate();
	SIGNAL_WATCH(server, "icon_set_changed");
	server->set_icon_set(InputIconServer::INPUT_ICON_FAMILY_NINTENDO, set);
	Array args;
	args.push_back(InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	Array calls;
	calls.push_back(args);
	SIGNAL_CHECK("icon_set_changed", calls);

	// Editing the set (the Inspector) emits the signal too.
	set->set_joy_button_icon(JoyButton::Y, make_icon(1));
	SIGNAL_CHECK("icon_set_changed", calls);

	server->reset_icon_set(InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	SIGNAL_CHECK("icon_set_changed", calls);
	CHECK_FALSE(server->has_icon_set_override(InputIconServer::INPUT_ICON_FAMILY_NINTENDO));
	set->set_joy_button_icon(JoyButton::Y, make_icon(2));
	SIGNAL_CHECK_FALSE("icon_set_changed");
	SIGNAL_UNWATCH(server, "icon_set_changed");

	// The project setting gives the set when there is no runtime set.
	CHECK(ProjectSettings::get_singleton()->has_setting("input_devices/icons/nintendo"));
	const String path = TestUtils::get_temp_path("input_icon_set_nintendo.tres");
	Ref<InputIconSet> saved;
	saved.instantiate();
	saved->set_joy_button_icon(JoyButton::B, make_icon(5));
	REQUIRE(ResourceSaver::save(saved, path) == OK);
	ProjectSettings::get_singleton()->set_setting("input_devices/icons/nintendo", path);
	const Ref<InputIconSet> loaded = server->get_icon_set(InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	REQUIRE(loaded.is_valid());
	const Ref<Texture2D> icon = server->get_icon_for_text("Gamepad:B", InputIconServer::INPUT_ICON_FAMILY_NINTENDO);
	REQUIRE(icon.is_valid());
	CHECK(icon->get_size() == Size2(5, 5));
	ProjectSettings::get_singleton()->set_setting("input_devices/icons/nintendo", "");
	CHECK(server->get_icon_set(InputIconServer::INPUT_ICON_FAMILY_NINTENDO).is_null());
}

} // namespace TestInputIconSet
