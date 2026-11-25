/**************************************************************************/
/*  performance_recorder.cpp                                              */
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

#include "performance_recorder.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/variant/typed_array.h"
#include "main/performance.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

#include <cinttypes>
#include <cstdio>

PerformanceRecorder *PerformanceRecorder::singleton = nullptr;

static const char *builtin_names[PerformanceRecorder::BUILTIN_MAX] = {
	"frame",
	"time_usec",
	"frame_time",
	"process_time",
	"physics_time",
	"physics_steps",
	"process_memory",
	"process_memory_peak",
	"gpu_time",
	"recorder_time",
};

PerformanceRecorder *PerformanceRecorder::get_singleton() {
	return singleton;
}

const char *PerformanceRecorder::get_builtin_name(Builtin p_builtin) {
	ERR_FAIL_INDEX_V(p_builtin, BUILTIN_MAX, "");
	return builtin_names[p_builtin];
}

/* Recording */

String PerformanceRecorder::_resolve_output_path(const String &p_path) {
	if (p_path.is_empty()) {
		return String();
	}
	const OS::DateTime now = OS::get_singleton()->get_datetime(false);
	const String date = vformat("%04d-%02d-%02d", (int)now.year, (int)now.month, (int)now.day);
	const String time = vformat("%02d-%02d-%02d", (int)now.hour, (int)now.minute, (int)now.second);
	return p_path.replace("{date}", date).replace("{time}", time).replace("{platform}", OS::get_singleton()->get_name().to_lower());
}

