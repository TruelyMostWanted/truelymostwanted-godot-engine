/**************************************************************************/
/*  input_icon_set.cpp                                                    */
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

#include "input_icon_set.h"

#include "core/input/input_action.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "core/templates/hash_set.h"

// Type prefix of the text form (#193) and Inspector prefix per category.
static const char *_text_types[] = { "Keyboard", "Mouse", "Gamepad", "GamepadAxis" };
static const char *_property_prefixes[] = { "keyboard/", "mouse/", "gamepad_buttons/", "gamepad_axes/" };
static constexpr int MOUSE_BUTTON_COUNT = 9; // MouseButton::LEFT (1) to MouseButton::MB_XBUTTON2 (9).

bool InputIconSet::_event_to_index(const Ref<InputEvent> &p_event, Category &r_category, int &r_index) {
	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid()) {
		// The code of the text form: physical keycode, else keycode, else key label.
		Key code = key_event->get_physical_keycode();
		if (code == Key::NONE) {
			code = key_event->get_keycode();
		}
		if (code == Key::NONE) {
			code = key_event->get_key_label();
		}
		code = code & KeyModifierMask::CODE_MASK;
		if (code == Key::NONE) {
			return false;
		}
		r_category = CATEGORY_KEY;
		r_index = int(code);
		return true;
	}
	const Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_valid()) {
		if (mouse_event->get_button_index() == MouseButton::NONE) {
			return false;
		}
		r_category = CATEGORY_MOUSE_BUTTON;
		r_index = int(mouse_event->get_button_index());
		return true;
	}
	const Ref<InputEventJoypadButton> button_event = p_event;
	if (button_event.is_valid()) {
		if (button_event->get_button_index() == JoyButton::INVALID) {
			return false;
		}
		r_category = CATEGORY_JOY_BUTTON;
		r_index = int(button_event->get_button_index());
		return true;
	}
	const Ref<InputEventJoypadMotion> motion_event = p_event;
	if (motion_event.is_valid()) {
		if (motion_event->get_axis() == JoyAxis::INVALID) {
			return false;
		}
		r_category = CATEGORY_JOY_AXIS;
		r_index = _axis_index(motion_event->get_axis(), motion_event->get_axis_value());
		return true;
	}
	return false;
}

String InputIconSet::_index_to_name(Category p_category, int p_index) {
	Ref<InputEvent> event;
	switch (p_category) {
		case CATEGORY_KEY: {
			Ref<InputEventKey> key_event;
			key_event.instantiate();
			key_event->set_physical_keycode(Key(p_index));
			event = key_event;
		} break;
		case CATEGORY_MOUSE_BUTTON: {
			Ref<InputEventMouseButton> mouse_event;
			mouse_event.instantiate();
			mouse_event->set_button_index(MouseButton(p_index));
			event = mouse_event;
		} break;
		case CATEGORY_JOY_BUTTON: {
			event = InputEventJoypadButton::create_reference(JoyButton(p_index), -1);
		} break;
		case CATEGORY_JOY_AXIS: {
			event = InputEventJoypadMotion::create_reference(JoyAxis(p_index / 2), (p_index % 2) == 1 ? 1.0f : -1.0f, -1);
		} break;
		default: {
			return String();
		}
	}
	// `Keyboard:Space` -> `Space`.
	const String text = InputAction::event_to_text(event);
	const int colon = text.find_char(':');
	return colon >= 0 ? text.substr(colon + 1) : String();
}

int InputIconSet::_name_to_index(Category p_category, const String &p_name) {
	if (p_category < 0 || p_category >= CATEGORY_MAX || p_name.strip_edges().is_empty()) {
		return -1;
	}
	Category category = CATEGORY_MAX;
	int index = -1;
	const Ref<InputEvent> event = InputAction::text_to_event(String(_text_types[p_category]) + ":" + p_name.strip_edges());
	if (!_event_to_index(event, category, index) || category != p_category) {
		return -1;
	}
	return index;
}

HashMap<int, Ref<Texture2D>> &InputIconSet::_get_map(Category p_category) {
	switch (p_category) {
		case CATEGORY_MOUSE_BUTTON:
			return mouse_button_icons;
		case CATEGORY_JOY_BUTTON:
			return joy_button_icons;
		case CATEGORY_JOY_AXIS:
			return joy_axis_icons;
		default:
			return key_icons;
	}
}

