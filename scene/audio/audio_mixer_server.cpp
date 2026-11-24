/**************************************************************************/
/*  audio_mixer_server.cpp                                                */
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

#include "audio_mixer_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "scene/audio/audio_command.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "servers/audio/audio_server.h"

#define MIXER_THREAD_GUARD() \
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "AudioMixerServer can only be used from the main thread.")

#define MIXER_THREAD_GUARD_V(m_ret) \
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), m_ret, "AudioMixerServer can only be used from the main thread.")

AudioMixerServer *AudioMixerServer::singleton = nullptr;

AudioMixerServer *AudioMixerServer::get_singleton() {
	return singleton;
}

// Setup.

void AudioMixerServer::_update_callback(void *p_self) {
	AudioMixerServer *self = static_cast<AudioMixerServer *>(p_self);
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	const double delta = self->last_tick_usec == 0 ? 0.0 : double(now - self->last_tick_usec) / 1000000.0;
	self->last_tick_usec = now;
	self->process(delta);
}

void AudioMixerServer::_ensure_callback() {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || audio_server == callback_server) {
		return;
	}
	audio_server->add_update_callback(&AudioMixerServer::_update_callback, this);
	callback_server = audio_server;
	last_tick_usec = OS::get_singleton()->get_ticks_usec();
}

void AudioMixerServer::_remove_callback() {
	if (callback_server != nullptr && callback_server == AudioServer::get_singleton()) {
		callback_server->remove_update_callback(&AudioMixerServer::_update_callback, this);
	}
	callback_server = nullptr;
	last_tick_usec = 0;
}

void AudioMixerServer::_ensure_default_layout() {
	if (default_layout_checked) {
		return;
	}
	default_layout_checked = true;
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const String path = GLOBAL_GET("audio/mixer/default_layout");
	if (path.is_empty()) {
		return;
	}
	Ref<AudioMixerLayout> layout = ResourceLoader::load(path);
	ERR_FAIL_COND_MSG(layout.is_null(), vformat("Cannot load the audio mixer layout \"%s\" (project setting \"audio/mixer/default_layout\").", path));
	apply_layout(layout);
}

// Helpers.

AudioMixerServer::Channel *AudioMixerServer::_get_channel(const StringName &p_channel) {
	_ensure_default_layout();
	Channel *channel = channels.getptr(p_channel);
	ERR_FAIL_NULL_V_MSG(channel, nullptr, vformat("The audio mixer channel \"%s\" does not exist.", p_channel));
	return channel;
}

const AudioMixerServer::Channel *AudioMixerServer::_get_channel(const StringName &p_channel) const {
	const_cast<AudioMixerServer *>(this)->_ensure_default_layout();
	const Channel *channel = channels.getptr(p_channel);
	ERR_FAIL_NULL_V_MSG(channel, nullptr, vformat("The audio mixer channel \"%s\" does not exist.", p_channel));
	return channel;
}

Ref<AudioStream> AudioMixerServer::_resolve_stream(const Variant &p_stream, String &r_path) const {
	if (p_stream.get_type() == Variant::STRING || p_stream.get_type() == Variant::STRING_NAME) {
		r_path = p_stream;
		ERR_FAIL_COND_V_MSG(r_path.is_empty(), Ref<AudioStream>(), "The stream path is empty.");
		Ref<AudioStream> stream = ResourceLoader::load(r_path);
		ERR_FAIL_COND_V_MSG(stream.is_null(), Ref<AudioStream>(), vformat("Cannot load the audio stream \"%s\".", r_path));
		return stream;
	}
	Ref<AudioStream> stream = p_stream;
	ERR_FAIL_COND_V_MSG(stream.is_null(), Ref<AudioStream>(), "The stream must be an AudioStream or a path to one.");
	r_path = stream->get_path();
	return stream;
}

Ref<AudioStream> AudioMixerServer::_load_entry(Channel &p_channel, int p_index) {
	ERR_FAIL_INDEX_V(p_index, (int)p_channel.queue.size(), Ref<AudioStream>());
	Entry &entry = p_channel.queue[p_index];
	if (entry.stream.is_valid()) {
		return entry.stream;
	}
	Ref<AudioStream> stream;
	if (entry.preloading) {
		entry.preloading = false;
		stream = ResourceLoader::load_threaded_get(entry.path);
	}
	if (stream.is_null()) {
		stream = ResourceLoader::load(entry.path);
	}
	ERR_FAIL_COND_V_MSG(stream.is_null(), Ref<AudioStream>(), vformat("Cannot load the audio stream \"%s\".", entry.path));
	// Path entries do not keep the stream: only the playing voice (and a preload) holds it.
	return stream;
}

void AudioMixerServer::_preload_entry(Channel &p_channel, int p_index) {
	if (p_index < 0 || p_index >= (int)p_channel.queue.size()) {
		return;
	}
	Entry &entry = p_channel.queue[p_index];
	if (entry.stream.is_valid() || entry.preloading || entry.path.is_empty()) {
		return;
	}
	entry.preloading = ResourceLoader::load_threaded_request(entry.path, "AudioStream") == OK;
}

float AudioMixerServer::_apply_curve(const Fade &p_fade) {
	const float t = p_fade.time > 0.0 ? (float)CLAMP(p_fade.elapsed / p_fade.time, 0.0, 1.0) : 1.0f;
	float shape = t;
	switch (p_fade.curve) {
		case FADE_CURVE_LINEAR: {
			shape = t;
		} break;
		case FADE_CURVE_EASE_IN: {
			shape = t * t;
		} break;
		case FADE_CURVE_EASE_OUT: {
			shape = 1.0f - (1.0f - t) * (1.0f - t);
		} break;
		case FADE_CURVE_EQUAL_POWER: {
			// Rising gains follow sin, falling gains follow cos, so the summed power of a cross-fade stays 1.
			const float angle = t * (float)Math::PI * 0.5f;
			shape = p_fade.to >= p_fade.from ? Math::sin(angle) : 1.0f - Math::cos(angle);
		} break;
	}
	return p_fade.from + (p_fade.to - p_fade.from) * shape;
}

