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
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

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
	if (node->has_method(SNAME("is_playing"))) {
		static const char *const methods[] = { "get_playback_position", "get_stream_position", "get_current_animation_position" };
		for (const char *method : methods) {
			if (node->has_method(method)) {
				sync_position_method = method;
				break;
			}
		}
	}
	ERR_FAIL_COND_V_MSG(sync_position_method.is_empty(), nullptr, vformat("SubtitlePlayer cannot follow a %s: use an AudioStreamPlayer, AudioStreamPlayer2D, AudioStreamPlayer3D, VideoStreamPlayer or AnimationPlayer.", node->get_class()));
	return node;
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
	if (node != nullptr && bool(node->call(SNAME("is_playing")))) {
		playing = true;
		position = node->call(sync_position_method);
		playback->update(position + time_offset);
	} else if (playing) {
		// The sync node stopped (or its audio stream was paused): hide what it showed.
		playing = false;
		playback->reset();
	}
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
			sync_id = ObjectID();
		} break;
	}
}

void SubtitlePlayer::set_subtitle(const Ref<Subtitle> &p_subtitle) {
	playback->set_subtitle(p_subtitle);
}

Ref<Subtitle> SubtitlePlayer::get_subtitle() const {
	return playback->get_subtitle();
}

void SubtitlePlayer::set_sync_node(const NodePath &p_path) {
	if (sync_node == p_path) {
		return;
	}
	sync_node = p_path;
	sync_id = ObjectID();
	playing = false;
	playback->reset();
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
