/**************************************************************************/
/*  input_icon_server.cpp                                                 */
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

#include "input_icon_server.h"

#include "core/config/project_settings.h"
#include "core/core_string_names.h"
#include "core/input/input.h"
#include "core/input/input_action.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"

InputIconServer *InputIconServer::singleton = nullptr;

static const char *_family_setting_names[] = { "keyboard_mouse", "generic_gamepad", "xbox", "playstation", "nintendo" };

InputIconServer *InputIconServer::get_singleton() {
	return singleton;
}

String InputIconServer::_get_setting_name(InputIconFamily p_family) {
	return String("input_devices/icons/") + _family_setting_names[p_family];
}

String InputIconServer::_get_setting_path(InputIconFamily p_family) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr) {
		return String();
	}
	const String setting = _get_setting_name(p_family);
	return project_settings->has_setting(setting) ? String(project_settings->get_setting_with_override(setting)).strip_edges() : String();
}

Ref<InputIconSet> InputIconServer::_get_set(InputIconFamily p_family) {
	// The setting is read without the lock (ProjectSettings has its own).
	const String path = _get_setting_path(p_family);
	{
		MutexLock lock(mutex);
		const Slot &slot = slots[p_family];
		if (slot.overridden || (slot.loaded && slot.loaded_path == path)) {
			return slot.set;
		}
	}
	// Loading may print errors: never with the lock held.
	Ref<InputIconSet> loaded;
	if (!path.is_empty()) {
		loaded = ResourceLoader::load(path, "InputIconSet");
	}
	Ref<InputIconSet> old_set;
	Ref<InputIconSet> result;
	{
		MutexLock lock(mutex);
		Slot &slot = slots[p_family];
		if (!slot.overridden) {
			old_set = slot.set;
			slot.set = loaded;
			slot.loaded = true;
			slot.loaded_path = path;
		}
		result = slot.set;
	}
	if (old_set != result) {
		_update_connections(old_set, result);
	}
	if (!path.is_empty() && loaded.is_null()) {
		WARN_PRINT(vformat("InputIconServer: \"%s\" is not an InputIconSet (project setting \"%s\").", path, _get_setting_name(p_family)));
	}
	return result;
}

void InputIconServer::_update_connections(const Ref<InputIconSet> &p_old, const Ref<InputIconSet> &p_new) {
	bool old_in_use = false;
	{
		MutexLock lock(mutex);
		for (const Slot &slot : slots) {
			if (p_old.is_valid() && slot.set == p_old) {
				old_in_use = true;
			}
		}
	}
	if (p_old.is_valid() && !old_in_use) {
		const Callable callable = callable_mp(this, &InputIconServer::_on_set_changed).bind(p_old->get_instance_id());
		if (p_old->is_connected(CoreStringName(changed), callable)) {
			p_old->disconnect(CoreStringName(changed), callable);
		}
	}
	if (p_new.is_valid()) {
		const Callable callable = callable_mp(this, &InputIconServer::_on_set_changed).bind(p_new->get_instance_id());
		if (!p_new->is_connected(CoreStringName(changed), callable)) {
			p_new->connect(CoreStringName(changed), callable);
		}
	}
}

void InputIconServer::_on_set_changed(ObjectID p_set_id) {
	LocalVector<InputIconFamily> families;
	{
		MutexLock lock(mutex);
		for (int i = 0; i < INPUT_ICON_FAMILY_MAX; i++) {
			if (slots[i].set.is_valid() && slots[i].set->get_instance_id() == p_set_id) {
				families.push_back(InputIconFamily(i));
			}
		}
	}
	for (const InputIconFamily family : families) {
		_emit_icon_set_changed(family);
	}
}

