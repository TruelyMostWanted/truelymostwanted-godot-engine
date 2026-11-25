/**************************************************************************/
/*  subtitle_formats.cpp                                                  */
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

#include "subtitle_formats.h"

#include "core/io/xml_parser.h"
#include "core/string/char_utils.h"
#include "servers/subtitle/subtitle_format_srt.h"

// Shared helpers.

static Vector<String> _lines_of(const String &p_text) {
	return p_text.replace("\r\n", "\n").replace("\r", "\n").split("\n");
}

// Sorted by start time (stable), without empty slots.
static Vector<Ref<SubtitleSegment>> _sorted(const Vector<Ref<SubtitleSegment>> &p_segments) {
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
	return sorted;
}

static Ref<SubtitleSegment> _make_segment(double p_start, double p_end, const String &p_raw_text) {
	Ref<SubtitleSegment> segment;
	segment.instantiate();
	segment->set_start_time(p_start);
	segment->set_end_time(p_end);
	SubtitleFormatSRT::read_cue_text(p_raw_text, segment);
	return segment;
}

static String _escape_markup(const String &p_text) {
	return p_text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
}

static bool _has_content(const Vector<String> &p_lines) {
	for (const String &line : p_lines) {
		if (!line.strip_edges().is_empty()) {
			return true;
		}
	}
	return false;
}

// WebVTT

String SubtitleFormatVTT::format_time(double p_seconds) {
	return SubtitleFormatSRT::format_time(p_seconds).replace(",", ".");
}

// `line:` near the top: a percentage below 50, or a line number from 0 to 3 (negative numbers count from the bottom).
static bool _vtt_settings_top(const String &p_settings) {
	for (const String &setting : p_settings.split(" ", false)) {
		if (!setting.begins_with("line:")) {
			continue;
		}
		const String value = setting.substr(5).get_slice(",", 0);
		if (value.ends_with("%")) {
			return value.trim_suffix("%").to_float() < 50.0;
		}
		return value.is_valid_int() && value.to_int() >= 0 && value.to_int() <= 3;
	}
	return false;
}

Error SubtitleFormatVTT::parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped) {
	r_segments.clear();
	int skipped = 0;
	const Vector<String> lines = _lines_of(p_text);

	// Blocks are separated by blank lines; the first block is the header ("WEBVTT …").
	Vector<Vector<String>> blocks;
	Vector<String> block;
	for (const String &line : lines) {
		if (line.strip_edges().is_empty()) {
			if (!block.is_empty()) {
				blocks.push_back(block);
				block.clear();
			}
		} else {
			block.push_back(line);
		}
	}
	if (!block.is_empty()) {
		blocks.push_back(block);
	}

	for (int b = 0; b < blocks.size(); b++) {
		const Vector<String> &lines_of_block = blocks[b];
		const String first = lines_of_block[0].strip_edges();
		if ((b == 0 && first.begins_with("WEBVTT")) || first.begins_with("NOTE") || first == "STYLE" || first == "REGION") {
			continue;
		}
		int time_line = lines_of_block[0].contains("-->") ? 0 : 1;
		if (time_line >= lines_of_block.size() || !lines_of_block[time_line].contains("-->")) {
			skipped++;
			continue;
		}
		const String times = lines_of_block[time_line];
		const int arrow = times.find("-->");
		const String rest = times.substr(arrow + 3).strip_edges();
		const int space = rest.find_char(' ');
		const String end_text = space >= 0 ? rest.substr(0, space) : rest;
		const String settings = space >= 0 ? rest.substr(space + 1) : String();
		double start = 0.0;
		double end = 0.0;
		if (!SubtitleFormatSRT::parse_time(times.substr(0, arrow), start) || !SubtitleFormatSRT::parse_time(end_text, end)) {
			skipped++;
			continue;
		}

		Vector<String> text_lines;
		for (int i = time_line + 1; i < lines_of_block.size(); i++) {
			text_lines.push_back(lines_of_block[i]);
		}
		String raw = String("\n").join(text_lines).strip_edges();

		// `<v Name>` or `<v.class Name>`: the speaker. The tag is removed before the italics check.
		String speaker;
		const int voice = raw.find("<v");
		if (voice >= 0 && voice + 2 < raw.length() && (raw[voice + 2] == ' ' || raw[voice + 2] == '.')) {
			const int close = raw.find_char('>', voice);
			if (close > voice) {
				const String tag = raw.substr(voice + 2, close - voice - 2);
				const int name_start = tag.find_char(' ');
				speaker = name_start >= 0 ? tag.substr(name_start + 1).strip_edges() : String();
				raw = (raw.substr(0, voice) + raw.substr(close + 1)).replace("</v>", "").strip_edges();
			}
		}

		Ref<SubtitleSegment> segment = _make_segment(start, end, raw);
		segment->set_text(SubtitleFormatSRT::decode_entities(segment->get_text()));
		if (!speaker.is_empty()) {
			segment->set_speaker(SubtitleFormatSRT::decode_entities(speaker));
		}
		if (_vtt_settings_top(settings)) {
			segment->set_placement(SubtitleSegment::PLACEMENT_TOP);
		}
		r_segments.push_back(segment);
	}

	if (r_skipped) {
		*r_skipped = skipped;
	}
	return (r_segments.is_empty() && skipped > 0) ? ERR_PARSE_ERROR : OK;
}

