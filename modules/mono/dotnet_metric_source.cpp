/**************************************************************************/
/*  dotnet_metric_source.cpp                                              */
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

#include "dotnet_metric_source.h"

#include "mono_gd/gd_mono_cache.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "main/performance.h"

DotNetMetricSource *DotNetMetricSource::singleton = nullptr;

static const char *value_names[DotNetMetricSource::VALUE_MAX] = {
	"gc_allocated_bytes",
	"gc_total_memory_bytes",
	"gc_heap_size_bytes",
	"gc_fragmented_bytes",
	"gc_committed_bytes",
	"gc_pause_time_percent",
	"gc_pause_total_ms",
	"gc_collections_gen0",
	"gc_collections_gen1",
	"gc_collections_gen2",
	"threadpool_threads",
	"threadpool_pending",
	"threadpool_completed",
	"lock_contentions",
	"jit_compiled_methods",
	"gc_allocated_bytes_delta",
	"gc_collections_delta",
	"gc_pause_delta_ms",
};

DotNetMetricSource *DotNetMetricSource::get_singleton() {
	return singleton;
}

const char *DotNetMetricSource::get_value_name(int p_value) {
	ERR_FAIL_INDEX_V(p_value, VALUE_MAX, "");
	return value_names[p_value];
}

bool DotNetMetricSource::_sample_managed() {
	int32_t written = 0;
	if (GDMonoCache::godot_api_cache_updated && GDMonoCache::managed_callbacks.RuntimeMetrics_Sample) {
		written = GDMonoCache::managed_callbacks.RuntimeMetrics_Sample(current, VALUE_MANAGED_MAX);
	}
	if (written != VALUE_MANAGED_MAX) {
		for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
			current[i] = Math::NaN;
		}
		cache_valid = false;
		return false;
	}
	cache_valid = true;
	cached_frame = Engine::get_singleton() ? Engine::get_singleton()->get_process_frames() : 0;
	return true;
}

double DotNetMetricSource::get_value(int p_value) {
	ERR_FAIL_INDEX_V(p_value, VALUE_MANAGED_MAX, Math::NaN);
	if (Thread::is_main_thread()) {
		const uint64_t frame = Engine::get_singleton() ? Engine::get_singleton()->get_process_frames() : 0;
		if (!cache_valid || cached_frame != frame) {
			_sample_managed();
		}
	}
	return current[p_value];
}

Variant DotNetMetricSource::_get_monitor(int p_value) {
	if (!singleton) {
		return 0.0;
	}
	const double value = singleton->get_value(p_value);
	return Math::is_nan(value) ? 0.0 : value;
}

void DotNetMetricSource::get_metric_names(Vector<StringName> &r_names) const {
	r_names.clear();
	for (int i = 0; i < VALUE_MAX; i++) {
		r_names.push_back(StringName(value_names[i]));
	}
}

void DotNetMetricSource::sample(double *r_values) {
	const bool valid = _sample_managed();

	// A new recording starts without a previous row.
	PerformanceRecorder *recorder = PerformanceRecorder::get_singleton();
	if (recorder && recorder->get_recorded_frame_count() == 0) {
		has_previous_row = false;
	}

	for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
		r_values[i] = current[i];
	}

	if (valid && has_previous_row) {
		r_values[VALUE_GC_ALLOCATED_BYTES_DELTA] = current[VALUE_GC_ALLOCATED_BYTES] - previous_row[VALUE_GC_ALLOCATED_BYTES];
		r_values[VALUE_GC_COLLECTIONS_DELTA] = current[VALUE_GC_COLLECTIONS_GEN0] - previous_row[VALUE_GC_COLLECTIONS_GEN0];
		r_values[VALUE_GC_PAUSE_DELTA_MS] = current[VALUE_GC_PAUSE_TOTAL_MS] - previous_row[VALUE_GC_PAUSE_TOTAL_MS];
	} else {
		r_values[VALUE_GC_ALLOCATED_BYTES_DELTA] = Math::NaN;
		r_values[VALUE_GC_COLLECTIONS_DELTA] = Math::NaN;
		r_values[VALUE_GC_PAUSE_DELTA_MS] = Math::NaN;
	}

	if (valid) {
		for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
			previous_row[i] = current[i];
		}
	}
	has_previous_row = valid;
}

void DotNetMetricSource::_register() {
	PerformanceRecorder *recorder = PerformanceRecorder::get_singleton();
	if (recorder && !recorder->has_metric_source(SOURCE_NAME)) {
		recorder->register_metric_source(SOURCE_NAME, this);
		source_registered = true;
	}

	Performance *performance = Performance::get_singleton();
	if (performance) {
		for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
			const StringName id = String(SOURCE_NAME) + "/" + value_names[i];
			if (performance->has_custom_monitor(id)) {
				continue;
			}
			Performance::MonitorType type = Performance::MONITOR_TYPE_QUANTITY;
			switch (i) {
				case VALUE_GC_ALLOCATED_BYTES:
				case VALUE_GC_TOTAL_MEMORY_BYTES:
				case VALUE_GC_HEAP_SIZE_BYTES:
				case VALUE_GC_FRAGMENTED_BYTES:
				case VALUE_GC_COMMITTED_BYTES:
					type = Performance::MONITOR_TYPE_MEMORY;
					break;
				case VALUE_GC_PAUSE_TIME_PERCENT:
					type = Performance::MONITOR_TYPE_PERCENTAGE;
					break;
				default:
					break;
			}
			Vector<Variant> args;
			args.push_back(i);
			performance->add_custom_monitor(id, callable_mp_static(&DotNetMetricSource::_get_monitor), args, type);
		}
		monitors_added = true;
	}
}

void DotNetMetricSource::_unregister() {
	if (source_registered) {
		PerformanceRecorder *recorder = PerformanceRecorder::get_singleton();
		if (recorder) {
			recorder->unregister_metric_source(SOURCE_NAME);
		}
		source_registered = false;
	}

	if (monitors_added) {
		Performance *performance = Performance::get_singleton();
		if (performance) {
			for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
				const StringName id = String(SOURCE_NAME) + "/" + value_names[i];
				if (performance->has_custom_monitor(id)) {
					performance->remove_custom_monitor(id);
				}
			}
		}
		monitors_added = false;
	}
}

void DotNetMetricSource::create_singleton() {
	if (singleton) {
		return;
	}
	singleton = memnew(DotNetMetricSource);
	singleton->_register();
}

void DotNetMetricSource::free_singleton() {
	if (!singleton) {
		return;
	}
	singleton->_unregister();
	memdelete(singleton);
	singleton = nullptr;
}

DotNetMetricSource::DotNetMetricSource() {
	for (int i = 0; i < VALUE_MANAGED_MAX; i++) {
		current[i] = Math::NaN;
		previous_row[i] = Math::NaN;
	}
}

DotNetMetricSource::~DotNetMetricSource() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
