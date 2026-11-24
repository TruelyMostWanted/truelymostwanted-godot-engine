/**************************************************************************/
/*  performance_summary.cpp                                               */
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

#include "performance_summary.h"

#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

/* PerformanceColumnSummary */

int PerformanceColumnSummary::get_histogram_bucket(double p_value) {
	if (Math::is_nan(p_value) || p_value == 0.0) {
		return HISTOGRAM_ZERO_BUCKET;
	}
	const double magnitude = Math::abs(p_value);
	int index = 0;
	if (magnitude >= Math::pow(2.0, (double)HISTOGRAM_MIN_EXPONENT)) {
		const double scaled = (Math::log2(magnitude) - HISTOGRAM_MIN_EXPONENT) * HISTOGRAM_SUBDIVISIONS;
		index = scaled >= HISTOGRAM_SIDE_BUCKETS ? HISTOGRAM_SIDE_BUCKETS - 1 : (int)Math::floor(scaled);
		index = CLAMP(index, 0, HISTOGRAM_SIDE_BUCKETS - 1);
	}
	return p_value > 0.0 ? HISTOGRAM_ZERO_BUCKET + 1 + index : HISTOGRAM_ZERO_BUCKET - 1 - index;
}

void PerformanceColumnSummary::get_histogram_bucket_range(int p_bucket, double &r_low, double &r_high) {
	r_low = 0.0;
	r_high = 0.0;
	ERR_FAIL_INDEX(p_bucket, HISTOGRAM_BUCKETS);
	if (p_bucket == HISTOGRAM_ZERO_BUCKET) {
		return;
	}
	const bool positive = p_bucket > HISTOGRAM_ZERO_BUCKET;
	const int index = positive ? p_bucket - HISTOGRAM_ZERO_BUCKET - 1 : HISTOGRAM_ZERO_BUCKET - 1 - p_bucket;
	double low = index == 0 ? 0.0 : Math::pow(2.0, HISTOGRAM_MIN_EXPONENT + (double)index / HISTOGRAM_SUBDIVISIONS);
	double high = index == HISTOGRAM_SIDE_BUCKETS - 1 ? Math::INF : Math::pow(2.0, HISTOGRAM_MIN_EXPONENT + (double)(index + 1) / HISTOGRAM_SUBDIVISIONS);
	if (positive) {
		r_low = low;
		r_high = high;
	} else {
		r_low = -high;
		r_high = -low;
	}
}

double PerformanceColumnSummary::_get_bucket_value(int p_bucket, double p_fraction) const {
	double low = 0.0;
	double high = 0.0;
	get_histogram_bucket_range(p_bucket, low, high);
	// Open and partly empty buckets are bounded by the exact minimum and maximum.
	low = MAX(low, minimum);
	high = MIN(high, maximum);
	if (high < low) {
		return CLAMP(low, minimum, maximum);
	}
	return low + (high - low) * CLAMP(p_fraction, 0.0, 1.0);
}

void PerformanceColumnSummary::set_data(const String &p_name, int64_t p_count, double p_min, double p_max, double p_sum, const uint32_t *p_histogram, const PackedFloat64Array &p_sorted_values) {
	name = p_name;
	sample_count = MAX<int64_t>(0, p_count);
	minimum = sample_count > 0 ? p_min : 0.0;
	maximum = sample_count > 0 ? p_max : 0.0;
	mean = sample_count > 0 ? p_sum / (double)sample_count : 0.0;
	sorted_values = p_sorted_values;
	exact = !sorted_values.is_empty() || sample_count == 0;
	histogram.clear();
	if (!exact && p_histogram) {
		histogram.resize(HISTOGRAM_BUCKETS);
		memcpy(histogram.ptr(), p_histogram, sizeof(uint32_t) * HISTOGRAM_BUCKETS);
	}
	p50 = get_percentile(50.0);
	p95 = get_percentile(95.0);
	p99 = get_percentile(99.0);
}

String PerformanceColumnSummary::get_name() const {
	return name;
}

int64_t PerformanceColumnSummary::get_sample_count() const {
	return sample_count;
}

double PerformanceColumnSummary::get_minimum() const {
	return minimum;
}

double PerformanceColumnSummary::get_maximum() const {
	return maximum;
}

double PerformanceColumnSummary::get_mean() const {
	return mean;
}

double PerformanceColumnSummary::get_p50() const {
	return p50;
}

double PerformanceColumnSummary::get_p95() const {
	return p95;
}

double PerformanceColumnSummary::get_p99() const {
	return p99;
}

bool PerformanceColumnSummary::is_exact() const {
	return exact;
}

