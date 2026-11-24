/**************************************************************************/
/*  audio_command.h                                                       */
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

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "scene/audio/audio_mixer_server.h"
#include "scene/resources/audio/audio_stream.h"

// One action on an AudioMixerServer channel. All stored properties are flat values,
// so a list of commands saves as .tres, JSON, INI or CSV (one row per command).
class AudioCommand : public Resource {
	GDCLASS(AudioCommand, Resource);

public:
	enum Command {
		COMMAND_PLAY,
		COMMAND_STOP,
		COMMAND_PAUSE,
		COMMAND_RESUME,
		COMMAND_TOGGLE,
		COMMAND_ENQUEUE,
		COMMAND_DEQUEUE,
		COMMAND_CLEAR,
		COMMAND_CHANGE_INDEX,
		COMMAND_NEXT,
		COMMAND_PREVIOUS,
		COMMAND_FADE_IN,
		COMMAND_FADE_OUT,
		COMMAND_CROSS_FADE,
		COMMAND_SET_VOLUME,
		COMMAND_SET_PITCH,
		COMMAND_SEEK,
		COMMAND_PLAY_ONE_SHOT,
		COMMAND_SWITCH_CLIP,
		COMMAND_STOP_ALL,
	};

	enum LoopMode {
		LOOP_MODE_CHANNEL = -1,
		LOOP_MODE_OFF = 0,
		LOOP_MODE_ON = 1,
	};

private:
	StringName command_name;
	StringName group;
	StringName channel;
	Command command = COMMAND_PLAY;
	String stream_path;
	float volume_db = 0.0f;
	float pitch_scale = 1.0f;
	float fade_time = -1.0f;
	AudioMixerServer::FadeCurve fade_curve = AudioMixerServer::FADE_CURVE_LINEAR;
	int queue_index = -1;
	float position = 0.0f;
	LoopMode loop_mode = LOOP_MODE_CHANNEL;
	StringName clip_name;

protected:
	static void _bind_methods();

public:
	// The Inspector and text-format name of a command, for example "Fade In".
	static String get_command_label(Command p_command);

	void set_command_name(const StringName &p_name);
	StringName get_command_name() const;

	void set_group(const StringName &p_group);
	StringName get_group() const;

	void set_channel(const StringName &p_channel);
	StringName get_channel() const;

	void set_command(Command p_command);
	Command get_command() const;

	void set_stream_path(const String &p_path);
	String get_stream_path() const;

	void set_volume_db(float p_volume_db);
	float get_volume_db() const;

	void set_pitch_scale(float p_pitch_scale);
	float get_pitch_scale() const;

	void set_fade_time(float p_time);
	float get_fade_time() const;

	void set_fade_curve(AudioMixerServer::FadeCurve p_curve);
	AudioMixerServer::FadeCurve get_fade_curve() const;

	void set_queue_index(int p_index);
	int get_queue_index() const;

	void set_position(float p_position);
	float get_position() const;

	void set_loop_mode(LoopMode p_mode);
	LoopMode get_loop_mode() const;

	void set_clip_name(const StringName &p_clip);
	StringName get_clip_name() const;
};

// Many commands in one resource; as CSV one row per command.
class AudioCommandList : public Resource {
	GDCLASS(AudioCommandList, Resource);

	TypedArray<AudioCommand> commands;
	LocalVector<Ref<AudioStream>> preloaded_streams; // Not stored: kept alive by preload_streams().

protected:
	static void _bind_methods();

public:
	void set_commands(const TypedArray<AudioCommand> &p_commands);
	TypedArray<AudioCommand> get_commands() const;

	void add_command(const Ref<AudioCommand> &p_command);
	Ref<AudioCommand> find_command(const StringName &p_name) const;

	Error preload_streams();
	void clear_preloaded_streams();
	int get_preloaded_stream_count() const;
};

// C++ only: the command dispatcher and the named commands of AudioMixerServer.
// Every call comes through an AudioMixerServer method, on the main thread.
class AudioCommandRegistry {
	AudioMixerServer *server = nullptr;
	LocalVector<Ref<AudioCommand>> registered; // In registration order, named or not.
	HashMap<StringName, Ref<AudioCommand>> named;
	bool default_commands_checked = false;

	void _ensure_default_commands();
	Error _dispatch(const AudioCommand &p_command, String &r_message);

public:
	// Loads an AudioStream from a path through the resource cache; r_message explains a failure.
	static Error load_stream(const String &p_path, Ref<AudioStream> &r_stream, String &r_message);

	Error execute(const Ref<AudioCommand> &p_command);
	Error execute_list(const Ref<AudioCommandList> &p_list);
	Error register_commands(const Ref<AudioCommandList> &p_list);
	void unregister_commands(const Ref<AudioCommandList> &p_list);
	bool has_command(const StringName &p_name);
	Error execute_named(const StringName &p_name);
	Error execute_group(const StringName &p_group);
	PackedStringArray get_command_names();
	void clear();

	explicit AudioCommandRegistry(AudioMixerServer *p_server);
};

VARIANT_ENUM_CAST(AudioCommand::Command);
VARIANT_ENUM_CAST(AudioCommand::LoopMode);
