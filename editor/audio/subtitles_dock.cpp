/**************************************************************************/
/*  subtitles_dock.cpp                                                    */
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

#include "subtitles_dock.h"

#include "core/input/input_event.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/audio/audio_stream_preview.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/inspector/subtitle_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/resources/audio/audio_stream.h"
#include "scene/scene_string_names.h"

// Static helpers

String SubtitlesDock::get_stream_subtitle_path(const AudioStream *p_stream) {
	if (p_stream == nullptr) {
		return String();
	}
	const String path = p_stream->get_subtitle_path();
	if (!path.is_empty()) {
		return path;
	}
	// The same sidecar rule as SubtitlePlayer.
	const String file = p_stream->get_path();
	if (!file.is_resource_file()) {
		return String();
	}
	static const char *const sidecar_extensions[] = { ".srt", ".vtt" };
	for (const char *extension : sidecar_extensions) {
		const String sidecar = file.get_basename() + extension;
		if (ResourceLoader::exists(sidecar, "Subtitle")) {
			return sidecar;
		}
	}
	return String();
}

String SubtitlesDock::find_sidecar_stream(const String &p_subtitle_path) {
	if (!p_subtitle_path.is_resource_file()) {
		return String();
	}
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("AudioStream", &extensions);
	for (const String &extension : extensions) {
		const String candidate = p_subtitle_path.get_basename() + "." + extension;
		if (candidate != p_subtitle_path && ResourceLoader::exists(candidate, "AudioStream")) {
			return candidate;
		}
	}
	return String();
}

bool SubtitlesDock::can_edit(const Object *p_object) {
	// Not for every audio file with a sidecar subtitle: browsing voice lines must not take over the bottom panel.
	return Object::cast_to<Subtitle>(p_object) != nullptr;
}

// Timeline geometry

real_t SubtitlesDock::_get_ruler_height() {
	return 18 * EDSCALE;
}

real_t SubtitlesDock::_get_lane_height() {
	return 44 * EDSCALE;
}

double SubtitlesDock::_get_offset() const {
	return subtitle.is_valid() ? subtitle->get_time_offset() : 0.0;
}

double SubtitlesDock::_get_length() const {
	if (stream.is_valid() && stream->get_length() > 0.0) {
		return stream->get_length();
	}
	// Without a stream: the segments and some room to spot new ones.
	const double end = subtitle.is_valid() && subtitle->get_segment_count() > 0 ? subtitle->get_end_time() : 0.0;
	return MAX(30.0, end + 5.0);
}

double SubtitlesDock::_x_to_time(double p_x) const {
	return view_start + p_x / pixels_per_second;
}

double SubtitlesDock::_time_to_x(double p_time) const {
	return (p_time - view_start) * pixels_per_second;
}

Rect2 SubtitlesDock::_get_segment_rect(int p_index) const {
	const Ref<SubtitleSegment> segment = subtitle->get_segment(p_index);
	if (segment.is_null()) {
		return Rect2();
	}
	const double from = _time_to_x(segment->get_start_time() + _get_offset());
	const double to = _time_to_x(segment->get_end_time() + _get_offset());
	const real_t lane_top = timeline->get_size().height - _get_lane_height();
	return Rect2(from, lane_top + 2 * EDSCALE, MAX(to - from, 2.0), _get_lane_height() - 4 * EDSCALE);
}

int SubtitlesDock::_get_segment_at(const Point2 &p_pos, DragMode &r_mode) const {
	r_mode = DRAG_SEEK;
	if (subtitle.is_null() || p_pos.y < timeline->get_size().height - _get_lane_height()) {
		return -1;
	}
	const real_t edge = 5 * EDSCALE;
	// The selected segment first, so its edges win over a neighbor's.
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < subtitle->get_segment_count(); i++) {
			if ((pass == 0) != (i == selected)) {
				continue;
			}
			const Rect2 rect = _get_segment_rect(i);
			if (p_pos.x < rect.position.x - edge || p_pos.x > rect.get_end().x + edge) {
				continue;
			}
			if (Math::abs(p_pos.x - rect.position.x) <= edge) {
				r_mode = DRAG_START;
			} else if (Math::abs(p_pos.x - rect.get_end().x) <= edge) {
				r_mode = DRAG_END;
			} else {
				r_mode = DRAG_MOVE;
			}
			return i;
		}
	}
	return -1;
}

// Updates

void SubtitlesDock::_queue_update() {
	if (update_queued) {
		return;
	}
	update_queued = true;
	callable_mp(this, &SubtitlesDock::_update).call_deferred();
}

void SubtitlesDock::_subtitle_changed() {
	if (drag_mode == DRAG_NONE || drag_mode == DRAG_SEEK) {
		_queue_update();
	} else {
		timeline->queue_redraw(); // While dragging, the table follows on release.
	}
}

void SubtitlesDock::_segment_changed(int p_index) {
	changed_rows.insert(p_index);
}

void SubtitlesDock::_watch_segments(bool p_watch) {
	for (int i = 0; i < shown_segments.size(); i++) {
		const Ref<SubtitleSegment> &segment = shown_segments[i];
		const Callable callback = callable_mp(this, &SubtitlesDock::_segment_changed).bind(i);
		if (segment.is_null()) {
			continue;
		}
		if (p_watch) {
			segment->connect_changed(callback);
		} else if (segment->is_connected(CoreStringName(changed), callback)) {
			segment->disconnect_changed(callback);
		}
	}
}

