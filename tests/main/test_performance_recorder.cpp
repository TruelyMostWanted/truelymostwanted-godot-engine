/**************************************************************************/
/*  test_performance_recorder.cpp                                         */
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

TEST_FORCE_LINK(test_performance_recorder)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "main/performance_recorder.h"
#include "main/performance_recording_profile.h"
#include "tests/test_utils.h"

namespace TestPerformanceRecorder {

// Creates a recorder for one test when the engine did not create the singleton.
struct RecorderScope {
	PerformanceRecorder *recorder = nullptr;
	bool owned = false;

	RecorderScope() {
		GDREGISTER_CLASS(PerformanceRecordingProfile);
		GDREGISTER_CLASS(PerformanceRecorder);
		recorder = PerformanceRecorder::get_singleton();
		if (!recorder) {
			recorder = memnew(PerformanceRecorder);
			owned = true;
		}
	}

	~RecorderScope() {
		recorder->stop();
		if (owned) {
			memdelete(recorder);
		}
	}
};

// A profile with only the given builtins and metrics, without Performance monitors and without a file.
static Ref<PerformanceRecordingProfile> make_profile(const PackedInt32Array &p_builtins, const PackedStringArray &p_metrics) {
	Ref<PerformanceRecordingProfile> profile;
	profile.instantiate();
	profile->set_builtins(p_builtins);
	profile->set_monitors(PackedInt32Array());
	profile->set_metrics(p_metrics);
	profile->set_flush_interval_frames(0);
	profile->set_output_path("");
	return profile;
}

static Vector<String> read_lines(const String &p_path) {
	return FileAccess::get_file_as_string(p_path).split("\n", false);
}

struct TestSource : public PerformanceRecorder::MetricSource {
	int samples = 0;

	void get_metric_names(Vector<StringName> &r_names) const override {
		r_names.push_back("alpha");
		r_names.push_back("beta");
	}

	void sample(double *r_values) override {
		samples++;
		r_values[0] = samples;
		r_values[1] = 0.5;
	}
};

TEST_CASE("[PerformanceRecorder] Default profile") {
	Ref<PerformanceRecordingProfile> profile;
	profile.instantiate();
	CHECK(profile->get_builtins().size() == 8);
	CHECK(profile->get_builtins()[0] == PerformanceRecorder::BUILTIN_FRAME);
	CHECK(profile->get_monitors().size() == 8);
	CHECK(profile->get_sample_interval_frames() == 1);
	CHECK(profile->get_buffer_frames() == 3600);
	CHECK(profile->get_flush_interval_frames() == 300);
	CHECK(profile->get_output_path() == "user://performance/{date}_{time}.csv");
	CHECK(profile->get_delimiter() == ",");
	CHECK(profile->get_hitch_threshold_usec() == 50000);
	CHECK_FALSE(profile->is_write_summary_json());

	profile->set_sample_interval_frames(0);
	CHECK(profile->get_sample_interval_frames() == 1);
	profile->set_flush_interval_frames(-5);
	CHECK(profile->get_flush_interval_frames() == 0);
}

TEST_CASE("[PerformanceRecorder] Metric registration") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const int enemies = recorder->register_metric("test_enemies");
	CHECK(enemies >= 0);
	CHECK(recorder->register_metric("test_enemies") == enemies);
	CHECK(recorder->get_metric_id("test_enemies") == enemies);
	CHECK(recorder->get_metric_id("test_unknown_metric") == -1);
}

