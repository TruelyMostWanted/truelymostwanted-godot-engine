/**************************************************************************/
/*  audio_command.cpp                                                     */
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

#include "audio_command.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "scene/resources/audio/audio_mixer_channel.h"

// Inspector and text-format names, in the order of AudioCommand::Command.
static const char *AUDIO_COMMAND_LABELS[] = {
	"Play",
	"Stop",
	"Pause",
	"Resume",
	"Toggle",
	"Enqueue",
	"Dequeue",
	"Clear",
	"Change Index",
	"Next",
	"Previous",
	"Fade In",
	"Fade Out",
	"Cross Fade",
	"Set Volume",
	"Set Pitch",
	"Seek",
	"Play One Shot",
	"Switch Clip",
	"Stop All",
};

static_assert(sizeof(AUDIO_COMMAND_LABELS) / sizeof(AUDIO_COMMAND_LABELS[0]) == AudioCommand::COMMAND_STOP_ALL + 1, "Every AudioCommand::Command needs a label.");

/////////////////////////////////////////////////////////////////////////////
// AudioCommand.

String AudioCommand::get_command_label(Command p_command) {
	const int value = (int)p_command;
	if (value < 0 || value > (int)COMMAND_STOP_ALL) {
		return itos(value);
	}
	return AUDIO_COMMAND_LABELS[value];
}

void AudioCommand::set_command_name(const StringName &p_name) {
	command_name = p_name;
	emit_changed();
}

StringName AudioCommand::get_command_name() const {
	return command_name;
}

void AudioCommand::set_group(const StringName &p_group) {
	group = p_group;
	emit_changed();
}

StringName AudioCommand::get_group() const {
	return group;
}

void AudioCommand::set_channel(const StringName &p_channel) {
	channel = p_channel;
	emit_changed();
}

StringName AudioCommand::get_channel() const {
	return channel;
}

void AudioCommand::set_command(Command p_command) {
	command = p_command;
	emit_changed();
}

AudioCommand::Command AudioCommand::get_command() const {
	return command;
}

void AudioCommand::set_stream_path(const String &p_path) {
	stream_path = p_path;
	emit_changed();
}

String AudioCommand::get_stream_path() const {
	return stream_path;
}

void AudioCommand::set_volume_db(float p_volume_db) {
	volume_db = p_volume_db;
	emit_changed();
}

float AudioCommand::get_volume_db() const {
	return volume_db;
}

void AudioCommand::set_pitch_scale(float p_pitch_scale) {
	// Not validated here, so a CSV cell is never lost; execute() rejects values <= 0.
	pitch_scale = p_pitch_scale;
	emit_changed();
}

float AudioCommand::get_pitch_scale() const {
	return pitch_scale;
}

void AudioCommand::set_fade_time(float p_time) {
	fade_time = p_time < 0.0f ? -1.0f : p_time;
	emit_changed();
}

float AudioCommand::get_fade_time() const {
	return fade_time;
}

void AudioCommand::set_fade_curve(AudioMixerServer::FadeCurve p_curve) {
	fade_curve = p_curve;
	emit_changed();
}

AudioMixerServer::FadeCurve AudioCommand::get_fade_curve() const {
	return fade_curve;
}

void AudioCommand::set_queue_index(int p_index) {
	queue_index = MAX(-1, p_index);
	emit_changed();
}

int AudioCommand::get_queue_index() const {
	return queue_index;
}

void AudioCommand::set_position(float p_position) {
	position = MAX(0.0f, p_position);
	emit_changed();
}

float AudioCommand::get_position() const {
	return position;
}

void AudioCommand::set_loop_mode(LoopMode p_mode) {
	loop_mode = p_mode;
	emit_changed();
}

AudioCommand::LoopMode AudioCommand::get_loop_mode() const {
	return loop_mode;
}

void AudioCommand::set_clip_name(const StringName &p_clip) {
	clip_name = p_clip;
	emit_changed();
}

