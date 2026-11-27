/**************************************************************************/
/*  test_audio_stream_tags.cpp                                            */
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

TEST_FORCE_LINK(test_audio_stream_tags)

#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "scene/resources/audio/audio_stream_tags.h"
#include "scene/resources/audio/audio_stream_wav.h"
#include "tests/test_utils.h"

namespace TestAudioStreamTags {

using Bytes = Vector<uint8_t>;

static void add_bytes(Bytes &r_bytes, std::initializer_list<uint8_t> p_values) {
	for (uint8_t v : p_values) {
		r_bytes.push_back(v);
	}
}

static void add(Bytes &r_bytes, const char *p_text, bool p_terminate = false) {
	for (const char *c = p_text; *c; c++) {
		r_bytes.push_back(uint8_t(*c));
	}
	if (p_terminate) {
		r_bytes.push_back(0);
	}
}

static void add(Bytes &r_bytes, const Bytes &p_more) {
	r_bytes.append_array(p_more);
}

static void add_be32(Bytes &r_bytes, uint32_t p_value) {
	add_bytes(r_bytes, { uint8_t(p_value >> 24), uint8_t(p_value >> 16), uint8_t(p_value >> 8), uint8_t(p_value) });
}

static void add_le32(Bytes &r_bytes, uint32_t p_value) {
	add_bytes(r_bytes, { uint8_t(p_value), uint8_t(p_value >> 8), uint8_t(p_value >> 16), uint8_t(p_value >> 24) });
}

static void add_syncsafe(Bytes &r_bytes, uint32_t p_value) {
	add_bytes(r_bytes, { uint8_t((p_value >> 21) & 0x7f), uint8_t((p_value >> 14) & 0x7f), uint8_t((p_value >> 7) & 0x7f), uint8_t(p_value & 0x7f) });
}

static Bytes utf8(const String &p_text) {
	const CharString cs = p_text.utf8();
	Bytes bytes;
	bytes.resize(cs.length());
	memcpy(bytes.ptrw(), cs.get_data(), cs.length());
	return bytes;
}

// UTF-16 with BOM (ID3 encoding 1).
static Bytes utf16_bom(const String &p_text) {
	const Char16String cs = p_text.utf16();
	Bytes bytes;
	add_bytes(bytes, { 0xff, 0xfe });
	for (int i = 0; i < cs.length(); i++) {
		add_bytes(bytes, { uint8_t(cs[i] & 0xff), uint8_t(cs[i] >> 8) });
	}
	return bytes;
}

static Bytes text_frame(uint8_t p_encoding, const Bytes &p_text) {
	Bytes payload;
	payload.push_back(p_encoding);
	add(payload, p_text);
	return payload;
}

static Bytes id3_frame(int p_version, const char *p_id, const Bytes &p_payload, uint8_t p_format_flags = 0) {
	Bytes frame;
	add(frame, p_id);
	if (p_version == 4) {
		add_syncsafe(frame, uint32_t(p_payload.size()));
	} else {
		add_be32(frame, uint32_t(p_payload.size()));
	}
	add_bytes(frame, { 0, p_format_flags });
	add(frame, p_payload);
	return frame;
}

static Bytes id3_tag(int p_version, const Bytes &p_frames, uint8_t p_flags = 0) {
	Bytes tag;
	add(tag, "ID3");
	add_bytes(tag, { uint8_t(p_version), 0, p_flags });
	add_syncsafe(tag, uint32_t(p_frames.size()));
	add(tag, p_frames);
	return tag;
}

static Bytes fake_png() {
	Bytes png;
	add_bytes(png, { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 1, 2, 3, 4, 0xff, 0xfe });
	return png;
}

static Ref<AudioStreamWAV> new_stream() {
	Ref<AudioStreamWAV> stream;
	stream.instantiate();
	return stream;
}

// ID3v2.3: UTF-16 title, Latin-1 artist, numbers "n/total", genre code, comment, front cover.
static Bytes id3v23_sample() {
	Bytes frames;
	add(frames, id3_frame(3, "TIT2", text_frame(1, utf16_bom(U"Grüße aus 東京"))));
	Bytes latin1;
	add_bytes(latin1, { 'C', 'a', 'f', 0xe9 });
	add(frames, id3_frame(3, "TPE1", text_frame(0, latin1)));
	add(frames, id3_frame(3, "TRCK", text_frame(0, utf8("3/12"))));
	add(frames, id3_frame(3, "TPOS", text_frame(0, utf8("1/2"))));
	add(frames, id3_frame(3, "TYER", text_frame(0, utf8("2001"))));
	add(frames, id3_frame(3, "TCON", text_frame(0, utf8("(17)"))));
	add(frames, id3_frame(3, "TXXX", text_frame(0, utf8("custom\0value"))));
	Bytes comment;
	add_bytes(comment, { 0 });
	add(comment, "eng");
	add_bytes(comment, { 0 }); // Empty description.
	add(comment, "Hello");
	add(frames, id3_frame(3, "COMM", comment));
	Bytes picture;
	add_bytes(picture, { 0 });
	add(picture, "image/png", true);
	add_bytes(picture, { 3 }); // Front cover.
	add(picture, "", true);
	add(picture, fake_png());
	add(frames, id3_frame(3, "APIC", picture));
	add_bytes(frames, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }); // Padding.
	return id3_tag(3, frames);
}

