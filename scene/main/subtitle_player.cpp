/**************************************************************************/
/*  subtitle_player.cpp                                                   */
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

#include "subtitle_player.h"

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

// A String property of `p_object`, empty when it has none (`AnimationPlayer` has no `subtitle_path`).
static String _get_string_property(const Object *p_object, const StringName &p_property) {
	const Variant value = p_object->get(p_property);
	return value.get_type() == Variant::STRING ? String(value) : String();
}

Node *SubtitlePlayer::_get_sync_node() {
	if (sync_id.is_valid()) {
		Node *node = ObjectDB::get_instance<Node>(sync_id);
		if (node != nullptr) {
			return sync_position_method.is_empty() ? nullptr : node;
		}
		sync_id = ObjectID();
	}
	Node *node = is_inside_tree() ? get_node_or_null(sync_node) : nullptr;
	if (node == nullptr) {
		return nullptr;
	}
	// Resolved once per node: audio players, VideoStreamPlayer and AnimationPlayer, read through their bound methods.
	sync_id = node->get_instance_id();
	sync_position_method = StringName();
	sync_signals = false;
	if (node->has_method(SNAME("is_playing"))) {
		// Audio players: the position in the current track of a playlist.
		static const char *const methods[] = { "get_track_position", "get_stream_position", "get_current_animation_position" };
		for (const char *method : methods) {
			if (node->has_method(method)) {
				sync_position_method = method;
				break;
			}
		}
	}
	ERR_FAIL_COND_V_MSG(sync_position_method.is_empty(), nullptr, vformat("SubtitlePlayer cannot follow a %s: use an AudioStreamPlayer, AudioStreamPlayer2D, AudioStreamPlayer3D, VideoStreamPlayer or AnimationPlayer.", node->get_class()));
	sync_signals = node->has_signal(SNAME("playing_changed"));
	sync_paused_method = node->has_method(SNAME("get_stream_paused")) ? SNAME("get_stream_paused") : (node->has_method(SNAME("is_paused")) ? SNAME("is_paused") : StringName());
	_connect_sync(true);
	_update_source();
	return node;
}

void SubtitlePlayer::_connect_sync(bool p_connect) {
	Node *node = sync_signals ? ObjectDB::get_instance<Node>(sync_id) : nullptr;
	if (node == nullptr) {
		return;
	}
	static const char *const signals[] = { "playing_changed", "stream_changed", "track_changed" };
	const Callable callables[] = { callable_mp(this, &SubtitlePlayer::_sync_playing_changed), callable_mp(this, &SubtitlePlayer::_sync_stream_changed), callable_mp(this, &SubtitlePlayer::_sync_track_changed) };
	for (int i = 0; i < 3; i++) {
		if (p_connect && node->has_signal(signals[i])) {
			node->connect(signals[i], callables[i]);
		} else if (!p_connect && node->is_connected(signals[i], callables[i])) {
			node->disconnect(signals[i], callables[i]);
		}
	}
}

void SubtitlePlayer::_release_sync() {
	_connect_sync(false);
	sync_id = ObjectID();
	if (is_ready() && !Engine::get_singleton()->is_editor_hint()) {
		set_process_internal(true); // Resolves the sync node again in the next frame.
	}
}

void SubtitlePlayer::_update_source(const Ref<Resource> &p_stream) {
	Node *node = subtitle.is_null() && sync_id.is_valid() ? ObjectDB::get_instance<Node>(sync_id) : nullptr;
	if (node == nullptr) {
		return;
	}
	// The player's override, else the current stream (or playlist track), else a sidecar file next to the stream file.
	String path = _get_string_property(node, SNAME("subtitle_path"));
	if (path.is_empty()) {
		Ref<Resource> stream = p_stream;
		if (stream.is_null() && node->has_method(SNAME("get_current_track_stream"))) {
			stream = node->call(SNAME("get_current_track_stream"));
		}
		if (stream.is_null()) {
			stream = node->get(SNAME("stream"));
		}
		if (stream.is_valid()) {
			path = _get_string_property(stream.ptr(), SNAME("subtitle_path"));
			if (path.is_empty()) {
				String file = _get_string_property(stream.ptr(), SNAME("file")); // Video streams.
				if (file.is_empty() && !stream->is_built_in()) {
					file = stream->get_path();
				}
				static const char *const sidecar_extensions[] = { ".srt", ".vtt" };
				for (const char *extension : sidecar_extensions) {
					const String sidecar = file.get_basename() + extension;
					if (!file.is_empty() && ResourceLoader::exists(sidecar, "Subtitle")) {
						path = sidecar;
						break;
					}
				}
			}
		}
	}
	if (path == source_path) {
		return;
	}
	source_path = path;
	// Through the ResourceLoader, so translation remaps apply and loaded subtitles are shared.
	playback->set_subtitle(path.is_empty() ? Ref<Subtitle>() : Ref<Subtitle>(ResourceLoader::load(path, "Subtitle")));
}