Error PerformanceRecorder::start(const Ref<PerformanceRecordingProfile> &p_profile) {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_UNAVAILABLE, "PerformanceRecorder.start() must be called on the main thread.");
	if (recording) {
		_stop(true);
	}

	Ref<PerformanceRecordingProfile> profile = p_profile;
	if (profile.is_null()) {
		profile.instantiate();
	}

	columns.clear();
	column_names.clear();
	custom_monitor_ids.clear();
	active_sources.clear();
	source_scratch.clear();
	recorder_time_column = -1;
	memory_wanted = false;

	// Builtins.
	const PackedInt32Array builtins = profile->get_builtins();
	for (int i = 0; i < builtins.size(); i++) {
		const int builtin = builtins[i];
		if (builtin < 0 || builtin >= BUILTIN_MAX) {
			WARN_PRINT(vformat("PerformanceRecorder: unknown builtin %d in the profile, skipped.", builtin));
			continue;
		}
		if (builtin == BUILTIN_RECORDER_TIME) {
			recorder_time_column = (int)columns.size();
		} else if (builtin == BUILTIN_PROCESS_MEMORY || builtin == BUILTIN_PROCESS_MEMORY_PEAK) {
			memory_wanted = true;
		}
		Column column;
		column.type = COLUMN_BUILTIN;
		column.index = builtin;
		columns.push_back(column);
		column_names.push_back(builtin_names[builtin]);
	}

	// Performance monitors.
	Performance *performance = Performance::get_singleton();
	const PackedInt32Array monitors = profile->get_monitors();
	for (int i = 0; i < monitors.size(); i++) {
		const int monitor = monitors[i];
		if (monitor < 0 || monitor >= Performance::MONITOR_MAX) {
			WARN_PRINT(vformat("PerformanceRecorder: unknown Performance monitor %d in the profile, skipped.", monitor));
			continue;
		}
		Column column;
		column.type = COLUMN_MONITOR;
		column.index = monitor;
		columns.push_back(column);
		column_names.push_back(performance ? performance->get_monitor_name(Performance::Monitor(monitor)) : vformat("monitor_%d", monitor));
	}

	// Custom Performance monitors (pulled through their Callables, so opt-in).
	if (profile->is_include_custom_monitors() && performance) {
		const TypedArray<StringName> ids = performance->get_custom_monitor_names();
		for (int i = 0; i < ids.size(); i++) {
			Column column;
			column.type = COLUMN_CUSTOM_MONITOR;
			column.index = (int)custom_monitor_ids.size();
			const StringName id = ids[i];
			custom_monitor_ids.push_back(id);
			columns.push_back(column);
			column_names.push_back(String(id));
		}
	}

	// Native metric sources.
	const PackedStringArray source_names = profile->get_metric_sources();
	for (int i = 0; i < source_names.size(); i++) {
		const StringName name = source_names[i];
		MetricSource *source = nullptr;
		for (const SourceEntry &entry : sources) {
			if (entry.name == name) {
				source = entry.source;
				break;
			}
		}
		if (!source) {
			WARN_PRINT(vformat("PerformanceRecorder: metric source \"%s\" is not registered, skipped.", name));
			continue;
		}
		Vector<StringName> value_names;
		source->get_metric_names(value_names);
		ActiveSource active;
		active.name = name;
		active.source = source;
		active.value_count = value_names.size();
		active.scratch_offset = (int)source_scratch.size();
		for (int j = 0; j < value_names.size(); j++) {
			Column column;
			column.type = COLUMN_SOURCE;
			column.index = (int)active_sources.size();
			column.offset = j;
			columns.push_back(column);
			column_names.push_back(String(name) + "/" + String(value_names[j]));
			source_scratch.push_back(Math::NaN);
		}
		active_sources.push_back(active);
	}

	// Pushed metrics. Names listed in the profile are registered as gauges if needed,
	// so a profile started from the project settings can name metrics that the game registers later.
	const PackedStringArray metric_list = profile->get_metrics();
	LocalVector<int> metric_ids;
	if (metric_list.is_empty()) {
		const int count = metric_count.load(std::memory_order_acquire);
		for (int i = 0; i < count; i++) {
			metric_ids.push_back(i);
		}
	} else {
		for (int i = 0; i < metric_list.size(); i++) {
			int id = get_metric_id(metric_list[i]);
			if (id < 0) {
				id = register_metric(metric_list[i], METRIC_GAUGE);
			}
			if (id >= 0) {
				metric_ids.push_back(id);
			}
		}
	}
	for (int id : metric_ids) {
		Column column;
		column.type = COLUMN_METRIC;
		column.index = id;
		columns.push_back(column);
		MutexLock lock(metric_mutex);
		column_names.push_back(String(metric_names[id]));
	}

	// Settings.
	sample_interval = MAX(1, profile->get_sample_interval_frames());
	process_memory_interval = MAX(1, profile->get_process_memory_interval_frames());
	flush_interval = MAX(0, profile->get_flush_interval_frames());
	flush_budget_usec = MAX(0, profile->get_flush_budget_usec());
	output_path = _resolve_output_path(profile->get_output_path());
	delimiter = profile->get_delimiter().utf8();
	if (delimiter.length() == 0) {
		delimiter = String(",").utf8();
	}

	// A flush split over frames must never be overtaken by new rows.
	capacity = (uint32_t)MAX(1, profile->get_buffer_frames());
	if (flush_interval > 0 && capacity < (uint32_t)flush_interval * 2) {
		capacity = (uint32_t)flush_interval * 2;
	}

	// All memory is allocated here; frame() does not allocate.
	data.resize(columns.size() * capacity);
	for (uint32_t i = 0; i < data.size(); i++) {
		data[i] = Math::NaN;
	}
	// Summary: running statistics and a fixed-size histogram per column, allocated here as well.
	statistics.resize(columns.size());
	for (ColumnStatistics &statistic : statistics) {
		statistic = ColumnStatistics();
	}
	histograms.resize(columns.size() * PerformanceColumnSummary::HISTOGRAM_BUCKETS);
	if (!histograms.is_empty()) {
		memset(histograms.ptr(), 0, sizeof(uint32_t) * histograms.size());
	}
	hitch_threshold_usec = (uint64_t)MAX(0, profile->get_hitch_threshold_usec());
	hitch_count = 0;
	hitch_frames.clear();
	hitch_frames.reserve(MAX_HITCH_FRAMES);
	write_summary_json = profile->is_write_summary_json();
	stop_usec = 0;

	write_buffer.clear();
	write_buffer.reserve(MIN(capacity, (uint32_t)MAX(flush_interval, 1)) * MAX(1u, columns.size()) * 12 + 256);

	rows_total = 0;
	rows_written = 0;
	frame_counter = 0;
	last_process_memory_frame = 0;
	pending_recorder_usec = 0;
	process_memory = Math::NaN;
	process_memory_peak = Math::NaN;
	flush_pending = false;
	header_written = false;
	write_failed = false;
	gpu_enabled = false;
	gpu_viewport = RID();
	gpu_viewport_id = ObjectID();

	if (!output_path.is_empty()) {
		const String dir = output_path.get_base_dir();
		if (!dir.is_empty() && !DirAccess::dir_exists_absolute(dir)) {
			DirAccess::make_dir_recursive_absolute(dir);
		}
	}

	start_usec = OS::get_singleton()->get_ticks_usec();
	recording = true;
	emit_signal(SNAME("recording_started"));
	return OK;
}