void InputIconServer::_on_settings_changed() {
	// Only marks the changed families; the sets load again on their next use.
	String paths[INPUT_ICON_FAMILY_MAX];
	for (int i = 0; i < INPUT_ICON_FAMILY_MAX; i++) {
		paths[i] = _get_setting_path(InputIconFamily(i));
	}
	LocalVector<InputIconFamily> families;
	{
		MutexLock lock(mutex);
		for (int i = 0; i < INPUT_ICON_FAMILY_MAX; i++) {
			Slot &slot = slots[i];
			if (!slot.overridden && slot.loaded && slot.loaded_path != paths[i]) {
				slot.loaded = false;
				families.push_back(InputIconFamily(i));
			}
		}
	}
	for (const InputIconFamily family : families) {
		_emit_icon_set_changed(family);
	}
}

void InputIconServer::_emit_icon_set_changed(InputIconFamily p_family) {
	if (Thread::is_main_thread()) {
		emit_signal(SNAME("icon_set_changed"), p_family);
	} else {
		callable_mp(this, &InputIconServer::_emit_icon_set_changed).call_deferred(p_family);
	}
}

void InputIconServer::set_icon_set(InputIconFamily p_family, const Ref<InputIconSet> &p_set) {
	ERR_FAIL_INDEX(int(p_family), int(INPUT_ICON_FAMILY_MAX));
	Ref<InputIconSet> old_set;
	{
		MutexLock lock(mutex);
		Slot &slot = slots[p_family];
		old_set = slot.set;
		slot.set = p_set;
		slot.overridden = true;
		slot.loaded = false;
		slot.loaded_path = String();
	}
	if (old_set != p_set) {
		_update_connections(old_set, p_set);
	}
	_emit_icon_set_changed(p_family);
}

Ref<InputIconSet> InputIconServer::get_icon_set(InputIconFamily p_family) {
	ERR_FAIL_INDEX_V(int(p_family), int(INPUT_ICON_FAMILY_MAX), Ref<InputIconSet>());
	return _get_set(p_family);
}

void InputIconServer::reset_icon_set(InputIconFamily p_family) {
	ERR_FAIL_INDEX(int(p_family), int(INPUT_ICON_FAMILY_MAX));
	Ref<InputIconSet> old_set;
	{
		MutexLock lock(mutex);
		Slot &slot = slots[p_family];
		old_set = slot.set;
		slot.set.unref();
		slot.overridden = false;
		slot.loaded = false;
		slot.loaded_path = String();
	}
	_update_connections(old_set, Ref<InputIconSet>());
	_emit_icon_set_changed(p_family);
}

bool InputIconServer::has_icon_set_override(InputIconFamily p_family) const {
	ERR_FAIL_INDEX_V(int(p_family), int(INPUT_ICON_FAMILY_MAX), false);
	MutexLock lock(mutex);
	return slots[p_family].overridden;
}

void InputIconServer::set_gamepad_family(InputIconFamily p_family) {
	ERR_FAIL_COND(int(p_family) < int(INPUT_ICON_FAMILY_AUTO) || int(p_family) >= int(INPUT_ICON_FAMILY_MAX));
	{
		MutexLock lock(mutex);
		if (gamepad_family == p_family) {
			return;
		}
		gamepad_family = p_family;
	}
	// All gamepad prompts may change.
	_emit_icon_set_changed(INPUT_ICON_FAMILY_GENERIC_GAMEPAD);
}

InputIconServer::InputIconFamily InputIconServer::get_gamepad_family() const {
	MutexLock lock(mutex);
	return gamepad_family;
}

InputIconServer::InputIconFamily InputIconServer::_resolve_family(const Ref<InputEvent> &p_event, InputIconFamily p_family) const {
	if (p_family != INPUT_ICON_FAMILY_AUTO) {
		return p_family;
	}
	return get_event_family(p_event);
}

InputIconServer::InputIconFamily InputIconServer::get_event_family(const Ref<InputEvent> &p_event) const {
	if (Ref<InputEventKey>(p_event).is_valid() || Ref<InputEventMouseButton>(p_event).is_valid()) {
		return INPUT_ICON_FAMILY_KEYBOARD_MOUSE;
	}
	const InputIconFamily forced = get_gamepad_family();
	if (forced != INPUT_ICON_FAMILY_AUTO) {
		return forced;
	}
	if (p_event.is_valid() && p_event->get_device() >= 0) {
		return get_device_family(p_event->get_device());
	}
	// All devices (the default of the Input Map): the first connected gamepad decides.
	Input *input = Input::get_singleton();
	if (input != nullptr) {
		const TypedArray<int> joypads = input->get_connected_joypads();
		if (!joypads.is_empty()) {
			return get_device_family(joypads[0]);
		}
	}
	return INPUT_ICON_FAMILY_GENERIC_GAMEPAD;
}

