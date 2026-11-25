/**************************************************************************/
/*  subtitle_formats.h                                                    */
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

#include "servers/subtitle/subtitle.h"

// The subtitle formats besides SubRip. Each reads the timing and the text a game needs (speaker, italics,
// top placement, kinds through the shared cue text rules of SubtitleFormatSRT) and drops styling.
// parse() skips cues it can't read (counted in r_skipped) and fails with ERR_PARSE_ERROR only if the text
// has content but no cue could be read. write() adds `p_time_offset` to the written times.

// WebVTT (.vtt): header `WEBVTT`, cues with optional identifier and settings, `<v Name>` for speakers.
class SubtitleFormatVTT {
public:
	static Error parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped = nullptr);
	static String write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset);
	// "00:01:02.450" (WebVTT needs a dot before the milliseconds).
	static String format_time(double p_seconds);
};

// TTML / DFXP (.ttml, .dfxp): `<p begin end|dur>` in the body; clock times, frames and offsets.
class SubtitleFormatTTML {
public:
	static Error parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped = nullptr, String *r_language = nullptr);
	static String write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset, const String &p_language);
	// "00:00:01.5", "00:00:01:12" (frames), "1.5s", "1500ms", "90f", "2m", "1h", "15000000t" (ticks).
	static bool parse_time(const String &p_text, double p_frame_rate, double p_tick_rate, double &r_seconds);
};

// Advanced SubStation Alpha (.ass) and SubStation Alpha (.ssa): the dialog event lines of the [Events] section.
class SubtitleFormatASS {
public:
	static Error parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped = nullptr);
	static String write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset);
	// "0:01:02.45" (centiseconds).
	static String format_time(double p_seconds);
};

// YouTube SBV (.sbv): `0:00:01.000,0:00:02.500`, then the text, cues separated by blank lines.
class SubtitleFormatSBV {
public:
	static Error parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped = nullptr);
	static String write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset);
	// "0:01:02.450".
	static String format_time(double p_seconds);
};
