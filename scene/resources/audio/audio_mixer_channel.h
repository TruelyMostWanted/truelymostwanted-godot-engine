/**************************************************************************/
/*  audio_mixer_channel.h                                                 */
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

// The definition of one AudioMixerServer channel (for example "music" or "ambience").
// A channel plays on an existing audio bus; the mixer never creates or removes buses.
class AudioMixerChannel : public Resource {
	GDCLASS(AudioMixerChannel, Resource);

public:
	enum QueueMode {
		QUEUE_MODE_STOP,
		QUEUE_MODE_LOOP_QUEUE,
		QUEUE_MODE_SHUFFLE,
	};

private:
	StringName channel_name;
	StringName bus = "Master";
	float volume_db = 0.0f;
	float pitch_scale = 1.0f;
	int max_polyphony = 1;
	QueueMode queue_mode = QUEUE_MODE_STOP;
	bool loop_current_entry = false;
	float default_fade_time = 0.0f;
	bool pause_with_tree = false;
	bool stop_on_scene_change = false;
	bool preload_next = false;

protected:
	static void _bind_methods();

public:
	void set_channel_name(const StringName &p_name);
	StringName get_channel_name() const;

	void set_bus(const StringName &p_bus);
	StringName get_bus() const;

	void set_volume_db(float p_volume_db);
	float get_volume_db() const;

	void set_pitch_scale(float p_pitch_scale);
	float get_pitch_scale() const;

	void set_max_polyphony(int p_max_polyphony);
	int get_max_polyphony() const;

	void set_queue_mode(QueueMode p_mode);
	QueueMode get_queue_mode() const;

	void set_loop_current_entry(bool p_enabled);
	bool is_loop_current_entry() const;

	void set_default_fade_time(float p_time);
	float get_default_fade_time() const;

	void set_pause_with_tree(bool p_enabled);
	bool is_pause_with_tree() const;

	void set_stop_on_scene_change(bool p_enabled);
	bool is_stop_on_scene_change() const;

	void set_preload_next(bool p_enabled);
	bool is_preload_next() const;
};

VARIANT_ENUM_CAST(AudioMixerChannel::QueueMode);
