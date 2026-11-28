/**************************************************************************/
/*  audio_stream.cpp                                                      */
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

#include "audio_stream.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "scene/resources/audio/audio_stream_tags.h"
#include "scene/resources/texture.h"
#include "servers/audio/audio_driver.h"
#include "servers/audio/audio_server.h"
#include "servers/subtitle/subtitle.h"

struct AudioStream::StreamInfo {
	String title;
	String artist;
	String album;
	String album_artist;
	String genre;
	String composer;
	String copyright;
	String comment;
	int track_number = 0;
	int track_count = 0;
	int disc_number = 0;
	int disc_count = 0;
	int year = 0;
	Ref<Texture2D> cover;
	Vector<uint8_t> cover_data; // Encoded picture as stored in the file, decoded into `cover` on first access.
	String subtitle_path;
	// Format of streams without audio data (`load_info()`).
	double length = 0.0;
	int sample_rate = 0;
	int channel_count = 0;
	int bitrate = 0;
};

void AudioStreamPlayback::start(double p_from_pos) {
	GDVIRTUAL_CALL(_start, p_from_pos);
}

void AudioStreamPlayback::stop() {
	GDVIRTUAL_CALL(_stop);
}

bool AudioStreamPlayback::is_playing() const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_playing, ret);
	return ret;
}

int AudioStreamPlayback::get_loop_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_loop_count, ret);
	return ret;
}

double AudioStreamPlayback::get_playback_position() const {
	double ret = 0.0;
	GDVIRTUAL_CALL(_get_playback_position, ret);
	return ret;
}

void AudioStreamPlayback::seek(double p_time) {
	GDVIRTUAL_CALL(_seek, p_time);
}

int AudioStreamPlayback::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	int ret = 0;
	GDVIRTUAL_CALL(_mix, p_buffer, p_rate_scale, p_frames, ret);
	return ret;
}

PackedVector2Array AudioStreamPlayback::_mix_audio_bind(float p_rate_scale, int p_frames) {
	Vector<AudioFrame> frames = mix_audio(p_rate_scale, p_frames);

	PackedVector2Array res;
	res.resize(frames.size());

	Vector2 *res_ptrw = res.ptrw();
	for (int i = 0; i < frames.size(); i++) {
		res_ptrw[i] = Vector2(frames[i].left, frames[i].right);
	}

	return res;
}

Vector<AudioFrame> AudioStreamPlayback::mix_audio(float p_rate_scale, int p_frames) {
	Vector<AudioFrame> res;
	res.resize(p_frames);

	int frames = mix(res.ptrw(), p_rate_scale, p_frames);
	res.resize(frames);

	return res;
}

void AudioStreamPlayback::start_playback(double p_from_pos) {
	start(p_from_pos);
}

void AudioStreamPlayback::stop_playback() {
	stop();
}

void AudioStreamPlayback::seek_playback(double p_time) {
	seek(p_time);
}

void AudioStreamPlayback::tag_used_streams() {
	GDVIRTUAL_CALL(_tag_used_streams);
}

void AudioStreamPlayback::set_parameter(const StringName &p_name, const Variant &p_value) {
	GDVIRTUAL_CALL(_set_parameter, p_name, p_value);
}

Variant AudioStreamPlayback::get_parameter(const StringName &p_name) const {
	Variant ret;
	GDVIRTUAL_CALL(_get_parameter, p_name, ret);
	return ret;
}

Ref<AudioSamplePlayback> AudioStreamPlayback::get_sample_playback() const {
	return nullptr;
}

void AudioStreamPlayback::_bind_methods() {
	GDVIRTUAL_BIND(_start, "from_pos")
	GDVIRTUAL_BIND(_stop)
	GDVIRTUAL_BIND(_is_playing)
	GDVIRTUAL_BIND(_get_loop_count)
	GDVIRTUAL_BIND(_get_playback_position)
	GDVIRTUAL_BIND(_seek, "position")
	GDVIRTUAL_BIND(_mix, "buffer", "rate_scale", "frames");
	GDVIRTUAL_BIND(_tag_used_streams);
	GDVIRTUAL_BIND(_set_parameter, "name", "value");
	GDVIRTUAL_BIND(_get_parameter, "name");

	ClassDB::bind_method(D_METHOD("set_sample_playback", "playback_sample"), &AudioStreamPlayback::set_sample_playback);
	ClassDB::bind_method(D_METHOD("get_sample_playback"), &AudioStreamPlayback::get_sample_playback);
	ClassDB::bind_method(D_METHOD("mix_audio", "rate_scale", "frames"), &AudioStreamPlayback::_mix_audio_bind);
	ClassDB::bind_method(D_METHOD("start", "from_pos"), &AudioStreamPlayback::start_playback, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("seek", "time"), &AudioStreamPlayback::seek_playback, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("stop"), &AudioStreamPlayback::stop_playback);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &AudioStreamPlayback::get_loop_count);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &AudioStreamPlayback::get_playback_position);
	ClassDB::bind_method(D_METHOD("is_playing"), &AudioStreamPlayback::is_playing);
}

