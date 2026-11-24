/**************************************************************************/
/*  builtin_input_actions.h                                               */
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

#include "core/input/input_enums.h"
#include "core/os/keyboard.h"

// Godot's built-in input actions (`ui_*`), the data of InputMap::get_builtins() (#172).
// The table is generated at build time from `core/input/builtin_input_actions.json`
// (see `make_builtin_input_actions()` in `core/input/input_builders.py`), so no file is read at runtime.
// An action name with a feature suffix (`ui_text_backspace_word.macos`) replaces the plain action
// when OS::has_feature() is true for the feature.
struct BuiltinInputActions {
	enum EventType {
		EVENT_KEY,
		EVENT_JOYPAD_BUTTON,
		EVENT_JOYPAD_MOTION,
	};

	struct Event {
		EventType type;
		Key key; // EVENT_KEY: keycode with modifier masks.
		JoyButton button; // EVENT_JOYPAD_BUTTON.
		JoyAxis axis; // EVENT_JOYPAD_MOTION.
		float axis_value; // EVENT_JOYPAD_MOTION.
	};

	struct Action {
		const char *name;
		const Event *events;
		int event_count;
	};

	static const Action actions[];
	static const int action_count;
};