void SubtitlesDock::_update_file_label() {
	String file = TTR("No subtitle");
	if (subtitle.is_valid()) {
		file = subtitle->get_path().is_resource_file() ? subtitle->get_path().get_file() : TTR("Built-in Subtitle");
		// Set by undo/redo operations on the subtitle, cleared when it is saved (also by Godot's Save).
		if (subtitle->is_edited()) {
			file += " (*)";
		}
	}
	file_label->set_text(file);
	file_label->set_tooltip_text(subtitle.is_valid() ? subtitle->get_path() : String());
}

void SubtitlesDock::_update() {
	update_queued = false;
	const bool has_subtitle = subtitle.is_valid();
	if (has_subtitle && selected >= subtitle->get_segment_count()) {
		selected = subtitle->get_segment_count() - 1;
	}
	_update_tree(); // Also updates the problems.
	_update_file_label();
	for (Button *button : { add_button, spot_start_button, play_button, save_button }) {
		button->set_disabled(!has_subtitle);
	}
	remove_button->set_disabled(!has_subtitle || selected < 0);
	spot_end_button->set_disabled(!has_subtitle || selected < 0);
	stream_picker->set_editable(has_subtitle);
	stream_label->set_text(stream.is_valid() ? (stream->get_path().is_resource_file() ? stream->get_path().get_file() : TTR("Built-in Stream")) : TTR("No audio"));
	stream_label->set_tooltip_text(stream.is_valid() ? stream->get_path() : String());

	String status;
	String save_tooltip = TTR("Save the subtitle file in its own format.");
	if (has_subtitle) {
		const int count = subtitle->get_segment_count();
		status = vformat(TTRN("%d segment", "%d segments", count), count);
		if (!problems.is_empty()) {
			status += ", " + vformat(TTRN("%d with a problem (highlighted, see the tooltip)", "%d with problems (highlighted, see the tooltips)", problems.size()), problems.size());
		}
		if (stream.is_null()) {
			status += ". " + TTR("Pick an audio stream to see its waveform and hear it; without one, Play runs a clock.");
		}
		const String extension = subtitle->get_path().get_extension().to_lower();
		if (subtitle->get_time_offset() != 0.0 && Subtitle::get_recognized_extensions().has(extension)) {
			save_tooltip += "\n" + vformat(TTR("Saving as .%s adds the time offset to the written times, because the format has no offset field."), extension);
		}
	} else {
		status = TTR("Open a Subtitle from the FileSystem dock, or an audio stream with subtitles from its Inspector.");
	}
	status_label->set_text(status);
	save_button->set_tooltip_text(save_tooltip);
	_update_scroll();
	_update_time_label();
	timeline->queue_redraw();
}

void SubtitlesDock::_fill_row(TreeItem *p_item, int p_index, const String &p_kinds) {
	const Ref<SubtitleSegment> segment = subtitle->get_segment(p_index);
	p_item->set_metadata(0, p_index);
	p_item->set_text(COLUMN_INDEX, itos(p_index + 1));
	if (segment.is_null()) {
		p_item->set_text(COLUMN_TEXT, TTR("<empty slot>"));
		return;
	}
	const double offset = _get_offset();
	p_item->set_text(COLUMN_START, SubtitleEditorHelper::format_short_time(segment->get_start_time() + offset));
	p_item->set_text(COLUMN_END, SubtitleEditorHelper::format_short_time(segment->get_end_time() + offset));
	p_item->set_text(COLUMN_SPEAKER, segment->get_speaker());
	if (p_item->get_cell_mode(COLUMN_KIND) != TreeItem::CELL_MODE_RANGE) {
		p_item->set_cell_mode(COLUMN_KIND, TreeItem::CELL_MODE_RANGE);
		p_item->set_text(COLUMN_KIND, p_kinds);
		p_item->set_edit_multiline(COLUMN_TEXT, true);
		for (int column = COLUMN_START; column < COLUMN_MAX; column++) {
			p_item->set_editable(column, true);
		}
	}
	p_item->set_range(COLUMN_KIND, segment->get_kind());
	p_item->set_text(COLUMN_TEXT, segment->get_text());
	const bool has_problem = problems.has(p_index);
	const String tooltip = has_problem ? String("\n").join(problems.get(p_index)) : String();
	const Color warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	for (int column = 0; column < COLUMN_MAX; column++) {
		if (has_problem) {
			p_item->set_custom_color(column, warning_color);
		} else if (p_item->get_custom_color(column) != Color()) {
			p_item->clear_custom_color(column);
		}
		p_item->set_tooltip_text(column, tooltip);
	}
}

void SubtitlesDock::_update_tree() {
	updating_tree = true;
	const HashMap<int, PackedStringArray> old_problems(problems);
	problems = subtitle.is_valid() ? SubtitleEditorHelper::get_problems(subtitle) : HashMap<int, PackedStringArray>();
	String kinds;
	for (int kind = 0; kind < SubtitleSegment::KIND_MAX; kind++) {
		kinds += (kind > 0 ? "," : "") + SubtitleEditorHelper::get_kind_name(SubtitleSegment::Kind(kind));
	}
	TreeItem *root = tree->get_root();
	const bool same_segments = subtitle.is_valid() && root != nullptr && shown_segments == subtitle->get_segment_list();
	if (same_segments) {
		// Only segments that changed, and rows whose problems changed: spotting stays fast with thousands of rows.
		for (const KeyValue<int, PackedStringArray> &problem : old_problems) {
			if (!problems.has(problem.key) || problems.get(problem.key) != problem.value) {
				changed_rows.insert(problem.key);
			}
		}
		for (const KeyValue<int, PackedStringArray> &problem : problems) {
			if (!old_problems.has(problem.key)) {
				changed_rows.insert(problem.key);
			}
		}
		for (int index : changed_rows) {
			if (index >= 0 && index < root->get_child_count()) {
				_fill_row(root->get_child(index), index, kinds);
			}
		}
	} else {
		_watch_segments(false);
		shown_segments.clear();
		if (subtitle.is_null()) {
			tree->clear();
		} else {
			// Rows are reused: cells whose text stays the same are not touched (Tree skips equal texts).
			shown_segments = subtitle->get_segment_list();
			_watch_segments(true);
			if (root == nullptr) {
				root = tree->create_item();
			}
			while (root->get_child_count() > shown_segments.size()) {
				TreeItem *last = root->get_child(root->get_child_count() - 1);
				root->remove_child(last);
				memdelete(last);
			}
			while (root->get_child_count() < shown_segments.size()) {
				tree->create_item(root);
			}
			for (int i = 0; i < shown_segments.size(); i++) {
				_fill_row(root->get_child(i), i, kinds);
			}
		}
	}
	changed_rows.clear();
	root = tree->get_root();
	TreeItem *item = root && selected >= 0 && selected < root->get_child_count() ? root->get_child(selected) : nullptr;
	if (item && !item->is_selected(0)) {
		item->select(0);
	}
	updating_tree = false;
}

