/**************************************************************************/
/*  audio_mixer_ducking.cpp                                               */
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

// Ducking of AudioMixerServer channels while TTSServer speaks, and manual ducking for dialog
// that plays as normal audio. Ducking is an offset in dB per channel with its own fade; it is
// added to the channel volume, so it multiplies with volume fades and voice fades.

#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "scene/audio/audio_mixer_server.h"
#include "servers/tts/tts_server.h"

#define DUCKING_THREAD_GUARD() \
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "AudioMixerServer can only be used from the main thread.")

#define DUCKING_THREAD_GUARD_V(m_ret) \
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), m_ret, "AudioMixerServer can only be used from the main thread.")

// While utterances are active, TTSServer.is_speaking() is polled at this interval (seconds).
static constexpr double TTS_POLL_INTERVAL = 0.25;
// After this long without speech reported by the OS, the active utterances count as finished
// (safeguard against a lost utterance_ended or utterance_canceled event).
static constexpr double TTS_SILENT_TIMEOUT = 1.0;

void AudioMixerServer::_connect_tts() {
	TTSServer *tts = TTSServer::get_singleton();
	if (tts == nullptr) {
		return;
	}
	tts->connect(SNAME("utterance_started"), callable_mp(this, &AudioMixerServer::_on_tts_utterance_started));
	tts->connect(SNAME("utterance_ended"), callable_mp(this, &AudioMixerServer::_on_tts_utterance_finished));
	tts->connect(SNAME("utterance_canceled"), callable_mp(this, &AudioMixerServer::_on_tts_utterance_finished));
}

void AudioMixerServer::_disconnect_tts() {
	TTSServer *tts = TTSServer::get_singleton();
	if (tts == nullptr) {
		return;
	}
	const Callable started = callable_mp(this, &AudioMixerServer::_on_tts_utterance_started);
	const Callable finished = callable_mp(this, &AudioMixerServer::_on_tts_utterance_finished);
	if (tts->is_connected(SNAME("utterance_started"), started)) {
		tts->disconnect(SNAME("utterance_started"), started);
	}
	if (tts->is_connected(SNAME("utterance_ended"), finished)) {
		tts->disconnect(SNAME("utterance_ended"), finished);
	}
	if (tts->is_connected(SNAME("utterance_canceled"), finished)) {
		tts->disconnect(SNAME("utterance_canceled"), finished);
	}
}

void AudioMixerServer::_on_tts_utterance_started(int64_t p_id) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &AudioMixerServer::_on_tts_utterance_started).call_deferred(p_id);
		return;
	}
	const bool was_speaking = !active_utterances.is_empty();
	active_utterances.insert(p_id);
	tts_poll_elapsed = 0.0;
	tts_silent_time = 0.0;
	if (!was_speaking) {
		_update_all_ducks();
	}
}

void AudioMixerServer::_on_tts_utterance_finished(int64_t p_id) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &AudioMixerServer::_on_tts_utterance_finished).call_deferred(p_id);
		return;
	}
	// Unknown ids (utterances that never started, or an event that arrives twice) change nothing.
	if (!active_utterances.erase(p_id)) {
		return;
	}
	if (active_utterances.is_empty()) {
		_update_all_ducks();
	}
}

bool AudioMixerServer::_is_tts_ducked(const Channel &p_channel) const {
	return ducking_enabled && !active_utterances.is_empty() && p_channel.config.is_valid() && p_channel.config->is_duck_on_tts();
}

void AudioMixerServer::_update_duck(const StringName &p_name, Channel &p_channel, float p_fade_time) {
	const bool tts_ducked = _is_tts_ducked(p_channel);
	// A manual duck and a TTS duck do not add up: the lower volume wins.
	float target = 0.0f;
	if (p_channel.manual_duck) {
		target = MIN(target, p_channel.manual_duck_db);
	}
	if (tts_ducked) {
		target = MIN(target, p_channel.config->get_duck_volume_db());
	}

	if (target != p_channel.duck_target_db) {
		p_channel.duck_target_db = target;
		if (p_fade_time <= 0.0f || (p_channel.voices.is_empty() && p_channel.one_shots.is_empty())) {
			// No fade wanted, or nothing plays: jump to the target.
			p_channel.duck_fade.active = false;
			p_channel.duck_db = target;
			for (const Voice &voice : p_channel.voices) {
				_apply_voice_volume(p_channel, voice);
			}
			for (const Voice &voice : p_channel.one_shots) {
				_apply_voice_volume(p_channel, voice);
			}
		} else {
			p_channel.duck_fade.active = true;
			p_channel.duck_fade.from = p_channel.duck_db;
			p_channel.duck_fade.to = target;
			p_channel.duck_fade.time = p_fade_time;
			p_channel.duck_fade.elapsed = 0.0;
			p_channel.duck_fade.curve = FADE_CURVE_LINEAR;
		}
	}

	const bool is_now_ducked = p_channel.manual_duck || tts_ducked;
	if (is_now_ducked != p_channel.ducked) {
		p_channel.ducked = is_now_ducked;
		_queue_signal(SIGNAL_DUCKING_CHANGED, p_name, is_now_ducked ? 1 : 0);
	}
}

