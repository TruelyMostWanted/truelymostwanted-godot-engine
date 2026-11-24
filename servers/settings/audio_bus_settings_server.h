/**************************************************************************/
/*  audio_bus_settings_server.h                                           */
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
#include "servers/settings/audio_settings.h"
#include "servers/settings/list_settings_server.h"

// Settings domain `audio_bus`: the values the player may change on the buses of AudioServer (AudioBusListSettings).
// The project's AudioBusLayout is the only source of buses, effects, routing and default values (#185): the domain
// stores volume, mute and solo of the player buses (`settings/audio_bus/player_buses`, empty = all buses), and only
// the values whose `settings/audio_bus/store_*` setting is on. Buses are identified by name, so a changed bus layout
// keeps the values of the buses it still has; a bus that the layout does not have is skipped (its entry stays).
class AudioBusSettingsServer : public ListSettingsServer {
	GDCLASS(AudioBusSettingsServer, ListSettingsServer);

	static AudioBusSettingsServer *singleton;

	PackedStringArray player_buses;
	bool store_volume = true;
	bool store_mute = true;
	bool store_solo = false;

	HashSet<String> warned_buses; // Buses missing in the layout that were reported once.
	Array signaled_buses; // The bus entries the typed signals last reported (or read from the engine).
	Array layout_buses; // The values of the layout, read from AudioServer when the defaults were captured.
	Ref<AudioBusListSettings> current_preset; // The preset of the last apply_preset().

	uint32_t _get_stored_values() const;
	Ref<AudioBusSettings> _get_layout_entry(const String &p_bus) const;
	Array _make_layout_entries() const;
	void _on_loaded();
	void _on_save_finished();
	void _connect_audio_server();
	void _on_bus_layout_changed();
	bool _check_settable(const StringName &p_bus, bool p_stored, const char *p_value) const;
	void _set_bus_value(const StringName &p_bus, const StringName &p_name, const Variant &p_value);
	Variant _get_bus_value(const StringName &p_bus, const StringName &p_name) const;

protected:
	static void _bind_methods();

	// Compares the applied buses with the last reported ones and emits the typed bus signals.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	// The file holds AudioBusSettingsRows: only the stored values are written.
	virtual Ref<Resource> _settings_to_storage(const Ref<Resource> &p_settings) const override;
	virtual Ref<Resource> _settings_from_storage(const Ref<Resource> &p_storage) const override;

public:
	static AudioBusSettingsServer *get_singleton();
	static void register_settings();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual StringName get_list_property() const override;
	virtual StringName get_entry_key() const override;

	void set_player_buses(const PackedStringArray &p_buses);
	PackedStringArray get_player_buses() const;
	bool is_player_bus(const StringName &p_bus) const;
	void set_store_volume(bool p_enabled);
	bool is_storing_volume() const;
	void set_store_mute(bool p_enabled);
	bool is_storing_mute() const;
	void set_store_solo(bool p_enabled);
	bool is_storing_solo() const;

	PackedStringArray get_bus_names() const;
	bool has_bus(const StringName &p_bus) const;

	void set_bus_volume_db(const StringName &p_bus, float p_volume_db);
	float get_bus_volume_db(const StringName &p_bus) const;
	void set_bus_volume_linear(const StringName &p_bus, float p_volume_linear);
	float get_bus_volume_linear(const StringName &p_bus) const;
	void set_bus_mute(const StringName &p_bus, bool p_mute);
	bool is_bus_mute(const StringName &p_bus) const;
	void set_bus_solo(const StringName &p_bus, bool p_solo);
	bool is_bus_solo(const StringName &p_bus) const;

	// Presets: an AudioBusListSettings in `res://`. Buses that a preset does not list keep their values.
	Error apply_preset(const Ref<AudioBusListSettings> &p_preset);
	bool matches_preset(const Ref<AudioBusListSettings> &p_preset) const;
	Ref<AudioBusListSettings> get_current_preset() const;
	bool is_custom() const;

	AudioBusSettingsServer();
	~AudioBusSettingsServer();
};
