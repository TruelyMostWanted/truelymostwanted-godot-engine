/**************************************************************************/
/*  audio_stream_playlist.cpp                                             */
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

#include "audio_stream_playlist.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "servers/audio/audio_server.h"

Ref<AudioStreamPlayback> AudioStreamPlaylist::instantiate_playback() {
	Ref<AudioStreamPlaybackPlaylist> playback_playlist;
	playback_playlist.instantiate();
	playback_playlist->playlist = Ref<AudioStreamPlaylist>(this);
	playback_playlist->_update_playback_instances();
	playbacks.insert(playback_playlist.operator->());
	return playback_playlist;
}

void AudioStreamPlaylist::set_list_stream(int p_stream_index, Ref<AudioStream> p_stream) {
	ERR_FAIL_COND(p_stream == this);
	ERR_FAIL_INDEX(p_stream_index, MAX_STREAMS);

	AudioServer::get_singleton()->lock();
	audio_streams[p_stream_index] = p_stream;
	for (AudioStreamPlaybackPlaylist *E : playbacks) {
		E->_update_playback_instances();
	}
	AudioServer::get_singleton()->unlock();
}

Ref<AudioStream> AudioStreamPlaylist::get_list_stream(int p_stream_index) const {
	ERR_FAIL_INDEX_V(p_stream_index, MAX_STREAMS, Ref<AudioStream>());

	return audio_streams[p_stream_index];
}

Vector<Ref<AudioStream>> AudioStreamPlaylist::_get_list() const {
	Vector<Ref<AudioStream>> list;
	list.resize(stream_count);
	for (int i = 0; i < stream_count; i++) {
		list.write[i] = audio_streams[i];
	}
	return list;
}

void AudioStreamPlaylist::_set_list(const Vector<Ref<AudioStream>> &p_list) {
	ERR_FAIL_COND_MSG(p_list.size() > MAX_STREAMS, vformat("An AudioStreamPlaylist holds at most %d streams.", (int)MAX_STREAMS));
	for (const Ref<AudioStream> &stream : p_list) {
		ERR_FAIL_COND_MSG(stream.ptr() == this, "An AudioStreamPlaylist cannot contain itself.");
	}

	AudioServer::get_singleton()->lock();
	stream_count = p_list.size();
	for (int i = 0; i < MAX_STREAMS; i++) {
		audio_streams[i] = i < stream_count ? p_list[i] : Ref<AudioStream>();
	}
	for (AudioStreamPlaybackPlaylist *E : playbacks) {
		E->_update_playback_instances();
	}
	AudioServer::get_singleton()->unlock();
}

void AudioStreamPlaylist::set_streams(const TypedArray<AudioStream> &p_streams) {
	Vector<Ref<AudioStream>> list;
	for (int i = 0; i < p_streams.size(); i++) {
		list.push_back(p_streams[i]);
	}
	_set_list(list);
}

TypedArray<AudioStream> AudioStreamPlaylist::get_streams() const {
	TypedArray<AudioStream> streams;
	for (int i = 0; i < stream_count; i++) {
		streams.push_back(audio_streams[i]);
	}
	return streams;
}

void AudioStreamPlaylist::add_stream(const Ref<AudioStream> &p_stream) {
	insert_stream(stream_count, p_stream);
}

void AudioStreamPlaylist::insert_stream(int p_index, const Ref<AudioStream> &p_stream) {
	ERR_FAIL_INDEX(p_index, stream_count + 1);
	Vector<Ref<AudioStream>> list = _get_list();
	list.insert(p_index, p_stream);
	_set_list(list);
}

void AudioStreamPlaylist::remove_stream(int p_index) {
	ERR_FAIL_INDEX(p_index, stream_count);
	Vector<Ref<AudioStream>> list = _get_list();
	list.remove_at(p_index);
	_set_list(list);
}