const HashMap<int, Ref<Texture2D>> &InputIconSet::_get_map(Category p_category) const {
	return const_cast<InputIconSet *>(this)->_get_map(p_category);
}

void InputIconSet::_set_icon(Category p_category, int p_index, const Ref<Texture2D> &p_icon) {
	HashMap<int, Ref<Texture2D>> &map = _get_map(p_category);
	if (p_icon.is_null()) {
		if (!map.erase(p_index)) {
			return;
		}
	} else {
		const HashMap<int, Ref<Texture2D>>::Iterator existing = map.find(p_index);
		if (existing && existing->value == p_icon) {
			return;
		}
		map[p_index] = p_icon;
	}
	emit_changed();
}

Ref<Texture2D> InputIconSet::_get_icon(Category p_category, int p_index) const {
	const HashMap<int, Ref<Texture2D>> &map = _get_map(p_category);
	const HashMap<int, Ref<Texture2D>>::ConstIterator found = map.find(p_index);
	return found ? found->value : Ref<Texture2D>();
}

Dictionary InputIconSet::_get_icon_dictionary(Category p_category) const {
	// Sorted by index, so saved files do not change with the hash order.
	const HashMap<int, Ref<Texture2D>> &map = _get_map(p_category);
	LocalVector<int> indices;
	for (const KeyValue<int, Ref<Texture2D>> &entry : map) {
		indices.push_back(entry.key);
	}
	indices.sort();
	Dictionary icons;
	for (const int index : indices) {
		String icon_name = _index_to_name(p_category, index);
		if (icon_name.is_empty()) {
			continue;
		}
		icons[icon_name] = map.get(index);
	}
	return icons;
}

void InputIconSet::_set_icon_dictionary(Category p_category, const Dictionary &p_icons) {
	HashMap<int, Ref<Texture2D>> &map = _get_map(p_category);
	map.clear();
	PackedStringArray unknown;
	for (const KeyValue<Variant, Variant> &entry : p_icons) {
		const Ref<Texture2D> icon = entry.value;
		if (icon.is_null()) {
			continue;
		}
		int index = -1;
		if (entry.key.get_type() == Variant::INT && p_category != CATEGORY_JOY_AXIS) {
			index = int(entry.key);
		} else {
			index = _name_to_index(p_category, String(entry.key));
		}
		if (index < 0) {
			unknown.push_back(String(entry.key));
			continue;
		}
		map[index] = icon;
	}
	emit_changed();
	ERR_FAIL_COND_MSG(!unknown.is_empty(), vformat("InputIconSet: unknown input names ignored: %s.", String(", ").join(unknown)));
}

bool InputIconSet::_property_to_index(const StringName &p_property, Category &r_category, int &r_index) {
	const String property = p_property;
	for (int i = 0; i < CATEGORY_MAX; i++) {
		if (property.begins_with(_property_prefixes[i])) {
			const int index = _name_to_index(Category(i), property.substr(String(_property_prefixes[i]).length()));
			if (index < 0) {
				return false;
			}
			r_category = Category(i);
			r_index = index;
			return true;
		}
	}
	return false;
}

bool InputIconSet::_set(const StringName &p_name, const Variant &p_value) {
	Category category = CATEGORY_MAX;
	int index = -1;
	if (!_property_to_index(p_name, category, index)) {
		return false;
	}
	_set_icon(category, index, Ref<Texture2D>(p_value));
	return true;
}

bool InputIconSet::_get(const StringName &p_name, Variant &r_ret) const {
	Category category = CATEGORY_MAX;
	int index = -1;
	if (!_property_to_index(p_name, category, index)) {
		return false;
	}
	r_ret = _get_icon(category, index);
	return true;
}

