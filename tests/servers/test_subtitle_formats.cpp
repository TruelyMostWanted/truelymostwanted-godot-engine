/**************************************************************************/
/*  test_subtitle_formats.cpp                                             */
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

TEST_FORCE_LINK(test_subtitle_formats)

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "servers/subtitle/subtitle.h"
#include "servers/subtitle/subtitle_formats.h"
#include "tests/test_utils.h"

namespace TestSubtitleFormats {

static const char *VTT_SAMPLE =
		"WEBVTT - exported\n\n"
		"NOTE A comment\nover two lines\n\n"
		"STYLE\n::cue { color: yellow }\n\n"
		"intro-1\n00:00:01.000 --> 00:00:03.500 align:center line:90%\n<v Roger Bingham>We are in New York City\n&amp; it is cold\n\n"
		"00:04.000 --> 00:05.000 line:0\n[Applause]\n\n"
		"00:00:06.000 --> 00:00:07.000\n<i>Far away</i>\n\n"
		"00:00:08.000 --> 00:00:09.000\n<c.yellow>Colored</c> &lt;text&gt;\n";

static const char *TTML_SAMPLE =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\" ttp:frameRate=\"25\" xml:lang=\"de\">\n"
		"  <body><div>\n"
		"    <p begin=\"00:00:01.000\" end=\"00:00:02.500\">Hallo<br/>\n      Welt</p>\n"
		"    <p begin=\"00:00:03:00\" end=\"00:00:04:12\">[Applaus]</p>\n"
		"    <p begin=\"5s\" dur=\"1500ms\"><span tts:fontStyle=\"italic\">Leise</span></p>\n"
		"    <p begin=\"175f\" end=\"200f\">WACHE: Halt &amp; stehen bleiben</p>\n"
		"    <p begin=\"broken\" end=\"1s\">skipped</p>\n"
		"  </div></body>\n</tt>\n";

static const char *ASS_SAMPLE =
		"[Script Info]\nTitle: Test\nScriptType: v4.00+\n\n"
		"[V4+ Styles]\nFormat: Name, Fontname, Fontsize\nStyle: Default,Arial,20\n\n"
		"[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Comment: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,ignored\n"
		"Dialogue: 0,0:00:01.00,0:00:02.50,Default,Guard,0,0,0,,Who is there,\\Nfriend?\n"
		"Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,{\\an8}[Door creaks]\n"
		"Dialogue: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,,{\\i1}Far away{\\i0}\n"
		"Dialogue: 0,0:00:07.00,0:00:08.00,Default,,0,0,0,,{\\fad(200,200)\\c&H00FFFF&}Styled\\hspace\n";

static const char *SSA_SAMPLE =
		"[Script Info]\nScriptType: v4.00\n\n"
		"[Events]\nFormat: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
		"Dialogue: Marked=0,0:00:01.00,0:00:02.00,Default,Narrator,0000,0000,0000,,Once upon a time\n";

static const char *SBV_SAMPLE =
		"0:00:01.000,0:00:02.500\nHello\nworld\n\n"
		"0:00:03.000,0:00:04.000\n[Laughs]\n\n"
		"broken,line\nskipped\n";

TEST_CASE("[Subtitle][Formats] Extensions") {
	CHECK(Subtitle::get_format_for_extension("srt") == Subtitle::FORMAT_SRT);
	CHECK(Subtitle::get_format_for_extension("VTT") == Subtitle::FORMAT_VTT);
	CHECK(Subtitle::get_format_for_extension("ttml") == Subtitle::FORMAT_TTML);
	CHECK(Subtitle::get_format_for_extension("dfxp") == Subtitle::FORMAT_TTML);
	CHECK(Subtitle::get_format_for_extension("ass") == Subtitle::FORMAT_ASS);
	CHECK(Subtitle::get_format_for_extension("ssa") == Subtitle::FORMAT_ASS);
	CHECK(Subtitle::get_format_for_extension("sbv") == Subtitle::FORMAT_SBV);
	CHECK(Subtitle::get_format_for_extension("txt") == Subtitle::FORMAT_MAX);
}

TEST_CASE("[Subtitle][Formats] WebVTT") {
	Vector<Ref<SubtitleSegment>> segments;
	int skipped = 0;
	REQUIRE(SubtitleFormatVTT::parse(VTT_SAMPLE, segments, &skipped) == OK);
	CHECK(skipped == 0);
	REQUIRE(segments.size() == 4);
	CHECK(segments[0]->get_start_time() == doctest::Approx(1.0));
	CHECK(segments[0]->get_end_time() == doctest::Approx(3.5));
	CHECK(segments[0]->get_speaker() == StringName("Roger Bingham"));
	CHECK(segments[0]->get_text() == "We are in New York City\n& it is cold");
	CHECK(segments[0]->get_placement() == SubtitleSegment::PLACEMENT_AUTO);
	CHECK(segments[1]->get_start_time() == doctest::Approx(4.0));
	CHECK(segments[1]->get_kind() == SubtitleSegment::KIND_SOUND);
	CHECK(segments[1]->get_placement() == SubtitleSegment::PLACEMENT_TOP);
	CHECK(segments[2]->is_off_screen());
	CHECK(segments[3]->get_text() == "Colored <text>");
	CHECK(SubtitleFormatVTT::format_time(3723.45) == "01:02:03.450");
}

TEST_CASE("[Subtitle][Formats] TTML with frames, offsets and duration") {
	Vector<Ref<SubtitleSegment>> segments;
	int skipped = 0;
	String language;
	REQUIRE(SubtitleFormatTTML::parse(TTML_SAMPLE, segments, &skipped, &language) == OK);
	CHECK(language == "de");
	CHECK(skipped == 1);
	REQUIRE(segments.size() == 4);
	CHECK(segments[0]->get_text() == "Hallo\nWelt");
	CHECK(segments[1]->get_kind() == SubtitleSegment::KIND_SOUND);
	CHECK(segments[1]->get_start_time() == doctest::Approx(3.0));
	CHECK(segments[1]->get_end_time() == doctest::Approx(4.48));
	CHECK(segments[2]->get_start_time() == doctest::Approx(5.0));
	CHECK(segments[2]->get_end_time() == doctest::Approx(6.5));
	CHECK(segments[2]->is_off_screen());
	CHECK(segments[3]->get_start_time() == doctest::Approx(7.0));
	CHECK(segments[3]->get_end_time() == doctest::Approx(8.0));
	CHECK(segments[3]->get_speaker() == StringName("WACHE"));
	CHECK(segments[3]->get_text() == "Halt & stehen bleiben");

	double seconds = 0.0;
	CHECK(SubtitleFormatTTML::parse_time("2m", 30, 1, seconds));
	CHECK(seconds == doctest::Approx(120.0));
	CHECK(SubtitleFormatTTML::parse_time("10000000t", 30, 10000000, seconds));
	CHECK(seconds == doctest::Approx(1.0));
	CHECK_FALSE(SubtitleFormatTTML::parse_time("soon", 30, 1, seconds));
	CHECK(SubtitleFormatTTML::parse("<html></html>", segments) == ERR_PARSE_ERROR);
}

TEST_CASE("[Subtitle][Formats] Advanced SubStation Alpha and SubStation Alpha") {
	Vector<Ref<SubtitleSegment>> segments;
	REQUIRE(SubtitleFormatASS::parse(ASS_SAMPLE, segments) == OK);
	REQUIRE(segments.size() == 4);
	CHECK(segments[0]->get_start_time() == doctest::Approx(1.0));
	CHECK(segments[0]->get_end_time() == doctest::Approx(2.5));
	CHECK(segments[0]->get_speaker() == StringName("Guard"));
	CHECK(segments[0]->get_text() == "Who is there,\nfriend?");
	CHECK(segments[1]->get_kind() == SubtitleSegment::KIND_SOUND);
	CHECK(segments[1]->get_placement() == SubtitleSegment::PLACEMENT_TOP);
	CHECK(segments[1]->get_text() == "Door creaks");
	CHECK(segments[2]->is_off_screen());
	CHECK(segments[3]->get_text() == "Styled space");

	REQUIRE(SubtitleFormatASS::parse(SSA_SAMPLE, segments) == OK);
	REQUIRE(segments.size() == 1);
	CHECK(segments[0]->get_speaker() == StringName("Narrator"));
	CHECK(segments[0]->get_text() == "Once upon a time");
	CHECK(SubtitleFormatASS::format_time(3723.456) == "1:02:03.46");
	CHECK(SubtitleFormatASS::parse("just text", segments) == ERR_PARSE_ERROR);
}

TEST_CASE("[Subtitle][Formats] YouTube SBV") {
	Vector<Ref<SubtitleSegment>> segments;
	int skipped = 0;
	REQUIRE(SubtitleFormatSBV::parse(SBV_SAMPLE, segments, &skipped) == OK);
	CHECK(skipped == 1);
	REQUIRE(segments.size() == 2);
	CHECK(segments[0]->get_text() == "Hello\nworld");
	CHECK(segments[1]->get_kind() == SubtitleSegment::KIND_SOUND);
	CHECK(SubtitleFormatSBV::format_time(3723.45) == "1:02:03.450");
}

static Ref<Subtitle> make_rich_subtitle() {
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	subtitle->set_language("de");
	struct Row {
		double start;
		double end;
		const char *text;
		SubtitleSegment::Kind kind;
		const char *speaker;
		bool off_screen;
		bool top;
	};
	const Row rows[] = {
		{ 1.0, 2.5, "Who is there?\nHello?", SubtitleSegment::KIND_DIALOG, "GUARD", false, false },
		{ 3.0, 4.0, "Applause", SubtitleSegment::KIND_SOUND, "", false, true },
		{ 5.0, 6.25, "calm music", SubtitleSegment::KIND_MUSIC, "", false, false },
		{ 7.0, 8.0, "A voice & an echo", SubtitleSegment::KIND_DIALOG, "", true, false },
	};
	for (const Row &row : rows) {
		Ref<SubtitleSegment> segment;
		segment.instantiate();
		segment->set_start_time(row.start);
		segment->set_end_time(row.end);
		segment->set_text(row.text);
		segment->set_kind(row.kind);
		segment->set_speaker(row.speaker);
		segment->set_off_screen(row.off_screen);
		segment->set_placement(row.top ? SubtitleSegment::PLACEMENT_TOP : SubtitleSegment::PLACEMENT_AUTO);
		subtitle->add_segment(segment);
	}
	return subtitle;
}

TEST_CASE("[Subtitle][Formats] Round trip through every format and through ResourceSaver/ResourceLoader") {
	const Ref<Subtitle> source = make_rich_subtitle();
	struct Case {
		const char *extension;
		bool keeps_italics;
		bool keeps_top;
	};
	const Case cases[] = { { "srt", true, true }, { "vtt", true, true }, { "ttml", true, false }, { "dfxp", true, false }, { "ass", true, true }, { "sbv", false, false } };
	for (const Case &format_case : cases) {
		INFO("format ", format_case.extension);
		const String path = TestUtils::get_temp_path(String("subtitle_round_trip.") + format_case.extension);
		REQUIRE(ResourceSaver::save(source, path) == OK);
		Ref<Subtitle> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(loaded.is_valid());
		REQUIRE(loaded->get_segment_count() == 4);
		for (int i = 0; i < 4; i++) {
			const Ref<SubtitleSegment> a = source->get_segment(i);
			const Ref<SubtitleSegment> b = loaded->get_segment(i);
			CHECK(b->get_start_time() == doctest::Approx(a->get_start_time()));
			CHECK(b->get_end_time() == doctest::Approx(a->get_end_time()));
			CHECK(b->get_text() == a->get_text());
			CHECK(b->get_kind() == (a->get_kind() == SubtitleSegment::KIND_DIALOG ? SubtitleSegment::KIND_DIALOG : (a->get_kind() == SubtitleSegment::KIND_SOUND ? SubtitleSegment::KIND_SOUND : SubtitleSegment::KIND_MUSIC)));
			CHECK(b->get_speaker() == a->get_speaker());
			if (format_case.keeps_italics) {
				CHECK(b->is_off_screen() == a->is_off_screen());
			}
			if (format_case.keeps_top) {
				CHECK(b->get_placement() == a->get_placement());
			}
		}
		if (String(format_case.extension) == "ttml" || String(format_case.extension) == "dfxp") {
			CHECK(loaded->get_language() == "de");
		}
	}

	Ref<Subtitle> unknown;
	unknown.instantiate();
	ERR_PRINT_OFF;
	CHECK(source->save_to_file(TestUtils::get_temp_path("subtitle.txt")) == ERR_FILE_UNRECOGNIZED);
	CHECK(unknown->load_from_file(TestUtils::get_temp_path("subtitle.txt")) == ERR_FILE_UNRECOGNIZED);
	ERR_PRINT_ON;
}

} // namespace TestSubtitleFormats
