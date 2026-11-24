/**************************************************************************/
/*  test_audio_mixer_ducking.cpp                                          */
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

TEST_FORCE_LINK(test_audio_mixer_ducking)

#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "scene/audio/audio_mixer_server.h"
#include "scene/resources/audio/audio_mixer_channel.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "servers/display/display_server.h"
#include "servers/tts/tts_server.h"
#include "tests/signal_watcher.h"

namespace TestAudioMixerDucking {

// Removes all channels and active utterances at the end of a test case, also when a REQUIRE fails.
struct DuckingReset {
	~DuckingReset() {
		MessageQueue::get_singleton()->flush();
		if (AudioMixerServer::get_singleton()) {
			AudioMixerServer::get_singleton()->reset();
		}
		// Stopped playbacks are removed by the audio thread on its next mix step.
		OS::get_singleton()->delay_usec(100000);
	}
};

static Ref<AudioStreamWAV> make_wav(double p_seconds) {
	// Silent 16-bit mono WAV; the tests check state, not sound.
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

static Ref<AudioMixerChannel> make_channel(const StringName &p_name, bool p_duck_on_tts) {
	Ref<AudioMixerChannel> channel;
	channel.instantiate();
	channel->set_channel_name(p_name);
	channel->set_duck_on_tts(p_duck_on_tts);
	return channel;
}

// Simulates the operating system: the event goes through TTSServer like a real one
// (deferred to the main thread), no speech engine is needed.
static void post_utterance(DisplayServerEnums::TTSUtteranceEvent p_event, int64_t p_id) {
	TTSServer::get_singleton()->post_utterance_event(p_event, p_id, 0);
	MessageQueue::get_singleton()->flush();
}

static Array ducking_args(const StringName &p_channel, bool p_ducked) {
	Array args;
	args.push_back(p_channel);
	args.push_back(p_ducked);
	return args;
}

TEST_CASE("[AudioMixerServer] AudioMixerChannel ducking defaults and value ranges") {
	Ref<AudioMixerChannel> channel;
	channel.instantiate();
	CHECK_FALSE(channel->is_duck_on_tts());
	CHECK(channel->get_duck_volume_db() == doctest::Approx(-12.0));
	CHECK(channel->get_duck_fade_time() == doctest::Approx(0.25));

	channel->set_duck_volume_db(6.0f);
	CHECK(channel->get_duck_volume_db() == doctest::Approx(0.0));
	channel->set_duck_volume_db(-200.0f);
	CHECK(channel->get_duck_volume_db() == doctest::Approx(-80.0));
	channel->set_duck_fade_time(-1.0f);
	CHECK(channel->get_duck_fade_time() == doctest::Approx(0.0));
}

TEST_CASE("[AudioMixerServer] TTS utterances duck until the last one ends") {
	DuckingReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	REQUIRE(TTSServer::get_singleton() != nullptr);
	MessageQueue::get_singleton()->flush();

	mixer->add_channel(make_channel("music", true));
	mixer->add_channel(make_channel("sfx", false));
	CHECK(mixer->is_ducking_enabled());
	CHECK(mixer->get_active_utterance_count() == 0);
	CHECK_FALSE(mixer->is_ducked("music"));

	SIGNAL_WATCH(mixer, "ducking_changed");

	// Two overlapping utterances. Nothing plays, so the offset jumps to the target.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 1);
	CHECK(mixer->get_active_utterance_count() == 1);
	CHECK(mixer->is_ducked("music"));
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-12.0));
	CHECK_FALSE(mixer->is_ducked("sfx"));
	CHECK(mixer->get_duck_volume_db("sfx") == doctest::Approx(0.0));
	Array ducked;
	ducked.push_back(ducking_args("music", true));
	SIGNAL_CHECK("ducking_changed", ducked);

	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 2);
	CHECK(mixer->get_active_utterance_count() == 2);
	SIGNAL_CHECK_FALSE("ducking_changed");

	// Ending the first one keeps the channel ducked; unknown ids change nothing.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 1);
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 1);
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_CANCELED, 99);
	CHECK(mixer->get_active_utterance_count() == 1);
	CHECK(mixer->is_ducked("music"));
	SIGNAL_CHECK_FALSE("ducking_changed");

	// The last one is canceled (TTSServer.stop()): the channel is restored.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_CANCELED, 2);
	CHECK(mixer->get_active_utterance_count() == 0);
	CHECK_FALSE(mixer->is_ducked("music"));
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(0.0));
	Array restored;
	restored.push_back(ducking_args("music", false));
	SIGNAL_CHECK("ducking_changed", restored);

	// A channel added during speech starts ducked.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 3);
	mixer->add_channel(make_channel("ambience", true));
	CHECK(mixer->is_ducked("ambience"));
	CHECK(mixer->get_duck_volume_db("ambience") == doctest::Approx(-12.0));
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 3);
	CHECK_FALSE(mixer->is_ducked("ambience"));

	SIGNAL_UNWATCH(mixer, "ducking_changed");
}