void AudioStreamPlaylist::move_stream(int p_from, int p_to) {
	ERR_FAIL_INDEX(p_from, stream_count);
	ERR_FAIL_INDEX(p_to, stream_count);
	Vector<Ref<AudioStream>> list = _get_list();
	const Ref<AudioStream> stream = list[p_from];
	list.remove_at(p_from);
	list.insert(p_to, stream);
	_set_list(list);
}

int AudioStreamPlaylist::find_stream(const Ref<AudioStream> &p_stream) const {
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i] == p_stream) {
			return i;
		}
	}
	return -1;
}

String AudioStreamPlaylist::get_stream_name(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, stream_count, String());
	const Ref<AudioStream> &stream = audio_streams[p_index];
	if (stream.is_null()) {
		return String();
	}
	if (!stream->get_name().is_empty()) {
		return stream->get_name();
	}
	// Built-in resources have no file name of their own.
	return stream->get_path().is_resource_file() ? stream->get_path().get_file().get_basename() : String();
}

double AudioStreamPlaylist::get_bpm() const {
	for (int i = 0; i < stream_count; i++) {
		if (audio_streams[i].is_valid()) {
			double bpm = audio_streams[i]->get_bpm();
			if (bpm != 0.0) {
				return bpm;
			}
		}
	}
	return 0.0;
}

double AudioStreamPlaylist::get_length() const {
	double total_length = 0.0;
	for (int i = 0; i < stream_count; i++) {
		total_length += get_stream_length(i);
	}
	return total_length;
}

double AudioStreamPlaylist::get_stream_length(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, stream_count, 0.0);
	const Ref<AudioStream> &stream = audio_streams[p_index];
	if (stream.is_null()) {
		return 0.0;
	}
	// Same rule as the playback: beats when the stream has a BPM, else its length.
	const double bpm = stream->get_bpm();
	const int beat_count = stream->get_beat_count();
	return bpm > 0.0 && beat_count > 0 ? beat_count * 60.0 / bpm : stream->get_length();
}

double AudioStreamPlaylist::get_stream_start_time(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, stream_count, 0.0);
	double start = 0.0;
	for (int i = 0; i < p_index; i++) {
		start += get_stream_length(i);
	}
	return start;
}

int AudioStreamPlaylist::get_stream_index_at(double p_time) const {
	const double length = get_length();
	if (p_time < 0.0 || length <= 0.0) {
		return -1;
	}
	if (p_time >= length) {
		if (!loop) {
			return -1;
		}
		p_time = Math::fmod(p_time, length); // Like start() of the playback.
	}
	for (int i = 0; i < stream_count; i++) {
		const double stream_length = get_stream_length(i);
		if (p_time < stream_length) {
			return i;
		}
		p_time -= stream_length;
	}
	return -1;
}

void AudioStreamPlaylist::set_stream_count(int p_count) {
	ERR_FAIL_COND(p_count < 0 || p_count > MAX_STREAMS);
	AudioServer::get_singleton()->lock();
	stream_count = p_count;
	AudioServer::get_singleton()->unlock();
	notify_property_list_changed();
}

int AudioStreamPlaylist::get_stream_count() const {
	return stream_count;
}

void AudioStreamPlaylist::set_fade_time(float p_time) {
	fade_time = p_time;
}

float AudioStreamPlaylist::get_fade_time() const {
	return fade_time;
}

void AudioStreamPlaylist::set_shuffle(bool p_shuffle) {
	shuffle = p_shuffle;
}

bool AudioStreamPlaylist::get_shuffle() const {
	return shuffle;
}

void AudioStreamPlaylist::set_loop(bool p_loop) {
	loop = p_loop;
}

bool AudioStreamPlaylist::has_loop() const {
	return loop;
}