AudioStreamPlayback::AudioStreamPlayback() {}

AudioStreamPlayback::~AudioStreamPlayback() {
	if (get_sample_playback().is_valid() && likely(AudioServer::get_singleton() != nullptr)) {
		AudioServer::get_singleton()->stop_sample_playback(get_sample_playback());
	}
}

//////////////////////////////

void AudioStreamPlaybackResampled::begin_resample() {
	//clear cubic interpolation history
	internal_buffer[0] = AudioFrame(0.0, 0.0);
	internal_buffer[1] = AudioFrame(0.0, 0.0);
	internal_buffer[2] = AudioFrame(0.0, 0.0);
	internal_buffer[3] = AudioFrame(0.0, 0.0);
	//mix buffer
	_mix_internal(internal_buffer + 4, INTERNAL_BUFFER_LEN);
	mix_offset = 0;
}

int AudioStreamPlaybackResampled::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	int ret = 0;
	GDVIRTUAL_CALL(_mix_resampled, p_buffer, p_frames, ret);
	return ret;
}
float AudioStreamPlaybackResampled::get_stream_sampling_rate() {
	float ret = 0;
	GDVIRTUAL_CALL(_get_stream_sampling_rate, ret);
	return ret;
}

void AudioStreamPlaybackResampled::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin_resample"), &AudioStreamPlaybackResampled::begin_resample);

	GDVIRTUAL_BIND(_mix_resampled, "dst_buffer", "frame_count");
	GDVIRTUAL_BIND(_get_stream_sampling_rate);
}

int AudioStreamPlaybackResampled::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	float target_rate = AudioServer::get_singleton()->get_mix_rate();
	float playback_speed_scale = AudioServer::get_singleton()->get_playback_speed_scale();

	uint64_t mix_increment = uint64_t(((get_stream_sampling_rate() * p_rate_scale * playback_speed_scale) / double(target_rate)) * double(FP_LEN));

	int mixed_frames_total = -1;

	int i;
	for (i = 0; i < p_frames; i++) {
		uint32_t idx = CUBIC_INTERP_HISTORY + uint32_t(mix_offset >> FP_BITS);
		//standard cubic interpolation (great quality/performance ratio)
		//this used to be moved to a LUT for greater performance, but nowadays CPU speed is generally faster than memory.
		float mu = (mix_offset & FP_MASK) / float(FP_LEN);
		AudioFrame y0 = internal_buffer[idx - 3];
		AudioFrame y1 = internal_buffer[idx - 2];
		AudioFrame y2 = internal_buffer[idx - 1];
		AudioFrame y3 = internal_buffer[idx - 0];

		if (idx >= internal_buffer_end && mixed_frames_total == -1) {
			// The internal buffer ends somewhere in this range, and we haven't yet recorded the number of good frames we have.
			mixed_frames_total = i;
		}

		float mu2 = mu * mu;
		float h11 = mu2 * (mu - 1);
		float z = mu2 - h11;
		float h01 = z - h11;
		float h10 = mu - z;

		p_buffer[i] = y1 + (y2 - y1) * h01 + ((y2 - y0) * h10 + (y3 - y1) * h11) * 0.5;

		mix_offset += mix_increment;

		while ((mix_offset >> FP_BITS) >= INTERNAL_BUFFER_LEN) {
			internal_buffer[0] = internal_buffer[INTERNAL_BUFFER_LEN + 0];
			internal_buffer[1] = internal_buffer[INTERNAL_BUFFER_LEN + 1];
			internal_buffer[2] = internal_buffer[INTERNAL_BUFFER_LEN + 2];
			internal_buffer[3] = internal_buffer[INTERNAL_BUFFER_LEN + 3];
			int mixed_frames = _mix_internal(internal_buffer + 4, INTERNAL_BUFFER_LEN);
			if (mixed_frames != INTERNAL_BUFFER_LEN) {
				// internal_buffer[mixed_frames] is the first frame of silence.
				internal_buffer_end = mixed_frames;
			} else {
				// The internal buffer does not contain the first frame of silence.
				internal_buffer_end = -1;
			}
			mix_offset -= (INTERNAL_BUFFER_LEN << FP_BITS);
		}
	}
	if (mixed_frames_total == -1 && i == p_frames) {
		mixed_frames_total = p_frames;
	}
	return mixed_frames_total;
}

