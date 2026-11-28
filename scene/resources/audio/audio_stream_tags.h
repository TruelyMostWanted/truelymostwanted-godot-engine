/**************************************************************************/
/*  audio_stream_tags.h                                                   */
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

#include "core/io/resource_importer.h"
#include "scene/resources/audio/audio_stream.h"

// Reads the track information of audio files into the fields of an AudioStream (title, artist, cover ...):
// ID3v2.3/2.4 and ID3v1 (MP3, `id3 ` chunk of WAV), RIFF LIST/INFO (WAV) and Vorbis comments (Ogg Vorbis, Opus).
// Pure byte parsing for all platforms; every size is checked against the buffer, truncated tags are read as far as they go.
class AudioStreamTags {
public:
	// Fills the fields of `p_stream` from the bytes of its file. Ogg files use the comments already parsed into `get_tags()`.
	static void read_tags(AudioStream *p_stream, const Vector<uint8_t> &p_file_data);
	// Called by the loaders (`load_from_buffer()`): reads the tags when `audio/info/read_file_tags` is on.
	static void read_file_tags(AudioStream *p_stream, const Vector<uint8_t> &p_file_data);

	// Returns the size of the ID3v2 tag at the start of `p_data` (header and footer included), 0 without a tag.
	static int64_t get_id3v2_size(const uint8_t *p_data, int64_t p_size);
	static void read_id3v2(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size);
	// Reads an ID3v1 tag from the last 128 bytes of `p_data`. Returns false without a tag.
	static bool read_id3v1(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size);
	// Reads the LIST/INFO and `id3 ` chunks of a RIFF (WAV) file.
	static void read_riff(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size);
	// Vorbis comments with lowercase keys (as in `AudioStreamOggVorbis.tags`).
	static void read_comments(AudioStream *p_stream, const Dictionary &p_comments);
	// Sets one field from a Vorbis comment name (`title`, `tracknumber`, `metadata_block_picture` ...). Empty values are ignored.
	static void set_field(AudioStream *p_stream, const String &p_key, const String &p_value);
	// "(17)", "17" -> "Rock"; "(17)Rock Remix" -> "Rock Remix"; other text unchanged.
	static String get_genre_name(const String &p_genre);

	static Ref<Texture2D> decode_picture(const Vector<uint8_t> &p_data);
	static Ref<AudioStream> load_info(const String &p_path);

	// Import option group "info/": `read_file_tags` and one override per field (empty or 0 = from the file).
	static void get_import_options(List<ResourceImporter::ImportOption> *r_options);
	static void apply_import_options(AudioStream *p_stream, const String &p_source_file, const HashMap<StringName, Variant> &p_options);
};
