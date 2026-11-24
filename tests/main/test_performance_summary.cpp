/**************************************************************************/
/*  test_performance_summary.cpp                                          */
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

TEST_FORCE_LINK(test_performance_summary)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "main/performance_budget.h"
#include "main/performance_recorder.h"
#include "main/performance_recording_profile.h"
#include "main/performance_summary.h"
#include "tests/test_utils.h"

namespace TestPerformanceSummary {

// Creates a recorder for one test when the engine did not create the singleton.
struct SummaryRecorderScope {
	PerformanceRecorder *recorder = nullptr;
	bool owned = false;

	SummaryRecorderScope() {
		GDREGISTER_CLASS(PerformanceRecordingProfile);
		GDREGISTER_CLASS(PerformanceRecorder);
		GDREGISTER_CLASS(PerformanceColumnSummary);
		GDREGISTER_CLASS(PerformanceSummary);
		GDREGISTER_CLASS(PerformanceBudgetRule);
		GDREGISTER_CLASS(PerformanceBudget);
		GDREGISTER_CLASS(PerformanceBudgetFailure);
		GDREGISTER_CLASS(PerformanceBudgetResult);
		recorder = PerformanceRecorder::get_singleton();
		if (!recorder) {
			recorder = memnew(PerformanceRecorder);
			owned = true;
		}
	}

	~SummaryRecorderScope() {
		recorder->stop();
		if (owned) {
			memdelete(recorder);
		}
	}
};

// A profile with the frame_time builtin only, without monitors and without a file.
static Ref<PerformanceRecordingProfile> make_frame_time_profile(int p_buffer_frames, int p_hitch_threshold_usec) {
	Ref<PerformanceRecordingProfile> profile;
	profile.instantiate();
	PackedInt32Array builtins;
	builtins.push_back(PerformanceRecorder::BUILTIN_FRAME_TIME);
	profile->set_builtins(builtins);
	profile->set_monitors(PackedInt32Array());
	profile->set_metrics(PackedStringArray());
	profile->set_flush_interval_frames(0);
	profile->set_output_path("");
	profile->set_buffer_frames(p_buffer_frames);
	profile->set_hitch_threshold_usec(p_hitch_threshold_usec);
	return profile;
}

static Ref<PerformanceBudgetRule> make_rule(const String &p_column, PerformanceBudgetRule::Statistic p_statistic, double p_limit, double p_percentile = 95.0) {
	Ref<PerformanceBudgetRule> rule;
	rule.instantiate();
	rule->set_column(p_column);
	rule->set_statistic(p_statistic);
	rule->set_limit(p_limit);
	rule->set_percentile(p_percentile);
	return rule;
}

TEST_CASE("[PerformanceSummary] Histogram buckets are ordered and contain their values") {
	const double values[] = { -5000.0, -1.0, -0.001, 0.0, 0.0001, 0.5, 1.0, 1.5, 16667.0, 1e9, 1e20 };
	int previous = -1;
	for (double value : values) {
		const int bucket = PerformanceColumnSummary::get_histogram_bucket(value);
		CHECK(bucket >= 0);
		CHECK(bucket < PerformanceColumnSummary::HISTOGRAM_BUCKETS);
		CHECK(bucket >= previous);
		previous = bucket;
		double low = 0.0;
		double high = 0.0;
		PerformanceColumnSummary::get_histogram_bucket_range(bucket, low, high);
		CHECK(value >= low);
		CHECK(value <= high);
	}
	CHECK(PerformanceColumnSummary::get_histogram_bucket(0.0) == PerformanceColumnSummary::HISTOGRAM_ZERO_BUCKET);
}

TEST_CASE("[PerformanceSummary] Exact percentiles while the run fits in the ring buffer") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	REQUIRE(recorder->start(make_frame_time_profile(200, 0)) == OK);
	for (int i = 100; i >= 1; i--) {
		recorder->frame(i, 0, 0, 0);
	}
	const Ref<PerformanceSummary> summary = recorder->get_summary();
	REQUIRE(summary.is_valid());
	CHECK(summary->get_frame_count() == 100);
	CHECK(summary->get_recorded_frame_count() == 100);
	CHECK(summary->is_percentiles_exact());
	CHECK(summary->get_column_names().size() == 1);
	REQUIRE(summary->has_column("frame_time"));
	CHECK_FALSE(summary->has_column("missing"));

	const Ref<PerformanceColumnSummary> column = summary->get_column("frame_time");
	REQUIRE(column.is_valid());
	CHECK(column->is_exact());
	CHECK(column->get_sample_count() == 100);
	CHECK(column->get_minimum() == 1.0);
	CHECK(column->get_maximum() == 100.0);
	CHECK(column->get_mean() == doctest::Approx(50.5));
	CHECK(column->get_p50() == doctest::Approx(50.5));
	CHECK(column->get_p95() == doctest::Approx(95.05));
	CHECK(column->get_p99() == doctest::Approx(99.01));
	CHECK(column->get_percentile(0.0) == 1.0);
	CHECK(column->get_percentile(100.0) == 100.0);

