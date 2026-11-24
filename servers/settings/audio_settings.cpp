/**************************************************************************/
/*  audio_settings.cpp                                                    */
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

#include "audio_settings.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

/////////////////////////////////////////////////////////////////////////////
// AudioDeviceSettings.

void AudioDeviceSettings::set_output_device(const String &p_device) {
	output_device = p_device.is_empty() ? String("Default") : p_device;
	emit_changed();
}

String AudioDeviceSettings::get_output_device() const {
	return output_device;
}

void AudioDeviceSettings::set_input_device(const String &p_device) {
	input_device = p_device.is_empty() ? String("Default") : p_device;
	emit_changed();
}

String AudioDeviceSettings::get_input_device() const {
	return input_device;
}

void AudioDeviceSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_output_device", "device"), &AudioDeviceSettings::set_output_device);
	ClassDB::bind_method(D_METHOD("get_output_device"), &AudioDeviceSettings::get_output_device);
	ClassDB::bind_method(D_METHOD("set_input_device", "device"), &AudioDeviceSettings::set_input_device);
	ClassDB::bind_method(D_METHOD("get_input_device"), &AudioDeviceSettings::get_input_device);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "output_device"), "set_output_device", "get_output_device");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "input_device"), "set_input_device", "get_input_device");
}

/////////////////////////////////////////////////////////////////////////////
// AudioBusSettings.

void AudioBusSettings::set_bus_name(const String &p_name) {
	bus_name = p_name;
	emit_changed();
}

String AudioBusSettings::get_bus_name() const {
	return bus_name;
}

void AudioBusSettings::set_volume_db(float p_volume_db) {
	// Hand-edited files and set_entry_value() bypass the validation of the server, so the value is clamped here.
	volume_db = Math::is_nan(p_volume_db) ? 0.0f : CLAMP(p_volume_db, MIN_VOLUME_DB, MAX_VOLUME_DB);
	emit_changed();
}

float AudioBusSettings::get_volume_db() const {
	return volume_db;
}

void AudioBusSettings::set_volume_linear(float p_volume_linear) {
	set_volume_db(p_volume_linear > 0.0f ? Math::linear_to_db(p_volume_linear) : MIN_VOLUME_DB);
}

float AudioBusSettings::get_volume_linear() const {
	return volume_db <= MIN_VOLUME_DB ? 0.0f : Math::db_to_linear(volume_db);
}

void AudioBusSettings::set_mute(bool p_mute) {
	mute = p_mute;
	emit_changed();
}

bool AudioBusSettings::is_mute() const {
	return mute;
}

void AudioBusSettings::set_solo(bool p_solo) {
	solo = p_solo;
	emit_changed();
}

bool AudioBusSettings::is_solo() const {
	return solo;
}

void AudioBusSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bus_name", "name"), &AudioBusSettings::set_bus_name);
	ClassDB::bind_method(D_METHOD("get_bus_name"), &AudioBusSettings::get_bus_name);
	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioBusSettings::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioBusSettings::get_volume_db);
	ClassDB::bind_method(D_METHOD("set_volume_linear", "volume_linear"), &AudioBusSettings::set_volume_linear);
	ClassDB::bind_method(D_METHOD("get_volume_linear"), &AudioBusSettings::get_volume_linear);
	ClassDB::bind_method(D_METHOD("set_mute", "mute"), &AudioBusSettings::set_mute);
	ClassDB::bind_method(D_METHOD("is_mute"), &AudioBusSettings::is_mute);
	ClassDB::bind_method(D_METHOD("set_solo", "solo"), &AudioBusSettings::set_solo);
	ClassDB::bind_method(D_METHOD("is_solo"), &AudioBusSettings::is_solo);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bus_name"), "set_bus_name", "get_bus_name");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.1,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mute"), "set_mute", "is_mute");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "solo"), "set_solo", "is_solo");
}

/////////////////////////////////////////////////////////////////////////////
// AudioBusListSettings.

void AudioBusListSettings::set_buses(const TypedArray<AudioBusSettings> &p_buses) {
	buses = p_buses;
	emit_changed();
}

TypedArray<AudioBusSettings> AudioBusListSettings::get_buses() const {
	return buses;
}

void AudioBusListSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_buses", "buses"), &AudioBusListSettings::set_buses);
	ClassDB::bind_method(D_METHOD("get_buses"), &AudioBusListSettings::get_buses);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "buses", PROPERTY_HINT_ARRAY_TYPE, "AudioBusSettings"), "set_buses", "get_buses");
}