ObjectID AudioMixerServer::_current_scene_id() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_current_scene() == nullptr) {
		return ObjectID();
	}
	return tree->get_current_scene()->get_instance_id();
}

static Vector<AudioFrame> _mixer_volume_vector(float p_linear) {
	// Same layout as AudioStreamPlayer with MIX_TARGET_STEREO.
	Vector<AudioFrame> volume_vector;
	volume_vector.resize(AuSC::MAX_CHANNELS_PER_BUS);
	for (AudioFrame &frame : volume_vector) {
		frame = AudioFrame(0, 0);
	}
	volume_vector.write[0] = AudioFrame(p_linear, p_linear);
	return volume_vector;
}

float AudioMixerServer::_voice_volume_linear(const Channel &p_channel, const Voice &p_voice) const {
	return Math::db_to_linear(p_channel.volume_db) * p_voice.gain * Math::db_to_linear(p_voice.volume_db);
}

void AudioMixerServer::_apply_voice_volume(const Channel &p_channel, const Voice &p_voice) const {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || p_voice.playback.is_null()) {
		return;
	}
	audio_server->set_playback_all_bus_volumes_linear(p_voice.playback, _mixer_volume_vector(_voice_volume_linear(p_channel, p_voice)));
}

void AudioMixerServer::_apply_voice_pitch(const Channel &p_channel, const Voice &p_voice) const {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || p_voice.playback.is_null()) {
		return;
	}
	audio_server->set_playback_pitch_scale(p_voice.playback, p_channel.pitch_scale * p_voice.pitch_scale);
}

void AudioMixerServer::_set_voice_paused(const Voice &p_voice, bool p_paused) const {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || p_voice.playback.is_null()) {
		return;
	}
	audio_server->set_playback_paused(p_voice.playback, p_paused);
	if (p_voice.playback->get_is_sample() && p_voice.playback->get_sample_playback().is_valid()) {
		audio_server->set_sample_playback_pause(p_voice.playback->get_sample_playback(), p_paused);
	}
}

bool AudioMixerServer::_is_voice_active(const Voice &p_voice) const {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || p_voice.playback.is_null()) {
		return false;
	}
	return audio_server->is_playback_active(p_voice.playback);
}

void AudioMixerServer::_stop_voice(const Voice &p_voice) const {
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || p_voice.playback.is_null()) {
		return;
	}
	audio_server->stop_playback_stream(p_voice.playback);
}

void AudioMixerServer::_start_fade(Voice &r_voice, float p_to, float p_time, FadeCurve p_curve) const {
	if (p_time <= 0.0f) {
		r_voice.fade.active = false;
		r_voice.gain = p_to;
		return;
	}
	r_voice.fade.active = true;
	r_voice.fade.from = r_voice.gain;
	r_voice.fade.to = p_to;
	r_voice.fade.time = p_time;
	r_voice.fade.elapsed = 0.0;
	r_voice.fade.curve = p_curve;
}

bool AudioMixerServer::_start_voice(Channel &p_channel, Voice &r_voice, float p_from) {
	AudioServer *audio_server = AudioServer::get_singleton();
	ERR_FAIL_NULL_V_MSG(audio_server, false, "AudioMixerServer cannot play without an AudioServer.");
	ERR_FAIL_COND_V(r_voice.stream.is_null(), false);
	_ensure_callback();

	Ref<AudioStreamPlayback> playback = r_voice.stream->instantiate_playback();
	ERR_FAIL_COND_V_MSG(playback.is_null(), false, "Failed to instantiate the playback of the audio stream.");

	StringName bus = p_channel.config->get_bus();
	if (audio_server->get_bus_index(bus) < 0) {
		if (!p_channel.bus_warned) {
			p_channel.bus_warned = true;
			WARN_PRINT(vformat("The audio bus \"%s\" of the audio mixer channel \"%s\" does not exist; playing on \"Master\".", bus, p_channel.config->get_channel_name()));
		}
		bus = SNAME("Master");
	}

	const float pitch_scale = p_channel.pitch_scale * r_voice.pitch_scale;
	const bool paused = p_channel.paused || p_channel.tree_paused;

	// Sample playback (default on Web), set up like AudioStreamPlayer::play().
	if (audio_server->get_default_playback_type() == AuSE::PlaybackType::PLAYBACK_TYPE_SAMPLE && r_voice.stream->can_be_sampled()) {
		playback->set_is_sample(true);
		if (playback->get_is_sample() && playback->get_sample_playback().is_null()) {
			if (!audio_server->is_stream_registered_as_sample(r_voice.stream)) {
				audio_server->register_stream_as_sample(r_voice.stream);
			}
			Ref<AudioSamplePlayback> sample_playback;
			sample_playback.instantiate();
			sample_playback->stream = r_voice.stream;
			sample_playback->pitch_scale = pitch_scale;
			playback->set_sample_playback(sample_playback);
		}
	}

	const Vector<AudioFrame> volume_vector = _mixer_volume_vector(_voice_volume_linear(p_channel, r_voice));
	audio_server->start_playback_stream(playback, bus, volume_vector, p_from, pitch_scale, paused);

	if (playback->get_is_sample() && playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = playback->get_sample_playback();
		sample_playback->offset = p_from;
		sample_playback->volume_vector = volume_vector;
		sample_playback->bus = bus;
		audio_server->start_sample_playback(sample_playback, paused);
	}

	r_voice.playback = playback;
	return true;
}

int AudioMixerServer::_current_voice_index(const Channel &p_channel) const {
	for (int i = (int)p_channel.voices.size() - 1; i >= 0; i--) {
		if (!p_channel.voices[i].stopping) {
			return i;
		}
	}
	return -1;
}

