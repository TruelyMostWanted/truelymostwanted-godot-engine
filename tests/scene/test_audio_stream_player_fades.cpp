/**************************************************************************/
/*  test_audio_stream_player_fades.cpp                                    */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_audio_stream_player_fades)

#include "scene/audio/audio_fade.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "servers/audio/audio_driver.h"
#include "servers/audio/audio_server.h"
#include "tests/signal_watcher.h"

namespace TestAudioStreamPlayerFades {

static Ref<AudioStreamWAV> make_wav(double p_seconds) {
	// Silent 16-bit mono WAV; the tests check state and gains, not sound.
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(44100);
	wav->set_stereo(false);
	Vector<uint8_t> data;
	data.resize(MAX(2, int(p_seconds * 44100.0) * 2));
	memset(data.ptrw(), 0, data.size());
	wav->set_data(data);
	return wav;
}

static AudioStreamPlayer *make_player(const Ref<AudioStream> &p_stream) {
	AudioStreamPlayer *player = memnew(AudioStreamPlayer);
	player->set_stream(p_stream);
	SceneTree::get_singleton()->get_root()->add_child(player);
	return player;
}

static Array one_emission(const Array &p_args = Array()) {
	Array emissions;
	emissions.push_back(p_args);
	return emissions;
}

TEST_CASE("[AudioFade] Curves, start and advance") {
	AudioFade fade;
	fade.start(0.0f, 1.0f, 1.0, AudioFade::CURVE_LINEAR);
	CHECK(fade.active);
	float value = 0.0f;
	CHECK_FALSE(fade.advance(0.25, value));
	CHECK(value == doctest::Approx(0.25));
	CHECK(fade.advance(1.0, value));
	CHECK(value == doctest::Approx(1.0));
	CHECK_FALSE(fade.active);

	// Equal power: a rising and a falling gain keep the summed power at 1.
	AudioFade rise;
	AudioFade fall;
	rise.start(0.0f, 1.0f, 1.0, AudioFade::CURVE_EQUAL_POWER);
	fall.start(1.0f, 0.0f, 1.0, AudioFade::CURVE_EQUAL_POWER);
	float up = 0.0f;
	float down = 0.0f;
	rise.advance(0.3, up);
	fall.advance(0.3, down);
	CHECK(up * up + down * down == doctest::Approx(1.0).epsilon(0.001));

	// A time of 0 ends at once.
	fade.start(0.0f, 0.5f, 0.0, AudioFade::CURVE_EASE_IN);
	CHECK_FALSE(fade.active);
	CHECK(fade.get_value() == doctest::Approx(0.5));
}

TEST_CASE("[SceneTree][Audio][AudioStreamPlayer] fade_in starts silent and rises, fade_out stops") {
	AudioStreamPlayer *player = make_player(make_wav(10.0));
	SIGNAL_WATCH(player, "fade_finished");

	player->fade_in(1.0f, AudioMixerServer::FADE_CURVE_LINEAR);
	CHECK(player->is_playing());
	CHECK(player->is_fading());
	CHECK(player->get_fade_gain() == doctest::Approx(0.0));
	SceneTree::get_singleton()->process(0.5);
	CHECK(player->get_fade_gain() == doctest::Approx(0.5));
	CHECK(player->get_volume_db() == doctest::Approx(0.0)); // The fade does not touch volume_db.
	SIGNAL_CHECK_FALSE("fade_finished");
	SceneTree::get_singleton()->process(0.6);
	CHECK(player->get_fade_gain() == doctest::Approx(1.0));
	CHECK_FALSE(player->is_fading());
	SIGNAL_CHECK("fade_finished", one_emission());

	// fade_out without stopping keeps the playback muted.
	player->fade_out(0.5f, AudioMixerServer::FADE_CURVE_EASE_OUT, false);
	SceneTree::get_singleton()->process(0.6);
	CHECK(player->get_fade_gain() == doctest::Approx(0.0));
	CHECK(player->is_playing());
	SIGNAL_CHECK("fade_finished", one_emission());

	// fade_in from the muted state, then fade_out with stop_at_end.
	player->fade_in(0.2f);
	SceneTree::get_singleton()->process(0.3);
	CHECK(player->get_fade_gain() == doctest::Approx(1.0));
	SIGNAL_DISCARD("fade_finished");
	player->fade_out(0.2f);
	SceneTree::get_singleton()->process(0.3);
	CHECK_FALSE(player->is_playing());
	SIGNAL_CHECK("fade_finished", one_emission());

	// A plain play() is not faded.
	player->play();
	CHECK(player->get_fade_gain() == doctest::Approx(1.0));
	CHECK_FALSE(player->is_fading());

	SIGNAL_UNWATCH(player, "fade_finished");
	memdelete(player);
}

TEST_CASE("[SceneTree][Audio][AudioStreamPlayer] crossfade_to swaps the stream and fades both") {
	Ref<AudioStreamWAV> first = make_wav(10.0);
	Ref<AudioStreamWAV> second = make_wav(10.0);
	AudioStreamPlayer *player = make_player(first);
	player->play();
	SIGNAL_WATCH(player, "stream_changed");
	SIGNAL_WATCH(player, "fade_finished");

	player->crossfade_to(second, 1.0f);
	CHECK(player->get_stream() == second);
	CHECK(player->is_playing());
	CHECK(player->is_fading());
	CHECK(player->get_fade_gain() == doctest::Approx(0.0));
	Array args;
	args.push_back(second);
	SIGNAL_CHECK("stream_changed", one_emission(args));

	SceneTree::get_singleton()->process(0.5);
	// Equal power: sin(45 degrees).
	CHECK(player->get_fade_gain() == doctest::Approx(Math::sin(Math::PI * 0.25)).epsilon(0.001));
	SceneTree::get_singleton()->process(0.6);
	CHECK(player->get_fade_gain() == doctest::Approx(1.0));
	CHECK_FALSE(player->is_fading()); // The old playback has stopped.
	CHECK(player->is_playing());
	SIGNAL_CHECK("fade_finished", one_emission());

	// Setting the same stream again emits nothing.
	player->set_stream(second);
	SIGNAL_CHECK_FALSE("stream_changed");

	SIGNAL_UNWATCH(player, "stream_changed");
	SIGNAL_UNWATCH(player, "fade_finished");
	memdelete(player);
}

TEST_CASE("[SceneTree][Audio][AudioStreamPlayer] Track info of a single stream and the position signal") {
	Ref<AudioStreamWAV> wav = make_wav(10.0);
	AudioStreamPlayer *player = make_player(wav);
	CHECK(player->get_current_track_index() == -1);
	CHECK(player->get_current_track_stream().is_null());
	CHECK(player->get_position_signal_interval() == doctest::Approx(0.0));

	SIGNAL_WATCH(player, "position_changed");
	SIGNAL_WATCH(player, "track_changed");
	player->play();
	CHECK(player->get_current_track_index() == 0);
	CHECK(player->get_current_track_stream() == wav);
	SceneTree::get_singleton()->process(0.5);
	SIGNAL_CHECK_FALSE("position_changed"); // Off by default.
	SIGNAL_CHECK_FALSE("track_changed"); // Single streams have no tracks.

	player->set_position_signal_interval(0.25f);
	SceneTree::get_singleton()->process(0.1);
	SIGNAL_CHECK_FALSE("position_changed");
	SceneTree::get_singleton()->process(0.2);
	CHECK_FALSE(SignalWatcher::get_singleton()->check_false("position_changed")); // Fired once (value: the position).

	player->set_position_signal_interval(-1.0f);
	CHECK(player->get_position_signal_interval() == doctest::Approx(0.0));

	SIGNAL_UNWATCH(player, "position_changed");
	SIGNAL_UNWATCH(player, "track_changed");
	memdelete(player);
}

} // namespace TestAudioStreamPlayerFades