void SubtitlesDock::_update_scroll() {
	if (fit_pending && timeline->get_size().width > 0) {
		fit_pending = false;
		pixels_per_second = CLAMP(timeline->get_size().width / _get_length(), 2.0 * EDSCALE, 100.0 * EDSCALE);
	}
	const double visible_seconds = timeline->get_size().width / pixels_per_second;
	const double length = _get_length();
	view_start = CLAMP(view_start, 0.0, MAX(0.0, length - visible_seconds));
	timeline_scroll->set_max(length);
	timeline_scroll->set_page(MIN(visible_seconds, length));
	timeline_scroll->set_value_no_signal(view_start);
}

void SubtitlesDock::_update_time_label() {
	time_label->set_text(SubtitleEditorHelper::format_short_time(playhead) + " / " + SubtitleEditorHelper::format_short_time(_get_length()));
}

void SubtitlesDock::_select(int p_index, bool p_scroll_tree) {
	selected = p_index;
	remove_button->set_disabled(selected < 0);
	spot_end_button->set_disabled(selected < 0);
	if (p_scroll_tree) {
		TreeItem *root = tree->get_root();
		TreeItem *item = root && selected >= 0 && selected < root->get_child_count() ? root->get_child(selected) : nullptr;
		updating_tree = true;
		if (item) {
			item->select(0);
			tree->scroll_to_item(item);
		} else {
			tree->deselect_all();
		}
		updating_tree = false;
	}
	timeline->queue_redraw();
}

// Edits

void SubtitlesDock::_commit_segments(const String &p_action, const TypedArray<SubtitleSegment> &p_segments, int p_select) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, UndoRedo::MERGE_DISABLE, subtitle.ptr());
	undo_redo->add_do_property(subtitle.ptr(), "segments", p_segments);
	undo_redo->add_undo_property(subtitle.ptr(), "segments", subtitle->get_segments());
	selected = p_select;
	undo_redo->commit_action();
}

void SubtitlesDock::_commit_times(const String &p_action, int p_index, double p_start, double p_end, double p_old_start, double p_old_end) {
	const Ref<SubtitleSegment> segment = subtitle->get_segment(p_index);
	ERR_FAIL_COND(segment.is_null());
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, UndoRedo::MERGE_DISABLE, subtitle.ptr());
	// Segments without a path count as built into the edited scene: keep the action in the history of the subtitle.
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(segment.ptr(), "set_start_time", p_start);
	undo_redo->add_do_method(segment.ptr(), "set_end_time", p_end);
	undo_redo->add_undo_method(segment.ptr(), "set_start_time", p_old_start);
	undo_redo->add_undo_method(segment.ptr(), "set_end_time", p_old_end);
	// Marks the subtitle as edited, so Godot's Save writes it too.
	undo_redo->add_do_method(subtitle.ptr(), "emit_changed");
	undo_redo->add_undo_method(subtitle.ptr(), "emit_changed");
	undo_redo->commit_action();
}

void SubtitlesDock::_add_segment() {
	ERR_FAIL_COND(subtitle.is_null());
	const double start = MAX(0.0, playhead - _get_offset());
	_commit_segments(TTR("Add Subtitle Segment"), SubtitleEditorHelper::get_segments_with_segment_at(subtitle, start, start + NEW_DURATION), SubtitleEditorHelper::get_insert_index(subtitle, start));
}

void SubtitlesDock::_remove_segment() {
	if (subtitle.is_null() || selected < 0 || selected >= subtitle->get_segment_count()) {
		return;
	}
	TypedArray<SubtitleSegment> segments = subtitle->get_segments();
	segments.remove_at(selected);
	_commit_segments(TTR("Remove Subtitle Segment"), segments, MIN(selected, segments.size() - 1));
}

void SubtitlesDock::_spot(bool p_end) {
	if (subtitle.is_null()) {
		return;
	}
	const double time = MAX(0.0, playhead - _get_offset());
	if (selected < 0 || selected >= subtitle->get_segment_count()) {
		if (!p_end) {
			_add_segment(); // Spotting from scratch: a new segment starts at the playhead.
		}
		return;
	}
	const Ref<SubtitleSegment> segment = subtitle->get_segment(selected);
	ERR_FAIL_COND(segment.is_null());
	const double start = segment->get_start_time();
	const double end = segment->get_end_time();
	if (!p_end) {
		_commit_times(TTR("Set Subtitle Segment Start"), selected, time, end > time ? end : time + NEW_DURATION, start, end);
		return;
	}
	if (time <= start) {
		status_label->set_text(TTR("The playhead is before the start of the selected segment."));
		return;
	}
	_commit_times(TTR("Set Subtitle Segment End"), selected, start, time, start, end);
	// The next segment is the one to spot next.
	if (selected + 1 < subtitle->get_segment_count()) {
		_select(selected + 1);
	}
}

