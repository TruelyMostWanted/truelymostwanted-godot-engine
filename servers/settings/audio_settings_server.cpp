/**************************************************************************/
/*  audio_settings_server.cpp                                             */
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

#include "audio_settings_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/audio/audio_driver.h"
#include "servers/audio/audio_server.h"
#include "servers/settings/audio_settings.h"

static constexpr const char *DEFAULT_DEVICE = "Default";

AudioSettingsServer *AudioSettingsServer::singleton = nullptr;

AudioSettingsServer *AudioSettingsServer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets. Without an AudioServer (headless tools, unit tests) nothing is pushed.

void AudioSettingsServer::_push_output_device(const String &p_device) {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server != nullptr) {
		audio_server->set_output_device(p_device);
	}
}

String AudioSettingsServer::_pull_output_device() {
	AudioServer *audio_server = AudioServer::get_singleton();
	return audio_server != nullptr ? audio_server->get_output_device() : String(DEFAULT_DEVICE);
}

void AudioSettingsServer::_push_input_device(const String &p_device) {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server != nullptr) {
		audio_server->set_input_device(p_device);
	}
}

String AudioSettingsServer::_pull_input_device() {
	AudioServer *audio_server = AudioServer::get_singleton();
	return audio_server != nullptr ? audio_server->get_input_device() : String(DEFAULT_DEVICE);
}

String AudioSettingsServer::_validate_device(const String &p_device, bool p_input) {
	if (p_device.is_empty() || p_device == DEFAULT_DEVICE) {
		return DEFAULT_DEVICE;
	}
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || AudioDriver::get_singleton() == nullptr) {
		// Can't be checked yet; the value is checked again when it is loaded or set with an AudioServer.
		return p_device;
	}
	const PackedStringArray devices = p_input ? audio_server->get_input_device_list() : audio_server->get_output_device_list();
	if (devices.has(p_device)) {
		return p_device;
	}
	// A device that was unplugged or belongs to another machine or driver: the normal case, no error.
	print_verbose(vformat("Audio settings: the %s device \"%s\" is not available; \"%s\" is used.", p_input ? "input" : "output", p_device, DEFAULT_DEVICE));
	return DEFAULT_DEVICE;
}

bool AudioSettingsServer::is_device_selection_supported() {
	const AudioDriver *driver = AudioDriver::get_singleton();
	if (driver == nullptr) {
		return false;
	}
	const String driver_name = driver->get_name();
	if (driver_name == "WASAPI" || driver_name == "PulseAudio" || driver_name == "ALSA") {
		return true;
	}
	// The CoreAudio driver lists devices only on macOS, not on iOS.
	return driver_name == "CoreAudio" && OS::get_singleton()->get_name() == "macOS";
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName AudioSettingsServer::get_domain() const {
	return SNAME("audio");
}

SettingsServer::ApplyPhase AudioSettingsServer::get_apply_phase() const {
	// After AudioServer::init(), before the SceneTree exists.
	return APPLY_PHASE_SERVERS;
}

Ref<Resource> AudioSettingsServer::_create_default_settings() const {
	Ref<AudioSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

Variant AudioSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (p_name == SNAME("output_device")) {
		return _validate_device(value, false);
	}
	if (p_name == SNAME("input_device")) {
		return _validate_device(value, true);
	}
	return value;
}

bool AudioSettingsServer::is_value_supported(const StringName &p_name) const {
	if (!SettingsServer::is_value_supported(p_name)) {
		return false;
	}
	if (p_name == SNAME("output_device")) {
		return is_device_selection_supported();
	}
	if (p_name == SNAME("input_device")) {
		const ProjectSettings *project_settings = ProjectSettings::get_singleton();
		const bool input_enabled = project_settings != nullptr && project_settings->has_setting("audio/driver/enable_input") && bool(project_settings->get_setting_with_override("audio/driver/enable_input"));
		return input_enabled && is_device_selection_supported();
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void AudioSettingsServer::set_output_device(const String &p_device) {
	set_value(SNAME("output_device"), p_device);
}

String AudioSettingsServer::get_output_device() const {
	return get_value(SNAME("output_device"));
}

void AudioSettingsServer::set_input_device(const String &p_device) {
	set_value(SNAME("input_device"), p_device);
}

String AudioSettingsServer::get_input_device() const {
	return get_value(SNAME("input_device"));
}

void AudioSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_output_device", "device"), &AudioSettingsServer::set_output_device);
	ClassDB::bind_method(D_METHOD("get_output_device"), &AudioSettingsServer::get_output_device);
	ClassDB::bind_method(D_METHOD("set_input_device", "device"), &AudioSettingsServer::set_input_device);
	ClassDB::bind_method(D_METHOD("get_input_device"), &AudioSettingsServer::get_input_device);
	ClassDB::bind_static_method("AudioSettingsServer", D_METHOD("is_device_selection_supported"), &AudioSettingsServer::is_device_selection_supported);
}

AudioSettingsServer::AudioSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	bind_setting_callbacks(SNAME("output_device"), callable_mp_static(&AudioSettingsServer::_push_output_device), callable_mp_static(&AudioSettingsServer::_pull_output_device));
	bind_setting_callbacks(SNAME("input_device"), callable_mp_static(&AudioSettingsServer::_push_input_device), callable_mp_static(&AudioSettingsServer::_pull_input_device));
}

AudioSettingsServer::~AudioSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
