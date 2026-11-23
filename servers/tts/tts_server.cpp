/**************************************************************************/
/*  tts_server.cpp                                                        */
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

#include "tts_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"

TtsServer *TtsServer::singleton = nullptr;

TtsServer *TtsServer::get_singleton() {
	return singleton;
}

// Platforms write languages as "en_US", "en-US" or "en". Compare them as lower case with underscores.
String TtsServer::_normalize_language(const String &p_language) {
	return p_language.strip_edges().replace_char('-', '_').to_lower();
}

// An empty language matches every voice. "en" matches "en_US" and "en_GB", "en_US" matches only "en_US".
bool TtsServer::_language_matches(const String &p_voice_language, const String &p_language) {
	const String language = _normalize_language(p_language);
	if (language.is_empty()) {
		return true;
	}
	const String voice_language = _normalize_language(p_voice_language);
	if (voice_language == language) {
		return true;
	}
	if (!language.contains_char('_')) {
		return voice_language.get_slicec('_', 0) == language;
	}
	return false;
}

String TtsServer::_find_voice_for_language(const TypedArray<Dictionary> &p_voices, const String &p_language) {
	const String language = _normalize_language(p_language);
	if (language.is_empty()) {
		return String();
	}
	// Same language and region first.
	for (int i = 0; i < p_voices.size(); i++) {
		const Dictionary &voice = p_voices[i];
		if (_normalize_language(voice.get("language", String())) == language) {
			return voice.get("id", String());
		}
	}
	// Then the same language in any region.
	const String base = language.get_slicec('_', 0);
	for (int i = 0; i < p_voices.size(); i++) {
		const Dictionary &voice = p_voices[i];
		if (_normalize_language(voice.get("language", String())).get_slicec('_', 0) == base) {
			return voice.get("id", String());
		}
	}
	return String();
}

String TtsServer::_get_unavailable_reason() const {
	if (!bool(GLOBAL_GET("audio/general/text_to_speech"))) {
		return "Text to speech is disabled. Enable the project setting \"audio/general/text_to_speech\" and restart the project.";
	}
	DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr) {
		return "Text to speech is not available: there is no DisplayServer.";
	}
	if (!display_server->has_feature(DisplayServerEnums::FEATURE_TEXT_TO_SPEECH)) {
		return "Text to speech is not supported by the current DisplayServer (\"" + display_server->get_name() + "\").";
	}
	return String();
}

Ref<TtsPersona> TtsServer::_get_default_persona_locked() const {
	if (!default_persona_name.is_empty()) {
		const Ref<TtsPersona> *persona = personas.getptr(default_persona_name);
		if (persona != nullptr) {
			return *persona;
		}
	}
	if (builtin_persona.is_null()) {
		builtin_persona.instantiate();
	}
	return builtin_persona;
}

void TtsServer::_emit_utterance_event(int p_event, int64_t p_id, int p_char_pos) {
	switch (p_event) {
		case DisplayServerEnums::TTS_UTTERANCE_STARTED: {
			emit_signal(SNAME("utterance_started"), p_id);
		} break;
		case DisplayServerEnums::TTS_UTTERANCE_ENDED: {
			{
				MutexLock lock(mutex);
				pending.erase(p_id);
			}
			emit_signal(SNAME("utterance_ended"), p_id);
		} break;
		case DisplayServerEnums::TTS_UTTERANCE_CANCELED: {
			{
				MutexLock lock(mutex);
				pending.erase(p_id);
			}
			emit_signal(SNAME("utterance_canceled"), p_id);
		} break;
		case DisplayServerEnums::TTS_UTTERANCE_BOUNDARY: {
			emit_signal(SNAME("utterance_boundary"), p_id, p_char_pos);
		} break;
		default:
			break;
	}
}

void TtsServer::post_utterance_event(int p_event, int64_t p_id, int p_char_pos) {
	ERR_FAIL_INDEX(p_event, DisplayServerEnums::TTS_UTTERANCE_MAX);
	// Platform callbacks may come from other threads in rapid succession: signals are always sent on the main thread.
	callable_mp(this, &TtsServer::_emit_utterance_event).call_deferred(p_event, p_id, p_char_pos);
}