void SubtitlesDock::_save() {
	if (subtitle.is_null()) {
		return;
	}
	if (!subtitle->get_path().is_resource_file()) {
		status_label->set_text(TTR("This subtitle is built into another resource or scene: it is saved with it."));
		return;
	}
	EditorNode::get_singleton()->save_resource(subtitle);
}

// Playback

void SubtitlesDock::_play_pause() {
	if (subtitle.is_null()) {
		return;
	}
	playing = !playing;
	if (playing) {
		if (playhead >= _get_length()) {
			playhead = 0.0;
		}
		if (stream.is_valid()) {
			player->play(playhead);
		}
	} else {
		player->stop();
	}
	play_button->set_button_icon(get_editor_theme_icon(playing ? SNAME("Pause") : SNAME("MainPlay")));
	set_process(playing);
}

void SubtitlesDock::_seek(double p_time) {
	playhead = CLAMP(p_time, 0.0, _get_length());
	if (playing && stream.is_valid()) {
		player->seek(playhead);
	}
	_update_time_label();
	timeline->queue_redraw();
}

void SubtitlesDock::_player_finished() {
	if (playing) {
		playhead = _get_length();
		_play_pause();
		_update_time_label();
		timeline->queue_redraw();
	}
}

void SubtitlesDock::_set_stream(const Ref<AudioStream> &p_stream) {
	if (playing) {
		_play_pause();
	}
	stream = p_stream;
	player->set_stream(stream);
	stream_picker->set_edited_resource(stream);
	playhead = MIN(playhead, _get_length());
	_queue_update();
}

void SubtitlesDock::_stream_picked(const Ref<Resource> &p_stream) {
	_set_stream(p_stream);
}

void SubtitlesDock::_preview_updated(ObjectID p_id) {
	if (stream.is_valid() && stream->get_instance_id() == p_id) {
		timeline->queue_redraw();
	}
}

// Timeline

void SubtitlesDock::_timeline_draw() {
	const Size2 size = timeline->get_size();
	const Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	const int font_size = int(get_theme_font_size(SceneStringName(font_size), SNAME("Label")) * 0.85);
	const Color font_color = get_theme_color(SceneStringName(font_color), SNAME("Label"));
	const Color accent_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	const Color warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	const real_t ruler = _get_ruler_height();
	const real_t lane_top = size.height - _get_lane_height();
	const double length = _get_length();

	timeline->draw_rect(Rect2(Point2(), size), get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
	timeline->draw_rect(Rect2(0, lane_top, size.width, _get_lane_height()), get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor)));
	const real_t end_x = _time_to_x(length);
	if (end_x < size.width) {
		timeline->draw_rect(Rect2(end_x, 0, size.width - end_x, size.height), get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));
	}

	// Waveform between ruler and segment lane.
	if (stream.is_valid() && stream->get_length() > 0.0) {
		const Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
		const float preview_length = preview.is_valid() ? preview->get_length() : 0.0f;
		const real_t wave_height = lane_top - ruler;
		Vector<Vector2> points;
		for (int x = 0; x < int(MIN(size.width, end_x)); x++) {
			const float from = float(_x_to_time(x));
			const float to = float(_x_to_time(x + 1));
			if (from >= preview_length) {
				break;
			}
			const float max = preview->get_max(from, to) * 0.5f + 0.5f;
			const float min = preview->get_min(from, to) * 0.5f + 0.5f;
			points.push_back(Vector2(x + 0.5, ruler + (1.0f - max) * wave_height));
			points.push_back(Vector2(x + 0.5, ruler + (1.0f - min) * wave_height + 1.0));
		}
		if (!points.is_empty()) {
			timeline->draw_multiline(points, get_theme_color(SNAME("contrast_color_2"), EditorStringName(Editor)));
		}
	}

	// Ruler: a step that keeps labels apart.
	static const double steps[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800 };
	double step = steps[std_size(steps) - 1];
	for (double candidate : steps) {
		if (candidate * pixels_per_second >= 80 * EDSCALE) {
			step = candidate;
			break;
		}
	}
	const Color tick_color = font_color * Color(1, 1, 1, 0.35);
	for (double t = Math::floor(view_start / step) * step; t <= _x_to_time(size.width); t += step) {
		const real_t x = _time_to_x(t);
		timeline->draw_line(Point2(x, 0), Point2(x, ruler), tick_color, Math::round(EDSCALE));
		timeline->draw_string(font, Point2(x + 3 * EDSCALE, ruler - 4 * EDSCALE), SubtitleEditorHelper::format_short_time(t), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color * Color(1, 1, 1, 0.7));
	}

	// Segments: sorted by start, so stop after the first one past the right edge.
	if (subtitle.is_valid()) {
		const real_t padding = 4 * EDSCALE;
		for (int i = 0; i < subtitle->get_segment_count(); i++) {
			const Rect2 rect = _get_segment_rect(i);
			if (rect.position.x > size.width) {
				break;
			}
			if (rect.size.width <= 0 || rect.get_end().x < 0) {
				continue;
			}
			const bool is_selected = i == selected;
			const Color base = problems.has(i) ? warning_color : accent_color;
			timeline->draw_rect(rect, base * Color(1, 1, 1, is_selected ? 0.55 : 0.25));
			timeline->draw_rect(rect, is_selected ? font_color : base, false, Math::round(EDSCALE * (is_selected ? 2 : 1)));
			const Ref<SubtitleSegment> segment = subtitle->get_segment(i);
			const real_t text_width = rect.size.width - padding * 2;
			if (segment.is_valid() && text_width > 8 * EDSCALE) {
				const Point2 text_pos = rect.position + Point2(padding, (rect.size.height + font->get_ascent(font_size) - font->get_descent(font_size)) * 0.5);
				timeline->draw_string(font, text_pos, SubtitleEditorHelper::get_row_text(subtitle, segment), HORIZONTAL_ALIGNMENT_LEFT, text_width, font_size, font_color);
			}
		}
	}

	// Playhead.
	const real_t playhead_x = Math::round(_time_to_x(playhead));
	timeline->draw_line(Point2(playhead_x, 0), Point2(playhead_x, size.height), accent_color, Math::round(2 * EDSCALE));
	const Ref<Texture2D> indicator = get_editor_theme_icon(SNAME("TimelineIndicator"));
	timeline->draw_texture(indicator, Point2(playhead_x - indicator->get_width() * 0.5, 0), accent_color);
}

