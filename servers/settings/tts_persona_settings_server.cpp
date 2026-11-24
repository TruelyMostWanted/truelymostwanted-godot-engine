/**************************************************************************/
/*  tts_persona_settings_server.cpp                                       */
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

#include "tts_persona_settings_server.h"

#include "core/object/class_db.h"
#include "servers/tts/tts_server.h"

TTSPersonaSettingsServer *TTSPersonaSettingsServer::singleton = nullptr;

TTSPersonaSettingsServer *TTSPersonaSettingsServer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName TTSPersonaSettingsServer::get_domain() const {
	return SNAME("tts_personas");
}

SettingsServer::ApplyPhase TTSPersonaSettingsServer::get_apply_phase() const {
	// TTSServer exists from register_server_types(); the voices are only checked, never needed, so the DisplayServer may come later.
	return APPLY_PHASE_SERVERS;
}

StringName TTSPersonaSettingsServer::get_list_property() const {
	return SNAME("personas");
}

StringName TTSPersonaSettingsServer::get_entry_key() const {
	return SNAME("persona_name");
}

Ref<Resource> TTSPersonaSettingsServer::_create_default_settings() const {
	// Empty: the domain stores only what the player created or changed. The personas of the game live in TTSServer.
	Ref<TTSPersonaListSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

// The indices of the entries whose voice is set but is not one of the voices of this device. Empty while the
// device reports no voices at all (text to speech unavailable, or the Web voice list is still loading).
PackedInt32Array TTSPersonaSettingsServer::_find_missing_voices(const Array &p_entries) {
	PackedInt32Array missing;
	const TTSServer *tts_server = TTSServer::get_singleton();
	if (tts_server == nullptr) {
		return missing;
	}
	const TypedArray<Dictionary> voices = tts_server->get_voices();
	if (voices.is_empty()) {
		return missing;
	}
	for (int i = 0; i < p_entries.size(); i++) {
		const Ref<TTSPersonaSettings> entry = p_entries[i];
		if (entry.is_null() || entry->get_voice().is_empty()) {
			continue;
		}
		const String wanted = entry->get_voice();
		bool found = false;
		for (int j = 0; j < voices.size() && !found; j++) {
			const Dictionary &voice = voices[j];
			found = String(voice.get("id", String())) == wanted || String(voice.get("name", String())) == wanted;
		}
		if (!found) {
			missing.push_back(i);
		}
	}
	return missing;
}

void TTSPersonaSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	const StringName property = get_list_property();
	TTSServer *tts_server = TTSServer::get_singleton();
	if (tts_server == nullptr || p_settings.is_null() || !p_names.has(String(property))) {
		return;
	}
	// The overrides of TTSServer belong to this domain: replaced as a whole.
	const Array entries = p_settings->get(property);
	tts_server->clear_persona_overrides();
	StringName default_name;
	for (int i = 0; i < entries.size(); i++) {
		const Ref<TTSPersonaSettings> entry = entries[i];
		if (entry.is_null() || entry->get_persona_name().is_empty()) {
			continue;
		}
		tts_server->set_persona_override(entry->to_persona());
		// A hand-edited file may mark several personas as default: the first one wins.
		if (entry->is_default() && default_name.is_empty()) {
			default_name = entry->get_persona_name();
		}
	}
	tts_server->set_default_persona_override(default_name);

	// A missing voice is not an error: the persona is kept (the voice may exist on the player's other device)
	// and TTSServer::resolve_voice_id() falls back to a voice of the persona's language.
	const PackedInt32Array missing = _find_missing_voices(entries);
	PackedStringArray reported;
	for (const int index : missing) {
		const Ref<TTSPersonaSettings> entry = entries[index];
		const String warned_key = String(entry->get_persona_name()) + "/" + entry->get_voice();
		if (!warned_voices.has(warned_key)) {
			warned_voices.insert(warned_key);
			reported.push_back(vformat("\"%s\" (voice \"%s\")", entry->get_persona_name(), entry->get_voice()));
		}
	}
	if (!reported.is_empty()) {
		WARN_PRINT(vformat("TTS persona settings: the voice of the persona %s is not available on this device; a voice for the persona's language is used instead.", String(", ").join(reported)));
	}
}