bool TtsServer::is_available() const {
	return _get_unavailable_reason().is_empty();
}

int64_t TtsServer::speak(const String &p_text, const Ref<TtsPersona> &p_persona) {
	ERR_FAIL_COND_V_MSG(p_text.is_empty(), -1, "Cannot speak an empty text.");
	const String reason = _get_unavailable_reason();
	ERR_FAIL_COND_V_MSG(!reason.is_empty(), -1, reason);

	Ref<TtsPersona> persona = p_persona;
	if (persona.is_null()) {
		MutexLock lock(mutex);
		persona = _get_default_persona_locked();
	}
	const String voice = resolve_voice(persona);

	int64_t id = 0;
	{
		MutexLock lock(mutex);
		id = next_utterance_id++;
		pending.insert(id);
	}
	DisplayServer::get_singleton()->tts_speak(p_text, voice, persona->get_volume(), persona->get_pitch(), persona->get_rate(), id, persona->is_interrupt());
	return id;
}

void TtsServer::stop() {
	{
		MutexLock lock(mutex);
		pending.clear();
	}
	if (is_available()) {
		DisplayServer::get_singleton()->tts_stop();
	}
}

void TtsServer::pause() {
	if (is_available()) {
		DisplayServer::get_singleton()->tts_pause();
	}
}

void TtsServer::resume() {
	if (is_available()) {
		DisplayServer::get_singleton()->tts_resume();
	}
}

bool TtsServer::is_speaking() const {
	if (!is_available()) {
		return false;
	}
	return DisplayServer::get_singleton()->tts_is_speaking();
}

bool TtsServer::is_paused() const {
	if (!is_available()) {
		return false;
	}
	return DisplayServer::get_singleton()->tts_is_paused();
}

PackedInt64Array TtsServer::get_pending_utterances() const {
	MutexLock lock(mutex);
	PackedInt64Array ids;
	for (const int64_t id : pending) {
		ids.push_back(id);
	}
	ids.sort();
	return ids;
}

void TtsServer::add_persona(const Ref<TtsPersona> &p_persona) {
	ERR_FAIL_COND_MSG(p_persona.is_null(), "Cannot add a null persona.");
	const String name = p_persona->get_persona_name();
	ERR_FAIL_COND_MSG(name.is_empty(), "Cannot add a persona without a name.");
	MutexLock lock(mutex);
	personas[name] = p_persona;
}

void TtsServer::remove_persona(const String &p_name) {
	MutexLock lock(mutex);
	personas.erase(p_name);
	if (default_persona_name == p_name) {
		default_persona_name = String();
	}
}

bool TtsServer::has_persona(const String &p_name) const {
	MutexLock lock(mutex);
	return personas.has(p_name);
}

Ref<TtsPersona> TtsServer::get_persona(const String &p_name) const {
	MutexLock lock(mutex);
	const Ref<TtsPersona> *persona = personas.getptr(p_name);
	if (persona == nullptr) {
		return Ref<TtsPersona>();
	}
	return *persona;
}

TypedArray<TtsPersona> TtsServer::get_personas() const {
	MutexLock lock(mutex);
	TypedArray<TtsPersona> list;
	for (const KeyValue<String, Ref<TtsPersona>> &E : personas) {
		list.push_back(E.value);
	}
	return list;
}

PackedStringArray TtsServer::get_persona_names() const {
	MutexLock lock(mutex);
	PackedStringArray names;
	for (const KeyValue<String, Ref<TtsPersona>> &E : personas) {
		names.push_back(E.key);
	}
	return names;
}

void TtsServer::clear_personas() {
	MutexLock lock(mutex);
	personas.clear();
	default_persona_name = String();
}

void TtsServer::set_default_persona(const String &p_name) {
	MutexLock lock(mutex);
	ERR_FAIL_COND_MSG(!p_name.is_empty() && !personas.has(p_name), vformat("No persona named \"%s\". Add it with add_persona() first.", p_name));
	default_persona_name = p_name;
}

Ref<TtsPersona> TtsServer::get_default_persona() const {
	MutexLock lock(mutex);
	return _get_default_persona_locked();
}

String TtsServer::get_default_persona_name() const {
	MutexLock lock(mutex);
	return default_persona_name;
}