StringName AudioCommand::get_clip_name() const {
	return clip_name;
}

void AudioCommand::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_command_name", "name"), &AudioCommand::set_command_name);
	ClassDB::bind_method(D_METHOD("get_command_name"), &AudioCommand::get_command_name);
	ClassDB::bind_method(D_METHOD("set_group", "group"), &AudioCommand::set_group);
	ClassDB::bind_method(D_METHOD("get_group"), &AudioCommand::get_group);
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &AudioCommand::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &AudioCommand::get_channel);
	ClassDB::bind_method(D_METHOD("set_command", "command"), &AudioCommand::set_command);
	ClassDB::bind_method(D_METHOD("get_command"), &AudioCommand::get_command);
	ClassDB::bind_method(D_METHOD("set_stream_path", "path"), &AudioCommand::set_stream_path);
	ClassDB::bind_method(D_METHOD("get_stream_path"), &AudioCommand::get_stream_path);
	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioCommand::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioCommand::get_volume_db);
	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &AudioCommand::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioCommand::get_pitch_scale);
	ClassDB::bind_method(D_METHOD("set_fade_time", "time"), &AudioCommand::set_fade_time);
	ClassDB::bind_method(D_METHOD("get_fade_time"), &AudioCommand::get_fade_time);
	ClassDB::bind_method(D_METHOD("set_fade_curve", "curve"), &AudioCommand::set_fade_curve);
	ClassDB::bind_method(D_METHOD("get_fade_curve"), &AudioCommand::get_fade_curve);
	ClassDB::bind_method(D_METHOD("set_queue_index", "index"), &AudioCommand::set_queue_index);
	ClassDB::bind_method(D_METHOD("get_queue_index"), &AudioCommand::get_queue_index);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &AudioCommand::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &AudioCommand::get_position);
	ClassDB::bind_method(D_METHOD("set_loop_mode", "mode"), &AudioCommand::set_loop_mode);
	ClassDB::bind_method(D_METHOD("get_loop_mode"), &AudioCommand::get_loop_mode);
	ClassDB::bind_method(D_METHOD("set_clip_name", "clip"), &AudioCommand::set_clip_name);
	ClassDB::bind_method(D_METHOD("get_clip_name"), &AudioCommand::get_clip_name);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "command_name"), "set_command_name", "get_command_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "group"), "set_group", "get_group");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "channel"), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "command", PROPERTY_HINT_ENUM, "Play,Stop,Pause,Resume,Toggle,Enqueue,Dequeue,Clear,Change Index,Next,Previous,Fade In,Fade Out,Cross Fade,Set Volume,Set Pitch,Seek,Play One Shot,Switch Clip,Stop All"), "set_command", "get_command");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stream_path", PROPERTY_HINT_FILE), "set_stream_path", "get_stream_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.01,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_time", PROPERTY_HINT_RANGE, "-1,10,0.01,or_greater,suffix:s"), "set_fade_time", "get_fade_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fade_curve", PROPERTY_HINT_ENUM, "Linear,Ease In,Ease Out,Equal Power"), "set_fade_curve", "get_fade_curve");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "queue_index", PROPERTY_HINT_RANGE, "-1,100,1,or_greater"), "set_queue_index", "get_queue_index");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "position", PROPERTY_HINT_RANGE, "0,600,0.01,or_greater,suffix:s"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_mode", PROPERTY_HINT_ENUM, "Channel:-1,Off:0,On:1"), "set_loop_mode", "get_loop_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "clip_name"), "set_clip_name", "get_clip_name");

	BIND_ENUM_CONSTANT(COMMAND_PLAY);
	BIND_ENUM_CONSTANT(COMMAND_STOP);
	BIND_ENUM_CONSTANT(COMMAND_PAUSE);
	BIND_ENUM_CONSTANT(COMMAND_RESUME);
	BIND_ENUM_CONSTANT(COMMAND_TOGGLE);
	BIND_ENUM_CONSTANT(COMMAND_ENQUEUE);
	BIND_ENUM_CONSTANT(COMMAND_DEQUEUE);
	BIND_ENUM_CONSTANT(COMMAND_CLEAR);
	BIND_ENUM_CONSTANT(COMMAND_CHANGE_INDEX);
	BIND_ENUM_CONSTANT(COMMAND_NEXT);
	BIND_ENUM_CONSTANT(COMMAND_PREVIOUS);
	BIND_ENUM_CONSTANT(COMMAND_FADE_IN);
	BIND_ENUM_CONSTANT(COMMAND_FADE_OUT);
	BIND_ENUM_CONSTANT(COMMAND_CROSS_FADE);
	BIND_ENUM_CONSTANT(COMMAND_SET_VOLUME);
	BIND_ENUM_CONSTANT(COMMAND_SET_PITCH);
	BIND_ENUM_CONSTANT(COMMAND_SEEK);
	BIND_ENUM_CONSTANT(COMMAND_PLAY_ONE_SHOT);
	BIND_ENUM_CONSTANT(COMMAND_SWITCH_CLIP);
	BIND_ENUM_CONSTANT(COMMAND_STOP_ALL);

	BIND_ENUM_CONSTANT(LOOP_MODE_CHANNEL);
	BIND_ENUM_CONSTANT(LOOP_MODE_OFF);
	BIND_ENUM_CONSTANT(LOOP_MODE_ON);
}

