/**************************************************************************/
/*  audio_stream_player_internal.cpp                                      */
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

#include "audio_stream_player_internal.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/audio/audio_stream.h"
#include "servers/audio/audio_server.h"

void AudioStreamPlayerInternal::_set_process(bool p_enabled) {
	if (physical) {
		node->set_physics_process_internal(p_enabled);
	} else {
		node->set_process_internal(p_enabled);
	}
}

void AudioStreamPlayerInternal::_update_stream_parameters() {
	if (stream.is_null()) {
		return;
	}

	List<AudioStream::Parameter> parameters;
	stream->get_parameter_list(&parameters);
	for (const AudioStream::Parameter &K : parameters) {
		const PropertyInfo &pi = K.property;
		StringName key = PARAM_PREFIX + pi.name;
		if (!playback_parameters.has(key)) {
			ParameterData pd;
			pd.path = pi.name;
			pd.value = K.default_value;
			playback_parameters.insert(key, pd);
		}
	}
}

bool AudioStreamPlayerInternal::_is_sample() {
	return (AudioServer::get_singleton()->get_default_playback_type() == AuSE::PlaybackType::PLAYBACK_TYPE_SAMPLE && get_playback_type() == AuSE::PlaybackType::PLAYBACK_TYPE_DEFAULT) || get_playback_type() == AuSE::PlaybackType::PLAYBACK_TYPE_SAMPLE;
}

void AudioStreamPlayerInternal::process() {
	const double delta = physical ? node->get_physics_process_delta_time() : node->get_process_delta_time();

	// Fades. The signals are emitted after the state is updated, so handlers may start new fades.
	for (uint32_t i = 0; i < fading_out.size();) {
		FadingPlayback &fading = fading_out[i];
		if (fading.fade.advance(delta, fading.gain) || !fading.fade.active) {
			AudioServer::get_singleton()->stop_playback_stream(fading.playback);
			fading_out.remove_at_unordered(i);
			continue;
		}
		volumes_dirty = true;
		i++;
	}
	if (fade.active) {
		volumes_dirty = true;
		if (fade.advance(delta, fade_gain)) {
			if (fade_stop_at_end) {
				stop_callable.call();
			}
			node->emit_signal(SNAME("fade_finished"));
		}
	}

	// Track changes of playlists and the opt-in position signal.
	if (!stream_playbacks.is_empty()) {
		const Ref<AudioStreamPlayback> playback = stream_playbacks[stream_playbacks.size() - 1];
		AudioStreamPlayback::TrackInfo info;
		if (playback->get_track_info(info) && info.index >= 0 && (playback->get_instance_id() != track_playback || info.serial != track_serial)) {
			track_playback = playback->get_instance_id();
			track_serial = info.serial;
			node->emit_signal(SNAME("track_changed"), info.index, info.stream);
		}
		if (position_signal_interval > 0.0f) {
			position_signal_elapsed += delta;
			if (position_signal_elapsed >= position_signal_interval) {
				position_signal_elapsed = 0.0;
				node->emit_signal(SNAME("position_changed"), get_track_position());
			}
		}
	}

	Vector<Ref<AudioStreamPlayback>> playbacks_to_remove;
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		if (playback.is_valid() && !AudioServer::get_singleton()->is_playback_active(playback) && !AudioServer::get_singleton()->is_playback_paused(playback)) {
			playbacks_to_remove.push_back(playback);
		}
	}
	// Now go through and remove playbacks that have finished. Removing elements from a Vector in a range based for is asking for trouble.
	for (Ref<AudioStreamPlayback> &playback : playbacks_to_remove) {
		stream_playbacks.erase(playback);
	}
	if (!playbacks_to_remove.is_empty() && stream_playbacks.is_empty()) {
		// This node is no longer actively playing audio.
		active.clear();
		_stop_fading_out(); // The old stream of a crossfade ends with the new one.
		_set_process(false);
	}
	if (!playbacks_to_remove.is_empty()) {
		node->emit_signal(SceneStringName(finished));
	}
}

void AudioStreamPlayerInternal::ensure_playback_limit() {
	while (stream_playbacks.size() > max_polyphony) {
		AudioServer::get_singleton()->stop_playback_stream(stream_playbacks[0]);
		stream_playbacks.remove_at(0);
	}
}