void AudioMixerServer::_limit_voices(Channel &p_channel) {
	// At most one voice fading out next to the current one.
	int stopping = 0;
	for (const Voice &voice : p_channel.voices) {
		if (voice.stopping) {
			stopping++;
		}
	}
	for (uint32_t i = 0; i < p_channel.voices.size() && stopping > 1;) {
		if (p_channel.voices[i].stopping) {
			_stop_voice(p_channel.voices[i]);
			p_channel.voices.remove_at(i);
			stopping--;
		} else {
			i++;
		}
	}
	// One-shots: the oldest voice is stolen.
	while ((int)p_channel.one_shots.size() > p_channel.config->get_max_polyphony()) {
		_stop_voice(p_channel.one_shots[0]);
		p_channel.one_shots.remove_at(0);
	}
}

bool AudioMixerServer::_play_stream(const StringName &p_name, Channel &p_channel, const Ref<AudioStream> &p_stream, const String &p_path, int p_index, float p_from, float p_fade_time, FadeCurve p_curve) {
	ERR_FAIL_COND_V(p_stream.is_null(), false);

	// The current voice fades out (cross-fade) or stops at once.
	if (p_fade_time > 0.0f) {
		const int current = _current_voice_index(p_channel);
		if (current >= 0) {
			Voice &old_voice = p_channel.voices[current];
			old_voice.stopping = true;
			_start_fade(old_voice, 0.0f, p_fade_time, p_curve);
		}
	} else {
		for (const Voice &voice : p_channel.voices) {
			_stop_voice(voice);
		}
		p_channel.voices.clear();
	}

	Voice voice;
	voice.stream = p_stream;
	voice.stream_path = p_path;
	voice.index = p_index;
	voice.from_queue = p_index >= 0;
	voice.scene_id = _current_scene_id();
	voice.gain = p_fade_time > 0.0f ? 0.0f : 1.0f;
	if (!_start_voice(p_channel, voice, p_from)) {
		return false;
	}
	_start_fade(voice, 1.0f, p_fade_time, p_curve);
	p_channel.voices.push_back(voice);
	_limit_voices(p_channel);

	if (p_index >= 0) {
		p_channel.index = p_index;
		if (p_channel.config->is_preload_next() && p_channel.config->get_queue_mode() != AudioMixerChannel::QUEUE_MODE_SHUFFLE) {
			const int next_index = _next_index(p_channel);
			if (next_index != p_index) {
				_preload_entry(p_channel, next_index);
			}
		}
	}
	_queue_signal(SIGNAL_CHANNEL_STARTED, p_name, p_index, p_path);
	return true;
}

bool AudioMixerServer::_play_entry(const StringName &p_name, Channel &p_channel, int p_index, float p_from, float p_fade_time, FadeCurve p_curve) {
	ERR_FAIL_INDEX_V(p_index, (int)p_channel.queue.size(), false);
	Ref<AudioStream> stream = _load_entry(p_channel, p_index);
	if (stream.is_null()) {
		return false;
	}
	String path = p_channel.queue[p_index].path;
	if (path.is_empty()) {
		path = stream->get_path();
	}
	return _play_stream(p_name, p_channel, stream, path, p_index, p_from, p_fade_time, p_curve);
}

int AudioMixerServer::_next_index(const Channel &p_channel) const {
	const int size = (int)p_channel.queue.size();
	if (size == 0) {
		return -1;
	}
	switch (p_channel.config->get_queue_mode()) {
		case AudioMixerChannel::QUEUE_MODE_STOP: {
			return p_channel.index + 1 < size ? p_channel.index + 1 : -1;
		}
		case AudioMixerChannel::QUEUE_MODE_LOOP_QUEUE: {
			return (p_channel.index + 1) % size;
		}
		case AudioMixerChannel::QUEUE_MODE_SHUFFLE: {
			if (size == 1 || p_channel.index < 0 || p_channel.index >= size) {
				return size == 1 ? 0 : Math::random(0, size - 1);
			}
			// A random entry other than the current one.
			const int pick = Math::random(0, size - 2);
			return pick >= p_channel.index ? pick + 1 : pick;
		}
	}
	return -1;
}

int AudioMixerServer::_previous_index(const Channel &p_channel) const {
	const int size = (int)p_channel.queue.size();
	if (size == 0) {
		return -1;
	}
	if (p_channel.index > 0) {
		return MIN(p_channel.index - 1, size - 1);
	}
	if (p_channel.config->get_queue_mode() == AudioMixerChannel::QUEUE_MODE_STOP) {
		return p_channel.index < 0 ? 0 : -1;
	}
	return size - 1;
}

void AudioMixerServer::_switch_to(const StringName &p_name, Channel &p_channel, int p_index, float p_fade_time) {
	if (p_index < 0) {
		return;
	}
	if (_current_voice_index(p_channel) < 0) {
		// Not playing: only move the cursor.
		p_channel.index = p_index;
		return;
	}
	const float fade_time = p_fade_time < 0.0f ? p_channel.config->get_default_fade_time() : p_fade_time;
	_play_entry(p_name, p_channel, p_index, 0.0f, fade_time, FADE_CURVE_EQUAL_POWER);
	_flush_signals();
}

void AudioMixerServer::_stop_channel(Channel &p_channel, float p_fade_time, FadeCurve p_curve) {
	if (p_fade_time > 0.0f) {
		for (Voice &voice : p_channel.voices) {
			if (!voice.stopping) {
				voice.stopping = true;
				_start_fade(voice, 0.0f, p_fade_time, p_curve);
			}
		}
	} else {
		for (const Voice &voice : p_channel.voices) {
			_stop_voice(voice);
		}
		p_channel.voices.clear();
	}
	for (const Voice &voice : p_channel.one_shots) {
		_stop_voice(voice);
	}
	p_channel.one_shots.clear();
}

void AudioMixerServer::_set_channel_paused(Channel &p_channel) {
	const bool paused = p_channel.paused || p_channel.tree_paused;
	for (const Voice &voice : p_channel.voices) {
		_set_voice_paused(voice, paused);
	}
	for (const Voice &voice : p_channel.one_shots) {
		_set_voice_paused(voice, paused);
	}
}