/////////////////////////////////////////////////////////////////////////////
// AudioCommandList.

void AudioCommandList::set_commands(const TypedArray<AudioCommand> &p_commands) {
	commands = p_commands;
	emit_changed();
}

TypedArray<AudioCommand> AudioCommandList::get_commands() const {
	return commands;
}

void AudioCommandList::add_command(const Ref<AudioCommand> &p_command) {
	ERR_FAIL_COND_MSG(p_command.is_null(), "The audio command is null.");
	commands.push_back(p_command);
	emit_changed();
}

Ref<AudioCommand> AudioCommandList::find_command(const StringName &p_name) const {
	for (int i = 0; i < commands.size(); i++) {
		const Ref<AudioCommand> entry = commands[i];
		if (entry.is_valid() && entry->get_command_name() == p_name) {
			return entry;
		}
	}
	return Ref<AudioCommand>();
}

Error AudioCommandList::preload_streams() {
	preloaded_streams.clear();
	Error first_error = OK;
	String first_message;
	for (int i = 0; i < commands.size(); i++) {
		const Ref<AudioCommand> entry = commands[i];
		if (entry.is_null() || entry->get_stream_path().is_empty()) {
			continue;
		}
		Ref<AudioStream> stream;
		String message;
		const Error err = AudioCommandRegistry::load_stream(entry->get_stream_path(), stream, message);
		if (err != OK) {
			if (first_error == OK) {
				first_error = err;
				first_message = message;
			}
			continue;
		}
		if (!preloaded_streams.has(stream)) {
			preloaded_streams.push_back(stream);
		}
	}
	if (first_error != OK) {
		ERR_PRINT(vformat("Cannot preload all audio streams of the command list: %s", first_message));
	}
	return first_error;
}

void AudioCommandList::clear_preloaded_streams() {
	preloaded_streams.clear();
}

int AudioCommandList::get_preloaded_stream_count() const {
	return (int)preloaded_streams.size();
}

void AudioCommandList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_commands", "commands"), &AudioCommandList::set_commands);
	ClassDB::bind_method(D_METHOD("get_commands"), &AudioCommandList::get_commands);
	ClassDB::bind_method(D_METHOD("add_command", "command"), &AudioCommandList::add_command);
	ClassDB::bind_method(D_METHOD("find_command", "name"), &AudioCommandList::find_command);
	ClassDB::bind_method(D_METHOD("preload_streams"), &AudioCommandList::preload_streams);
	ClassDB::bind_method(D_METHOD("clear_preloaded_streams"), &AudioCommandList::clear_preloaded_streams);
	ClassDB::bind_method(D_METHOD("get_preloaded_stream_count"), &AudioCommandList::get_preloaded_stream_count);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "commands", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("AudioCommand")), "set_commands", "get_commands");
}