void PerformanceRecorder::_stop(bool p_emit_signals) {
	if (!recording) {
		return;
	}
	recording = false;
	stop_usec = OS::get_singleton()->get_ticks_usec();
	uint64_t rows = 0;
	if (!output_path.is_empty()) {
		rows = _write_rows(0);
		if (write_summary_json) {
			_write_summary_json();
		}
	}
	flush_pending = false;
	_disable_gpu_measurement();
	if (p_emit_signals) {
		if (rows > 0) {
			emit_signal(SNAME("flushed"), (int64_t)rows);
		}
		emit_signal(SNAME("recording_stopped"), output_path);
	}
}

void PerformanceRecorder::stop() {
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "PerformanceRecorder.stop() must be called on the main thread.");
	_stop(true);
}

void PerformanceRecorder::finish() {
	_stop(false);
	if (exit_budget.is_null()) {
		return;
	}
	const Ref<PerformanceBudgetResult> result = check_budget(exit_budget);
	exit_budget.unref();
	// print_line() is already disabled in Main::cleanup(), so the result goes to OS::print() directly.
	const CharString text = result->to_text().utf8();
	if (result->is_passed()) {
		OS::get_singleton()->print("PerformanceRecorder: %s\n", text.get_data());
		return;
	}
	OS::get_singleton()->printerr("PerformanceRecorder: %s\n", text.get_data());
	// Never hide an exit code that SceneTree.quit() or the engine already set.
	if (OS::get_singleton()->get_exit_code() == EXIT_SUCCESS) {
		OS::get_singleton()->set_exit_code(result->get_exit_code());
	}
}

Error PerformanceRecorder::flush() {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_UNAVAILABLE, "PerformanceRecorder.flush() must be called on the main thread.");
	ERR_FAIL_COND_V_MSG(output_path.is_empty(), ERR_UNCONFIGURED, "PerformanceRecorder has no output path; set PerformanceRecordingProfile.output_path.");
	const uint64_t rows = _write_rows(0);
	_finish_flush(rows);
	return write_failed ? ERR_FILE_CANT_WRITE : OK;
}

bool PerformanceRecorder::is_recording() const {
	return recording;
}

/* Per frame */

void PerformanceRecorder::_frame(uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps) {
	const uint64_t begin = OS::get_singleton()->get_ticks_usec();
	frame_counter++;

	// Hitches are checked on every frame, also between sampled rows.
	if (hitch_threshold_usec > 0 && p_frame_usec > hitch_threshold_usec) {
		hitch_count++;
		if (hitch_frames.size() < (uint32_t)MAX_HITCH_FRAMES) {
			hitch_frames.push_back((int64_t)Engine::get_singleton()->get_process_frames());
		}
	}

	if (sample_interval <= 1 || (frame_counter - 1) % (uint64_t)sample_interval == 0) {
		_record_row(begin, p_frame_usec, p_process_usec, p_physics_usec, p_physics_steps);
	}

	if (flush_pending) {
		const uint64_t flush_begin = OS::get_singleton()->get_ticks_usec();
		const uint64_t rows = _write_rows(flush_budget_usec);
		pending_recorder_usec += OS::get_singleton()->get_ticks_usec() - flush_begin;
		if (rows_written >= rows_total || write_failed) {
			flush_pending = false;
		}
		// Signal last: a handler may call stop() or start().
		_finish_flush(rows);
	}
}

