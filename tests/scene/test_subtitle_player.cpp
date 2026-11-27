/**************************************************************************/
/*  test_subtitle_player.cpp                                              */
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

TEST_FORCE_LINK(test_subtitle_player)

#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "scene/animation/animation_player.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/main/scene_tree.h"
#include "scene/main/subtitle_player.h"
#include "scene/main/window.h"
#include "scene/resources/animation_library.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestSubtitlePlayer {

// Collects the text of every segment_changed signal ("" for none).
class ChangeLog : public Object {
public:
	Vector<String> texts;

	void on_changed(const Ref<SubtitleSegment> &p_segment) { texts.push_back(p_segment.is_valid() ? p_segment->get_text() : String()); }

	String take() {
		const String result = String("|").join(texts);
		texts.clear();
		return result;
	}
};

static Ref<SubtitleSegment> make_segment(double p_start, double p_end, const String &p_text) {
	Ref<SubtitleSegment> segment;
	segment.instantiate();
	segment->set_start_time(p_start);
	segment->set_end_time(p_end);
	segment->set_text(p_text);
	return segment;
}

// 0-1 "A", 2-4 "B".
static Ref<Subtitle> make_subtitle() {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	subtitle->add_segment(make_segment(0.0, 1.0, "A"));
	subtitle->add_segment(make_segment(2.0, 4.0, "B"));
	return subtitle;
}

TEST_CASE("[SceneTree][SubtitlePlayer] Own clock: play, process, seek back, stop") {
	SubtitlePlayer *player = memnew(SubtitlePlayer);
	ChangeLog *log = memnew(ChangeLog);
	player->connect("segment_changed", callable_mp(log, &ChangeLog::on_changed));
	player->set_subtitle(make_subtitle());
	SceneTree::get_singleton()->get_root()->add_child(player);

	SceneTree::get_singleton()->process(1.0);
	CHECK_FALSE(player->is_playing()); // No autoplay.
	CHECK(log->take().is_empty());

	player->play(0.5);
	CHECK(player->is_playing());
	CHECK(log->take() == "A");

	SceneTree::get_singleton()->process(2.0); // 2.5
	CHECK(log->take() == "B");
	CHECK(player->get_playback_position() == doctest::Approx(2.5));

	player->seek(0.25);
	CHECK(log->take() == "A");

	player->set_time_offset(2.0); // Segments appear 2 s earlier: 0.25 + 1.0 + 2.0 = 3.25 is in B.
	SceneTree::get_singleton()->process(1.0);
	CHECK(log->take() == "B");

	player->stop();
	CHECK_FALSE(player->is_playing());
	CHECK(log->texts.size() == 1);
	CHECK(log->take().is_empty());

	memdelete(player);
	memdelete(log);
}

TEST_CASE("[SceneTree][SubtitlePlayer] Sync with an AnimationPlayer") {
	Ref<Animation> animation;
	animation.instantiate();
	animation->set_length(10.0);
	Ref<AnimationLibrary> library;
	library.instantiate();
	library->add_animation("talk", animation);

	Node *root = SceneTree::get_singleton()->get_root();
	AnimationPlayer *animation_player = memnew(AnimationPlayer);
	animation_player->set_name("AnimationPlayer");
	animation_player->add_animation_library("", library);
	root->add_child(animation_player);

	SubtitlePlayer *player = memnew(SubtitlePlayer);
	ChangeLog *log = memnew(ChangeLog);
	player->connect("segment_changed", callable_mp(log, &ChangeLog::on_changed));
	player->set_subtitle(make_subtitle());
	root->add_child(player);
	player->set_sync_node(player->get_path_to(animation_player));

	ERR_PRINT_OFF;
	player->play(); // The sync node decides.
	ERR_PRINT_ON;
	CHECK_FALSE(player->is_playing());

	animation_player->play("talk");
	animation_player->seek(2.5, true);
	SceneTree::get_singleton()->process(0.0);
	CHECK(player->is_playing());
	CHECK(player->get_playback_position() == doctest::Approx(2.5));
	CHECK(log->take() == "B");

	animation_player->seek(0.5, true); // Backwards.
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "A");

	animation_player->stop();
	SceneTree::get_singleton()->process(0.0);
	CHECK_FALSE(player->is_playing());
	CHECK(log->texts.size() == 1);
	CHECK(log->take().is_empty());

	memdelete(player);
	memdelete(animation_player);
	memdelete(log);
}

TEST_CASE("[SceneTree][SubtitlePlayer] Unsupported sync node") {
	Node *root = SceneTree::get_singleton()->get_root();
	Node *other = memnew(Node);
	root->add_child(other);
	SubtitlePlayer *player = memnew(SubtitlePlayer);
	player->set_subtitle(make_subtitle());
	root->add_child(player);
	player->set_sync_node(player->get_path_to(other));

	ERR_PRINT_OFF;
	SceneTree::get_singleton()->process(1.0);
	ERR_PRINT_ON;
	CHECK_FALSE(player->is_playing());
	CHECK(player->get_playback()->get_active_segments().is_empty());

	memdelete(player);
	memdelete(other);
}

// Saves a subtitle with 0-1 "<prefix>1" and 2-60 "<prefix>2" and returns its path.
static String save_subtitle(const String &p_file, const String &p_prefix) {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	subtitle->add_segment(make_segment(0.0, 1.0, p_prefix + "1"));
	subtitle->add_segment(make_segment(2.0, 60.0, p_prefix + "2"));
	const String path = TestUtils::get_temp_path(p_file);
	CHECK(ResourceSaver::save(subtitle, path) == OK);
	return path;
}