String SubtitleFormatVTT::write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset) {
	String result = "WEBVTT\n";
	for (const Ref<SubtitleSegment> &segment : _sorted(p_segments)) {
		result += "\n" + format_time(segment->get_start_time() + p_time_offset) + " --> " + format_time(segment->get_end_time() + p_time_offset);
		if (segment->get_placement() == SubtitleSegment::PLACEMENT_TOP) {
			result += " line:0";
		}
		String text = _escape_markup(SubtitleFormatSRT::decorate_text(segment));
		if (segment->is_off_screen()) {
			text = "<i>" + text + "</i>";
		}
		const String speaker = String(segment->get_speaker()).strip_edges();
		if (!speaker.is_empty() && !segment->is_caption()) {
			text = "<v " + _escape_markup(speaker) + ">" + text;
		}
		result += "\n" + text + "\n";
	}
	return result;
}

// TTML

bool SubtitleFormatTTML::parse_time(const String &p_text, double p_frame_rate, double p_tick_rate, double &r_seconds) {
	const String text = p_text.strip_edges();
	if (text.is_empty()) {
		return false;
	}
	if (text.contains(":")) {
		const Vector<String> parts = text.split(":");
		if (parts.size() < 3 || parts.size() > 4) {
			return false;
		}
		for (const String &part : parts) {
			if (!part.is_valid_float()) {
				return false;
			}
		}
		double seconds = parts[0].to_float() * 3600.0 + parts[1].to_float() * 60.0 + parts[2].to_float();
		if (parts.size() == 4) {
			seconds += parts[3].to_float() / MAX(p_frame_rate, 1.0);
		}
		r_seconds = seconds;
		return true;
	}
	// Offset time: a number and a unit.
	struct Unit {
		const char *suffix;
		double factor;
	};
	const Unit units[] = { { "ms", 0.001 }, { "h", 3600.0 }, { "m", 60.0 }, { "s", 1.0 }, { "f", 1.0 / MAX(p_frame_rate, 1.0) }, { "t", 1.0 / MAX(p_tick_rate, 1.0) } };
	for (const Unit &unit : units) {
		if (text.ends_with(unit.suffix)) {
			const String number = text.substr(0, text.length() - String(unit.suffix).length());
			if (!number.is_valid_float()) {
				return false;
			}
			r_seconds = number.to_float() * unit.factor;
			return true;
		}
	}
	return false;
}

static String _local_name(const String &p_name) {
	const int colon = p_name.find_char(':');
	return colon >= 0 ? p_name.substr(colon + 1) : p_name;
}