void AudioMixerServer::_process_channel(const StringName &p_name, Channel &p_channel, double p_delta, ObjectID p_scene_id, bool p_tree_paused) {
	// Follow SceneTree.paused.
	const bool tree_paused = p_channel.config->is_pause_with_tree() && p_tree_paused;
	if (tree_paused != p_channel.tree_paused) {
		p_channel.tree_paused = tree_paused;
		_set_channel_paused(p_channel);
	}

	// Stop voices started in another scene.
	if (p_channel.config->is_stop_on_scene_change() && p_scene_id.is_valid()) {
		bool scene_changed = false;
		for (Voice &voice : p_channel.voices) {
			if (voice.scene_id.is_null()) {
				voice.scene_id = p_scene_id;
			} else if (voice.scene_id != p_scene_id && !voice.stopping) {
				scene_changed = true;
			}
		}
		for (const Voice &voice : p_channel.one_shots) {
			if (voice.scene_id.is_valid() && voice.scene_id != p_scene_id) {
				scene_changed = true;
			}
		}
		if (scene_changed) {
			_stop_channel(p_channel, p_channel.config->get_default_fade_time(), FADE_CURVE_LINEAR);
		}
	}

	if (p_channel.paused || p_channel.tree_paused) {
		return; // Fades and end detection wait while the channel is paused.
	}

	bool fade_finished = false;
	bool volume_changed = false;
	if (p_channel.volume_fade.active) {
		p_channel.volume_fade.elapsed += p_delta;
		p_channel.volume_db = _apply_curve(p_channel.volume_fade);
		volume_changed = true;
		if (p_channel.volume_fade.elapsed >= p_channel.volume_fade.time) {
			p_channel.volume_fade.active = false;
			p_channel.volume_db = p_channel.volume_fade.to;
			fade_finished = true;
		}
	}

	bool ended = false;
	int ended_index = -1;
	bool ended_from_queue = false;
	Ref<AudioStream> ended_stream;
	String ended_path;

	for (uint32_t i = 0; i < p_channel.voices.size();) {
		Voice &voice = p_channel.voices[i];
		bool changed = volume_changed;
		if (voice.fade.active) {
			voice.fade.elapsed += p_delta;
			voice.gain = _apply_curve(voice.fade);
			changed = true;
			if (voice.fade.elapsed >= voice.fade.time) {
				voice.fade.active = false;
				voice.gain = voice.fade.to;
				fade_finished = true;
			}
		}
		if (voice.stopping && !voice.fade.active) {
			_stop_voice(voice);
			p_channel.voices.remove_at(i);
			continue;
		}
		if (!_is_voice_active(voice)) {
			if (!voice.stopping) {
				ended = true;
				ended_index = voice.index;
				ended_from_queue = voice.from_queue;
				ended_stream = voice.stream;
				ended_path = voice.stream_path;
			} else if (voice.fade.active) {
				fade_finished = true; // The stream ended during its fade-out: the fade is over too.
			}
			p_channel.voices.remove_at(i);
			continue;
		}
		if (changed) {
			_apply_voice_volume(p_channel, voice);
		}
		i++;
	}

	for (uint32_t i = 0; i < p_channel.one_shots.size();) {
		if (!_is_voice_active(p_channel.one_shots[i])) {
			p_channel.one_shots.remove_at(i);
			continue;
		}
		if (volume_changed) {
			_apply_voice_volume(p_channel, p_channel.one_shots[i]);
		}
		i++;
	}

	if (fade_finished) {
		_queue_signal(SIGNAL_FADE_FINISHED, p_name);
	}

	// The current entry reached its end: loop it, advance the queue or finish.
	if (ended && _current_voice_index(p_channel) < 0) {
		_queue_signal(SIGNAL_CHANNEL_FINISHED, p_name, ended_index);
		if (p_channel.config->is_loop_current_entry()) {
			if (ended_index >= 0) {
				_play_entry(p_name, p_channel, ended_index, 0.0f, 0.0f, FADE_CURVE_LINEAR);
			} else {
				_play_stream(p_name, p_channel, ended_stream, ended_path, -1, 0.0f, 0.0f, FADE_CURVE_LINEAR);
			}
		} else if (ended_from_queue) {
			const int next_index = _next_index(p_channel);
			if (next_index >= 0) {
				_play_entry(p_name, p_channel, next_index, 0.0f, 0.0f, FADE_CURVE_LINEAR);
			} else {
				_queue_signal(SIGNAL_QUEUE_FINISHED, p_name);
			}
		}
	}
}

void AudioMixerServer::_queue_signal(SignalType p_type, const StringName &p_channel, int p_index, const String &p_path) {
	PendingSignal pending;
	pending.type = p_type;
	pending.channel = p_channel;
	pending.index = p_index;
	pending.stream_path = p_path;
	pending_signals.push_back(pending);
}

void AudioMixerServer::_flush_signals() {
	// Emitted after the state is updated, so handlers may call the mixer again.
	while (!pending_signals.is_empty()) {
		LocalVector<PendingSignal> signals;
		for (const PendingSignal &pending : pending_signals) {
			signals.push_back(pending);
		}
		pending_signals.clear();
		for (const PendingSignal &pending : signals) {
			switch (pending.type) {
				case SIGNAL_CHANNEL_STARTED: {
					emit_signal(SNAME("channel_started"), pending.channel, pending.index, pending.stream_path);
				} break;
				case SIGNAL_CHANNEL_FINISHED: {
					emit_signal(SNAME("channel_finished"), pending.channel, pending.index);
				} break;
				case SIGNAL_QUEUE_FINISHED: {
					emit_signal(SNAME("queue_finished"), pending.channel);
				} break;
				case SIGNAL_FADE_FINISHED: {
					emit_signal(SNAME("fade_finished"), pending.channel);
				} break;
			}
		}
	}
}

// Channels.

