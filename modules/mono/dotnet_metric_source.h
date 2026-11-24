/**************************************************************************/
/*  dotnet_metric_source.h                                                */
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

#include "main/performance_recorder.h"

// .NET runtime metrics (garbage collector, allocations, thread pool, JIT) as the
// PerformanceRecorder metric source "dotnet" and as Performance custom monitors "dotnet/<name>".
// One sample is a single native-to-managed call (ManagedCallbacks::RuntimeMetrics_Sample)
// that writes all managed values into a buffer owned by this class; it does not allocate.
class DotNetMetricSource : public PerformanceRecorder::MetricSource {
public:
	// The first VALUE_MANAGED_MAX values are written by the managed side, in this order
	// (must match Godot.Bridge.RuntimeMetrics in GodotSharp). The deltas are computed here.
	enum Value {
		VALUE_GC_ALLOCATED_BYTES,
		VALUE_GC_TOTAL_MEMORY_BYTES,
		VALUE_GC_HEAP_SIZE_BYTES,
		VALUE_GC_FRAGMENTED_BYTES,
		VALUE_GC_COMMITTED_BYTES,
		VALUE_GC_PAUSE_TIME_PERCENT,
		VALUE_GC_PAUSE_TOTAL_MS,
		VALUE_GC_COLLECTIONS_GEN0,
		VALUE_GC_COLLECTIONS_GEN1,
		VALUE_GC_COLLECTIONS_GEN2,
		VALUE_THREADPOOL_THREADS,
		VALUE_THREADPOOL_PENDING,
		VALUE_THREADPOOL_COMPLETED,
		VALUE_LOCK_CONTENTIONS,
		VALUE_JIT_COMPILED_METHODS,
		VALUE_MANAGED_MAX,
		VALUE_GC_ALLOCATED_BYTES_DELTA = VALUE_MANAGED_MAX,
		VALUE_GC_COLLECTIONS_DELTA,
		VALUE_GC_PAUSE_DELTA_MS,
		VALUE_MAX,
	};

	static constexpr const char *SOURCE_NAME = "dotnet";

private:
	static DotNetMetricSource *singleton;

	double current[VALUE_MANAGED_MAX];
	double previous_row[VALUE_MANAGED_MAX];
	bool has_previous_row = false;
	uint64_t cached_frame = 0;
	bool cache_valid = false;
	bool monitors_added = false;
	bool source_registered = false;

	// One managed call; fills `current` (NaN when the runtime is not available).
	bool _sample_managed();
	static Variant _get_monitor(int p_value);

	void _register();
	void _unregister();

public:
	static DotNetMetricSource *get_singleton();

	// Called by CSharpLanguage when the .NET runtime is initialized and before it is finalized.
	static void create_singleton();
	static void free_singleton();

	static const char *get_value_name(int p_value);

	// Latest values (refreshed with one managed call at most once per process frame on the main thread).
	double get_value(int p_value);

	// PerformanceRecorder::MetricSource
	virtual void get_metric_names(Vector<StringName> &r_names) const override;
	virtual void sample(double *r_values) override;

	DotNetMetricSource();
	virtual ~DotNetMetricSource() override;
};