void InputIconSet::_get_property_list(List<PropertyInfo> *p_list) const {
	// Editor only: the values are saved in the dictionaries.
	const uint32_t usage = PROPERTY_USAGE_EDITOR;
	HashSet<int> keys_found;
	for (int i = 0; i < keycode_get_count(); i++) {
		const int code = keycode_get_value_by_index(i);
		if (code == int(Key::NONE) || keys_found.has(code)) {
			continue;
		}
		keys_found.insert(code);
		p_list->push_back(PropertyInfo(Variant::OBJECT, String(_property_prefixes[CATEGORY_KEY]) + _index_to_name(CATEGORY_KEY, code), PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", usage));
	}
	for (int i = 1; i <= MOUSE_BUTTON_COUNT; i++) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, String(_property_prefixes[CATEGORY_MOUSE_BUTTON]) + _index_to_name(CATEGORY_MOUSE_BUTTON, i), PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", usage));
	}
	for (int i = 0; i < int(JoyButton::SDL_MAX); i++) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, String(_property_prefixes[CATEGORY_JOY_BUTTON]) + _index_to_name(CATEGORY_JOY_BUTTON, i), PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", usage));
	}
	for (int i = 0; i < int(JoyAxis::SDL_MAX) * 2; i++) {
		const JoyAxis axis = JoyAxis(i / 2);
		if ((axis == JoyAxis::TRIGGER_LEFT || axis == JoyAxis::TRIGGER_RIGHT) && (i % 2) == 0) {
			continue; // Triggers only have the positive direction.
		}
		p_list->push_back(PropertyInfo(Variant::OBJECT, String(_property_prefixes[CATEGORY_JOY_AXIS]) + _index_to_name(CATEGORY_JOY_AXIS, i), PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", usage));
	}
}

bool InputIconSet::_property_can_revert(const StringName &p_name) const {
	Category category = CATEGORY_MAX;
	int index = -1;
	return _property_to_index(p_name, category, index);
}

bool InputIconSet::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	Category category = CATEGORY_MAX;
	int index = -1;
	if (!_property_to_index(p_name, category, index)) {
		return false;
	}
	r_property = Ref<Texture2D>();
	return true;
}

void InputIconSet::set_key_icon(Key p_key, const Ref<Texture2D> &p_icon) {
	const Key code = p_key & KeyModifierMask::CODE_MASK;
	ERR_FAIL_COND_MSG(code == Key::NONE, "InputIconSet: the key must not be KEY_NONE.");
	_set_icon(CATEGORY_KEY, int(code), p_icon);
}

Ref<Texture2D> InputIconSet::get_key_icon(Key p_key) const {
	return _get_icon(CATEGORY_KEY, int(p_key & KeyModifierMask::CODE_MASK));
}

void InputIconSet::set_mouse_button_icon(MouseButton p_button, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND_MSG(p_button == MouseButton::NONE, "InputIconSet: the mouse button must not be MOUSE_BUTTON_NONE.");
	_set_icon(CATEGORY_MOUSE_BUTTON, int(p_button), p_icon);
}

Ref<Texture2D> InputIconSet::get_mouse_button_icon(MouseButton p_button) const {
	return _get_icon(CATEGORY_MOUSE_BUTTON, int(p_button));
}

void InputIconSet::set_joy_button_icon(JoyButton p_button, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND_MSG(int(p_button) < 0 || p_button >= JoyButton::MAX, "InputIconSet: invalid joypad button.");
	_set_icon(CATEGORY_JOY_BUTTON, int(p_button), p_icon);
}

Ref<Texture2D> InputIconSet::get_joy_button_icon(JoyButton p_button) const {
	return _get_icon(CATEGORY_JOY_BUTTON, int(p_button));
}

void InputIconSet::set_joy_axis_icon(JoyAxis p_axis, float p_direction, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND_MSG(int(p_axis) < 0 || p_axis >= JoyAxis::MAX, "InputIconSet: invalid joypad axis.");
	_set_icon(CATEGORY_JOY_AXIS, _axis_index(p_axis, p_direction), p_icon);
}

Ref<Texture2D> InputIconSet::get_joy_axis_icon(JoyAxis p_axis, float p_direction) const {
	if (int(p_axis) < 0) {
		return Ref<Texture2D>();
	}
	return _get_icon(CATEGORY_JOY_AXIS, _axis_index(p_axis, p_direction));
}

void InputIconSet::set_key_icons(const Dictionary &p_icons) {
	_set_icon_dictionary(CATEGORY_KEY, p_icons);
}

Dictionary InputIconSet::get_key_icons() const {
	return _get_icon_dictionary(CATEGORY_KEY);
}

void InputIconSet::set_mouse_button_icons(const Dictionary &p_icons) {
	_set_icon_dictionary(CATEGORY_MOUSE_BUTTON, p_icons);
}

Dictionary InputIconSet::get_mouse_button_icons() const {
	return _get_icon_dictionary(CATEGORY_MOUSE_BUTTON);
}

void InputIconSet::set_joy_button_icons(const Dictionary &p_icons) {
	_set_icon_dictionary(CATEGORY_JOY_BUTTON, p_icons);
}

Dictionary InputIconSet::get_joy_button_icons() const {
	return _get_icon_dictionary(CATEGORY_JOY_BUTTON);
}

void InputIconSet::set_joy_axis_icons(const Dictionary &p_icons) {
	_set_icon_dictionary(CATEGORY_JOY_AXIS, p_icons);
}

Dictionary InputIconSet::get_joy_axis_icons() const {
	return _get_icon_dictionary(CATEGORY_JOY_AXIS);
}

void InputIconSet::set_fallback_key_icon(const Ref<Texture2D> &p_icon) {
	if (fallback_key_icon != p_icon) {
		fallback_key_icon = p_icon;
		emit_changed();
	}
}

Ref<Texture2D> InputIconSet::get_fallback_key_icon() const {
	return fallback_key_icon;
}

void InputIconSet::set_fallback_mouse_button_icon(const Ref<Texture2D> &p_icon) {
	if (fallback_mouse_button_icon != p_icon) {
		fallback_mouse_button_icon = p_icon;
		emit_changed();
	}
}

Ref<Texture2D> InputIconSet::get_fallback_mouse_button_icon() const {
	return fallback_mouse_button_icon;
}

void InputIconSet::set_fallback_joy_button_icon(const Ref<Texture2D> &p_icon) {
	if (fallback_joy_button_icon != p_icon) {
		fallback_joy_button_icon = p_icon;
		emit_changed();
	}
}

Ref<Texture2D> InputIconSet::get_fallback_joy_button_icon() const {
	return fallback_joy_button_icon;
}

void InputIconSet::set_fallback_joy_axis_icon(const Ref<Texture2D> &p_icon) {
	if (fallback_joy_axis_icon != p_icon) {
		fallback_joy_axis_icon = p_icon;
		emit_changed();
	}
}

Ref<Texture2D> InputIconSet::get_fallback_joy_axis_icon() const {
	return fallback_joy_axis_icon;
}

Ref<Texture2D> InputIconSet::get_icon(const Ref<InputEvent> &p_event, bool p_use_fallback) const {
	Category category = CATEGORY_MAX;
	int index = -1;
	if (!_event_to_index(p_event, category, index)) {
		return Ref<Texture2D>();
	}
	const Ref<Texture2D> icon = _get_icon(category, index);
	if (icon.is_valid() || !p_use_fallback) {
		return icon;
	}
	return get_fallback_icon(p_event);
}

Ref<Texture2D> InputIconSet::get_icon_for_text(const String &p_text, bool p_use_fallback) const {
	return get_icon(InputAction::text_to_event(p_text), p_use_fallback);
}

Ref<Texture2D> InputIconSet::get_fallback_icon(const Ref<InputEvent> &p_event) const {
	Category category = CATEGORY_MAX;
	int index = -1;
	if (!_event_to_index(p_event, category, index)) {
		return Ref<Texture2D>();
	}
	switch (category) {
		case CATEGORY_KEY:
			return fallback_key_icon;
		case CATEGORY_MOUSE_BUTTON:
			return fallback_mouse_button_icon;
		case CATEGORY_JOY_BUTTON:
			return fallback_joy_button_icon;
		case CATEGORY_JOY_AXIS:
			return fallback_joy_axis_icon;
		default:
			return Ref<Texture2D>();
	}
}

bool InputIconSet::has_icon(const Ref<InputEvent> &p_event) const {
	return get_icon(p_event, false).is_valid();
}

void InputIconSet::clear() {
	key_icons.clear();
	mouse_button_icons.clear();
	joy_button_icons.clear();
	joy_axis_icons.clear();
	fallback_key_icon.unref();
	fallback_mouse_button_icon.unref();
	fallback_joy_button_icon.unref();
	fallback_joy_axis_icon.unref();
	emit_changed();
}

void InputIconSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_key_icon", "key", "icon"), &InputIconSet::set_key_icon);
	ClassDB::bind_method(D_METHOD("get_key_icon", "key"), &InputIconSet::get_key_icon);
	ClassDB::bind_method(D_METHOD("set_mouse_button_icon", "button", "icon"), &InputIconSet::set_mouse_button_icon);
	ClassDB::bind_method(D_METHOD("get_mouse_button_icon", "button"), &InputIconSet::get_mouse_button_icon);
	ClassDB::bind_method(D_METHOD("set_joy_button_icon", "button", "icon"), &InputIconSet::set_joy_button_icon);
	ClassDB::bind_method(D_METHOD("get_joy_button_icon", "button"), &InputIconSet::get_joy_button_icon);
	ClassDB::bind_method(D_METHOD("set_joy_axis_icon", "axis", "direction", "icon"), &InputIconSet::set_joy_axis_icon);
	ClassDB::bind_method(D_METHOD("get_joy_axis_icon", "axis", "direction"), &InputIconSet::get_joy_axis_icon);

	ClassDB::bind_method(D_METHOD("set_key_icons", "icons"), &InputIconSet::set_key_icons);
	ClassDB::bind_method(D_METHOD("get_key_icons"), &InputIconSet::get_key_icons);
	ClassDB::bind_method(D_METHOD("set_mouse_button_icons", "icons"), &InputIconSet::set_mouse_button_icons);
	ClassDB::bind_method(D_METHOD("get_mouse_button_icons"), &InputIconSet::get_mouse_button_icons);
	ClassDB::bind_method(D_METHOD("set_joy_button_icons", "icons"), &InputIconSet::set_joy_button_icons);
	ClassDB::bind_method(D_METHOD("get_joy_button_icons"), &InputIconSet::get_joy_button_icons);
	ClassDB::bind_method(D_METHOD("set_joy_axis_icons", "icons"), &InputIconSet::set_joy_axis_icons);
	ClassDB::bind_method(D_METHOD("get_joy_axis_icons"), &InputIconSet::get_joy_axis_icons);

	ClassDB::bind_method(D_METHOD("set_fallback_key_icon", "icon"), &InputIconSet::set_fallback_key_icon);
	ClassDB::bind_method(D_METHOD("get_fallback_key_icon"), &InputIconSet::get_fallback_key_icon);
	ClassDB::bind_method(D_METHOD("set_fallback_mouse_button_icon", "icon"), &InputIconSet::set_fallback_mouse_button_icon);
	ClassDB::bind_method(D_METHOD("get_fallback_mouse_button_icon"), &InputIconSet::get_fallback_mouse_button_icon);
	ClassDB::bind_method(D_METHOD("set_fallback_joy_button_icon", "icon"), &InputIconSet::set_fallback_joy_button_icon);
	ClassDB::bind_method(D_METHOD("get_fallback_joy_button_icon"), &InputIconSet::get_fallback_joy_button_icon);
	ClassDB::bind_method(D_METHOD("set_fallback_joy_axis_icon", "icon"), &InputIconSet::set_fallback_joy_axis_icon);
	ClassDB::bind_method(D_METHOD("get_fallback_joy_axis_icon"), &InputIconSet::get_fallback_joy_axis_icon);

	ClassDB::bind_method(D_METHOD("get_icon", "event", "use_fallback"), &InputIconSet::get_icon, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_icon_for_text", "text", "use_fallback"), &InputIconSet::get_icon_for_text, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_fallback_icon", "event"), &InputIconSet::get_fallback_icon);
	ClassDB::bind_method(D_METHOD("has_icon", "event"), &InputIconSet::has_icon);
	ClassDB::bind_method(D_METHOD("clear"), &InputIconSet::clear);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "key_icons", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_key_icons", "get_key_icons");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "mouse_button_icons", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_mouse_button_icons", "get_mouse_button_icons");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "joy_button_icons", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_joy_button_icons", "get_joy_button_icons");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "joy_axis_icons", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_joy_axis_icons", "get_joy_axis_icons");

	ADD_GROUP("Fallback", "fallback_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_key_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_fallback_key_icon", "get_fallback_key_icon");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_mouse_button_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_fallback_mouse_button_icon", "get_fallback_mouse_button_icon");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_joy_button_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_fallback_joy_button_icon", "get_fallback_joy_button_icon");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_joy_axis_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_fallback_joy_axis_icon", "get_fallback_joy_axis_icon");
}
