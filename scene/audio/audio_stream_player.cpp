/**************************************************************************/
/*  audio_stream_player.cpp                                               */
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

#include "audio_stream_player.h"
#include "audio_stream_player.compat.inc"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/audio/audio_stream_player_internal.h"
#include "scene/resources/audio/audio_stream.h"
#include "servers/audio/audio_server.h"
#include "servers/display/accessibility_server.h"
#include "servers/subtitle/subtitle.h"

void AudioStreamPlayer::_notification(int p_what) {
	if (p_what == NOTIFICATION_ACCESSIBILITY_UPDATE) {
		RID ae = get_accessibility_element();
		ERR_FAIL_COND(ae.is_null());

		AccessibilityServer::get_singleton()->update_set_role(ae, AccessibilityServerEnums::AccessibilityRole::ROLE_AUDIO);
	} else {
		internal->notification(p_what);
		if (internal->volumes_dirty) {
			set_volume_db(internal->volume_db); // Applies the new fade gains.
		}
	}
}

void AudioStreamPlayer::set_stream(Ref<AudioStream> p_stream) {
	internal->set_stream(p_stream);
}

bool AudioStreamPlayer::_set(const StringName &p_name, const Variant &p_value) {
	return internal->set(p_name, p_value);
}

bool AudioStreamPlayer::_get(const StringName &p_name, Variant &r_ret) const {
	return internal->get(p_name, r_ret);
}

void AudioStreamPlayer::_get_property_list(List<PropertyInfo> *p_list) const {
	internal->get_property_list(p_list);
}

Ref<AudioStream> AudioStreamPlayer::get_stream() const {
	return internal->stream;
}

void AudioStreamPlayer::set_subtitle_path(const String &p_subtitle_path) {
	internal->subtitle_path = p_subtitle_path;
}

String AudioStreamPlayer::get_subtitle_path() const {
	return internal->subtitle_path;
}

void AudioStreamPlayer::set_volume_db(float p_volume) {
	ERR_FAIL_COND_MSG(Math::is_nan(p_volume), "Volume can't be set to NaN.");
	internal->volume_db = p_volume;

	HashMap<StringName, Vector<AudioFrame>> bus_volumes;
	bus_volumes[internal->bus] = _get_volume_vector();
	internal->apply_bus_volumes(bus_volumes);
}

float AudioStreamPlayer::get_volume_db() const {
	return internal->volume_db;
}

void AudioStreamPlayer::set_volume_linear(float p_volume) {
	set_volume_db(Math::linear_to_db(p_volume));
}

float AudioStreamPlayer::get_volume_linear() const {
	return Math::db_to_linear(get_volume_db());
}

void AudioStreamPlayer::set_pitch_scale(float p_pitch_scale) {
	internal->set_pitch_scale(p_pitch_scale);
}

float AudioStreamPlayer::get_pitch_scale() const {
	return internal->pitch_scale;
}

void AudioStreamPlayer::set_max_polyphony(int p_max_polyphony) {
	internal->set_max_polyphony(p_max_polyphony);
}

int AudioStreamPlayer::get_max_polyphony() const {
	return internal->max_polyphony;
}

void AudioStreamPlayer::play(float p_from_pos) {
	Ref<AudioStreamPlayback> stream_playback = internal->play_basic();
	if (stream_playback.is_null()) {
		return;
	}
	AudioServer::get_singleton()->start_playback_stream(stream_playback, internal->bus, internal->get_faded_volumes(_get_volume_vector()), p_from_pos, internal->pitch_scale, !can_process());
	internal->ensure_playback_limit();

	// Sample handling.
	if (stream_playback->get_is_sample() && stream_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = stream_playback->get_sample_playback();
		sample_playback->offset = p_from_pos;
		sample_playback->volume_vector = internal->get_faded_volumes(_get_volume_vector());
		sample_playback->bus = get_bus();

		AudioServer::get_singleton()->start_sample_playback(sample_playback, !can_process());
	}
	internal->update_playing_state(); // After the start, so is_playing() is true in handlers.
}