TEST_CASE("[PerformanceRecorder] Columns and ring buffer wrap") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const int id = recorder->register_metric("test_wrap");
	PackedInt32Array builtins;
	builtins.push_back(PerformanceRecorder::BUILTIN_FRAME_TIME);
	builtins.push_back(PerformanceRecorder::BUILTIN_PHYSICS_STEPS);
	PackedStringArray metrics;
	metrics.push_back("test_wrap");
	Ref<PerformanceRecordingProfile> profile = make_profile(builtins, metrics);
	profile->set_buffer_frames(4);

	REQUIRE(recorder->start(profile) == OK);
	CHECK(recorder->is_recording());
	const PackedStringArray names = recorder->get_column_names();
	REQUIRE(names.size() == 3);
	CHECK(names[0] == "frame_time");
	CHECK(names[1] == "physics_steps");
	CHECK(names[2] == "test_wrap");

	for (int i = 0; i < 10; i++) {
		recorder->set_metric(id, i);
		recorder->frame(1000 + i, 500, 200, i % 3);
	}
	CHECK(recorder->get_recorded_frame_count() == 10);

	// Only the last four rows are kept, oldest first.
	const PackedFloat64Array values = recorder->get_column("test_wrap");
	REQUIRE(values.size() == 4);
	CHECK(values[0] == 6.0);
	CHECK(values[3] == 9.0);
	const PackedFloat64Array frame_times = recorder->get_column("frame_time");
	CHECK(frame_times[0] == 1006.0);
	CHECK(frame_times[3] == 1009.0);
	const PackedFloat64Array steps = recorder->get_column("physics_steps");
	CHECK(steps[3] == 0.0); // 9 % 3

	recorder->stop();
	CHECK_FALSE(recorder->is_recording());
	// The data stays readable after stop().
	CHECK(recorder->get_column("test_wrap").size() == 4);
}

TEST_CASE("[PerformanceRecorder] Gauges keep their value, counters reset each row") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const int gauge = recorder->register_metric("test_gauge", PerformanceRecorder::METRIC_GAUGE);
	const int counter = recorder->register_metric("test_counter", PerformanceRecorder::METRIC_COUNTER);
	PackedStringArray metrics;
	metrics.push_back("test_gauge");
	metrics.push_back("test_counter");
	REQUIRE(recorder->start(make_profile(PackedInt32Array(), metrics)) == OK);

	recorder->frame(0, 0, 0, 0); // Gauge never set: empty value.
	recorder->set_metric(gauge, 7.0);
	recorder->add_metric(counter, 1.0);
	recorder->add_metric(counter, 2.0);
	recorder->frame(0, 0, 0, 0);
	recorder->frame(0, 0, 0, 0);
	recorder->add_metric(gauge, 0.5);
	recorder->add_metric(counter, 5.0);
	recorder->frame(0, 0, 0, 0);

	const PackedFloat64Array gauges = recorder->get_column("test_gauge");
	const PackedFloat64Array counters = recorder->get_column("test_counter");
	REQUIRE(gauges.size() == 4);
	CHECK(Math::is_nan(gauges[0]));
	CHECK(gauges[1] == 7.0);
	CHECK(gauges[2] == 7.0);
	CHECK(gauges[3] == 7.5);
	CHECK(counters[0] == 0.0);
	CHECK(counters[1] == 3.0);
	CHECK(counters[2] == 0.0);
	CHECK(counters[3] == 5.0);
}

TEST_CASE("[PerformanceRecorder] Sample interval sums counters between rows") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const int counter = recorder->register_metric("test_interval_counter", PerformanceRecorder::METRIC_COUNTER);
	PackedStringArray metrics;
	metrics.push_back("test_interval_counter");
	Ref<PerformanceRecordingProfile> profile = make_profile(PackedInt32Array(), metrics);
	profile->set_sample_interval_frames(2);
	REQUIRE(recorder->start(profile) == OK);

	for (int i = 0; i < 6; i++) {
		recorder->add_metric(counter, 1.0);
		recorder->frame(0, 0, 0, 0);
	}
	// Rows on frames 1, 3 and 5.
	CHECK(recorder->get_recorded_frame_count() == 3);
	const PackedFloat64Array counters = recorder->get_column("test_interval_counter");
	REQUIRE(counters.size() == 3);
	CHECK(counters[0] == 1.0);
	CHECK(counters[1] == 2.0);
	CHECK(counters[2] == 2.0);
}