Ref<TTSPersonaSettings> TTSPersonaSettingsServer::_get_entry_by_name(const StringName &p_name) const {
	const int index = find_entry(p_name);
	if (index < 0) {
		return Ref<TTSPersonaSettings>();
	}
	return get_entry(index);
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

PackedStringArray TTSPersonaSettingsServer::get_persona_names() const {
	PackedStringArray names;
	const TypedArray<Resource> entries = get_entries();
	for (int i = 0; i < entries.size(); i++) {
		const Ref<TTSPersonaSettings> entry = entries[i];
		if (entry.is_valid()) {
			names.push_back(entry->get_persona_name());
		}
	}
	return names;
}

bool TTSPersonaSettingsServer::has_persona(const StringName &p_name) const {
	return find_entry(p_name) >= 0;
}

void TTSPersonaSettingsServer::add_persona(const Ref<TTSPersona> &p_persona) {
	ERR_FAIL_COND_MSG(p_persona.is_null(), "TTS persona settings: cannot add a null persona.");
	ERR_FAIL_COND_MSG(p_persona->get_persona_name().is_empty(), "TTS persona settings: cannot add a persona without a name.");
	const Ref<TTSPersonaSettings> existing = _get_entry_by_name(p_persona->get_persona_name());
	if (existing.is_valid()) {
		// Replaced in place: the position and the default flag stay.
		existing->copy_from_persona(p_persona);
		notify_value_changed(get_list_property());
		return;
	}
	Ref<TTSPersonaSettings> entry;
	entry.instantiate();
	entry->copy_from_persona(p_persona);
	add_entry(entry);
}

Ref<TTSPersona> TTSPersonaSettingsServer::get_persona(const StringName &p_name) const {
	const Ref<TTSPersonaSettings> entry = _get_entry_by_name(p_name);
	if (entry.is_null()) {
		return Ref<TTSPersona>();
	}
	return entry->to_persona();
}

void TTSPersonaSettingsServer::remove_persona(const StringName &p_name) {
	const int index = find_entry(p_name);
	if (index >= 0) {
		remove_entry(index);
	}
}

void TTSPersonaSettingsServer::set_persona_value(const StringName &p_name, const StringName &p_property, const Variant &p_value) {
	if (p_property == SNAME("default")) {
		if (bool(p_value)) {
			set_default_persona(p_name);
		} else if (get_default_persona() == p_name) {
			set_default_persona(StringName());
		}
		return;
	}
	ERR_FAIL_COND_MSG(p_property == get_entry_key(), "TTS persona settings: the name of a persona can't be changed; add a new persona instead.");
	const int index = find_entry(p_name);
	if (index >= 0) {
		set_entry_value(index, p_property, p_value);
		return;
	}
	// A persona of the game without an entry yet: the player's version starts from the values of the game.
	const TTSServer *tts_server = TTSServer::get_singleton();
	const Ref<TTSPersona> base = tts_server != nullptr ? tts_server->get_persona(p_name) : Ref<TTSPersona>();
	ERR_FAIL_COND_MSG(base.is_null(), vformat("TTS persona settings: there is no persona \"%s\".", p_name));
	Ref<TTSPersonaSettings> entry;
	entry.instantiate();
	entry->copy_from_persona(base);
	bool valid = false;
	entry->get(p_property, &valid);
	ERR_FAIL_COND_MSG(!valid, vformat("TTS persona settings: a persona has no value \"%s\".", p_property));
	entry->set(p_property, validate_property_value(entry.ptr(), p_property, p_value));
	add_entry(entry);
}

Variant TTSPersonaSettingsServer::get_persona_value(const StringName &p_name, const StringName &p_property) const {
	const int index = find_entry(p_name);
	if (index >= 0) {
		return get_entry_value(index, p_property);
	}
	const TTSServer *tts_server = TTSServer::get_singleton();
	const Ref<TTSPersona> base = tts_server != nullptr ? tts_server->get_persona(p_name) : Ref<TTSPersona>();
	ERR_FAIL_COND_V_MSG(base.is_null(), Variant(), vformat("TTS persona settings: there is no persona \"%s\".", p_name));
	if (p_property == SNAME("default")) {
		return false;
	}
	return base->get(p_property);
}

void TTSPersonaSettingsServer::set_default_persona(const StringName &p_name) {
	const int index = p_name.is_empty() ? -1 : find_entry(p_name);
	Ref<TTSPersonaSettings> new_entry;
	if (!p_name.is_empty() && index < 0) {
		// A persona of the game becomes the default: the player's entry starts from its values.
		const TTSServer *tts_server = TTSServer::get_singleton();
		const Ref<TTSPersona> base = tts_server != nullptr ? tts_server->get_persona(p_name) : Ref<TTSPersona>();
		ERR_FAIL_COND_MSG(base.is_null(), vformat("TTS persona settings: there is no persona \"%s\".", p_name));
		new_entry.instantiate();
		new_entry->copy_from_persona(base);
		new_entry->set_default(true);
	}
	// At most one entry is the default. The flags are changed in place; the list is applied once below.
	bool changed = false;
	const TypedArray<Resource> entries = get_entries();
	for (int i = 0; i < entries.size(); i++) {
		const Ref<TTSPersonaSettings> entry = entries[i];
		if (entry.is_null()) {
			continue;
		}
		const bool is_new_default = i == index;
		if (entry->is_default() != is_new_default) {
			entry->set_default(is_new_default);
			changed = true;
		}
	}
	if (new_entry.is_valid()) {
		add_entry(new_entry);
	} else if (changed) {
		notify_value_changed(get_list_property());
	}
}

StringName TTSPersonaSettingsServer::get_default_persona() const {
	const TypedArray<Resource> entries = get_entries();
	for (int i = 0; i < entries.size(); i++) {
		const Ref<TTSPersonaSettings> entry = entries[i];
		if (entry.is_valid() && entry->is_default()) {
			return entry->get_persona_name();
		}
	}
	return StringName();
}

PackedStringArray TTSPersonaSettingsServer::get_missing_voice_personas() const {
	PackedStringArray names;
	const TypedArray<Resource> entries = get_entries();
	const PackedInt32Array missing = _find_missing_voices(entries);
	for (const int index : missing) {
		const Ref<TTSPersonaSettings> entry = entries[index];
		names.push_back(entry->get_persona_name());
	}
	return names;
}

void TTSPersonaSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_persona_names"), &TTSPersonaSettingsServer::get_persona_names);
	ClassDB::bind_method(D_METHOD("has_persona", "name"), &TTSPersonaSettingsServer::has_persona);
	ClassDB::bind_method(D_METHOD("add_persona", "persona"), &TTSPersonaSettingsServer::add_persona);
	ClassDB::bind_method(D_METHOD("get_persona", "name"), &TTSPersonaSettingsServer::get_persona);
	ClassDB::bind_method(D_METHOD("remove_persona", "name"), &TTSPersonaSettingsServer::remove_persona);
	ClassDB::bind_method(D_METHOD("set_persona_value", "name", "property", "value"), &TTSPersonaSettingsServer::set_persona_value);
	ClassDB::bind_method(D_METHOD("get_persona_value", "name", "property"), &TTSPersonaSettingsServer::get_persona_value);
	ClassDB::bind_method(D_METHOD("set_default_persona", "name"), &TTSPersonaSettingsServer::set_default_persona);
	ClassDB::bind_method(D_METHOD("get_default_persona"), &TTSPersonaSettingsServer::get_default_persona);
	ClassDB::bind_method(D_METHOD("get_missing_voice_personas"), &TTSPersonaSettingsServer::get_missing_voice_personas);
}

TTSPersonaSettingsServer::TTSPersonaSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
}

TTSPersonaSettingsServer::~TTSPersonaSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
