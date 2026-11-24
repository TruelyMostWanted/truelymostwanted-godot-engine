/**************************************************************************/
/*  test_audio_command.cpp                                                */
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

TEST_FORCE_LINK(test_audio_command)

#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "core/os/os.h"
#include "scene/audio/audio_command.h"
#include "scene/audio/audio_mixer_server.h"
#include "scene/resources/audio/audio_mixer_channel.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestAudioCommand {

// Removes all channels and registered commands at the end of a test case, also when a REQUIRE fails.
struct MixerReset {
	~MixerReset() {
		if (AudioMixerServer::get_singleton()) {
			AudioMixerServer::get_singleton()->reset();
		}
		// Stopped playbacks are removed by the audio thread on its next mix step.
		OS::get_singleton()->delay_usec(100000);
	}
};

// Saves a silent 16-bit mono WAV of p_seconds as a .tres file and returns its path.
static String save_wav(const String &p_file, double p_seconds) {
	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(44100);
	wav->set_stereo(false);
	Vector<uint8_t> data;
	data.resize(MAX(2, int(p_seconds * 44100.0) * 2));
	memset(data.ptrw(), 0, data.size());
	wav->set_data(data);
	const String path = TestUtils::get_temp_path(p_file);
	CHECK(ResourceSaver::save(wav, path) == OK);
	return path;
}

static Ref<AudioMixerChannel> make_channel(const StringName &p_name) {
	Ref<AudioMixerChannel> channel;
	channel.instantiate();
	channel->set_channel_name(p_name);
	return channel;
}

static Ref<AudioCommand> make_command(const StringName &p_name, AudioCommand::Command p_type, const StringName &p_channel, const String &p_stream_path = String()) {
	Ref<AudioCommand> command;
	command.instantiate();
	command->set_command_name(p_name);
	command->set_command(p_type);
	command->set_channel(p_channel);
	command->set_stream_path(p_stream_path);
	return command;
}

static void check_same_command(const Ref<AudioCommand> &p_a, const Ref<AudioCommand> &p_b) {
	REQUIRE(p_a.is_valid());
	REQUIRE(p_b.is_valid());
	CHECK(p_a->get_command_name() == p_b->get_command_name());
	CHECK(p_a->get_group() == p_b->get_group());
	CHECK(p_a->get_channel() == p_b->get_channel());
	CHECK(p_a->get_command() == p_b->get_command());
	CHECK(p_a->get_stream_path() == p_b->get_stream_path());
	CHECK(p_a->get_volume_db() == doctest::Approx(p_b->get_volume_db()));
	CHECK(p_a->get_pitch_scale() == doctest::Approx(p_b->get_pitch_scale()));
	CHECK(p_a->get_fade_time() == doctest::Approx(p_b->get_fade_time()));
	CHECK(p_a->get_fade_curve() == p_b->get_fade_curve());
	CHECK(p_a->get_queue_index() == p_b->get_queue_index());
	CHECK(p_a->get_position() == doctest::Approx(p_b->get_position()));
	CHECK(p_a->get_loop_mode() == p_b->get_loop_mode());
	CHECK(p_a->get_clip_name() == p_b->get_clip_name());
}

static Ref<AudioCommandList> make_mixed_list() {
	Ref<AudioCommandList> list;
	list.instantiate();
	Ref<AudioCommand> toggle = make_command("pad_1", AudioCommand::COMMAND_TOGGLE, "board", "res://sounds/horn.wav");
	toggle->set_group("board");
	list->add_command(toggle);
	Ref<AudioCommand> fade = make_command("forest_in", AudioCommand::COMMAND_FADE_IN, "ambience", "res://ambience/forest.ogg");
	fade->set_group("forest");
	fade->set_fade_time(2.5f);
	fade->set_fade_curve(AudioMixerServer::FADE_CURVE_EQUAL_POWER);
	fade->set_loop_mode(AudioCommand::LOOP_MODE_ON);
	list->add_command(fade);
	Ref<AudioCommand> volume = make_command("quiet", AudioCommand::COMMAND_SET_VOLUME, "music");
	volume->set_volume_db(-12.5f);
	volume->set_fade_time(0.0f);
	list->add_command(volume);
	Ref<AudioCommand> jump = make_command("track_3", AudioCommand::COMMAND_CHANGE_INDEX, "music");
	jump->set_queue_index(3);
	jump->set_loop_mode(AudioCommand::LOOP_MODE_OFF);
	list->add_command(jump);
	Ref<AudioCommand> clip = make_command("battle", AudioCommand::COMMAND_SWITCH_CLIP, "music");
	clip->set_clip_name("Battle");
	clip->set_position(1.25f);
	clip->set_pitch_scale(1.5f);
	list->add_command(clip);
	list->add_command(make_command(StringName(), AudioCommand::COMMAND_STOP_ALL, StringName()));
	return list;
}

