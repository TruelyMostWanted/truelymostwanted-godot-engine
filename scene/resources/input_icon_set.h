/**************************************************************************/
/*  input_icon_set.h                                                      */
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

#include "core/input/input_event.h"
#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "scene/resources/texture.h"

// Icons per input value (#198): one Texture2D per key, mouse button, gamepad button and gamepad axis direction,
// plus one fallback icon per event type. The values are saved with the text names of InputAction.event_to_text()
// (`Space`, `Left`, `LeftStickPress`, `LeftStickLeft`); the Inspector shows one property per value.
class InputIconSet : public Resource {
	GDCLASS(InputIconSet, Resource);

	HashMap<int, Ref<Texture2D>> key_icons;
	HashMap<int, Ref<Texture2D>> mouse_button_icons;
	HashMap<int, Ref<Texture2D>> joy_button_icons;
	HashMap<int, Ref<Texture2D>> joy_axis_icons; // Key: axis * 2 + 1 for the positive direction, axis * 2 for the negative one.

	Ref<Texture2D> fallback_key_icon;
	Ref<Texture2D> fallback_mouse_button_icon;
	Ref<Texture2D> fallback_joy_button_icon;
	Ref<Texture2D> fallback_joy_axis_icon;

	enum Category {
		CATEGORY_KEY,
		CATEGORY_MOUSE_BUTTON,
		CATEGORY_JOY_BUTTON,
		CATEGORY_JOY_AXIS,
		CATEGORY_MAX,
	};

	static int _axis_index(JoyAxis p_axis, float p_direction) { return int(p_axis) * 2 + (p_direction < 0.0f ? 0 : 1); }
	// Category and map index of an event; false for events without an icon (touch, MIDI, ...).
	static bool _event_to_index(const Ref<InputEvent> &p_event, Category &r_category, int &r_index);
	// Name of a value in the text form of #193 (`Space`, `Left`, `A`, `LeftStickLeft`) and back.
	static String _index_to_name(Category p_category, int p_index);
	static int _name_to_index(Category p_category, const String &p_name);

	HashMap<int, Ref<Texture2D>> &_get_map(Category p_category);
	const HashMap<int, Ref<Texture2D>> &_get_map(Category p_category) const;
	void _set_icon(Category p_category, int p_index, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> _get_icon(Category p_category, int p_index) const;
	Dictionary _get_icon_dictionary(Category p_category) const;
	void _set_icon_dictionary(Category p_category, const Dictionary &p_icons);
	// Inspector property `keyboard/Space` to category and index.
	static bool _property_to_index(const StringName &p_property, Category &r_category, int &r_index);

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

public:
	void set_key_icon(Key p_key, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_key_icon(Key p_key) const;
	void set_mouse_button_icon(MouseButton p_button, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_mouse_button_icon(MouseButton p_button) const;
	void set_joy_button_icon(JoyButton p_button, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_joy_button_icon(JoyButton p_button) const;
	void set_joy_axis_icon(JoyAxis p_axis, float p_direction, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_joy_axis_icon(JoyAxis p_axis, float p_direction) const;

	void set_key_icons(const Dictionary &p_icons);
	Dictionary get_key_icons() const;
	void set_mouse_button_icons(const Dictionary &p_icons);
	Dictionary get_mouse_button_icons() const;
	void set_joy_button_icons(const Dictionary &p_icons);
	Dictionary get_joy_button_icons() const;
	void set_joy_axis_icons(const Dictionary &p_icons);
	Dictionary get_joy_axis_icons() const;

	void set_fallback_key_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_fallback_key_icon() const;
	void set_fallback_mouse_button_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_fallback_mouse_button_icon() const;
	void set_fallback_joy_button_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_fallback_joy_button_icon() const;
	void set_fallback_joy_axis_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_fallback_joy_axis_icon() const;

	// The icon of the event's key, button or axis direction; with p_use_fallback the fallback icon of its type if
	// there is none. Modifiers and the device are ignored.
	Ref<Texture2D> get_icon(const Ref<InputEvent> &p_event, bool p_use_fallback = true) const;
	Ref<Texture2D> get_icon_for_text(const String &p_text, bool p_use_fallback = true) const;
	// Only the fallback icon of the event's type (used by InputIconServer after all specific icons).
	Ref<Texture2D> get_fallback_icon(const Ref<InputEvent> &p_event) const;
	bool has_icon(const Ref<InputEvent> &p_event) const;
	void clear();
};