static String _collapse_whitespace(const String &p_line) {
	String result;
	bool space = false;
	for (int i = 0; i < p_line.length(); i++) {
		const char32_t c = p_line[i];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			space = true;
			continue;
		}
		if (space && !result.is_empty()) {
			result += " ";
		}
		space = false;
		result += c;
	}
	return result;
}

static int _count_visible(const String &p_text) {
	int count = 0;
	for (int i = 0; i < p_text.length(); i++) {
		if (!is_whitespace(p_text[i])) {
			count++;
		}
	}
	return count;
}

Error SubtitleFormatTTML::parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped, String *r_language) {
	r_segments.clear();
	int skipped = 0;
	if (p_text.strip_edges().is_empty()) {
		if (r_skipped) {
			*r_skipped = 0;
		}
		return OK;
	}

	Ref<XMLParser> parser;
	parser.instantiate();
	if (parser->open_buffer(p_text.to_utf8_buffer()) != OK) {
		return ERR_PARSE_ERROR;
	}

	bool found_root = false;
	double frame_rate = 30.0;
	double tick_rate = 1.0;
	bool in_paragraph = false;
	String begin;
	String end;
	String duration;
	bool paragraph_italic = false;
	Vector<bool> span_italic;
	String text;
	int visible = 0;
	int italic_visible = 0;

	while (parser->read() == OK) {
		switch (parser->get_node_type()) {
			case XMLParser::NODE_ELEMENT: {
				const String name = _local_name(parser->get_node_name());
				if (name == "tt") {
					found_root = true;
					if (r_language) {
						*r_language = parser->get_named_attribute_value_safe("xml:lang");
					}
					const String rate = parser->get_named_attribute_value_safe("ttp:frameRate");
					if (rate.is_valid_float() && rate.to_float() > 0.0) {
						frame_rate = rate.to_float();
						tick_rate = frame_rate;
					}
					const String ticks = parser->get_named_attribute_value_safe("ttp:tickRate");
					if (ticks.is_valid_float() && ticks.to_float() > 0.0) {
						tick_rate = ticks.to_float();
					}
				} else if (name == "p") {
					in_paragraph = !parser->is_empty();
					begin = parser->get_named_attribute_value_safe("begin");
					end = parser->get_named_attribute_value_safe("end");
					duration = parser->get_named_attribute_value_safe("dur");
					paragraph_italic = parser->get_named_attribute_value_safe("tts:fontStyle") == "italic";
					span_italic.clear();
					text = String();
					visible = 0;
					italic_visible = 0;
				} else if (in_paragraph && name == "br") {
					text += "\n";
				} else if (in_paragraph && name == "span" && !parser->is_empty()) {
					span_italic.push_back(parser->get_named_attribute_value_safe("tts:fontStyle") == "italic");
				}
			} break;
			case XMLParser::NODE_TEXT:
			case XMLParser::NODE_CDATA: {
				if (!in_paragraph) {
					break;
				}
				const String data = parser->get_node_type() == XMLParser::NODE_TEXT ? parser->get_node_data().xml_unescape() : parser->get_node_data();
				text += data;
				const int count = _count_visible(data);
				visible += count;
				if (paragraph_italic || span_italic.has(true)) {
					italic_visible += count;
				}
			} break;
			case XMLParser::NODE_ELEMENT_END: {
				const String name = _local_name(parser->get_node_name());
				if (in_paragraph && name == "span" && !span_italic.is_empty()) {
					span_italic.resize(span_italic.size() - 1);
				} else if (in_paragraph && name == "p") {
					in_paragraph = false;
					double start = 0.0;
					double stop = 0.0;
					double length = 0.0;
					if (!parse_time(begin, frame_rate, tick_rate, start)) {
						skipped++;
						break;
					}
					if (!parse_time(end, frame_rate, tick_rate, stop)) {
						if (!parse_time(duration, frame_rate, tick_rate, length)) {
							skipped++;
							break;
						}
						stop = start + length;
					}
					Vector<String> lines = text.split("\n");
					Vector<String> kept;
					for (const String &line : lines) {
						const String collapsed = _collapse_whitespace(line);
						if (!collapsed.is_empty()) {
							kept.push_back(collapsed);
						}
					}
					Ref<SubtitleSegment> segment = _make_segment(start, stop, String("\n").join(kept));
					if (visible > 0 && italic_visible == visible) {
						segment->set_off_screen(true);
					}
					r_segments.push_back(segment);
				}
			} break;
			default:
				break;
		}
	}

	if (r_skipped) {
		*r_skipped = skipped;
	}
	return (!found_root || (r_segments.is_empty() && skipped > 0)) ? ERR_PARSE_ERROR : OK;
}

