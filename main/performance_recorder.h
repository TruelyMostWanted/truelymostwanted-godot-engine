/**************************************************************************/
/*  performance_recorder.h                                                */
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

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "main/performance_budget.h"
#include "main/performance_recording_profile.h"
#include "main/performance_summary.h"

#include <atomic>

// Records one row of metrics per frame (or every N frames) into a preallocated
// column-major ring buffer and appends the rows to a CSV file in chunks.
// Main::iteration() calls frame() once per frame; when not recording, that is a single bool check.
class PerformanceRecorder : public Object {
	GDCLASS(PerformanceRecorder, Object);

public:
	enum Builtin {
		BUILTIN_FRAME,
		BUILTIN_TIME_USEC,
		BUILTIN_FRAME_TIME,
		BUILTIN_PROCESS_TIME,
		BUILTIN_PHYSICS_TIME,
		BUILTIN_PHYSICS_STEPS,
		BUILTIN_PROCESS_MEMORY,
		BUILTIN_PROCESS_MEMORY_PEAK,
		BUILTIN_GPU_TIME,
		BUILTIN_RECORDER_TIME,
		BUILTIN_MAX,
	};

	enum MetricKind {
		METRIC_GAUGE,
		METRIC_COUNTER,
	};

	// Native metric source for modules (C++ only), e.g. the .NET runtime metrics.
	// The names are read once in start(); sample() is called on the main thread once per recorded row
	// and writes exactly one value per name (NaN for "no value").
	struct MetricSource {
		virtual void get_metric_names(Vector<StringName> &r_names) const = 0;
		virtual void sample(double *r_values) = 0;
		virtual ~MetricSource() {}
	};

	static constexpr int MAX_METRICS = 256;
	// Hitch frame numbers kept per run; hitches after that are only counted.
	static constexpr int MAX_HITCH_FRAMES = 1024;

private:
	static PerformanceRecorder *singleton;

	enum ColumnType {
		COLUMN_BUILTIN,
		COLUMN_MONITOR,
		COLUMN_CUSTOM_MONITOR,
		COLUMN_SOURCE,
		COLUMN_METRIC,
	};

	struct Column {
		ColumnType type = COLUMN_BUILTIN;
		int index = 0; // Builtin, Performance::Monitor, custom monitor index, active source index, or metric id.
		int offset = 0; // Value index inside a metric source.
	};

	struct SourceEntry {
		StringName name;
		MetricSource *source = nullptr;
	};

	// Running statistics of one column over the whole run (the ring buffer only holds the last rows).
	struct ColumnStatistics {
		uint64_t count = 0;
		double minimum = 0.0;
		double maximum = 0.0;
		double sum = 0.0;
	};

	struct ActiveSource {
		StringName name;
		MetricSource *source = nullptr;
		int value_count = 0;
		int scratch_offset = 0;
	};

	// Pushed metrics. Names and kinds are guarded by metric_mutex; values are lock-free.
	mutable Mutex metric_mutex;
	LocalVector<StringName> metric_names;
	MetricKind metric_kinds[MAX_METRICS] = {};
	std::atomic<double> metric_values[MAX_METRICS];
	std::atomic<int> metric_count{ 0 };

	// Registered native sources (main thread).
	LocalVector<SourceEntry> sources;

	// Recording state (main thread).
	bool recording = false;
	LocalVector<Column> columns;
	PackedStringArray column_names;
	LocalVector<double> data; // columns.size() * capacity, column-major.
	uint32_t capacity = 0; // Rows in the ring buffer.
	uint64_t rows_total = 0; // Rows recorded since start().
	uint64_t rows_written = 0; // Rows before this index are written to the file or dropped.
	LocalVector<StringName> custom_monitor_ids;
	LocalVector<ActiveSource> active_sources;
	LocalVector<double> source_scratch;
	int recorder_time_column = -1;