void AudioStreamPlayerInternal::notification(int p_what) {
	switch (p_what) {
		case Node::NOTIFICATION_ENTER_TREE: {
			if (autoplay && !node->is_part_of_edited_scene()) {
				play_callable.call(0.0);
			}
			set_stream_paused(!node->can_process());
		} break;

		case Node::NOTIFICATION_EXIT_TREE: {
			set_stream_paused(true);
		} break;

		case Node::NOTIFICATION_INTERNAL_PROCESS: {
			process();
		} break;

		case Node::NOTIFICATION_PREDELETE: {
			for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
				AudioServer::get_singleton()->stop_playback_stream(playback);
			}
			stream_playbacks.clear();
		} break;

		case Node::NOTIFICATION_SUSPENDED:
		case Node::NOTIFICATION_PAUSED: {
			bool can_process = node->is_inside_tree() && node->can_process();
			if (!can_process) {
				// Node can't process so we start fading out to silence
				set_stream_paused(true);
			}
		} break;

		case Node::NOTIFICATION_UNSUSPENDED: {
			if (node->get_tree()->is_paused()) {
				break;
			}
			[[fallthrough]];
		}

		case Node::NOTIFICATION_UNPAUSED: {
			set_stream_paused(false);
		} break;
	}
}

Ref<AudioStreamPlayback> AudioStreamPlayerInternal::play_basic() {
	Ref<AudioStreamPlayback> stream_playback;
	if (stream.is_null()) {
		return stream_playback;
	}
	ERR_FAIL_COND_V_MSG(!node->is_inside_tree(), stream_playback, "Playback can only happen when a node is inside the scene tree");
	if (stream->is_monophonic() && is_playing()) {
		stop_callable.call();
	}
	stream_playback = stream->instantiate_playback();
	ERR_FAIL_COND_V_MSG(stream_playback.is_null(), stream_playback, "Failed to instantiate playback.");

	for (const KeyValue<StringName, ParameterData> &K : playback_parameters) {
		stream_playback->set_parameter(K.value.path, K.value.value);
	}

	// Sample handling.
	if (_is_sample()) {
		if (stream->can_be_sampled()) {
			stream_playback->set_is_sample(true);
			if (stream_playback->get_is_sample() && stream_playback->get_sample_playback().is_null()) {
				if (!AudioServer::get_singleton()->is_stream_registered_as_sample(stream)) {
					AudioServer::get_singleton()->register_stream_as_sample(stream);
				}
				Ref<AudioSamplePlayback> sample_playback;
				sample_playback.instantiate();
				sample_playback->stream = stream;
				sample_playback->pitch_scale = pitch_scale;
				stream_playback->set_sample_playback(sample_playback);
			}
		} else if (!stream->is_meta_stream()) {
			WARN_PRINT(vformat(R"(%s is trying to play a sample from a stream that cannot be sampled.)", node->get_path()));
		}
	}

	if (!keep_fade_on_play) {
		// A plain play() is not faded.
		fade.active = false;
		fade_gain = 1.0f;
	}
	position_signal_elapsed = 0.0;

	stream_playbacks.push_back(stream_playback);
	active.set();
	_set_process(true);
	return stream_playback;
}

void AudioStreamPlayerInternal::set_stream_paused(bool p_pause) {
	// TODO this does not have perfect recall, fix that maybe? If there are zero playbacks registered with the AudioServer, this bool isn't persisted.
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->set_playback_paused(playback, p_pause);
		if (_is_sample() && playback->get_sample_playback().is_valid()) {
			AudioServer::get_singleton()->set_sample_playback_pause(playback->get_sample_playback(), p_pause);
		}
	}
}

bool AudioStreamPlayerInternal::get_stream_paused() const {
	// There's currently no way to pause some playback streams but not others. Check the first and don't bother looking at the rest.
	if (!stream_playbacks.is_empty()) {
		return AudioServer::get_singleton()->is_playback_paused(stream_playbacks[0]);
	}
	return false;
}

bool AudioStreamPlayerInternal::set(const StringName &p_name, const Variant &p_value) {
	ParameterData *pd = playback_parameters.getptr(p_name);
	if (!pd) {
		return false;
	}
	pd->value = p_value;
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		playback->set_parameter(pd->path, pd->value);
	}
	return true;
}

bool AudioStreamPlayerInternal::get(const StringName &p_name, Variant &r_ret) const {
	const ParameterData *pd = playback_parameters.getptr(p_name);
	if (!pd) {
		return false;
	}
	r_ret = pd->value;
	return true;
}

void AudioStreamPlayerInternal::get_property_list(List<PropertyInfo> *p_list) const {
	if (stream.is_null()) {
		return;
	}
	List<AudioStream::Parameter> parameters;
	stream->get_parameter_list(&parameters);
	for (const AudioStream::Parameter &K : parameters) {
		PropertyInfo pi = K.property;
		pi.name = PARAM_PREFIX + pi.name;

		const ParameterData *pd = playback_parameters.getptr(pi.name);
		if (pd && pd->value == K.default_value) {
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
		}

		p_list->push_back(pi);
	}
}