// ID3v2.4: UTF-8, several values, TDRC, numeric genre, described comment before the plain one, extended header.
static Bytes id3v24_sample() {
	Bytes frames;
	add(frames, id3_frame(4, "TIT2", text_frame(3, utf8(U"Ünïcode ✓"))));
	Bytes artists = utf8("Alpha");
	artists.push_back(0);
	add(artists, utf8("Beta"));
	add(frames, id3_frame(4, "TPE1", text_frame(3, artists)));
	add(frames, id3_frame(4, "TPE2", text_frame(3, utf8("Various"))));
	add(frames, id3_frame(4, "TDRC", text_frame(3, utf8("2024-05-01"))));
	add(frames, id3_frame(4, "TCON", text_frame(3, utf8("17"))));
	add(frames, id3_frame(4, "TCOM", text_frame(3, utf8("Composer"))));
	add(frames, id3_frame(4, "TCOP", text_frame(3, utf8("(c) Someone"))));
	Bytes itunes;
	add_bytes(itunes, { 3 });
	add(itunes, "eng");
	add(itunes, "iTunNORM", true);
	add(itunes, " 0000 0000");
	add(frames, id3_frame(4, "COMM", itunes));
	Bytes comment;
	add_bytes(comment, { 3 });
	add(comment, "eng");
	add_bytes(comment, { 0 });
	add(comment, utf8(U"Grüße"));
	add(frames, id3_frame(4, "COMM", comment));
	Bytes extended;
	add_syncsafe(extended, 6); // Extended header size including itself.
	add_bytes(extended, { 1, 0 });
	add(extended, frames);
	return id3_tag(4, extended, 0x40);
}

static Bytes id3v1_sample() {
	Bytes tag;
	tag.resize(128);
	memset(tag.ptrw(), 0, 128);
	uint8_t *w = tag.ptrw();
	memcpy(w, "TAG", 3);
	memcpy(w + 3, "Old Title", 9);
	memcpy(w + 33, "Old Artist", 10);
	memcpy(w + 63, "Old Album", 9);
	memcpy(w + 93, "1987", 4);
	memcpy(w + 97, "Old comment", 11);
	w[126] = 5; // ID3v1.1 track.
	w[127] = 8; // Jazz.
	return tag;
}

static Bytes riff_chunk(const char *p_id, const Bytes &p_body) {
	Bytes chunk;
	add(chunk, p_id);
	add_le32(chunk, uint32_t(p_body.size()));
	add(chunk, p_body);
	if (p_body.size() & 1) {
		chunk.push_back(0);
	}
	return chunk;
}

static Bytes wav_sample() {
	Bytes fmt;
	add_bytes(fmt, { 1, 0, 1, 0 }); // PCM, mono.
	add_le32(fmt, 8000);
	add_le32(fmt, 16000);
	add_bytes(fmt, { 2, 0, 16, 0 });
	Bytes data;
	data.resize(1600); // 800 samples, 0.1 seconds.
	memset(data.ptrw(), 0, data.size());
	Bytes info;
	add(info, "INFO");
	add(info, riff_chunk("INAM", utf8("Wave Title\0")));
	add(info, riff_chunk("IART", utf8(U"Wäve Ärtist")));
	add(info, riff_chunk("ITRK", utf8("7")));
	add(info, riff_chunk("ICRD", utf8("1999-01-01")));
	add(info, riff_chunk("IGNR", utf8("Jazz")));
	Bytes riff;
	add(riff, "WAVE");
	add(riff, riff_chunk("fmt ", fmt));
	add(riff, riff_chunk("LIST", info));
	add(riff, riff_chunk("data", data));
	add(riff, riff_chunk("id3 ", id3_tag(4, id3_frame(4, "TALB", text_frame(3, utf8(U"Älbum ✓"))))));
	Bytes file;
	add(file, "RIFF");
	add_le32(file, uint32_t(riff.size()));
	add(file, riff);
	return file;
}