void SubtitlesDock::_cancel_drag() {
	const DragMode mode = drag_mode;
	drag_mode = DRAG_NONE;
	const Ref<SubtitleSegment> segment = subtitle.is_valid() && selected >= 0 ? subtitle->get_segment(selected) : Ref<SubtitleSegment>();
	if (mode >= DRAG_MOVE && segment.is_valid()) {
		segment->set_start_time(drag_old_start);
		segment->set_end_time(drag_old_end);
	}
}

void SubtitlesDock::_timeline_gui_input(const Ref<InputEvent> &p_event) {
	if (subtitle.is_null()) {
		return;
	}
	const Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::ESCAPE && drag_mode != DRAG_NONE) {
		_cancel_drag();
		timeline->accept_event();
		return;
	}
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MouseButton::WHEEL_UP || mb->get_button_index() == MouseButton::WHEEL_DOWN)) {
		const bool up = mb->get_button_index() == MouseButton::WHEEL_UP;
		if (mb->is_command_or_control_pressed()) {
			// Zoom around the mouse.
			const double time = _x_to_time(mb->get_position().x);
			pixels_per_second = CLAMP(pixels_per_second * (up ? 1.25 : 0.8), 2.0 * EDSCALE, 2000.0 * EDSCALE);
			view_start = time - mb->get_position().x / pixels_per_second;
		} else {
			view_start += (up ? -0.1 : 0.1) * timeline->get_size().width / pixels_per_second;
		}
		_update_scroll();
		timeline->queue_redraw();
		timeline->accept_event();
		return;
	}
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			timeline->grab_focus();
			DragMode mode;
			const int index = _get_segment_at(mb->get_position(), mode);
			drag_mode = mode;
			drag_from_time = _x_to_time(mb->get_position().x);
			if (index >= 0) {
				_select(index);
				const Ref<SubtitleSegment> segment = subtitle->get_segment(index);
				drag_old_start = segment->get_start_time();
				drag_old_end = segment->get_end_time();
				if (mb->is_double_click()) {
					drag_mode = DRAG_NONE;
					EditorInterface::get_singleton()->edit_resource(segment);
				}
			} else {
				_seek(drag_from_time);
			}
		} else if (drag_mode != DRAG_NONE) {
			const DragMode mode = drag_mode;
			drag_mode = DRAG_NONE;
			const Ref<SubtitleSegment> segment = selected >= 0 ? subtitle->get_segment(selected) : Ref<SubtitleSegment>();
			if (mode >= DRAG_MOVE && segment.is_valid()) {
				const double start = segment->get_start_time();
				const double end = segment->get_end_time();
				if (start != drag_old_start || end != drag_old_end) {
					static const char *const actions[] = { "", "", TTRC("Move Subtitle Segment"), TTRC("Trim Subtitle Segment Start"), TTRC("Trim Subtitle Segment End") };
					_commit_times(TTR(actions[mode]), selected, start, end, drag_old_start, drag_old_end);
				}
			}
		}
		timeline->accept_event();
		return;
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_null()) {
		return;
	}
	if (drag_mode == DRAG_NONE) {
		DragMode mode;
		_get_segment_at(mm->get_position(), mode);
		timeline->set_default_cursor_shape(mode == DRAG_START || mode == DRAG_END ? CURSOR_HSIZE : (mode == DRAG_MOVE ? CURSOR_MOVE : CURSOR_ARROW));
		return;
	}
	const double time = _x_to_time(mm->get_position().x);
	if (drag_mode == DRAG_SEEK) {
		_seek(time);
		return;
	}
	const Ref<SubtitleSegment> segment = selected >= 0 ? subtitle->get_segment(selected) : Ref<SubtitleSegment>();
	if (segment.is_null()) {
		return;
	}
	// Times in segment time, snapped to milliseconds like the subtitle formats, kept within the stream.
	const double length = _get_length() - _get_offset();
	double delta = Math::round((time - drag_from_time) * 1000.0) / 1000.0;
	switch (drag_mode) {
		case DRAG_MOVE: {
			delta = CLAMP(delta, -drag_old_start, MAX(0.0, length - drag_old_end));
			segment->set_start_time(drag_old_start + delta);
			segment->set_end_time(drag_old_end + delta);
		} break;
		case DRAG_START: {
			segment->set_start_time(MAX(0.0, MIN(drag_old_start + delta, drag_old_end - MIN_DURATION)));
		} break;
		case DRAG_END: {
			segment->set_end_time(MIN(MAX(drag_old_end + delta, drag_old_start + MIN_DURATION), MAX(length, drag_old_end)));
		} break;
		default:
			break;
	}
}

