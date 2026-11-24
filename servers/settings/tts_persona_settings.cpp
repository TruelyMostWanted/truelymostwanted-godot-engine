/**************************************************************************/
/*  tts_persona_settings.cpp                                              */
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

#include "tts_persona_settings.h"

#include "core/object/class_db.h"

/////////////////////////////////////////////////////////////////////////////
// TTSPersonaSettings.

void TTSPersonaSettings::set_persona_name(const StringName &p_name) {
	persona_name = p_name;
	emit_changed();
}

StringName TTSPersonaSettings::get_persona_name() const {
	return persona_name;
}

void TTSPersonaSettings::set_voice(const String &p_voice) {
	voice = p_voice;
	emit_changed();
}

String TTSPersonaSettings::get_voice() const {
	return voice;
}

void TTSPersonaSettings::set_language(const String &p_language) {
	language = p_language;
	emit_changed();
}

String TTSPersonaSettings::get_language() const {
	return language;
}

// Same ranges as TTSPersona.
void TTSPersonaSettings::set_volume(int p_volume) {
	volume = CLAMP(p_volume, 0, 100);
	emit_changed();
}

int TTSPersonaSettings::get_volume() const {
	return volume;
}

void TTSPersonaSettings::set_pitch(float p_pitch) {
	pitch = CLAMP(p_pitch, 0.0f, 2.0f);
	emit_changed();
}

float TTSPersonaSettings::get_pitch() const {
	return pitch;
}

void TTSPersonaSettings::set_rate(float p_rate) {
	rate = CLAMP(p_rate, 0.1f, 10.0f);
	emit_changed();
}

float TTSPersonaSettings::get_rate() const {
	return rate;
}

void TTSPersonaSettings::set_interrupt(bool p_interrupt) {
	interrupt = p_interrupt;
	emit_changed();
}

bool TTSPersonaSettings::is_interrupt() const {
	return interrupt;
}

void TTSPersonaSettings::set_default(bool p_default) {
	default_persona = p_default;
	emit_changed();
}

bool TTSPersonaSettings::is_default() const {
	return default_persona;
}

Ref<TTSPersona> TTSPersonaSettings::to_persona() const {
	Ref<TTSPersona> persona;
	persona.instantiate();
	persona->set_persona_name(persona_name);
	persona->set_voice(voice);
	persona->set_language(language);
	persona->set_volume(volume);
	persona->set_pitch(pitch);
	persona->set_rate(rate);
	persona->set_interrupt(interrupt);
	return persona;
}

void TTSPersonaSettings::copy_from_persona(const Ref<TTSPersona> &p_persona) {
	ERR_FAIL_COND(p_persona.is_null());
	persona_name = p_persona->get_persona_name();
	voice = p_persona->get_voice();
	language = p_persona->get_language();
	volume = p_persona->get_volume();
	pitch = p_persona->get_pitch();
	rate = p_persona->get_rate();
	interrupt = p_persona->is_interrupt();
	emit_changed();
}

void TTSPersonaSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_persona_name", "persona_name"), &TTSPersonaSettings::set_persona_name);
	ClassDB::bind_method(D_METHOD("get_persona_name"), &TTSPersonaSettings::get_persona_name);
	ClassDB::bind_method(D_METHOD("set_voice", "voice"), &TTSPersonaSettings::set_voice);
	ClassDB::bind_method(D_METHOD("get_voice"), &TTSPersonaSettings::get_voice);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &TTSPersonaSettings::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &TTSPersonaSettings::get_language);
	ClassDB::bind_method(D_METHOD("set_volume", "volume"), &TTSPersonaSettings::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &TTSPersonaSettings::get_volume);
	ClassDB::bind_method(D_METHOD("set_pitch", "pitch"), &TTSPersonaSettings::set_pitch);
	ClassDB::bind_method(D_METHOD("get_pitch"), &TTSPersonaSettings::get_pitch);
	ClassDB::bind_method(D_METHOD("set_rate", "rate"), &TTSPersonaSettings::set_rate);
	ClassDB::bind_method(D_METHOD("get_rate"), &TTSPersonaSettings::get_rate);
	ClassDB::bind_method(D_METHOD("set_interrupt", "interrupt"), &TTSPersonaSettings::set_interrupt);
	ClassDB::bind_method(D_METHOD("is_interrupt"), &TTSPersonaSettings::is_interrupt);
	ClassDB::bind_method(D_METHOD("set_default", "default_persona"), &TTSPersonaSettings::set_default);
	ClassDB::bind_method(D_METHOD("is_default"), &TTSPersonaSettings::is_default);
	ClassDB::bind_method(D_METHOD("to_persona"), &TTSPersonaSettings::to_persona);
	ClassDB::bind_method(D_METHOD("copy_from_persona", "persona"), &TTSPersonaSettings::copy_from_persona);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "persona_name"), "set_persona_name", "get_persona_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voice"), "set_voice", "get_voice");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "volume", PROPERTY_HINT_RANGE, "0,100,1"), "set_volume", "get_volume");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_pitch", "get_pitch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate", PROPERTY_HINT_RANGE, "0.1,10,0.01"), "set_rate", "get_rate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interrupt"), "set_interrupt", "is_interrupt");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default"), "set_default", "is_default");
}

/////////////////////////////////////////////////////////////////////////////
// TTSPersonaListSettings.

void TTSPersonaListSettings::set_personas(const TypedArray<TTSPersonaSettings> &p_personas) {
	personas = p_personas;
	emit_changed();
}

TypedArray<TTSPersonaSettings> TTSPersonaListSettings::get_personas() const {
	return personas;
}

void TTSPersonaListSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_personas", "personas"), &TTSPersonaListSettings::set_personas);
	ClassDB::bind_method(D_METHOD("get_personas"), &TTSPersonaListSettings::get_personas);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "personas", PROPERTY_HINT_ARRAY_TYPE, "TTSPersonaSettings"), "set_personas", "get_personas");
}
