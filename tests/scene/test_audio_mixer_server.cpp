/**************************************************************************/
/*  test_audio_mixer_server.cpp                                           */
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

TEST_FORCE_LINK(test_audio_mixer_server)

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "scene/audio/audio_mixer_server.h"
#include "scene/resources/audio/audio_mixer_channel.h"
#include "scene/resources/audio/audio_mixer_layout.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestAudioMixerServer {

// Removes all channels at the end of a test case, also when a REQUIRE fails.
struct MixerReset {
	~MixerReset() {
		if (AudioMixerServer::get_singleton()) {
			AudioMixerServer::get_singleton()->reset();
		}
		// Stopped playbacks are removed by the audio thread on its next mix step; give it time,
		// so the streams are not reported as leaked at exit.
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

static Ref<AudioMixerChannel> make_channel(const StringName &p_name) {
	Ref<AudioMixerChannel> channel;
	channel.instantiate();
	channel->set_channel_name(p_name);
	return channel;
}

static Array args1(const Variant &p_a) {
	Array args;
	args.push_back(p_a);
	return args;
}

static Array args2(const Variant &p_a, const Variant &p_b) {
	Array args = args1(p_a);
	args.push_back(p_b);
	return args;
}

static Array args3(const Variant &p_a, const Variant &p_b, const Variant &p_c) {
	Array args = args2(p_a, p_b);
	args.push_back(p_c);
	return args;
}

TEST_CASE("[AudioMixerServer] Singleton is registered") {
	REQUIRE(AudioMixerServer::get_singleton() != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("AudioMixerServer"));
}

TEST_CASE("[AudioMixerServer] AudioMixerChannel defaults and value ranges") {
	Ref<AudioMixerChannel> channel;
	channel.instantiate();
	CHECK(channel->get_channel_name() == StringName());
	CHECK(channel->get_bus() == StringName("Master"));
	CHECK(channel->get_volume_db() == doctest::Approx(0.0));
	CHECK(channel->get_pitch_scale() == doctest::Approx(1.0));
	CHECK(channel->get_polyphony() == 1);
	CHECK(channel->get_queue_mode() == AudioMixerChannel::QUEUE_MODE_STOP);
	CHECK_FALSE(channel->is_loop_entry());
	CHECK(channel->get_default_fade_time() == doctest::Approx(0.0));
	CHECK_FALSE(channel->is_pause_with_tree());
	CHECK_FALSE(channel->is_stop_on_scene_change());
	CHECK_FALSE(channel->is_preload_next());

	channel->set_polyphony(0);
	CHECK(channel->get_polyphony() == 1);
	channel->set_polyphony(1000);
	CHECK(channel->get_polyphony() == 128);
	channel->set_default_fade_time(-2.0f);
	CHECK(channel->get_default_fade_time() == doctest::Approx(0.0));
	ERR_PRINT_OFF;
	channel->set_pitch_scale(0.0f);
	ERR_PRINT_ON;
	CHECK(channel->get_pitch_scale() == doctest::Approx(1.0));
}

TEST_CASE("[AudioMixerServer] AudioMixerLayout saves and loads as .tres") {
	Ref<AudioMixerLayout> layout;
	layout.instantiate();
	Ref<AudioMixerChannel> music = make_channel("music");
	music->set_bus("Music");
	music->set_volume_db(-6.0f);
	music->set_queue_mode(AudioMixerChannel::QUEUE_MODE_LOOP_QUEUE);
	music->set_default_fade_time(2.0f);
	music->set_pause_with_tree(true);
	layout->add_channel(music);
	layout->add_channel(make_channel("sfx"));
	CHECK(layout->find_channel("music") == music);
	CHECK(layout->find_channel("missing").is_null());

	const String path = TestUtils::get_temp_path("audio_mixer_layout.tres");
	REQUIRE(ResourceSaver::save(layout, path) == OK);
	Ref<AudioMixerLayout> loaded = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	REQUIRE(loaded->get_channels().size() == 2);
	Ref<AudioMixerChannel> loaded_music = loaded->find_channel("music");
	REQUIRE(loaded_music.is_valid());
	CHECK(loaded_music->get_bus() == StringName("Music"));
	CHECK(loaded_music->get_volume_db() == doctest::Approx(-6.0));
	CHECK(loaded_music->get_queue_mode() == AudioMixerChannel::QUEUE_MODE_LOOP_QUEUE);
	CHECK(loaded_music->get_default_fade_time() == doctest::Approx(2.0));
	CHECK(loaded_music->is_pause_with_tree());
}

TEST_CASE("[AudioMixerServer] Channels from a layout") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);

