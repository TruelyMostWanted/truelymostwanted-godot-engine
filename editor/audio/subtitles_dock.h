/**************************************************************************/
/*  subtitles_dock.h                                                      */
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

#include "editor/docks/editor_dock.h"
#include "servers/subtitle/subtitle.h"

class AudioStream;
class AudioStreamPlayer;
class Button;
class EditorResourcePicker;
class HScrollBar;
class Label;
class Tree;

// Edits the cues of a `Subtitle` in a bottom dock: a table of cues and a timeline with the waveform of the
// audio stream, where cues are moved and trimmed with the mouse and spotted at the playhead with the keyboard.
// Opened for a Subtitle resource or an AudioStream with a subtitle file; the Inspector table is the quick path.
class SubtitlesDock : public EditorDock {
	GDCLASS(SubtitlesDock, EditorDock);

	enum DragMode {
		DRAG_NONE,
		DRAG_SEEK,
		DRAG_MOVE,
		DRAG_START,
		DRAG_END,
	};

	static inline SubtitlesDock *singleton = nullptr;

	Ref<Subtitle> subtitle;
	Ref<AudioStream> stream;
	int selected = -1;
	bool dirty = false;
	bool update_queued = false;
	bool updating_tree = false;

	// Timeline view, in stream seconds (segment times plus the time offset).
	double view_start = 0.0;
	double pixels_per_second = 100.0;
	bool fit_pending = false; // Zoom out to the whole stream once the timeline has a size.
	double playhead = 0.0;
	bool playing = false;

	DragMode drag_mode = DRAG_NONE;
	double drag_from_time = 0.0;
	double drag_old_start = 0.0;
	double drag_old_end = 0.0;

	Label *file_label = nullptr;
	EditorResourcePicker *stream_picker = nullptr;
	Button *play_button = nullptr;
	Label *time_label = nullptr;
	Button *add_button = nullptr;
	Button *remove_button = nullptr;
	Button *spot_start_button = nullptr;
	Button *spot_end_button = nullptr;
	Button *save_button = nullptr;
	Label *status_label = nullptr;
	Control *timeline = nullptr;
	HScrollBar *timeline_scroll = nullptr;
	Tree *tree = nullptr;
	AudioStreamPlayer *player = nullptr;

	double _get_offset() const;
	double _get_length() const;
	double _x_to_time(double p_x) const;
	double _time_to_x(double p_time) const;
	Rect2 _get_cue_rect(int p_index) const;
	int _get_cue_at(const Point2 &p_pos, DragMode &r_mode) const;

	void _queue_update();
	void _update();
	void _update_tree();
	void _update_scroll();
	void _update_time_label();
	void _subtitle_changed();
	void _select(int p_index, bool p_scroll_tree = true);

	void _commit_segments(const String &p_action, const TypedArray<SubtitleSegment> &p_segments, int p_select);
	void _commit_times(const String &p_action, int p_index, double p_start, double p_end, double p_old_start, double p_old_end);
	void _add_cue();
	void _remove_cue();
	void _spot(bool p_end);
	void _save();

	void _play_pause();
	void _seek(double p_time);
	void _player_finished();
	void _stream_picked(const Ref<Resource> &p_stream);
	void _preview_updated(ObjectID p_id);

	void _timeline_draw();
	void _timeline_gui_input(const Ref<InputEvent> &p_event);
	void _scroll_changed(double p_value);
	void _tree_selected();
	void _tree_edited();
	void _tree_activated();

protected:
	void _notification(int p_what);
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

public:
	static SubtitlesDock *get_singleton() { return singleton; }
	// A Subtitle, or an AudioStream with a `subtitle_path` or a sidecar subtitle file.
	static bool can_edit(const Object *p_object);
	// The subtitle file of `p_stream`: its `subtitle_path`, else an .srt or .vtt next to the stream file.
	static String get_stream_subtitle_path(const AudioStream *p_stream);
	// An audio file with the base name of `p_subtitle_path` (voice.srt -> voice.wav, .ogg, ...), or empty.
	static String find_sidecar_stream(const String &p_subtitle_path);

	// Edits a Subtitle (finding a sidecar audio stream) or an AudioStream (loading its subtitle file).
	void edit(const Ref<Resource> &p_resource);
	Ref<Subtitle> get_subtitle() const { return subtitle; }

	SubtitlesDock();
	~SubtitlesDock();
};