////////////////////////////////

Ref<AudioStreamPlayback> AudioStream::instantiate_playback() {
	Ref<AudioStreamPlayback> ret;
	GDVIRTUAL_CALL(_instantiate_playback, ret);
	return ret;
}

double AudioStream::get_length() const {
	double ret = 0;
	if (GDVIRTUAL_CALL(_get_length, ret)) {
		return ret;
	}
	return stream_info ? stream_info->length : 0.0;
}

bool AudioStream::is_monophonic() const {
	bool ret = true;
	GDVIRTUAL_CALL(_is_monophonic, ret);
	return ret;
}

double AudioStream::get_bpm() const {
	double ret = 0;
	GDVIRTUAL_CALL(_get_bpm, ret);
	return ret;
}

bool AudioStream::has_loop() const {
	bool ret = false;
	GDVIRTUAL_CALL(_has_loop, ret);
	return ret;
}

int AudioStream::get_bar_beats() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_bar_beats, ret);
	return ret;
}

int AudioStream::get_beat_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_beat_count, ret);
	return ret;
}

Dictionary AudioStream::get_tags() const {
	Dictionary ret;
	GDVIRTUAL_CALL(_get_tags, ret);
	return ret;
}

void AudioStream::tag_used(float p_offset) {
	if (tagged_frame != AudioServer::get_singleton()->get_mixed_frames()) {
		offset_count = 0;
		tagged_frame = AudioServer::get_singleton()->get_mixed_frames();
	}
	if (offset_count < MAX_TAGGED_OFFSETS) {
		tagged_offsets[offset_count++] = p_offset;
	}
}

uint64_t AudioStream::get_tagged_frame() const {
	return tagged_frame;
}

uint32_t AudioStream::get_tagged_frame_count() const {
	return offset_count;
}

float AudioStream::get_tagged_frame_offset(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, MAX_TAGGED_OFFSETS, 0);
	return tagged_offsets[p_index];
}

void AudioStream::get_parameter_list(List<Parameter> *r_parameters) {
	TypedArray<Dictionary> ret;
	GDVIRTUAL_CALL(_get_parameter_list, ret);
	for (int i = 0; i < ret.size(); i++) {
		Dictionary d = ret[i];
		ERR_CONTINUE(!d.has("default_value"));
		r_parameters->push_back(Parameter(PropertyInfo::from_dict(d), d["default_value"]));
	}
}

Ref<AudioSample> AudioStream::generate_sample() const {
	ERR_FAIL_COND_V_MSG(!can_be_sampled(), nullptr, "Cannot generate a sample for a stream that cannot be sampled.");
	Ref<AudioSample> sample;
	sample.instantiate();
	sample->stream = this;
	return sample;
}

AudioStream::StreamInfo *AudioStream::_stream_info_ptrw() {
	if (!stream_info) {
		stream_info = memnew(StreamInfo);
	}
	return stream_info;
}

void AudioStream::clear_track_info() {
	if (stream_info) {
		memdelete(stream_info);
		stream_info = nullptr;
	}
}

AudioStream::~AudioStream() {
	clear_track_info();
}

int AudioStream::get_sample_rate() const {
	return stream_info ? stream_info->sample_rate : 0;
}

int AudioStream::get_channel_count() const {
	return stream_info ? stream_info->channel_count : 0;
}

int AudioStream::get_bitrate() const {
	return stream_info ? stream_info->bitrate : 0;
}

String AudioStream::get_codec() const {
	return String();
}

void AudioStream::set_format_info(double p_length, int p_sample_rate, int p_channel_count, int p_bitrate) {
	StreamInfo *si = _stream_info_ptrw();
	si->length = p_length;
	si->sample_rate = p_sample_rate;
	si->channel_count = p_channel_count;
	si->bitrate = p_bitrate;
}

// Setters allocate the info only for non-empty values, so streams without tags stay at one null pointer.
#define AUDIO_STREAM_INFO_FIELD(m_type, m_arg_type, m_name) \
	void AudioStream::set_##m_name(m_arg_type p_value) { \
		if (stream_info || p_value != m_type()) { \
			_stream_info_ptrw()->m_name = p_value; \
		} \
	} \
	m_type AudioStream::get_##m_name() const { \
		return stream_info ? stream_info->m_name : m_type(); \
	}