InputIconServer::InputIconFamily InputIconServer::get_device_family(int p_device) const {
	Input *input = Input::get_singleton();
	if (input == nullptr || p_device < 0 || !input->is_joy_known(p_device)) {
		return INPUT_ICON_FAMILY_GENERIC_GAMEPAD;
	}
	const Dictionary info = input->get_joy_info(p_device);
	const String vendor = info.get("vendor_id", String());
	return get_family_for_joy_name(input->get_joy_name(p_device), vendor.is_valid_int() ? int(vendor.to_int()) : -1);
}

InputIconServer::InputIconFamily InputIconServer::get_family_for_joy_name(const String &p_joy_name, int p_vendor_id) {
	struct Vendor {
		int id;
		const char *hex;
		InputIconFamily family;
	};
	static const Vendor vendors[] = {
		{ 0x054C, "054c", INPUT_ICON_FAMILY_PLAYSTATION },
		{ 0x057E, "057e", INPUT_ICON_FAMILY_NINTENDO },
		{ 0x045E, "045e", INPUT_ICON_FAMILY_XBOX },
	};
	const String joy_name = p_joy_name.to_lower();
	for (const Vendor &vendor : vendors) {
		// Browsers put the vendor id into Gamepad.id: `054c-0ce6-...` (Firefox), `Vendor: 054c` (Chromium).
		const String hex = vendor.hex;
		if (p_vendor_id == vendor.id || joy_name.begins_with(hex + "-") || joy_name.contains("vendor: " + hex) || joy_name.contains("vendor:" + hex)) {
			return vendor.family;
		}
	}
	struct Keyword {
		const char *text;
		InputIconFamily family;
	};
	static const Keyword keywords[] = {
		{ "playstation", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "dualshock", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "dualsense", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "ps3", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "ps4", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "ps5", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "sony", INPUT_ICON_FAMILY_PLAYSTATION },
		{ "nintendo", INPUT_ICON_FAMILY_NINTENDO },
		{ "switch", INPUT_ICON_FAMILY_NINTENDO },
		{ "joy-con", INPUT_ICON_FAMILY_NINTENDO },
		{ "joycon", INPUT_ICON_FAMILY_NINTENDO },
		{ "pro controller", INPUT_ICON_FAMILY_NINTENDO },
		{ "wii", INPUT_ICON_FAMILY_NINTENDO },
		{ "xbox", INPUT_ICON_FAMILY_XBOX },
		{ "x-box", INPUT_ICON_FAMILY_XBOX },
		{ "xinput", INPUT_ICON_FAMILY_XBOX },
		{ "microsoft", INPUT_ICON_FAMILY_XBOX },
	};
	for (const Keyword &keyword : keywords) {
		if (joy_name.contains(keyword.text)) {
			return keyword.family;
		}
	}
	return INPUT_ICON_FAMILY_GENERIC_GAMEPAD;
}

Ref<Texture2D> InputIconServer::get_icon(const Ref<InputEvent> &p_event, InputIconFamily p_family) {
	ERR_FAIL_COND_V(int(p_family) < int(INPUT_ICON_FAMILY_AUTO) || int(p_family) >= int(INPUT_ICON_FAMILY_MAX), Ref<Texture2D>());
	if (p_event.is_null()) {
		return Ref<Texture2D>();
	}
	const InputIconFamily family = _resolve_family(p_event, p_family);
	// Gamepad families share the icons of GENERIC_GAMEPAD.
	InputIconFamily chain[2] = { family, INPUT_ICON_FAMILY_MAX };
	int chain_size = 1;
	if (family == INPUT_ICON_FAMILY_XBOX || family == INPUT_ICON_FAMILY_PLAYSTATION || family == INPUT_ICON_FAMILY_NINTENDO) {
		chain[chain_size++] = INPUT_ICON_FAMILY_GENERIC_GAMEPAD;
	}
	Ref<InputIconSet> sets[2];
	for (int i = 0; i < chain_size; i++) {
		sets[i] = _get_set(chain[i]);
		if (sets[i].is_valid()) {
			const Ref<Texture2D> icon = sets[i]->get_icon(p_event, false);
			if (icon.is_valid()) {
				return icon;
			}
		}
	}
	for (int i = 0; i < chain_size; i++) {
		if (sets[i].is_valid()) {
			const Ref<Texture2D> icon = sets[i]->get_fallback_icon(p_event);
			if (icon.is_valid()) {
				return icon;
			}
		}
	}
	return Ref<Texture2D>();
}