void AudioStreamPlayer::seek(float p_seconds) {
	internal->seek(p_seconds);
}

void AudioStreamPlayer::stop() {
	internal->stop_basic();
}

bool AudioStreamPlayer::is_playing() const {
	return internal->is_playing();
}

float AudioStreamPlayer::get_playback_position() {
	return internal->get_playback_position();
}

void AudioStreamPlayer::set_bus(const StringName &p_bus) {
	internal->bus = p_bus;
	HashMap<StringName, Vector<AudioFrame>> bus_volumes;
	bus_volumes[p_bus] = _get_volume_vector();
	internal->apply_bus_volumes(bus_volumes);
}

StringName AudioStreamPlayer::get_bus() const {
	return internal->get_bus();
}

void AudioStreamPlayer::set_autoplay(bool p_enable) {
	internal->autoplay = p_enable;
}

bool AudioStreamPlayer::is_autoplay_enabled() const {
	return internal->autoplay;
}

void AudioStreamPlayer::set_mix_target(MixTarget p_target) {
	mix_target = p_target;
}

AudioStreamPlayer::MixTarget AudioStreamPlayer::get_mix_target() const {
	return mix_target;
}

void AudioStreamPlayer::_set_playing(bool p_enable) {
	internal->set_playing(p_enable);
}

void AudioStreamPlayer::set_stream_paused(bool p_pause) {
	internal->set_stream_paused(p_pause);
}

bool AudioStreamPlayer::get_stream_paused() const {
	return internal->get_stream_paused();
}

Vector<AudioFrame> AudioStreamPlayer::_get_volume_vector() {
	Vector<AudioFrame> volume_vector;
	// We need at most four stereo pairs (for 7.1 systems).
	volume_vector.resize(4);

	// Initialize the volume vector to zero.
	for (AudioFrame &channel_volume_db : volume_vector) {
		channel_volume_db = AudioFrame(0, 0);
	}

	float volume_linear = Math::db_to_linear(internal->volume_db);

	// Set the volume vector up according to the speaker mode and mix target.
	// TODO do we need to scale the volume down when we output to more channels?
	if (AudioServer::get_singleton()->get_speaker_mode() == AuSE::SPEAKER_MODE_STEREO) {
		volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
	} else {
		switch (mix_target) {
			case MIX_TARGET_STEREO: {
				volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
			} break;
			case MIX_TARGET_SURROUND: {
				// TODO Make sure this is right.
				volume_vector.write[0] = AudioFrame(volume_linear, volume_linear);
				volume_vector.write[1] = AudioFrame(volume_linear, /* LFE= */ 1.0f);
				volume_vector.write[2] = AudioFrame(volume_linear, volume_linear);
				volume_vector.write[3] = AudioFrame(volume_linear, volume_linear);
			} break;
			case MIX_TARGET_CENTER: {
				// TODO Make sure this is right.
				volume_vector.write[1] = AudioFrame(volume_linear, /* LFE= */ 1.0f);
			} break;
		}
	}
	return volume_vector;
}

bool AudioStreamPlayer::has_stream_playback() {
	return internal->has_stream_playback();
}

Ref<AudioStreamPlayback> AudioStreamPlayer::get_stream_playback() {
	return internal->get_stream_playback();
}

AuSE::PlaybackType AudioStreamPlayer::get_playback_type() const {
	return internal->get_playback_type();
}

void AudioStreamPlayer::set_playback_type(AuSE::PlaybackType p_playback_type) {
	internal->set_playback_type(p_playback_type);
}

void AudioStreamPlayer::fade_in(float p_duration, AudioMixerServer::FadeCurve p_curve) {
	internal->fade_in(p_duration, p_curve);
}

void AudioStreamPlayer::fade_out(float p_duration, AudioMixerServer::FadeCurve p_curve, bool p_stop_at_end) {
	internal->fade_out(p_duration, p_curve, p_stop_at_end);
}