/////////////////////////////////////////////////////////////////////////////
// AudioCommandRegistry.

Error AudioCommandRegistry::load_stream(const String &p_path, Ref<AudioStream> &r_stream, String &r_message) {
	if (p_path.is_empty()) {
		r_message = "the stream path is empty.";
		return ERR_INVALID_PARAMETER;
	}
	if (!ResourceLoader::exists(p_path)) {
		r_message = vformat("the stream file \"%s\" does not exist.", p_path);
		return ERR_FILE_NOT_FOUND;
	}
	// ResourceLoader's cache is the stream cache: one instance per path while it is in use.
	const Ref<AudioStream> loaded = ResourceLoader::load(p_path);
	if (loaded.is_null()) {
		r_message = vformat("\"%s\" is not an AudioStream.", p_path);
		return ERR_FILE_UNRECOGNIZED;
	}
	r_stream = loaded;
	return OK;
}

void AudioCommandRegistry::_ensure_default_commands() {
	if (default_commands_checked) {
		return;
	}
	default_commands_checked = true;
	if (Engine::get_singleton()->is_editor_hint() || ProjectSettings::get_singleton() == nullptr || !ProjectSettings::get_singleton()->has_setting("audio/mixer/default_commands")) {
		return;
	}
	const String path = GLOBAL_GET("audio/mixer/default_commands");
	if (path.is_empty()) {
		return;
	}
	// The type hint lets a CSV file load as well.
	const Ref<AudioCommandList> list = ResourceLoader::load(path, "AudioCommandList");
	ERR_FAIL_COND_MSG(list.is_null(), vformat("Cannot load the audio command list \"%s\" (project setting \"audio/mixer/default_commands\").", path));
	register_commands(list);
}

