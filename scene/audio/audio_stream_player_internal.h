/**************************************************************************/
/*  audio_stream_player_internal.h                                        */
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

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#include "scene/audio/audio_fade.h"
#include "servers/audio/audio_server_enums.h"

class AudioStream;
class AudioStreamPlayback;
class AudioSamplePlayback;
class Node;
struct AudioFrame;

class AudioStreamPlayerInternal : public Object {
	GDCLASS(AudioStreamPlayerInternal, Object);

private:
	struct ParameterData {
		StringName path;
		Variant value;
	};

	static inline const String PARAM_PREFIX = "parameters/";

	Node *node = nullptr;
	Callable play_callable;
	Callable stop_callable;
	bool physical = false;
	AuSE::PlaybackType playback_type = AuSE::PlaybackType::PLAYBACK_TYPE_DEFAULT;

	HashMap<StringName, ParameterData> playback_parameters;

	void _set_process(bool p_enabled);
	void _update_stream_parameters();

	bool _is_sample();

	// Playbacks of the previous stream of a crossfade; not counted in polyphony, no `finished`.
	struct FadingPlayback {
		Ref<AudioStreamPlayback> playback;
		float gain = 1.0f;
		AudioFade fade;
	};
	LocalVector<FadingPlayback> fading_out;
	AudioFade fade;
	bool fade_stop_at_end = false;
	bool keep_fade_on_play = false;
	ObjectID track_playback;
	uint32_t track_serial = 0;
	double position_signal_elapsed = 0.0;

	void _assign_stream(const Ref<AudioStream> &p_stream);
	void _stop_fading_out();

public:
	Vector<Ref<AudioStreamPlayback>> stream_playbacks;
	Ref<AudioStream> stream;

	SafeFlag active;

	float pitch_scale = 1.0;
	float volume_db = 0.0;
	bool autoplay = false;
	StringName bus;
	int max_polyphony = 1;
	float fade_gain = 1.0f; // Multiplies the volume of the current playbacks.
	bool volumes_dirty = false; // A fade gain changed; cleared by apply_bus_volumes().
	float position_signal_interval = 0.0f;

	void process();
	void ensure_playback_limit();

	void notification(int p_what);
	bool set(const StringName &p_name, const Variant &p_value);
	bool get(const StringName &p_name, Variant &r_ret) const;
	void get_property_list(List<PropertyInfo> *p_list) const;

	void set_stream(Ref<AudioStream> p_stream);
	void set_pitch_scale(float p_pitch_scale);
	void set_max_polyphony(int p_max_polyphony);

	StringName get_bus() const;

	Ref<AudioStreamPlayback> play_basic();
	void seek(float p_seconds);
	void stop_basic();
	bool is_playing() const;
	float get_playback_position();

	void set_playing(bool p_enable);
	bool is_active() const;

	void set_stream_paused(bool p_pause);
	bool get_stream_paused() const;

	bool has_stream_playback();
	Ref<AudioStreamPlayback> get_stream_playback();

	void set_playback_type(AuSE::PlaybackType p_playback_type);
	AuSE::PlaybackType get_playback_type() const;

	// Fades and crossfade. The nodes set every playback volume through apply_bus_volumes() and start
	// playbacks with get_faded_volumes(), so the fade gains apply on top of their own volume.
	void fade_in(float p_duration, int p_curve);
	void fade_out(float p_duration, int p_curve, bool p_stop_at_end);
	void crossfade_to(const Ref<AudioStream> &p_stream, float p_duration, int p_curve);
	bool is_fading() const;
	void apply_bus_volumes(const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes);
	Vector<AudioFrame> get_faded_volumes(const Vector<AudioFrame> &p_volumes) const;

	// Track info: playlists report their current track, single streams index 0.
	int get_current_track_index();
	Ref<AudioStream> get_current_track_stream();
	double get_track_position();

	AudioStreamPlayerInternal(Node *p_node, const Callable &p_play_callable, const Callable &p_stop_callable, bool p_physical);
};