void PerformanceRecorder::_record_row(uint64_t p_begin_usec, uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps) {
	// The ring buffer is full of unwritten rows: write them now, or drop the oldest one (memory-only mode).
	if (rows_total - rows_written >= capacity) {
		if (!output_path.is_empty() && flush_interval > 0 && !write_failed) {
			const uint64_t flush_begin = OS::get_singleton()->get_ticks_usec();
			_write_rows(0);
			pending_recorder_usec += OS::get_singleton()->get_ticks_usec() - flush_begin;
			flush_pending = false;
		}
		if (rows_total - rows_written >= capacity) {
			rows_written = rows_total - capacity + 1;
		}
	}

	if (memory_wanted && (last_process_memory_frame == 0 || frame_counter - last_process_memory_frame >= (uint64_t)process_memory_interval)) {
		last_process_memory_frame = frame_counter;
		const uint64_t usage = OS::get_singleton()->get_process_memory_usage();
		process_memory = usage > 0 ? (double)usage : Math::NaN;
		if (usage > 0 && (Math::is_nan(process_memory_peak) || process_memory > process_memory_peak)) {
			process_memory_peak = process_memory;
		}
	}

	for (ActiveSource &active : active_sources) {
		if (active.source) {
			active.source->sample(source_scratch.ptr() + active.scratch_offset);
		}
	}

	const uint32_t row = (uint32_t)(rows_total % capacity);
	Performance *performance = Performance::get_singleton();
	const uint32_t column_count = columns.size();
	for (uint32_t c = 0; c < column_count; c++) {
		const Column &column = columns[c];
		double value = Math::NaN;
		switch (column.type) {
			case COLUMN_BUILTIN: {
				value = _sample_builtin(column.index, p_frame_usec, p_process_usec, p_physics_usec, p_physics_steps);
			} break;
			case COLUMN_MONITOR: {
				if (performance) {
					value = performance->get_monitor(Performance::Monitor(column.index));
				}
			} break;
			case COLUMN_CUSTOM_MONITOR: {
				value = _sample_custom_monitor(column.index);
			} break;
			case COLUMN_SOURCE: {
				const ActiveSource &active = active_sources[column.index];
				if (active.source) {
					value = source_scratch[active.scratch_offset + column.offset];
				}
			} break;
			case COLUMN_METRIC: {
				if (metric_kinds[column.index] == METRIC_COUNTER) {
					value = metric_values[column.index].exchange(0.0, std::memory_order_acq_rel);
				} else {
					value = metric_values[column.index].load(std::memory_order_acquire);
				}
			} break;
		}
		data[c * capacity + row] = value;
		if ((int)c != recorder_time_column) {
			_add_statistic(c, value);
		}
	}

	if (recorder_time_column >= 0) {
		// Sampling of this row plus the flush work done since the previous row.
		const uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - p_begin_usec + pending_recorder_usec;
		data[(uint32_t)recorder_time_column * capacity + row] = (double)elapsed;
		_add_statistic((uint32_t)recorder_time_column, (double)elapsed);
		pending_recorder_usec = 0;
	}

	rows_total++;
	if (flush_interval > 0 && !output_path.is_empty() && !write_failed && rows_total - rows_written >= (uint64_t)flush_interval) {
		flush_pending = true;
	}
}

void PerformanceRecorder::_add_statistic(uint32_t p_column, double p_value) {
	if (Math::is_nan(p_value)) {
		return; // Empty cell.
	}
	ColumnStatistics &statistic = statistics[p_column];
	if (statistic.count == 0) {
		statistic.minimum = p_value;
		statistic.maximum = p_value;
	} else {
		statistic.minimum = MIN(statistic.minimum, p_value);
		statistic.maximum = MAX(statistic.maximum, p_value);
	}
	statistic.count++;
	statistic.sum += p_value;
	histograms[p_column * PerformanceColumnSummary::HISTOGRAM_BUCKETS + PerformanceColumnSummary::get_histogram_bucket(p_value)]++;
}

double PerformanceRecorder::_sample_builtin(int p_builtin, uint64_t p_frame_usec, uint64_t p_process_usec, uint64_t p_physics_usec, int p_physics_steps) {
	switch (p_builtin) {
		case BUILTIN_FRAME:
			return (double)Engine::get_singleton()->get_process_frames();
		case BUILTIN_TIME_USEC:
			return (double)(OS::get_singleton()->get_ticks_usec() - start_usec);
		case BUILTIN_FRAME_TIME:
			return (double)p_frame_usec;
		case BUILTIN_PROCESS_TIME:
			return (double)p_process_usec;
		case BUILTIN_PHYSICS_TIME:
			return (double)p_physics_usec;
		case BUILTIN_PHYSICS_STEPS:
			return (double)p_physics_steps;
		case BUILTIN_PROCESS_MEMORY:
			return process_memory;
		case BUILTIN_PROCESS_MEMORY_PEAK:
			return process_memory_peak;
		case BUILTIN_GPU_TIME: {
			if (!gpu_enabled) {
				_enable_gpu_measurement();
			}
			RenderingServer *rs = RenderingServer::get_singleton();
			if (gpu_enabled && rs) {
				return rs->viewport_get_measured_render_time_gpu(gpu_viewport);
			}
			return Math::NaN;
		}
		case BUILTIN_RECORDER_TIME:
			return 0.0; // Written after the row is sampled.
		default:
			return Math::NaN;
	}
}

double PerformanceRecorder::_sample_custom_monitor(int p_index) const {
	Performance *performance = Performance::get_singleton();
	const StringName &id = custom_monitor_ids[p_index];
	if (!performance || !performance->has_custom_monitor(id)) {
		return Math::NaN;
	}
	const Variant value = performance->get_custom_monitor(id);
	switch (value.get_type()) {
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::BOOL:
			return (double)value;
		default:
			return Math::NaN;
	}
}