	Ref<AudioMixerLayout> layout;
	layout.instantiate();
	layout->add_channel(make_channel("music"));
	layout->add_channel(make_channel("ambience"));
	mixer->load_layout(layout);

	CHECK(mixer->has_channel("music"));
	CHECK(mixer->has_channel("ambience"));
	CHECK_FALSE(mixer->has_channel("voice"));
	PackedStringArray names = mixer->get_channel_names();
	REQUIRE(names.size() == 2);
	CHECK(names[0] == "music");
	CHECK(names[1] == "ambience");

	// A new definition with the same name replaces the old one.
	Ref<AudioMixerChannel> music = make_channel("music");
	music->set_volume_db(-3.0f);
	mixer->add_channel(music);
	CHECK(mixer->get_channel("music") == music);
	CHECK(mixer->get_channel_volume_db("music") == doctest::Approx(-3.0));
	CHECK(mixer->get_channel_names().size() == 2);

	mixer->remove_channel("ambience");
	CHECK_FALSE(mixer->has_channel("ambience"));

	// Loading a layout replaces all channels.
	Ref<AudioMixerLayout> other;
	other.instantiate();
	other->add_channel(make_channel("voice"));
	mixer->load_layout(other);
	CHECK(mixer->get_channel_names().size() == 1);
	CHECK(mixer->has_channel("voice"));

	ERR_PRINT_OFF;
	CHECK_FALSE(mixer->is_playing("missing"));
	mixer->add_channel(make_channel(StringName()));
	ERR_PRINT_ON;
	CHECK(mixer->get_channel_names().size() == 1);
}

TEST_CASE("[AudioMixerServer] Queue editing and cursor without playback") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));

	mixer->enqueue("music", "res://a.ogg");
	mixer->enqueue("music", "res://c.ogg");
	mixer->enqueue("music", "res://b.ogg", 1);
	PackedStringArray queue = mixer->get_queue("music");
	REQUIRE(queue.size() == 3);
	CHECK(queue[0] == "res://a.ogg");
	CHECK(queue[1] == "res://b.ogg");
	CHECK(queue[2] == "res://c.ogg");
	CHECK(mixer->get_index("music") == -1);

	// Not playing: next(), previous() and change_index() only move the cursor.
	mixer->next("music");
	CHECK(mixer->get_index("music") == 0);
	mixer->next("music");
	CHECK(mixer->get_index("music") == 1);
	mixer->change_index("music", 2);
	CHECK(mixer->get_index("music") == 2);
	mixer->next("music"); // QUEUE_MODE_STOP: no entry after the last one.
	CHECK(mixer->get_index("music") == 2);
	mixer->previous("music");
	CHECK(mixer->get_index("music") == 1);
	CHECK_FALSE(mixer->is_playing("music"));

	// Removing the current entry moves the cursor to the entry before it.
	mixer->dequeue("music", 1);
	CHECK(mixer->get_index("music") == 0);
	CHECK(mixer->get_queue("music").size() == 2);
	mixer->dequeue("music"); // Last entry.
	CHECK(mixer->get_queue("music").size() == 1);
	CHECK(mixer->get_queue("music")[0] == "res://a.ogg");

	ERR_PRINT_OFF;
	mixer->change_index("music", 5);
	mixer->dequeue("music", 7);
	ERR_PRINT_ON;
	CHECK(mixer->get_index("music") == 0);

	mixer->clear_queue("music");
	CHECK(mixer->get_queue("music").is_empty());
	CHECK(mixer->get_index("music") == -1);

	// LOOP_QUEUE wraps around in both directions.
	Ref<AudioMixerChannel> looped = make_channel("loop");
	looped->set_queue_mode(AudioMixerChannel::QUEUE_MODE_LOOP_QUEUE);
	mixer->add_channel(looped);
	mixer->enqueue("loop", "res://1.ogg");
	mixer->enqueue("loop", "res://2.ogg");
	mixer->previous("loop");
	CHECK(mixer->get_index("loop") == 1);
	mixer->next("loop");
	CHECK(mixer->get_index("loop") == 0);
}

