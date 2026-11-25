/**************************************************************************/
/*  subtitle_format_srt.h                                                 */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "servers/subtitle/subtitle.h"

// SubRip (.srt) without formatting. Reading removes formatting tags and recognizes kinds and speakers;
// writing puts the decorations back, so the file works in any player.
class SubtitleFormatSRT {
public:
	// Text from file bytes: UTF-8 with or without BOM, UTF-16 LE/BE with BOM.
	static String decode(const Vector<uint8_t> &p_bytes);
	// Cues that can't be read are skipped and counted in `r_skipped`. Fails with ERR_PARSE_ERROR
	// only if the text has content but not a single cue could be read.
	static Error parse(const String &p_text, Vector<Ref<SubtitleSegment>> &r_segments, int *r_skipped = nullptr);
	// Segment times plus `p_time_offset`, sorted by start time, `\n` line endings.
	static String write(const Vector<Ref<SubtitleSegment>> &p_segments, double p_time_offset);

	// "01:02:03,450" for 3723.45 seconds; negative times are written as 0.
	static String format_time(double p_seconds);
	// Reads "01:02:03,450", "02:03.45" and "1:02:03,4"; returns false for anything else.
	static bool parse_time(const String &p_text, double &r_seconds);
	// Removes `<i>`, `<b>`, `<u>`, `<font …>`, `<c.…>`, `<v …>` and `{\…}` tags; a lone `<` stays.
	static String strip_tags(const String &p_text);
	// Fills text, kind, speaker and off_screen of `r_segment` from the raw text of a cue.
	static void read_cue_text(const String &p_raw_text, const Ref<SubtitleSegment> &r_segment);
	// The text of a cue for a segment: decorations by kind, speaker prefix, `<i>` for off-screen.
	static String make_cue_text(const Ref<SubtitleSegment> &p_segment);
};

class ResourceFormatLoaderSubtitleSRT : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderSubtitleSRT, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverSubtitleSRT : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverSubtitleSRT, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};
