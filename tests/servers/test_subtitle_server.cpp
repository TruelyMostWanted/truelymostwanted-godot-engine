/**************************************************************************/
/*  test_subtitle_server.cpp                                              */
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

TEST_FORCE_LINK(test_subtitle_server)

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/string/translation.h"
#include "core/string/translation_domain.h"
#include "core/string/translation_server.h"
#include "servers/subtitle/subtitle_server.h"

namespace TestSubtitleServer {

// Writes every signal of the server as one line, in order.
class SignalLog : public Object {
public:
	Vector<String> events;

	void on_track_started(int p_id, const Ref<Subtitle> &p_subtitle) { events.push_back(vformat("track_started %d", p_id)); }
	void on_segment_started(int p_id, const Ref<SubtitleSegment> &p_segment, const String &p_text) { events.push_back(vformat("segment_started %d %s", p_id, p_text)); }
	void on_segment_ended(int p_id, const Ref<SubtitleSegment> &p_segment) { events.push_back(vformat("segment_ended %d %s", p_id, p_segment->get_text())); }
	void on_text_changed(int p_id, const String &p_text) { events.push_back(vformat("text_changed %d %s", p_id, p_text.replace("\n", "|"))); }
	void on_track_paused(int p_id) { events.push_back(vformat("track_paused %d", p_id)); }
	void on_track_resumed(int p_id) { events.push_back(vformat("track_resumed %d", p_id)); }
	void on_track_seeked(int p_id, double p_time) { events.push_back(vformat("track_seeked %d %s", p_id, String::num(p_time, 2))); }
	void on_track_finished(int p_id) { events.push_back(vformat("track_finished %d", p_id)); }
	void on_track_canceled(int p_id) { events.push_back(vformat("track_canceled %d", p_id)); }

	void connect_to(SubtitleServer *p_server) {
		p_server->connect("track_started", callable_mp(this, &SignalLog::on_track_started));
		p_server->connect("segment_started", callable_mp(this, &SignalLog::on_segment_started));
		p_server->connect("segment_ended", callable_mp(this, &SignalLog::on_segment_ended));
		p_server->connect("text_changed", callable_mp(this, &SignalLog::on_text_changed));
		p_server->connect("track_paused", callable_mp(this, &SignalLog::on_track_paused));
		p_server->connect("track_resumed", callable_mp(this, &SignalLog::on_track_resumed));
		p_server->connect("track_seeked", callable_mp(this, &SignalLog::on_track_seeked));
		p_server->connect("track_finished", callable_mp(this, &SignalLog::on_track_finished));
		p_server->connect("track_canceled", callable_mp(this, &SignalLog::on_track_canceled));
	}

	void disconnect_from(SubtitleServer *p_server) {
		p_server->disconnect("track_started", callable_mp(this, &SignalLog::on_track_started));
		p_server->disconnect("segment_started", callable_mp(this, &SignalLog::on_segment_started));
		p_server->disconnect("segment_ended", callable_mp(this, &SignalLog::on_segment_ended));
		p_server->disconnect("text_changed", callable_mp(this, &SignalLog::on_text_changed));
		p_server->disconnect("track_paused", callable_mp(this, &SignalLog::on_track_paused));
		p_server->disconnect("track_resumed", callable_mp(this, &SignalLog::on_track_resumed));
		p_server->disconnect("track_seeked", callable_mp(this, &SignalLog::on_track_seeked));
		p_server->disconnect("track_finished", callable_mp(this, &SignalLog::on_track_finished));
		p_server->disconnect("track_canceled", callable_mp(this, &SignalLog::on_track_canceled));
	}