void PerformanceRecorder::_enable_gpu_measurement() {
	// The root viewport is resolved lazily: the SceneTree does not exist yet when recording starts at boot.
	SceneTree *tree = SceneTree::get_singleton();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!tree || !rs) {
		return;
	}
	gpu_viewport = tree->get_root()->get_viewport_rid();
	gpu_viewport_id = tree->get_root()->get_instance_id();
	if (gpu_viewport.is_valid()) {
		rs->viewport_set_measure_render_time(gpu_viewport, true);
		gpu_enabled = true;
	}
}

void PerformanceRecorder::_disable_gpu_measurement() {
	if (gpu_enabled && RenderingServer::get_singleton() && ObjectDB::get_instance(gpu_viewport_id)) {
		RenderingServer::get_singleton()->viewport_set_measure_render_time(gpu_viewport, false);
	}
	gpu_enabled = false;
	gpu_viewport = RID();
	gpu_viewport_id = ObjectID();
}

/* CSV */

void PerformanceRecorder::_append_bytes(const char *p_bytes, int p_length) {
	const uint32_t old_size = write_buffer.size();
	if (old_size + p_length > write_buffer.get_capacity()) {
		write_buffer.reserve(MAX(write_buffer.get_capacity() * 2, old_size + p_length));
	}
	write_buffer.resize(old_size + p_length);
	memcpy(write_buffer.ptr() + old_size, p_bytes, p_length);
}

void PerformanceRecorder::_append_value(double p_value) {
	if (Math::is_nan(p_value)) {
		return; // Empty cell.
	}
	char buffer[40];
	int length;
	if (p_value == Math::floor(p_value) && Math::abs(p_value) < 9007199254740992.0) {
		length = snprintf(buffer, sizeof(buffer), "%" PRId64, (int64_t)p_value);
	} else {
		length = snprintf(buffer, sizeof(buffer), "%.9g", p_value);
		// Some C libraries use the locale's decimal separator: CSV always uses '.'.
		for (int i = 0; i < length; i++) {
			if (buffer[i] == ',') {
				buffer[i] = '.';
			}
		}
	}
	if (length > 0) {
		_append_bytes(buffer, MIN(length, (int)sizeof(buffer) - 1));
	}
}

void PerformanceRecorder::_append_header() {
	for (int i = 0; i < column_names.size(); i++) {
		if (i > 0) {
			_append_bytes(delimiter.get_data(), delimiter.length());
		}
		String name = column_names[i];
		if (name.contains(String::utf8(delimiter.get_data())) || name.contains_char('"')) {
			name = "\"" + name.replace("\"", "\"\"") + "\"";
		}
		const CharString utf8 = name.utf8();
		_append_bytes(utf8.get_data(), utf8.length());
	}
	_append_bytes("\n", 1);
}

uint64_t PerformanceRecorder::_write_rows(uint64_t p_budget_usec) {
	if (output_path.is_empty() || write_failed || capacity == 0) {
		return 0;
	}
	const uint64_t begin = OS::get_singleton()->get_ticks_usec();
	write_buffer.clear();
	if (!header_written) {
		_append_header();
	}

	// Rows older than the ring buffer are gone.
	if (rows_total > capacity && rows_written < rows_total - capacity) {
		rows_written = rows_total - capacity;
	}

	uint64_t rows = 0;
	const uint32_t column_count = columns.size();
	while (rows_written < rows_total) {
		const uint32_t row = (uint32_t)(rows_written % capacity);
		for (uint32_t c = 0; c < column_count; c++) {
			if (c > 0) {
				_append_bytes(delimiter.get_data(), delimiter.length());
			}
			_append_value(data[c * capacity + row]);
		}
		_append_bytes("\n", 1);
		rows_written++;
		rows++;
		if (p_budget_usec > 0 && (rows & 15) == 0 && OS::get_singleton()->get_ticks_usec() - begin >= p_budget_usec) {
			break;
		}
	}

	if (write_buffer.is_empty()) {
		return rows;
	}

	// Open, append and close on every chunk: on Web, closing the file schedules the IndexedDB sync.
	Error err = OK;
	Ref<FileAccess> file;
	if (header_written) {
		file = FileAccess::open(output_path, FileAccess::READ_WRITE, &err);
		if (file.is_valid()) {
			file->seek_end();
		}
	} else {
		file = FileAccess::open(output_path, FileAccess::WRITE, &err);
	}
	if (file.is_null() || !file->store_buffer(write_buffer.ptr(), write_buffer.size())) {
		write_failed = true;
		flush_pending = false;
		ERR_PRINT(vformat("PerformanceRecorder: cannot write to \"%s\" (error %d). Recording continues in memory only.", output_path, err));
		return rows;
	}
	file.unref();
	header_written = true;
	return rows;
}

