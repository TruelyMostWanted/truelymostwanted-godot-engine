/**************************************************************************/
/*  audio_device_settings_server.h                                        */
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

#include "servers/settings/settings_server.h"

// Settings domain `audio`: the output and the input device of AudioServer (AudioDeviceSettings).
// The bus values are a separate list domain (AudioBusSettingsServer), so that list can be stored as CSV.
class AudioDeviceSettingsServer : public SettingsServer {
	GDCLASS(AudioDeviceSettingsServer, SettingsServer);

	static AudioDeviceSettingsServer *singleton;

	static void _push_output_device(const String &p_device);
	static String _pull_output_device();
	static void _push_input_device(const String &p_device);
	static String _pull_input_device();
	static String _validate_device(const String &p_device, bool p_input);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static AudioDeviceSettingsServer *get_singleton();

	// True if the active audio driver can switch devices (WASAPI, PulseAudio, ALSA, CoreAudio on macOS).
	static bool is_device_selection_supported();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;

	void set_output_device(const String &p_device);
	String get_output_device() const;
	void set_input_device(const String &p_device);
	String get_input_device() const;

	AudioDeviceSettingsServer();
	~AudioDeviceSettingsServer();
};
