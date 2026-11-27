/**************************************************************************/
/*  audio_stream_tags.cpp                                                 */
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

#include "audio_stream_tags.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "scene/resources/image_texture.h"
#include "servers/subtitle/subtitle.h"

namespace {

uint32_t be32(const uint8_t *p) {
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint32_t le32(const uint8_t *p) {
	return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint32_t syncsafe32(const uint8_t *p) {
	return (uint32_t(p[0] & 0x7f) << 21) | (uint32_t(p[1] & 0x7f) << 14) | (uint32_t(p[2] & 0x7f) << 7) | uint32_t(p[3] & 0x7f);
}

bool is_utf8(const uint8_t *p, int64_t n) {
	for (int64_t i = 0; i < n;) {
		const uint8_t c = p[i];
		const int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1
				: (c >> 4) == 0xe						 ? 2
				: (c >> 3) == 0x1e						 ? 3
														 : -1;
		if (extra < 0 || extra >= n - i) {
			return false;
		}
		for (int k = 1; k <= extra; k++) {
			if ((p[i + k] & 0xc0) != 0x80) {
				return false;
			}
		}
		i += extra + 1;
	}
	return true;
}

// ID3v2 text encodings: 0 ISO-8859-1 (UTF-8 is accepted too, some taggers write it), 1 UTF-16 with BOM, 2 UTF-16BE, 3 UTF-8.
int64_t char_size(uint8_t p_encoding) {
	return (p_encoding == 1 || p_encoding == 2) ? 2 : 1;
}

// Length in bytes of the first string up to its terminator (or the end).
int64_t string_length(uint8_t p_encoding, const uint8_t *p, int64_t n) {
	const int64_t step = char_size(p_encoding);
	for (int64_t i = 0; i + step <= n; i += step) {
		if (p[i] == 0 && (step == 1 || p[i + 1] == 0)) {
			return i;
		}
	}
	return n;
}

String decode(uint8_t p_encoding, const uint8_t *p, int64_t n) {
	if (char_size(p_encoding) == 2) {
		bool big_endian = p_encoding == 2;
		if (n >= 2 && ((p[0] == 0xff && p[1] == 0xfe) || (p[0] == 0xfe && p[1] == 0xff))) {
			big_endian = p[0] == 0xfe;
			p += 2;
			n -= 2;
		}
		Vector<char16_t> chars;
		chars.resize(n / 2);
		char16_t *w = chars.ptrw();
		for (int64_t i = 0; i < n / 2; i++) {
			w[i] = big_endian ? char16_t((p[2 * i] << 8) | p[2 * i + 1]) : char16_t(p[2 * i] | (p[2 * i + 1] << 8));
		}
		return chars.is_empty() ? String() : String::utf16(chars.ptr(), int(chars.size()));
	}
	if (p_encoding == 3 || is_utf8(p, n)) {
		return String::utf8((const char *)p, int(n));
	}
	return String::latin1(Span<char>((const char *)p, n));
}

// Text that may hold several terminated values (ID3v2.4), joined with ", ".
String decode_text(uint8_t p_encoding, const uint8_t *p, int64_t n) {
	String result;
	while (n > 0) {
		const int64_t length = string_length(p_encoding, p, n);
		const String part = decode(p_encoding, p, length).strip_edges();
		if (!part.is_empty()) {
			if (!result.is_empty()) {
				result += ", ";
			}
			result += part;
		}
		const int64_t skip = MIN(n, length + char_size(p_encoding));
		p += skip;
		n -= skip;
	}
	return result;
}

Vector<uint8_t> remove_unsynchronisation(const uint8_t *p, int64_t n) {
	Vector<uint8_t> out;
	out.resize(n);
	uint8_t *w = out.ptrw();
	int64_t count = 0;
	for (int64_t i = 0; i < n; i++) {
		if (i > 0 && p[i] == 0 && p[i - 1] == 0xff) {
			continue;
		}
		w[count++] = p[i];
	}
	out.resize(count);
	return out;
}

void read_flac_picture(AudioStream *p_stream, const uint8_t *p, int64_t n) {
	int64_t pos = 4; // Picture type.
	for (int k = 0; k < 2; k++) { // MIME type and description.
		if (pos + 4 > n) {
			return;
		}
		const int64_t length = be32(p + pos);
		pos += 4;
		if (length > n - pos) {
			return;
		}
		pos += length;
	}
	pos += 16; // Width, height, color depth, color count.
	if (pos + 4 > n) {
		return;
	}
	const int64_t length = be32(p + pos);
	pos += 4;
	if (length == 0 || length > n - pos) {
		return;
	}
	Vector<uint8_t> data;
	data.resize(length);
	memcpy(data.ptrw(), p + pos, length);
	p_stream->set_cover_data(data);
}

void read_riff_info(AudioStream *p_stream, const uint8_t *p, int64_t n) {
	static const char *const ids[][2] = {
		{ "INAM", "title" }, { "IART", "artist" }, { "IPRD", "album" }, { "ITRK", "tracknumber" }, { "IPRT", "tracknumber" },
		{ "ICRD", "date" }, { "IGNR", "genre" }, { "ICMT", "comment" }, { "ICOP", "copyright" }, { "IMUS", "composer" }
	};
	for (int64_t pos = 0; pos + 8 <= n;) {
		const uint8_t *chunk = p + pos;
		const int64_t length = MIN(int64_t(le32(chunk + 4)), n - pos - 8);
		for (const auto &id : ids) {
			if (memcmp(chunk, id[0], 4) == 0) {
				AudioStreamTags::set_field(p_stream, id[1], decode(0, chunk + 8, string_length(0, chunk + 8, length)).strip_edges());
			}
		}
		pos += 8 + length + (length & 1);
	}
}

void read_riff_chunk(AudioStream *p_stream, const uint8_t *p_id, const uint8_t *p, int64_t n) {
	if (memcmp(p_id, "LIST", 4) == 0 && n >= 4 && memcmp(p, "INFO", 4) == 0) {
		read_riff_info(p_stream, p + 4, n - 4);
	} else if (memcmp(p_id, "id3 ", 4) == 0 || memcmp(p_id, "ID3 ", 4) == 0) {
		AudioStreamTags::read_id3v2(p_stream, p, n);
	}
}

Dictionary parse_vorbis_comments(const uint8_t *p, int64_t n) {
	Dictionary comments;
	if (n < 8) {
		return comments;
	}
	const int64_t vendor = le32(p);
	if (vendor > n - 8) {
		return comments;
	}
	int64_t pos = 4 + vendor;
	const uint32_t count = le32(p + pos);
	pos += 4;
	for (uint32_t i = 0; i < count && pos + 4 <= n; i++) {
		const int64_t length = le32(p + pos);
		pos += 4;
		if (length > n - pos) {
			break;
		}
		const String comment = String::utf8((const char *)p + pos, int(length));
		pos += length;
		const int equals = comment.find_char('=');
		if (equals > 0) {
			comments[comment.substr(0, equals).to_lower()] = comment.substr(equals + 1);
		}
	}
	return comments;
}

const char *const genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies",
	"Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks",
	"Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
	"Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative",
	"Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk",
	"Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
	"Native American", "Cabaret", "New Wave", "Psychedelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk",
	"Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk", "Folk-Rock", "National Folk", "Swing",
	"Fast Fusion", "Bebop", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock",
	"Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humor", "Speech",
	"Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam",
	"Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock",
	"Drum Solo", "A Cappella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
	"BritPop", "Afro-Punk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover",
	"Contemporary Christian", "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop"
};

String genre_name(int p_code) {
	return (p_code >= 0 && p_code < int(sizeof(genres) / sizeof(genres[0]))) ? String(genres[p_code]) : String();
}

void set_number(AudioStream *p_stream, const String &p_value, void (AudioStream::*p_number)(int), void (AudioStream::*p_count)(int)) {
	// "3" or "3/12".
	(p_stream->*p_number)(p_value.get_slicec('/', 0).strip_edges().to_int());
	if (p_value.contains_char('/')) {
		(p_stream->*p_count)(p_value.get_slicec('/', 1).strip_edges().to_int());
	}
}

Vector<uint8_t> read_at(const Ref<FileAccess> &p_file, uint64_t p_position, int64_t p_length) {
	p_file->seek(p_position);
	return p_file->get_buffer(p_length);
}

Ref<AudioStream> instantiate_stream(const StringName &p_class) {
	ERR_FAIL_COND_V_MSG(!ClassDB::class_exists(p_class), Ref<AudioStream>(), vformat("%s is not available in this build.", p_class));
	return Ref<AudioStream>(Object::cast_to<AudioStream>(ClassDB::instantiate(p_class)));
}

Ref<AudioStream> load_mp3_info(const Ref<FileAccess> &p_file, uint64_t p_size) {
	Ref<AudioStream> stream = instantiate_stream("AudioStreamMP3");
	if (stream.is_null()) {
		return stream;
	}
	const Vector<uint8_t> head = read_at(p_file, 0, 10);
	const int64_t tag_size = AudioStreamTags::get_id3v2_size(head.ptr(), head.size());
	int64_t audio_end = p_size;
	if (p_size >= 128) {
		const Vector<uint8_t> v1 = read_at(p_file, p_size - 128, 128);
		if (AudioStreamTags::read_id3v1(stream.ptr(), v1.ptr(), v1.size())) {
			audio_end -= 128;
		}
	}
	if (tag_size > 0) {
		const Vector<uint8_t> tag = read_at(p_file, 0, tag_size);
		AudioStreamTags::read_id3v2(stream.ptr(), tag.ptr(), tag.size());
	}

	// First MPEG Layer III frame header, plus the Xing/Info frame of VBR files for the exact length.
	static const int bitrates[2][15] = {
		{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
		{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
	};
	static const int sample_rates[3] = { 44100, 48000, 32000 };
	const Vector<uint8_t> frames = read_at(p_file, tag_size, 4096);
	const uint8_t *p = frames.ptr();
	for (int64_t i = 0; i + 4 <= frames.size(); i++) {
		const uint8_t *h = p + i;
		const int version = (h[1] >> 3) & 3; // 3 = MPEG 1, 2 = MPEG 2, 0 = MPEG 2.5.
		const int layer = (h[1] >> 1) & 3; // 1 = Layer III.
		const int bitrate_index = h[2] >> 4;
		const int rate_index = (h[2] >> 2) & 3;
		if (h[0] != 0xff || (h[1] & 0xe0) != 0xe0 || version == 1 || layer != 1 || bitrate_index == 0 || bitrate_index == 15 || rate_index == 3) {
			continue;
		}
		const bool mpeg1 = version == 3;
		const int sample_rate = sample_rates[rate_index] >> (mpeg1 ? 0 : (version == 2 ? 1 : 2));
		const int channels = (h[3] >> 6) == 3 ? 1 : 2;
		int bitrate = bitrates[mpeg1 ? 0 : 1][bitrate_index] * 1000;
		const int64_t audio_bytes = audio_end - tag_size - i;
		double length = audio_bytes * 8.0 / bitrate;
		const int64_t xing = i + 4 + (mpeg1 ? (channels == 1 ? 17 : 32) : (channels == 1 ? 9 : 17));
		if (xing + 12 <= frames.size() && (memcmp(p + xing, "Xing", 4) == 0 || memcmp(p + xing, "Info", 4) == 0) && (be32(p + xing + 4) & 1)) {
			length = double(be32(p + xing + 8)) * (mpeg1 ? 1152 : 576) / sample_rate;
			if (length > 0) {
				bitrate = int(audio_bytes * 8 / length);
			}
		}
		stream->set_format_info(length, sample_rate, channels, bitrate);
		break;
	}
	return stream;
}

Ref<AudioStream> load_wav_info(const Ref<FileAccess> &p_file, uint64_t p_size) {
	Ref<AudioStream> stream = instantiate_stream("AudioStreamWAV");
	if (stream.is_null()) {
		return stream;
	}
	int channels = 0;
	int sample_rate = 0;
	int byte_rate = 0;
	int block_align = 0;
	int64_t data_size = 0;
	for (uint64_t pos = 12; pos + 8 <= p_size;) {
		const Vector<uint8_t> header = read_at(p_file, pos, 8);
		const int64_t length = MIN(int64_t(le32(header.ptr() + 4)), int64_t(p_size - pos - 8));
		if (memcmp(header.ptr(), "data", 4) == 0) {
			data_size = length; // The audio itself is skipped.
		} else if (memcmp(header.ptr(), "fmt ", 4) == 0 || memcmp(header.ptr(), "LIST", 4) == 0 || memcmp(header.ptr(), "id3 ", 4) == 0 || memcmp(header.ptr(), "ID3 ", 4) == 0) {
			const Vector<uint8_t> body = p_file->get_buffer(length);
			if (header[0] == 'f' && body.size() >= 16) {
				channels = body[2] | (body[3] << 8);
				sample_rate = int(le32(body.ptr() + 4));
				byte_rate = int(le32(body.ptr() + 8));
				block_align = body[12] | (body[13] << 8);
			} else {
				read_riff_chunk(stream.ptr(), header.ptr(), body.ptr(), body.size());
			}
		}
		pos += 8 + length + (length & 1);
	}
	const double length = (block_align > 0 && sample_rate > 0) ? double(data_size / block_align) / sample_rate : 0.0;
	stream->set_format_info(length, sample_rate, channels, byte_rate * 8);
	return stream;
}

// Header packets of the first logical stream of an Ogg file.
void read_ogg_packets(const Ref<FileAccess> &p_file, uint64_t p_size, uint32_t p_count, LocalVector<Vector<uint8_t>> &r_packets, uint32_t &r_serial) {
	const int64_t max_packet_size = 64 * 1024 * 1024; // Covers big embedded pictures, stops on broken files.
	Vector<uint8_t> packet;
	bool first_page = true;
	for (uint64_t pos = 0; r_packets.size() < p_count && pos + 27 <= p_size;) {
		const Vector<uint8_t> header = read_at(p_file, pos, 27);
		if (header.size() < 27 || memcmp(header.ptr(), "OggS", 4) != 0) {
			return;
		}
		const uint32_t serial = le32(header.ptr() + 14);
		const Vector<uint8_t> lacing = p_file->get_buffer(header[26]);
		if (lacing.size() < header[26]) {
			return;
		}
		int64_t page_size = 0;
		for (const uint8_t segment : lacing) {
			page_size += segment;
		}
		if (first_page) {
			r_serial = serial;
			first_page = false;
		}
		if (serial == r_serial) {
			const Vector<uint8_t> page = p_file->get_buffer(page_size);
			if (page.size() < page_size || packet.size() + page_size > max_packet_size) {
				return;
			}
			int64_t offset = 0;
			for (int64_t i = 0; i < lacing.size() && r_packets.size() < p_count; i++) {
				const int64_t old_size = packet.size();
				packet.resize(old_size + lacing[i]);
				memcpy(packet.ptrw() + old_size, page.ptr() + offset, lacing[i]);
				offset += lacing[i];
				if (lacing[i] < 255) {
					r_packets.push_back(packet);
					packet.clear();
				}
			}
		}
		pos += 27 + lacing.size() + page_size;
	}
}

// Granule position of the last page of the stream (samples at the end).
int64_t read_ogg_last_granule(const Ref<FileAccess> &p_file, uint64_t p_size, uint32_t p_serial) {
	const uint64_t start = p_size > 65536 ? p_size - 65536 : 0;
	const Vector<uint8_t> tail = read_at(p_file, start, p_size - start);
	for (int64_t i = tail.size() - 27; i >= 0; i--) {
		const uint8_t *p = tail.ptr() + i;
		if (memcmp(p, "OggS", 4) == 0 && le32(p + 14) == p_serial) {
			const int64_t granule = int64_t(uint64_t(le32(p + 6)) | (uint64_t(le32(p + 10)) << 32));
			if (granule >= 0) {
				return granule;
			}
		}
	}
	return 0;
}

Ref<AudioStream> load_ogg_info(const Ref<FileAccess> &p_file, uint64_t p_size) {
	LocalVector<Vector<uint8_t>> packets;
	uint32_t serial = 0;
	read_ogg_packets(p_file, p_size, 2, packets, serial);
	if (packets.size() < 2) {
		return Ref<AudioStream>();
	}
	const Vector<uint8_t> &id = packets[0];
	const Vector<uint8_t> &comments = packets[1];
	const int64_t granule = read_ogg_last_granule(p_file, p_size, serial);
	Ref<AudioStream> stream;
	double length = 0.0;
	if (id.size() >= 19 && memcmp(id.ptr(), "OpusHead", 8) == 0 && comments.size() >= 8 && memcmp(comments.ptr(), "OpusTags", 8) == 0) {
		stream = instantiate_stream("AudioStreamOpus");
		if (stream.is_valid()) {
			const int pre_skip = id[10] | (id[11] << 8);
			length = MAX(granule - pre_skip, int64_t(0)) / 48000.0;
			stream->set_format_info(length, 48000, id[9], length > 0 ? int(p_size * 8 / length) : 0);
			AudioStreamTags::read_comments(stream.ptr(), parse_vorbis_comments(comments.ptr() + 8, comments.size() - 8));
		}
	} else if (id.size() >= 30 && id[0] == 1 && memcmp(id.ptr() + 1, "vorbis", 6) == 0 && comments.size() >= 7 && comments[0] == 3 && memcmp(comments.ptr() + 1, "vorbis", 6) == 0) {
		stream = instantiate_stream("AudioStreamOggVorbis");
		if (stream.is_valid()) {
			const int sample_rate = int(le32(id.ptr() + 12));
			length = sample_rate > 0 ? double(granule) / sample_rate : 0.0;
			int bitrate = int32_t(le32(id.ptr() + 20)); // Nominal bitrate.
			if (bitrate <= 0) {
				bitrate = length > 0 ? int(p_size * 8 / length) : 0;
			}
			stream->set_format_info(length, sample_rate, id[11], bitrate);
			AudioStreamTags::read_comments(stream.ptr(), parse_vorbis_comments(comments.ptr() + 7, comments.size() - 7));
		}
	}
	return stream;
}

const char *const info_fields[] = {
	"title", "artist", "album", "album_artist", "track_number", "track_count", "disc_number", "disc_count", "year", "genre", "composer", "copyright", "comment"
};

} // namespace

int64_t AudioStreamTags::get_id3v2_size(const uint8_t *p_data, int64_t p_size) {
	const uint8_t *p = p_data;
	if (p_size < 10 || p[0] != 'I' || p[1] != 'D' || p[2] != '3' || p[3] == 0xff || p[4] == 0xff || ((p[6] | p[7] | p[8] | p[9]) & 0x80)) {
		return 0;
	}
	return 10 + int64_t(syncsafe32(p + 6)) + ((p[5] & 0x10) ? 10 : 0);
}

void AudioStreamTags::read_id3v2(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size) {
	const int64_t tag_size = get_id3v2_size(p_data, p_size);
	if (tag_size == 0 || p_data[3] < 3 || p_data[3] > 4) {
		return; // No tag, or ID3v2.2 (three-character frame IDs).
	}
	const uint8_t version = p_data[3];
	const uint8_t flags = p_data[5];
	const uint8_t *p = p_data + 10;
	int64_t n = MAX(MIN(tag_size, p_size) - 10 - ((flags & 0x10) ? 10 : 0), int64_t(0));
	Vector<uint8_t> clean;
	if (version == 3 && (flags & 0x80)) { // ID3v2.3 unsynchronises the whole tag, ID3v2.4 each frame.
		clean = remove_unsynchronisation(p, n);
		p = clean.ptr();
		n = clean.size();
	}
	int64_t pos = 0;
	if ((flags & 0x40) && n >= 4) { // Extended header.
		pos = version == 3 ? 4 + int64_t(be32(p)) : int64_t(syncsafe32(p));
	}
	int comment_rank = 0;
	int picture_rank = 0;
	while (pos + 10 <= n) {
		const uint8_t *frame = p + pos;
		if (frame[0] == 0) {
			break; // Padding.
		}
		const int64_t frame_size = version == 4 ? int64_t(syncsafe32(frame + 4)) : int64_t(be32(frame + 4));
		const uint8_t frame_flags = frame[9];
		pos += 10;
		if (frame_size > n - pos) {
			break;
		}
		pos += frame_size;
		const uint8_t *d = frame + 10;
		int64_t dn = frame_size;
		if (version == 3 ? (frame_flags & 0xc0) : (frame_flags & 0x0c)) {
			continue; // Compressed or encrypted.
		}
		Vector<uint8_t> clean_frame;
		if (version == 4) {
			if (frame_flags & 0x01) { // Data length indicator.
				if (dn < 4) {
					continue;
				}
				d += 4;
				dn -= 4;
			}
			if ((frame_flags & 0x02) || (flags & 0x80)) {
				clean_frame = remove_unsynchronisation(d, dn);
				d = clean_frame.ptr();
				dn = clean_frame.size();
			}
		}
		if (dn < 1) {
			continue;
		}
		const uint8_t encoding = d[0];
		if (memcmp(frame, "COMM", 4) == 0 && dn >= 4) {
			// Language, short description, text. Prefer the comment without description (players write their data in described ones).
			const uint8_t *text = d + 4;
			const int64_t text_size = dn - 4;
			const int64_t description_size = string_length(encoding, text, text_size);
			const String description = decode(encoding, text, description_size);
			const int rank = description.is_empty() ? 2 : (description.begins_with("iTun") ? 0 : 1);
			const int64_t skip = MIN(text_size, description_size + char_size(encoding));
			if (rank > comment_rank) {
				comment_rank = rank;
				p_stream->set_comment(decode_text(encoding, text + skip, text_size - skip));
			}
		} else if (memcmp(frame, "APIC", 4) == 0) {
			// MIME type, picture type, description, picture data. Prefer the front cover (type 3).
			int64_t at = 1 + string_length(0, d + 1, dn - 1) + 1;
			if (at + 1 > dn) {
				continue;
			}
			const int rank = d[at] == 3 ? 2 : 1;
			at += 1;
			at += MIN(dn - at, string_length(encoding, d + at, dn - at) + char_size(encoding));
			if (rank > picture_rank && at < dn) {
				picture_rank = rank;
				Vector<uint8_t> picture;
				picture.resize(dn - at);
				memcpy(picture.ptrw(), d + at, dn - at);
				p_stream->set_cover_data(picture);
			}
		} else if (frame[0] == 'T' && memcmp(frame, "TXXX", 4) != 0) {
			static const char *const ids[][2] = {
				{ "TIT2", "title" }, { "TPE1", "artist" }, { "TALB", "album" }, { "TPE2", "albumartist" }, { "TRCK", "tracknumber" },
				{ "TPOS", "discnumber" }, { "TYER", "date" }, { "TDRC", "date" }, { "TCON", "genre" }, { "TCOM", "composer" }, { "TCOP", "copyright" }
			};
			for (const auto &id : ids) {
				if (memcmp(frame, id[0], 4) == 0) {
					set_field(p_stream, id[1], decode_text(encoding, d + 1, dn - 1));
				}
			}
		}
	}
}

bool AudioStreamTags::read_id3v1(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size) {
	if (p_size < 128) {
		return false;
	}
	const uint8_t *p = p_data + p_size - 128;
	if (p[0] != 'T' || p[1] != 'A' || p[2] != 'G') {
		return false;
	}
	const auto field = [p](int p_offset, int p_length) {
		return decode(0, p + p_offset, string_length(0, p + p_offset, p_length)).strip_edges();
	};
	const bool v11 = p[125] == 0 && p[126] != 0; // ID3v1.1: the last comment byte is the track number.
	set_field(p_stream, "title", field(3, 30));
	set_field(p_stream, "artist", field(33, 30));
	set_field(p_stream, "album", field(63, 30));
	set_field(p_stream, "date", field(93, 4));
	set_field(p_stream, "comment", field(97, v11 ? 28 : 30));
	if (v11) {
		p_stream->set_track_number(p[126]);
	}
	set_field(p_stream, "genre", genre_name(p[127]));
	return true;
}

void AudioStreamTags::read_riff(AudioStream *p_stream, const uint8_t *p_data, int64_t p_size) {
	if (p_size < 12 || memcmp(p_data, "RIFF", 4) != 0) {
		return;
	}
	for (int64_t pos = 12; pos + 8 <= p_size;) {
		const int64_t length = MIN(int64_t(le32(p_data + pos + 4)), p_size - pos - 8);
		read_riff_chunk(p_stream, p_data + pos, p_data + pos + 8, length);
		pos += 8 + length + (length & 1);
	}
}

void AudioStreamTags::read_comments(AudioStream *p_stream, const Dictionary &p_comments) {
	for (const KeyValue<Variant, Variant> &E : p_comments) {
		set_field(p_stream, String(E.key).to_lower(), E.value);
	}
}

void AudioStreamTags::set_field(AudioStream *p_stream, const String &p_key, const String &p_value) {
	if (p_value.is_empty()) {
		return;
	}
	if (p_key == "title") {
		p_stream->set_title(p_value);
	} else if (p_key == "artist") {
		p_stream->set_artist(p_value);
	} else if (p_key == "album") {
		p_stream->set_album(p_value);
	} else if (p_key == "albumartist" || p_key == "album artist" || p_key == "album_artist") {
		p_stream->set_album_artist(p_value);
	} else if (p_key == "tracknumber") {
		set_number(p_stream, p_value, &AudioStream::set_track_number, &AudioStream::set_track_count);
	} else if (p_key == "tracktotal" || p_key == "totaltracks") {
		p_stream->set_track_count(p_value.to_int());
	} else if (p_key == "discnumber") {
		set_number(p_stream, p_value, &AudioStream::set_disc_number, &AudioStream::set_disc_count);
	} else if (p_key == "disctotal" || p_key == "totaldiscs") {
		p_stream->set_disc_count(p_value.to_int());
	} else if (p_key == "date" || p_key == "year") {
		p_stream->set_year(p_value.substr(0, 4).to_int());
	} else if (p_key == "genre") {
		p_stream->set_genre(get_genre_name(p_value));
	} else if (p_key == "composer") {
		p_stream->set_composer(p_value);
	} else if (p_key == "copyright") {
		p_stream->set_copyright(p_value);
	} else if (p_key == "comment" || p_key == "description") {
		p_stream->set_comment(p_value);
	} else if (p_key == "metadata_block_picture") {
		const CharString base64 = p_value.ascii();
		Vector<uint8_t> block;
		block.resize(base64.length() / 4 * 3 + 3);
		size_t size = 0;
		if (CryptoCore::b64_decode(block.ptrw(), size_t(block.size()), &size, (const uint8_t *)base64.get_data(), size_t(base64.length())) == OK) {
			read_flac_picture(p_stream, block.ptr(), size);
		}
	}
}

String AudioStreamTags::get_genre_name(const String &p_genre) {
	const String genre = p_genre.strip_edges();
	if (genre.is_valid_int()) {
		const String name = genre_name(genre.to_int());
		return name.is_empty() ? genre : name;
	}
	if (genre.begins_with("(") && genre.find_char(')') > 1) {
		const int end = genre.find_char(')');
		const String code = genre.substr(1, end - 1);
		const String rest = genre.substr(end + 1).strip_edges();
		if (!rest.is_empty()) {
			return rest; // "(17)Rock Remix": the text refines the code.
		}
		if (code == "RX") {
			return "Remix";
		}
		if (code == "CR") {
			return "Cover";
		}
		const String name = code.is_valid_int() ? genre_name(code.to_int()) : String();
		return name.is_empty() ? genre : name;
	}
	return genre;
}

Ref<Texture2D> AudioStreamTags::decode_picture(const Vector<uint8_t> &p_data) {
	if (p_data.size() < 12) {
		return Ref<Texture2D>();
	}
	const uint8_t *p = p_data.ptr();
	Ref<Image> image;
	image.instantiate();
	Error err = ERR_FILE_UNRECOGNIZED;
	if (p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G') {
		err = image->load_png_from_buffer(p_data);
	} else if (p[0] == 0xff && p[1] == 0xd8) {
		err = image->load_jpg_from_buffer(p_data);
	} else if (memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "WEBP", 4) == 0) {
		err = image->load_webp_from_buffer(p_data);
	}
	if (err != OK || image->is_empty()) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(image);
}

void AudioStreamTags::read_tags(AudioStream *p_stream, const Vector<uint8_t> &p_file_data) {
	ERR_FAIL_NULL(p_stream);
	const uint8_t *p = p_file_data.ptr();
	const int64_t n = p_file_data.size();
	if (n >= 12 && memcmp(p, "RIFF", 4) == 0) {
		read_riff(p_stream, p, n);
	} else if (n >= 4 && memcmp(p, "OggS", 4) == 0) {
		read_comments(p_stream, p_stream->get_tags()); // Already parsed by libvorbis / opusfile.
	} else {
		read_id3v1(p_stream, p, n); // ID3v2 is read last and wins.
		read_id3v2(p_stream, p, n);
	}
}

void AudioStreamTags::read_file_tags(AudioStream *p_stream, const Vector<uint8_t> &p_file_data) {
	if (p_stream && GLOBAL_GET_CACHED(bool, "audio/info/read_file_tags")) {
		read_tags(p_stream, p_file_data);
	}
}

Ref<AudioStream> AudioStreamTags::load_info(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		// Source files are not exported: an imported file keeps its tags in the imported resource.
		return ResourceLoader::load(p_path, "AudioStream");
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), Ref<AudioStream>(), vformat("Cannot open audio file \"%s\".", p_path));
	const uint64_t size = file->get_length();
	const Vector<uint8_t> head = read_at(file, 0, 12);
	Ref<AudioStream> stream;
	if (head.size() >= 12 && memcmp(head.ptr(), "RIFF", 4) == 0) {
		stream = load_wav_info(file, size);
	} else if (head.size() >= 4 && memcmp(head.ptr(), "OggS", 4) == 0) {
		stream = load_ogg_info(file, size);
	} else {
		stream = load_mp3_info(file, size);
	}
	ERR_FAIL_COND_V_MSG(stream.is_null(), stream, vformat("Cannot read audio info of \"%s\".", p_path));
	return stream;
}