void SubtitlesDock::_scroll_changed(double p_value) {
	view_start = p_value;
	timeline->queue_redraw();
}

// Table

void SubtitlesDock::_tree_selected() {
	if (updating_tree) {
		return;
	}
	TreeItem *item = tree->get_selected();
	if (item == nullptr) {
		return;
	}
	_select(item->get_metadata(0), false);
	// Show the segment on the timeline and move the playhead to it, ready to play from there.
	const Ref<SubtitleSegment> segment = subtitle->get_segment(selected);
	if (segment.is_valid()) {
		const double start = segment->get_start_time() + _get_offset();
		const double visible_seconds = timeline->get_size().width / pixels_per_second;
		if (start < view_start || start > view_start + visible_seconds * 0.9) {
			view_start = start - visible_seconds * 0.1;
			_update_scroll();
		}
		_seek(start);
	}
}

void SubtitlesDock::_tree_edited() {
	TreeItem *item = tree->get_edited();
	if (item == nullptr || subtitle.is_null()) {
		return;
	}
	const int column = tree->get_edited_column();
	const int index = item->get_metadata(0);
	const Ref<SubtitleSegment> segment = subtitle->get_segment(index);
	ERR_FAIL_COND(segment.is_null());
	if (column == COLUMN_START || column == COLUMN_END) {
		double time = 0.0;
		if (!SubtitleEditorHelper::parse_short_time(item->get_text(column), time)) {
			status_label->set_text(TTR("Enter a time as seconds (5.5), minutes:seconds (0:05.500) or hours:minutes:seconds."));
			changed_rows.insert(index);
			_queue_update();
			return;
		}
		time = MAX(0.0, time - _get_offset());
		const double start = segment->get_start_time();
		const double end = segment->get_end_time();
		if (column == COLUMN_START) {
			_commit_times(TTR("Set Subtitle Segment Start"), index, time, end, start, end);
		} else {
			_commit_times(TTR("Set Subtitle Segment End"), index, start, time, start, end);
		}
		return;
	}
	StringName property;
	Variant value;
	switch (column) {
		case COLUMN_SPEAKER: {
			property = SNAME("speaker");
			value = StringName(item->get_text(column).strip_edges());
		} break;
		case COLUMN_KIND: {
			property = SNAME("kind");
			value = int(item->get_range(column));
		} break;
		case COLUMN_TEXT: {
			property = SNAME("text");
			value = item->get_text(column);
		} break;
		default:
			return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Edit Subtitle Segment"), UndoRedo::MERGE_DISABLE, subtitle.ptr());
	undo_redo->force_fixed_history(); // The segment, without a path, would count as built into the edited scene.
	undo_redo->add_do_property(segment.ptr(), property, value);
	undo_redo->add_undo_property(segment.ptr(), property, segment->get(property));
	undo_redo->add_do_method(subtitle.ptr(), "emit_changed");
	undo_redo->add_undo_method(subtitle.ptr(), "emit_changed");
	undo_redo->commit_action();
}

void SubtitlesDock::_tree_activated() {
	TreeItem *item = tree->get_selected();
	if (item == nullptr || subtitle.is_null()) {
		return;
	}
	// The # column is not edited in place: all properties of the segment in the Inspector.
	if (tree->get_selected_column() == COLUMN_INDEX) {
		const Ref<SubtitleSegment> segment = subtitle->get_segment(item->get_metadata(0));
		if (segment.is_valid()) {
			EditorInterface::get_singleton()->edit_resource(segment);
		}
	}
}

void SubtitlesDock::_file_removed(const String &p_file) {
	if (subtitle.is_valid() && subtitle->get_path() == p_file) {
		// Save would write the deleted file again.
		edit(Ref<Resource>());
		close();
	}
}

// Shortcuts

void SubtitlesDock::input(const Ref<InputEvent> &p_event) {
	// Before the focused control: the table and the buttons would take Space and Delete themselves.
	const Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed() || subtitle.is_null() || !is_visible_in_tree() || drag_mode != DRAG_NONE) {
		return;
	}
	const Control *focus = get_viewport()->gui_get_focus_owner();
	if (focus == nullptr || !is_ancestor_of(focus) || Object::cast_to<LineEdit>(focus) || Object::cast_to<TextEdit>(focus) || tree->is_editing()) {
		return;
	}
	if (ED_IS_SHORTCUT("subtitles_dock/play_pause", p_event)) {
		_play_pause();
	} else if (ED_IS_SHORTCUT("subtitles_dock/spot_start", p_event)) {
		_spot(false);
	} else if (ED_IS_SHORTCUT("subtitles_dock/spot_end", p_event)) {
		_spot(true);
	} else if (ED_IS_SHORTCUT("subtitles_dock/back", p_event)) {
		_seek(playhead - 1.0);
	} else if (ED_IS_SHORTCUT("subtitles_dock/forward", p_event)) {
		_seek(playhead + 1.0);
	} else if (ED_IS_SHORTCUT("subtitles_dock/add_segment", p_event)) {
		_add_segment();
	} else if (ED_IS_SHORTCUT("subtitles_dock/remove_segment", p_event)) {
		_remove_segment();
	} else {
		return;
	}
	get_viewport()->set_input_as_handled();
}

void SubtitlesDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			AudioStreamPreviewGenerator::get_singleton()->connect(SNAME("preview_updated"), callable_mp(this, &SubtitlesDock::_preview_updated));
			EditorUndoRedoManager::get_singleton()->connect(SNAME("history_changed"), callable_mp(this, &SubtitlesDock::_update_file_label));
			EditorNode::get_singleton()->connect(SNAME("resource_saved"), callable_mp(this, &SubtitlesDock::_update_file_label).unbind(1));
			if (FileSystemDock::get_singleton()) {
				FileSystemDock::get_singleton()->connect(SNAME("file_removed"), callable_mp(this, &SubtitlesDock::_file_removed));
				// A moved subtitle keeps its resource with the new path: only the label changes.
				FileSystemDock::get_singleton()->connect(SNAME("files_moved"), callable_mp(this, &SubtitlesDock::_queue_update).unbind(2));
			}
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			play_button->set_button_icon(get_editor_theme_icon(playing ? SNAME("Pause") : SNAME("MainPlay")));
			add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			remove_button->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			save_button->set_button_icon(get_editor_theme_icon(SNAME("Save")));
			time_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts)));
			shown_segments.clear(); // Rebuild the rows with the new colors.
			_queue_update();
		} break;
		case NOTIFICATION_PROCESS: {
			if (!playing) {
				break;
			}
			if (stream.is_valid() && player->is_playing()) {
				playhead = player->get_playback_position();
			} else if (stream.is_null()) {
				playhead += get_process_delta_time(); // No stream: a clock, to spot segments against video or speech elsewhere.
				if (playhead >= _get_length()) {
					_player_finished();
				}
			}
			// Follow the playhead.
			const double visible_seconds = timeline->get_size().width / pixels_per_second;
			if (playhead > view_start + visible_seconds * 0.9 || playhead < view_start) {
				view_start = playhead - visible_seconds * 0.1;
				_update_scroll();
			}
			_update_time_label();
			timeline->queue_redraw();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				if (playing) {
					_play_pause();
				}
				_cancel_drag();
			}
		} break;
	}
}

void SubtitlesDock::edit(const Ref<Resource> &p_resource) {
	Ref<Subtitle> new_subtitle = Ref<Subtitle>(Object::cast_to<Subtitle>(p_resource.ptr()));
	Ref<AudioStream> new_stream = Ref<AudioStream>(Object::cast_to<AudioStream>(p_resource.ptr()));
	if (new_stream.is_valid()) {
		const String path = get_stream_subtitle_path(new_stream.ptr());
		new_subtitle = path.is_empty() ? Ref<Subtitle>() : Ref<Subtitle>(ResourceLoader::load(path, "Subtitle"));
	}
	if (new_subtitle.is_valid() && new_subtitle == subtitle) {
		// The same subtitle again (back from inspecting a segment, or the Inspector button): keep the view.
		if (new_stream.is_valid() && new_stream != stream) {
			_set_stream(new_stream);
		}
		return;
	}
	if (playing) {
		_play_pause();
	}
	const Callable callback = callable_mp(this, &SubtitlesDock::_subtitle_changed);
	if (subtitle.is_valid() && subtitle->is_connected(CoreStringName(changed), callback)) {
		subtitle->disconnect_changed(callback);
	}
	if (new_stream.is_null() && new_subtitle.is_valid()) {
		const String sidecar = find_sidecar_stream(new_subtitle->get_path());
		if (!sidecar.is_empty()) {
			new_stream = ResourceLoader::load(sidecar, "AudioStream");
		}
	}
	subtitle = new_subtitle;
	if (subtitle.is_valid()) {
		subtitle->connect_changed(callback);
	}
	selected = -1;
	playhead = 0.0;
	view_start = 0.0;
	fit_pending = true;
	_set_stream(new_stream);
}

// Toolbar buttons: flat like Godot's other bottom panels, no keyboard focus (Space and Delete stay with the dock),
// the live shortcut in the tooltip.
static Button *_subtitles_dock_make_button(Control *p_context, const String &p_text, const String &p_tooltip, const String &p_shortcut) {
	Button *button = memnew(Button(p_text));
	button->set_theme_type_variation(SceneStringName(FlatButton));
	button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	button->set_tooltip_text(p_tooltip);
	if (!p_shortcut.is_empty()) {
		button->set_shortcut(ED_GET_SHORTCUT(p_shortcut));
		button->set_shortcut_context(p_context);
		button->set_shortcut_in_tooltip(true);
	}
	return button;
}