TEST_CASE("[Audio][AudioMixerServer] Play, pause, resume and stop") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	Ref<AudioStreamWAV> wav = make_wav(10.0);

	SIGNAL_WATCH(mixer, "channel_started");
	mixer->play("music", wav);
	CHECK(mixer->is_playing("music"));
	CHECK(mixer->get_voice_count("music") == 1);
	CHECK(mixer->get_channel_playback("music").is_valid());
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(1.0));
	Array started;
	started.push_back(args3(StringName("music"), -1, String()));
	SIGNAL_CHECK("channel_started", started);
	SIGNAL_UNWATCH(mixer, "channel_started");

	mixer->pause("music");
	CHECK(mixer->is_paused("music"));
	CHECK(mixer->is_playing("music"));
	mixer->process(1.0);
	CHECK(mixer->get_voice_count("music") == 1);
	mixer->resume("music");
	CHECK_FALSE(mixer->is_paused("music"));

	mixer->seek("music", 2.0f);
	CHECK(mixer->is_playing("music"));
	CHECK(mixer->get_voice_count("music") == 1);

	mixer->stop("music");
	CHECK_FALSE(mixer->is_playing("music"));
	CHECK(mixer->get_voice_count("music") == 0);

	// A missing bus falls back to Master.
	Ref<AudioMixerChannel> lost = make_channel("lost");
	lost->set_bus("DoesNotExist");
	mixer->add_channel(lost);
	ERR_PRINT_OFF;
	mixer->play("lost", wav);
	ERR_PRINT_ON;
	CHECK(mixer->is_playing("lost"));
}

TEST_CASE("[Audio][AudioMixerServer] Fade out stops the voice and emits fade_finished") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	mixer->play("music", make_wav(10.0));

	SIGNAL_WATCH(mixer, "fade_finished");
	mixer->fade_out("music", 1.0f);
	CHECK_FALSE(mixer->is_playing("music"));
	CHECK(mixer->is_fading("music"));
	mixer->process(0.5);
	CHECK(mixer->get_voice_count("music") == 1);
	SIGNAL_CHECK_FALSE("fade_finished");
	mixer->process(0.6);
	CHECK(mixer->get_voice_count("music") == 0);
	CHECK_FALSE(mixer->is_fading("music"));
	Array finished;
	finished.push_back(args1(StringName("music")));
	SIGNAL_CHECK("fade_finished", finished);

	// fade_in() starts the queue at volume 0 and rises to 1.
	mixer->enqueue("music", make_wav(10.0));
	mixer->fade_in("music", 1.0f, AudioMixerServer::FADE_CURVE_LINEAR);
	CHECK(mixer->is_playing("music"));
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(0.0));
	mixer->process(0.25);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(0.25));
	mixer->process(1.0);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(1.0));
	SIGNAL_CHECK("fade_finished", finished);
	SIGNAL_UNWATCH(mixer, "fade_finished");
}

TEST_CASE("[Audio][AudioMixerServer] Fade curves and equal power cross-fade") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	mixer->play("music", make_wav(10.0));

	mixer->cross_fade("music", make_wav(10.0), 1.0f);
	CHECK(mixer->get_voice_count("music") == 2);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(0.0));
	mixer->process(0.25);
	// The incoming voice follows sin, the outgoing one cos: their power sums to 1.
	const double in_gain = Math::sin(0.25 * Math::PI * 0.5);
	const double out_gain = Math::cos(0.25 * Math::PI * 0.5);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(in_gain).epsilon(0.001));
	CHECK(in_gain * in_gain + out_gain * out_gain == doctest::Approx(1.0));
	mixer->process(0.8);
	CHECK(mixer->get_voice_count("music") == 1);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(1.0));

	// Ease curves.
	mixer->fade_out("music", 1.0f, AudioMixerServer::FADE_CURVE_EASE_IN);
	mixer->play("music", make_wav(10.0));
	mixer->fade_in("music", 0.0f);
	mixer->cross_fade("music", make_wav(10.0), 1.0f, AudioMixerServer::FADE_CURVE_EASE_IN);
	mixer->process(0.5);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(0.25).epsilon(0.001));
	mixer->cross_fade("music", make_wav(10.0), 1.0f, AudioMixerServer::FADE_CURVE_EASE_OUT);
	mixer->process(0.5);
	CHECK(mixer->get_fade_gain("music") == doctest::Approx(0.75).epsilon(0.001));
	// Never more than one voice fading out next to the current one.
	CHECK(mixer->get_voice_count("music") == 2);
}

