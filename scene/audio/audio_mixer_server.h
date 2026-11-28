/**************************************************************************/
/*  audio_mixer_server.h                                                  */
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

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "scene/audio/audio_fade.h"
#include "scene/resources/audio/audio_mixer_channel.h"
#include "scene/resources/audio/audio_mixer_layout.h"
#include "scene/resources/audio/audio_stream.h"

class AudioCommand;
class AudioCommands;
class AudioCommandRegistry;
class AudioServer;

// Global audio channels (music, ambience, voice, ...) that live outside the scene tree.
// Each channel plays on an existing bus through the node-free playback API of AudioServer,
// has a queue of streams with a cursor, and supports fades and cross-fades.
// All methods must be called on the main thread, like the AudioServer playback API itself.
class AudioMixerServer : public Object {
	GDCLASS(AudioMixerServer, Object);

public:
	// Also the curve of the AudioStreamPlayer fades; the math is in AudioFade.
	enum FadeCurve {
		FADE_CURVE_LINEAR = AudioFade::CURVE_LINEAR,
		FADE_CURVE_EASE_IN = AudioFade::CURVE_EASE_IN,
		FADE_CURVE_EASE_OUT = AudioFade::CURVE_EASE_OUT,
		FADE_CURVE_EQUAL_POWER = AudioFade::CURVE_EQUAL_POWER,
	};

private:
	static AudioMixerServer *singleton;

	using Fade = AudioFade;

	struct Voice {
		Ref<AudioStreamPlayback> playback;
		Ref<AudioStream> stream;
		String stream_path;
		int index = -1; // Queue index, -1 when played directly or when the entry was removed.
		bool from_queue = false; // Advances the queue when it ends.
		bool one_shot = false;
		bool stopping = false; // Fading out, stops at the end of the fade.
		float gain = 1.0f; // Fade gain, 0 to 1.
		Fade fade;
		float volume_db = 0.0f; // Extra volume of one-shots.
		float pitch_scale = 1.0f; // Extra pitch of one-shots.
		ObjectID scene_id; // Current scene when the voice started (stop_on_scene_change).
	};

	struct Entry {
		String path;
		Ref<AudioStream> stream;
		bool preloading = false;
	};

	struct Channel {
		Ref<AudioMixerChannel> config;
		float volume_db = 0.0f;
		Fade volume_fade; // In dB.
		float pitch_scale = 1.0f;
		LocalVector<Entry> queue;
		int index = -1;
		LocalVector<Voice> voices; // The current voice is the last one that is not stopping.
		LocalVector<Voice> one_shots;
		bool paused = false;
		bool tree_paused = false;
		bool bus_warned = false;
		// Ducking (scene/audio/audio_mixer_ducking.cpp): an offset in dB added to volume_db.
		float duck_db = 0.0f;
		float duck_target_db = 0.0f;
		Fade duck_fade; // In dB.
		bool ducked = false;
		bool manual_duck = false;
		float manual_duck_db = 0.0f;
	};

	enum SignalType {
		SIGNAL_CHANNEL_STARTED,
		SIGNAL_CHANNEL_FINISHED,
		SIGNAL_QUEUE_FINISHED,
		SIGNAL_FADE_FINISHED,
		SIGNAL_DUCKING_CHANGED, // index: 1 ducked, 0 restored.
	};

	struct PendingSignal {
		SignalType type = SIGNAL_CHANNEL_STARTED;
		StringName channel;
		int index = -1;
		String stream_path;
	};

	HashMap<StringName, Channel> channels;
	LocalVector<PendingSignal> pending_signals;
	AudioServer *callback_server = nullptr;
	uint64_t last_tick_usec = 0;
	bool default_layout_checked = false;
	AudioCommandRegistry *command_registry = nullptr; // Data-driven commands, see scene/audio/audio_command.h.

	// Ducking while TTSServer speaks, see scene/audio/audio_mixer_ducking.cpp.
	HashSet<int64_t> active_utterances;
	bool ducking_enabled = true;
	double tts_poll_elapsed = 0.0;
	double tts_silent_time = 0.0;

	static void _update_callback(void *p_self);
	void _ensure_callback();
	void _remove_callback();
	void _ensure_default_layout();

	Channel *_get_channel(const StringName &p_channel);
	const Channel *_get_channel(const StringName &p_channel) const;
	Ref<AudioStream> _resolve_stream(const Variant &p_stream, String &r_path) const;
	Ref<AudioStream> _load_entry(Channel &p_channel, int p_index);
	void _preload_entry(Channel &p_channel, int p_index);

	static float _apply_curve(const Fade &p_fade);
	static ObjectID _current_scene_id();
	float _voice_volume_linear(const Channel &p_channel, const Voice &p_voice) const;
	void _apply_voice_volume(const Channel &p_channel, const Voice &p_voice) const;
	void _apply_voice_pitch(const Channel &p_channel, const Voice &p_voice) const;
	void _set_voice_paused(const Voice &p_voice, bool p_paused) const;
	bool _is_voice_active(const Voice &p_voice) const;
	void _stop_voice(const Voice &p_voice) const;
	void _start_fade(Voice &r_voice, float p_to, float p_time, FadeCurve p_curve) const;
	bool _start_voice(Channel &p_channel, Voice &r_voice, float p_from);

