/**************************************************************************/
/*  engine_settings.cpp                                                   */
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

#include "engine_settings.h"

#include "core/object/class_db.h"

void EngineSettings::set_max_fps(int p_max_fps) {
	max_fps = p_max_fps;
	emit_changed();
}

int EngineSettings::get_max_fps() const {
	return max_fps;
}

void EngineSettings::set_time_scale(double p_time_scale) {
	time_scale = p_time_scale;
	emit_changed();
}

double EngineSettings::get_time_scale() const {
	return time_scale;
}

void EngineSettings::set_physics_ticks_per_second(int p_ticks_per_second) {
	physics_ticks_per_second = p_ticks_per_second;
	emit_changed();
}

int EngineSettings::get_physics_ticks_per_second() const {
	return physics_ticks_per_second;
}

void EngineSettings::set_max_physics_steps_per_frame(int p_max_physics_steps) {
	max_physics_steps_per_frame = p_max_physics_steps;
	emit_changed();
}

int EngineSettings::get_max_physics_steps_per_frame() const {
	return max_physics_steps_per_frame;
}

void EngineSettings::set_physics_jitter_fix(double p_threshold) {
	physics_jitter_fix = p_threshold;
	emit_changed();
}

double EngineSettings::get_physics_jitter_fix() const {
	return physics_jitter_fix;
}

void EngineSettings::set_physics_interpolation_enabled(bool p_enabled) {
	physics_interpolation = p_enabled;
	emit_changed();
}

bool EngineSettings::is_physics_interpolation_enabled() const {
	return physics_interpolation;
}

void EngineSettings::set_audio_playback_speed_scale(float p_scale) {
	audio_playback_speed_scale = p_scale;
	emit_changed();
}

float EngineSettings::get_audio_playback_speed_scale() const {
	return audio_playback_speed_scale;
}

void EngineSettings::set_low_processor_mode_enabled(bool p_enabled) {
	low_processor_mode = p_enabled;
	emit_changed();
}

bool EngineSettings::is_low_processor_mode_enabled() const {
	return low_processor_mode;
}

void EngineSettings::set_low_processor_mode_sleep_usec(int p_usec) {
	low_processor_mode_sleep_usec = p_usec;
	emit_changed();
}

int EngineSettings::get_low_processor_mode_sleep_usec() const {
	return low_processor_mode_sleep_usec;
}

void EngineSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_fps", "max_fps"), &EngineSettings::set_max_fps);
	ClassDB::bind_method(D_METHOD("get_max_fps"), &EngineSettings::get_max_fps);
	ClassDB::bind_method(D_METHOD("set_time_scale", "time_scale"), &EngineSettings::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &EngineSettings::get_time_scale);
	ClassDB::bind_method(D_METHOD("set_physics_ticks_per_second", "ticks_per_second"), &EngineSettings::set_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("get_physics_ticks_per_second"), &EngineSettings::get_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("set_max_physics_steps_per_frame", "max_physics_steps"), &EngineSettings::set_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_physics_steps_per_frame"), &EngineSettings::get_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("set_physics_jitter_fix", "threshold"), &EngineSettings::set_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("get_physics_jitter_fix"), &EngineSettings::get_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("set_physics_interpolation_enabled", "enabled"), &EngineSettings::set_physics_interpolation_enabled);
	ClassDB::bind_method(D_METHOD("is_physics_interpolation_enabled"), &EngineSettings::is_physics_interpolation_enabled);
	ClassDB::bind_method(D_METHOD("set_audio_playback_speed_scale", "scale"), &EngineSettings::set_audio_playback_speed_scale);
	ClassDB::bind_method(D_METHOD("get_audio_playback_speed_scale"), &EngineSettings::get_audio_playback_speed_scale);
	ClassDB::bind_method(D_METHOD("set_low_processor_mode_enabled", "enabled"), &EngineSettings::set_low_processor_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_low_processor_mode_enabled"), &EngineSettings::is_low_processor_mode_enabled);
	ClassDB::bind_method(D_METHOD("set_low_processor_mode_sleep_usec", "usec"), &EngineSettings::set_low_processor_mode_sleep_usec);
	ClassDB::bind_method(D_METHOD("get_low_processor_mode_sleep_usec"), &EngineSettings::get_low_processor_mode_sleep_usec);

	// `or_less`/`or_greater`: the Inspector shows the usual range, EngineSettingsServer clamps with a warning.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_fps", PROPERTY_HINT_RANGE, "0,1000,1,or_greater,or_less,suffix:FPS"), "set_max_fps", "get_max_fps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_scale", PROPERTY_HINT_RANGE, "0.01,100,0.01,or_greater,or_less"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_ticks_per_second", PROPERTY_HINT_RANGE, "1,1000,1,or_greater,or_less"), "set_physics_ticks_per_second", "get_physics_ticks_per_second");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "physics_jitter_fix", PROPERTY_HINT_RANGE, "0,2,0.001,or_greater,or_less"), "set_physics_jitter_fix", "get_physics_jitter_fix");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_interpolation"), "set_physics_interpolation_enabled", "is_physics_interpolation_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_physics_steps_per_frame", PROPERTY_HINT_RANGE, "1,100,1,or_greater,or_less"), "set_max_physics_steps_per_frame", "get_max_physics_steps_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "audio_playback_speed_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater,or_less"), "set_audio_playback_speed_scale", "get_audio_playback_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "low_processor_mode"), "set_low_processor_mode_enabled", "is_low_processor_mode_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "low_processor_mode_sleep_usec", PROPERTY_HINT_RANGE, "0,33200,1,or_greater,or_less"), "set_low_processor_mode_sleep_usec", "get_low_processor_mode_sleep_usec");
}