String SubtitleFormatTTML::write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset, const String &p_language) {
	String result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	result += "<tt xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"" + p_language.xml_escape(true) + "\">\n";
	result += "\t<body>\n\t\t<div>\n";
	for (const Ref<SubtitleSegment> &segment : _sorted(p_segments)) {
		String text = SubtitleFormatSRT::decorate_text(segment);
		const String speaker = String(segment->get_speaker()).strip_edges();
		if (!speaker.is_empty() && !segment->is_caption()) {
			text = speaker.to_upper() + ": " + text;
		}
		text = text.xml_escape().replace("\n", "<br/>");
		if (segment->is_off_screen()) {
			text = "<span tts:fontStyle=\"italic\">" + text + "</span>";
		}
		result += "\t\t\t<p begin=\"" + SubtitleFormatVTT::format_time(segment->get_start_time() + p_time_offset) + "\" end=\"" + SubtitleFormatVTT::format_time(segment->get_end_time() + p_time_offset) + "\">" + text + "</p>\n";
	}
	result += "\t\t</div>\n\t</body>\n</tt>\n";
	return result;
}

// SubStation Alpha

// The event line keyword of SubStation Alpha.
static const char *ASS_DIALOG_EVENT = "Dialogue:";

String SubtitleFormatASS::format_time(double p_seconds) {
	int64_t total_cs = int64_t(Math::round(MAX(0.0, p_seconds) * 100.0));
	const int64_t cs = total_cs % 100;
	total_cs /= 100;
	const int64_t seconds = total_cs % 60;
	const int64_t minutes = (total_cs / 60) % 60;
	const int64_t hours = total_cs / 3600;
	return vformat("%d:%s:%s.%s", hours, String::num_int64(minutes).pad_zeros(2), String::num_int64(seconds).pad_zeros(2), String::num_int64(cs).pad_zeros(2));
}

Error SubtitleFormatASS::parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped) {
	r_segments.clear();
	int skipped = 0;
	const Vector<String> lines = _lines_of(p_text);
	String section;
	bool known_section = false;
	Vector<String> columns = { "layer", "start", "end", "style", "name", "marginl", "marginr", "marginv", "effect", "text" };

	for (const String &original : lines) {
		const String line = original.strip_edges();
		if (line.begins_with("[") && line.ends_with("]")) {
			section = line.to_lower();
			known_section = known_section || section == "[script info]" || section == "[events]";
			continue;
		}
		if (section != "[events]") {
			continue;
		}
		if (line.begins_with("Format:")) {
			columns.clear();
			for (const String &column : line.substr(7).split(",")) {
				columns.push_back(column.strip_edges().to_lower());
			}
			continue;
		}
		if (!line.begins_with(ASS_DIALOG_EVENT)) {
			continue; // Comments and other event types.
		}
		const Vector<String> fields = line.substr(String(ASS_DIALOG_EVENT).length()).strip_edges(true, false).split(",", true, columns.size() - 1);
		const int start_index = columns.find("start");
		const int end_index = columns.find("end");
		const int text_index = columns.find("text");
		const int name_index = columns.find("name");
		double start = 0.0;
		double end = 0.0;
		if (fields.size() < columns.size() || start_index < 0 || end_index < 0 || text_index < 0 || !SubtitleFormatSRT::parse_time(fields[start_index], start) || !SubtitleFormatSRT::parse_time(fields[end_index], end)) {
			skipped++;
			continue;
		}
		String raw = fields[text_index];
		raw = raw.replace("\\N", "\n").replace("\\n", "\n").replace("\\h", " ");
		raw = raw.replace("{\\i1}", "<i>").replace("{\\i0}", "</i>");
		Ref<SubtitleSegment> segment = _make_segment(start, end, raw);
		const String speaker = name_index >= 0 ? fields[name_index].strip_edges() : String();
		if (!speaker.is_empty()) {
			segment->set_speaker(speaker);
		}
		r_segments.push_back(segment);
	}

	if (r_skipped) {
		*r_skipped = skipped;
	}
	return (r_segments.is_empty() && (skipped > 0 || (!known_section && _has_content(lines)))) ? ERR_PARSE_ERROR : OK;
}

