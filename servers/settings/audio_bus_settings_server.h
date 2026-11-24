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

// Settings domain `audio_buses`: volume, mute and solo per bus of AudioServer (AudioBusListSettings).
// Buses are identified by name, so a changed bus layout keeps the values of the buses it still has.
class AudioBusSettingsServer : public ListSettingsServer {
	GDCLASS(AudioBusSettingsServer, ListSettingsServer);

	static AudioBusSettingsServer *singleton;

	bool create_missing_buses = false;
	HashSet<String> warned_buses; // Buses missing in the layout that were reported once.

	static Ref<AudioBusSettings> _make_entry_from_engine(int p_bus);
	void _merge_layout_buses();
	void _set_bus_value(const StringName &p_bus, const StringName &p_name, const Variant &p_value);
	Variant _get_bus_value(const StringName &p_bus, const StringName &p_name) const;

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;
	virtual void _retrieve_settings(const Ref<Resource> &p_settings) override;

public:
	static AudioBusSettingsServer *get_singleton();
	static void register_settings();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual StringName get_list_property() const override;
	virtual StringName get_entry_key() const override;

	void set_create_missing_buses(bool p_enabled);
	bool is_create_missing_buses_enabled() const;

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

	AudioBusSettingsServer();
	~AudioBusSettingsServer();
};
