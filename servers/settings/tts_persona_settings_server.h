/**************************************************************************/
/*  tts_persona_settings_server.h                                         */
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

#include "core/templates/hash_set.h"
#include "servers/settings/list_settings_server.h"
#include "servers/settings/tts_persona_settings.h"

// Settings domain `tts_personas`: the personas the player created or changed, and the player's default persona
// (TTSPersonaListSettings, one row per persona). Applied to TTSServer as persona overrides, so the personas of
// the game stay unchanged and come back when the player's version is removed.
class TTSPersonaSettingsServer : public ListSettingsServer {
	GDCLASS(TTSPersonaSettingsServer, ListSettingsServer);

	static TTSPersonaSettingsServer *singleton;

	HashSet<String> warned_voices; // "<persona>/<voice>" pairs reported once as missing.

	Ref<TTSPersonaSettings> _get_entry_by_name(const StringName &p_name) const;
	static PackedInt32Array _find_missing_voices(const Array &p_entries);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;

public:
	static TTSPersonaSettingsServer *get_singleton();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual StringName get_list_property() const override;
	virtual StringName get_entry_key() const override;

	PackedStringArray get_persona_names() const;
	bool has_persona(const StringName &p_name) const;
	void add_persona(const Ref<TTSPersona> &p_persona);
	Ref<TTSPersona> get_persona(const StringName &p_name) const;
	void remove_persona(const StringName &p_name);
	void set_persona_value(const StringName &p_name, const StringName &p_property, const Variant &p_value);
	Variant get_persona_value(const StringName &p_name, const StringName &p_property) const;

	void set_default_persona(const StringName &p_name);
	StringName get_default_persona() const;

	PackedStringArray get_missing_voice_personas() const;

	TTSPersonaSettingsServer();
	~TTSPersonaSettingsServer();
};