AUDIO_STREAM_INFO_FIELD(String, const String &, title)
AUDIO_STREAM_INFO_FIELD(String, const String &, artist)
AUDIO_STREAM_INFO_FIELD(String, const String &, album)
AUDIO_STREAM_INFO_FIELD(String, const String &, album_artist)
AUDIO_STREAM_INFO_FIELD(String, const String &, genre)
AUDIO_STREAM_INFO_FIELD(String, const String &, composer)
AUDIO_STREAM_INFO_FIELD(String, const String &, copyright)
AUDIO_STREAM_INFO_FIELD(String, const String &, comment)
AUDIO_STREAM_INFO_FIELD(int, int, track_number)
AUDIO_STREAM_INFO_FIELD(int, int, track_count)
AUDIO_STREAM_INFO_FIELD(int, int, disc_number)
AUDIO_STREAM_INFO_FIELD(int, int, disc_count)
AUDIO_STREAM_INFO_FIELD(int, int, year)
AUDIO_STREAM_INFO_FIELD(String, const String &, subtitle_path)

#undef AUDIO_STREAM_INFO_FIELD

void AudioStream::set_cover(const Ref<Texture2D> &p_cover) {
	if (!stream_info && p_cover.is_null()) {
		return;
	}
	StreamInfo *si = _stream_info_ptrw();
	si->cover = p_cover;
	si->cover_data.clear(); // A texture set by hand replaces the picture of the file.
	notify_property_list_changed();
}

Ref<Texture2D> AudioStream::get_cover() const {
	if (!stream_info) {
		return Ref<Texture2D>();
	}
	if (stream_info->cover.is_null() && !stream_info->cover_data.is_empty()) {
		stream_info->cover = AudioStreamTags::decode_picture(stream_info->cover_data);
	}
	return stream_info->cover;
}

void AudioStream::set_cover_data(const Vector<uint8_t> &p_cover_data) {
	if (!stream_info && p_cover_data.is_empty()) {
		return;
	}
	StreamInfo *si = _stream_info_ptrw();
	si->cover_data = p_cover_data;
	si->cover.unref(); // Decoded again on the next get_cover().
	notify_property_list_changed();
}

Vector<uint8_t> AudioStream::get_cover_data() const {
	return stream_info ? stream_info->cover_data : Vector<uint8_t>();
}

Ref<AudioStream> AudioStream::load_info(const String &p_path) {
	return AudioStreamTags::load_info(p_path);
}

void AudioStream::_validate_property(PropertyInfo &p_property) const {
	// A cover decoded from `cover_data` is shown, but not saved a second time as a texture.
	if (p_property.name == "cover" && stream_info && !stream_info->cover_data.is_empty()) {
		p_property.usage = PROPERTY_USAGE_EDITOR;
	}
}

void AudioStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_length"), &AudioStream::get_length);
	ClassDB::bind_method(D_METHOD("is_monophonic"), &AudioStream::is_monophonic);
	ClassDB::bind_method(D_METHOD("instantiate_playback"), &AudioStream::instantiate_playback);
	ClassDB::bind_method(D_METHOD("can_be_sampled"), &AudioStream::can_be_sampled);
	ClassDB::bind_method(D_METHOD("generate_sample"), &AudioStream::generate_sample);
	ClassDB::bind_method(D_METHOD("is_meta_stream"), &AudioStream::is_meta_stream);

	GDVIRTUAL_BIND(_instantiate_playback);
#ifndef DISABLE_DEPRECATED
	GDVIRTUAL_BIND(_get_stream_name);
