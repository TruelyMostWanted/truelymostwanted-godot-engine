/**************************************************************************/
/*  tts_persona.h                                                         */
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

// A voice with its settings for TTSServer. The persona name is a separate property
// (not `resource_name`), because it is the key in TTSServer's persona list.
class TTSPersona : public Resource {
	GDCLASS(TTSPersona, Resource);

	StringName persona_name;
	String voice;
	String language;
	int volume = 50;
	float pitch = 1.0f;
	float rate = 1.0f;
	bool interrupt = false;

protected:
	static void _bind_methods();

public:
	void set_persona_name(const StringName &p_name);
	StringName get_persona_name() const;

	void set_voice(const String &p_voice);
	String get_voice() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_volume(int p_volume);
	int get_volume() const;

	void set_pitch(float p_pitch);
	float get_pitch() const;

	void set_rate(float p_rate);
	float get_rate() const;

	void set_interrupt(bool p_interrupt);
	bool is_interrupt() const;
};