double PerformanceColumnSummary::get_percentile(double p_percentile) const {
	if (sample_count == 0) {
		return 0.0;
	}
	const double percentile = CLAMP(p_percentile, 0.0, 100.0);
	if (percentile <= 0.0) {
		return minimum;
	}
	if (percentile >= 100.0) {
		return maximum;
	}

	// Fractional rank, linear interpolation between neighbors (numpy's default "linear" method).
	const double rank = percentile / 100.0 * (double)(sample_count - 1);

	if (!sorted_values.is_empty()) {
		const int64_t count = sorted_values.size();
		const int64_t lower = CLAMP((int64_t)Math::floor(rank), (int64_t)0, count - 1);
		const int64_t upper = MIN(lower + 1, count - 1);
		const double fraction = rank - (double)lower;
		const double *r = sorted_values.ptr();
		return r[lower] + (r[upper] - r[lower]) * fraction;
	}

	if (histogram.size() != (uint32_t)HISTOGRAM_BUCKETS) {
		return mean;
	}
	uint64_t cumulative = 0;
	for (int i = 0; i < HISTOGRAM_BUCKETS; i++) {
		const uint32_t count = histogram[i];
		if (count == 0) {
			continue;
		}
		if ((double)(cumulative + count) > rank) {
			// Samples are assumed to be spread evenly inside the bucket.
			const double fraction = (rank - (double)cumulative + 0.5) / (double)count;
			return CLAMP(_get_bucket_value(i, fraction), minimum, maximum);
		}
		cumulative += count;
	}
	return maximum;
}

PackedInt64Array PerformanceColumnSummary::get_histogram(const PackedFloat64Array &p_limits) const {
	PackedInt64Array result;
	const int64_t limit_count = p_limits.size();
	for (int64_t i = 1; i < limit_count; i++) {
		ERR_FAIL_COND_V_MSG(p_limits[i] <= p_limits[i - 1], result, "PerformanceColumnSummary.get_histogram(): the limits must be in ascending order.");
	}
	result.resize(limit_count + 1);
	result.fill(0);
	int64_t *w = result.ptrw();
	const double *limits = p_limits.ptr();

	// Index of the range a value falls into: range i holds values <= limits[i] and > limits[i - 1].
	auto range_of = [limits, limit_count](double p_value) -> int64_t {
		int64_t low = 0;
		int64_t high = limit_count;
		while (low < high) {
			const int64_t middle = (low + high) / 2;
			if (p_value <= limits[middle]) {
				high = middle;
			} else {
				low = middle + 1;
			}
		}
		return low;
	};

	if (!sorted_values.is_empty()) {
		const double *r = sorted_values.ptr();
		for (int64_t i = 0; i < sorted_values.size(); i++) {
			w[range_of(r[i])]++;
		}
	} else if (histogram.size() == (uint32_t)HISTOGRAM_BUCKETS) {
		// Each bucket is counted in the range of its middle value.
		for (int i = 0; i < HISTOGRAM_BUCKETS; i++) {
			if (histogram[i] > 0) {
				w[range_of(_get_bucket_value(i, 0.5))] += histogram[i];
			}
		}
	}
	return result;
}

void PerformanceColumnSummary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &PerformanceColumnSummary::get_name);
	ClassDB::bind_method(D_METHOD("get_sample_count"), &PerformanceColumnSummary::get_sample_count);
	ClassDB::bind_method(D_METHOD("get_minimum"), &PerformanceColumnSummary::get_minimum);
	ClassDB::bind_method(D_METHOD("get_maximum"), &PerformanceColumnSummary::get_maximum);
	ClassDB::bind_method(D_METHOD("get_mean"), &PerformanceColumnSummary::get_mean);
	ClassDB::bind_method(D_METHOD("get_p50"), &PerformanceColumnSummary::get_p50);
	ClassDB::bind_method(D_METHOD("get_p95"), &PerformanceColumnSummary::get_p95);
	ClassDB::bind_method(D_METHOD("get_p99"), &PerformanceColumnSummary::get_p99);
	ClassDB::bind_method(D_METHOD("is_exact"), &PerformanceColumnSummary::is_exact);
	ClassDB::bind_method(D_METHOD("get_percentile", "percentile"), &PerformanceColumnSummary::get_percentile);
	ClassDB::bind_method(D_METHOD("get_histogram", "limits"), &PerformanceColumnSummary::get_histogram);
}

/* PerformanceSummary */

PackedFloat64Array PerformanceSummary::get_default_frame_time_limits() {
	PackedFloat64Array limits;
	// 240, 120, 60, 30, 20 and 10 frames per second.
	limits.push_back(4167.0);
	limits.push_back(8333.0);
	limits.push_back(16667.0);
	limits.push_back(33333.0);
	limits.push_back(50000.0);
	limits.push_back(100000.0);
	return limits;
}

void PerformanceSummary::set_run(int64_t p_frame_count, int64_t p_recorded_frame_count, int64_t p_duration_usec, bool p_percentiles_exact) {
	frame_count = p_frame_count;
	recorded_frame_count = p_recorded_frame_count;
	duration_usec = p_duration_usec;
	percentiles_exact = p_percentiles_exact;
}