void AudioStreamPlaylist::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream_count", "stream_count"), &AudioStreamPlaylist::set_stream_count);
	ClassDB::bind_method(D_METHOD("get_stream_count"), &AudioStreamPlaylist::get_stream_count);

	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamPlaylist::get_bpm);

	ClassDB::bind_method(D_METHOD("set_list_stream", "stream_index", "audio_stream"), &AudioStreamPlaylist::set_list_stream);
	ClassDB::bind_method(D_METHOD("get_list_stream", "stream_index"), &AudioStreamPlaylist::get_list_stream);

	ClassDB::bind_method(D_METHOD("set_streams", "streams"), &AudioStreamPlaylist::set_streams);
	ClassDB::bind_method(D_METHOD("get_streams"), &AudioStreamPlaylist::get_streams);
	ClassDB::bind_method(D_METHOD("add_stream", "stream"), &AudioStreamPlaylist::add_stream);
	ClassDB::bind_method(D_METHOD("insert_stream", "index", "stream"), &AudioStreamPlaylist::insert_stream);
	ClassDB::bind_method(D_METHOD("remove_stream", "index"), &AudioStreamPlaylist::remove_stream);
	ClassDB::bind_method(D_METHOD("move_stream", "from", "to"), &AudioStreamPlaylist::move_stream);
	ClassDB::bind_method(D_METHOD("find_stream", "stream"), &AudioStreamPlaylist::find_stream);
	ClassDB::bind_method(D_METHOD("get_stream_name", "index"), &AudioStreamPlaylist::get_stream_name);
	ClassDB::bind_method(D_METHOD("get_stream_length", "index"), &AudioStreamPlaylist::get_stream_length);
	ClassDB::bind_method(D_METHOD("get_stream_start_time", "index"), &AudioStreamPlaylist::get_stream_start_time);
	ClassDB::bind_method(D_METHOD("get_stream_index_at", "time"), &AudioStreamPlaylist::get_stream_index_at);

	ClassDB::bind_method(D_METHOD("set_shuffle", "shuffle"), &AudioStreamPlaylist::set_shuffle);
	ClassDB::bind_method(D_METHOD("get_shuffle"), &AudioStreamPlaylist::get_shuffle);

	ClassDB::bind_method(D_METHOD("set_fade_time", "dec"), &AudioStreamPlaylist::set_fade_time);
	ClassDB::bind_method(D_METHOD("get_fade_time"), &AudioStreamPlaylist::get_fade_time);

	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &AudioStreamPlaylist::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamPlaylist::has_loop);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shuffle"), "set_shuffle", "get_shuffle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_time", PROPERTY_HINT_RANGE, "0,1,0.01,suffix:s"), "set_fade_time", "get_fade_time");

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "streams", PROPERTY_HINT_ARRAY_TYPE, AudioStream::get_class_static()), "set_streams", "get_streams");

	// Not stored any more (`streams` is), but still set when old files load.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream_count", PROPERTY_HINT_RANGE, "0," + itos(MAX_STREAMS), PROPERTY_USAGE_NONE), "set_stream_count", "get_stream_count");

	for (int i = 0; i < MAX_STREAMS; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "stream_" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, AudioStream::get_class_static(), PROPERTY_USAGE_INTERNAL), "set_list_stream", "get_list_stream", i);
	}

	BIND_CONSTANT(MAX_STREAMS);
}

//////////////////////
//////////////////////

AudioStreamPlaybackPlaylist::~AudioStreamPlaybackPlaylist() {
	if (playlist.is_valid()) {
		playlist->playbacks.erase(this);
	}
}

void AudioStreamPlaybackPlaylist::stop() {
	active = false;
	_publish_track();
	for (int i = 0; i < playlist->stream_count; i++) {
		if (playback[i].is_valid()) {
			playback[i]->stop();
		}
	}
}

void AudioStreamPlaybackPlaylist::_update_order() {
	for (int i = 0; i < playlist->stream_count; i++) {
		play_order[i] = i;
	}

	if (playlist->shuffle) {
		for (int i = 0; i < playlist->stream_count; i++) {
			int swap_with = Math::rand() % uint32_t(playlist->stream_count);
			SWAP(play_order[i], play_order[swap_with]);
		}
	}
}