void AudioStreamPlayer::crossfade_to(const Ref<AudioStream> &p_stream, float p_duration, AudioMixerServer::FadeCurve p_curve) {
	internal->crossfade_to(p_stream, p_duration, p_curve);
}

bool AudioStreamPlayer::is_fading() const {
	return internal->is_fading();
}

float AudioStreamPlayer::get_fade_gain() const {
	return internal->fade_gain;
}

int AudioStreamPlayer::get_current_track_index() {
	return internal->get_current_track_index();
}

Ref<AudioStream> AudioStreamPlayer::get_current_track_stream() {
	return internal->get_current_track_stream();
}

double AudioStreamPlayer::get_track_position() {
	return internal->get_track_position();
}

double AudioStreamPlayer::get_position_in_playlist() {
	return internal->get_position_in_playlist();
}

void AudioStreamPlayer::set_position_signal_interval(float p_interval) {
	internal->position_signal_interval = MAX(p_interval, 0.0f);
}

float AudioStreamPlayer::get_position_signal_interval() const {
	return internal->position_signal_interval;
}

void AudioStreamPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &AudioStreamPlayer::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &AudioStreamPlayer::get_stream);
	ClassDB::bind_method(D_METHOD("set_subtitle_path", "subtitle_path"), &AudioStreamPlayer::set_subtitle_path);
	ClassDB::bind_method(D_METHOD("get_subtitle_path"), &AudioStreamPlayer::get_subtitle_path);

	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioStreamPlayer::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioStreamPlayer::get_volume_db);

	ClassDB::bind_method(D_METHOD("set_volume_linear", "volume_linear"), &AudioStreamPlayer::set_volume_linear);
	ClassDB::bind_method(D_METHOD("get_volume_linear"), &AudioStreamPlayer::get_volume_linear);

	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &AudioStreamPlayer::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioStreamPlayer::get_pitch_scale);

	ClassDB::bind_method(D_METHOD("play", "from_position"), &AudioStreamPlayer::play, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("seek", "to_position"), &AudioStreamPlayer::seek);
	ClassDB::bind_method(D_METHOD("stop"), &AudioStreamPlayer::stop);

	ClassDB::bind_method(D_METHOD("is_playing"), &AudioStreamPlayer::is_playing);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &AudioStreamPlayer::get_playback_position);

	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &AudioStreamPlayer::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &AudioStreamPlayer::get_bus);

	ClassDB::bind_method(D_METHOD("set_autoplay", "enable"), &AudioStreamPlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &AudioStreamPlayer::is_autoplay_enabled);

	ClassDB::bind_method(D_METHOD("set_mix_target", "mix_target"), &AudioStreamPlayer::set_mix_target);
	ClassDB::bind_method(D_METHOD("get_mix_target"), &AudioStreamPlayer::get_mix_target);

	ClassDB::bind_method(D_METHOD("set_playing", "enable"), &AudioStreamPlayer::_set_playing);

	ClassDB::bind_method(D_METHOD("set_stream_paused", "pause"), &AudioStreamPlayer::set_stream_paused);
	ClassDB::bind_method(D_METHOD("get_stream_paused"), &AudioStreamPlayer::get_stream_paused);

	ClassDB::bind_method(D_METHOD("set_max_polyphony", "max_polyphony"), &AudioStreamPlayer::set_max_polyphony);
	ClassDB::bind_method(D_METHOD("get_max_polyphony"), &AudioStreamPlayer::get_max_polyphony);

	ClassDB::bind_method(D_METHOD("has_stream_playback"), &AudioStreamPlayer::has_stream_playback);
	ClassDB::bind_method(D_METHOD("get_stream_playback"), &AudioStreamPlayer::get_stream_playback);

	ClassDB::bind_method(D_METHOD("set_playback_type", "playback_type"), &AudioStreamPlayer::set_playback_type);
	ClassDB::bind_method(D_METHOD("get_playback_type"), &AudioStreamPlayer::get_playback_type);

	ClassDB::bind_method(D_METHOD("fade_in", "duration", "curve"), &AudioStreamPlayer::fade_in, DEFVAL(AudioMixerServer::FADE_CURVE_LINEAR));
	ClassDB::bind_method(D_METHOD("fade_out", "duration", "curve", "stop_at_end"), &AudioStreamPlayer::fade_out, DEFVAL(AudioMixerServer::FADE_CURVE_LINEAR), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("crossfade_to", "stream", "duration", "curve"), &AudioStreamPlayer::crossfade_to, DEFVAL(AudioMixerServer::FADE_CURVE_EQUAL_POWER));
	ClassDB::bind_method(D_METHOD("is_fading"), &AudioStreamPlayer::is_fading);
	ClassDB::bind_method(D_METHOD("get_fade_gain"), &AudioStreamPlayer::get_fade_gain);
	ClassDB::bind_method(D_METHOD("get_current_track_index"), &AudioStreamPlayer::get_current_track_index);
	ClassDB::bind_method(D_METHOD("get_current_track_stream"), &AudioStreamPlayer::get_current_track_stream);
	ClassDB::bind_method(D_METHOD("get_track_position"), &AudioStreamPlayer::get_track_position);
	ClassDB::bind_method(D_METHOD("get_position_in_playlist"), &AudioStreamPlayer::get_position_in_playlist);
	ClassDB::bind_method(D_METHOD("set_position_signal_interval", "interval"), &AudioStreamPlayer::set_position_signal_interval);
	ClassDB::bind_method(D_METHOD("get_position_signal_interval"), &AudioStreamPlayer::get_position_signal_interval);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, AudioStream::get_class_static()), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "subtitle_path", PROPERTY_HINT_FILE, SUBTITLE_PATH_HINT), "set_subtitle_path", "get_subtitle_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,or_greater,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_linear", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_volume_linear", "get_volume_linear");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing", PROPERTY_HINT_ONESHOT, "", PROPERTY_USAGE_EDITOR), "set_playing", "is_playing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream_paused", PROPERTY_HINT_NONE, ""), "set_stream_paused", "get_stream_paused");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mix_target", PROPERTY_HINT_ENUM, "Stereo,Surround,Center"), "set_mix_target", "get_mix_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_polyphony", PROPERTY_HINT_NONE, ""), "set_max_polyphony", "get_max_polyphony");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_AUDIO_BUS), "set_bus", "get_bus");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_type", PROPERTY_HINT_ENUM, "Default,Stream,Sample"), "set_playback_type", "get_playback_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "position_signal_interval", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater,suffix:s"), "set_position_signal_interval", "get_position_signal_interval");

	ADD_SIGNAL(MethodInfo("finished"));
	ADD_SIGNAL(MethodInfo("playing_changed", PropertyInfo(Variant::BOOL, "playing")));
	ADD_SIGNAL(MethodInfo("stream_changed", PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, AudioStream::get_class_static())));
	ADD_SIGNAL(MethodInfo("track_changed", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, AudioStream::get_class_static())));
	ADD_SIGNAL(MethodInfo("fade_finished"));
	ADD_SIGNAL(MethodInfo("position_changed", PropertyInfo(Variant::FLOAT, "position")));

	BIND_ENUM_CONSTANT(MIX_TARGET_STEREO);
	BIND_ENUM_CONSTANT(MIX_TARGET_SURROUND);
	BIND_ENUM_CONSTANT(MIX_TARGET_CENTER);
}

AudioStreamPlayer::AudioStreamPlayer() {
	internal = memnew(AudioStreamPlayerInternal(this, callable_mp(this, &AudioStreamPlayer::play), callable_mp(this, &AudioStreamPlayer::stop), false));
}

AudioStreamPlayer::~AudioStreamPlayer() {
	memdelete(internal);
}