#endif
	GDVIRTUAL_BIND(_get_length);
	GDVIRTUAL_BIND(_is_monophonic);
	GDVIRTUAL_BIND(_get_bpm)
	GDVIRTUAL_BIND(_get_beat_count)
	GDVIRTUAL_BIND(_get_tags);
	GDVIRTUAL_BIND(_get_parameter_list)
	GDVIRTUAL_BIND(_has_loop);
	GDVIRTUAL_BIND(_get_bar_beats);

	ADD_SIGNAL(MethodInfo("parameter_list_changed"));

	ClassDB::bind_method(D_METHOD("get_sample_rate"), &AudioStream::get_sample_rate);
	ClassDB::bind_method(D_METHOD("get_channel_count"), &AudioStream::get_channel_count);
	ClassDB::bind_method(D_METHOD("get_bitrate"), &AudioStream::get_bitrate);
	ClassDB::bind_method(D_METHOD("get_codec"), &AudioStream::get_codec);
	ClassDB::bind_static_method("AudioStream", D_METHOD("load_info", "path"), &AudioStream::load_info);

	ClassDB::bind_method(D_METHOD("set_title", "title"), &AudioStream::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &AudioStream::get_title);
	ClassDB::bind_method(D_METHOD("set_artist", "artist"), &AudioStream::set_artist);
	ClassDB::bind_method(D_METHOD("get_artist"), &AudioStream::get_artist);
	ClassDB::bind_method(D_METHOD("set_album", "album"), &AudioStream::set_album);
	ClassDB::bind_method(D_METHOD("get_album"), &AudioStream::get_album);
	ClassDB::bind_method(D_METHOD("set_album_artist", "album_artist"), &AudioStream::set_album_artist);
	ClassDB::bind_method(D_METHOD("get_album_artist"), &AudioStream::get_album_artist);
	ClassDB::bind_method(D_METHOD("set_track_number", "track_number"), &AudioStream::set_track_number);
	ClassDB::bind_method(D_METHOD("get_track_number"), &AudioStream::get_track_number);
	ClassDB::bind_method(D_METHOD("set_track_count", "track_count"), &AudioStream::set_track_count);
	ClassDB::bind_method(D_METHOD("get_track_count"), &AudioStream::get_track_count);
	ClassDB::bind_method(D_METHOD("set_disc_number", "disc_number"), &AudioStream::set_disc_number);
	ClassDB::bind_method(D_METHOD("get_disc_number"), &AudioStream::get_disc_number);
	ClassDB::bind_method(D_METHOD("set_disc_count", "disc_count"), &AudioStream::set_disc_count);
	ClassDB::bind_method(D_METHOD("get_disc_count"), &AudioStream::get_disc_count);
	ClassDB::bind_method(D_METHOD("set_year", "year"), &AudioStream::set_year);
	ClassDB::bind_method(D_METHOD("get_year"), &AudioStream::get_year);
	ClassDB::bind_method(D_METHOD("set_genre", "genre"), &AudioStream::set_genre);
	ClassDB::bind_method(D_METHOD("get_genre"), &AudioStream::get_genre);
	ClassDB::bind_method(D_METHOD("set_composer", "composer"), &AudioStream::set_composer);
	ClassDB::bind_method(D_METHOD("get_composer"), &AudioStream::get_composer);
	ClassDB::bind_method(D_METHOD("set_copyright", "copyright"), &AudioStream::set_copyright);
	ClassDB::bind_method(D_METHOD("get_copyright"), &AudioStream::get_copyright);
	ClassDB::bind_method(D_METHOD("set_comment", "comment"), &AudioStream::set_comment);
	ClassDB::bind_method(D_METHOD("get_comment"), &AudioStream::get_comment);
	ClassDB::bind_method(D_METHOD("set_cover", "cover"), &AudioStream::set_cover);
	ClassDB::bind_method(D_METHOD("get_cover"), &AudioStream::get_cover);
	ClassDB::bind_method(D_METHOD("set_cover_data", "cover_data"), &AudioStream::set_cover_data);
	ClassDB::bind_method(D_METHOD("get_cover_data"), &AudioStream::get_cover_data);
	ClassDB::bind_method(D_METHOD("set_subtitle_path", "subtitle_path"), &AudioStream::set_subtitle_path);
	ClassDB::bind_method(D_METHOD("get_subtitle_path"), &AudioStream::get_subtitle_path);

	ADD_GROUP("Track", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "artist"), "set_artist", "get_artist");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "album"), "set_album", "get_album");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "album_artist"), "set_album_artist", "get_album_artist");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "track_number", PROPERTY_HINT_RANGE, "0,999,1,or_greater"), "set_track_number", "get_track_number");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "track_count", PROPERTY_HINT_RANGE, "0,999,1,or_greater"), "set_track_count", "get_track_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "disc_number", PROPERTY_HINT_RANGE, "0,99,1,or_greater"), "set_disc_number", "get_disc_number");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "disc_count", PROPERTY_HINT_RANGE, "0,99,1,or_greater"), "set_disc_count", "get_disc_count");
	ADD_GROUP("Release", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "year", PROPERTY_HINT_RANGE, "0,9999,1"), "set_year", "get_year");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "genre"), "set_genre", "get_genre");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "composer"), "set_composer", "get_composer");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright"), "set_copyright", "get_copyright");
	ADD_GROUP("Artwork", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "cover", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_cover", "get_cover");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "cover_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_cover_data", "get_cover_data");
	ADD_GROUP("Notes", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "comment", PROPERTY_HINT_MULTILINE_TEXT), "set_comment", "get_comment");
	ADD_GROUP("", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "subtitle_path", PROPERTY_HINT_FILE, SUBTITLE_PATH_HINT), "set_subtitle_path", "get_subtitle_path");
}