void AudioStreamPlaybackPlaylist::start(double p_from_pos) {
	if (active) {
		stop();
	}

	p_from_pos = MAX(0, p_from_pos);

	float pl_length = playlist->get_length();
	if (p_from_pos >= pl_length) {
		if (!playlist->loop) {
			return; // No loop, end.
		}
		p_from_pos = Math::fmod((float)p_from_pos, (float)pl_length);
	}

	_update_order();

	play_index = -1;

	double play_ofs = p_from_pos;
	for (int i = 0; i < playlist->stream_count; i++) {
		int idx = play_order[i];
		if (playlist->audio_streams[idx].is_valid()) {
			double bpm = playlist->audio_streams[idx]->get_bpm();
			int beat_count = playlist->audio_streams[idx]->get_beat_count();
			double length;
			if (bpm > 0.0 && beat_count > 0) {
				length = beat_count * 60.0 / bpm;
			} else {
				length = playlist->audio_streams[idx]->get_length();
			}
			if (play_ofs < length) {
				play_index = i;
				track_length = length;
				stream_todo = length - play_ofs;
				break;
			} else {
				play_ofs -= length;
			}
		}
	}

	if (play_index == -1) {
		return;
	}

	playback[play_order[play_index]]->start(play_ofs);
	fade_index = -1;
	loop_count = 0;
	offset = p_from_pos;

	active = true;
	track_serial.increment();
	_update_track_start();
	_publish_track();
}

void AudioStreamPlaybackPlaylist::seek(double p_time) {
	stop();
	start(p_time);
}

int AudioStreamPlaybackPlaylist::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (!active) {
		return 0;
	}

	double time_dec = (1.0 / AudioServer::get_singleton()->get_mix_rate());
	double fade_dec = (1.0 / playlist->fade_time) / AudioServer::get_singleton()->get_mix_rate();

	int todo = p_frames;

	while (todo) {
		int to_mix = MIN(todo, MIX_BUFFER_SIZE);

		playback[play_order[play_index]]->mix(mix_buffer, 1.0, to_mix);
		if (fade_index != -1) {
			playback[fade_index]->mix(fade_buffer, 1.0, to_mix);
		}

		offset += time_dec * to_mix;

		for (int i = 0; i < to_mix; i++) {
			*p_buffer = mix_buffer[i];
			stream_todo -= time_dec;
			if (stream_todo < 0) {
				//find next stream.
				int prev = play_order[play_index];

				for (int j = 0; j < playlist->stream_count; j++) {
					play_index++;
					if (play_index >= playlist->stream_count) {
						// No loop, exit.
						if (!playlist->loop) {
							for (int k = i; k < todo - i; k++) {
								p_buffer[k] = AudioFrame(0, 0);
							}
							todo = to_mix;
							active = false;
							break;
						}

						_update_order();
						play_index = 0;
						loop_count++;
						offset = time_dec * (to_mix - i);
					}
					if (playback[play_order[play_index]].is_valid()) {
						break;
					}
				}

				if (!active) {
					break;
				}

				if (playback[play_order[play_index]].is_null()) {
					todo = to_mix; // Weird error.
					active = false;
					break;
				}

				bool restart = true;
				if (prev == play_order[play_index]) {
					// Went back to the same one, continue loop (if it loops) or restart if it does not.
					if (playlist->audio_streams[prev]->has_loop()) {
						restart = false;
					}
					fade_index = -1;
				} else {
					// Index i was already mixed but will be accessed later already, so set it to 0.
					fade_buffer[i] = AudioFrame(0.0, 0.0);
					// Move remaining mixed data to fade buffer.
					for (int j = i + 1; j < to_mix; j++) {
						fade_buffer[j] = mix_buffer[j];
					}

					fade_index = prev;
					fade_volume = 1.0;
				}

				int idx = play_order[play_index];

				if (restart) {
					playback[idx]->start(0); // No loop, just cold-restart.
					playback[idx]->mix(mix_buffer + i, 1.0, to_mix - i); // Fill rest of mix buffer
				}

				// Update fade todo.
				double bpm = playlist->audio_streams[idx]->get_bpm();
				int beat_count = playlist->audio_streams[idx]->get_beat_count();

				if (bpm > 0.0 && beat_count > 0) {
					stream_todo = beat_count * 60.0 / bpm;
				} else {
					stream_todo = playlist->audio_streams[idx]->get_length();
				}
				track_length = stream_todo;
				track_serial.increment();
				_update_track_start();
			}

			if (fade_index != -1) {
				*p_buffer += fade_buffer[i] * fade_volume;
				fade_volume -= fade_dec;
				if (fade_volume <= 0.0) {
					playback[fade_index]->stop();
					fade_index = -1;
				}
			}

			p_buffer++;
		}

		todo -= to_mix;
	}

	_publish_track();
	return p_frames;
}

