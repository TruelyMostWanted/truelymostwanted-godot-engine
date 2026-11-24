/**************************************************************************/
/*  tts_persona.cpp                                                       */
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

#include "tts_persona.h"

#include "core/object/class_db.h"

void TTSPersona::set_persona_name(const StringName &p_name) {
	if (persona_name == p_name) {
		return;
	}
	persona_name = p_name;
	emit_changed();
}

StringName TTSPersona::get_persona_name() const {
	return persona_name;
}

void TTSPersona::set_voice(const String &p_voice) {
	if (voice == p_voice) {
		return;
	}
	voice = p_voice;
	emit_changed();
}

String TTSPersona::get_voice() const {
	return voice;
}

void TTSPersona::set_language(const String &p_language) {
	if (language == p_language) {
		return;
	}
	language = p_language;
	emit_changed();
}

String TTSPersona::get_language() const {
	return language;
}

void TTSPersona::set_volume(int p_volume) {
	const int clamped = CLAMP(p_volume, 0, 100);
	if (volume == clamped) {
		return;
	}
	volume = clamped;
	emit_changed();
}

int TTSPersona::get_volume() const {
	return volume;
}

void TTSPersona::set_pitch(float p_pitch) {
	const float clamped = CLAMP(p_pitch, 0.0f, 2.0f);
	if (pitch == clamped) {
		return;
	}
	pitch = clamped;
	emit_changed();
}

float TTSPersona::get_pitch() const {
	return pitch;
}

void TTSPersona::set_rate(float p_rate) {
	const float clamped = CLAMP(p_rate, 0.1f, 10.0f);
	if (rate == clamped) {
		return;
	}
	rate = clamped;
	emit_changed();
}

float TTSPersona::get_rate() const {
	return rate;
}

void TTSPersona::set_interrupt(bool p_interrupt) {
	if (interrupt == p_interrupt) {
		return;
	}
	interrupt = p_interrupt;
	emit_changed();
}

bool TTSPersona::is_interrupt() const {
	return interrupt;
}

void TTSPersona::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_persona_name", "persona_name"), &TTSPersona::set_persona_name);
	ClassDB::bind_method(D_METHOD("get_persona_name"), &TTSPersona::get_persona_name);
	ClassDB::bind_method(D_METHOD("set_voice", "voice"), &TTSPersona::set_voice);
	ClassDB::bind_method(D_METHOD("get_voice"), &TTSPersona::get_voice);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &TTSPersona::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &TTSPersona::get_language);
	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &TTSPersona::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &TTSPersona::get_volume);
	ClassDB::bind_method(D_METHOD("set_pitch", "pitch"), &TTSPersona::set_pitch);
	ClassDB::bind_method(D_METHOD("get_pitch"), &TTSPersona::get_pitch);
	ClassDB::bind_method(D_METHOD("set_rate", "rate"), &TTSPersona::set_rate);
	ClassDB::bind_method(D_METHOD("get_rate"), &TTSPersona::get_rate);
	ClassDB::bind_method(D_METHOD("set_interrupt", "interrupt"), &TTSPersona::set_interrupt);
	ClassDB::bind_method(D_METHOD("is_interrupt"), &TTSPersona::is_interrupt);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "persona_name"), "set_persona_name", "get_persona_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voice"), "set_voice", "get_voice");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "volume", PROPERTY_HINT_RANGE, "0,100,1"), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_pitch", "get_pitch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate", PROPERTY_HINT_RANGE, "0.1,10,0.01"), "set_rate", "get_rate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interrupt"), "set_interrupt", "is_interrupt");
}
