/**************************************************************************/
/*  subtitle_label.cpp                                                    */
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

#include "subtitle_label.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/subtitle_player.h"
#include "servers/settings/accessibility_settings_server.h"
#include "servers/subtitle/subtitle_server.h"

static const char *const PLAYER_SIGNALS[] = { "segment_changed", "segment_started", "segment_ended" };
static const char *const SETTINGS_SIGNALS[] = { "subtitles_enabled_changed", "subtitle_scale_changed", "font_size_factor_changed" };

SubtitlePlayer *SubtitleLabel::_get_player() const {
	return ObjectDB::get_instance<SubtitlePlayer>(player_id);
}

void SubtitleLabel::_connect_player(bool p_connect) {
	const Callable callback = callable_mp(this, &SubtitleLabel::refresh).unbind(1);
	SubtitlePlayer *subtitle_player = _get_player();
	if (subtitle_player != nullptr) {
		for (const char *signal_name : PLAYER_SIGNALS) {
			if (subtitle_player->is_connected(signal_name, callback)) {
				subtitle_player->disconnect(signal_name, callback);
			}
		}
	}
	player_id = ObjectID();
	if (!p_connect || !is_inside_tree() || player.is_empty()) {
		return;
	}
	SubtitlePlayer *found = Object::cast_to<SubtitlePlayer>(get_node_or_null(player));
	if (found == nullptr) {
		return;
	}
	player_id = found->get_instance_id();
	for (const char *signal_name : PLAYER_SIGNALS) {
		found->connect(signal_name, callback);
	}
}

void SubtitleLabel::_connect_settings(bool p_connect) {
	AccessibilitySettingsServer *settings = AccessibilitySettingsServer::get_singleton();
	if (settings == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &SubtitleLabel::refresh).unbind(1);
	for (const char *signal_name : SETTINGS_SIGNALS) {
		const bool connected = settings->is_connected(signal_name, callback);
		if (p_connect && !connected) {
			settings->connect(signal_name, callback);
		} else if (!p_connect && connected) {
			settings->disconnect(signal_name, callback);
		}
	}
}

int SubtitleLabel::get_display_font_size() const {
	const AccessibilitySettingsServer *settings = AccessibilitySettingsServer::get_singleton();
	return (follow_accessibility_settings && settings != nullptr) ? settings->scale_subtitle_font_size(font_size) : font_size;
}

String SubtitleLabel::get_subtitle_text() const {
	const SubtitleServer *server = SubtitleServer::get_singleton();
	const SubtitlePlayer *subtitle_player = _get_player();
	if (server == nullptr || subtitle_player == nullptr || !server->is_enabled()) {
		return String();
	}
	if (follow_accessibility_settings) {
		const AccessibilitySettingsServer *settings = AccessibilitySettingsServer::get_singleton();
		if (settings != nullptr && !settings->is_subtitles_enabled()) {
			return String();
		}
	}
	const Ref<SubtitlePlayback> playback = subtitle_player->get_playback();
	const Ref<Subtitle> subtitle = playback->get_subtitle();
	const TypedArray<SubtitleSegment> segments = playback->get_active_segments();
	PackedStringArray lines;
	for (int idx = 0; idx < segments.size(); idx++) {
		const Ref<SubtitleSegment> segment = segments[idx];
		if (segment.is_null() || (segment->is_caption() && !server->is_captions_enabled())) {
			continue;
		}
		String line = server->get_display_text(subtitle, segment, true);
		const StringName speaker = segment->get_speaker();
		if (speaker != StringName() && !segment->is_caption()) {
			if (show_speaker_names) {
				line = String(speaker).replace("[", "[lb]") + ": " + line;
			}
			if (speaker_colors.has(speaker)) {
				const Color color = speaker_colors.get(speaker, Color());
				line = "[color=#" + color.to_html() + "]" + line + "[/color]";
			}
		}
		lines.push_back(line);
	}
	if (lines.is_empty()) {
		return String();
	}
	// A BBCode tag rather than a theme override: nothing of it is saved with the scene.
	return vformat("[font_size=%d]", get_display_font_size()) + String("\n").join(lines) + "[/font_size]";
}