Ref<Texture2D> InputIconServer::get_icon_for_text(const String &p_text, InputIconFamily p_family) {
	return get_icon(InputAction::text_to_event(p_text), p_family);
}

void InputIconServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_icon_set", "family", "set"), &InputIconServer::set_icon_set);
	ClassDB::bind_method(D_METHOD("get_icon_set", "family"), &InputIconServer::get_icon_set);
	ClassDB::bind_method(D_METHOD("reset_icon_set", "family"), &InputIconServer::reset_icon_set);
	ClassDB::bind_method(D_METHOD("has_icon_set_override", "family"), &InputIconServer::has_icon_set_override);

	ClassDB::bind_method(D_METHOD("set_gamepad_family", "family"), &InputIconServer::set_gamepad_family);
	ClassDB::bind_method(D_METHOD("get_gamepad_family"), &InputIconServer::get_gamepad_family);

	ClassDB::bind_method(D_METHOD("get_icon", "event", "family"), &InputIconServer::get_icon, DEFVAL(INPUT_ICON_FAMILY_AUTO));
	ClassDB::bind_method(D_METHOD("get_icon_for_text", "text", "family"), &InputIconServer::get_icon_for_text, DEFVAL(INPUT_ICON_FAMILY_AUTO));
	ClassDB::bind_method(D_METHOD("get_event_family", "event"), &InputIconServer::get_event_family);
	ClassDB::bind_method(D_METHOD("get_device_family", "device"), &InputIconServer::get_device_family);
	ClassDB::bind_static_method("InputIconServer", D_METHOD("get_family_for_joy_name", "joy_name", "vendor_id"), &InputIconServer::get_family_for_joy_name, DEFVAL(-1));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "gamepad_family", PROPERTY_HINT_ENUM, "Auto:-1,Keyboard Mouse,Generic Gamepad,Xbox,PlayStation,Nintendo"), "set_gamepad_family", "get_gamepad_family");

	ADD_SIGNAL(MethodInfo("icon_set_changed", PropertyInfo(Variant::INT, "family", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_CLASS_IS_ENUM, "InputIconServer.InputIconFamily")));

	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_AUTO);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_KEYBOARD_MOUSE);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_GENERIC_GAMEPAD);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_XBOX);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_PLAYSTATION);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_NINTENDO);
	BIND_ENUM_CONSTANT(INPUT_ICON_FAMILY_MAX);
}

InputIconServer::InputIconServer() {
	singleton = this;
	for (int i = 0; i < INPUT_ICON_FAMILY_MAX; i++) {
		GLOBAL_DEF(PropertyInfo(Variant::STRING, _get_setting_name(InputIconFamily(i)), PROPERTY_HINT_FILE, "*.tres,*.res"), "");
	}
	if (ProjectSettings::get_singleton() != nullptr) {
		ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &InputIconServer::_on_settings_changed));
	}
}

InputIconServer::~InputIconServer() {
	if (ProjectSettings::get_singleton() != nullptr) {
		ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &InputIconServer::_on_settings_changed));
	}
	for (int i = 0; i < INPUT_ICON_FAMILY_MAX; i++) {
		const Ref<InputIconSet> old_set = slots[i].set;
		slots[i].set.unref();
		_update_connections(old_set, Ref<InputIconSet>());
	}
	singleton = nullptr;
}