TEST_CASE("[AudioCommand] Defaults and labels") {
	Ref<AudioCommand> command;
	command.instantiate();
	CHECK(command->get_command_name() == StringName());
	CHECK(command->get_group() == StringName());
	CHECK(command->get_channel() == StringName());
	CHECK(command->get_command() == AudioCommand::COMMAND_PLAY);
	CHECK(command->get_stream_path().is_empty());
	CHECK(command->get_volume_db() == doctest::Approx(0.0));
	CHECK(command->get_pitch_scale() == doctest::Approx(1.0));
	CHECK(command->get_fade_time() == doctest::Approx(-1.0));
	CHECK(command->get_fade_curve() == AudioMixerServer::FADE_CURVE_LINEAR);
	CHECK(command->get_queue_index() == -1);
	CHECK(command->get_position() == doctest::Approx(0.0));
	CHECK(command->get_loop_mode() == AudioCommand::LOOP_MODE_CHANNEL);
	CHECK(command->get_clip_name() == StringName());

	command->set_fade_time(-5.0f);
	CHECK(command->get_fade_time() == doctest::Approx(-1.0));
	command->set_queue_index(-7);
	CHECK(command->get_queue_index() == -1);
	command->set_position(-2.0f);
	CHECK(command->get_position() == doctest::Approx(0.0));

	CHECK(AudioCommand::get_command_label(AudioCommand::COMMAND_PLAY) == "Play");
	CHECK(AudioCommand::get_command_label(AudioCommand::COMMAND_FADE_IN) == "Fade In");
	CHECK(AudioCommand::get_command_label(AudioCommand::COMMAND_STOP_ALL) == "Stop All");

	Ref<AudioCommandList> list = make_mixed_list();
	CHECK(list->get_commands().size() == 6);
	CHECK(list->find_command("quiet").is_valid());
	CHECK(list->find_command("missing").is_null());
}

TEST_CASE("[AudioCommand] A command list round-trips through CSV, JSON, INI and XML") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	const Ref<AudioCommandList> list = make_mixed_list();
	CHECK(serializer->can_serialize(list, ResourceTextSerializer::FORMAT_CSV));

	const String csv = serializer->serialize(list, ResourceTextSerializer::FORMAT_CSV);
	// One header row and one row per command; enums appear as names.
	CHECK(csv.strip_edges().split("\n").size() == 7);
	CHECK(csv.contains("Toggle"));
	CHECK(csv.contains("Fade In"));
	CHECK(csv.contains("Equal Power"));
	CHECK(csv.contains("res://ambience/forest.ogg"));

	const ResourceTextSerializer::Format formats[] = { ResourceTextSerializer::FORMAT_CSV, ResourceTextSerializer::FORMAT_JSON, ResourceTextSerializer::FORMAT_INI, ResourceTextSerializer::FORMAT_XML };
	for (const ResourceTextSerializer::Format format : formats) {
		CAPTURE(format);
		const String text = serializer->serialize(list, format);
		REQUIRE_FALSE(text.is_empty());
		const Ref<AudioCommandList> loaded = serializer->deserialize(text, format, "AudioCommandList");
		REQUIRE(loaded.is_valid());
		const TypedArray<AudioCommand> original = list->get_commands();
		const TypedArray<AudioCommand> commands = loaded->get_commands();
		REQUIRE(commands.size() == original.size());
		for (int i = 0; i < original.size(); i++) {
			check_same_command(commands[i], original[i]);
		}
	}

	SUBCASE("Through ResourceSaver and ResourceLoader") {
		const String path = TestUtils::get_temp_path("audio_commands.csv");
		CHECK(ResourceSaver::save(Ref<Resource>(list), path) == OK);
		const Ref<AudioCommandList> from_file = ResourceLoader::load(path, "AudioCommandList", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(from_file.is_valid());
		CHECK(from_file->get_commands().size() == 6);
		check_same_command(from_file->find_command("forest_in"), list->find_command("forest_in"));
		DirAccess::remove_absolute(path);
	}

	SUBCASE("Numbers load as well as names") {
		const Ref<AudioCommandList> numeric = serializer->deserialize("command_name,command,channel,loop_mode\npad,4,board,-1\nbed,Fade In,ambience,1\n", ResourceTextSerializer::FORMAT_CSV, "AudioCommandList");
		REQUIRE(numeric.is_valid());
		REQUIRE(numeric->get_commands().size() == 2);
		const Ref<AudioCommand> pad = numeric->find_command("pad");
		REQUIRE(pad.is_valid());
		CHECK(pad->get_command() == AudioCommand::COMMAND_TOGGLE);
		CHECK(pad->get_channel() == StringName("board"));
		CHECK(pad->get_loop_mode() == AudioCommand::LOOP_MODE_CHANNEL);
		const Ref<AudioCommand> bed = numeric->find_command("bed");
		REQUIRE(bed.is_valid());
		CHECK(bed->get_command() == AudioCommand::COMMAND_FADE_IN);
		CHECK(bed->get_loop_mode() == AudioCommand::LOOP_MODE_ON);
	}
}

