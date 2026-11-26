/**************************************************************************/
/*  subtitle_player.h                                                     */
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

#include "scene/main/node.h"
#include "servers/subtitle/subtitle_playback.h"

// A thin scene front end for `SubtitlePlayback`: it follows an audio, video or animation player
// (or its own clock), honors `process_mode` and pause, and forwards the segment signals.
class SubtitlePlayer : public Node {
	GDCLASS(SubtitlePlayer, Node);

	Ref<SubtitlePlayback> playback;
	NodePath sync_node;
	double time_offset = 0.0;
	bool autoplay = false;

	double position = 0.0;
	bool playing = false;

	ObjectID sync_id;
	StringName sync_position_method; // Empty when the sync node is not supported.

	Node *_get_sync_node();
	void _advance(double p_delta);
	void _segment_changed(const Ref<SubtitleSegment> &p_segment);
	void _segment_started(const Ref<SubtitleSegment> &p_segment);
	void _segment_ended(const Ref<SubtitleSegment> &p_segment);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_subtitle(const Ref<Subtitle> &p_subtitle);
	Ref<Subtitle> get_subtitle() const;
	void set_sync_node(const NodePath &p_path);
	NodePath get_sync_node() const;
	void set_time_offset(double p_offset);
	double get_time_offset() const;
	void set_autoplay(bool p_enabled);
	bool is_autoplay_enabled() const;

	void play(double p_from_position = 0.0);
	void stop();
	void seek(double p_position);
	bool is_playing() const;
	double get_playback_position() const;
	Ref<SubtitlePlayback> get_playback() const;

	SubtitlePlayer();
};
