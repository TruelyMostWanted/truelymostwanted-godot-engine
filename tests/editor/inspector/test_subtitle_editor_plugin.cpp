/**************************************************************************/
/*  test_subtitle_editor_plugin.cpp                                       */
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

TEST_FORCE_LINK(test_subtitle_editor_plugin)

#ifdef TOOLS_ENABLED

#include "editor/inspector/subtitle_editor_plugin.h"

namespace TestSubtitleEditorPlugin {

static Ref<SubtitleSegment> make_segment(double p_start, double p_end) {
	Ref<SubtitleSegment> segment;
	segment.instantiate();
	segment->set_start_time(p_start);
	segment->set_end_time(p_end);
	segment->set_text("x");
	return segment;
}

TEST_CASE("[SubtitleEditorPlugin] Suggested time offset for files from video editors") {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	CHECK(SubtitleEditorHelper::get_suggested_time_offset(subtitle) == 0.0);
	subtitle->add_segment(make_segment(5.0, 6.0));
	CHECK(SubtitleEditorHelper::get_suggested_time_offset(subtitle) == 0.0);

	subtitle->clear_segments();
	subtitle->add_segment(make_segment(3600.016, 3605.5));
	CHECK(SubtitleEditorHelper::get_suggested_time_offset(subtitle) == -3600.0);
	subtitle->set_time_offset(-3600.0); // The suggestion is based on the segment times, not on the offset.
	CHECK(SubtitleEditorHelper::get_suggested_time_offset(subtitle) == -3600.0);

	subtitle->clear_segments();
	subtitle->add_segment(make_segment(7300.0, 7301.0));
	CHECK(SubtitleEditorHelper::get_suggested_time_offset(subtitle) == -7200.0);
}

TEST_CASE("[SubtitleEditorPlugin] Sorted segments and a new segment, without changing the subtitle") {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	TypedArray<SubtitleSegment> unsorted;
	unsorted.push_back(make_segment(4.0, 5.0));
	unsorted.push_back(make_segment(1.0, 2.0));
	subtitle->set_segments(unsorted);

	const TypedArray<SubtitleSegment> sorted = SubtitleEditorHelper::get_sorted_segments(subtitle);
	CHECK(Ref<SubtitleSegment>(sorted[0])->get_start_time() == 1.0);
	CHECK(subtitle->get_segment(0)->get_start_time() == 4.0);

	const TypedArray<SubtitleSegment> added = SubtitleEditorHelper::get_segments_with_new_segment(subtitle);
	REQUIRE(added.size() == 3);
	CHECK(Ref<SubtitleSegment>(added[2])->get_start_time() == 5.0);
	CHECK(Ref<SubtitleSegment>(added[2])->get_end_time() == 7.0);
	CHECK(subtitle->get_segment_count() == 2);

	CHECK(SubtitleEditorHelper::get_kind_name(SubtitleSegment::KIND_SOUND) == "Sound");
}

} // namespace TestSubtitleEditorPlugin

#endif // TOOLS_ENABLED