TEST_CASE("[AudioCommand] Registering, collisions and unregistering") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);

	Ref<AudioCommandList> base;
	base.instantiate();
	Ref<AudioCommand> base_stop = make_command("stop_music", AudioCommand::COMMAND_STOP, "music");
	base->add_command(base_stop);
	base->add_command(make_command("next_track", AudioCommand::COMMAND_NEXT, "music"));
	base->add_command(make_command(StringName(), AudioCommand::COMMAND_STOP, "ambience"));
	CHECK(mixer->register_commands(base) == OK);
	CHECK(mixer->has_command("stop_music"));
	CHECK(mixer->has_command("next_track"));
	PackedStringArray names = mixer->get_command_names();
	REQUIRE(names.size() == 2);
	CHECK(names[0] == "stop_music");
	CHECK(names[1] == "next_track");

	// Registering the same list again changes nothing.
	CHECK(mixer->register_commands(base) == OK);
	CHECK(mixer->get_command_names().size() == 2);

	// The last registered command with a name wins.
	Ref<AudioCommandList> mod;
	mod.instantiate();
	Ref<AudioCommand> mod_stop = make_command("stop_music", AudioCommand::COMMAND_FADE_OUT, "music");
	mod->add_command(mod_stop);
	ERR_PRINT_OFF;
	CHECK(mixer->register_commands(mod) == OK);
	ERR_PRINT_ON;
	CHECK(mixer->get_command_names().size() == 2);

	mixer->unregister_commands(mod);
	CHECK_FALSE(mixer->has_command("stop_music"));
	CHECK(mixer->has_command("next_track"));
	mixer->unregister_commands(base);
	CHECK(mixer->get_command_names().is_empty());

	ERR_PRINT_OFF;
	CHECK(mixer->execute_named("stop_music") == ERR_DOES_NOT_EXIST);
	CHECK(mixer->execute_group("nothing") == ERR_DOES_NOT_EXIST);
	CHECK(mixer->register_commands(Ref<AudioCommandList>()) == ERR_INVALID_PARAMETER);
	CHECK(mixer->execute(Ref<AudioCommand>()) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[Audio][AudioCommand] Queue commands without playback") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	const String track_a = save_wav("audio_command_track_a.tres", 1.0);
	const String track_b = save_wav("audio_command_track_b.tres", 1.0);

	Ref<AudioCommand> enqueue = make_command("add", AudioCommand::COMMAND_ENQUEUE, "music", track_a);
	CHECK(mixer->execute(enqueue) == OK);
	enqueue->set_stream_path(track_b);
	enqueue->set_queue_index(0);
	CHECK(mixer->execute(enqueue) == OK);
	PackedStringArray queue = mixer->get_queue("music");
	REQUIRE(queue.size() == 2);
	CHECK(queue[0] == track_b);
	CHECK(queue[1] == track_a);

	Ref<AudioCommand> dequeue = make_command("remove", AudioCommand::COMMAND_DEQUEUE, "music");
	dequeue->set_queue_index(5);
	ERR_PRINT_OFF;
	CHECK(mixer->execute(dequeue) == ERR_PARAMETER_RANGE_ERROR);
	enqueue->set_stream_path(TestUtils::get_temp_path("audio_command_missing.tres"));
	CHECK(mixer->execute(enqueue) == ERR_FILE_NOT_FOUND);
	enqueue->set_stream_path(String());
	CHECK(mixer->execute(enqueue) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK(mixer->get_queue("music").size() == 2);
	dequeue->set_queue_index(-1); // The last entry.
	CHECK(mixer->execute(dequeue) == OK);
	CHECK(mixer->get_queue("music").size() == 1);

	CHECK(mixer->execute(make_command("clear", AudioCommand::COMMAND_CLEAR, "music")) == OK);
	CHECK(mixer->get_queue("music").is_empty());
	ERR_PRINT_OFF;
	CHECK(mixer->execute(make_command("next", AudioCommand::COMMAND_NEXT, "music")) == ERR_DOES_NOT_EXIST);
	ERR_PRINT_ON;

	DirAccess::remove_absolute(track_a);
	DirAccess::remove_absolute(track_b);
}

TEST_CASE("[Audio][AudioCommand] A soundboard of five toggles") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);

	Ref<AudioCommandList> board;
	board.instantiate();
	LocalVector<String> paths;
	for (int i = 0; i < 5; i++) {
		const StringName pad = vformat("pad_%d", i + 1);
		mixer->add_channel(make_channel(pad));
		paths.push_back(save_wav(vformat("audio_command_pad_%d.tres", i + 1), 10.0));
		board->add_command(make_command(pad, AudioCommand::COMMAND_TOGGLE, pad, paths[i]));
	}
	CHECK(board->preload_streams() == OK);
	CHECK(board->get_preloaded_stream_count() == 5);
	CHECK(mixer->register_commands(board) == OK);
	CHECK(mixer->get_command_names().size() == 5);

	SIGNAL_WATCH(mixer, "command_executed");
	CHECK(mixer->execute_named("pad_2") == OK);
	CHECK(mixer->is_playing("pad_2"));
	CHECK_FALSE(mixer->is_playing("pad_1"));
	Array executed;
	Array args;
	args.push_back(StringName("pad_2"));
	args.push_back(board->find_command("pad_2"));
	executed.push_back(args);
	SIGNAL_CHECK("command_executed", executed);
	SIGNAL_UNWATCH(mixer, "command_executed");

	CHECK(mixer->execute_named("pad_4") == OK);
	CHECK(mixer->is_playing("pad_4"));
	CHECK(mixer->is_playing("pad_2"));

	// The second press stops the pad; fade_time -1 uses the channel default (0).
	CHECK(mixer->execute_named("pad_2") == OK);
	CHECK_FALSE(mixer->is_playing("pad_2"));
	CHECK(mixer->is_playing("pad_4"));
	CHECK(mixer->execute_named("pad_2") == OK);
	CHECK(mixer->is_playing("pad_2"));

	board->clear_preloaded_streams();
	CHECK(board->get_preloaded_stream_count() == 0);
	mixer->stop_all();
	for (const String &path : paths) {
		DirAccess::remove_absolute(path);
	}
}

