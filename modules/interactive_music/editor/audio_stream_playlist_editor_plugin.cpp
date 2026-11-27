/**************************************************************************/
/*  audio_stream_playlist_editor_plugin.cpp                               */
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

#include "audio_stream_playlist_editor_plugin.h"

#include "../audio_stream_playlist.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/scene_string_names.h"

void EditorPropertyAudioStreamList::_set_streams(const Array &p_streams) {
	// The Inspector applies the value as one undo step.
	emit_changed(get_edited_property(), p_streams);
}

void EditorPropertyAudioStreamList::_stream_changed(const Ref<Resource> &p_stream, int p_index) {
	Array streams = Array(get_edited_property_value()).duplicate();
	if (p_index >= 0 && p_index < streams.size()) {
		streams.set(p_index, p_stream);
		_set_streams(streams);
	}
}

void EditorPropertyAudioStreamList::_stream_selected(const Ref<Resource> &p_stream, bool p_inspect) {
	emit_signal(SNAME("resource_selected"), get_edited_property(), p_stream);
}

void EditorPropertyAudioStreamList::_move(int p_index, int p_offset) {
	Array streams = Array(get_edited_property_value()).duplicate();
	const int target = p_index + p_offset;
	if (p_index < 0 || p_index >= streams.size() || target < 0 || target >= streams.size()) {
		return;
	}
	const Variant stream = streams.get(p_index);
	streams.set(p_index, streams.get(target));
	streams.set(target, stream);
	_set_streams(streams);
}

void EditorPropertyAudioStreamList::_remove(int p_index) {
	Array streams = Array(get_edited_property_value()).duplicate();
	if (p_index >= 0 && p_index < streams.size()) {
		streams.remove_at(p_index);
		_set_streams(streams);
	}
}

void EditorPropertyAudioStreamList::_add() {
	Array streams = Array(get_edited_property_value()).duplicate();
	streams.push_back(Variant());
	_set_streams(streams);
}

void EditorPropertyAudioStreamList::update_property() {
	for (int i = rows->get_child_count() - 1; i >= 0; i--) {
		Node *row = rows->get_child(i);
		rows->remove_child(row);
		row->queue_free();
	}

	const Array streams = get_edited_property_value();
	const bool locked = is_read_only();
	for (int i = 0; i < streams.size(); i++) {
		HBoxContainer *row = memnew(HBoxContainer);
		rows->add_child(row);

		Label *number = memnew(Label);
		number->set_text(itos(i + 1));
		row->add_child(number);

		EditorResourcePicker *picker = memnew(EditorResourcePicker);
		picker->set_base_type(AudioStream::get_class_static());
		picker->set_edited_resource(Ref<Resource>(streams[i]));
		picker->set_editable(!locked);
		picker->set_custom_minimum_size(Size2(96 * EDSCALE, 0));
		picker->connect(SNAME("resource_changed"), callable_mp(this, &EditorPropertyAudioStreamList::_stream_changed).bind(i));
		picker->connect(SNAME("resource_selected"), callable_mp(this, &EditorPropertyAudioStreamList::_stream_selected));
		row->add_child(picker);

		// The picker alone shows only a waveform thumbnail: name the track (title from the tags, else the file name).
		Label *name = memnew(Label);
		const Ref<AudioStream> stream = streams[i];
		String track_name = TTR("(empty)");
		if (stream.is_valid()) {
			track_name = !stream->get_title().is_empty() ? stream->get_title() : (!stream->get_path().is_empty() ? stream->get_path().get_file() : stream->get_class());
			name->set_tooltip_text(stream->get_path());
		}
		name->set_text(track_name);
		name->set_mouse_filter(MOUSE_FILTER_PASS);
		name->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		name->set_clip_text(true);
		name->set_h_size_flags(SIZE_EXPAND_FILL);
		row->add_child(name);

		struct RowButton {
			const char *icon;
			String tooltip;
			Callable callback;
			bool disabled;
		};
		const RowButton buttons[] = {
			{ "MoveUp", TTR("Move up: played earlier."), callable_mp(this, &EditorPropertyAudioStreamList::_move).bind(i, -1), i == 0 },
			{ "MoveDown", TTR("Move down: played later."), callable_mp(this, &EditorPropertyAudioStreamList::_move).bind(i, 1), i == streams.size() - 1 },
			{ "Remove", TTR("Remove."), callable_mp(this, &EditorPropertyAudioStreamList::_remove).bind(i), false },
		};
		for (const RowButton &info : buttons) {
			Button *button = memnew(Button);
			button->set_flat(true);
			button->set_button_icon(get_editor_theme_icon(info.icon));
			button->set_tooltip_text(info.tooltip);
			button->set_disabled(locked || info.disabled);
			button->connect(SceneStringName(pressed), info.callback);
			row->add_child(button);
		}
	}
	add_button->set_disabled(locked || streams.size() >= ClassDB::get_integer_constant(AudioStreamPlaylist::get_class_static(), SNAME("MAX_STREAMS")));
}

void EditorPropertyAudioStreamList::_set_read_only(bool p_read_only) {
	update_property();
}

void EditorPropertyAudioStreamList::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		update_property();
	}
}

EditorPropertyAudioStreamList::EditorPropertyAudioStreamList() {
	VBoxContainer *box = memnew(VBoxContainer);
	add_child(box);
	set_bottom_editor(box);

	rows = memnew(VBoxContainer);
	box->add_child(rows);

	add_button = memnew(Button);
	add_button->set_text(TTR("Add Stream"));
	add_button->set_h_size_flags(SIZE_SHRINK_BEGIN);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyAudioStreamList::_add));
	box->add_child(add_button);
}

bool EditorInspectorPluginAudioStreamPlaylist::can_handle(Object *p_object) {
	return Object::cast_to<AudioStreamPlaylist>(p_object) != nullptr;
}

bool EditorInspectorPluginAudioStreamPlaylist::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path != "streams") {
		return false;
	}
	add_property_editor(p_path, memnew(EditorPropertyAudioStreamList));
	return true;
}

AudioStreamPlaylistEditorPlugin::AudioStreamPlaylistEditorPlugin() {
	Ref<EditorInspectorPluginAudioStreamPlaylist> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