Error AudioCommandRegistry::_dispatch(const AudioCommand &p_command, String &r_message) {
	const AudioCommand::Command type = p_command.get_command();
	if ((int)type < 0 || (int)type > (int)AudioCommand::COMMAND_STOP_ALL) {
		r_message = vformat("%d is not a valid command.", (int)type);
		return ERR_INVALID_PARAMETER;
	}
	if (type == AudioCommand::COMMAND_STOP_ALL) {
		server->stop_all(MAX(0.0f, p_command.get_fade_time()));
		return OK;
	}

	const StringName channel_name = p_command.get_channel();
	if (channel_name == StringName()) {
		r_message = "the command has no channel.";
		return ERR_INVALID_PARAMETER;
	}
	if (!server->has_channel(channel_name)) {
		r_message = vformat("the audio mixer channel \"%s\" does not exist.", channel_name);
		return ERR_DOES_NOT_EXIST;
	}
	const Ref<AudioMixerChannel> config = server->get_channel(channel_name);
	const float fade = p_command.get_fade_time() >= 0.0f ? p_command.get_fade_time() : (config.is_valid() ? config->get_default_fade_time() : 0.0f);
	const AudioMixerServer::FadeCurve curve = p_command.get_fade_curve();
	const bool playing = server->is_playing(channel_name);
	const bool stops = type == AudioCommand::COMMAND_TOGGLE && playing;

	// The stream: required by some commands, optional for others, loaded on first use.
	const String path = p_command.get_stream_path();
	const bool needs_stream = type == AudioCommand::COMMAND_ENQUEUE || type == AudioCommand::COMMAND_CROSS_FADE || type == AudioCommand::COMMAND_PLAY_ONE_SHOT;
	const bool takes_stream = needs_stream || type == AudioCommand::COMMAND_PLAY || (type == AudioCommand::COMMAND_TOGGLE && !stops) || type == AudioCommand::COMMAND_FADE_IN;
	if (needs_stream && path.is_empty()) {
		r_message = "the command needs a stream_path.";
		return ERR_INVALID_PARAMETER;
	}
	Ref<AudioStream> stream;
	if (takes_stream && !path.is_empty()) {
		if (type == AudioCommand::COMMAND_ENQUEUE) {
			// The queue loads its entries when they play; only check that the file is there.
			if (!ResourceLoader::exists(path)) {
				r_message = vformat("the stream file \"%s\" does not exist.", path);
				return ERR_FILE_NOT_FOUND;
			}
		} else {
			const Error err = load_stream(path, stream, r_message);
			if (err != OK) {
				return err;
			}
		}
	}

	const int queue_size = (int)server->get_queue(channel_name).size();
	const bool starts_playback = (type == AudioCommand::COMMAND_TOGGLE && !stops) || type == AudioCommand::COMMAND_PLAY || type == AudioCommand::COMMAND_FADE_IN || type == AudioCommand::COMMAND_CROSS_FADE || type == AudioCommand::COMMAND_CHANGE_INDEX || type == AudioCommand::COMMAND_NEXT || type == AudioCommand::COMMAND_PREVIOUS;
	if (starts_playback && stream.is_null() && queue_size == 0 && !(type == AudioCommand::COMMAND_FADE_IN && playing)) {
		r_message = vformat("the command has no stream_path and the queue of the audio mixer channel \"%s\" is empty.", channel_name);
		return ERR_DOES_NOT_EXIST;
	}
	if (starts_playback && p_command.get_loop_mode() != AudioCommand::LOOP_MODE_CHANNEL && config.is_valid()) {
		config->set_loop_entry(p_command.get_loop_mode() == AudioCommand::LOOP_MODE_ON);
	}
	Variant stream_argument;
	if (stream.is_valid()) {
		stream_argument = stream;
	}

	switch (type) {
		case AudioCommand::COMMAND_PLAY: {
			server->play(channel_name, stream_argument, p_command.get_position());
		} break;
		case AudioCommand::COMMAND_STOP: {
			server->stop(channel_name, fade);
		} break;
		case AudioCommand::COMMAND_PAUSE: {
			server->pause(channel_name);
		} break;
		case AudioCommand::COMMAND_RESUME: {
			server->resume(channel_name);
		} break;
		case AudioCommand::COMMAND_TOGGLE: {
			if (stops) {
				server->stop(channel_name, fade);
			} else {
				server->play(channel_name, stream_argument, p_command.get_position());
			}
		} break;
		case AudioCommand::COMMAND_ENQUEUE: {
			server->enqueue(channel_name, path, p_command.get_queue_index());
		} break;
		case AudioCommand::COMMAND_DEQUEUE: {
			const int index = p_command.get_queue_index() < 0 ? queue_size - 1 : p_command.get_queue_index();
			if (index < 0 || index >= queue_size) {
				r_message = vformat("the queue of the audio mixer channel \"%s\" has no entry %d.", channel_name, index);
				return ERR_PARAMETER_RANGE_ERROR;
			}
			server->dequeue(channel_name, index);
		} break;
		case AudioCommand::COMMAND_CLEAR: {
			server->clear_queue(channel_name);
		} break;
		case AudioCommand::COMMAND_CHANGE_INDEX: {
			const int index = p_command.get_queue_index();
			if (index < 0 || index >= queue_size) {
				r_message = vformat("the queue of the audio mixer channel \"%s\" has no entry %d.", channel_name, index);
				return ERR_PARAMETER_RANGE_ERROR;
			}
			server->change_index(channel_name, index, fade);
		} break;
		case AudioCommand::COMMAND_NEXT: {
			server->next(channel_name, fade);
		} break;
		case AudioCommand::COMMAND_PREVIOUS: {
			server->previous(channel_name, fade);
		} break;
		case AudioCommand::COMMAND_FADE_IN: {
			if (stream.is_valid()) {
				// On a silent channel a cross-fade is a fade-in of the new stream.
				server->cross_fade(channel_name, stream_argument, fade, curve);
			} else {
				server->fade_in(channel_name, fade, curve);
			}
		} break;
		case AudioCommand::COMMAND_FADE_OUT: {
			server->fade_out(channel_name, fade, curve);
		} break;
		case AudioCommand::COMMAND_CROSS_FADE: {
			server->cross_fade(channel_name, stream_argument, fade, curve);
		} break;
		case AudioCommand::COMMAND_SET_VOLUME: {
			server->set_channel_volume_db(channel_name, p_command.get_volume_db(), fade);
		} break;
		case AudioCommand::COMMAND_SET_PITCH: {
			if (p_command.get_pitch_scale() <= 0.0f) {
				r_message = "pitch_scale must be greater than 0.";
				return ERR_INVALID_PARAMETER;
			}
			server->set_channel_pitch_scale(channel_name, p_command.get_pitch_scale());
		} break;
		case AudioCommand::COMMAND_SEEK: {
			if (!playing) {
				r_message = vformat("the audio mixer channel \"%s\" is not playing.", channel_name);
				return ERR_UNAVAILABLE;
			}
			server->seek(channel_name, p_command.get_position());
		} break;
		case AudioCommand::COMMAND_PLAY_ONE_SHOT: {
			if (p_command.get_pitch_scale() <= 0.0f) {
				r_message = "pitch_scale must be greater than 0.";
				return ERR_INVALID_PARAMETER;
			}
			server->play_one_shot(channel_name, stream_argument, p_command.get_volume_db(), p_command.get_pitch_scale());
		} break;
		case AudioCommand::COMMAND_SWITCH_CLIP: {
			if (p_command.get_clip_name() == StringName()) {
				r_message = "the command needs a clip_name.";
				return ERR_INVALID_PARAMETER;
			}
			if (!playing) {
				r_message = vformat("the audio mixer channel \"%s\" is not playing.", channel_name);
				return ERR_UNAVAILABLE;
			}
			const Ref<AudioStreamPlayback> playback = server->get_channel_playback(channel_name);
			if (playback.is_null() || !playback->has_method(SNAME("switch_to_clip_by_name"))) {
				r_message = vformat("the stream of the audio mixer channel \"%s\" is not an AudioStreamInteractive.", channel_name);
				return ERR_UNAVAILABLE;
			}
			server->switch_clip(channel_name, p_command.get_clip_name());
		} break;
		case AudioCommand::COMMAND_STOP_ALL: {
			// Handled above: it has no channel.
		} break;
	}
	return OK;
}