	// Summary (main thread): kept after stop() until the next start().
	LocalVector<ColumnStatistics> statistics;
	LocalVector<uint32_t> histograms; // columns.size() * PerformanceColumnSummary::HISTOGRAM_BUCKETS.
	uint64_t hitch_threshold_usec = 0;
	uint64_t hitch_count = 0;
	LocalVector<int64_t> hitch_frames;
	uint64_t stop_usec = 0;
	bool write_summary_json = false;
	Ref<PerformanceBudget> exit_budget; // From --performance-budget or the project settings, checked in finish().

	int sample_interval = 1;
	int process_memory_interval = 30;
	int flush_interval = 300;
	int flush_budget_usec = 2000;
	uint64_t frame_counter = 0;
	uint64_t last_process_memory_frame = 0;
	uint64_t start_usec = 0;
	uint64_t pending_recorder_usec = 0; // Flush work since the last row, added to its recorder_time.
	double process_memory = 0.0;
	double process_memory_peak = 0.0;

	bool memory_wanted = false;
	bool gpu_enabled = false;
	RID gpu_viewport;
	ObjectID gpu_viewport_id;

	// CSV output.
	String output_path;
	CharString delimiter;
	bool flush_pending = false;
	bool header_written = false;
	bool write_failed = false;
	LocalVector<uint8_t> write_buffer;

	void _frame(uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps);
	void _record_row(uint64_t p_begin_usec, uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps);
	double _sample_builtin(int p_builtin, uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps);
	double _sample_custom_monitor(int p_index) const;
	void _add_statistic(uint32_t p_column, double p_value);
	void _write_summary_json();
	void _enable_gpu_measurement();
	void _disable_gpu_measurement();

	// Writes unwritten rows to the CSV file. A budget of 0 writes everything. Returns the number of rows written.
	uint64_t _write_rows(uint64_t p_budget_usec);
	void _append_bytes(const char *p_bytes, int p_length);
	void _append_value(double p_value);
	void _append_header();
	void _finish_flush(uint64_t p_rows);
	void _stop(bool p_emit_signals);

	static String _resolve_output_path(const String &p_path);

protected:
	static void _bind_methods();

public:
	static PerformanceRecorder *get_singleton();

	static const char *get_builtin_name(Builtin p_builtin);

	// Recording.
	Error start(const Ref<PerformanceRecordingProfile> &p_profile = Ref<PerformanceRecordingProfile>());
	void stop();
	Error flush();
	bool is_recording() const;

	// Pushed metrics (thread-safe).
	int register_metric(const StringName &p_name, MetricKind p_kind = METRIC_GAUGE);
	int get_metric_id(const StringName &p_name) const;
	void set_metric(int p_id, double p_value);
	void add_metric(int p_id, double p_delta);

	// Native metric sources (C++ only, main thread).
	void register_metric_source(const StringName &p_name, MetricSource *p_source);
	void unregister_metric_source(const StringName &p_name);
	bool has_metric_source(const StringName &p_name) const;

	// Recorded data.
	PackedStringArray get_column_names() const;
	PackedFloat64Array get_column(const StringName &p_name) const;
	int64_t get_recorded_frame_count() const;
	String get_output_path() const;

	// Summary and budget.
	Ref<PerformanceSummary> get_summary() const;
	Ref<PerformanceBudgetResult> check_budget(const Ref<PerformanceBudget> &p_budget) const;

	// Starts recording from the command line options or the project settings (called by Main::start()).
	void start_from_settings(const String &p_cli_output_path, const String &p_cli_profile_path, const String &p_cli_budget_path, bool p_use_project_settings);
	// Writes the remaining rows without emitting signals, then checks the budget from the command line
	// or the project settings and sets a failing exit code (called by Main::cleanup()).
	void finish();

	// Called once per frame by Main::iteration().
	_FORCE_INLINE_ void frame(uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps) {
		if (unlikely(recording)) {
			_frame(p_frame_usec, p_process_usec, p_physics_usec, p_physics_steps);
		}
	}

	PerformanceRecorder();
	~PerformanceRecorder();
};

VARIANT_ENUM_CAST(PerformanceRecorder::Builtin);
VARIANT_ENUM_CAST(PerformanceRecorder::MetricKind);