	// Ranges: <= 10, (10, 50], > 50.
	PackedFloat64Array limits;
	limits.push_back(10.0);
	limits.push_back(50.0);
	const PackedInt64Array histogram = column->get_histogram(limits);
	REQUIRE(histogram.size() == 3);
	CHECK(histogram[0] == 10);
	CHECK(histogram[1] == 40);
	CHECK(histogram[2] == 50);

	// The summary stays readable after stop().
	recorder->stop();
	CHECK(recorder->get_summary()->get_column("frame_time")->get_sample_count() == 100);
}

TEST_CASE("[PerformanceSummary] Histogram percentiles for runs longer than the ring buffer") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	REQUIRE(recorder->start(make_frame_time_profile(16, 0)) == OK);
	for (int i = 1; i <= 10000; i++) {
		recorder->frame(1000 + i, 0, 0, 0);
	}
	const Ref<PerformanceSummary> summary = recorder->get_summary();
	CHECK_FALSE(summary->is_percentiles_exact());
	const Ref<PerformanceColumnSummary> column = summary->get_column("frame_time");
	REQUIRE(column.is_valid());
	CHECK_FALSE(column->is_exact());
	CHECK(column->get_sample_count() == 10000);
	// Minimum, maximum and mean are exact over the whole run.
	CHECK(column->get_minimum() == 1001.0);
	CHECK(column->get_maximum() == 11000.0);
	CHECK(column->get_mean() == doctest::Approx(6000.5));
	// Percentiles are within one bucket (2^(1/8) - 1 = 9.05 %) of the exact value.
	CHECK(Math::abs(column->get_p50() - 6000.5) / 6000.5 < 0.0905);
	CHECK(Math::abs(column->get_p95() - 10500.95) / 10500.95 < 0.0905);
	CHECK(Math::abs(column->get_p99() - 10900.99) / 10900.99 < 0.0905);
	CHECK(column->get_p99() <= column->get_maximum());

	PackedFloat64Array limits;
	limits.push_back(5000.0);
	const PackedInt64Array histogram = column->get_histogram(limits);
	REQUIRE(histogram.size() == 2);
	CHECK(histogram[0] + histogram[1] == 10000);
}

TEST_CASE("[PerformanceSummary] Hitches are counted on every frame") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	Ref<PerformanceRecordingProfile> profile = make_frame_time_profile(100, 20000);
	profile->set_sample_interval_frames(4);
	REQUIRE(recorder->start(profile) == OK);
	for (int i = 0; i < 20; i++) {
		// Frames 2, 7, 12 and 17 are hitches; with a sample interval of 4 only frame 12 is a recorded row.
		recorder->frame(i % 5 == 2 ? 40000 : 16000, 0, 0, 0);
	}
	const Ref<PerformanceSummary> summary = recorder->get_summary();
	CHECK(summary->get_frame_count() == 20);
	CHECK(summary->get_recorded_frame_count() == 5);
	CHECK(summary->get_hitch_threshold_usec() == 20000);
	CHECK(summary->get_hitch_count() == 4);
	CHECK(summary->get_hitch_frames().size() == 4);

	// A threshold of 0 disables hitch detection.
	REQUIRE(recorder->start(make_frame_time_profile(100, 0)) == OK);
	recorder->frame(1000000, 0, 0, 0);
	CHECK(recorder->get_summary()->get_hitch_count() == 0);
}

TEST_CASE("[PerformanceSummary] Empty cells are skipped") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const int id = recorder->register_metric("test_summary_gauge");
	Ref<PerformanceRecordingProfile> profile = make_frame_time_profile(100, 0);
	PackedStringArray metrics;
	metrics.push_back("test_summary_gauge");
	profile->set_metrics(metrics);
	recorder->set_metric(id, Math::NaN);
	REQUIRE(recorder->start(profile) == OK);
	recorder->frame(1000, 0, 0, 0);
	recorder->set_metric(id, 4.0);
	recorder->frame(1000, 0, 0, 0);
	recorder->set_metric(id, 8.0);
	recorder->frame(1000, 0, 0, 0);

	const Ref<PerformanceColumnSummary> column = recorder->get_summary()->get_column("test_summary_gauge");
	REQUIRE(column.is_valid());
	CHECK(column->get_sample_count() == 2);
	CHECK(column->get_minimum() == 4.0);
	CHECK(column->get_maximum() == 8.0);
	CHECK(column->get_mean() == 6.0);
}