	String take() {
		const String result = String(", ").join(events);
		events.clear();
		return result;
	}
};

// A time source: a clock that the test sets by hand.
class ManualClock : public Object {
public:
	double time = 0.0;
	double get_time() const { return time; }
};

static Ref<SubtitleSegment> make_segment(double p_start, double p_end, const String &p_text, SubtitleSegment::Kind p_kind = SubtitleSegment::KIND_DIALOG) {
	Ref<SubtitleSegment> segment;
	segment.instantiate();
	segment->set_start_time(p_start);
	segment->set_end_time(p_end);
	segment->set_text(p_text);
	segment->set_kind(p_kind);
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

TEST_CASE("[SubtitleServer] Singleton is registered") {
	CHECK(SubtitleServer::get_singleton() != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("SubtitleServer"));
}

TEST_CASE("[SubtitleServer] Own clock: signals in order, overlaps, finish") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	SignalLog *log = memnew(SignalLog);
	log->connect_to(server);

	const int id = server->play(make_subtitle());
	CHECK(id != 0);
	CHECK(server->is_playing(id));
	CHECK(log->take() == vformat("track_started %d, segment_started %d A, text_changed %d A", id, id, id));
	CHECK(server->get_track_text(id) == "A");

	server->process(1.5);
	CHECK(log->take() == vformat("segment_ended %d A, text_changed %d ", id, id));
	CHECK(server->get_active_segments(id).is_empty());

	server->process(1.0); // 2.5
	CHECK(log->take() == vformat("segment_started %d B, text_changed %d B", id, id));

	server->process(1.0); // 3.5: B and C
	CHECK(log->take() == vformat("segment_started %d C, text_changed %d B|C", id, id));
	CHECK(server->get_active_segments(id).size() == 2);
	CHECK(server->get_track_time(id) == doctest::Approx(3.5));

	server->process(1.0); // 4.5: only C
	CHECK(log->take() == vformat("segment_ended %d B, text_changed %d C", id, id));

	server->process(1.0); // 5.5: past the end
	CHECK(log->take() == vformat("segment_ended %d C, text_changed %d , track_finished %d", id, id, id));
	CHECK_FALSE(server->is_playing(id));

	log->disconnect_from(server);
	memdelete(log);
}

TEST_CASE("[SubtitleServer] Pause, resume, seek and stop") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	SignalLog *log = memnew(SignalLog);
	const int id = server->play(make_subtitle(), Callable(), 2.5);
	log->connect_to(server);

	server->pause(id);
	CHECK(server->is_paused(id));
	server->process(10.0);
	CHECK(server->get_track_time(id) == doctest::Approx(2.5));
	server->resume(id);
	CHECK(log->take() == vformat("track_paused %d, track_resumed %d", id, id));

	server->seek(id, 0.5);
	CHECK(log->take() == vformat("track_seeked %d 0.5, segment_ended %d B, segment_started %d A, text_changed %d A", id, id, id, id));

	server->stop(id);
	CHECK(log->take() == vformat("segment_ended %d A, text_changed %d , track_canceled %d", id, id, id));
	CHECK_FALSE(server->is_playing(id));

	ERR_PRINT_OFF;
	server->stop(id); // Unknown id: error, nothing emitted.
	ERR_PRINT_ON;
	CHECK(log->take().is_empty());

	log->disconnect_from(server);
	memdelete(log);
}

TEST_CASE("[SubtitleServer] Time source") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	SignalLog *log = memnew(SignalLog);
	ManualClock *clock = memnew(ManualClock);
	clock->time = 2.5;
	log->connect_to(server);

	const int id = server->play(make_subtitle(), callable_mp(clock, &ManualClock::get_time));
	CHECK(log->take() == vformat("track_started %d, segment_started %d B, text_changed %d B", id, id, id));

	server->process(100.0); // The delta does not matter, the source decides.
	CHECK(server->get_track_time(id) == doctest::Approx(2.5));
	CHECK(log->take().is_empty());

	clock->time = 0.5; // Jump back.
	server->process(0.016);
	CHECK(log->take() == vformat("track_seeked %d 0.5, segment_ended %d B, segment_started %d A, text_changed %d A", id, id, id, id));

	ERR_PRINT_OFF;
	server->seek(id, 3.0); // Not allowed with a time source.
	ERR_PRINT_ON;
	CHECK(server->get_track_time(id) == doctest::Approx(0.5));
	CHECK(log->take().is_empty());

	// The object of the time source is freed: the track is canceled.
	memdelete(clock);
	server->process(0.016);
	CHECK(log->take() == vformat("segment_ended %d A, text_changed %d , track_canceled %d", id, id, id));
	CHECK_FALSE(server->is_playing(id));

	log->disconnect_from(server);
	memdelete(log);
}

