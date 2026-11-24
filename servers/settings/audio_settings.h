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

// Settings of the `audio` domain (AudioSettingsServer): the output and the input device.
class AudioSettings : public Resource {
	GDCLASS(AudioSettings, Resource);

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

// One bus of the `audio_buses` domain (AudioBusSettingsServer), identified by its name.
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

// Settings of the `audio_buses` domain: one entry per bus. It holds nothing but the list, so it can be stored as CSV.
class AudioBusListSettings : public Resource {
	GDCLASS(AudioBusListSettings, Resource);

	TypedArray<AudioBusSettings> buses;

protected:
	static void _bind_methods();

public:
	void set_buses(const TypedArray<AudioBusSettings> &p_buses);
	TypedArray<AudioBusSettings> get_buses() const;
};