void AudioMixerServer::_update_all_ducks() {
	for (KeyValue<StringName, Channel> &E : channels) {
		_update_duck(E.key, E.value, E.value.config.is_valid() ? E.value.config->get_duck_fade_time() : 0.0f);
	}
	_flush_signals();
}

bool AudioMixerServer::_process_duck(const StringName &p_name, Channel &p_channel, double p_delta) {
	// Picks up changes of duck_on_tts and duck_volume_db made while the channel exists.
	_update_duck(p_name, p_channel, p_channel.config->get_duck_fade_time());
	if (!p_channel.duck_fade.active) {
		return false;
	}
	p_channel.duck_fade.elapsed += p_delta;
	p_channel.duck_db = _apply_curve(p_channel.duck_fade);
	if (p_channel.duck_fade.elapsed >= p_channel.duck_fade.time) {
		p_channel.duck_fade.active = false;
		p_channel.duck_db = p_channel.duck_fade.to;
	}
	return true;
}

void AudioMixerServer::_process_utterance_safeguard(double p_delta) {
	if (active_utterances.is_empty()) {
		tts_poll_elapsed = 0.0;
		tts_silent_time = 0.0;
		return;
	}
	TTSServer *tts = TTSServer::get_singleton();
	if (tts == nullptr || !tts->is_available()) {
		return; // Without TTS, utterance events come only from tests or custom code.
	}
	tts_poll_elapsed += p_delta;
	if (tts_poll_elapsed < TTS_POLL_INTERVAL) {
		return;
	}
	const double elapsed = tts_poll_elapsed;
	tts_poll_elapsed = 0.0;
	if (tts->is_speaking() || tts->is_paused()) {
		tts_silent_time = 0.0;
		return;
	}
	tts_silent_time += elapsed;
	if (tts_silent_time >= TTS_SILENT_TIMEOUT) {
		// The OS lost an end event: restore the channels.
		active_utterances.clear();
		tts_silent_time = 0.0;
		_update_all_ducks();
	}
}

void AudioMixerServer::set_ducking_enabled(bool p_enabled) {
	DUCKING_THREAD_GUARD();
	if (ducking_enabled == p_enabled) {
		return;
	}
	ducking_enabled = p_enabled;
	_update_all_ducks();
}

bool AudioMixerServer::is_ducking_enabled() const {
	return ducking_enabled;
}

void AudioMixerServer::duck(const StringName &p_channel, float p_volume_db, float p_fade_time) {
	DUCKING_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	channel->manual_duck = true;
	channel->manual_duck_db = CLAMP(p_volume_db, -80.0f, 0.0f);
	_update_duck(p_channel, *channel, p_fade_time < 0.0f ? channel->config->get_duck_fade_time() : p_fade_time);
	_flush_signals();
}

void AudioMixerServer::unduck(const StringName &p_channel, float p_fade_time) {
	DUCKING_THREAD_GUARD();
	Channel *channel = _get_channel(p_channel);
	if (channel == nullptr) {
		return;
	}
	channel->manual_duck = false;
	channel->manual_duck_db = 0.0f;
	// A TTS duck that is still active stays.
	_update_duck(p_channel, *channel, p_fade_time < 0.0f ? channel->config->get_duck_fade_time() : p_fade_time);
	_flush_signals();
}

bool AudioMixerServer::is_ducked(const StringName &p_channel) const {
	DUCKING_THREAD_GUARD_V(false);
	const Channel *channel = _get_channel(p_channel);
	return channel != nullptr && channel->ducked;
}

float AudioMixerServer::get_duck_volume_db(const StringName &p_channel) const {
	DUCKING_THREAD_GUARD_V(0.0f);
	const Channel *channel = _get_channel(p_channel);
	return channel != nullptr ? channel->duck_db : 0.0f;
}

int AudioMixerServer::get_active_utterance_count() const {
	return active_utterances.size();
}