TEST_CASE("[SubtitleServer] enabled and captions_enabled") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	subtitle->add_segment(make_segment(0.0, 2.0, "Hello"));
	subtitle->add_segment(make_segment(0.0, 2.0, "Applause", SubtitleSegment::KIND_SOUND));

	const int id = server->play(subtitle);
	CHECK(server->get_track_text(id) == "Hello\n[Applause]");

	server->set_captions_enabled(false);
	CHECK(server->get_track_text(id) == "Hello");
	server->set_captions_enabled(true);
	CHECK(server->get_track_text(id) == "Hello\n[Applause]");

	server->set_enabled(false);
	CHECK(server->get_track_text(id).is_empty());
	CHECK(server->get_active_segments(id).is_empty());
	server->set_enabled(true);
	CHECK(server->get_active_segments(id).size() == 2);

	server->stop_all();
	CHECK(server->get_track_ids().is_empty());
}

TEST_CASE("[SubtitleServer] Display text") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	const String note = String::chr(0x266A);

	CHECK(server->get_display_text(subtitle, make_segment(0, 1, "Hi")) == "Hi");
	CHECK(server->get_display_text(subtitle, make_segment(0, 1, "laughs", SubtitleSegment::KIND_REACTION)) == "[laughs]");
	CHECK(server->get_display_text(subtitle, make_segment(0, 1, "la\nla", SubtitleSegment::KIND_LYRICS)) == note + " la " + note + "\n" + note + " la " + note);
	CHECK(server->get_display_text(subtitle, make_segment(0, 1, "Applause", SubtitleSegment::KIND_SOUND), true) == "[lb]Applause[rb]");

	Ref<SubtitleSegment> off_screen = make_segment(0, 1, "a [b]");
	off_screen->set_off_screen(true);
	CHECK(server->get_display_text(subtitle, off_screen) == "a [b]");
	CHECK(server->get_display_text(subtitle, off_screen, true) == "[i]a [lb]b][/i]");
	CHECK(server->get_display_text(subtitle, make_segment(0, 1, "Once upon a time", SubtitleSegment::KIND_NARRATION), true) == "[i]Once upon a time[/i]");
}

TEST_CASE("[SubtitleServer] auto_translate follows the subtitles channel") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	TranslationServer *translation_server = TranslationServer::get_singleton();
	Ref<Translation> german;
	german.instantiate();
	german->set_locale("de");
	german->add_message("LINE_1", "Wo sind wir?");
	german->add_message("LINE_1", "Wo bin ich?", "alone");
	const Ref<TranslationDomain> domain = translation_server->get_or_add_domain("subtitles");
	domain->add_translation(german);
	const String locale = translation_server->get_locale();
	translation_server->set_locale("en");

	Ref<Subtitle> subtitle;
	subtitle.instantiate();
	Ref<SubtitleSegment> line = make_segment(0, 1, "LINE_1");
	CHECK(server->get_display_text(subtitle, line) == "LINE_1"); // auto_translate is off.
	subtitle->set_auto_translate(true);
	CHECK(server->get_display_text(subtitle, line) == "LINE_1"); // No English text.

	// Voices in English, subtitles in German: only the subtitles channel changes.
	translation_server->set_locale_for("subtitles", "de");
	CHECK(translation_server->get_locale_for("interface") == "en");
	CHECK(server->get_display_text(subtitle, line) == "Wo sind wir?");
	line->set_translation_context("alone");
	CHECK(server->get_display_text(subtitle, line) == "Wo bin ich?");
	CHECK(String(translation_server->translate("LINE_1")) == "LINE_1"); // Not in the main domain.

	translation_server->clear_locale_for("subtitles");
	CHECK(server->get_display_text(subtitle, line) == "LINE_1");

	domain->remove_translation(german);
	translation_server->set_locale(locale);
}

TEST_CASE("[SubtitleServer] An empty subtitle finishes at the next frame") {
	SubtitleServer *server = SubtitleServer::get_singleton();
	SignalLog *log = memnew(SignalLog);
	log->connect_to(server);
	Ref<Subtitle> empty;
	empty.instantiate();
	const int id = server->play(empty);
	server->process(0.016);
	CHECK(log->take() == vformat("track_started %d, track_finished %d", id, id));

	ERR_PRINT_OFF;
	CHECK(server->play(Ref<Subtitle>()) == 0);
	ERR_PRINT_ON;
	CHECK(log->take().is_empty());

	log->disconnect_from(server);
	memdelete(log);
}

} // namespace TestSubtitleServer