// A silent 10 s stream with a subtitle path.
static Ref<AudioStreamWAV> make_stream(const String &p_subtitle_path) {
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(44100);
	Vector<uint8_t> data;
	data.resize(10 * 44100 * 2);
	memset(data.ptrw(), 0, data.size());
	wav->set_data(data);
	wav->set_subtitle_path(p_subtitle_path);
	return wav;
}

// The first segment of the subtitle the player plays, "" without one.
static String get_source_text(SubtitlePlayer *p_player) {
	const Ref<Subtitle> subtitle = p_player->get_playback()->get_subtitle();
	return subtitle.is_valid() && subtitle->get_segment_count() > 0 ? subtitle->get_segment(0)->get_text() : String();
}

TEST_CASE("[SceneTree][Audio][SubtitlePlayer] Subtitle of the synced AudioStreamPlayer: play, pause, seek, stop") {
	AudioStreamPlayer *audio = memnew(AudioStreamPlayer);
	audio->set_stream(make_stream(save_subtitle("subtitle_player_sync_a.srt", "A")));
	SceneTree::get_singleton()->get_root()->add_child(audio);
	SubtitlePlayer *player = memnew(SubtitlePlayer);
	ChangeLog *log = memnew(ChangeLog);
	player->connect("segment_changed", callable_mp(log, &ChangeLog::on_changed));
	player->set_sync_node(NodePath(".."));
	audio->add_child(player);
	SIGNAL_WATCH(audio, "playing_changed");
	Array started;
	started.push_back(Array({ true }));
	Array stopped;
	stopped.push_back(Array({ false }));

	SceneTree::get_singleton()->process(0.0); // Resolves the sync node and loads its subtitle.
	CHECK(player->get_subtitle().is_null());
	CHECK(get_source_text(player) == "A1");
	CHECK_FALSE(player->is_playing());

	audio->play();
	SIGNAL_CHECK("playing_changed", started);
	SceneTree::get_singleton()->process(0.0);
	CHECK(player->is_playing());
	CHECK(log->take() == "A1");

	audio->seek(2.5); // Stops and starts internally, without playing_changed.
	SIGNAL_CHECK_FALSE("playing_changed");
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "A2");

	audio->set_stream_paused(true);
	SIGNAL_CHECK("playing_changed", stopped);
	CHECK_FALSE(player->is_playing());
	CHECK(log->texts.size() == 1);
	CHECK(log->take().is_empty());

	audio->set_stream_paused(false);
	SIGNAL_CHECK("playing_changed", started);
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "A2");

	audio->stop();
	SIGNAL_CHECK("playing_changed", stopped);
	CHECK_FALSE(player->is_playing());
	CHECK(log->texts.size() == 1);
	CHECK(log->take().is_empty());

	SIGNAL_UNWATCH(audio, "playing_changed");
	memdelete(player);
	memdelete(audio);
	memdelete(log);
}

TEST_CASE("[SceneTree][Audio][SubtitlePlayer] Track change, crossfade, sidecar, player override and explicit subtitle") {
	const Ref<AudioStreamWAV> stream_a = make_stream(save_subtitle("subtitle_player_track_a.srt", "A"));
	const Ref<AudioStreamWAV> stream_b = make_stream(save_subtitle("subtitle_player_track_b.srt", "B"));
	AudioStreamPlayer *audio = memnew(AudioStreamPlayer);
	audio->set_stream(stream_a);
	SceneTree::get_singleton()->get_root()->add_child(audio);
	SubtitlePlayer *player = memnew(SubtitlePlayer);
	ChangeLog *log = memnew(ChangeLog);
	player->connect("segment_changed", callable_mp(log, &ChangeLog::on_changed));
	player->set_sync_node(NodePath(".."));
	audio->add_child(player);
	audio->play();
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "A1");

	// A playlist reports its next track; its own subtitle replaces the shown one.
	audio->emit_signal(SNAME("track_changed"), 1, stream_b);
	CHECK(get_source_text(player) == "B1");
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "|B1"); // The old text ends, the new track's text starts.

	// A crossfade switches with the new stream.
	audio->crossfade_to(stream_a, 0.5f);
	CHECK(get_source_text(player) == "A1");
	SceneTree::get_singleton()->process(0.0);
	CHECK(log->take() == "|A1");

	// Without subtitle_path: a sidecar file with the same base name next to the stream file.
	const Ref<AudioStreamWAV> voice = make_stream(String());
	save_subtitle("subtitle_player_voice.srt", "V");
	voice->set_path_cache(TestUtils::get_temp_path("subtitle_player_voice.wav"));
	audio->set_stream(voice);
	CHECK(get_source_text(player) == "V1");

	// The player's override wins over the stream; it is picked up when the player starts.
	audio->set_subtitle_path(save_subtitle("subtitle_player_override.srt", "O"));
	audio->play();
	CHECK(get_source_text(player) == "O1");

	// An explicit subtitle wins over everything and stays on track changes.
	const Ref<Subtitle> explicit_subtitle = make_subtitle();
	player->set_subtitle(explicit_subtitle);
	audio->emit_signal(SNAME("track_changed"), 1, stream_b);
	CHECK(player->get_subtitle() == explicit_subtitle);
	CHECK(player->get_playback()->get_subtitle() == explicit_subtitle);

	// Cleared again: back to the player's override.
	player->set_subtitle(Ref<Subtitle>());
	CHECK(get_source_text(player) == "O1");

	memdelete(player);
	memdelete(audio);
	memdelete(log);
}

} // namespace TestSubtitlePlayer