void AudioStreamPlayerInternal::set_stream(Ref<AudioStream> p_stream) {
	stop_callable.call();
	_assign_stream(p_stream);
}

void AudioStreamPlayerInternal::_assign_stream(const Ref<AudioStream> &p_stream) {
	if (stream.is_valid()) {
		stream->disconnect(SNAME("parameter_list_changed"), callable_mp(this, &AudioStreamPlayerInternal::_update_stream_parameters));
	}
	const bool changed = stream != p_stream;
	stream = p_stream;
	_update_stream_parameters();
	if (stream.is_valid()) {
		stream->connect(SNAME("parameter_list_changed"), callable_mp(this, &AudioStreamPlayerInternal::_update_stream_parameters));
	}
	node->notify_property_list_changed();
	if (changed) {
		node->emit_signal(SNAME("stream_changed"), stream);
	}
}

void AudioStreamPlayerInternal::seek(float p_seconds) {
	if (is_playing()) {
		// A seek keeps a running fade.
		keep_fade_on_play = true;
		stop_callable.call();
		play_callable.call(p_seconds);
		keep_fade_on_play = false;
	}
}

void AudioStreamPlayerInternal::stop_basic() {
	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->stop_playback_stream(playback);
	}
	stream_playbacks.clear();
	_stop_fading_out();
	if (!keep_fade_on_play) {
		fade.active = false;
	}

	active.clear();
	_set_process(false);
}

void AudioStreamPlayerInternal::_stop_fading_out() {
	for (const FadingPlayback &fading : fading_out) {
		AudioServer::get_singleton()->stop_playback_stream(fading.playback);
	}
	fading_out.clear();
}

bool AudioStreamPlayerInternal::is_playing() const {
	for (const Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		if (AudioServer::get_singleton()->is_playback_active(playback)) {
			return true;
		}
	}
	return false;
}

float AudioStreamPlayerInternal::get_playback_position() {
	// Return the playback position of the most recently started playback stream.
	if (!stream_playbacks.is_empty()) {
		return AudioServer::get_singleton()->get_playback_position(stream_playbacks[stream_playbacks.size() - 1]);
	}
	return 0;
}

void AudioStreamPlayerInternal::set_playing(bool p_enable) {
	if (p_enable) {
		play_callable.call(0.0);
	} else {
		stop_callable.call();
	}
}

bool AudioStreamPlayerInternal::is_active() const {
	return active.is_set();
}

void AudioStreamPlayerInternal::set_pitch_scale(float p_pitch_scale) {
	ERR_FAIL_COND(p_pitch_scale <= 0.0);
	pitch_scale = p_pitch_scale;

	for (Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		AudioServer::get_singleton()->set_playback_pitch_scale(playback, pitch_scale);
	}
}

void AudioStreamPlayerInternal::set_max_polyphony(int p_max_polyphony) {
	if (p_max_polyphony > 0) {
		max_polyphony = p_max_polyphony;
	}
}

bool AudioStreamPlayerInternal::has_stream_playback() {
	return !stream_playbacks.is_empty();
}

Ref<AudioStreamPlayback> AudioStreamPlayerInternal::get_stream_playback() {
	ERR_FAIL_COND_V_MSG(stream_playbacks.is_empty(), Ref<AudioStreamPlayback>(), "Player is inactive. Call play() before requesting get_stream_playback().");
	return stream_playbacks[stream_playbacks.size() - 1];
}

void AudioStreamPlayerInternal::set_playback_type(AuSE::PlaybackType p_playback_type) {
	playback_type = p_playback_type;
}

AuSE::PlaybackType AudioStreamPlayerInternal::get_playback_type() const {
	return playback_type;
}

void AudioStreamPlayerInternal::fade_in(float p_duration, int p_curve) {
	if (!is_playing()) {
		// Start silent; the node applies the gain to the first mix block.
		fade_gain = 0.0f;
		keep_fade_on_play = true;
		play_callable.call(0.0);
		keep_fade_on_play = false;
		if (stream_playbacks.is_empty()) {
			fade_gain = 1.0f; // Nothing to play.
			return;
		}
	}
	fade.start(fade_gain, 1.0f, p_duration, p_curve);
	fade.active = true; // A duration of 0 finishes in the next frame and emits `fade_finished` there.
	fade_stop_at_end = false;
}