void AudioStreamTags::get_import_options(List<ResourceImporter::ImportOption> *r_options) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "info/read_file_tags"), true));
	for (int i = 0; i < int(sizeof(info_fields) / sizeof(info_fields[0])); i++) {
		const bool is_number = i >= 4 && i <= 8;
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(is_number ? Variant::INT : Variant::STRING, String("info/") + info_fields[i]), is_number ? Variant(0) : Variant(String())));
	}
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "subtitle_path", PROPERTY_HINT_FILE, SUBTITLE_PATH_HINT), String()));
}

void AudioStreamTags::apply_import_options(AudioStream *p_stream, const String &p_source_file, const HashMap<StringName, Variant> &p_options) {
	ERR_FAIL_NULL(p_stream);
	const Variant *read = p_options.getptr("info/read_file_tags");
	if (read && !bool(*read)) {
		p_stream->clear_track_info();
	} else if (!GLOBAL_GET_CACHED(bool, "audio/info/read_file_tags")) {
		read_tags(p_stream, FileAccess::get_file_as_bytes(p_source_file)); // The loader skipped them.
	}
	for (const char *field : info_fields) {
		const Variant *value = p_options.getptr(StringName(String("info/") + field));
		if (value && value->booleanize()) {
			p_stream->set(field, *value);
		}
	}
	const Variant *subtitle_path = p_options.getptr("subtitle_path");
	if (subtitle_path) {
		p_stream->set_subtitle_path(*subtitle_path);
	}
}
