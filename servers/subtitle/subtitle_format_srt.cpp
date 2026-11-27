/**************************************************************************/
/*  subtitle_format_srt.cpp                                               */
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

#include "subtitle_format_srt.h"

#include "core/string/char_utils.h"

static const char32_t MUSIC_NOTE = 0x266A; // ♪
static const char32_t MUSIC_NOTES = 0x266B; // ♫

// Decoding.

String SubtitleFormatSRT::decode(const Vector<uint8_t> &p_bytes) {
	const int size = p_bytes.size();
	const uint8_t *data = p_bytes.ptr();
	if (size >= 2 && ((data[0] == 0xFF && data[1] == 0xFE) || (data[0] == 0xFE && data[1] == 0xFF))) {
		const bool little_endian = data[0] == 0xFF;
		Vector<char16_t> units;
		units.resize((size - 2) / 2);
		for (int i = 0; i < units.size(); i++) {
			const uint8_t a = data[2 + i * 2];
			const uint8_t b = data[3 + i * 2];
			units.write[i] = little_endian ? char16_t(a | (b << 8)) : char16_t(b | (a << 8));
		}
		return String::utf16(units.ptr(), units.size());
	}
	int start = 0;
	if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
		start = 3;
	}
	return String::utf8((const char *)data + start, size - start);
}

// Times.

String SubtitleFormatSRT::format_time(double p_seconds) {
	int64_t total_ms = int64_t(Math::round(MAX(0.0, p_seconds) * 1000.0));
	const int64_t ms = total_ms % 1000;
	total_ms /= 1000;
	const int64_t seconds = total_ms % 60;
	total_ms /= 60;
	const int64_t minutes = total_ms % 60;
	const int64_t hours = total_ms / 60;
	return vformat("%s:%s:%s,%s", String::num_int64(hours).pad_zeros(2), String::num_int64(minutes).pad_zeros(2), String::num_int64(seconds).pad_zeros(2), String::num_int64(ms).pad_zeros(3));
}

static bool _is_digits(const String &p_text) {
	if (p_text.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_text.length(); i++) {
		if (!is_digit(p_text[i])) {
			return false;
		}
	}
	return true;
}

bool SubtitleFormatSRT::parse_time(const String &p_text, double &r_seconds) {
	const String text = p_text.strip_edges();
	int fraction_pos = text.rfind_char(',');
	if (fraction_pos < 0) {
		fraction_pos = text.rfind_char('.');
	}
	const String clock = fraction_pos < 0 ? text : text.substr(0, fraction_pos);
	const String fraction = fraction_pos < 0 ? String() : text.substr(fraction_pos + 1);
	const Vector<String> parts = clock.split(":");
	if (parts.size() < 2 || parts.size() > 3) {
		return false;
	}
	for (const String &part : parts) {
		if (!_is_digits(part)) {
			return false;
		}
	}
	if (!fraction.is_empty() && !_is_digits(fraction)) {
		return false;
	}
	double seconds = 0.0;
	for (const String &part : parts) {
		seconds = seconds * 60.0 + double(part.to_int());
	}
	if (!fraction.is_empty()) {
		// "5" is half a second, "500" too: the digits are a decimal fraction.
		seconds += double(fraction.to_int()) / Math::pow(10.0, double(fraction.length()));
	}
	r_seconds = seconds;
	return true;
}

// Text.

String SubtitleFormatSRT::strip_tags(const String &p_text) {
	String result;
	const int length = p_text.length();
	int i = 0;
	while (i < length) {
		const char32_t c = p_text[i];
		if (c == '<') {
			// A tag starts with a letter or `/` and ends at the next `>` without another `<` in between.
			const int close = p_text.find_char('>', i + 1);
			const int next_open = p_text.find_char('<', i + 1);
			if (close > i && (next_open < 0 || next_open > close) && i + 1 < length && (is_ascii_alphabet_char(p_text[i + 1]) || p_text[i + 1] == '/')) {
				i = close + 1;
				continue;
			}
		} else if (c == '{' && i + 1 < length && p_text[i + 1] == '\\') {
			const int close = p_text.find_char('}', i + 2);
			if (close > i) {
				i = close + 1;
				continue;
			}
		}
		result += c;
		i++;
	}
	return result;
}

static bool _is_wrapped(const String &p_text, char32_t p_open, char32_t p_close) {
	if (p_text.length() < 2 || p_text[0] != p_open || p_text[p_text.length() - 1] != p_close) {
		return false;
	}
	// "[a] and [b]" is not one bracketed text.
	const int first_close = p_text.find_char(p_close, 1);
	return first_close == p_text.length() - 1;
}

