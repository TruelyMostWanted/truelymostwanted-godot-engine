/**************************************************************************/
/*  test_audio_stream_playlist.h                                          */
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

#include "../audio_stream_playlist.h"

#include "scene/resources/audio/audio_stream_wav.h"
#include "servers/audio/audio_server.h"
#include "tests/test_macros.h"

namespace TestAudioStreamPlaylist {

static Ref<AudioStreamWAV> make_track(double p_seconds, const String &p_name = String()) {
	// Silent 16-bit mono WAV without loop; the tests check indices, not sound.
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(44100);
	wav->set_stereo(false);
	Vector<uint8_t> data;
	data.resize(MAX(2, int(p_seconds * 44100.0) * 2));
	memset(data.ptrw(), 0, data.size());
	wav->set_data(data);
	wav->set_name(p_name);
	return wav;
}

TEST_CASE("[Audio][AudioStreamPlaylist] Typed list API") {
	Ref<AudioStreamPlaylist> playlist;
	playlist.instantiate();
	Ref<AudioStreamWAV> a = make_track(1.0, "A");
	Ref<AudioStreamWAV> b = make_track(1.0, "B");
	Ref<AudioStreamWAV> c = make_track(1.0);

	playlist->add_stream(a);
	playlist->add_stream(c);
	playlist->insert_stream(1, b);
	CHECK(playlist->get_stream_count() == 3);
	CHECK(playlist->get_streams().size() == 3);
	CHECK(playlist->find_stream(b) == 1);
	CHECK(playlist->find_stream(make_track(1.0)) == -1);
	CHECK(playlist->get_stream_name(0) == "A");
	CHECK(playlist->get_stream_name(2) == ""); // No name and no file.

	playlist->move_stream(0, 2);
	CHECK(playlist->get_list_stream(0) == b);
	CHECK(playlist->get_list_stream(1) == c);
	CHECK(playlist->get_list_stream(2) == a);

	playlist->remove_stream(1);
	CHECK(playlist->get_stream_count() == 2);
	CHECK(playlist->find_stream(c) == -1);

	ERR_PRINT_OFF;
	playlist->remove_stream(5);
	playlist->move_stream(0, 9);
	playlist->add_stream(playlist); // A playlist cannot contain itself.
	ERR_PRINT_ON;
	CHECK(playlist->get_stream_count() == 2);

	TypedArray<AudioStream> streams;
	streams.push_back(c);
	playlist->set_streams(streams);
	CHECK(playlist->get_stream_count() == 1);
	CHECK(playlist->get_list_stream(0) == c);
	CHECK(playlist->get_list_stream(1).is_null());
}

TEST_CASE("[Audio][AudioStreamPlaylist] Old stream_count and stream_N properties still load") {
	Ref<AudioStreamPlaylist> playlist;
	playlist.instantiate();
	Ref<AudioStreamWAV> a = make_track(1.0);
	Ref<AudioStreamWAV> b = make_track(1.0);

	// What the text loader does for a file saved before `streams` existed.
	bool valid = false;
	playlist->set("stream_count", 2, &valid);
	CHECK(valid);
	playlist->set("stream_0", a, &valid);
	playlist->set("stream_1", b, &valid);
	CHECK(valid);
	const TypedArray<AudioStream> streams = playlist->get_streams();
	REQUIRE(streams.size() == 2);
	CHECK(Ref<AudioStream>(streams[0]) == a);
	CHECK(Ref<AudioStream>(streams[1]) == b);

	// Only `streams` is stored now.
	List<PropertyInfo> properties;
	playlist->get_property_list(&properties);
	bool streams_stored = false;
	for (const PropertyInfo &property : properties) {
		if (property.name == "streams") {
			streams_stored = (property.usage & PROPERTY_USAGE_STORAGE) != 0;
		} else if (property.name.begins_with("stream_")) {
			CHECK((property.usage & PROPERTY_USAGE_STORAGE) == 0);
		}
	}
	CHECK(streams_stored);
}

TEST_CASE("[Audio][AudioStreamPlaylist] Playback publishes the current track") {
	Ref<AudioStreamPlaylist> playlist;
	playlist.instantiate();
	playlist->set_loop(true);
	Ref<AudioStreamWAV> a = make_track(0.05);
	Ref<AudioStreamWAV> b = make_track(0.05);
	playlist->add_stream(a);
	playlist->add_stream(b);

	Ref<AudioStreamPlaybackPlaylist> playback = playlist->instantiate_playback();
	REQUIRE(playback.is_valid());
	CHECK(playback->get_current_index() == -1);
	CHECK(playback->get_current_stream().is_null());

	playback->start(0.0);
	CHECK(playback->get_current_index() == 0);
	CHECK(playback->get_current_stream() == a);
	AudioStreamPlayback::TrackInfo info;
	REQUIRE(playback->get_track_info(info));
	const uint32_t first_serial = info.serial;

	// Mix 0.075 s at the server rate: the second track has started 0.025 s ago.
	const int rate = int(AudioServer::get_singleton()->get_mix_rate());
	Vector<AudioFrame> buffer;
	buffer.resize(rate);
	playback->mix(buffer.ptrw(), 1.0f, int(rate * 0.075));
	CHECK(playback->get_current_index() == 1);
	CHECK(playback->get_current_stream() == b);
	CHECK(playback->get_position_in_stream() == doctest::Approx(0.025).epsilon(0.05));
	REQUIRE(playback->get_track_info(info));
	CHECK(info.serial == first_serial + 1);

	// Looping back to the first track counts as a new track.
	playback->mix(buffer.ptrw(), 1.0f, int(rate * 0.05));
	CHECK(playback->get_current_index() == 0);
	REQUIRE(playback->get_track_info(info));
	CHECK(info.serial == first_serial + 2);

	playback->stop();
	CHECK(playback->get_current_index() == -1);
}

TEST_CASE("[Audio][AudioStreamPlaylist] Start times and indices round trip, with loop and fade time") {
	Ref<AudioStreamPlaylist> playlist;
	playlist.instantiate();
	playlist->add_stream(make_track(1.0));
	playlist->add_stream(Ref<AudioStream>()); // Null entries take no time.
	playlist->add_stream(make_track(2.0));
	playlist->add_stream(make_track(0.5));

	CHECK(playlist->get_length() == doctest::Approx(3.5));
	CHECK(playlist->get_stream_length(1) == doctest::Approx(0.0));
	CHECK(playlist->get_stream_start_time(0) == doctest::Approx(0.0));
	CHECK(playlist->get_stream_start_time(2) == doctest::Approx(1.0));
	CHECK(playlist->get_stream_start_time(3) == doctest::Approx(3.0));
	for (int index : { 0, 2, 3 }) {
		const double start = playlist->get_stream_start_time(index);
		CHECK(playlist->get_stream_index_at(start) == index);
		CHECK(playlist->get_stream_index_at(start + playlist->get_stream_length(index) * 0.5) == index);
	}

	// The fade time does not change the timeline.
	playlist->set_fade_time(0.5f);
	CHECK(playlist->get_stream_start_time(3) == doctest::Approx(3.0));
	CHECK(playlist->get_stream_index_at(2.9) == 2);

	// Past the end: wraps with loop, -1 without.
	playlist->set_loop(true);
	CHECK(playlist->get_stream_index_at(3.5 + 1.5) == 2);
	playlist->set_loop(false);
	CHECK(playlist->get_stream_index_at(3.6) == -1);
	CHECK(playlist->get_stream_index_at(-0.1) == -1);
}

TEST_CASE("[Audio][AudioStreamPlaylist] Position in the playlist follows the play order") {
	Ref<AudioStreamPlaylist> playlist;
	playlist.instantiate();
	playlist->add_stream(make_track(0.05));
	playlist->add_stream(make_track(0.05));
	Ref<AudioStreamPlaybackPlaylist> playback = playlist->instantiate_playback();
	REQUIRE(playback.is_valid());

	// A start in the middle of the second track (what seek() does).
	playback->start(0.07);
	CHECK(playback->get_current_index() == 1);
	CHECK(playback->get_position_in_stream() == doctest::Approx(0.02).epsilon(0.01));
	CHECK(playback->get_position_in_playlist() == doctest::Approx(0.07).epsilon(0.01));

	const int rate = int(AudioServer::get_singleton()->get_mix_rate());
	Vector<AudioFrame> buffer;
	buffer.resize(rate);
	playback->mix(buffer.ptrw(), 1.0f, int(rate * 0.04)); // Loops back into the first track at 0.01.
	CHECK(playback->get_current_index() == 0);
	CHECK(playback->get_position_in_playlist() == doctest::Approx(0.01).epsilon(0.05));
	playback->stop();
}

} // namespace TestAudioStreamPlaylist