TEST_CASE("[Audio][AudioCommand] A group starts all ambience commands") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	const String birds_path = save_wav("audio_command_birds.tres", 10.0);
	const String wind_path = save_wav("audio_command_wind.tres", 10.0);

	Ref<AudioCommandList> ambience;
	ambience.instantiate();
	const char *forest_channels[] = { "birds", "wind", "river" };
	for (const char *channel_name : forest_channels) {
		mixer->add_channel(make_channel(channel_name));
		Ref<AudioCommand> command = make_command(StringName(), AudioCommand::COMMAND_PLAY, channel_name, String(channel_name) == "birds" ? birds_path : wind_path);
		command->set_group("forest");
		command->set_loop_mode(AudioCommand::LOOP_MODE_ON);
		ambience->add_command(command);
	}
	mixer->add_channel(make_channel("traffic"));
	Ref<AudioCommand> city = make_command("city", AudioCommand::COMMAND_PLAY, "traffic", wind_path);
	city->set_group("city");
	ambience->add_command(city);
	CHECK(mixer->register_commands(ambience) == OK);

	CHECK(mixer->execute_group("forest") == OK);
	CHECK(mixer->is_playing("birds"));
	CHECK(mixer->is_playing("wind"));
	CHECK(mixer->is_playing("river"));
	CHECK_FALSE(mixer->is_playing("traffic"));
	CHECK(mixer->get_channel("birds")->is_loop_current_entry());

	// The whole list at once.
	Ref<AudioCommandList> stop_all;
	stop_all.instantiate();
	Ref<AudioCommand> stop = make_command(StringName(), AudioCommand::COMMAND_STOP_ALL, StringName());
	stop->set_fade_time(0.0f);
	stop_all->add_command(stop);
	CHECK(mixer->execute_list(stop_all) == OK);
	CHECK_FALSE(mixer->is_playing("birds"));

	DirAccess::remove_absolute(birds_path);
	DirAccess::remove_absolute(wind_path);
}