static String _strip_music_notes(const String &p_text) {
	int from = 0;
	int to = p_text.length();
	while (from < to && (p_text[from] == MUSIC_NOTE || p_text[from] == MUSIC_NOTES || p_text[from] == ' ')) {
		from++;
	}
	while (to > from && (p_text[to - 1] == MUSIC_NOTE || p_text[to - 1] == MUSIC_NOTES || p_text[to - 1] == ' ')) {
		to--;
	}
	return p_text.substr(from, to - from);
}

// "JOHN: Hello" -> "JOHN". A speaker is written in capitals, at most 32 characters, and has at least one letter.
static bool _split_speaker(const String &p_line, String &r_speaker, String &r_rest) {
	const int colon = p_line.find_char(':');
	if (colon < 1 || colon > 32 || colon + 1 >= p_line.length() || p_line[colon + 1] != ' ') {
		return false;
	}
	const String name = p_line.substr(0, colon).strip_edges();
	bool has_letter = false;
	for (int i = 0; i < name.length(); i++) {
		const char32_t c = name[i];
		const String one = String::chr(c);
		if (one.to_lower() != one) {
			has_letter = true; // An upper-case letter.
		} else if (one.to_upper() != one) {
			return false; // A lower-case letter: normal text such as "Note: ...".
		} else if (!(is_digit(c) || c == ' ' || c == '.' || c == '-' || c == '\'' || c == '_')) {
			return false;
		}
	}
	if (!has_letter) {
		return false;
	}
	r_speaker = name;
	r_rest = p_line.substr(colon + 1).strip_edges();
	return true;
}

void SubtitleFormatSRT::read_cue_text(const String &p_raw_text, const Ref<SubtitleSegment> &r_segment) {
	ERR_FAIL_COND(r_segment.is_null());
	String raw = p_raw_text.strip_edges();

	// `{\an7}`, `{\an8}`, `{\an9}`: alignment at the top (SubStation Alpha tags, also used in SubRip files).
	const bool top = raw.contains("\\an7") || raw.contains("\\an8") || raw.contains("\\an9");

	// Leading `{\…}` blocks don't count for the italics check below.
	while (raw.begins_with("{\\") && raw.find_char('}') > 0) {
		raw = raw.substr(raw.find_char('}') + 1).strip_edges();
	}

	// A text fully in italics is usually an off-screen voice.
	bool off_screen = false;
	if (raw.begins_with("<i>") && raw.ends_with("</i>") && raw.find("<i>", 3) < 0) {
		off_screen = true;
	}

	// Remove formatting, then empty lines and the spaces around each line.
	const Vector<String> lines = strip_tags(raw).split("\n");
	Vector<String> kept;
	for (const String &line : lines) {
		const String trimmed = line.strip_edges();
		if (!trimmed.is_empty()) {
			kept.push_back(trimmed);
		}
	}
	String text = String("\n").join(kept);

	SubtitleSegment::Kind kind = SubtitleSegment::KIND_DIALOG;
	String speaker;
	if (_is_wrapped(text, '[', ']') || _is_wrapped(text, '(', ')')) {
		kind = SubtitleSegment::KIND_SOUND;
		text = text.substr(1, text.length() - 2).strip_edges();
	} else if (!text.is_empty() && (text[0] == MUSIC_NOTE || text[0] == MUSIC_NOTES)) {
		kind = SubtitleSegment::KIND_MUSIC;
		Vector<String> music_lines = text.split("\n");
		for (int i = 0; i < music_lines.size(); i++) {
			music_lines.write[i] = _strip_music_notes(music_lines[i]);
		}
		text = String("\n").join(music_lines);
	} else if (!kept.is_empty()) {
		String rest;
		if (_split_speaker(kept[0], speaker, rest)) {
			kept.write[0] = rest;
			text = String("\n").join(kept);
		}
	}

	r_segment->set_text(text);
	r_segment->set_kind(kind);
	r_segment->set_speaker(speaker);
	r_segment->set_off_screen(off_screen);
	r_segment->set_placement(top ? SubtitleSegment::PLACEMENT_TOP : SubtitleSegment::PLACEMENT_AUTO);
}

String SubtitleFormatSRT::decode_entities(const String &p_text) {
	return p_text.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"").replace("&apos;", "'").replace("&nbsp;", String::chr(0x00A0)).replace("&lrm;", String::chr(0x200E)).replace("&rlm;", String::chr(0x200F)).replace("&amp;", "&");
}