void AudioStreamPlaybackPlaylist::tag_used_streams() {
	if (active) {
		playlist->audio_streams[play_order[play_index]]->tag_used(playback[play_order[play_index]]->get_playback_position());
	}

	playlist->tag_used(0);
}

int AudioStreamPlaybackPlaylist::get_loop_count() const {
	return loop_count;
}

double AudioStreamPlaybackPlaylist::get_playback_position() const {
	return offset;
}

bool AudioStreamPlaybackPlaylist::is_playing() const {
	return active;
}

void AudioStreamPlaybackPlaylist::_publish_track() {
	current_index.set(active ? play_order[play_index] : -1);
	track_position.set(active ? (float)MAX(0.0, track_length - stream_todo) : 0.0f);
	playlist_position.set(active ? (float)(track_start + MAX(0.0, track_length - stream_todo)) : 0.0f);
}

void AudioStreamPlaybackPlaylist::_update_track_start() {
	// Runs when a track starts (at most once per track), not per mix block.
	track_start = 0.0;
	for (int i = 0; i < play_index; i++) {
		track_start += playlist->get_stream_length(play_order[i]);
	}
}

double AudioStreamPlaybackPlaylist::get_position_in_playlist() const {
	return playlist_position.get();
}

int AudioStreamPlaybackPlaylist::get_current_index() const {
	return current_index.get();
}

Ref<AudioStream> AudioStreamPlaybackPlaylist::get_current_stream() const {
	const int index = current_index.get();
	if (playlist.is_null() || index < 0 || index >= playlist->stream_count) {
		return Ref<AudioStream>();
	}
	return playlist->audio_streams[index];
}

double AudioStreamPlaybackPlaylist::get_position_in_stream() const {
	return track_position.get();
}

bool AudioStreamPlaybackPlaylist::get_track_info(TrackInfo &r_info) const {
	r_info.index = current_index.get();
	r_info.serial = track_serial.get();
	r_info.position = track_position.get();
	r_info.list_position = playlist_position.get();
	r_info.stream = get_current_stream();
	return true;
}

void AudioStreamPlaybackPlaylist::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_current_index"), &AudioStreamPlaybackPlaylist::get_current_index);
	ClassDB::bind_method(D_METHOD("get_current_stream"), &AudioStreamPlaybackPlaylist::get_current_stream);
	ClassDB::bind_method(D_METHOD("get_position_in_stream"), &AudioStreamPlaybackPlaylist::get_position_in_stream);
	ClassDB::bind_method(D_METHOD("get_position_in_playlist"), &AudioStreamPlaybackPlaylist::get_position_in_playlist);
}

void AudioStreamPlaybackPlaylist::_update_playback_instances() {
	stop();

	for (int i = 0; i < playlist->stream_count; i++) {
		if (playlist->audio_streams[i].is_valid()) {
			playback[i] = playlist->audio_streams[i]->instantiate_playback();
		} else {
			playback[i].unref();
		}
	}
}