static Bytes flac_picture(const Bytes &p_data) {
	Bytes block;
	add_be32(block, 3);
	add_be32(block, 9);
	add(block, "image/png");
	add_be32(block, 0);
	add_be32(block, 8);
	add_be32(block, 8);
	add_be32(block, 24);
	add_be32(block, 0);
	add_be32(block, uint32_t(p_data.size()));
	add(block, p_data);
	return block;
}

TEST_CASE("[Audio][AudioStreamTags] Fields are empty by default and set by hand") {
	Ref<AudioStreamWAV> stream = new_stream();
	CHECK(stream->get_title().is_empty());
	CHECK(stream->get_track_number() == 0);
	CHECK(stream->get_cover().is_null());
	CHECK(stream->get_cover_data().is_empty());
	CHECK(stream->get_codec() == "PCM");

	stream->set_title("Title");
	stream->set_track_number(4);
	stream->set_comment(U"Grüße");
	CHECK(stream->get_title() == "Title");
	CHECK(stream->get_track_number() == 4);
	CHECK(stream->get_comment() == U"Grüße");
	stream->clear_track_info();
	CHECK(stream->get_title().is_empty());

	// A cover read from the file is stored as `cover_data`, not as a second texture.
	stream->set_cover_data(fake_png());
	List<PropertyInfo> properties;
	stream->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.name == "cover") {
			CHECK(property.usage == PROPERTY_USAGE_EDITOR);
		}
	}
	const String size_message = vformat("sizeof(AudioStream) = %d bytes (8 of them for the track info pointer).", int(sizeof(AudioStream)));
	MESSAGE(size_message);
}

TEST_CASE("[Audio][AudioStreamTags] ID3v2.3 with UTF-16, Latin-1, numbers, genre code, comment and cover") {
	Ref<AudioStreamWAV> stream = new_stream();
	const Bytes tag = id3v23_sample();
	CHECK(AudioStreamTags::get_id3v2_size(tag.ptr(), tag.size()) == tag.size());
	AudioStreamTags::read_id3v2(stream.ptr(), tag.ptr(), tag.size());
	CHECK(stream->get_title() == U"Grüße aus 東京");
	CHECK(stream->get_artist() == U"Café");
	CHECK(stream->get_track_number() == 3);
	CHECK(stream->get_track_count() == 12);
	CHECK(stream->get_disc_number() == 1);
	CHECK(stream->get_disc_count() == 2);
	CHECK(stream->get_year() == 2001);
	CHECK(stream->get_genre() == "Rock");
	CHECK(stream->get_comment() == "Hello");
	CHECK(stream->get_cover_data() == fake_png());
}

TEST_CASE("[Audio][AudioStreamTags] ID3v2.4 with UTF-8, several values, extended header and described comments") {
	Ref<AudioStreamWAV> stream = new_stream();
	const Bytes tag = id3v24_sample();
	AudioStreamTags::read_id3v2(stream.ptr(), tag.ptr(), tag.size());
	CHECK(stream->get_title() == U"Ünïcode ✓");
	CHECK(stream->get_artist() == "Alpha, Beta");
	CHECK(stream->get_album_artist() == "Various");
	CHECK(stream->get_year() == 2024);
	CHECK(stream->get_genre() == "Rock");
	CHECK(stream->get_composer() == "Composer");
	CHECK(stream->get_copyright() == "(c) Someone");
	CHECK(stream->get_comment() == U"Grüße");
}