void PerformanceRecorder::_finish_flush(uint64_t p_rows) {
	if (p_rows > 0) {
		emit_signal(SNAME("flushed"), (int64_t)p_rows);
	}
}

/* Pushed metrics */

int PerformanceRecorder::register_metric(const StringName &p_name, MetricKind p_kind) {
	ERR_FAIL_COND_V_MSG(p_name == StringName(), -1, "PerformanceRecorder: the metric name is empty.");
	int id = -1;
	bool full = false;
	{
		MutexLock lock(metric_mutex);
		for (uint32_t i = 0; i < metric_names.size(); i++) {
			if (metric_names[i] == p_name) {
				id = (int)i;
				break;
			}
		}
		if (id >= 0) {
			if (metric_kinds[id] != p_kind) {
				metric_kinds[id] = p_kind;
				metric_values[id].store(p_kind == METRIC_COUNTER ? 0.0 : Math::NaN, std::memory_order_release);
			}
		} else if (metric_names.size() >= (uint32_t)MAX_METRICS) {
			full = true;
		} else {
			id = (int)metric_names.size();
			metric_names.push_back(p_name);
			metric_kinds[id] = p_kind;
			metric_values[id].store(p_kind == METRIC_COUNTER ? 0.0 : Math::NaN, std::memory_order_release);
			metric_count.store(id + 1, std::memory_order_release);
		}
	}
	// Printed after the lock is released.
	ERR_FAIL_COND_V_MSG(full, -1, vformat("PerformanceRecorder: cannot register metric \"%s\", the limit of %d metrics is reached.", p_name, MAX_METRICS));
	return id;
}

int PerformanceRecorder::get_metric_id(const StringName &p_name) const {
	MutexLock lock(metric_mutex);
	for (uint32_t i = 0; i < metric_names.size(); i++) {
		if (metric_names[i] == p_name) {
			return (int)i;
		}
	}
	return -1;
}

void PerformanceRecorder::set_metric(int p_id, double p_value) {
	ERR_FAIL_INDEX(p_id, metric_count.load(std::memory_order_acquire));
	metric_values[p_id].store(p_value, std::memory_order_release);
}

void PerformanceRecorder::add_metric(int p_id, double p_delta) {
	ERR_FAIL_INDEX(p_id, metric_count.load(std::memory_order_acquire));
	double current = metric_values[p_id].load(std::memory_order_relaxed);
	double next;
	do {
		next = (Math::is_nan(current) ? 0.0 : current) + p_delta;
	} while (!metric_values[p_id].compare_exchange_weak(current, next, std::memory_order_acq_rel, std::memory_order_relaxed));
}

/* Metric sources */

void PerformanceRecorder::register_metric_source(const StringName &p_name, MetricSource *p_source) {
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "PerformanceRecorder::register_metric_source() must be called on the main thread.");
	ERR_FAIL_NULL(p_source);
	for (SourceEntry &entry : sources) {
		if (entry.name == p_name) {
			entry.source = p_source;
			return;
		}
	}
	SourceEntry entry;
	entry.name = p_name;
	entry.source = p_source;
	sources.push_back(entry);
}

void PerformanceRecorder::unregister_metric_source(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "PerformanceRecorder::unregister_metric_source() must be called on the main thread.");
	for (uint32_t i = 0; i < sources.size(); i++) {
		if (sources[i].name == p_name) {
			sources.remove_at(i);
			break;
		}
	}
	// A running recording keeps its columns; they stay empty from now on.
	for (ActiveSource &active : active_sources) {
		if (active.name == p_name) {
			active.source = nullptr;
		}
	}
}

bool PerformanceRecorder::has_metric_source(const StringName &p_name) const {
	for (const SourceEntry &entry : sources) {
		if (entry.name == p_name) {
			return true;
		}
	}
	return false;
}

/* Recorded data */

PackedStringArray PerformanceRecorder::get_column_names() const {
	return column_names;
}

PackedFloat64Array PerformanceRecorder::get_column(const StringName &p_name) const {
	PackedFloat64Array result;
	const String name = p_name;
	const int64_t column = column_names.find(name);
	ERR_FAIL_COND_V_MSG(column < 0, result, vformat("PerformanceRecorder: there is no column \"%s\".", name));
	const uint64_t count = MIN(rows_total, (uint64_t)capacity);
	result.resize(count);
	double *w = result.ptrw();
	const uint64_t first = rows_total - count;
	for (uint64_t i = 0; i < count; i++) {
		w[i] = data[(uint32_t)column * capacity + (uint32_t)((first + i) % capacity)];
	}
	return result;
}

int64_t PerformanceRecorder::get_recorded_frame_count() const {
	return (int64_t)rows_total;
}

