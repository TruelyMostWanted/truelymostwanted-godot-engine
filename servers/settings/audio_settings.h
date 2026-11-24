/**************************************************************************/
/*  audio_settings.h                                                      */
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

// Data of the audio settings domains (#16). No engine dependency: the servers push the values to AudioServer.

// Settings of the `audio_device` domain (AudioDeviceSettingsServer): the output and the input device.
class AudioDeviceSettings : public Resource {
	GDCLASS(AudioDeviceSettings, Resource);

	String output_device = "Default";
	String input_device = "Default";

protected:
	static void _bind_methods();

public:
	void set_output_device(const String &p_device);
	String get_output_device() const;
	void set_input_device(const String &p_device);
	String get_input_device() const;
};

// One bus of the `audio_bus` domain (AudioBusSettingsServer), identified by its name.
class AudioBusSettings : public Resource {
	GDCLASS(AudioBusSettings, Resource);

	String bus_name;
	float volume_db = 0.0f;
	bool mute = false;
	bool solo = false;

protected:
	static void _bind_methods();

public:
	static constexpr float MIN_VOLUME_DB = -80.0f;
	static constexpr float MAX_VOLUME_DB = 24.0f;

	void set_bus_name(const String &p_name);
	String get_bus_name() const;
	void set_volume_db(float p_volume_db);
	float get_volume_db() const;
	void set_volume_linear(float p_volume_linear);
	float get_volume_linear() const;
	void set_mute(bool p_mute);
	bool is_mute() const;
	void set_solo(bool p_solo);
	bool is_solo() const;
};

// Settings of the `audio_bus` domain: one entry per player bus. In `res://` it is also a preset (AudioBusSettingsServer::apply_preset()).
class AudioBusListSettings : public Resource {
	GDCLASS(AudioBusListSettings, Resource);

	TypedArray<AudioBusSettings> buses;

protected:
	static void _bind_methods();

public:
	void set_buses(const TypedArray<AudioBusSettings> &p_buses);
	TypedArray<AudioBusSettings> get_buses() const;
};

// One row of the file of the `audio_bus` domain: the storage form of an AudioBusSettings (#185).
// Only the values that the project stores (`settings/audio_bus/store_*`) are written; while a file is read, every
// value is accepted, and the row remembers which values the file had.
class AudioBusSettingsRow : public Resource {
	GDCLASS(AudioBusSettingsRow, Resource);

public:
	enum StoredValue {
		STORED_VOLUME = 1,
		STORED_MUTE = 2,
		STORED_SOLO = 4,
		STORED_ALL = STORED_VOLUME | STORED_MUTE | STORED_SOLO,
	};

private:
	// The values written to a file (AudioBusSettingsServer sets it while it saves). Static: the CSV writer takes
	// its columns from a new instance of the row class.
	static uint32_t written_values;

	String bus_name;
	float volume_db = 0.0f;
	bool mute = false;
	bool solo = false;
	uint32_t present = 0; // The values set since the row was created (by the reader of a file).

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static void set_written_values(uint32_t p_values);
	static uint32_t get_written_values();

	void set_bus_name(const String &p_name);
	String get_bus_name() const;
	void set_volume_db(float p_volume_db);
	float get_volume_db() const;
	void set_mute(bool p_mute);
	bool is_mute() const;
	void set_solo(bool p_solo);
	bool is_solo() const;

	// True if the value was set (for a row read from a file: the file has the value).
	bool has_value(StoredValue p_value) const;
};

// The file of the `audio_bus` domain: one AudioBusSettingsRow per bus (CSV: one line per bus).
class AudioBusSettingsRows : public Resource {
	GDCLASS(AudioBusSettingsRows, Resource);

	TypedArray<AudioBusSettingsRow> buses;

protected:
	static void _bind_methods();

public:
	void set_buses(const TypedArray<AudioBusSettingsRow> &p_buses);
	TypedArray<AudioBusSettingsRow> get_buses() const;
};