String SubtitleFormatASS::write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset) {
	String result = "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n\n";
	result += "[V4+ Styles]\n";
	result += "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
	result += "Style: Default,Arial,64,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,3,1,2,40,40,40,1\n\n";
	result += "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
	for (const Ref<SubtitleSegment> &segment : _sorted(p_segments)) {
		String text = SubtitleFormatSRT::decorate_text(segment).replace("\n", "\\N");
		if (segment->is_off_screen()) {
			text = "{\\i1}" + text + "{\\i0}";
		}
		if (segment->get_placement() == SubtitleSegment::PLACEMENT_TOP) {
			text = "{\\an8}" + text;
		}
		const String speaker = segment->is_caption() ? String() : String(segment->get_speaker()).replace(",", " ").strip_edges();
		result += String(ASS_DIALOG_EVENT) + " 0," + format_time(segment->get_start_time() + p_time_offset) + "," + format_time(segment->get_end_time() + p_time_offset) + ",Default," + speaker + ",0,0,0,," + text + "\n";
	}
	return result;
}

// SBV

String SubtitleFormatSBV::format_time(double p_seconds) {
	int64_t total_ms = int64_t(Math::round(MAX(0.0, p_seconds) * 1000.0));
	const int64_t ms = total_ms % 1000;
	total_ms /= 1000;
	const int64_t seconds = total_ms % 60;
	const int64_t minutes = (total_ms / 60) % 60;
	const int64_t hours = total_ms / 3600;
	return vformat("%d:%s:%s.%s", hours, String::num_int64(minutes).pad_zeros(2), String::num_int64(seconds).pad_zeros(2), String::num_int64(ms).pad_zeros(3));
}

Error SubtitleFormatSBV::parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped) {
	r_segments.clear();
	int skipped = 0;
	const Vector<String> lines = _lines_of(p_text);
	int i = 0;
	while (i < lines.size()) {
		const String line = lines[i].strip_edges();
		if (line.is_empty()) {
			i++;
			continue;
		}
		Vector<String> text_lines;
		int j = i + 1;
		while (j < lines.size() && !lines[j].strip_edges().is_empty()) {
			text_lines.push_back(lines[j]);
			j++;
		}
		const Vector<String> times = line.split(",");
		double start = 0.0;
		double end = 0.0;
		if (times.size() != 2 || !SubtitleFormatSRT::parse_time(times[0], start) || !SubtitleFormatSRT::parse_time(times[1], end)) {
			skipped++;
		} else {
			r_segments.push_back(_make_segment(start, end, String("\n").join(text_lines)));
		}
		i = j;
	}
	if (r_skipped) {
		*r_skipped = skipped;
	}
	return (r_segments.is_empty() && skipped > 0) ? ERR_PARSE_ERROR : OK;
}

String SubtitleFormatSBV::write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset) {
	String result;
	for (const Ref<SubtitleSegment> &segment : _sorted(p_segments)) {
		String text = SubtitleFormatSRT::decorate_text(segment);
		const String speaker = String(segment->get_speaker()).strip_edges();
		if (!speaker.is_empty() && !segment->is_caption()) {
			text = speaker.to_upper() + ": " + text;
		}
		if (!result.is_empty()) {
			result += "\n";
		}
		result += format_time(segment->get_start_time() + p_time_offset) + "," + format_time(segment->get_end_time() + p_time_offset) + "\n" + text + "\n";
	}
	return result;
}