TEST_CASE("[AudioMixerServer] Global switch and manual ducking") {
	DuckingReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	MessageQueue::get_singleton()->flush();

	Ref<AudioMixerChannel> music = make_channel("music", true);
	music->set_duck_volume_db(-20.0f);
	mixer->add_channel(music);
	mixer->add_channel(make_channel("ambience", false));

	// Disabled: utterances are counted, but nothing ducks.
	mixer->set_ducking_enabled(false);
	CHECK_FALSE(mixer->is_ducking_enabled());
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 1);
	CHECK(mixer->get_active_utterance_count() == 1);
	CHECK_FALSE(mixer->is_ducked("music"));

	// Enabled during speech: ducks at once; disabled again: restores.
	mixer->set_ducking_enabled(true);
	CHECK(mixer->is_ducked("music"));
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-20.0));
	mixer->set_ducking_enabled(false);
	CHECK_FALSE(mixer->is_ducked("music"));
	mixer->set_ducking_enabled(true);

	// Manual and TTS ducks do not add up: the lower volume wins.
	mixer->duck("music", -6.0f);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-20.0));
	mixer->duck("music", -30.0f);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-30.0));
	mixer->unduck("music");
	CHECK(mixer->is_ducked("music")); // Still speaking.
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-20.0));
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 1);
	CHECK_FALSE(mixer->is_ducked("music"));

	// Manual ducks work on any channel and do not depend on the global switch.
	mixer->set_ducking_enabled(false);
	mixer->duck("ambience", -9.0f, 0.0f);
	CHECK(mixer->is_ducked("ambience"));
	CHECK(mixer->get_duck_volume_db("ambience") == doctest::Approx(-9.0));
	mixer->duck("ambience", 5.0f, 0.0f); // Clamped: ducking only lowers.
	CHECK(mixer->get_duck_volume_db("ambience") == doctest::Approx(0.0));
	mixer->unduck("ambience", 0.0f);
	CHECK_FALSE(mixer->is_ducked("ambience"));

	// Settings changed on an added channel apply on the next frame.
	music->set_duck_on_tts(false);
	mixer->set_ducking_enabled(true);
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 2);
	CHECK_FALSE(mixer->is_ducked("music"));
	music->set_duck_on_tts(true);
	mixer->process(0.01);
	CHECK(mixer->is_ducked("music"));
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 2);

	ERR_PRINT_OFF;
	mixer->duck("missing", -6.0f);
	CHECK_FALSE(mixer->is_ducked("missing"));
	ERR_PRINT_ON;
}

TEST_CASE("[Audio][AudioMixerServer] Ducking fades a playing channel down and back up") {
	DuckingReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	MessageQueue::get_singleton()->flush();

	Ref<AudioMixerChannel> music = make_channel("music", true);
	music->set_duck_volume_db(-12.0f);
	music->set_duck_fade_time(0.5f);
	mixer->add_channel(music);
	mixer->set_channel_volume_db("music", -3.0f);
	mixer->play("music", make_wav(10.0));
	REQUIRE(mixer->is_playing("music"));

	// TTSServer.speak() starts: the channel fades down by duck_volume_db.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 1);
	CHECK(mixer->is_ducked("music"));
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(0.0));
	mixer->process(0.25);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-6.0));
	mixer->process(0.3);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-12.0));
	// The channel volume itself is unchanged; the offset is added to it.
	CHECK(mixer->get_channel_volume_db("music") == doctest::Approx(-3.0));
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(1.0));

	// TTSServer.stop() during speech: the platform cancels the utterance, the volume returns.
	TTSServer::get_singleton()->stop();
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_CANCELED, 1);
	CHECK_FALSE(mixer->is_ducked("music"));
	mixer->process(0.25);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-6.0));
	mixer->process(0.3);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(0.0));
	CHECK(mixer->is_playing("music"));

	// The duck fade also runs while the channel is paused.
	mixer->pause("music");
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 2);
	mixer->process(0.6);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-12.0));
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 2);
	mixer->process(0.6);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(0.0));
	mixer->resume("music");
	CHECK(mixer->is_playing("music"));
}

TEST_CASE("[Audio][AudioMixerServer] A fade-out during ducking ends at silence") {
	DuckingReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	MessageQueue::get_singleton()->flush();

	Ref<AudioMixerChannel> music = make_channel("music", true);
	music->set_duck_fade_time(0.2f);
	mixer->add_channel(music);
	mixer->play("music", make_wav(10.0));

	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 1);
	mixer->process(0.1);
	mixer->fade_out("music", 1.0f);
	mixer->process(0.5);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(-12.0));
	CHECK(mixer->is_fading("music"));
	CHECK(mixer->get_voice_count("music") == 1);

	// Speech ends while the fade-out runs: the duck fades back up, the fade-out goes on.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 1);
	mixer->process(0.3);
	CHECK(mixer->get_duck_volume_db("music") == doctest::Approx(0.0));
	CHECK(mixer->is_fading("music")); // The fade-out is not reverted by the unduck.
	CHECK(mixer->get_voice_count("music") == 1);
	mixer->process(0.3);
	CHECK(mixer->get_voice_count("music") == 0);
	CHECK_FALSE(mixer->is_playing("music"));

	// Nothing jumps back up afterwards.
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_STARTED, 2);
	post_utterance(DisplayServerEnums::TTS_UTTERANCE_ENDED, 2);
	mixer->process(0.5);
	CHECK(mixer->get_voice_count("music") == 0);
	CHECK_FALSE(mixer->is_playing("music"));
}

} // namespace TestAudioMixerDucking
