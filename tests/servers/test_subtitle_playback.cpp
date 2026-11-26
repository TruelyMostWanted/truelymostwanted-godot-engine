/**************************************************************************/
/*  test_subtitle_playback.cpp                                            */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_subtitle_playback)

#include "core/object/callable_mp.h"
#include "servers/subtitle/subtitle_playback.h"

namespace TestSubtitlePlayback {

// Writes every segment signal as one word, in order: "+A" started, "-A" ended, "=A" changed ("=" alone for none).
class SegmentLog : public Object {
public:
	Vector<String> events;

	static String text_of(const Ref<SubtitleSegment> &p_segment) { return p_segment.is_valid() ? p_segment->get_text() : String(); }
	void on_changed(const Ref<SubtitleSegment> &p_segment) { events.push_back("=" + text_of(p_segment)); }
	void on_started(const Ref<SubtitleSegment> &p_segment) { events.push_back("+" + text_of(p_segment)); }
	void on_ended(const Ref<SubtitleSegment> &p_segment) { events.push_back("-" + text_of(p_segment)); }

	void connect_to(Object *p_source) {
		p_source->connect("segment_changed", callable_mp(this, &SegmentLog::on_changed));
		p_source->connect("segment_started", callable_mp(this, &SegmentLog::on_started));
		p_source->connect("segment_ended", callable_mp(this, &SegmentLog::on_ended));
	}

	String take() {
		const String result = String(" ").join(events);
		events.clear();
		return result;
	}
};

static Ref<SubtitleSegment> make_segment(double p_start, double p_end, const String &p_text) {
	Ref<SubtitleSegment> segment;
	segment.instantiate();
	segment->set_start_time(p_start);
	segment->set_end_time(p_end);
	segment->set_text(p_text);
	return segment;
}

// 0-1 "A", 2-4 "B", 3-5 "C" (overlaps B).
static Ref<Subtitle> make_subtitle() {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	subtitle->add_segment(make_segment(0.0, 1.0, "A"));
	subtitle->add_segment(make_segment(2.0, 4.0, "B"));
	subtitle->add_segment(make_segment(3.0, 5.0, "C"));
	return subtitle;
}

TEST_CASE("[SubtitlePlayback] Update: signals in order, overlaps and the current segment") {
	Ref<SubtitlePlayback> playback;
	playback.instantiate();
	SegmentLog *log = memnew(SegmentLog);
	log->connect_to(playback.ptr());
	playback->set_subtitle(make_subtitle());

	playback->update(0.5);
	CHECK(log->take() == "+A =A");
	CHECK(playback->get_current_segment()->get_text() == "A");

	playback->update(0.9);
	CHECK(log->take().is_empty());

	playback->update(1.0); // End times are exclusive.
	CHECK(log->take() == "-A =");
	CHECK(playback->get_current_segment().is_null());

	playback->update(3.5);
	CHECK(log->take() == "+B +C =C");
	CHECK(playback->get_active_segments().size() == 2);
	CHECK(playback->get_time() == doctest::Approx(3.5));

	playback->update(4.5);
	CHECK(log->take() == "-B");
	CHECK(playback->get_current_segment()->get_text() == "C");

	playback->update(6.0);
	CHECK(log->take() == "-C =");

	memdelete(log);
}

TEST_CASE("[SubtitlePlayback] Update reports skipped segments, seek does not") {
	Ref<SubtitlePlayback> playback;
	playback.instantiate();
	SegmentLog *log = memnew(SegmentLog);
	log->connect_to(playback.ptr());
	playback->set_subtitle(make_subtitle());

	playback->update(0.5);
	log->take();
	playback->update(10.0); // A long frame jumps over B and C.
	CHECK(log->take() == "-A +B -B +C -C =");

	playback->seek(0.5); // Backwards.
	CHECK(log->take() == "+A =A");
	playback->seek(10.0);
	CHECK(log->take() == "-A =");

	playback->seek(2.5);
	CHECK(log->take() == "+B =B");
	playback->update(1.5); // Backwards with update: no skipped segments.
	CHECK(log->take() == "-B =");

	memdelete(log);
}

TEST_CASE("[SubtitlePlayback] Long segments, time offset, changes and reset") {
	Ref<SubtitlePlayback> playback;
	playback.instantiate();
	SegmentLog *log = memnew(SegmentLog);
	log->connect_to(playback.ptr());
	Ref<Subtitle> subtitle = make_subtitle();
	playback->set_subtitle(subtitle);

	subtitle->add_segment(make_segment(1.0, 10.0, "L")); // Runs under B and C.
	playback->update(4.5);
	CHECK(log->take() == "+L +C =C");

	subtitle->set_time_offset(10.0); // Segment times move to 10-20.
	playback->update(10.5);
	CHECK(log->take() == "-L -C +A =A");

	playback->reset();
	CHECK(log->take() == "-A =");
	CHECK(playback->get_active_segments().is_empty());

	playback->update(10.5);
	CHECK(log->take() == "+A =A");
	playback->set_subtitle(Ref<Subtitle>());
	CHECK(log->take() == "-A =");
	playback->update(10.5);
	CHECK(log->take().is_empty());

	memdelete(log);
}

TEST_CASE("[SubtitlePlayback] Unsorted segments") {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	TypedArray<SubtitleSegment> segments;
	segments.push_back(make_segment(3.0, 4.0, "late"));
	segments.push_back(make_segment(0.0, 1.0, "early"));
	subtitle->set_segments(segments);

	Ref<SubtitlePlayback> playback;
	playback.instantiate();
	playback->set_subtitle(subtitle);
	playback->update(0.5);
	CHECK(playback->get_current_segment()->get_text() == "early");
	playback->update(3.5);
	CHECK(playback->get_current_segment()->get_text() == "late");
}

} // namespace TestSubtitlePlayback