String PerformanceRecorder::get_output_path() const {
	return output_path;
}

/* Summary and budget */

Ref<PerformanceSummary> PerformanceRecorder::get_summary() const {
	Ref<PerformanceSummary> summary;
	summary.instantiate();

	// Exact percentiles while the whole run still fits in the ring buffer.
	const bool exact = rows_total <= (uint64_t)capacity;
	const uint64_t end_usec = recording ? OS::get_singleton()->get_ticks_usec() : stop_usec;
	const int64_t duration = (start_usec > 0 && end_usec >= start_usec) ? (int64_t)(end_usec - start_usec) : 0;
	summary->set_run((int64_t)frame_counter, (int64_t)rows_total, duration, exact);

	PackedInt64Array frames;
	frames.resize(hitch_frames.size());
	for (uint32_t i = 0; i < hitch_frames.size(); i++) {
		frames.set(i, hitch_frames[i]);
	}
	summary->set_hitches((int64_t)hitch_threshold_usec, (int64_t)hitch_count, frames);

	const uint32_t column_count = MIN(columns.size(), statistics.size());
	const uint64_t row_count = MIN(rows_total, (uint64_t)capacity);
	for (uint32_t c = 0; c < column_count; c++) {
		const ColumnStatistics &statistic = statistics[c];
		PackedFloat64Array sorted_values;
		if (exact && statistic.count > 0) {
			sorted_values.resize((int64_t)statistic.count);
			double *w = sorted_values.ptrw();
			int64_t count = 0;
			for (uint64_t i = 0; i < row_count && count < (int64_t)statistic.count; i++) {
				const double value = data[c * capacity + (uint32_t)i];
				if (!Math::is_nan(value)) {
					w[count++] = value;
				}
			}
			sorted_values.resize(count);
			sorted_values.sort();
		}
		const uint32_t *histogram = nullptr;
		if (histograms.size() >= (c + 1) * (uint32_t)PerformanceColumnSummary::HISTOGRAM_BUCKETS) {
			histogram = histograms.ptr() + c * PerformanceColumnSummary::HISTOGRAM_BUCKETS;
		}
		Ref<PerformanceColumnSummary> column;
		column.instantiate();
		column->set_data(column_names[c], (int64_t)statistic.count, statistic.minimum, statistic.maximum, statistic.sum, histogram, sorted_values);
		summary->add_column(column);
	}
	return summary;
}

Ref<PerformanceBudgetResult> PerformanceRecorder::check_budget(const Ref<PerformanceBudget> &p_budget) const {
	ERR_FAIL_COND_V_MSG(p_budget.is_null(), Ref<PerformanceBudgetResult>(), "PerformanceRecorder.check_budget(): the budget is null.");
	return p_budget->check_summary(get_summary());
}

void PerformanceRecorder::_write_summary_json() {
	const String path = output_path.get_basename() + ".summary.json";
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
	if (file.is_null() || !file->store_string(get_summary()->to_text())) {
		ERR_PRINT(vformat("PerformanceRecorder: cannot write the summary to \"%s\" (error %d).", path, err));
	}
}

/* Start from the command line or the project settings */

void PerformanceRecorder::start_from_settings(const String &p_cli_output_path, const String &p_cli_profile_path, const String &p_cli_budget_path, bool p_use_project_settings) {
	// A budget from the command line records in memory, even without an output path.
	const bool enabled = !p_cli_output_path.is_empty() || !p_cli_budget_path.is_empty() || (p_use_project_settings && bool(GLOBAL_GET("debug/performance_recording/enabled")));
	if (!enabled) {
		return;
	}

	String budget_path = p_cli_budget_path;
	if (budget_path.is_empty() && p_use_project_settings) {
		budget_path = GLOBAL_GET("debug/performance_recording/budget");
	}
	exit_budget.unref();
	if (!budget_path.is_empty()) {
		exit_budget = ResourceLoader::load(budget_path, "PerformanceBudget");
		if (exit_budget.is_null()) {
			ERR_PRINT(vformat("PerformanceRecorder: cannot load the performance budget \"%s\".", budget_path));
			// A budget that cannot be loaded fails the run instead of passing silently.
			if (OS::get_singleton()->get_exit_code() == EXIT_SUCCESS) {
				OS::get_singleton()->set_exit_code(EXIT_FAILURE);
			}
		}
	}

	String profile_path = p_cli_profile_path;
	if (profile_path.is_empty() && p_use_project_settings) {
		profile_path = GLOBAL_GET("debug/performance_recording/profile");
	}
	String path = p_cli_output_path;
	if (path.is_empty() && p_use_project_settings) {
		path = GLOBAL_GET("debug/performance_recording/output_path");
	}

	Ref<PerformanceRecordingProfile> profile;
	if (!profile_path.is_empty()) {
		profile = ResourceLoader::load(profile_path, "PerformanceRecordingProfile");
		if (profile.is_null()) {
			ERR_PRINT(vformat("PerformanceRecorder: cannot load the recording profile \"%s\", the default profile is used.", profile_path));
		} else if (!path.is_empty()) {
			profile = profile->duplicate(); // Do not change the cached resource.
		}
	}
	if (profile.is_null()) {
		profile.instantiate();
	}
	if (!path.is_empty()) {
		profile->set_output_path(path);
	}
	start(profile);
}

