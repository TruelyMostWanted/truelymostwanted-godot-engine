/**************************************************************************/
/*  tts_persona_settings.h                                                */
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
#include "core/variant/typed_array.h"
#include "servers/tts/tts_persona.h"

// One persona of the `tts_personas` settings domain (TTSPersonaSettingsServer): the values of a TTSPersona
// and whether it is the player's default persona. One row of the CSV file.
class TTSPersonaSettings : public Resource {
	GDCLASS(TTSPersonaSettings, Resource);

	StringName persona_name;
	String voice;
	String language;
	int volume = 50;
	float pitch = 1.0f;
	float rate = 1.0f;
	bool interrupt = false;
	bool default_persona = false;

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
	void set_default(bool p_default);
	bool is_default() const;

	// A new TTSPersona with these values (not shared with this entry).
	Ref<TTSPersona> to_persona() const;
	// Takes the values of `p_persona` (everything but `default`).
	void copy_from_persona(const Ref<TTSPersona> &p_persona);
};

// Settings of the `tts_personas` domain: the player's personas. It holds nothing but the list, so it can be stored as CSV.
class TTSPersonaListSettings : public Resource {
	GDCLASS(TTSPersonaListSettings, Resource);

	TypedArray<TTSPersonaSettings> personas;

protected:
	static void _bind_methods();

public:
	void set_personas(const TypedArray<TTSPersonaSettings> &p_personas);
	TypedArray<TTSPersonaSettings> get_personas() const;
};
