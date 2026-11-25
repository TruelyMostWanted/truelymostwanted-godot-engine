/**************************************************************************/
/*  input_icon_server.h                                                   */
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

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/variant/binder_common.h"
#include "scene/resources/input_icon_set.h"

// Holds one InputIconSet per device family (#198) and returns the icon of an input event: the family comes from the
// event's device (joypad name and vendor id), the sets from the project settings `input_devices/icons/<family>` or
// from set_icon_set() at runtime. Used by the editor's Input Map and by in-game button prompts.
class InputIconServer : public Object {
	GDCLASS(InputIconServer, Object);

public:
	enum InputIconFamily {
		INPUT_ICON_FAMILY_AUTO = -1,
		INPUT_ICON_FAMILY_KEYBOARD_MOUSE,
		INPUT_ICON_FAMILY_GENERIC_GAMEPAD,
		INPUT_ICON_FAMILY_XBOX,
		INPUT_ICON_FAMILY_PLAYSTATION,
		INPUT_ICON_FAMILY_NINTENDO,
		INPUT_ICON_FAMILY_MAX,
	};

private:
	static InputIconServer *singleton;

	struct Slot {
		Ref<InputIconSet> set;
		bool overridden = false; // Set at runtime; the project setting is ignored.
		bool loaded = false; // Loaded from loaded_path.
		String loaded_path;
	};

	mutable Mutex mutex;
	Slot slots[INPUT_ICON_FAMILY_MAX];
	InputIconFamily gamepad_family = INPUT_ICON_FAMILY_AUTO;

	static String _get_setting_name(InputIconFamily p_family);
	static String _get_setting_path(InputIconFamily p_family);
	// The set of a family; loads it from the project setting on first use. Never emits a signal.
	Ref<InputIconSet> _get_set(InputIconFamily p_family);
	// Connects the `changed` signal of a new set and disconnects the old one; call without the lock.
	void _update_connections(const Ref<InputIconSet> &p_old, const Ref<InputIconSet> &p_new);
	void _on_set_changed(ObjectID p_set_id);
	void _on_settings_changed();
	void _emit_icon_set_changed(InputIconFamily p_family);
	InputIconFamily _resolve_family(const Ref<InputEvent> &p_event, InputIconFamily p_family) const;

protected:
	static void _bind_methods();

public:
	static InputIconServer *get_singleton();

	void set_icon_set(InputIconFamily p_family, const Ref<InputIconSet> &p_set);
	Ref<InputIconSet> get_icon_set(InputIconFamily p_family);
	// Drops the set of set_icon_set(); the project setting applies again.
	void reset_icon_set(InputIconFamily p_family);
	bool has_icon_set_override(InputIconFamily p_family) const;

	void set_gamepad_family(InputIconFamily p_family);
	InputIconFamily get_gamepad_family() const;

	// The icon of an event: specific icon of the family, of GENERIC_GAMEPAD (gamepad families only), then the
	// fallback icon of the event type in the same order; null if no set has one.
	Ref<Texture2D> get_icon(const Ref<InputEvent> &p_event, InputIconFamily p_family = INPUT_ICON_FAMILY_AUTO);
	Ref<Texture2D> get_icon_for_text(const String &p_text, InputIconFamily p_family = INPUT_ICON_FAMILY_AUTO);
	// The family get_icon() uses for an event with INPUT_ICON_FAMILY_AUTO.
	InputIconFamily get_event_family(const Ref<InputEvent> &p_event) const;
	// Family of a connected joypad; GENERIC_GAMEPAD if it is unknown or not connected.
	InputIconFamily get_device_family(int p_device) const;
	// Family of a joypad name (`Xbox Series Controller`, `PS5 Controller`, a browser's `054c-0ce6-...`) and USB vendor id.
	static InputIconFamily get_family_for_joy_name(const String &p_joy_name, int p_vendor_id = -1);

	InputIconServer();
	~InputIconServer();
};

VARIANT_ENUM_CAST(InputIconServer::InputIconFamily);