String SubtitleFormatSRT::decorate_text(const Ref<SubtitleSegment> &p_segment) {
	ERR_FAIL_COND_V(p_segment.is_null(), String());
	String text = p_segment->get_text().strip_edges();
	switch (p_segment->get_kind()) {
		case SubtitleSegment::KIND_SOUND:
		case SubtitleSegment::KIND_REACTION:
			return "[" + text + "]";
		case SubtitleSegment::KIND_MUSIC:
		case SubtitleSegment::KIND_LYRICS: {
			Vector<String> lines = text.split("\n");
			for (int i = 0; i < lines.size(); i++) {
				lines.write[i] = String::chr(MUSIC_NOTE) + " " + lines[i] + " " + String::chr(MUSIC_NOTE);
			}
			return String("\n").join(lines);
		}
		default:
			return text;
	}
}

String SubtitleFormatSRT::make_cue_text(const Ref<SubtitleSegment> &p_segment) {
	ERR_FAIL_COND_V(p_segment.is_null(), String());
	String text = decorate_text(p_segment);
	const String speaker = String(p_segment->get_speaker()).strip_edges();
	if (!speaker.is_empty() && !p_segment->is_caption()) {
		text = speaker.to_upper() + ": " + text;
	}
	if (p_segment->is_off_screen()) {
		text = "<i>" + text + "</i>";
	}
	if (p_segment->get_placement() == SubtitleSegment::PLACEMENT_TOP) {
		text = "{\\an8}" + text;
	}
	return text;
}

// Cues.

Error SubtitleFormatSRT::parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped) {
	r_segments.clear();
	int skipped = 0;
	const Vector<String> lines = p_text.replace("\r\n", "\n").replace("\r", "\n").split("\n");
	bool has_content = false;

	int i = 0;
	while (i < lines.size()) {
		const String line = lines[i].strip_edges();
		if (line.is_empty()) {
			i++;
			continue;
		}
		has_content = true;

		// Optional cue number, then the time line.
		int time_line = i;
		if (!line.contains("-->") && _is_digits(line) && i + 1 < lines.size() && lines[i + 1].contains("-->")) {
			time_line = i + 1;
		}
		const String times = lines[time_line].strip_edges();
		const int arrow = times.find("-->");
		if (arrow < 0) {
			// Not a cue: skip to the next blank line.
			skipped++;
			while (i < lines.size() && !lines[i].strip_edges().is_empty()) {
				i++;
			}
			continue;
		}

		// The end time may be followed by position coordinates ("X1:…"): only the first token counts.
		const String start_text = times.substr(0, arrow);
		String end_text = times.substr(arrow + 3).strip_edges();
		const int space = end_text.find_char(' ');
		if (space >= 0) {
			end_text = end_text.substr(0, space);
		}

		// Text up to the next blank line, or up to the number and time line of the next cue.
		Vector<String> text_lines;
		int j = time_line + 1;
		while (j < lines.size()) {
			const String text_line = lines[j];
			if (text_line.strip_edges().is_empty()) {
				break;
			}
			if (text_line.contains("-->") || (_is_digits(text_line.strip_edges()) && j + 1 < lines.size() && lines[j + 1].contains("-->"))) {
				break;
			}
			text_lines.push_back(text_line);
			j++;
		}
		i = j;

		double start = 0.0;
		double end = 0.0;
		if (!parse_time(start_text, start) || !parse_time(end_text, end)) {
			skipped++;
			continue;
		}

		Ref<SubtitleSegment> segment;
		segment.instantiate();
		segment->set_start_time(start);
		segment->set_end_time(end);
		read_cue_text(String("\n").join(text_lines), segment);
		r_segments.push_back(segment);
	}

	if (r_skipped) {
		*r_skipped = skipped;
	}
	if (has_content && r_segments.is_empty()) {
		return ERR_PARSE_ERROR;
	}
	return OK;
}

String SubtitleFormatSRT::write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset) {
	// Sorted copy (stable), without empty slots.
	Vector<Ref<SubtitleSegment>> sorted;
	for (const Ref<SubtitleSegment> &segment : p_segments) {
		if (segment.is_null()) {
			continue;
		}
		int index = sorted.size();
		while (index > 0 && sorted[index - 1]->get_start_time() > segment->get_start_time()) {
			index--;
		}
		sorted.insert(index, segment);
	}

	String result;
	int number = 0;
	for (const Ref<SubtitleSegment> &segment : sorted) {
		// An empty cue ends its block at the timing line: readers drop it or misread the next cue.
		const String text = make_cue_text(segment);
		if (text.strip_edges().is_empty()) {
			continue;
		}
		if (number > 0) {
			result += "\n";
		}
		number++;
		result += itos(number) + "\n";
		result += format_time(segment->get_start_time() + p_time_offset) + " --> " + format_time(segment->get_end_time() + p_time_offset) + "\n";
		result += text + "\n";
	}
	return result;
}
