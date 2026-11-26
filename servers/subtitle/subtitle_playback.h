/**************************************************************************/
/*  subtitle_playback.h                                                   */
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

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "servers/subtitle/subtitle.h"

// Plays a `Subtitle` against a time that the caller provides and signals when segments start and end.
// It needs no scene and no threads: `SubtitlePlayer` drives it every frame; tools and tests call it directly.
class SubtitlePlayback : public RefCounted {
	GDCLASS(SubtitlePlayback, RefCounted);

	struct Entry {
		double start = 0.0;
		double end = 0.0;
		double max_end = 0.0; // The latest end time of this entry and all entries before it.
		int order = 0;
		Ref<SubtitleSegment> segment;

		bool operator<(const Entry &p_other) const {
			if (start != p_other.start) {
				return start < p_other.start;
			}
			if (end != p_other.end) {
				return end < p_other.end;
			}
			return order < p_other.order;
		}
	};

	Ref<Subtitle> subtitle;
	LocalVector<Entry> entries; // Sorted by start time, rebuilt after the subtitle changed.
	bool entries_dirty = true;
	LocalVector<Ref<SubtitleSegment>> active; // Sorted by start time.
	LocalVector<Ref<SubtitleSegment>> next_active; // Reused by every update, so updates do not allocate.
	double time = 0.0;
	bool has_time = false;

	void _subtitle_changed();
	void _rebuild_entries();
	// The number of entries that start at or before `p_local_time`.
	uint32_t _count_started(double p_local_time) const;
	void _apply(double p_time, bool p_advance);

protected:
	static void _bind_methods();

public:
	void set_subtitle(const Ref<Subtitle> &p_subtitle);
	Ref<Subtitle> get_subtitle() const;

	void update(double p_time);
	void seek(double p_time);
	void reset();

	double get_time() const;
	TypedArray<SubtitleSegment> get_active_segments() const;
	Ref<SubtitleSegment> get_current_segment() const;
};