bool SubtitlePlayer::_is_sync_paused(Node *p_node) const {
	// An audio player with a paused stream, or a video that is paused while it plays (`paused` stays set after stop()).
	if (sync_paused_method.is_empty() || !bool(p_node->call(sync_paused_method))) {
		return false;
	}
	return sync_paused_method == SNAME("get_stream_paused") || bool(p_node->call(SNAME("is_playing")));
}

void SubtitlePlayer::_sync_stopped(Node *p_node) {
	playing = false;
	if (p_node != nullptr && _is_sync_paused(p_node)) {
		// Paused: the text stays. Checked every frame, since a stop while paused emits no `playing_changed`.
		set_process_internal(true);
		return;
	}
	// Stopped or finished: hide what it showed.
	playback->reset();
	if (p_node != nullptr && sync_signals) {
		set_process_internal(false); // Until `playing_changed(true)`.
	}
}

void SubtitlePlayer::_sync_playing_changed(bool p_playing) {
	if (p_playing) {
		set_process_internal(true);
		_update_source(); // Picks up a changed override; the position is read in the next frame.
	} else {
		_sync_stopped(_get_sync_node());
	}
}

void SubtitlePlayer::_sync_stream_changed(const Ref<Resource> &p_stream) {
	_update_source(p_stream);
}

void SubtitlePlayer::_sync_track_changed(int p_index, const Ref<Resource> &p_stream) {
	_update_source(p_stream);
}

void SubtitlePlayer::_advance(double p_delta) {
	if (sync_node.is_empty()) {
		if (playing) {
			position += p_delta;
			playback->update(position + time_offset);
		}
		return;
	}
	Node *node = _get_sync_node();
	// A node with `playing_changed` is known to play while this processes; others are asked every frame.
	if (node != nullptr && ((sync_signals && playing) || (bool(node->call(SNAME("is_playing"))) && !_is_sync_paused(node)))) {
		playing = true;
		position = node->call(sync_position_method);
		playback->update(position + time_offset);
		return;
	}
	_sync_stopped(node);
}

void SubtitlePlayer::_segment_changed(const Ref<SubtitleSegment> &p_segment) {
	emit_signal(SNAME("segment_changed"), p_segment);
}

void SubtitlePlayer::_segment_started(const Ref<SubtitleSegment> &p_segment) {
	emit_signal(SNAME("segment_started"), p_segment);
}

void SubtitlePlayer::_segment_ended(const Ref<SubtitleSegment> &p_segment) {
	emit_signal(SNAME("segment_ended"), p_segment);
}

void SubtitlePlayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
			set_process_internal(true);
			if (autoplay && sync_node.is_empty()) {
				play();
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			_advance(get_process_delta_time());
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_release_sync();
		} break;
	}
}

void SubtitlePlayer::set_subtitle(const Ref<Subtitle> &p_subtitle) {
	subtitle = p_subtitle;
	source_path = String();
	playback->set_subtitle(subtitle);
	_update_source();
}

Ref<Subtitle> SubtitlePlayer::get_subtitle() const {
	return subtitle;
}

void SubtitlePlayer::set_sync_node(const NodePath &p_path) {
	if (sync_node == p_path) {
		return;
	}
	_release_sync();
	sync_node = p_path;
	playing = false;
	playback->reset();
	if (subtitle.is_null()) {
		source_path = String();
		playback->set_subtitle(Ref<Subtitle>());
	}
}