void AudioStreamPlayerInternal::fade_out(float p_duration, int p_curve, bool p_stop_at_end) {
	if (stream_playbacks.is_empty()) {
		return;
	}
	fade.start(fade_gain, 0.0f, p_duration, p_curve);
	fade.active = true;
	fade_stop_at_end = p_stop_at_end;
}

void AudioStreamPlayerInternal::crossfade_to(const Ref<AudioStream> &p_stream, float p_duration, int p_curve) {
	ERR_FAIL_COND_MSG(p_stream.is_null(), "Cannot crossfade to a null stream; use fade_out() to fade to silence.");
	// The current playbacks fade out from their gain and stop at the end; the new stream fades in from 0.
	for (const Ref<AudioStreamPlayback> &playback : stream_playbacks) {
		FadingPlayback fading;
		fading.playback = playback;
		fading.gain = fade_gain;
		fading.fade.start(fade_gain, 0.0f, p_duration, p_curve);
		fading.fade.active = true;
		fading_out.push_back(fading);
	}
	stream_playbacks.clear();
	_assign_stream(p_stream);
	fade_in(p_duration, p_curve);
}

bool AudioStreamPlayerInternal::is_fading() const {
	return fade.active || !fading_out.is_empty();
}

static HashMap<StringName, Vector<AudioFrame>> _scale_bus_volumes(const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes, float p_gain) {
	HashMap<StringName, Vector<AudioFrame>> scaled(p_bus_volumes);
	for (KeyValue<StringName, Vector<AudioFrame>> &E : scaled) {
		for (AudioFrame &frame : E.value) {
			frame *= p_gain;
		}
	}
	return scaled;
}

void AudioStreamPlayerInternal::apply_bus_volumes(const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes) {
	volumes_dirty = false;
	AudioServer *audio_server = AudioServer::get_singleton();
	if (!stream_playbacks.is_empty()) {
		// Unfaded players pass the map through, as before.
		const HashMap<StringName, Vector<AudioFrame>> faded = fade_gain == 1.0f ? HashMap<StringName, Vector<AudioFrame>>() : _scale_bus_volumes(p_bus_volumes, fade_gain);
		for (const Ref<AudioStreamPlayback> &playback : stream_playbacks) {
			audio_server->set_playback_bus_volumes_linear(playback, fade_gain == 1.0f ? p_bus_volumes : faded);
		}
	}
	for (const FadingPlayback &fading : fading_out) {
		audio_server->set_playback_bus_volumes_linear(fading.playback, _scale_bus_volumes(p_bus_volumes, fading.gain));
	}
}

Vector<AudioFrame> AudioStreamPlayerInternal::get_faded_volumes(const Vector<AudioFrame> &p_volumes) const {
	Vector<AudioFrame> faded = p_volumes;
	if (fade_gain != 1.0f) {
		for (AudioFrame &frame : faded) {
			frame *= fade_gain;
		}
	}
	return faded;
}

int AudioStreamPlayerInternal::get_current_track_index() {
	if (stream_playbacks.is_empty()) {
		return -1;
	}
	AudioStreamPlayback::TrackInfo info;
	return stream_playbacks[stream_playbacks.size() - 1]->get_track_info(info) ? info.index : 0;
}

Ref<AudioStream> AudioStreamPlayerInternal::get_current_track_stream() {
	if (stream_playbacks.is_empty()) {
		return Ref<AudioStream>();
	}
	AudioStreamPlayback::TrackInfo info;
	return stream_playbacks[stream_playbacks.size() - 1]->get_track_info(info) ? info.stream : stream;
}

double AudioStreamPlayerInternal::get_track_position() {
	if (stream_playbacks.is_empty()) {
		return 0.0;
	}
	AudioStreamPlayback::TrackInfo info;
	return stream_playbacks[stream_playbacks.size() - 1]->get_track_info(info) ? info.position : (double)get_playback_position();
}

StringName AudioStreamPlayerInternal::get_bus() const {
	const String bus_name = bus;
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		if (AudioServer::get_singleton()->get_bus_name(i) == bus_name) {
			return bus;
		}
	}
	return SceneStringName(Master);
}

AudioStreamPlayerInternal::AudioStreamPlayerInternal(Node *p_node, const Callable &p_play_callable, const Callable &p_stop_callable, bool p_physical) {
	node = p_node;
	play_callable = p_play_callable;
	stop_callable = p_stop_callable;
	physical = p_physical;
	bus = SceneStringName(Master);

	AudioServer::get_singleton()->connect("bus_layout_changed", callable_mp((Object *)node, &Object::notify_property_list_changed));
	AudioServer::get_singleton()->connect("bus_renamed", callable_mp((Object *)node, &Object::notify_property_list_changed).unbind(3));
}
