/**************************************************************************/
/*  engine_settings.h                                                     */
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

// Data of the `engine` settings domain (#26): frame rate limit, time scale, physics rate and interpolation,
// audio playback speed and low-processor mode. Inspector-editable; EngineSettingsServer clamps the values
// and pushes them to Engine, SceneTree, AudioServer and OS. The setters store values as they are, so the
// server can warn about values out of range in a hand-edited file.
class EngineSettings : public Resource {
	GDCLASS(EngineSettings, Resource);

	int max_fps = 0;
	double time_scale = 1.0;
	int physics_ticks_per_second = 60;
	int max_physics_steps_per_frame = 8;
	double physics_jitter_fix = 0.5;
	bool physics_interpolation = false;
	float audio_playback_speed_scale = 1.0f;
	bool low_processor_mode = false;
	int low_processor_mode_sleep_usec = 6900;

protected:
	static void _bind_methods();

public:
	void set_max_fps(int p_max_fps);
	int get_max_fps() const;
	void set_time_scale(double p_time_scale);
	double get_time_scale() const;
	void set_physics_ticks_per_second(int p_ticks_per_second);
	int get_physics_ticks_per_second() const;
	void set_max_physics_steps_per_frame(int p_max_physics_steps);
	int get_max_physics_steps_per_frame() const;
	void set_physics_jitter_fix(double p_threshold);
	double get_physics_jitter_fix() const;
	void set_physics_interpolation_enabled(bool p_enabled);
	bool is_physics_interpolation_enabled() const;
	void set_audio_playback_speed_scale(float p_scale);
	float get_audio_playback_speed_scale() const;
	void set_low_processor_mode_enabled(bool p_enabled);
	bool is_low_processor_mode_enabled() const;
	void set_low_processor_mode_sleep_usec(int p_usec);
	int get_low_processor_mode_sleep_usec() const;
};