	int _current_voice_index(const Channel &p_channel) const;
	void _limit_voices(Channel &p_channel);
	bool _play_stream(const StringName &p_name, Channel &p_channel, const Ref<AudioStream> &p_stream, const String &p_path, int p_index, float p_from, float p_fade_time, FadeCurve p_curve);
	bool _play_entry(const StringName &p_name, Channel &p_channel, int p_index, float p_from, float p_fade_time, FadeCurve p_curve);
	int _next_index(const Channel &p_channel) const;
	int _previous_index(const Channel &p_channel) const;
	void _switch_to(const StringName &p_name, Channel &p_channel, int p_index, float p_fade_time);
	void _stop_channel(Channel &p_channel, float p_fade_time, FadeCurve p_curve);
	void _set_channel_paused(Channel &p_channel);
	void _process_channel(const StringName &p_name, Channel &p_channel, double p_delta, ObjectID p_scene_id, bool p_tree_paused);

	void _connect_tts();
	void _disconnect_tts();
	void _on_tts_utterance_started(int64_t p_id);
	void _on_tts_utterance_finished(int64_t p_id);
	bool _is_tts_ducked(const Channel &p_channel) const;
	void _update_duck(const StringName &p_name, Channel &p_channel, float p_fade_time);
	void _update_all_ducks();
	bool _process_duck(const StringName &p_name, Channel &p_channel, double p_delta);
	void _process_utterance_safeguard(double p_delta);

	void _queue_signal(SignalType p_type, const StringName &p_channel, int p_index = -1, const String &p_path = String());
	void _flush_signals();

protected:
	static void _bind_methods();

public:
	static AudioMixerServer *get_singleton();

	// Channels.
	void apply_layout(const Ref<AudioMixerLayout> &p_layout);
	void add_channel(const Ref<AudioMixerChannel> &p_channel);
	void remove_channel(const StringName &p_channel);
	bool has_channel(const StringName &p_channel) const;
	Ref<AudioMixerChannel> get_channel(const StringName &p_channel) const;
	PackedStringArray get_channel_names() const;

	// Transport.
	void play(const StringName &p_channel, const Variant &p_stream = Variant(), float p_from = 0.0f);
	void stop(const StringName &p_channel, float p_fade_time = 0.0f);
	void pause(const StringName &p_channel);
	void resume(const StringName &p_channel);
	void seek(const StringName &p_channel, float p_position);
	void stop_all(float p_fade_time = 0.0f);
	bool is_playing(const StringName &p_channel) const;
	bool is_paused(const StringName &p_channel) const;
	float get_playback_position(const StringName &p_channel) const;
	void play_one_shot(const StringName &p_channel, const Variant &p_stream, float p_volume_db = 0.0f, float p_pitch_scale = 1.0f);
	int get_voice_count(const StringName &p_channel) const;

	// Queue.
	void enqueue(const StringName &p_channel, const Variant &p_stream, int p_at = -1);
	void dequeue(const StringName &p_channel, int p_index = -1);
	void clear_queue(const StringName &p_channel);
	PackedStringArray get_queue(const StringName &p_channel) const;
	void set_queue_index(const StringName &p_channel, int p_index, float p_fade_time = -1.0f);
	void play_next(const StringName &p_channel, float p_fade_time = -1.0f);
	void play_previous(const StringName &p_channel, float p_fade_time = -1.0f);
	int get_queue_index(const StringName &p_channel) const;

	// Mixing.
	void fade_in(const StringName &p_channel, float p_time, FadeCurve p_curve = FADE_CURVE_LINEAR);
	void fade_out(const StringName &p_channel, float p_time, FadeCurve p_curve = FADE_CURVE_LINEAR);
	void cross_fade(const StringName &p_channel, const Variant &p_stream, float p_time, FadeCurve p_curve = FADE_CURVE_EQUAL_POWER);
	bool is_fading(const StringName &p_channel) const;
	float get_fade_gain(const StringName &p_channel) const;
	void set_channel_volume_db(const StringName &p_channel, float p_volume_db, float p_fade_time = 0.0f);
	float get_channel_volume_db(const StringName &p_channel) const;
	void set_channel_pitch_scale(const StringName &p_channel, float p_pitch_scale);
	float get_channel_pitch_scale(const StringName &p_channel) const;
	void switch_to_clip(const StringName &p_channel, const StringName &p_clip);
	Ref<AudioStreamPlayback> get_channel_playback(const StringName &p_channel) const;

	// Ducking (while TTSServer speaks, or manually for dialog played as audio).
	void set_ducking_enabled(bool p_enabled);
	bool is_ducking_enabled() const;
	void duck(const StringName &p_channel, float p_volume_db, float p_fade_time = -1.0f);
	void unduck(const StringName &p_channel, float p_fade_time = -1.0f);
	bool is_ducked(const StringName &p_channel) const;
	float get_duck_volume_db(const StringName &p_channel) const;
	int get_active_utterance_count() const;

	// Commands (AudioCommand, AudioCommands), implemented by AudioCommandRegistry.
	Error execute(const Ref<AudioCommand> &p_command);
	Error execute_commands(const Ref<AudioCommands> &p_commands);
	Error register_commands(const Ref<AudioCommands> &p_commands);
	void unregister_commands(const Ref<AudioCommands> &p_commands);
	bool has_command(const StringName &p_name) const;
	Error execute_named(const StringName &p_name);
	Error execute_group(const StringName &p_group);
	PackedStringArray get_command_names() const;

	// C++ only: advances fades, end-of-stream detection and queues by p_delta seconds.
	// Called every frame from AudioServer::update(); unit tests call it directly.
	void process(double p_delta);
	// C++ only: stops every voice and removes all channels (tests and shutdown).
	void reset();

	AudioMixerServer();
	~AudioMixerServer();
};

VARIANT_ENUM_CAST(AudioMixerServer::FadeCurve);