void SubtitleLabel::refresh() {
	const String bbcode = get_subtitle_text();
	if (bbcode != get_text()) {
		set_text(bbcode);
	}
	if (!Engine::get_singleton()->is_editor_hint()) {
		set_visible(!bbcode.is_empty());
	}
}

void SubtitleLabel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// Ready, not enter tree: the player may be a later sibling.
			_connect_player(true);
			_connect_settings(true);
			refresh();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_connect_player(false);
			_connect_settings(false);
			request_ready();
		} break;
	}
}

void SubtitleLabel::_validate_property(PropertyInfo &p_property) const {
	// Written by the label itself: not stored, not editable.
	if (p_property.name == "text" || p_property.name == "bbcode_enabled") {
		p_property.usage = (p_property.usage & ~PROPERTY_USAGE_STORAGE) | PROPERTY_USAGE_READ_ONLY;
	}
}

void SubtitleLabel::set_player(const NodePath &p_path) {
	if (player == p_path) {
		return;
	}
	player = p_path;
	if (is_ready()) {
		_connect_player(true);
		refresh();
	}
}

NodePath SubtitleLabel::get_player() const {
	return player;
}

void SubtitleLabel::set_speaker_colors(const TypedDictionary<StringName, Color> &p_colors) {
	speaker_colors = p_colors;
	refresh();
}

TypedDictionary<StringName, Color> SubtitleLabel::get_speaker_colors() const {
	return speaker_colors;
}

void SubtitleLabel::set_show_speaker_names(bool p_enabled) {
	show_speaker_names = p_enabled;
	refresh();
}

bool SubtitleLabel::is_showing_speaker_names() const {
	return show_speaker_names;
}

void SubtitleLabel::set_font_size(int p_size) {
	font_size = MAX(1, p_size);
	refresh();
}

int SubtitleLabel::get_font_size() const {
	return font_size;
}

void SubtitleLabel::set_follow_accessibility_settings(bool p_enabled) {
	follow_accessibility_settings = p_enabled;
	refresh();
}

bool SubtitleLabel::is_following_accessibility_settings() const {
	return follow_accessibility_settings;
}

void SubtitleLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_player", "path"), &SubtitleLabel::set_player);
	ClassDB::bind_method(D_METHOD("get_player"), &SubtitleLabel::get_player);
	ClassDB::bind_method(D_METHOD("set_speaker_colors", "colors"), &SubtitleLabel::set_speaker_colors);
	ClassDB::bind_method(D_METHOD("get_speaker_colors"), &SubtitleLabel::get_speaker_colors);
	ClassDB::bind_method(D_METHOD("set_show_speaker_names", "enabled"), &SubtitleLabel::set_show_speaker_names);
	ClassDB::bind_method(D_METHOD("is_showing_speaker_names"), &SubtitleLabel::is_showing_speaker_names);
	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &SubtitleLabel::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &SubtitleLabel::get_font_size);
	ClassDB::bind_method(D_METHOD("set_follow_accessibility_settings", "enabled"), &SubtitleLabel::set_follow_accessibility_settings);
	ClassDB::bind_method(D_METHOD("is_following_accessibility_settings"), &SubtitleLabel::is_following_accessibility_settings);
	ClassDB::bind_method(D_METHOD("get_display_font_size"), &SubtitleLabel::get_display_font_size);
	ClassDB::bind_method(D_METHOD("get_subtitle_text"), &SubtitleLabel::get_subtitle_text);
	ClassDB::bind_method(D_METHOD("refresh"), &SubtitleLabel::refresh);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "player", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "SubtitlePlayer"), "set_player", "get_player");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "speaker_colors", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;Color"), "set_speaker_colors", "get_speaker_colors");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_speaker_names"), "set_show_speaker_names", "is_showing_speaker_names");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size", PROPERTY_HINT_RANGE, "1,256,1,or_greater,suffix:px"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "follow_accessibility_settings"), "set_follow_accessibility_settings", "is_following_accessibility_settings");
}

SubtitleLabel::SubtitleLabel() {
	set_use_bbcode(true);
	set_fit_content(true);
	set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}