NodePath SubtitlePlayer::get_sync_node() const {
	return sync_node;
}

void SubtitlePlayer::set_time_offset(double p_offset) {
	time_offset = p_offset;
}

double SubtitlePlayer::get_time_offset() const {
	return time_offset;
}

void SubtitlePlayer::set_autoplay(bool p_enabled) {
	autoplay = p_enabled;
}

bool SubtitlePlayer::is_autoplay_enabled() const {
	return autoplay;
}

void SubtitlePlayer::play(double p_from_position) {
	ERR_FAIL_COND_MSG(!sync_node.is_empty(), "This SubtitlePlayer follows its sync node: play the sync node instead.");
	playing = true;
	position = p_from_position;
	playback->seek(position + time_offset);
}

void SubtitlePlayer::stop() {
	ERR_FAIL_COND_MSG(!sync_node.is_empty(), "This SubtitlePlayer follows its sync node: stop the sync node instead.");
	playing = false;
	position = 0.0;
	playback->reset();
}

void SubtitlePlayer::seek(double p_position) {
	ERR_FAIL_COND_MSG(!sync_node.is_empty(), "This SubtitlePlayer follows its sync node: seek the sync node instead.");
	position = p_position;
	if (playing) {
		playback->seek(position + time_offset);
	}
}

bool SubtitlePlayer::is_playing() const {
	return playing;
}

double SubtitlePlayer::get_playback_position() const {
	return position;
}

Ref<SubtitlePlayback> SubtitlePlayer::get_playback() const {
	return playback;
}

void SubtitlePlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_subtitle", "subtitle"), &SubtitlePlayer::set_subtitle);
	ClassDB::bind_method(D_METHOD("get_subtitle"), &SubtitlePlayer::get_subtitle);
	ClassDB::bind_method(D_METHOD("set_sync_node", "path"), &SubtitlePlayer::set_sync_node);
	ClassDB::bind_method(D_METHOD("get_sync_node"), &SubtitlePlayer::get_sync_node);
	ClassDB::bind_method(D_METHOD("set_time_offset", "offset"), &SubtitlePlayer::set_time_offset);
	ClassDB::bind_method(D_METHOD("get_time_offset"), &SubtitlePlayer::get_time_offset);
	ClassDB::bind_method(D_METHOD("set_autoplay", "enabled"), &SubtitlePlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &SubtitlePlayer::is_autoplay_enabled);

	ClassDB::bind_method(D_METHOD("play", "from_position"), &SubtitlePlayer::play, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("stop"), &SubtitlePlayer::stop);
	ClassDB::bind_method(D_METHOD("seek", "position"), &SubtitlePlayer::seek);
	ClassDB::bind_method(D_METHOD("is_playing"), &SubtitlePlayer::is_playing);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &SubtitlePlayer::get_playback_position);
	ClassDB::bind_method(D_METHOD("get_playback"), &SubtitlePlayer::get_playback);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "subtitle", PROPERTY_HINT_RESOURCE_TYPE, "Subtitle"), "set_subtitle", "get_subtitle");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sync_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "AudioStreamPlayer,AudioStreamPlayer2D,AudioStreamPlayer3D,VideoStreamPlayer,AnimationPlayer"), "set_sync_node", "get_sync_node");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_offset", PROPERTY_HINT_RANGE, "-60,60,0.001,or_less,or_greater,suffix:s"), "set_time_offset", "get_time_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");

	ADD_SIGNAL(MethodInfo("segment_changed", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
	ADD_SIGNAL(MethodInfo("segment_started", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
	ADD_SIGNAL(MethodInfo("segment_ended", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
}

SubtitlePlayer::SubtitlePlayer() {
	playback.instantiate();
	playback->connect(SNAME("segment_changed"), callable_mp(this, &SubtitlePlayer::_segment_changed));
	playback->connect(SNAME("segment_started"), callable_mp(this, &SubtitlePlayer::_segment_started));
	playback->connect(SNAME("segment_ended"), callable_mp(this, &SubtitlePlayer::_segment_ended));
}