TEST_CASE("[Audio][AudioCommand] An invalid command fails alone") {
	MixerReset reset;
	AudioMixerServer *mixer = AudioMixerServer::get_singleton();
	REQUIRE(mixer != nullptr);
	mixer->add_channel(make_channel("music"));
	mixer->add_channel(make_channel("sfx"));
	const String song = save_wav("audio_command_song.tres", 10.0);
	CHECK(mixer->execute(make_command("song", AudioCommand::COMMAND_PLAY, "music", song)) == OK);
	REQUIRE(mixer->is_playing("music"));

	ERR_PRINT_OFF;
	CHECK(mixer->execute(make_command("ghost", AudioCommand::COMMAND_PLAY, "ghost", song)) == ERR_DOES_NOT_EXIST);
	CHECK(mixer->execute(make_command("missing", AudioCommand::COMMAND_PLAY, "sfx", TestUtils::get_temp_path("audio_command_nothing.tres"))) == ERR_FILE_NOT_FOUND);
	CHECK(mixer->execute(make_command("no_channel", AudioCommand::COMMAND_STOP, StringName())) == ERR_INVALID_PARAMETER);
	CHECK(mixer->execute(make_command("one_shot", AudioCommand::COMMAND_PLAY_ONE_SHOT, "sfx")) == ERR_INVALID_PARAMETER);
	CHECK(mixer->execute(make_command("seek", AudioCommand::COMMAND_SEEK, "sfx")) == ERR_UNAVAILABLE);
	CHECK(mixer->execute(make_command("clip", AudioCommand::COMMAND_SWITCH_CLIP, "music")) == ERR_INVALID_PARAMETER);
	Ref<AudioCommand> clip = make_command("clip", AudioCommand::COMMAND_SWITCH_CLIP, "music");
	clip->set_clip_name("Battle");
	CHECK(mixer->execute(clip) == ERR_UNAVAILABLE); // A WAV is not an AudioStreamInteractive.
	Ref<AudioCommand> pitch = make_command("pitch", AudioCommand::COMMAND_SET_PITCH, "music");
	pitch->set_pitch_scale(0.0f);
	CHECK(mixer->execute(pitch) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;

	// Nothing of the failed commands reached the channels.
	CHECK(mixer->is_playing("music"));
	CHECK_FALSE(mixer->is_playing("sfx"));
	CHECK(mixer->get_channel_pitch_scale("music") == doctest::Approx(1.0));

	// Mixing commands on the playing channel.
	Ref<AudioCommand> volume = make_command("volume", AudioCommand::COMMAND_SET_VOLUME, "music");
	volume->set_volume_db(-6.0f);
	CHECK(mixer->execute(volume) == OK);
	CHECK(mixer->get_channel_volume_db("music") == doctest::Approx(-6.0));
	Ref<AudioCommand> fade_out = make_command("out", AudioCommand::COMMAND_FADE_OUT, "music");
	fade_out->set_fade_time(1.0f);
	CHECK(mixer->execute(fade_out) == OK);
	CHECK(mixer->is_fading("music"));
	mixer->process(2.0);
	CHECK_FALSE(mixer->is_playing("music"));

	DirAccess::remove_absolute(song);
}

} // namespace TestAudioCommand