TEST_CASE("[Audio][AudioStreamTags] ID3v2.3 unsynchronisation") {
	Bytes picture;
	add_bytes(picture, { 0 });
	add(picture, "image/jpeg", true);
	add_bytes(picture, { 3, 0 });
	Bytes expected;
	add_bytes(expected, { 0xff, 0xd8, 0xff, 0xe0, 1, 2 });
	add(picture, expected);
	// ID3v2.3 unsynchronises the whole tag after writing it: a 0x00 follows every 0xFF.
	const Bytes frame = id3_frame(3, "APIC", picture);
	Bytes unsynchronized;
	for (uint8_t byte : frame) {
		unsynchronized.push_back(byte);
		if (byte == 0xff) {
			unsynchronized.push_back(0);
		}
	}
	Ref<AudioStreamWAV> stream = new_stream();
	const Bytes tag = id3_tag(3, unsynchronized, 0x80);
	AudioStreamTags::read_id3v2(stream.ptr(), tag.ptr(), tag.size());
	CHECK(stream->get_cover_data() == expected);
}

TEST_CASE("[Audio][AudioStreamTags] ID3v1 fallback, ID3v2 wins") {
	Bytes file = id3v23_sample();
	file.resize_initialized(file.size() + 1000); // Audio.
	add(file, id3v1_sample());
	Ref<AudioStreamWAV> only_v1 = new_stream();
	CHECK(AudioStreamTags::read_id3v1(only_v1.ptr(), file.ptr(), file.size()));
	CHECK(only_v1->get_title() == "Old Title");
	CHECK(only_v1->get_artist() == "Old Artist");
	CHECK(only_v1->get_album() == "Old Album");
	CHECK(only_v1->get_year() == 1987);
	CHECK(only_v1->get_comment() == "Old comment");
	CHECK(only_v1->get_track_number() == 5);
	CHECK(only_v1->get_genre() == "Jazz");

	Ref<AudioStreamWAV> both = new_stream();
	AudioStreamTags::read_tags(both.ptr(), file);
	CHECK(both->get_title() == U"Grüße aus 東京");
	CHECK(both->get_album() == "Old Album"); // Only in ID3v1.
	CHECK(both->get_genre() == "Rock");
}

TEST_CASE("[Audio][AudioStreamTags] WAV LIST/INFO and id3 chunk") {
	Ref<AudioStreamWAV> stream = new_stream();
	const Bytes file = wav_sample();
	AudioStreamTags::read_riff(stream.ptr(), file.ptr(), file.size());
	CHECK(stream->get_title() == "Wave Title");
	CHECK(stream->get_artist() == U"Wäve Ärtist");
	CHECK(stream->get_track_number() == 7);
	CHECK(stream->get_year() == 1999);
	CHECK(stream->get_genre() == "Jazz");
	CHECK(stream->get_album() == U"Älbum ✓");
}

TEST_CASE("[Audio][AudioStreamTags] Vorbis comments with picture") {
	Dictionary comments;
	comments["title"] = U"Ögg Title";
	comments["artist"] = "Artist";
	comments["albumartist"] = "Album Artist";
	comments["tracknumber"] = "2";
	comments["tracktotal"] = "9";
	comments["discnumber"] = "1/3";
	comments["date"] = "2010";
	comments["genre"] = "(RX)";
	comments["description"] = "Notes";
	const Bytes block = flac_picture(fake_png());
	comments["metadata_block_picture"] = CryptoCore::b64_encode_str(block.ptr(), block.size());
	Ref<AudioStreamWAV> stream = new_stream();
	AudioStreamTags::read_comments(stream.ptr(), comments);
	CHECK(stream->get_title() == U"Ögg Title");
	CHECK(stream->get_album_artist() == "Album Artist");
	CHECK(stream->get_track_number() == 2);
	CHECK(stream->get_track_count() == 9);
	CHECK(stream->get_disc_number() == 1);
	CHECK(stream->get_disc_count() == 3);
	CHECK(stream->get_year() == 2010);
	CHECK(stream->get_genre() == "Remix");
	CHECK(stream->get_comment() == "Notes");
	CHECK(stream->get_cover_data() == fake_png());
}

TEST_CASE("[Audio][AudioStreamTags] Genre names") {
	CHECK(AudioStreamTags::get_genre_name("(17)") == "Rock");
	CHECK(AudioStreamTags::get_genre_name("17") == "Rock");
	CHECK(AudioStreamTags::get_genre_name("(17)Rock Remix") == "Rock Remix");
	CHECK(AudioStreamTags::get_genre_name("(CR)") == "Cover");
	CHECK(AudioStreamTags::get_genre_name("(999)") == "(999)");
	CHECK(AudioStreamTags::get_genre_name(" Synthwave ") == "Synthwave");
}

