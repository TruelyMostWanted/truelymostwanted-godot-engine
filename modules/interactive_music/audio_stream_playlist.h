/**************************************************************************/
/*  audio_stream_playlist.h                                               */
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

#include "core/templates/safe_refcount.h"
#include "scene/resources/audio/audio_stream.h"

class AudioStreamPlaybackPlaylist;

class AudioStreamPlaylist : public AudioStream {
	GDCLASS(AudioStreamPlaylist, AudioStream)
	OBJ_SAVE_TYPE(AudioStream)

private:
	friend class AudioStreamPlaybackPlaylist;

	enum {
		MAX_STREAMS = 64
	};

	bool shuffle = false;
	bool loop = true;
	double fade_time = 0.3;

	int stream_count = 0;
	Ref<AudioStream> audio_streams[MAX_STREAMS];
	HashSet<AudioStreamPlaybackPlaylist *> playbacks;

	Vector<Ref<AudioStream>> _get_list() const;
	void _set_list(const Vector<Ref<AudioStream>> &p_list);

public:
	virtual double get_bpm() const override;
	void set_stream_count(int p_count);
	int get_stream_count() const;
	void set_fade_time(float p_time);
	float get_fade_time() const;
	void set_shuffle(bool p_shuffle);
	bool get_shuffle() const;
	void set_loop(bool p_loop);
	virtual bool has_loop() const override;
	void set_list_stream(int p_stream_index, Ref<AudioStream> p_stream);
	Ref<AudioStream> get_list_stream(int p_stream_index) const;

	// Typed list API (the stream_count and stream_N properties stay for old files).
	void set_streams(const TypedArray<AudioStream> &p_streams);
	TypedArray<AudioStream> get_streams() const;
	void add_stream(const Ref<AudioStream> &p_stream);
	void insert_stream(int p_index, const Ref<AudioStream> &p_stream);
	void remove_stream(int p_index);
	void move_stream(int p_from, int p_to);
	int find_stream(const Ref<AudioStream> &p_stream) const;
	String get_stream_name(int p_index) const;

	// Time helpers in list order; tracks follow each other without overlap (see fade_time).
	double get_stream_length(int p_index) const;
	double get_stream_start_time(int p_index) const;
	int get_stream_index_at(double p_time) const;

	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual double get_length() const override;
	virtual bool is_meta_stream() const override { return true; }

protected:
	static void _bind_methods();
};

///////////////////////////////////////

class AudioStreamPlaybackPlaylist : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackPlaylist, AudioStreamPlayback)
	friend class AudioStreamPlaylist;

private:
	enum {
		MIX_BUFFER_SIZE = 128
	};
	AudioFrame mix_buffer[MIX_BUFFER_SIZE];
	AudioFrame fade_buffer[MIX_BUFFER_SIZE];

	Ref<AudioStreamPlaylist> playlist;
	Ref<AudioStreamPlayback> playback[AudioStreamPlaylist::MAX_STREAMS];

	int play_order[AudioStreamPlaylist::MAX_STREAMS] = {};

	double stream_todo = 0.0;
	int fade_index = -1;
	double fade_volume = 1.0;
	int play_index = 0;
	double offset = 0.0;

	int loop_count = 0;

	bool active = false;

	// Track info written by start() and mix(), read on the main thread without a lock.
	double track_length = 0.0;
	double track_start = 0.0; // Start of the current track in the playlist, in play order.
	SafeNumeric<int> current_index{ -1 };
	SafeNumeric<uint32_t> track_serial;
	SafeNumeric<float> track_position;
	SafeNumeric<float> playlist_position;
	void _update_track_start();
	void _publish_track();

	void _update_order();

	void _update_playback_instances();

protected:
	static void _bind_methods();

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;

	int get_current_index() const;
	Ref<AudioStream> get_current_stream() const;
	double get_position_in_stream() const;
	double get_position_in_playlist() const;
	virtual bool get_track_info(TrackInfo &r_info) const override;

	~AudioStreamPlaybackPlaylist();
};