Error AudioCommandRegistry::execute(const Ref<AudioCommand> &p_command) {
	ERR_FAIL_COND_V_MSG(p_command.is_null(), ERR_INVALID_PARAMETER, "The audio command is null.");
	// Keep the command alive: a signal handler may unregister it while it runs.
	const Ref<AudioCommand> held = p_command;
	String message;
	const Error err = _dispatch(*held.ptr(), message);
	if (err != OK) {
		const String label = AudioCommand::get_command_label(held->get_command());
		if (held->get_command_name() == StringName()) {
			ERR_PRINT(vformat("Audio command %s failed: %s", label, message));
		} else {
			ERR_PRINT(vformat("Audio command \"%s\" (%s) failed: %s", held->get_command_name(), label, message));
		}
		return err;
	}
	server->emit_signal(SNAME("command_executed"), held->get_command_name(), held);
	return OK;
}

Error AudioCommandRegistry::execute_list(const Ref<AudioCommandList> &p_list) {
	ERR_FAIL_COND_V_MSG(p_list.is_null(), ERR_INVALID_PARAMETER, "The audio command list is null.");
	const TypedArray<AudioCommand> list_commands = p_list->get_commands();
	Error first_error = OK;
	for (int i = 0; i < list_commands.size(); i++) {
		const Error err = execute(list_commands[i]);
		if (err != OK && first_error == OK) {
			first_error = err;
		}
	}
	return first_error;
}

