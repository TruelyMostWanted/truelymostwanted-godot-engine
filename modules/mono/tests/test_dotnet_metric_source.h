/**************************************************************************/
/*  test_dotnet_metric_source.h                                           */
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

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"
#include "main/performance_recorder.h"
#include "main/performance_recording_profile.h"
#include "tests/test_macros.h"

#include "modules/mono/dotnet_metric_source.h"

namespace TestDotNetMetricSource {

TEST_CASE("[Modules][Mono][DotNetMetricSource] Metric names") {
	DotNetMetricSource source;
	Vector<StringName> names;
	source.get_metric_names(names);
	CHECK(names.size() == DotNetMetricSource::VALUE_MAX);

	HashSet<StringName> unique;
	for (const StringName &value_name : names) {
		CHECK_FALSE(String(value_name).is_empty());
		unique.insert(value_name);
	}
	CHECK(unique.size() == (uint32_t)DotNetMetricSource::VALUE_MAX);

	CHECK(names[DotNetMetricSource::VALUE_GC_ALLOCATED_BYTES] == StringName("gc_allocated_bytes"));
	CHECK(names[DotNetMetricSource::VALUE_GC_COLLECTIONS_GEN0] == StringName("gc_collections_gen0"));
	CHECK(names[DotNetMetricSource::VALUE_JIT_COMPILED_METHODS] == StringName("jit_compiled_methods"));
	CHECK(names[DotNetMetricSource::VALUE_GC_ALLOCATED_BYTES_DELTA] == StringName("gc_allocated_bytes_delta"));
	CHECK(names[DotNetMetricSource::VALUE_GC_PAUSE_DELTA_MS] == StringName("gc_pause_delta_ms"));
	CHECK(String(DotNetMetricSource::SOURCE_NAME) == "dotnet");
}

TEST_CASE("[Modules][Mono][DotNetMetricSource] Columns in PerformanceRecorder") {
	GDREGISTER_CLASS(PerformanceRecordingProfile);
	GDREGISTER_CLASS(PerformanceRecorder);
	PerformanceRecorder *recorder = PerformanceRecorder::get_singleton();
	bool owned = false;
	if (!recorder) {
		recorder = memnew(PerformanceRecorder);
		owned = true;
	}

	DotNetMetricSource source;
	const StringName source_name = "dotnet_test";
	recorder->register_metric_source(source_name, &source);
	CHECK(recorder->has_metric_source(source_name));

	Ref<PerformanceRecordingProfile> profile;
	profile.instantiate();
	profile->set_builtins(PackedInt32Array());
	profile->set_monitors(PackedInt32Array());
	PackedStringArray sources;
	sources.push_back(String(source_name));
	profile->set_metric_sources(sources);
	profile->set_flush_interval_frames(0);
	profile->set_output_path("");

	CHECK(recorder->start(profile) == OK);
	const PackedStringArray columns = recorder->get_column_names();
	CHECK(columns.size() == DotNetMetricSource::VALUE_MAX);
	CHECK(columns.has("dotnet_test/gc_allocated_bytes"));
	CHECK(columns.has("dotnet_test/gc_collections_gen2"));
	CHECK(columns.has("dotnet_test/threadpool_threads"));
	CHECK(columns.has("dotnet_test/gc_allocated_bytes_delta"));

	recorder->frame(16000, 1000, 1000, 1);
	recorder->frame(16000, 1000, 1000, 1);
	CHECK(recorder->get_recorded_frame_count() == 2);

	// The first row of a recording has no previous row, so its deltas have no value.
	const PackedFloat64Array deltas = recorder->get_column("dotnet_test/gc_allocated_bytes_delta");
	REQUIRE(deltas.size() == 2);
	CHECK(Math::is_nan(deltas[0]));

	// Without a .NET runtime (unit tests) the managed values have no value; with one, the total only grows.
	const PackedFloat64Array allocated = recorder->get_column("dotnet_test/gc_allocated_bytes");
	REQUIRE(allocated.size() == 2);
	CHECK((Math::is_nan(allocated[0]) || allocated[1] >= allocated[0]));

	recorder->stop();
	recorder->unregister_metric_source(source_name);
	CHECK_FALSE(recorder->has_metric_source(source_name));
	if (owned) {
		memdelete(recorder);
	}
}

} // namespace TestDotNetMetricSource