void AudioMixerServer::apply_layout(const Ref<AudioMixerLayout> &p_layout) {
	MIXER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(p_layout.is_null(), "The audio mixer layout is null.");
	_ensure_default_layout();
	for (KeyValue<StringName, Channel> &E : channels) {
		_stop_channel(E.value, 0.0f, FADE_CURVE_LINEAR);
	}
	channels.clear();
	const TypedArray<AudioMixerChannel> layout_channels = p_layout->get_channels();
	for (int i = 0; i < layout_channels.size(); i++) {
		const Ref<AudioMixerChannel> channel = layout_channels[i];
		add_channel(channel);
	}
}

void AudioMixerServer::add_channel(const Ref<AudioMixerChannel> &p_channel) {
	MIXER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(p_channel.is_null(), "The audio mixer channel is null.");
	ERR_FAIL_COND_MSG(p_channel->get_channel_name() == StringName(), "The audio mixer channel has no name.");
	_ensure_default_layout();
	// A new definition replaces an existing one; playing voices keep playing with the new volume and pitch.
	Channel &channel = channels[p_channel->get_channel_name()];
	channel.config = p_channel;
	channel.volume_db = p_channel->get_volume_db();
	channel.volume_fade.active = false;
	channel.pitch_scale = p_channel->get_pitch_scale();
	channel.bus_warned = false;
	for (const Voice &voice : channel.voices) {
		_apply_voice_volume(channel, voice);
		_apply_voice_pitch(channel, voice);
	}
	_limit_voices(channel);
}

void AudioMixerServer::remove_channel(const StringName &p_channel) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	_stop_channel(*channel, 0.0f, FADE_CURVE_LINEAR);
	channels.erase(p_channel);
}

bool AudioMixerServer::has_channel(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(false);
	const_cast<AudioMixerServer *>(this)->_ensure_default_layout();
	return channels.has(p_channel);
}

Ref<AudioMixerChannel> AudioMixerServer::get_channel(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(Ref<AudioMixerChannel>());
	const Channel *channel = _get_channel(p_channel);
	return channel ? channel->config : Ref<AudioMixerChannel>();
}

PackedStringArray AudioMixerServer::get_channel_names() const {
	MIXER_THREAD_GUARD_V(PackedStringArray());
	const_cast<AudioMixerServer *>(this)->_ensure_default_layout();
	PackedStringArray names;
	for (const KeyValue<StringName, Channel> &E : channels) {
		names.push_back(E.key);
	}
	return names;
}

// Transport.

void AudioMixerServer::play(const StringName &p_channel, const Variant &p_stream, float p_from) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	if (channel->paused) {
		channel->paused = false;
		_set_channel_paused(*channel);
	}
	if (p_stream.get_type() == Variant::NIL) {
		ERR_FAIL_COND_MSG(channel->queue.is_empty(), vformat("The queue of the audio mixer channel \"%s\" is empty; pass a stream to play().", p_channel));
		const int index = CLAMP(channel->index, 0, (int)channel->queue.size() - 1);
		_play_entry(p_channel, *channel, index, p_from, 0.0f, FADE_CURVE_LINEAR);
	} else {
		String path;
		Ref<AudioStream> stream = _resolve_stream(p_stream, path);
		if (stream.is_null()) {
			return;
		}
		_play_stream(p_channel, *channel, stream, path, -1, p_from, 0.0f, FADE_CURVE_LINEAR);
	}
	_flush_signals();
}

void AudioMixerServer::stop(const StringName &p_channel, float p_fade_time) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	_stop_channel(*channel, p_fade_time, FADE_CURVE_LINEAR);
}

void AudioMixerServer::pause(const StringName &p_channel) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr || channel->paused) {
		return;
	}
	channel->paused = true;
	_set_channel_paused(*channel);
}

void AudioMixerServer::resume(const StringName &p_channel) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr || !channel->paused) {
		return;
	}
	channel->paused = false;
	_set_channel_paused(*channel);
}

void AudioMixerServer::seek(const StringName &p_channel, float p_position) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	const int current = _current_voice_index(*channel);
	ERR_FAIL_COND_MSG(current < 0, vformat("The audio mixer channel \"%s\" is not playing.", p_channel));
	// Stop and restart at the new position, like AudioStreamPlayer::seek().
	Voice voice = channel->voices[current];
	_stop_voice(voice);
	channel->voices.remove_at(current);
	voice.playback.unref();
	if (_start_voice(*channel, voice, MAX(0.0f, p_position))) {
		channel->voices.push_back(voice);
	}
}

void AudioMixerServer::stop_all(float p_fade_time) {
	MIXER_THREAD_GUARD();
	_ensure_default_layout();
	for (KeyValue<StringName, Channel> &E : channels) {
		_stop_channel(E.value, p_fade_time, FADE_CURVE_LINEAR);
	}
}

bool AudioMixerServer::is_playing(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(false);
	const Channel *channel = _get_channel(p_channel);
	return channel != nullptr && _current_voice_index(*channel) >= 0;
}

bool AudioMixerServer::is_paused(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(false);
	const Channel *channel = _get_channel(p_channel);
	return channel != nullptr && (channel->paused || channel->tree_paused);
}

float AudioMixerServer::get_playback_position(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(0.0f);
	const Channel *channel = _get_channel(p_channel);
	if (channel == nullptr || AudioServer::get_singleton() == nullptr) {
		return 0.0f;
	}
	const int current = _current_voice_index(*channel);
	if (current < 0) {
		return 0.0f;
	}
	return AudioServer::get_singleton()->get_playback_position(channel->voices[current].playback);
}

void AudioMixerServer::play_one_shot(const StringName &p_channel, const Variant &p_stream, float p_volume_db, float p_pitch_scale) {
	MIXER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(p_pitch_scale <= 0.0f, "Pitch scale must be greater than 0.");
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	String path;
	Ref<AudioStream> stream = _resolve_stream(p_stream, path);
	if (stream.is_null()) {
		return;
	}
	Voice voice;
	voice.stream = stream;
	voice.stream_path = path;
	voice.one_shot = true;
	voice.volume_db = p_volume_db;
	voice.pitch_scale = p_pitch_scale;
	voice.scene_id = _current_scene_id();
	if (_start_voice(*channel, voice, 0.0f)) {
		channel->one_shots.push_back(voice);
		_limit_voices(*channel);
	}
}

