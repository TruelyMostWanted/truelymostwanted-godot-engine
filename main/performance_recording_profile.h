/**************************************************************************/
/*  performance_recording_profile.h                                       */
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

#include "core/io/resource.h"

// Describes what PerformanceRecorder records, how often, and where the CSV goes.
class PerformanceRecordingProfile : public Resource {
	GDCLASS(PerformanceRecordingProfile, Resource);

	PackedInt32Array builtins;
	PackedInt32Array monitors;
	bool include_custom_monitors = false;
	PackedStringArray metric_sources;
	PackedStringArray metrics;
	int sample_interval_frames = 1;
	int process_memory_interval_frames = 30;
	int buffer_frames = 3600;
	int flush_interval_frames = 300;
	int flush_budget_usec = 2000;
	String output_path = "user://performance/{date}_{time}.csv";
	String delimiter = ",";
	int hitch_threshold_usec = 50000;
	bool write_summary_json = false;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_builtins(const PackedInt32Array &p_builtins);
	PackedInt32Array get_builtins() const;

	void set_monitors(const PackedInt32Array &p_monitors);
	PackedInt32Array get_monitors() const;

	void set_include_custom_monitors(bool p_include);
	bool is_include_custom_monitors() const;

	void set_metric_sources(const PackedStringArray &p_sources);
	PackedStringArray get_metric_sources() const;

	void set_metrics(const PackedStringArray &p_metrics);
	PackedStringArray get_metrics() const;

	void set_sample_interval_frames(int p_frames);
	int get_sample_interval_frames() const;

	void set_process_memory_interval_frames(int p_frames);
	int get_process_memory_interval_frames() const;

	void set_buffer_frames(int p_frames);
	int get_buffer_frames() const;

	void set_flush_interval_frames(int p_frames);
	int get_flush_interval_frames() const;

	void set_flush_budget_usec(int p_usec);
	int get_flush_budget_usec() const;

	void set_output_path(const String &p_path);
	String get_output_path() const;

	void set_delimiter(const String &p_delimiter);
	String get_delimiter() const;

	void set_hitch_threshold_usec(int p_usec);
	int get_hitch_threshold_usec() const;

	void set_write_summary_json(bool p_write);
	bool is_write_summary_json() const;

	// A new profile starts with the default selection of builtins and monitors,
	// the same one PerformanceRecorder.start() uses without a profile.
	PerformanceRecordingProfile();
};