/* Bindings */

void PerformanceRecorder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "profile"), &PerformanceRecorder::start, DEFVAL(Ref<PerformanceRecordingProfile>()));
	ClassDB::bind_method(D_METHOD("stop"), &PerformanceRecorder::stop);
	ClassDB::bind_method(D_METHOD("flush"), &PerformanceRecorder::flush);
	ClassDB::bind_method(D_METHOD("is_recording"), &PerformanceRecorder::is_recording);

	ClassDB::bind_method(D_METHOD("register_metric", "name", "kind"), &PerformanceRecorder::register_metric, DEFVAL(METRIC_GAUGE));
	ClassDB::bind_method(D_METHOD("get_metric_id", "name"), &PerformanceRecorder::get_metric_id);
	ClassDB::bind_method(D_METHOD("set_metric", "id", "value"), &PerformanceRecorder::set_metric);
	ClassDB::bind_method(D_METHOD("add_metric", "id", "delta"), &PerformanceRecorder::add_metric);

	ClassDB::bind_method(D_METHOD("has_metric_source", "name"), &PerformanceRecorder::has_metric_source);

	ClassDB::bind_method(D_METHOD("get_column_names"), &PerformanceRecorder::get_column_names);
	ClassDB::bind_method(D_METHOD("get_column", "name"), &PerformanceRecorder::get_column);
	ClassDB::bind_method(D_METHOD("get_recorded_frame_count"), &PerformanceRecorder::get_recorded_frame_count);
	ClassDB::bind_method(D_METHOD("get_output_path"), &PerformanceRecorder::get_output_path);

	ClassDB::bind_method(D_METHOD("get_summary"), &PerformanceRecorder::get_summary);
	ClassDB::bind_method(D_METHOD("check_budget", "budget"), &PerformanceRecorder::check_budget);

	ADD_SIGNAL(MethodInfo("recording_started"));
	ADD_SIGNAL(MethodInfo("recording_stopped", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("flushed", PropertyInfo(Variant::INT, "rows")));

	BIND_ENUM_CONSTANT(BUILTIN_FRAME);
	BIND_ENUM_CONSTANT(BUILTIN_TIME_USEC);
	BIND_ENUM_CONSTANT(BUILTIN_FRAME_TIME);
	BIND_ENUM_CONSTANT(BUILTIN_PROCESS_TIME);
	BIND_ENUM_CONSTANT(BUILTIN_PHYSICS_TIME);
	BIND_ENUM_CONSTANT(BUILTIN_PHYSICS_STEPS);
	BIND_ENUM_CONSTANT(BUILTIN_PROCESS_MEMORY);
	BIND_ENUM_CONSTANT(BUILTIN_PROCESS_MEMORY_PEAK);
	BIND_ENUM_CONSTANT(BUILTIN_GPU_TIME);
	BIND_ENUM_CONSTANT(BUILTIN_RECORDER_TIME);
	BIND_ENUM_CONSTANT(BUILTIN_MAX);

	BIND_ENUM_CONSTANT(METRIC_GAUGE);
	BIND_ENUM_CONSTANT(METRIC_COUNTER);
}

PerformanceRecorder::PerformanceRecorder() {
	singleton = this;
	for (int i = 0; i < MAX_METRICS; i++) {
		metric_values[i].store(Math::NaN, std::memory_order_relaxed);
	}
	if (ProjectSettings::get_singleton()) {
		GLOBAL_DEF("debug/performance_recording/enabled", false);
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "debug/performance_recording/profile", PROPERTY_HINT_FILE, "*.tres,*.res"), "");
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "debug/performance_recording/output_path", PROPERTY_HINT_SAVE_FILE, "*.csv"), "");
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "debug/performance_recording/budget", PROPERTY_HINT_FILE, "*.tres,*.res"), "");
	}
}

PerformanceRecorder::~PerformanceRecorder() {
	_stop(false);
	if (singleton == this) {
		singleton = nullptr;
	}
}
