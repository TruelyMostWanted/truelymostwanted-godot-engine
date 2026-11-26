/**************************************************************************/
/*  subtitle_playback.cpp                                                 */
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

#include "subtitle_playback.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void SubtitlePlayback::_subtitle_changed() {
	entries_dirty = true;
}

void SubtitlePlayback::_rebuild_entries() {
	entries_dirty = false;
	entries.clear();
	if (subtitle.is_null()) {
		return;
	}
	const Vector<Ref<SubtitleSegment>> &list = subtitle->get_segment_list();
	for (int i = 0; i < list.size(); i++) {
		if (list[i].is_null()) {
			continue;
		}
		Entry entry;
		entry.start = list[i]->get_start_time();
		entry.end = list[i]->get_end_time();
		entry.order = i;
		entry.segment = list[i];
		entries.push_back(entry);
	}
	entries.sort();
	double max_end = -Math::INF;
	for (Entry &entry : entries) {
		max_end = MAX(max_end, entry.end);
		entry.max_end = max_end;
	}
}

uint32_t SubtitlePlayback::_count_started(double p_local_time) const {
	uint32_t low = 0;
	uint32_t high = entries.size();
	while (low < high) {
		const uint32_t middle = (low + high) / 2;
		if (entries[middle].start <= p_local_time) {
			low = middle + 1;
		} else {
			high = middle;
		}
	}
	return low;
}

void SubtitlePlayback::_apply(double p_time, bool p_advance) {
	if (entries_dirty) {
		_rebuild_entries();
	}
	const double offset = subtitle.is_valid() ? subtitle->get_time_offset() : 0.0;
	const double local_time = p_time - offset;
	const uint32_t started_count = _count_started(local_time);

	// Active segments contain the time. Walk back from the last started one while an earlier one can still be running.
	next_active.clear();
	for (uint32_t i = started_count; i > 0 && entries[i - 1].max_end > local_time; i--) {
		if (entries[i - 1].end > local_time) {
			next_active.push_back(entries[i - 1].segment);
		}
	}
	next_active.reverse();

	// Moving forward, segments that started after the previous time and are already over were skipped by a long frame.
	uint32_t skipped_from = started_count;
	if (p_advance && has_time && p_time > time) {
		skipped_from = _count_started(time - offset);
	}
	time = p_time;
	has_time = true;

	bool unchanged = active.size() == next_active.size();
	for (uint32_t i = 0; unchanged && i < active.size(); i++) {
		unchanged = active[i] == next_active[i];
	}
	Vector<Ref<SubtitleSegment>> skipped;
	for (uint32_t i = skipped_from; i < started_count; i++) {
		if (entries[i].end <= local_time) {
			skipped.push_back(entries[i].segment);
		}
	}
	if (unchanged && skipped.is_empty()) {
		return;
	}

	// Signal handlers may call back into this playback: work on copies from here on.
	const Ref<SubtitleSegment> old_current = active.is_empty() ? Ref<SubtitleSegment>() : active[active.size() - 1];
	Vector<Ref<SubtitleSegment>> ended;
	for (const Ref<SubtitleSegment> &segment : active) {
		if (next_active.find(segment) < 0) {
			ended.push_back(segment);
		}
	}
	Vector<Ref<SubtitleSegment>> started;
	for (const Ref<SubtitleSegment> &segment : next_active) {
		if (active.find(segment) < 0) {
			started.push_back(segment);
		}
	}
	active.clear();
	for (const Ref<SubtitleSegment> &segment : next_active) {
		active.push_back(segment);
	}
	const Ref<SubtitleSegment> new_current = active.is_empty() ? Ref<SubtitleSegment>() : active[active.size() - 1];

	for (const Ref<SubtitleSegment> &segment : ended) {
		emit_signal(SNAME("segment_ended"), segment);
	}
	for (const Ref<SubtitleSegment> &segment : skipped) {
		emit_signal(SNAME("segment_started"), segment);
		emit_signal(SNAME("segment_ended"), segment);
	}
	for (const Ref<SubtitleSegment> &segment : started) {
		emit_signal(SNAME("segment_started"), segment);
	}
	if (new_current != old_current) {
		emit_signal(SNAME("segment_changed"), new_current);
	}
}

void SubtitlePlayback::set_subtitle(const Ref<Subtitle> &p_subtitle) {
	if (subtitle == p_subtitle) {
		return;
	}
	reset();
	if (subtitle.is_valid()) {
		subtitle->disconnect_changed(callable_mp(this, &SubtitlePlayback::_subtitle_changed));
	}
	subtitle = p_subtitle;
	if (subtitle.is_valid()) {
		subtitle->connect_changed(callable_mp(this, &SubtitlePlayback::_subtitle_changed));
	}
	entries_dirty = true;
}

Ref<Subtitle> SubtitlePlayback::get_subtitle() const {
	return subtitle;
}

void SubtitlePlayback::update(double p_time) {
	_apply(p_time, true);
}

void SubtitlePlayback::seek(double p_time) {
	_apply(p_time, false);
}

void SubtitlePlayback::reset() {
	time = 0.0;
	has_time = false;
	if (active.is_empty()) {
		return;
	}
	Vector<Ref<SubtitleSegment>> ended;
	for (const Ref<SubtitleSegment> &segment : active) {
		ended.push_back(segment);
	}
	active.clear();
	for (const Ref<SubtitleSegment> &segment : ended) {
		emit_signal(SNAME("segment_ended"), segment);
	}
	emit_signal(SNAME("segment_changed"), Ref<SubtitleSegment>());
}

double SubtitlePlayback::get_time() const {
	return time;
}

TypedArray<SubtitleSegment> SubtitlePlayback::get_active_segments() const {
	TypedArray<SubtitleSegment> result;
	for (const Ref<SubtitleSegment> &segment : active) {
		result.push_back(segment);
	}
	return result;
}

Ref<SubtitleSegment> SubtitlePlayback::get_current_segment() const {
	return active.is_empty() ? Ref<SubtitleSegment>() : active[active.size() - 1];
}

void SubtitlePlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_subtitle", "subtitle"), &SubtitlePlayback::set_subtitle);
	ClassDB::bind_method(D_METHOD("get_subtitle"), &SubtitlePlayback::get_subtitle);
	ClassDB::bind_method(D_METHOD("update", "time"), &SubtitlePlayback::update);
	ClassDB::bind_method(D_METHOD("seek", "time"), &SubtitlePlayback::seek);
	ClassDB::bind_method(D_METHOD("reset"), &SubtitlePlayback::reset);
	ClassDB::bind_method(D_METHOD("get_time"), &SubtitlePlayback::get_time);
	ClassDB::bind_method(D_METHOD("get_active_segments"), &SubtitlePlayback::get_active_segments);
	ClassDB::bind_method(D_METHOD("get_current_segment"), &SubtitlePlayback::get_current_segment);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "subtitle", PROPERTY_HINT_RESOURCE_TYPE, "Subtitle"), "set_subtitle", "get_subtitle");

	ADD_SIGNAL(MethodInfo("segment_changed", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
	ADD_SIGNAL(MethodInfo("segment_started", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
	ADD_SIGNAL(MethodInfo("segment_ended", PropertyInfo(Variant::OBJECT, "segment", PROPERTY_HINT_RESOURCE_TYPE, "SubtitleSegment")));
}