int AudioMixerServer::get_voice_count(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(0);
	const Channel *channel = _get_channel(p_channel);
	return channel ? int(channel->voices.size() + channel->one_shots.size()) : 0;
}

// Queue.

void AudioMixerServer::enqueue(const StringName &p_channel, const Variant &p_stream, int p_at) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	Entry entry;
	if (p_stream.get_type() == Variant::STRING || p_stream.get_type() == Variant::STRING_NAME) {
		entry.path = p_stream;
		ERR_FAIL_COND_MSG(entry.path.is_empty(), "The stream path is empty.");
	} else {
		Ref<AudioStream> stream = p_stream;
		ERR_FAIL_COND_MSG(stream.is_null(), "The stream must be an AudioStream or a path to one.");
		entry.stream = stream;
		entry.path = stream->get_path();
	}
	const int size = (int)channel->queue.size();
	if (p_at < 0 || p_at >= size) {
		channel->queue.push_back(entry);
		return;
	}
	channel->queue.insert(p_at, entry);
	if (p_at <= channel->index) {
		channel->index++;
	}
	for (Voice &voice : channel->voices) {
		if (voice.index >= p_at) {
			voice.index++;
		}
	}
}

void AudioMixerServer::dequeue(const StringName &p_channel, int p_index) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	const int size = (int)channel->queue.size();
	const int index = p_index < 0 ? size - 1 : p_index;
	ERR_FAIL_INDEX_MSG(index, size, vformat("The queue of the audio mixer channel \"%s\" has no entry %d.", p_channel, index));
	channel->queue.remove_at(index);
	// The cursor stays on the entry before, so play_next() plays the entry that followed the removed one.
	if (index <= channel->index) {
		channel->index--;
	}
	if (channel->queue.is_empty()) {
		channel->index = -1;
	}
	for (Voice &voice : channel->voices) {
		if (voice.index == index) {
			voice.index = -1;
		} else if (voice.index > index) {
			voice.index--;
		}
	}
}

void AudioMixerServer::clear_queue(const StringName &p_channel) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	channel->queue.clear();
	channel->index = -1;
	for (Voice &voice : channel->voices) {
		voice.index = -1;
	}
}

PackedStringArray AudioMixerServer::get_queue(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(PackedStringArray());
	const Channel *channel = _get_channel(p_channel);
	PackedStringArray paths;
	if (channel == nullptr) {
		return paths;
	}
	for (const Entry &entry : channel->queue) {
		paths.push_back(entry.path);
	}
	return paths;
}

void AudioMixerServer::set_queue_index(const StringName &p_channel, int p_index, float p_fade_time) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	ERR_FAIL_INDEX_MSG(p_index, (int)channel->queue.size(), vformat("The queue of the audio mixer channel \"%s\" has no entry %d.", p_channel, p_index));
	_switch_to(p_channel, *channel, p_index, p_fade_time);
}

void AudioMixerServer::play_next(const StringName &p_channel, float p_fade_time) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	_switch_to(p_channel, *channel, _next_index(*channel), p_fade_time);
}

void AudioMixerServer::play_previous(const StringName &p_channel, float p_fade_time) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	_switch_to(p_channel, *channel, _previous_index(*channel), p_fade_time);
}

int AudioMixerServer::get_queue_index(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(-1);
	const Channel *channel = _get_channel(p_channel);
	return channel ? channel->index : -1;
}

// Mixing.

void AudioMixerServer::fade_in(const StringName &p_channel, float p_time, FadeCurve p_curve) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	if (channel->paused) {
		channel->paused = false;
		_set_channel_paused(*channel);
	}
	const int current = _current_voice_index(*channel);
	if (current >= 0) {
		Voice &voice = channel->voices[current];
		_start_fade(voice, 1.0f, p_time, p_curve);
		if (!voice.fade.active) {
			_apply_voice_volume(*channel, voice);
			_queue_signal(SIGNAL_FADE_FINISHED, p_channel);
		}
	} else {
		ERR_FAIL_COND_MSG(channel->queue.is_empty(), vformat("The audio mixer channel \"%s\" is not playing and its queue is empty.", p_channel));
		const int index = CLAMP(channel->index, 0, (int)channel->queue.size() - 1);
		_play_entry(p_channel, *channel, index, 0.0f, p_time, p_curve);
	}
	_flush_signals();
}

void AudioMixerServer::fade_out(const StringName &p_channel, float p_time, FadeCurve p_curve) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	_stop_channel(*channel, p_time, p_curve);
	if (p_time <= 0.0f) {
		_queue_signal(SIGNAL_FADE_FINISHED, p_channel);
		_flush_signals();
	}
}

void AudioMixerServer::cross_fade(const StringName &p_channel, const Variant &p_stream, float p_time, FadeCurve p_curve) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	String path;
	Ref<AudioStream> stream = _resolve_stream(p_stream, path);
	if (stream.is_null()) {
		return;
	}
	if (channel->paused) {
		channel->paused = false;
		_set_channel_paused(*channel);
	}
	_play_stream(p_channel, *channel, stream, path, -1, 0.0f, p_time, p_curve);
	_flush_signals();
}

bool AudioMixerServer::is_fading(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(false);
	const Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return false;
	}
	if (channel->volume_fade.active) {
		return true;
	}
	for (const Voice &voice : channel->voices) {
		if (voice.fade.active) {
			return true;
		}
	}
	return false;
}

float AudioMixerServer::get_fade_gain(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(0.0f);
	const Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return 0.0f;
	}
	const int current = _current_voice_index(*channel);
	return current >= 0 ? channel->voices[current].gain : 0.0f;
}