TEST_CASE("[PerformanceBudget] Check a summary") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	REQUIRE(recorder->start(make_frame_time_profile(200, 30000)) == OK);
	for (int i = 0; i < 100; i++) {
		recorder->frame(i == 50 ? 40000 : 10000, 0, 0, 0);
	}
	recorder->stop();

	Ref<PerformanceBudget> budget;
	budget.instantiate();
	budget->add_rule(make_rule("frame_time", PerformanceBudgetRule::STATISTIC_PERCENTILE, 16667.0, 95.0));
	budget->add_rule(make_rule("frame_time", PerformanceBudgetRule::STATISTIC_MEAN, 20000.0));
	CHECK(budget->get_rule_count() == 2);

	Ref<PerformanceBudgetResult> result = recorder->check_budget(budget);
	REQUIRE(result.is_valid());
	CHECK(result->is_passed());
	CHECK(result->get_checked_rule_count() == 2);
	CHECK(result->get_failure_count() == 0);
	CHECK(result->get_exit_code() == 0);

	// Over the limit, a missing column, and too many hitches.
	budget->add_rule(make_rule("frame_time", PerformanceBudgetRule::STATISTIC_MAXIMUM, 30000.0));
	budget->add_rule(make_rule("missing_column", PerformanceBudgetRule::STATISTIC_MEAN, 1.0));
	budget->add_rule(make_rule("", PerformanceBudgetRule::STATISTIC_HITCH_COUNT, 0.0));
	result = recorder->check_budget(budget);
	CHECK_FALSE(result->is_passed());
	CHECK(result->get_checked_rule_count() == 5);
	REQUIRE(result->get_failure_count() == 3);
	CHECK(result->get_exit_code() == 1);

	const TypedArray<PerformanceBudgetFailure> failures = result->get_failures();
	const Ref<PerformanceBudgetFailure> over_limit = failures[0];
	CHECK(over_limit->get_reason() == PerformanceBudgetFailure::REASON_OVER_LIMIT);
	CHECK(over_limit->get_actual_value() == 40000.0);
	CHECK(over_limit->get_limit() == 30000.0);
	CHECK(over_limit->get_message().contains("frame_time max"));
	const Ref<PerformanceBudgetFailure> missing = failures[1];
	CHECK(missing->get_reason() == PerformanceBudgetFailure::REASON_COLUMN_MISSING);
	const Ref<PerformanceBudgetFailure> hitches = failures[2];
	CHECK(hitches->get_reason() == PerformanceBudgetFailure::REASON_OVER_LIMIT);
	CHECK(hitches->get_actual_value() == 1.0);
	CHECK(result->to_text().contains("3 of 5"));
}

TEST_CASE("[PerformanceBudget] A column without samples fails") {
	Ref<PerformanceColumnSummary> column;
	column.instantiate();
	column->set_data("empty", 0, 0.0, 0.0, 0.0, nullptr, PackedFloat64Array());
	Ref<PerformanceSummary> summary;
	summary.instantiate();
	summary->add_column(column);

	Ref<PerformanceBudget> budget;
	budget.instantiate();
	budget->add_rule(make_rule("empty", PerformanceBudgetRule::STATISTIC_MAXIMUM, 1.0));
	const Ref<PerformanceBudgetResult> result = budget->check_summary(summary);
	REQUIRE(result->get_failure_count() == 1);
	const Ref<PerformanceBudgetFailure> failure = result->get_failures()[0];
	CHECK(failure->get_reason() == PerformanceBudgetFailure::REASON_NO_SAMPLES);
}

TEST_CASE("[PerformanceSummary] JSON file next to the CSV") {
	SummaryRecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const String path = TestUtils::get_temp_path("performance_summary/run.csv");
	const String json_path = TestUtils::get_temp_path("performance_summary/run.summary.json");
	if (FileAccess::exists(json_path)) {
		DirAccess::remove_absolute(json_path);
	}
	Ref<PerformanceRecordingProfile> profile = make_frame_time_profile(100, 15000);
	profile->set_output_path(path);
	profile->set_write_summary_json(true);
	REQUIRE(recorder->start(profile) == OK);
	recorder->frame(10000, 0, 0, 0);
	recorder->frame(20000, 0, 0, 0);
	recorder->stop();

	REQUIRE(FileAccess::exists(json_path));
	const Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(json_path));
	REQUIRE(parsed.get_type() == Variant::DICTIONARY);
	const Dictionary root = parsed;
	CHECK(int64_t(root["frame_count"]) == 2);
	const Dictionary hitches = root["hitches"];
	CHECK(int64_t(hitches["count"]) == 1);
	const Dictionary columns = root["columns"];
	REQUIRE(columns.has("frame_time"));
	const Dictionary frame_time = columns["frame_time"];
	CHECK(double(frame_time["max"]) == 20000.0);
	CHECK(double(frame_time["avg"]) == 15000.0);
	CHECK(root.has("frame_time_histogram"));
}

} // namespace TestPerformanceSummary