TEST_CASE("[Audio][AudioMixerServer] Queue jumps with a cross-fade") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	Ref<AudioMixerChannel> music = make_channel("music");
	music->set_default_fade_time(0.5f);
	mixer->add_channel(music);
	mixer->enqueue("music", make_wav(10.0));
	mixer->enqueue("music", make_wav(10.0));
	mixer->enqueue("music", make_wav(10.0));

	SIGNAL_WATCH(mixer, "channel_started");
	mixer->play("music");
	CHECK(mixer->get_index("music") == 0);
	Array started;
	started.push_back(args3(StringName("music"), 0, String()));
	SIGNAL_CHECK("channel_started", started);

	mixer->next("music");
	CHECK(mixer->get_index("music") == 1);
	CHECK(mixer->get_voice_count("music") == 2);
	CHECK(mixer->is_fading("music"));
	mixer->process(0.6);
	CHECK(mixer->get_voice_count("music") == 1);
	CHECK_FALSE(mixer->is_fading("music"));

	mixer->previous("music", 0.2f);
	CHECK(mixer->get_index("music") == 0);
	mixer->change_index("music", 2, 0.0f);
	CHECK(mixer->get_index("music") == 2);
	CHECK(mixer->get_voice_count("music") == 1);
	SIGNAL_UNWATCH(mixer, "channel_started");
}

TEST_CASE("[Audio][AudioMixerServer] Channel volume fade and one-shots") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	Ref<AudioMixerChannel> sfx = make_channel("sfx");
	sfx->set_polyphony(2);
	mixer->add_channel(sfx);
	Ref<AudioStreamWAV> wav = make_wav(10.0);

	mixer->play_one_shot("sfx", wav);
	mixer->play_one_shot("sfx", wav, -6.0f, 1.5f);
	mixer->play_one_shot("sfx", wav);
	CHECK(mixer->get_voice_count("sfx") == 2); // The oldest one-shot was stolen.
	CHECK_FALSE(mixer->is_playing("sfx")); // One-shots are not the current voice.

	mixer->set_channel_volume_db("sfx", -12.0f, 1.0f);
	CHECK(mixer->is_fading("sfx"));
	mixer->process(0.5);
	CHECK(mixer->get_channel_volume_db("sfx") == doctest::Approx(-6.0));
	mixer->process(0.6);
	CHECK(mixer->get_channel_volume_db("sfx") == doctest::Approx(-12.0));
	CHECK_FALSE(mixer->is_fading("sfx"));

	mixer->set_channel_pitch_scale("sfx", 0.5f);
	CHECK(mixer->get_channel_pitch_scale("sfx") == doctest::Approx(0.5));

	mixer->stop_all();
	CHECK(mixer->get_voice_count("sfx") == 0);

	ERR_PRINT_OFF;
	mixer->switch_clip("sfx", "intro"); // Not playing: error only.
	ERR_PRINT_ON;
}

TEST_CASE("[Audio][AudioMixerServer] The queue advances at the end of a stream") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	mixer->enqueue("music", make_wav(0.02));
	mixer->enqueue("music", make_wav(0.02));

	SIGNAL_WATCH(mixer, "queue_finished");
	mixer->play("music");
	CHECK(mixer->get_index("music") == 0);

	// The dummy driver mixes in its own thread; wait until both entries have ended.
	bool reached_second = false;
	for (int i = 0; i < 300 && mixer->is_playing("music"); i++) {
		OS::get_singleton()->delay_usec(10000);
		mixer->process(0.01);
		if (mixer->get_index("music") == 1) {
			reached_second = true;
		}
	}
	CHECK(reached_second);
	CHECK(mixer->get_index("music") == 1);
	CHECK_FALSE(mixer->is_playing("music"));
	Array finished;
	finished.push_back(args1(StringName("music")));
	SIGNAL_CHECK("queue_finished", finished);
	SIGNAL_UNWATCH(mixer, "queue_finished");
}

} // namespace TestAudioMixerServer