TEST_CASE("[PerformanceRecorder] Native metric source") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	TestSource source;
	recorder->register_metric_source("test_source", &source);
	CHECK(recorder->has_metric_source("test_source"));

	Ref<PerformanceRecordingProfile> profile = make_profile(PackedInt32Array(), PackedStringArray());
	PackedStringArray sources;
	sources.push_back("test_source");
	profile->set_metric_sources(sources);
	REQUIRE(recorder->start(profile) == OK);

	const PackedStringArray names = recorder->get_column_names();
	CHECK(names.has("test_source/alpha"));
	CHECK(names.has("test_source/beta"));

	recorder->frame(0, 0, 0, 0);
	recorder->frame(0, 0, 0, 0);
	const PackedFloat64Array alpha = recorder->get_column("test_source/alpha");
	REQUIRE(alpha.size() == 2);
	CHECK(alpha[0] == 1.0);
	CHECK(alpha[1] == 2.0);
	CHECK(recorder->get_column("test_source/beta")[1] == 0.5);

	// After unregistering, the columns stay but get no more values.
	recorder->unregister_metric_source("test_source");
	CHECK_FALSE(recorder->has_metric_source("test_source"));
	recorder->frame(0, 0, 0, 0);
	CHECK(Math::is_nan(recorder->get_column("test_source/alpha")[2]));
	CHECK(source.samples == 2);
	recorder->stop();
}

TEST_CASE("[PerformanceRecorder] CSV header and rows") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const String path = TestUtils::get_temp_path("performance_recorder/run.csv");
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}

	const int id = recorder->register_metric("test_csv");
	PackedInt32Array builtins;
	builtins.push_back(PerformanceRecorder::BUILTIN_FRAME_TIME);
	builtins.push_back(PerformanceRecorder::BUILTIN_PHYSICS_STEPS);
	PackedStringArray metrics;
	metrics.push_back("test_csv");
	Ref<PerformanceRecordingProfile> profile = make_profile(builtins, metrics);
	profile->set_output_path(path);
	profile->set_flush_interval_frames(2);
	profile->set_flush_budget_usec(0);
	REQUIRE(recorder->start(profile) == OK);
	CHECK(recorder->get_output_path() == path);

	recorder->frame(1000, 0, 0, 1); // test_csv is not set yet: empty cell.
	recorder->set_metric(id, 1.5);
	recorder->frame(2000, 0, 0, 2); // Second row: the flush interval is reached.

	Vector<String> lines = read_lines(path);
	REQUIRE(lines.size() == 3);
	CHECK(lines[0] == "frame_time,physics_steps,test_csv");
	CHECK(lines[1] == "1000,1,");
	CHECK(lines[2] == "2000,2,1.5");

	recorder->set_metric(id, -3.0);
	recorder->frame(3000, 0, 0, 0); // Not flushed yet.
	CHECK(read_lines(path).size() == 3);

	recorder->stop(); // Writes the rest.
	lines = read_lines(path);
	REQUIRE(lines.size() == 4);
	CHECK(lines[3] == "3000,0,-3");

	DirAccess::remove_absolute(path);
}

TEST_CASE("[PerformanceRecorder] Memory-only mode writes the last rows on flush") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	const String path = TestUtils::get_temp_path("performance_recorder/last_rows.csv");
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}

	const int id = recorder->register_metric("test_last");
	PackedStringArray metrics;
	metrics.push_back("test_last");
	Ref<PerformanceRecordingProfile> profile = make_profile(PackedInt32Array(), metrics);
	profile->set_output_path(path);
	profile->set_flush_interval_frames(0);
	profile->set_buffer_frames(3);
	profile->set_delimiter(";");
	REQUIRE(recorder->start(profile) == OK);

	for (int i = 1; i <= 5; i++) {
		recorder->set_metric(id, i);
		recorder->frame(0, 0, 0, 0);
	}
	CHECK_FALSE(FileAccess::exists(path)); // Nothing is written before flush().

	CHECK(recorder->flush() == OK);
	const Vector<String> lines = read_lines(path);
	REQUIRE(lines.size() == 4);
	CHECK(lines[0] == "test_last");
	CHECK(lines[1] == "3");
	CHECK(lines[3] == "5");

	recorder->stop();
	CHECK(read_lines(path).size() == 4); // No row twice.
	DirAccess::remove_absolute(path);
}

TEST_CASE("[PerformanceRecorder] Flush without an output path") {
	RecorderScope scope;
	PerformanceRecorder *recorder = scope.recorder;

	REQUIRE(recorder->start(make_profile(PackedInt32Array(), PackedStringArray())) == OK);
	ERR_PRINT_OFF;
	CHECK(recorder->flush() == ERR_UNCONFIGURED);
	ERR_PRINT_ON;
	recorder->stop();
}

} // namespace TestPerformanceRecorder
