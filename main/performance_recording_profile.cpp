/**************************************************************************/
/*  performance_recording_profile.cpp                                     */
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

#include "performance_recording_profile.h"

#include "core/object/class_db.h"
#include "main/performance.h"
#include "main/performance_recorder.h"

void PerformanceRecordingProfile::set_builtins(const PackedInt32Array &p_builtins) {
	builtins = p_builtins;
	emit_changed();
}

PackedInt32Array PerformanceRecordingProfile::get_builtins() const {
	return builtins;
}

void PerformanceRecordingProfile::set_monitors(const PackedInt32Array &p_monitors) {
	monitors = p_monitors;
	emit_changed();
}

PackedInt32Array PerformanceRecordingProfile::get_monitors() const {
	return monitors;
}

void PerformanceRecordingProfile::set_include_custom_monitors(bool p_include) {
	include_custom_monitors = p_include;
	emit_changed();
}

bool PerformanceRecordingProfile::is_include_custom_monitors() const {
	return include_custom_monitors;
}

void PerformanceRecordingProfile::set_metric_sources(const PackedStringArray &p_sources) {
	metric_sources = p_sources;
	emit_changed();
}

PackedStringArray PerformanceRecordingProfile::get_metric_sources() const {
	return metric_sources;
}

void PerformanceRecordingProfile::set_metrics(const PackedStringArray &p_metrics) {
	metrics = p_metrics;
	emit_changed();
}

PackedStringArray PerformanceRecordingProfile::get_metrics() const {
	return metrics;
}

void PerformanceRecordingProfile::set_sample_interval_frames(int p_frames) {
	sample_interval_frames = MAX(1, p_frames);
	emit_changed();
}

int PerformanceRecordingProfile::get_sample_interval_frames() const {
	return sample_interval_frames;
}

void PerformanceRecordingProfile::set_process_memory_interval_frames(int p_frames) {
	process_memory_interval_frames = MAX(1, p_frames);
	emit_changed();
}

int PerformanceRecordingProfile::get_process_memory_interval_frames() const {
	return process_memory_interval_frames;
}

void PerformanceRecordingProfile::set_buffer_frames(int p_frames) {
	buffer_frames = MAX(1, p_frames);
	emit_changed();
}

int PerformanceRecordingProfile::get_buffer_frames() const {
	return buffer_frames;
}

void PerformanceRecordingProfile::set_flush_interval_frames(int p_frames) {
	flush_interval_frames = MAX(0, p_frames);
	emit_changed();
}

int PerformanceRecordingProfile::get_flush_interval_frames() const {
	return flush_interval_frames;
}

void PerformanceRecordingProfile::set_flush_budget_usec(int p_usec) {
	flush_budget_usec = MAX(0, p_usec);
	emit_changed();
}

int PerformanceRecordingProfile::get_flush_budget_usec() const {
	return flush_budget_usec;
}

void PerformanceRecordingProfile::set_output_path(const String &p_path) {
	output_path = p_path;
	emit_changed();
}

String PerformanceRecordingProfile::get_output_path() const {
	return output_path;
}

void PerformanceRecordingProfile::set_delimiter(const String &p_delimiter) {
	delimiter = p_delimiter;
	emit_changed();
}

String PerformanceRecordingProfile::get_delimiter() const {
	return delimiter;
}

void PerformanceRecordingProfile::set_hitch_threshold_usec(int p_usec) {
	hitch_threshold_usec = MAX(0, p_usec);
	emit_changed();
}

int PerformanceRecordingProfile::get_hitch_threshold_usec() const {
	return hitch_threshold_usec;
}

void PerformanceRecordingProfile::set_write_summary_json(bool p_write) {
	write_summary_json = p_write;
	emit_changed();
}

bool PerformanceRecordingProfile::is_write_summary_json() const {
	return write_summary_json;
}

void PerformanceRecordingProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_builtins", "builtins"), &PerformanceRecordingProfile::set_builtins);
	ClassDB::bind_method(D_METHOD("get_builtins"), &PerformanceRecordingProfile::get_builtins);
	ClassDB::bind_method(D_METHOD("set_monitors", "monitors"), &PerformanceRecordingProfile::set_monitors);
	ClassDB::bind_method(D_METHOD("get_monitors"), &PerformanceRecordingProfile::get_monitors);
	ClassDB::bind_method(D_METHOD("set_include_custom_monitors", "include"), &PerformanceRecordingProfile::set_include_custom_monitors);
	ClassDB::bind_method(D_METHOD("is_include_custom_monitors"), &PerformanceRecordingProfile::is_include_custom_monitors);
	ClassDB::bind_method(D_METHOD("set_metric_sources", "sources"), &PerformanceRecordingProfile::set_metric_sources);
	ClassDB::bind_method(D_METHOD("get_metric_sources"), &PerformanceRecordingProfile::get_metric_sources);
	ClassDB::bind_method(D_METHOD("set_metrics", "metrics"), &PerformanceRecordingProfile::set_metrics);
	ClassDB::bind_method(D_METHOD("get_metrics"), &PerformanceRecordingProfile::get_metrics);
	ClassDB::bind_method(D_METHOD("set_sample_interval_frames", "frames"), &PerformanceRecordingProfile::set_sample_interval_frames);
	ClassDB::bind_method(D_METHOD("get_sample_interval_frames"), &PerformanceRecordingProfile::get_sample_interval_frames);
	ClassDB::bind_method(D_METHOD("set_process_memory_interval_frames", "frames"), &PerformanceRecordingProfile::set_process_memory_interval_frames);
	ClassDB::bind_method(D_METHOD("get_process_memory_interval_frames"), &PerformanceRecordingProfile::get_process_memory_interval_frames);
	ClassDB::bind_method(D_METHOD("set_buffer_frames", "frames"), &PerformanceRecordingProfile::set_buffer_frames);
	ClassDB::bind_method(D_METHOD("get_buffer_frames"), &PerformanceRecordingProfile::get_buffer_frames);
	ClassDB::bind_method(D_METHOD("set_flush_interval_frames", "frames"), &PerformanceRecordingProfile::set_flush_interval_frames);
	ClassDB::bind_method(D_METHOD("get_flush_interval_frames"), &PerformanceRecordingProfile::get_flush_interval_frames);
	ClassDB::bind_method(D_METHOD("set_flush_budget_usec", "usec"), &PerformanceRecordingProfile::set_flush_budget_usec);
	ClassDB::bind_method(D_METHOD("get_flush_budget_usec"), &PerformanceRecordingProfile::get_flush_budget_usec);
	ClassDB::bind_method(D_METHOD("set_output_path", "path"), &PerformanceRecordingProfile::set_output_path);
	ClassDB::bind_method(D_METHOD("get_output_path"), &PerformanceRecordingProfile::get_output_path);
	ClassDB::bind_method(D_METHOD("set_delimiter", "delimiter"), &PerformanceRecordingProfile::set_delimiter);
	ClassDB::bind_method(D_METHOD("get_delimiter"), &PerformanceRecordingProfile::get_delimiter);
	ClassDB::bind_method(D_METHOD("set_hitch_threshold_usec", "usec"), &PerformanceRecordingProfile::set_hitch_threshold_usec);
	ClassDB::bind_method(D_METHOD("get_hitch_threshold_usec"), &PerformanceRecordingProfile::get_hitch_threshold_usec);
	ClassDB::bind_method(D_METHOD("set_write_summary_json", "write"), &PerformanceRecordingProfile::set_write_summary_json);
	ClassDB::bind_method(D_METHOD("is_write_summary_json"), &PerformanceRecordingProfile::is_write_summary_json);

	ADD_GROUP("Columns", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "builtins", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::INT, PROPERTY_HINT_ENUM, "Frame,Time Usec,Frame Time,Process Time,Physics Time,Physics Steps,Process Memory,Process Memory Peak,GPU Time,Recorder Time")), "set_builtins", "get_builtins");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "monitors"), "set_monitors", "get_monitors");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_custom_monitors"), "set_include_custom_monitors", "is_include_custom_monitors");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "metric_sources"), "set_metric_sources", "get_metric_sources");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "metrics"), "set_metrics", "get_metrics");
	ADD_GROUP("Sampling", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_interval_frames", PROPERTY_HINT_RANGE, "1,600,1,or_greater"), "set_sample_interval_frames", "get_sample_interval_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_memory_interval_frames", PROPERTY_HINT_RANGE, "1,600,1,or_greater"), "set_process_memory_interval_frames", "get_process_memory_interval_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffer_frames", PROPERTY_HINT_RANGE, "1,36000,1,or_greater"), "set_buffer_frames", "get_buffer_frames");
	ADD_GROUP("Output", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flush_interval_frames", PROPERTY_HINT_RANGE, "0,3600,1,or_greater"), "set_flush_interval_frames", "get_flush_interval_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flush_budget_usec", PROPERTY_HINT_RANGE, "0,16000,1,or_greater,suffix:usec"), "set_flush_budget_usec", "get_flush_budget_usec");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "output_path"), "set_output_path", "get_output_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "delimiter"), "set_delimiter", "get_delimiter");
	ADD_GROUP("Summary", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hitch_threshold_usec", PROPERTY_HINT_RANGE, "0,1000000,1,or_greater,suffix:usec"), "set_hitch_threshold_usec", "get_hitch_threshold_usec");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "write_summary_json"), "set_write_summary_json", "is_write_summary_json");
}

PerformanceRecordingProfile::PerformanceRecordingProfile() {
	const int default_builtins[] = {
		PerformanceRecorder::BUILTIN_FRAME,
		PerformanceRecorder::BUILTIN_TIME_USEC,
		PerformanceRecorder::BUILTIN_FRAME_TIME,
		PerformanceRecorder::BUILTIN_PROCESS_TIME,
		PerformanceRecorder::BUILTIN_PHYSICS_TIME,
		PerformanceRecorder::BUILTIN_PHYSICS_STEPS,
		PerformanceRecorder::BUILTIN_PROCESS_MEMORY,
		PerformanceRecorder::BUILTIN_RECORDER_TIME,
	};
	for (int builtin : default_builtins) {
		builtins.push_back(builtin);
	}

	const int default_monitors[] = {
		Performance::TIME_FPS,
		Performance::MEMORY_STATIC,
		Performance::OBJECT_COUNT,
		Performance::OBJECT_NODE_COUNT,
		Performance::RENDER_TOTAL_OBJECTS_IN_FRAME,
		Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME,
		Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME,
		Performance::RENDER_VIDEO_MEM_USED,
	};
	for (int monitor : default_monitors) {
		monitors.push_back(monitor);
	}
}
