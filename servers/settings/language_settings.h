/**************************************************************************/
/*  language_settings.h                                                   */
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

#include "core/io/resource.h"

// Data of the `language` settings domain (#17). No engine dependency: LanguageSettingsServer pushes the
// values to TranslationServer and the root window.
class LanguageSettings : public Resource {
	GDCLASS(LanguageSettings, Resource);

	String locale; // Empty: follow the OS (or the test locale of the project).
	bool pseudolocalization_enabled = false;
	int layout_direction = 0; // Window::LAYOUT_DIRECTION_INHERITED.
	int root_auto_translate_mode = 1; // Node::AUTO_TRANSLATE_MODE_ALWAYS.

protected:
	static void _bind_methods();

public:
	void set_locale(const String &p_locale);
	String get_locale() const;
	void set_pseudolocalization_enabled(bool p_enabled);
	bool is_pseudolocalization_enabled() const;
	void set_layout_direction(int p_direction);
	int get_layout_direction() const;
	void set_root_auto_translate_mode(int p_mode);
	int get_root_auto_translate_mode() const;
};