void AudioMixerServer::set_channel_volume_db(const StringName &p_channel, float p_volume_db, float p_fade_time) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	if (p_fade_time > 0.0f) {
		channel->volume_fade.active = true;
		channel->volume_fade.from = channel->volume_db;
		channel->volume_fade.to = p_volume_db;
		channel->volume_fade.time = p_fade_time;
		channel->volume_fade.elapsed = 0.0;
		channel->volume_fade.curve = FADE_CURVE_LINEAR;
		return;
	}
	channel->volume_fade.active = false;
	channel->volume_db = p_volume_db;
	for (const Voice &voice : channel->voices) {
		_apply_voice_volume(*channel, voice);
	}
	for (const Voice &voice : channel->one_shots) {
		_apply_voice_volume(*channel, voice);
	}
}

float AudioMixerServer::get_channel_volume_db(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(0.0f);
	const Channel *channel = _get_channel(p_channel);
	return channel ? channel->volume_db : 0.0f;
}

void AudioMixerServer::set_channel_pitch_scale(const StringName &p_channel, float p_pitch_scale) {
	MIXER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(p_pitch_scale <= 0.0f, "Pitch scale must be greater than 0.");
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	channel->pitch_scale = p_pitch_scale;
	for (const Voice &voice : channel->voices) {
		_apply_voice_pitch(*channel, voice);
	}
	for (const Voice &voice : channel->one_shots) {
		_apply_voice_pitch(*channel, voice);
	}
}

float AudioMixerServer::get_channel_pitch_scale(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(1.0f);
	const Channel *channel = _get_channel(p_channel);
	return channel ? channel->pitch_scale : 1.0f;
}

void AudioMixerServer::switch_to_clip(const StringName &p_channel, const StringName &p_clip) {
	MIXER_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	const int current = _current_voice_index(*channel);
	ERR_FAIL_COND_MSG(current < 0, vformat("The audio mixer channel \"%s\" is not playing.", p_channel));
	Ref<AudioStreamPlayback> playback = channel->voices[current].playback;
	// AudioStreamPlaybackInteractive lives in the optional interactive_music module, so it is called by name.
	ERR_FAIL_COND_MSG(playback.is_null() || !playback->has_method(SNAME("switch_to_clip_by_name")), vformat("The stream of the audio mixer channel \"%s\" is not an AudioStreamInteractive.", p_channel));
	playback->call(SNAME("switch_to_clip_by_name"), p_clip);
}

Ref<AudioStreamPlayback> AudioMixerServer::get_channel_playback(const StringName &p_channel) const {
	MIXER_THREAD_GUARD_V(Ref<AudioStreamPlayback>());
	const Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return Ref<AudioStreamPlayback>();
	}
	const int current = _current_voice_index(*channel);
	return current >= 0 ? channel->voices[current].playback : Ref<AudioStreamPlayback>();
}

// Commands.

Error AudioMixerServer::execute(const Ref<AudioCommand> &p_command) {
	MIXER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	return command_registry->execute(p_command);
}

Error AudioMixerServer::execute_list(const Ref<AudioCommandList> &p_list) {
	MIXER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	return command_registry->execute_list(p_list);
}

Error AudioMixerServer::register_commands(const Ref<AudioCommandList> &p_list) {
	MIXER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	return command_registry->register_commands(p_list);
}

void AudioMixerServer::unregister_commands(const Ref<AudioCommandList> &p_list) {
	MIXER_THREAD_GUARD();
	command_registry->unregister_commands(p_list);
}

bool AudioMixerServer::has_command(const StringName &p_name) const {
	MIXER_THREAD_GUARD_V(false);
	return command_registry->has_command(p_name);
}

Error AudioMixerServer::execute_named(const StringName &p_name) {
	MIXER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	return command_registry->execute_named(p_name);
}

Error AudioMixerServer::execute_group(const StringName &p_group) {
	MIXER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	return command_registry->execute_group(p_group);
}

PackedStringArray AudioMixerServer::get_command_names() const {
	MIXER_THREAD_GUARD_V(PackedStringArray());
	return command_registry->get_command_names();
}

// Tick.

void AudioMixerServer::process(double p_delta) {
	MIXER_THREAD_GUARD();
	if (channels.is_empty()) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	const bool tree_paused = tree != nullptr && tree->is_paused();
	const ObjectID scene_id = _current_scene_id();
	for (KeyValue<StringName, Channel> &E : channels) {
		_process_channel(E.key, E.value, MAX(0.0, p_delta), scene_id, tree_paused);
	}
	_flush_signals();
}

void AudioMixerServer::reset() {
	MIXER_THREAD_GUARD();
	for (KeyValue<StringName, Channel> &E : channels) {
		_stop_channel(E.value, 0.0f, FADE_CURVE_LINEAR);
	}
	channels.clear();
	pending_signals.clear();
	command_registry->clear();
	_remove_callback();
}

void AudioMixerServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_layout", "layout"), &AudioMixerServer::apply_layout);
	ClassDB::bind_method(D_METHOD("add_channel", "channel"), &AudioMixerServer::add_channel);
	ClassDB::bind_method(D_METHOD("remove_channel", "channel"), &AudioMixerServer::remove_channel);
	ClassDB::bind_method(D_METHOD("has_channel", "channel"), &AudioMixerServer::has_channel);
	ClassDB::bind_method(D_METHOD("get_channel", "channel"), &AudioMixerServer::get_channel);
	ClassDB::bind_method(D_METHOD("get_channel_names"), &AudioMixerServer::get_channel_names);

	ClassDB::bind_method(D_METHOD("play", "channel", "stream", "from_position"), &AudioMixerServer::play, DEFVAL(Variant()), DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("stop", "channel", "fade_time"), &AudioMixerServer::stop, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("pause", "channel"), &AudioMixerServer::pause);
	ClassDB::bind_method(D_METHOD("resume", "channel"), &AudioMixerServer::resume);
	ClassDB::bind_method(D_METHOD("seek", "channel", "position"), &AudioMixerServer::seek);
	ClassDB::bind_method(D_METHOD("stop_all", "fade_time"), &AudioMixerServer::stop_all, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("is_playing", "channel"), &AudioMixerServer::is_playing);
	ClassDB::bind_method(D_METHOD("is_paused", "channel"), &AudioMixerServer::is_paused);
	ClassDB::bind_method(D_METHOD("get_playback_position", "channel"), &AudioMixerServer::get_playback_position);
	ClassDB::bind_method(D_METHOD("play_one_shot", "channel", "stream", "volume_db", "pitch_scale"), &AudioMixerServer::play_one_shot, DEFVAL(0.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("get_voice_count", "channel"), &AudioMixerServer::get_voice_count);

	ClassDB::bind_method(D_METHOD("enqueue", "channel", "stream", "at"), &AudioMixerServer::enqueue, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("dequeue", "channel", "index"), &AudioMixerServer::dequeue, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("clear_queue", "channel"), &AudioMixerServer::clear_queue);
	ClassDB::bind_method(D_METHOD("get_queue", "channel"), &AudioMixerServer::get_queue);
	ClassDB::bind_method(D_METHOD("set_queue_index", "channel", "index", "fade_time"), &AudioMixerServer::set_queue_index, DEFVAL(-1.0));
	ClassDB::bind_method(D_METHOD("play_next", "channel", "fade_time"), &AudioMixerServer::play_next, DEFVAL(-1.0));
	ClassDB::bind_method(D_METHOD("play_previous", "channel", "fade_time"), &AudioMixerServer::play_previous, DEFVAL(-1.0));
	ClassDB::bind_method(D_METHOD("get_queue_index", "channel"), &AudioMixerServer::get_queue_index);

	ClassDB::bind_method(D_METHOD("fade_in", "channel", "time", "curve"), &AudioMixerServer::fade_in, DEFVAL(FADE_CURVE_LINEAR));
	ClassDB::bind_method(D_METHOD("fade_out", "channel", "time", "curve"), &AudioMixerServer::fade_out, DEFVAL(FADE_CURVE_LINEAR));
	ClassDB::bind_method(D_METHOD("cross_fade", "channel", "stream", "time", "curve"), &AudioMixerServer::cross_fade, DEFVAL(FADE_CURVE_EQUAL_POWER));
	ClassDB::bind_method(D_METHOD("is_fading", "channel"), &AudioMixerServer::is_fading);
	ClassDB::bind_method(D_METHOD("get_fade_gain", "channel"), &AudioMixerServer::get_fade_gain);
	ClassDB::bind_method(D_METHOD("set_channel_volume_db", "channel", "volume_db", "fade_time"), &AudioMixerServer::set_channel_volume_db, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("get_channel_volume_db", "channel"), &AudioMixerServer::get_channel_volume_db);
	ClassDB::bind_method(D_METHOD("set_channel_pitch_scale", "channel", "pitch_scale"), &AudioMixerServer::set_channel_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_channel_pitch_scale", "channel"), &AudioMixerServer::get_channel_pitch_scale);
	ClassDB::bind_method(D_METHOD("switch_to_clip", "channel", "clip"), &AudioMixerServer::switch_to_clip);
	ClassDB::bind_method(D_METHOD("get_channel_playback", "channel"), &AudioMixerServer::get_channel_playback);

	ClassDB::bind_method(D_METHOD("execute", "command"), &AudioMixerServer::execute);
	ClassDB::bind_method(D_METHOD("execute_list", "list"), &AudioMixerServer::execute_list);
	ClassDB::bind_method(D_METHOD("register_commands", "list"), &AudioMixerServer::register_commands);
	ClassDB::bind_method(D_METHOD("unregister_commands", "list"), &AudioMixerServer::unregister_commands);
	ClassDB::bind_method(D_METHOD("has_command", "command_name"), &AudioMixerServer::has_command);
	ClassDB::bind_method(D_METHOD("execute_named", "command_name"), &AudioMixerServer::execute_named);
	ClassDB::bind_method(D_METHOD("execute_group", "group"), &AudioMixerServer::execute_group);
	ClassDB::bind_method(D_METHOD("get_command_names"), &AudioMixerServer::get_command_names);

	ADD_SIGNAL(MethodInfo("channel_started", PropertyInfo(Variant::STRING_NAME, "channel"), PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::STRING, "stream_path")));
	ADD_SIGNAL(MethodInfo("channel_finished", PropertyInfo(Variant::STRING_NAME, "channel"), PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("queue_finished", PropertyInfo(Variant::STRING_NAME, "channel")));
	ADD_SIGNAL(MethodInfo("fade_finished", PropertyInfo(Variant::STRING_NAME, "channel")));
	ADD_SIGNAL(MethodInfo("command_executed", PropertyInfo(Variant::STRING_NAME, "command_name"), PropertyInfo(Variant::OBJECT, "command", PROPERTY_HINT_RESOURCE_TYPE, "AudioCommand")));

	BIND_ENUM_CONSTANT(FADE_CURVE_LINEAR);
	BIND_ENUM_CONSTANT(FADE_CURVE_EASE_IN);
	BIND_ENUM_CONSTANT(FADE_CURVE_EASE_OUT);
	BIND_ENUM_CONSTANT(FADE_CURVE_EQUAL_POWER);
}

AudioMixerServer::AudioMixerServer() {
	singleton = this;
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "audio/mixer/default_layout", PROPERTY_HINT_FILE, "*.tres,*.res"), "");
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "audio/mixer/default_commands", PROPERTY_HINT_FILE, "*.tres,*.res,*.json,*.ini,*.csv"), "");
	command_registry = memnew(AudioCommandRegistry(this));
}

AudioMixerServer::~AudioMixerServer() {
	for (KeyValue<StringName, Channel> &E : channels) {
		_stop_channel(E.value, 0.0f, FADE_CURVE_LINEAR);
	}
	channels.clear();
	memdelete(command_registry);
	command_registry = nullptr;
	_remove_callback();
	singleton = nullptr;
}