TypedArray<Dictionary> TtsServer::get_voices(const String &p_language) const {
	TypedArray<Dictionary> result;
	if (!is_available()) {
		return result;
	}
	const TypedArray<Dictionary> voices = DisplayServer::get_singleton()->tts_get_voices();
	for (int i = 0; i < voices.size(); i++) {
		const Dictionary &voice = voices[i];
		if (_language_matches(voice.get("language", String()), p_language)) {
			result.push_back(voice);
		}
	}
	return result;
}

String TtsServer::resolve_voice(const Ref<TtsPersona> &p_persona) const {
	Ref<TtsPersona> persona = p_persona;
	if (persona.is_null()) {
		MutexLock lock(mutex);
		persona = _get_default_persona_locked();
	}

	const TypedArray<Dictionary> voices = get_voices();
	if (voices.is_empty()) {
		// No voice list (TTS unavailable, or the Web voice list is still loading): let the platform decide.
		return persona->get_voice();
	}

	// 1. The voice of the persona, by ID, then by display name (names are more stable across machines).
	const String wanted = persona->get_voice();
	if (!wanted.is_empty()) {
		for (int i = 0; i < voices.size(); i++) {
			const Dictionary &voice = voices[i];
			if (String(voice.get("id", String())) == wanted) {
				return wanted;
			}
		}
		for (int i = 0; i < voices.size(); i++) {
			const Dictionary &voice = voices[i];
			if (String(voice.get("name", String())) == wanted) {
				return voice.get("id", String());
			}
		}
	}

	// 2. A voice for the language of the persona.
	String found = _find_voice_for_language(voices, persona->get_language());
	if (!found.is_empty()) {
		return found;
	}

	// 3. A voice for the language of the system.
	found = _find_voice_for_language(voices, OS::get_singleton()->get_locale());
	if (!found.is_empty()) {
		return found;
	}

	// 4. The first voice of the system.
	const Dictionary &first = voices[0];
	return first.get("id", String());
}

void TtsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &TtsServer::is_available);

	ClassDB::bind_method(D_METHOD("speak", "text", "persona"), &TtsServer::speak, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("stop"), &TtsServer::stop);
	ClassDB::bind_method(D_METHOD("pause"), &TtsServer::pause);
	ClassDB::bind_method(D_METHOD("resume"), &TtsServer::resume);
	ClassDB::bind_method(D_METHOD("is_speaking"), &TtsServer::is_speaking);
	ClassDB::bind_method(D_METHOD("is_paused"), &TtsServer::is_paused);
	ClassDB::bind_method(D_METHOD("get_pending_utterances"), &TtsServer::get_pending_utterances);

	ClassDB::bind_method(D_METHOD("add_persona", "persona"), &TtsServer::add_persona);
	ClassDB::bind_method(D_METHOD("remove_persona", "name"), &TtsServer::remove_persona);
	ClassDB::bind_method(D_METHOD("has_persona", "name"), &TtsServer::has_persona);
	ClassDB::bind_method(D_METHOD("get_persona", "name"), &TtsServer::get_persona);
	ClassDB::bind_method(D_METHOD("get_personas"), &TtsServer::get_personas);
	ClassDB::bind_method(D_METHOD("get_persona_names"), &TtsServer::get_persona_names);
	ClassDB::bind_method(D_METHOD("clear_personas"), &TtsServer::clear_personas);

	ClassDB::bind_method(D_METHOD("set_default_persona", "name"), &TtsServer::set_default_persona);
	ClassDB::bind_method(D_METHOD("get_default_persona"), &TtsServer::get_default_persona);
	ClassDB::bind_method(D_METHOD("get_default_persona_name"), &TtsServer::get_default_persona_name);

	ClassDB::bind_method(D_METHOD("get_voices", "language"), &TtsServer::get_voices, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("resolve_voice", "persona"), &TtsServer::resolve_voice, DEFVAL(Variant()));

	ADD_SIGNAL(MethodInfo("utterance_started", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("utterance_ended", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("utterance_canceled", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("utterance_boundary", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "char_pos")));
}

TtsServer::TtsServer() {
	singleton = this;
}

TtsServer::~TtsServer() {
	singleton = nullptr;
	personas.clear();
	builtin_persona.unref();
}