Error AudioCommandRegistry::register_commands(const Ref<AudioCommandList> &p_list) {
	_ensure_default_commands();
	ERR_FAIL_COND_V_MSG(p_list.is_null(), ERR_INVALID_PARAMETER, "The audio command list is null.");
	const TypedArray<AudioCommand> list_commands = p_list->get_commands();
	LocalVector<StringName> replaced;
	for (int i = 0; i < list_commands.size(); i++) {
		const Ref<AudioCommand> entry = list_commands[i];
		if (entry.is_null() || registered.has(entry)) {
			continue;
		}
		registered.push_back(entry);
		const StringName entry_name = entry->get_command_name();
		if (entry_name == StringName()) {
			continue;
		}
		Ref<AudioCommand> *existing = named.getptr(entry_name);
		if (existing != nullptr && *existing != entry) {
			// The last registered command wins, so a later list (a mod) can override one on purpose.
			registered.erase(*existing);
			replaced.push_back(entry_name);
		}
		named[entry_name] = entry;
	}
	for (const StringName &replaced_name : replaced) {
		WARN_PRINT(vformat("The audio command \"%s\" was registered twice; the last one replaces the first.", replaced_name));
	}
	return OK;
}

void AudioCommandRegistry::unregister_commands(const Ref<AudioCommandList> &p_list) {
	_ensure_default_commands();
	ERR_FAIL_COND_MSG(p_list.is_null(), "The audio command list is null.");
	const TypedArray<AudioCommand> list_commands = p_list->get_commands();
	for (int i = 0; i < list_commands.size(); i++) {
		const Ref<AudioCommand> entry = list_commands[i];
		if (entry.is_null()) {
			continue;
		}
		registered.erase(entry);
		const Ref<AudioCommand> *existing = named.getptr(entry->get_command_name());
		if (existing != nullptr && *existing == entry) {
			named.erase(entry->get_command_name());
		}
	}
}

bool AudioCommandRegistry::has_command(const StringName &p_name) {
	_ensure_default_commands();
	return named.has(p_name);
}

Error AudioCommandRegistry::execute_named(const StringName &p_name) {
	_ensure_default_commands();
	const Ref<AudioCommand> *found = named.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(found, ERR_DOES_NOT_EXIST, vformat("No audio command named \"%s\" is registered.", p_name));
	const Ref<AudioCommand> found_command = *found;
	return execute(found_command);
}

Error AudioCommandRegistry::execute_group(const StringName &p_group) {
	_ensure_default_commands();
	// Collect first: a command (or a signal handler) may change the registry.
	LocalVector<Ref<AudioCommand>> members;
	for (const Ref<AudioCommand> &entry : registered) {
		if (entry->get_group() == p_group) {
			members.push_back(entry);
		}
	}
	ERR_FAIL_COND_V_MSG(members.is_empty(), ERR_DOES_NOT_EXIST, vformat("No audio command of the group \"%s\" is registered.", p_group));
	Error first_error = OK;
	for (const Ref<AudioCommand> &entry : members) {
		const Error err = execute(entry);
		if (err != OK && first_error == OK) {
			first_error = err;
		}
	}
	return first_error;
}

PackedStringArray AudioCommandRegistry::get_command_names() {
	_ensure_default_commands();
	PackedStringArray names;
	for (const Ref<AudioCommand> &entry : registered) {
		const StringName entry_name = entry->get_command_name();
		if (entry_name != StringName()) {
			names.push_back(entry_name);
		}
	}
	return names;
}

void AudioCommandRegistry::clear() {
	registered.clear();
	named.clear();
}

AudioCommandRegistry::AudioCommandRegistry(AudioMixerServer *p_server) {
	server = p_server;
}
