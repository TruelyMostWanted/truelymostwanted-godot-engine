/**************************************************************************/
/*  performance_summary.h                                                 */
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

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

// Statistics of one recorded column over a whole PerformanceRecorder run.
class PerformanceColumnSummary : public RefCounted {
	GDCLASS(PerformanceColumnSummary, RefCounted);

public:
	// Log-scaled histogram with fixed memory: HISTOGRAM_SUBDIVISIONS buckets per power of two
	// between 2^HISTOGRAM_MIN_EXPONENT and 2^HISTOGRAM_MAX_EXPONENT, mirrored for negative values,
	// plus one bucket for zero. Values outside the range go to the outermost buckets.
	static constexpr int HISTOGRAM_SUBDIVISIONS = 8;
	static constexpr int HISTOGRAM_MIN_EXPONENT = -16;
	static constexpr int HISTOGRAM_MAX_EXPONENT = 48;
	static constexpr int HISTOGRAM_SIDE_BUCKETS = (HISTOGRAM_MAX_EXPONENT - HISTOGRAM_MIN_EXPONENT) * HISTOGRAM_SUBDIVISIONS;
	static constexpr int HISTOGRAM_ZERO_BUCKET = HISTOGRAM_SIDE_BUCKETS;
	static constexpr int HISTOGRAM_BUCKETS = HISTOGRAM_SIDE_BUCKETS * 2 + 1;

	static int get_histogram_bucket(double p_value);
	// Lower and upper bound of a bucket; the outermost buckets are open (r_low or r_high is +-inf).
	static void get_histogram_bucket_range(int p_bucket, double &r_low, double &r_high);

private:
	String name;
	int64_t sample_count = 0;
	double minimum = 0.0;
	double maximum = 0.0;
	double mean = 0.0;
	double p50 = 0.0;
	double p95 = 0.0;
	double p99 = 0.0;
	bool exact = false;
	PackedFloat64Array sorted_values; // Exact mode: all samples, sorted.
	LocalVector<uint32_t> histogram; // Histogram mode: HISTOGRAM_BUCKETS counters.

	double _get_bucket_value(int p_bucket, double p_fraction) const;

protected:
	static void _bind_methods();

public:
	// Fills the summary (C++ only). With p_sorted_values not empty the percentiles are exact,
	// otherwise they are estimated from p_histogram (HISTOGRAM_BUCKETS counters, may be null when p_count is 0).
	void set_data(const String &p_name, int64_t p_count, double p_min, double p_max, double p_sum, const uint32_t *p_histogram, const PackedFloat64Array &p_sorted_values);

	String get_name() const;
	int64_t get_sample_count() const;
	double get_minimum() const;
	double get_maximum() const;
	double get_mean() const;
	double get_p50() const;
	double get_p95() const;
	double get_p99() const;
	bool is_exact() const;

	double get_percentile(double p_percentile) const;
	PackedInt64Array get_histogram(const PackedFloat64Array &p_limits) const;
};

// Summary of a PerformanceRecorder run: statistics per column and frame-time hitches.
class PerformanceSummary : public RefCounted {
	GDCLASS(PerformanceSummary, RefCounted);

	int64_t frame_count = 0;
	int64_t recorded_frame_count = 0;
	int64_t duration_usec = 0;
	int64_t hitch_threshold_usec = 0;
	int64_t hitch_count = 0;
	PackedInt64Array hitch_frames;
	bool percentiles_exact = true;
	LocalVector<Ref<PerformanceColumnSummary>> columns;

protected:
	static void _bind_methods();

public:
	// Default limits of the frame-time histogram in to_text(), in microseconds.
	static PackedFloat64Array get_default_frame_time_limits();

	// Fills the run values (C++ only).
	void set_run(int64_t p_frame_count, int64_t p_recorded_frame_count, int64_t p_duration_usec, bool p_percentiles_exact);
	void set_hitches(int64_t p_threshold_usec, int64_t p_count, const PackedInt64Array &p_frames);
	void add_column(const Ref<PerformanceColumnSummary> &p_column);

	int64_t get_frame_count() const;
	int64_t get_recorded_frame_count() const;
	int64_t get_duration_usec() const;
	bool is_percentiles_exact() const;
	int64_t get_hitch_threshold_usec() const;
	int64_t get_hitch_count() const;
	PackedInt64Array get_hitch_frames() const;

	PackedStringArray get_column_names() const;
	bool has_column(const String &p_name) const;
	Ref<PerformanceColumnSummary> get_column(const String &p_name) const;
	// Like get_column(), without an error when the column is missing.
	Ref<PerformanceColumnSummary> find_column(const String &p_name) const;
	TypedArray<PerformanceColumnSummary> get_columns() const;

	// The summary as JSON text (the content of the .summary.json file).
	String to_text() const;
};