void PerformanceSummary::set_hitches(int64_t p_threshold_usec, int64_t p_count, const PackedInt64Array &p_frames) {
	hitch_threshold_usec = p_threshold_usec;
	hitch_count = p_count;
	hitch_frames = p_frames;
}

void PerformanceSummary::add_column(const Ref<PerformanceColumnSummary> &p_column) {
	ERR_FAIL_COND(p_column.is_null());
	columns.push_back(p_column);
}

int64_t PerformanceSummary::get_frame_count() const {
	return frame_count;
}

int64_t PerformanceSummary::get_recorded_frame_count() const {
	return recorded_frame_count;
}

int64_t PerformanceSummary::get_duration_usec() const {
	return duration_usec;
}

bool PerformanceSummary::is_percentiles_exact() const {
	return percentiles_exact;
}

int64_t PerformanceSummary::get_hitch_threshold_usec() const {
	return hitch_threshold_usec;
}

int64_t PerformanceSummary::get_hitch_count() const {
	return hitch_count;
}

PackedInt64Array PerformanceSummary::get_hitch_frames() const {
	return hitch_frames;
}

PackedStringArray PerformanceSummary::get_column_names() const {
	PackedStringArray result;
	for (const Ref<PerformanceColumnSummary> &column : columns) {
		result.push_back(column->get_name());
	}
	return result;
}

bool PerformanceSummary::has_column(const String &p_name) const {
	return find_column(p_name).is_valid();
}

Ref<PerformanceColumnSummary> PerformanceSummary::find_column(const String &p_name) const {
	for (const Ref<PerformanceColumnSummary> &column : columns) {
		if (column->get_name() == p_name) {
			return column;
		}
	}
	return Ref<PerformanceColumnSummary>();
}

Ref<PerformanceColumnSummary> PerformanceSummary::get_column(const String &p_name) const {
	const Ref<PerformanceColumnSummary> column = find_column(p_name);
	ERR_FAIL_COND_V_MSG(column.is_null(), column, vformat("PerformanceSummary: there is no column \"%s\".", p_name));
	return column;
}

TypedArray<PerformanceColumnSummary> PerformanceSummary::get_columns() const {
	TypedArray<PerformanceColumnSummary> result;
	for (const Ref<PerformanceColumnSummary> &column : columns) {
		result.push_back(column);
	}
	return result;
}

String PerformanceSummary::to_text() const {
	Dictionary root;
	root["frame_count"] = frame_count;
	root["recorded_frame_count"] = recorded_frame_count;
	root["duration_usec"] = duration_usec;
	root["percentiles_exact"] = percentiles_exact;

	Dictionary hitches;
	hitches["threshold_usec"] = hitch_threshold_usec;
	hitches["count"] = hitch_count;
	hitches["frames"] = hitch_frames;
	root["hitches"] = hitches;

	Dictionary column_values;
	for (const Ref<PerformanceColumnSummary> &column : columns) {
		Dictionary values;
		values["count"] = column->get_sample_count();
		values["min"] = column->get_minimum();
		values["max"] = column->get_maximum();
		values["avg"] = column->get_mean();
		values["p50"] = column->get_p50();
		values["p95"] = column->get_p95();
		values["p99"] = column->get_p99();
		column_values[column->get_name()] = values;
	}
	root["columns"] = column_values;

	const Ref<PerformanceColumnSummary> frame_time = find_column("frame_time");
	if (frame_time.is_valid()) {
		Dictionary histogram;
		const PackedFloat64Array limits = get_default_frame_time_limits();
		histogram["limits_usec"] = limits;
		histogram["counts"] = frame_time->get_histogram(limits);
		root["frame_time_histogram"] = histogram;
	}

	return JSON::stringify(root, "\t", false);
}

void PerformanceSummary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_frame_count"), &PerformanceSummary::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_recorded_frame_count"), &PerformanceSummary::get_recorded_frame_count);
	ClassDB::bind_method(D_METHOD("get_duration_usec"), &PerformanceSummary::get_duration_usec);
	ClassDB::bind_method(D_METHOD("is_percentiles_exact"), &PerformanceSummary::is_percentiles_exact);
	ClassDB::bind_method(D_METHOD("get_hitch_threshold_usec"), &PerformanceSummary::get_hitch_threshold_usec);
	ClassDB::bind_method(D_METHOD("get_hitch_count"), &PerformanceSummary::get_hitch_count);
	ClassDB::bind_method(D_METHOD("get_hitch_frames"), &PerformanceSummary::get_hitch_frames);
	ClassDB::bind_method(D_METHOD("get_column_names"), &PerformanceSummary::get_column_names);
	ClassDB::bind_method(D_METHOD("has_column", "name"), &PerformanceSummary::has_column);
	ClassDB::bind_method(D_METHOD("get_column", "name"), &PerformanceSummary::get_column);
	ClassDB::bind_method(D_METHOD("get_columns"), &PerformanceSummary::get_columns);
	ClassDB::bind_method(D_METHOD("to_text"), &PerformanceSummary::to_text);
}
