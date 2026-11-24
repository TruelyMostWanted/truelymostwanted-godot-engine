/**************************************************************************/
/*  audio_mixer_channel.cpp                                               */
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

#include "audio_mixer_channel.h"

#include "core/object/class_db.h"

// Upper limit for overlapping one-shot voices on one channel.
static constexpr int AUDIO_MIXER_MAX_POLYPHONY = 128;

void AudioMixerChannel::set_channel_name(const StringName &p_name) {
	channel_name = p_name;
	emit_changed();
}

StringName AudioMixerChannel::get_channel_name() const {
	return channel_name;
}

void AudioMixerChannel::set_bus(const StringName &p_bus) {
	bus = p_bus;
	emit_changed();
}

StringName AudioMixerChannel::get_bus() const {
	return bus;
}

void AudioMixerChannel::set_volume_db(float p_volume_db) {
	volume_db = p_volume_db;
	emit_changed();
}

float AudioMixerChannel::get_volume_db() const {
	return volume_db;
}

void AudioMixerChannel::set_pitch_scale(float p_pitch_scale) {
	ERR_FAIL_COND_MSG(p_pitch_scale <= 0.0f, "Pitch scale must be greater than 0.");
	pitch_scale = p_pitch_scale;
	emit_changed();
}

float AudioMixerChannel::get_pitch_scale() const {
	return pitch_scale;
}

void AudioMixerChannel::set_max_polyphony(int p_max_polyphony) {
	max_polyphony = CLAMP(p_max_polyphony, 1, AUDIO_MIXER_MAX_POLYPHONY);
	emit_changed();
}

int AudioMixerChannel::get_max_polyphony() const {
	return max_polyphony;
}

void AudioMixerChannel::set_queue_mode(QueueMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, 3);
	queue_mode = p_mode;
	emit_changed();
}

AudioMixerChannel::QueueMode AudioMixerChannel::get_queue_mode() const {
	return queue_mode;
}

void AudioMixerChannel::set_loop_current_entry(bool p_enabled) {
	loop_current_entry = p_enabled;
	emit_changed();
}

bool AudioMixerChannel::is_loop_current_entry() const {
	return loop_current_entry;
}

void AudioMixerChannel::set_default_fade_time(float p_time) {
	default_fade_time = MAX(0.0f, p_time);
	emit_changed();
}

float AudioMixerChannel::get_default_fade_time() const {
	return default_fade_time;
}

void AudioMixerChannel::set_pause_with_tree(bool p_enabled) {
	pause_with_tree = p_enabled;
	emit_changed();
}

bool AudioMixerChannel::is_pause_with_tree() const {
	return pause_with_tree;
}

void AudioMixerChannel::set_stop_on_scene_change(bool p_enabled) {
	stop_on_scene_change = p_enabled;
	emit_changed();
}

bool AudioMixerChannel::is_stop_on_scene_change() const {
	return stop_on_scene_change;
}

void AudioMixerChannel::set_preload_next(bool p_enabled) {
	preload_next = p_enabled;
	emit_changed();
}

bool AudioMixerChannel::is_preload_next() const {
	return preload_next;
}

void AudioMixerChannel::set_duck_on_tts(bool p_enabled) {
	duck_on_tts = p_enabled;
	emit_changed();
}

bool AudioMixerChannel::is_duck_on_tts() const {
	return duck_on_tts;
}

void AudioMixerChannel::set_duck_volume_db(float p_volume_db) {
	// Ducking only lowers the volume.
	duck_volume_db = CLAMP(p_volume_db, -80.0f, 0.0f);
	emit_changed();
}

float AudioMixerChannel::get_duck_volume_db() const {
	return duck_volume_db;
}

void AudioMixerChannel::set_duck_fade_time(float p_time) {
	duck_fade_time = MAX(0.0f, p_time);
	emit_changed();
}

float AudioMixerChannel::get_duck_fade_time() const {
	return duck_fade_time;
}

void AudioMixerChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel_name", "name"), &AudioMixerChannel::set_channel_name);
	ClassDB::bind_method(D_METHOD("get_channel_name"), &AudioMixerChannel::get_channel_name);
	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &AudioMixerChannel::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &AudioMixerChannel::get_bus);
	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioMixerChannel::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioMixerChannel::get_volume_db);
	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &AudioMixerChannel::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioMixerChannel::get_pitch_scale);
	ClassDB::bind_method(D_METHOD("set_max_polyphony", "max_polyphony"), &AudioMixerChannel::set_max_polyphony);
	ClassDB::bind_method(D_METHOD("get_max_polyphony"), &AudioMixerChannel::get_max_polyphony);
	ClassDB::bind_method(D_METHOD("set_queue_mode", "mode"), &AudioMixerChannel::set_queue_mode);
	ClassDB::bind_method(D_METHOD("get_queue_mode"), &AudioMixerChannel::get_queue_mode);
	ClassDB::bind_method(D_METHOD("set_loop_current_entry", "enabled"), &AudioMixerChannel::set_loop_current_entry);
	ClassDB::bind_method(D_METHOD("is_loop_current_entry"), &AudioMixerChannel::is_loop_current_entry);
	ClassDB::bind_method(D_METHOD("set_default_fade_time", "time"), &AudioMixerChannel::set_default_fade_time);
	ClassDB::bind_method(D_METHOD("get_default_fade_time"), &AudioMixerChannel::get_default_fade_time);
	ClassDB::bind_method(D_METHOD("set_pause_with_tree", "enabled"), &AudioMixerChannel::set_pause_with_tree);
	ClassDB::bind_method(D_METHOD("is_pause_with_tree"), &AudioMixerChannel::is_pause_with_tree);
	ClassDB::bind_method(D_METHOD("set_stop_on_scene_change", "enabled"), &AudioMixerChannel::set_stop_on_scene_change);
	ClassDB::bind_method(D_METHOD("is_stop_on_scene_change"), &AudioMixerChannel::is_stop_on_scene_change);
	ClassDB::bind_method(D_METHOD("set_preload_next", "enabled"), &AudioMixerChannel::set_preload_next);
	ClassDB::bind_method(D_METHOD("is_preload_next"), &AudioMixerChannel::is_preload_next);
	ClassDB::bind_method(D_METHOD("set_duck_on_tts", "enabled"), &AudioMixerChannel::set_duck_on_tts);
	ClassDB::bind_method(D_METHOD("is_duck_on_tts"), &AudioMixerChannel::is_duck_on_tts);
	ClassDB::bind_method(D_METHOD("set_duck_volume_db", "volume_db"), &AudioMixerChannel::set_duck_volume_db);
	ClassDB::bind_method(D_METHOD("get_duck_volume_db"), &AudioMixerChannel::get_duck_volume_db);
	ClassDB::bind_method(D_METHOD("set_duck_fade_time", "time"), &AudioMixerChannel::set_duck_fade_time);
	ClassDB::bind_method(D_METHOD("get_duck_fade_time"), &AudioMixerChannel::get_duck_fade_time);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "channel_name"), "set_channel_name", "get_channel_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_AUDIO_BUS), "set_bus", "get_bus");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.01,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_polyphony", PROPERTY_HINT_RANGE, "1,128,1"), "set_max_polyphony", "get_max_polyphony");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "queue_mode", PROPERTY_HINT_ENUM, "Stop,Loop Queue,Shuffle"), "set_queue_mode", "get_queue_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop_current_entry"), "set_loop_current_entry", "is_loop_current_entry");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_fade_time", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater,suffix:s"), "set_default_fade_time", "get_default_fade_time");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pause_with_tree"), "set_pause_with_tree", "is_pause_with_tree");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stop_on_scene_change"), "set_stop_on_scene_change", "is_stop_on_scene_change");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preload_next"), "set_preload_next", "is_preload_next");

	ADD_GROUP("Ducking", "duck_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "duck_on_tts"), "set_duck_on_tts", "is_duck_on_tts");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duck_volume_db", PROPERTY_HINT_RANGE, "-80,0,0.01,suffix:dB"), "set_duck_volume_db", "get_duck_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duck_fade_time", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater,suffix:s"), "set_duck_fade_time", "get_duck_fade_time");

	BIND_ENUM_CONSTANT(QUEUE_MODE_STOP);
	BIND_ENUM_CONSTANT(QUEUE_MODE_LOOP_QUEUE);
	BIND_ENUM_CONSTANT(QUEUE_MODE_SHUFFLE);
}
