/**************************************************************************/
/*  tts_server.h                                                          */
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
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/typed_array.h"
#include "servers/tts/tts_persona.h"

// Text to speech in one place. Speaks text with a TTSPersona, keeps a list of named personas
// and sends one signal per utterance event. The speech itself is done by DisplayServer::tts_*(),
// which stays available as before; TTSServer only wraps it.
class TTSServer : public Object {
	GDCLASS(TTSServer, Object);

	static TTSServer *singleton;

	mutable Mutex mutex;
	HashMap<StringName, Ref<TTSPersona>> personas;
	StringName default_persona_name;
	// The player's versions of personas (TTSPersonaSettingsServer): seen before the personas of the game.
	HashMap<StringName, Ref<TTSPersona>> persona_overrides;
	StringName default_persona_override;
	mutable Ref<TTSPersona> builtin_persona;
	HashSet<int64_t> pending;
	int64_t next_utterance_id = 1;

	static String _normalize_language(const String &p_language);
	static bool _language_matches(const String &p_voice_language, const String &p_language);
	static String _find_voice_for_language(const TypedArray<Dictionary> &p_voices, const String &p_language);
	String _get_unavailable_reason() const;
	Ref<TTSPersona> _get_persona_locked(const StringName &p_name) const;
	StringName _get_default_persona_name_locked() const;
	Ref<TTSPersona> _get_default_persona_locked() const;

	void _emit_utterance_event(int p_event, int64_t p_id, int p_char_pos);

protected:
	static void _bind_methods();

public:
	static TTSServer *get_singleton();

	// Availability.
	bool is_available() const;

	// Speech.
	int64_t speak(const String &p_text, const Ref<TTSPersona> &p_persona = Ref<TTSPersona>());
	int64_t speak_with_persona(const String &p_text, const StringName &p_persona_name);
	void stop();
	void pause();
	void resume();
	bool is_speaking() const;
	bool is_paused() const;
	PackedInt64Array get_pending_utterances() const;

	// Personas.
	void add_persona(const Ref<TTSPersona> &p_persona);
	void remove_persona(const StringName &p_name);
	bool has_persona(const StringName &p_name) const;
	Ref<TTSPersona> get_persona(const StringName &p_name) const;
	TypedArray<TTSPersona> get_personas() const;
	PackedStringArray get_persona_names() const;
	void clear_personas();

	void set_default_persona(const StringName &p_name);
	Ref<TTSPersona> get_default_persona() const;
	StringName get_default_persona_name() const;

	// The player's overrides (stored by TTSPersonaSettingsServer).
	void set_persona_override(const Ref<TTSPersona> &p_persona);
	void remove_persona_override(const StringName &p_name);
	bool has_persona_override(const StringName &p_name) const;
	Ref<TTSPersona> get_persona_override(const StringName &p_name) const;
	TypedArray<TTSPersona> get_persona_overrides() const;
	void clear_persona_overrides();
	Ref<TTSPersona> get_base_persona(const StringName &p_name) const;

	void set_default_persona_override(const StringName &p_name);
	StringName get_default_persona_override() const;

	// Voices.
	TypedArray<Dictionary> get_voices(const String &p_language = String()) const;
	String resolve_voice_id(const Ref<TTSPersona> &p_persona = Ref<TTSPersona>()) const;

	// Called by DisplayServer::tts_post_utterance_event(), from any thread.
	void post_utterance_event(int p_event, int64_t p_id, int p_char_pos);

	TTSServer();
	~TTSServer();
};