SubtitlesDock::SubtitlesDock() {
	singleton = this;
	set_name(TTRC("Subtitles"));
	set_icon_name("Subtitle");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_subtitles_bottom_panel", TTRC("Toggle Subtitles Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);
	set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	set_process_input(true);
	pixels_per_second = 100.0 * EDSCALE;

	ED_SHORTCUT_ARRAY("subtitles_dock/play_pause", TTRC("Play/Pause"), { int32_t(Key::SPACE), int32_t(Key::K) });
	ED_SHORTCUT("subtitles_dock/spot_start", TTRC("Set Segment Start at Playhead"), Key::I);
	ED_SHORTCUT("subtitles_dock/spot_end", TTRC("Set Segment End at Playhead"), Key::O);
	ED_SHORTCUT("subtitles_dock/back", TTRC("Back 1 Second"), Key::J);
	ED_SHORTCUT("subtitles_dock/forward", TTRC("Forward 1 Second"), Key::L);
	ED_SHORTCUT("subtitles_dock/add_segment", TTRC("Add Segment at Playhead"), Key::N);
	ED_SHORTCUT("subtitles_dock/remove_segment", TTRC("Remove Segment"), Key::KEY_DELETE);
	ED_SHORTCUT_OVERRIDE("subtitles_dock/remove_segment", "macos", KeyModifierMask::META | Key::BACKSPACE);

	player = memnew(AudioStreamPlayer);
	player->connect(SceneStringName(finished), callable_mp(this, &SubtitlesDock::_player_finished));
	add_child(player);

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	main_vbox->add_child(toolbar);
	file_label = memnew(Label);
	file_label->set_mouse_filter(MOUSE_FILTER_PASS);
	toolbar->add_child(file_label);
	toolbar->add_child(memnew(VSeparator));

	stream_picker = memnew(EditorResourcePicker);
	stream_picker->set_base_type("AudioStream");
	stream_picker->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
	stream_picker->set_tooltip_text(TTR("The audio for the waveform and playback. Found as an audio file with the subtitle's name, or the stream the dock was opened for; a stream picked here is not saved."));
	stream_picker->connect("resource_changed", callable_mp(this, &SubtitlesDock::_stream_picked));
	toolbar->add_child(stream_picker);
	stream_label = memnew(Label);
	stream_label->set_mouse_filter(MOUSE_FILTER_PASS);
	toolbar->add_child(stream_label);
	play_button = _subtitles_dock_make_button(this, String(), TTR("Play or pause from the playhead. The Back 1 Second and Forward 1 Second shortcuts move the playhead."), "subtitles_dock/play_pause");
	play_button->set_accessibility_name(TTRC("Play/Pause"));
	play_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_play_pause));
	toolbar->add_child(play_button);
	time_label = memnew(Label);
	toolbar->add_child(time_label);
	toolbar->add_child(memnew(VSeparator));

	add_button = _subtitles_dock_make_button(this, TTR("Add Segment"), TTR("Add a two-second segment at the playhead."), "subtitles_dock/add_segment");
	add_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_add_segment));
	toolbar->add_child(add_button);
	remove_button = _subtitles_dock_make_button(this, TTR("Remove Segment"), TTR("Remove the selected segment."), "subtitles_dock/remove_segment");
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_remove_segment));
	toolbar->add_child(remove_button);
	spot_start_button = _subtitles_dock_make_button(this, TTR("Set Start"), TTR("Set the start of the selected segment at the playhead. Without a selected segment, add one there."), "subtitles_dock/spot_start");
	spot_start_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_spot).bind(false));
	toolbar->add_child(spot_start_button);
	spot_end_button = _subtitles_dock_make_button(this, TTR("Set End"), TTR("Set the end of the selected segment at the playhead and select the next segment."), "subtitles_dock/spot_end");
	spot_end_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_spot).bind(true));
	toolbar->add_child(spot_end_button);
	toolbar->add_child(memnew(VSeparator));
	save_button = _subtitles_dock_make_button(this, TTR("Save"), String(), String());
	save_button->connect(SceneStringName(pressed), callable_mp(this, &SubtitlesDock::_save));
	toolbar->add_child(save_button);

	status_label = memnew(Label);
	status_label->set_h_size_flags(SIZE_EXPAND_FILL);
	status_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	status_label->set_mouse_filter(MOUSE_FILTER_PASS);
	toolbar->add_child(status_label);

	VSplitContainer *split = memnew(VSplitContainer);
	split->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_child(split);

	VBoxContainer *timeline_vbox = memnew(VBoxContainer);
	timeline_vbox->add_theme_constant_override("separation", 0);
	split->add_child(timeline_vbox);
	timeline = memnew(Control);
	timeline->set_custom_minimum_size(Size2(0, 130) * EDSCALE);
	timeline->set_v_size_flags(SIZE_EXPAND_FILL);
	timeline->set_focus_mode(FOCUS_ALL);
	timeline->set_clip_contents(true);
	timeline->set_accessibility_name(TTRC("Subtitle Timeline"));
	timeline->set_tooltip_text(TTR("Click to move the playhead. Drag a segment to move it, its edges to trim it (Escape cancels); double-click a segment to inspect it. Ctrl+Wheel zooms."));
	timeline->connect(SceneStringName(draw), callable_mp(this, &SubtitlesDock::_timeline_draw));
	timeline->connect(SceneStringName(gui_input), callable_mp(this, &SubtitlesDock::_timeline_gui_input));
	timeline->connect(SceneStringName(resized), callable_mp(this, &SubtitlesDock::_update_scroll));
	timeline_vbox->add_child(timeline);
	timeline_scroll = memnew(HScrollBar);
	timeline_scroll->connect(SceneStringName(value_changed), callable_mp(this, &SubtitlesDock::_scroll_changed));
	timeline_vbox->add_child(timeline_scroll);

	tree = memnew(Tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
	tree->set_columns(COLUMN_MAX);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_ROW);
	tree->set_column_titles_visible(true);
	static const char *const titles[] = { "#", TTRC("Start"), TTRC("End"), TTRC("Speaker"), TTRC("Kind"), TTRC("Text") };
	static const int widths[] = { 50, 100, 100, 120, 120, 0 };
	for (int column = 0; column < COLUMN_MAX; column++) {
		tree->set_column_title(column, column == COLUMN_INDEX ? String(titles[column]) : TTR(titles[column]));
		if (widths[column] > 0) {
			tree->set_column_expand(column, false);
			tree->set_column_custom_minimum_width(column, widths[column] * EDSCALE);
		}
	}
	tree->set_tooltip_text(TTR("Click a cell of a selected row to edit it. Double-click # to inspect all properties of the segment."));
	tree->connect(SceneStringName(item_selected), callable_mp(this, &SubtitlesDock::_tree_selected));
	tree->connect("item_edited", callable_mp(this, &SubtitlesDock::_tree_edited));
	// Deferred: inspecting the segment rebuilds editors while the tree emits.
	tree->connect("item_activated", callable_mp(this, &SubtitlesDock::_tree_activated), CONNECT_DEFERRED);
	split->add_child(tree);
}

SubtitlesDock::~SubtitlesDock() {
	_watch_segments(false);
	if (singleton == this) {
		singleton = nullptr;
	}
}