TEST_CASE("[Audio][AudioStreamTags] Truncated and broken data is read without overreads") {
	const Bytes samples[] = { id3v23_sample(), id3v24_sample(), wav_sample() };
	for (const Bytes &sample : samples) {
		for (int64_t size = 0; size < sample.size(); size++) {
			Bytes truncated = sample;
			truncated.resize(size);
			Ref<AudioStreamWAV> stream = new_stream();
			AudioStreamTags::read_tags(stream.ptr(), truncated);
		}
	}
	// Sizes that point far outside the buffer.
	Bytes broken = id3_tag(3, id3_frame(3, "TIT2", text_frame(0, utf8("Title"))));
	broken.write[14] = 0x7f; // Frame size high byte.
	Ref<AudioStreamWAV> stream = new_stream();
	AudioStreamTags::read_tags(stream.ptr(), broken);
	CHECK(stream->get_title().is_empty());
	Bytes riff;
	add(riff, "RIFF");
	add_le32(riff, 0xffffffff);
	add(riff, "WAVE");
	add(riff, "LIST");
	add_le32(riff, 0xfffffff0);
	add(riff, "INFOINAM");
	add_le32(riff, 0xfffffff0);
	add(riff, "Cut");
	AudioStreamTags::read_tags(stream.ptr(), riff);
	CHECK(stream->get_title() == "Cut");
	Dictionary comments;
	comments["metadata_block_picture"] = "AAAAAw=="; // Picture block cut after the type.
	AudioStreamTags::read_comments(stream.ptr(), comments);
	CHECK(stream->get_cover_data().is_empty());
}

TEST_CASE("[Audio][AudioStreamTags] load_info reads tags and format without audio data") {
	const String path = TestUtils::get_temp_path("track_info.wav");
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	const Bytes wav = wav_sample();
	file->store_buffer(wav.ptr(), wav.size());
	file->close();

	Ref<AudioStream> info = AudioStream::load_info(path);
	REQUIRE(info.is_valid());
	CHECK(info->is_class("AudioStreamWAV"));
	CHECK(Object::cast_to<AudioStreamWAV>(info.ptr())->get_data().is_empty());
	CHECK(info->get_title() == "Wave Title");
	CHECK(info->get_album() == U"Älbum ✓");
	CHECK(info->get_sample_rate() == 8000);
	CHECK(info->get_channel_count() == 1);
	CHECK(info->get_bitrate() == 128000);
	CHECK(info->get_length() == doctest::Approx(0.1));

	// The full load reads the same fields from the same bytes.
	Ref<AudioStreamWAV> loaded = AudioStreamWAV::load_from_buffer(wav, Dictionary());
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_title() == "Wave Title");
	CHECK(loaded->get_album() == U"Älbum ✓");
	CHECK(loaded->get_sample_rate() == 8000);
	CHECK(loaded->get_length() == doctest::Approx(0.1));
}

#ifdef MODULE_MP3_ENABLED
TEST_CASE("[Audio][AudioStreamTags] load_info of an MP3 reads the ID3 tags and the first frame header") {
	Bytes mp3 = id3v23_sample();
	const int64_t tag_size = mp3.size();
	for (int i = 0; i < 10; i++) {
		add_bytes(mp3, { 0xff, 0xfb, 0x90, 0x64 }); // MPEG 1 Layer III, 128 kbit/s, 44100 Hz, joint stereo.
		mp3.resize_initialized(mp3.size() + 413); // Frame of 417 bytes.
	}
	const String path = TestUtils::get_temp_path("track_info.mp3");
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_buffer(mp3.ptr(), mp3.size());
	file->close();

	Ref<AudioStream> info = AudioStream::load_info(path);
	REQUIRE(info.is_valid());
	CHECK(info->is_class("AudioStreamMP3"));
	CHECK(info->get_title() == U"Grüße aus 東京");
	CHECK(info->get_codec() == "MP3");
	CHECK(info->get_sample_rate() == 44100);
	CHECK(info->get_channel_count() == 2);
	CHECK(info->get_bitrate() == 128000);
	CHECK(info->get_length() == doctest::Approx((mp3.size() - tag_size) * 8.0 / 128000));
}
#endif

} // namespace TestAudioStreamTags
