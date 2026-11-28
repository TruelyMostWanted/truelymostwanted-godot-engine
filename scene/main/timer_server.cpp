/**************************************************************************/
/*  timer_server.cpp                                                      */
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

#include "timer_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "core/templates/sort_array.h"

// The heaps and the RID owner are not thread-safe.
#define TIMER_SERVER_MAIN_THREAD_GUARD ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "TimerServer can only be changed from the main thread.")

TimerServer *TimerServer::singleton = nullptr;

TimerServer *TimerServer::get_singleton() {
	return singleton;
}

double TimerServer::_get_elapsed(const TimerData *p_data) const {
	if (!p_data->is_active()) {
		return p_data->base_elapsed;
	}
	return p_data->base_elapsed + (clocks[p_data->clock].now - p_data->base_now) * p_data->time_scale;
}

void TimerServer::_freeze(TimerData *p_data) {
	p_data->base_elapsed = _get_elapsed(p_data);
	p_data->base_now = clocks[p_data->clock].now;
	if (p_data->queued) {
		clocks[p_data->clock].stale++;
		p_data->queued = false;
	}
	p_data->version++;
}

void TimerServer::_schedule(const RID &p_timer, TimerData *p_data) {
	Clock &clock = clocks[p_data->clock];
	p_data->base_now = clock.now;
	if (!p_data->is_active() || p_data->time_scale <= 0.0) {
		return;
	}

	double next = Math::INF;
	if (p_data->has_end()) {
		next = p_data->total_time;
	}
	if (p_data->tick_interval > 0.0) {
		next = MIN(next, p_data->next_tick);
	}
	if (p_data->alert_index < p_data->alert_times.size()) {
		next = MIN(next, p_data->alert_times[p_data->alert_index]);
	}
	if (next == Math::INF) {
		return; // An endless stopwatch without ticks and alerts has no event.
	}

	HeapEntry entry;
	entry.time = clock.now + MAX(next - p_data->base_elapsed, 0.0) / p_data->time_scale;
	entry.rid = p_timer;
	entry.version = p_data->version;
	entry.seq = next_seq++;
	clock.heap.push_back(entry);
	SortArray<HeapEntry, HeapEntryCompare> sorter;
	sorter.push_heap(0, clock.heap.size() - 1, 0, entry, clock.heap.ptr());
	p_data->queued = true;

	if (clock.stale > 64 && clock.stale * 2 > clock.heap.size()) {
		_compact(clock);
	}
}

void TimerServer::_schedule_started(const RID &p_timer, TimerData *p_data) {
	if (!in_node_pass[(p_data->clock & CLOCK_PHYSICS) ? 1 : 0]) {
		_schedule(p_timer, p_data);
		return;
	}
	// Started, resumed or unsuspended while SceneTree processes nodes: re-based after this frame's clock advance in
	// process(), so this frame's delta is not counted (as for a Timer node that starts processing during the node pass).
	p_data->base_now = clocks[p_data->clock].now;
	if (!p_data->pending) {
		p_data->pending = true;
		pending.push_back(p_timer);
	}
}

void TimerServer::_compact(Clock &r_clock) {
	uint32_t kept = 0;
	for (uint32_t idx = 0; idx < r_clock.heap.size(); idx++) {
		const HeapEntry &entry = r_clock.heap[idx];
		const TimerData *data = timer_owner.get_or_null(entry.rid);
		if (data && data->queued && data->version == entry.version) {
			r_clock.heap[kept] = entry;
			kept++;
		}
	}
	r_clock.heap.resize(kept);
	r_clock.stale = 0;
	SortArray<HeapEntry, HeapEntryCompare> sorter;
	sorter.make_heap(0, kept, r_clock.heap.ptr());
}

void TimerServer::_seek(TimerData *p_data) {
	const double elapsed = p_data->base_elapsed;
	p_data->alert_index = 0;
	while (p_data->alert_index < p_data->alert_times.size() && p_data->alert_times[p_data->alert_index] <= elapsed) {
		p_data->alert_index++;
	}
	if (p_data->tick_interval > 0.0) {
		p_data->next_tick = (Math::floor(elapsed / p_data->tick_interval) + 1.0) * p_data->tick_interval;
	}
}

void TimerServer::_update_alert_times(TimerData *p_data) {
	p_data->alert_times.clear();
	for (double alert : p_data->alerts) {
		const double elapsed = p_data->to_elapsed(alert);
		if (elapsed >= 0.0) {
			p_data->alert_times.push_back(elapsed);
		}
	}
	p_data->alert_times.sort();
}

void TimerServer::_emit(const RID &p_timer, TimerEvent p_event, double p_time) {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	if (!data || !data->callback.is_valid()) {
		return;
	}
	const Callable callback = data->callback; // The callback may free the timer.
	callback.call((int)p_event, p_time);
}

void TimerServer::_process_timer(const HeapEntry &p_entry) {
	TimerData *data = timer_owner.get_or_null(p_entry.rid);
	if (!data || data->version != p_entry.version) {
		return; // Changed by a callback of an earlier event in this frame; it was scheduled again.
	}
	const RID rid = p_entry.rid;
	const uint32_t version = data->version;
	Ref<RefCounted> hold; // Released when this function returns, after the last access to the timer.
	const double elapsed = _get_elapsed(data);
	data->base_elapsed = elapsed;
	data->base_now = clocks[data->clock].now;
	// The entry was popped because its event is due; the recomputed elapsed time may be 1 ulp short of it.
	const double reached = elapsed + 1e-9 * MAX(1.0, Math::abs(elapsed));

	while (data->alert_index < data->alert_times.size() && data->alert_times[data->alert_index] <= reached) {
		const double alert = data->to_shown(data->alert_times[data->alert_index]);
		data->alert_index++;
		_emit(rid, TIMER_EVENT_ALERT_REACHED, alert);
		data = timer_owner.get_or_null(rid);
		if (!data || data->version != version) {
			return;
		}
	}

	if (data->tick_interval > 0.0 && reached >= data->next_tick) {
		data->next_tick = (Math::floor(reached / data->tick_interval) + 1.0) * data->tick_interval;
		_emit(rid, TIMER_EVENT_TICKED, data->to_shown(elapsed));
		data = timer_owner.get_or_null(rid);
		if (!data || data->version != version) {
			return;
		}
	}

	if (data->has_end() && reached >= data->total_time) {
		const double shown = data->tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? 0.0 : data->total_time;
		if (data->loop) {
			// The remainder carries over, as in Godot's Timer.
			data->base_elapsed = elapsed - data->total_time;
			_seek(data);
		} else {
			data->base_elapsed = data->total_time;
			data->running = false;
			hold = data->hold;
			data->hold.unref();
		}
		_emit(rid, TIMER_EVENT_TIMEOUT, shown);
		data = timer_owner.get_or_null(rid);
		if (!data || data->version != version) {
			return;
		}
	}

	_schedule(rid, data);
}

void TimerServer::begin_node_pass(bool p_physics_frame) {
	in_node_pass[p_physics_frame ? 1 : 0] = true;
}

void TimerServer::process(double p_delta, double p_unscaled_delta, bool p_physics_frame, bool p_tree_paused) {
	in_node_pass[p_physics_frame ? 1 : 0] = false;
	bool advanced[CLOCK_MAX];
	for (int idx = 0; idx < CLOCK_MAX; idx++) {
		advanced[idx] = ((idx & CLOCK_PHYSICS) != 0) == p_physics_frame && (!p_tree_paused || (idx & CLOCK_ALWAYS) != 0);
		if (advanced[idx]) {
			clocks[idx].now += (idx & CLOCK_UNSCALED) ? p_unscaled_delta : p_delta;
		}
	}
	for (const RID &rid : pending) {
		TimerData *data = timer_owner.get_or_null(rid);
		if (!data || !data->pending) {
			continue;
		}
		data->pending = false;
		if (data->queued) {
			clocks[data->clock].stale++; // Scheduled from the clock time before the advance.
			data->queued = false;
		}
		data->version++;
		_schedule(rid, data); // Starts counting from the advanced clock time.
	}
	pending.clear();

	SortArray<HeapEntry, HeapEntryCompare> sorter;
	for (int idx = 0; idx < CLOCK_MAX; idx++) {
		if (!advanced[idx]) {
			continue;
		}
		Clock &clock = clocks[idx];
		while (!clock.heap.is_empty() && clock.heap[0].time <= clock.now) {
			sorter.pop_heap(0, clock.heap.size(), clock.heap.ptr());
			const HeapEntry entry = clock.heap[clock.heap.size() - 1];
			clock.heap.remove_at(clock.heap.size() - 1);
			TimerData *data = timer_owner.get_or_null(entry.rid);
			if (!data || !data->queued || data->version != entry.version) {
				if (clock.stale > 0) {
					clock.stale--;
				}
				continue;
			}
			data->queued = false;
			due.push_back(entry);
		}
	}

	if (due.is_empty()) {
		return;
	}
	// Callbacks may create, free or change timers, but only process() fills `due`.
	for (uint32_t idx = 0; idx < due.size(); idx++) {
		const HeapEntry entry = due[idx];
		_process_timer(entry);
	}
	due.clear();
}

RID TimerServer::timer_create() {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), RID(), "TimerServer can only be changed from the main thread.");
	const RID rid = timer_owner.make_rid();
	emit_signal(SNAME("timer_registered"), rid);
	return rid;
}

void TimerServer::free_rid(const RID &p_timer) {
	if (!Thread::is_main_thread()) {
		// For example a TimerInstance released on another thread: the RID stays valid until then.
		callable_mp(this, &TimerServer::free_rid).call_deferred(p_timer);
		return;
	}
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->queued) {
		clocks[data->clock].stale++;
	}
	Ref<RefCounted> hold = data->hold;
	timer_owner.free(p_timer);
	emit_signal(SNAME("timer_unregistered"), p_timer);
	hold.unref();
}

bool TimerServer::timer_is_valid(const RID &p_timer) const {
	return timer_owner.owns(p_timer);
}

void TimerServer::timer_set_info(const RID &p_timer, const Ref<TimerInfo> &p_info, bool p_include_clock) {
	ERR_FAIL_COND(p_info.is_null());
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (p_info->get_timer_name() != StringName()) {
		data->name = p_info->get_timer_name();
	}
	_freeze(data);
	data->tick_mode = p_info->get_tick_mode();
	data->total_time = p_info->get_total_time();
	data->loop = p_info->is_loop();
	data->tick_interval = p_info->get_tick_interval();
	data->time_scale = p_info->get_time_scale();
	data->alerts = p_info->get_alerts();
	_update_alert_times(data);
	_seek(data);
	_schedule(p_timer, data);
	if (p_include_clock) {
		timer_set_process_in_physics(p_timer, p_info->get_process_callback() == TimerInfo::PROCESS_CALLBACK_PHYSICS);
		timer_set_process_always(p_timer, p_info->get_process_mode() == TimerInfo::PROCESS_MODE_ALWAYS);
	}
	timer_set_ignore_time_scale(p_timer, p_info->is_ignoring_time_scale());
}

void TimerServer::timer_set_name(const RID &p_timer, const StringName &p_name) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	data->name = p_name;
}

StringName TimerServer::timer_get_name(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, StringName());
	return data->name;
}

void TimerServer::timer_set_object(const RID &p_timer, Object *p_object) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	data->object = p_object ? p_object->get_instance_id() : ObjectID();
}

Object *TimerServer::timer_get_object(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, nullptr);
	return ObjectDB::get_instance(data->object);
}

void TimerServer::timer_set_callback(const RID &p_timer, const Callable &p_callback) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	data->callback = p_callback;
}

void TimerServer::timer_set_tick_mode(const RID &p_timer, TimerInfo::TimerTickMode p_mode) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->tick_mode == p_mode) {
		return;
	}
	_freeze(data);
	data->tick_mode = p_mode;
	_update_alert_times(data);
	_seek(data);
	_schedule(p_timer, data);
}

TimerInfo::TimerTickMode TimerServer::timer_get_tick_mode(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, TimerInfo::TICK_MODE_COUNTDOWN);
	return data->tick_mode;
}

void TimerServer::timer_set_total_time(const RID &p_timer, double p_time) {
	ERR_FAIL_COND_MSG(p_time < 0, "Total time can't be negative.");
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->total_time == p_time) {
		return;
	}
	_freeze(data);
	data->total_time = p_time;
	_update_alert_times(data);
	_seek(data);
	_schedule(p_timer, data);
}

double TimerServer::timer_get_total_time(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, 0.0);
	return data->total_time;
}

void TimerServer::timer_set_loop(const RID &p_timer, bool p_loop) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	data->loop = p_loop; // Read when the end is reached; the schedule does not change.
}

bool TimerServer::timer_is_loop(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, false);
	return data->loop;
}

void TimerServer::timer_set_tick_interval(const RID &p_timer, double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Tick interval can't be negative.");
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	_freeze(data);
	data->tick_interval = p_interval;
	_seek(data);
	_schedule(p_timer, data);
}

void TimerServer::timer_set_alerts(const RID &p_timer, const PackedFloat64Array &p_alerts) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	_freeze(data);
	data->alerts = p_alerts;
	_update_alert_times(data);
	_seek(data);
	_schedule(p_timer, data);
}

void TimerServer::timer_set_time_scale(const RID &p_timer, double p_scale) {
	ERR_FAIL_COND_MSG(p_scale < 0, "Time scale can't be negative.");
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->time_scale == p_scale) {
		return;
	}
	_freeze(data);
	data->time_scale = p_scale;
	_schedule(p_timer, data);
}

void TimerServer::_set_clock(const RID &p_timer, uint8_t p_bit, bool p_enable) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	const uint8_t clock = p_enable ? (data->clock | p_bit) : (data->clock & ~p_bit);
	if (clock == data->clock) {
		return;
	}
	_freeze(data);
	data->clock = clock;
	_schedule(p_timer, data);
}

void TimerServer::timer_set_process_in_physics(const RID &p_timer, bool p_enable) {
	_set_clock(p_timer, CLOCK_PHYSICS, p_enable);
}

void TimerServer::timer_set_process_always(const RID &p_timer, bool p_enable) {
	_set_clock(p_timer, CLOCK_ALWAYS, p_enable);
}

void TimerServer::timer_set_ignore_time_scale(const RID &p_timer, bool p_ignore) {
	_set_clock(p_timer, CLOCK_UNSCALED, p_ignore);
}

void TimerServer::timer_set_suspended(const RID &p_timer, bool p_suspended) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->suspended == p_suspended) {
		return;
	}
	_freeze(data);
	data->suspended = p_suspended;
	_schedule_started(p_timer, data);
}

void TimerServer::timer_start(const RID &p_timer) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	_freeze(data);
	data->running = true;
	data->was_started = true;
	data->base_elapsed = 0.0;
	if (data->hold.is_null()) {
		data->hold = Ref<RefCounted>(Object::cast_to<RefCounted>(ObjectDB::get_instance(data->object)));
	}
	_seek(data);
	_schedule_started(p_timer, data);
	_emit(p_timer, TIMER_EVENT_STARTED, data->to_shown(0.0));
}

void TimerServer::timer_stop(const RID &p_timer) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (!data->running) {
		return;
	}
	_freeze(data);
	data->running = false;
	Ref<RefCounted> hold = data->hold; // Released after the signal.
	data->hold.unref();
	_emit(p_timer, TIMER_EVENT_STOPPED, data->to_shown(data->base_elapsed));
}

void TimerServer::timer_set_paused(const RID &p_timer, bool p_paused) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	if (data->paused == p_paused) {
		return;
	}
	_freeze(data);
	data->paused = p_paused;
	_schedule_started(p_timer, data);
	_emit(p_timer, p_paused ? TIMER_EVENT_PAUSED : TIMER_EVENT_RESUMED, data->to_shown(data->base_elapsed));
}

bool TimerServer::timer_is_paused(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, false);
	return data->paused;
}

bool TimerServer::timer_is_running(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, false);
	return data->running;
}

void TimerServer::timer_set_current_time(const RID &p_timer, double p_time) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	_freeze(data);
	// A countdown may get more time left than its total; a stopwatch does not go below zero.
	data->base_elapsed = data->tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? data->total_time - p_time : MAX(p_time, 0.0);
	_seek(data);
	_schedule(p_timer, data);
}

double TimerServer::timer_get_current_time(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, 0.0);
	// A countdown that has not started yet shows its full time; after stop() or its end, none is left (Godot's Timer).
	if (!data->running && data->tick_mode == TimerInfo::TICK_MODE_COUNTDOWN && data->was_started) {
		return 0.0;
	}
	return data->to_shown(_get_elapsed(data));
}

double TimerServer::timer_get_elapsed_time(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, 0.0);
	return _get_elapsed(data);
}

double TimerServer::timer_get_time_left(const RID &p_timer) const {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL_V(data, 0.0);
	if (!data->running) {
		return !data->was_started && data->tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? data->to_shown(data->base_elapsed) : 0.0;
	}
	if (!data->has_end()) {
		return 0.0;
	}
	return MAX(data->total_time - _get_elapsed(data), 0.0);
}

void TimerServer::timer_add_time(const RID &p_timer, double p_time) {
	const TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	timer_set_current_time(p_timer, data->to_shown(_get_elapsed(data)) + p_time);
}

Ref<TimerInstance> TimerServer::create_timer(const Ref<TimerInfo> &p_info) {
	Ref<TimerInstance> instance;
	instance.instantiate();
	if (p_info.is_valid()) {
		instance->set_timer_info(p_info);
	}
	return instance;
}

TypedArray<RID> TimerServer::get_timers() const {
	TypedArray<RID> timers;
	for (const RID &rid : timer_owner.get_owned_list()) {
		timers.push_back(rid);
	}
	return timers;
}

RID TimerServer::get_timer(const StringName &p_name) const {
	for (const RID &rid : timer_owner.get_owned_list()) {
		if (timer_owner.get_or_null(rid)->name == p_name) {
			return rid;
		}
	}
	return RID();
}

int TimerServer::get_timer_count() const {
	return (int)timer_owner.get_rid_count();
}

void TimerServer::pause_all() {
	for (const RID &rid : timer_owner.get_owned_list()) {
		const TimerData *data = timer_owner.get_or_null(rid);
		if (data && data->running && !data->engine_owned) {
			timer_set_paused(rid, true);
		}
	}
}

void TimerServer::resume_all() {
	for (const RID &rid : timer_owner.get_owned_list()) {
		const TimerData *data = timer_owner.get_or_null(rid);
		if (data && !data->engine_owned) {
			timer_set_paused(rid, false);
		}
	}
}

void TimerServer::stop_all() {
	for (const RID &rid : timer_owner.get_owned_list()) {
		const TimerData *data = timer_owner.get_or_null(rid);
		if (data && !data->engine_owned) {
			timer_stop(rid);
		}
	}
}

String TimerServer::format_time(double p_seconds, const String &p_format, bool p_round_up) {
	const bool has_h = p_format.contains("{h}") || p_format.contains("{hh}");
	const bool has_m = p_format.contains("{m}") || p_format.contains("{mm}");
	const bool has_s = p_format.contains("{s}") || p_format.contains("{ss}");
	double unit = 0.0;
	if (p_format.contains("{ms}")) {
		unit = 0.001;
	} else if (p_format.contains("{cs}")) {
		unit = 0.01;
	} else if (p_format.contains("{ds}")) {
		unit = 0.1;
	} else if (has_s) {
		unit = 1.0;
	} else if (has_m) {
		unit = 60.0;
	} else if (has_h) {
		unit = 3600.0;
	}

	double seconds = MAX(p_seconds, 0.0);
	if (p_round_up && unit > 0.0) {
		seconds = Math::ceil(seconds / unit - 1e-9) * unit;
	}
	// The largest unit in the format carries the full amount (`{mm}:{ss}` shows 75:03).
	int64_t rest = (int64_t)Math::floor(seconds * 1000.0 + 1e-6);
	const int64_t hours = has_h ? rest / 3600000 : 0;
	rest -= hours * 3600000;
	const int64_t minutes = has_m ? rest / 60000 : 0;
	rest -= minutes * 60000;
	const int64_t secs = has_s ? rest / 1000 : 0;
	rest -= secs * 1000;

	Dictionary values;
	values["h"] = hours;
	values["hh"] = String::num_int64(hours).pad_zeros(2);
	values["m"] = minutes;
	values["mm"] = String::num_int64(minutes).pad_zeros(2);
	values["s"] = secs;
	values["ss"] = String::num_int64(secs).pad_zeros(2);
	values["ds"] = rest / 100;
	values["cs"] = String::num_int64(rest / 10).pad_zeros(2);
	values["ms"] = String::num_int64(rest).pad_zeros(3);
	return p_format.format(values);
}

void TimerServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("timer_create"), &TimerServer::timer_create);
	ClassDB::bind_method(D_METHOD("free_rid", "timer"), &TimerServer::free_rid);
	ClassDB::bind_method(D_METHOD("timer_is_valid", "timer"), &TimerServer::timer_is_valid);
	ClassDB::bind_method(D_METHOD("timer_set_info", "timer", "info", "include_clock"), &TimerServer::timer_set_info, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("timer_set_name", "timer", "name"), &TimerServer::timer_set_name);
	ClassDB::bind_method(D_METHOD("timer_get_name", "timer"), &TimerServer::timer_get_name);
	ClassDB::bind_method(D_METHOD("timer_set_object", "timer", "object"), &TimerServer::timer_set_object);
	ClassDB::bind_method(D_METHOD("timer_get_object", "timer"), &TimerServer::timer_get_object);
	ClassDB::bind_method(D_METHOD("timer_set_callback", "timer", "callback"), &TimerServer::timer_set_callback);
	ClassDB::bind_method(D_METHOD("timer_set_tick_mode", "timer", "mode"), &TimerServer::timer_set_tick_mode);
	ClassDB::bind_method(D_METHOD("timer_get_tick_mode", "timer"), &TimerServer::timer_get_tick_mode);
	ClassDB::bind_method(D_METHOD("timer_set_total_time", "timer", "time_sec"), &TimerServer::timer_set_total_time);
	ClassDB::bind_method(D_METHOD("timer_get_total_time", "timer"), &TimerServer::timer_get_total_time);
	ClassDB::bind_method(D_METHOD("timer_set_loop", "timer", "enable"), &TimerServer::timer_set_loop);
	ClassDB::bind_method(D_METHOD("timer_is_loop", "timer"), &TimerServer::timer_is_loop);
	ClassDB::bind_method(D_METHOD("timer_set_tick_interval", "timer", "interval_sec"), &TimerServer::timer_set_tick_interval);
	ClassDB::bind_method(D_METHOD("timer_set_alerts", "timer", "alerts"), &TimerServer::timer_set_alerts);
	ClassDB::bind_method(D_METHOD("timer_set_time_scale", "timer", "scale"), &TimerServer::timer_set_time_scale);
	ClassDB::bind_method(D_METHOD("timer_set_process_in_physics", "timer", "enable"), &TimerServer::timer_set_process_in_physics);
	ClassDB::bind_method(D_METHOD("timer_set_process_always", "timer", "enable"), &TimerServer::timer_set_process_always);
	ClassDB::bind_method(D_METHOD("timer_set_ignore_time_scale", "timer", "ignore"), &TimerServer::timer_set_ignore_time_scale);
	ClassDB::bind_method(D_METHOD("timer_set_suspended", "timer", "suspended"), &TimerServer::timer_set_suspended);
	ClassDB::bind_method(D_METHOD("timer_start", "timer"), &TimerServer::timer_start);
	ClassDB::bind_method(D_METHOD("timer_stop", "timer"), &TimerServer::timer_stop);
	ClassDB::bind_method(D_METHOD("timer_set_paused", "timer", "paused"), &TimerServer::timer_set_paused);
	ClassDB::bind_method(D_METHOD("timer_is_paused", "timer"), &TimerServer::timer_is_paused);
	ClassDB::bind_method(D_METHOD("timer_is_running", "timer"), &TimerServer::timer_is_running);
	ClassDB::bind_method(D_METHOD("timer_set_current_time", "timer", "time_sec"), &TimerServer::timer_set_current_time);
	ClassDB::bind_method(D_METHOD("timer_get_current_time", "timer"), &TimerServer::timer_get_current_time);
	ClassDB::bind_method(D_METHOD("timer_get_elapsed_time", "timer"), &TimerServer::timer_get_elapsed_time);
	ClassDB::bind_method(D_METHOD("timer_get_time_left", "timer"), &TimerServer::timer_get_time_left);
	ClassDB::bind_method(D_METHOD("timer_add_time", "timer", "time_sec"), &TimerServer::timer_add_time);

	ClassDB::bind_method(D_METHOD("create_timer", "info"), &TimerServer::create_timer, DEFVAL(Ref<TimerInfo>()));
	ClassDB::bind_method(D_METHOD("get_timers"), &TimerServer::get_timers);
	ClassDB::bind_method(D_METHOD("get_timer", "name"), &TimerServer::get_timer);
	ClassDB::bind_method(D_METHOD("get_timer_count"), &TimerServer::get_timer_count);
	ClassDB::bind_method(D_METHOD("pause_all"), &TimerServer::pause_all);
	ClassDB::bind_method(D_METHOD("resume_all"), &TimerServer::resume_all);
	ClassDB::bind_method(D_METHOD("stop_all"), &TimerServer::stop_all);
	ClassDB::bind_static_method("TimerServer", D_METHOD("format_time", "seconds", "format", "round_up"), &TimerServer::format_time, DEFVAL("{mm}:{ss}"), DEFVAL(false));

	ADD_SIGNAL(MethodInfo("timer_registered", PropertyInfo(Variant::RID, "timer")));
	ADD_SIGNAL(MethodInfo("timer_unregistered", PropertyInfo(Variant::RID, "timer")));

	BIND_ENUM_CONSTANT(TIMER_EVENT_STARTED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_STOPPED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_PAUSED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_RESUMED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_TICKED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_ALERT_REACHED);
	BIND_ENUM_CONSTANT(TIMER_EVENT_TIMEOUT);
}

void TimerServer::set_used_for_timer_nodes(bool p_enable) {
	used_for_timer_nodes = p_enable;
}

bool TimerServer::is_used_for_timer_nodes() const {
	return used_for_timer_nodes;
}

void TimerServer::timer_set_engine_owned(const RID &p_timer, bool p_engine_owned) {
	TIMER_SERVER_MAIN_THREAD_GUARD;
	TimerData *data = timer_owner.get_or_null(p_timer);
	ERR_FAIL_NULL(data);
	data->engine_owned = p_engine_owned;
}

TimerServer::TimerServer() {
	singleton = this;
	// Opt-in: off, Timer nodes keep Godot's own code path (internal processing).
	used_for_timer_nodes = GLOBAL_DEF_RST("application/run/timer_nodes_use_timer_server", false);
}

TimerServer::~TimerServer() {
	// Free what is left (code timers still referenced at exit); objects held while running die after the singleton is gone.
	LocalVector<Ref<RefCounted>> holds;
	for (const RID &rid : timer_owner.get_owned_list()) {
		TimerData *data = timer_owner.get_or_null(rid);
		holds.push_back(data->hold);
		timer_owner.free(rid);
	}
	singleton = nullptr;
	holds.clear();
}

/* TimerInstance */

void TimerInstance::_on_event(int p_event, double p_time) {
	switch (p_event) {
		case TimerServer::TIMER_EVENT_STARTED:
			emit_signal(SNAME("started"));
			break;
		case TimerServer::TIMER_EVENT_STOPPED:
			emit_signal(SNAME("stopped"));
			break;
		case TimerServer::TIMER_EVENT_PAUSED:
			emit_signal(SNAME("pause_changed"), true);
			break;
		case TimerServer::TIMER_EVENT_RESUMED:
			emit_signal(SNAME("pause_changed"), false);
			break;
		case TimerServer::TIMER_EVENT_TICKED:
			emit_signal(SNAME("ticked"), p_time);
			break;
		case TimerServer::TIMER_EVENT_ALERT_REACHED:
			emit_signal(SNAME("alert_reached"), p_time);
			break;
		case TimerServer::TIMER_EVENT_TIMEOUT:
			emit_signal(SNAME("timeout"));
			break;
	}
}

void TimerInstance::_on_info_changed() {
	TimerServer::get_singleton()->timer_set_info(timer, timer_info);
}

RID TimerInstance::get_rid() const {
	return timer;
}

void TimerInstance::set_timer_info(const Ref<TimerInfo> &p_info) {
	if (timer_info == p_info) {
		return;
	}
	const Callable changed = callable_mp(this, &TimerInstance::_on_info_changed);
	if (timer_info.is_valid()) {
		timer_info->disconnect_changed(changed);
	}
	timer_info = p_info;
	if (timer_info.is_valid()) {
		timer_info->connect_changed(changed);
		_on_info_changed();
	}
}

Ref<TimerInfo> TimerInstance::get_timer_info() const {
	return timer_info;
}

void TimerInstance::start() {
	TimerServer::get_singleton()->timer_start(timer);
}

void TimerInstance::stop() {
	TimerServer::get_singleton()->timer_stop(timer);
}

void TimerInstance::pause() {
	set_paused(true);
}

void TimerInstance::resume() {
	set_paused(false);
}

void TimerInstance::restart() {
	TimerServer::get_singleton()->timer_set_paused(timer, false);
	TimerServer::get_singleton()->timer_start(timer);
}

void TimerInstance::set_paused(bool p_paused) {
	TimerServer::get_singleton()->timer_set_paused(timer, p_paused);
}

bool TimerInstance::is_paused() const {
	return TimerServer::get_singleton()->timer_is_paused(timer);
}

bool TimerInstance::is_running() const {
	return TimerServer::get_singleton()->timer_is_running(timer);
}

bool TimerInstance::is_stopped() const {
	return !is_running();
}

void TimerInstance::set_current_time(double p_time) {
	TimerServer::get_singleton()->timer_set_current_time(timer, p_time);
}

double TimerInstance::get_current_time() const {
	return TimerServer::get_singleton()->timer_get_current_time(timer);
}

double TimerInstance::get_elapsed_time() const {
	return TimerServer::get_singleton()->timer_get_elapsed_time(timer);
}

double TimerInstance::get_time_left() const {
	return TimerServer::get_singleton()->timer_get_time_left(timer);
}

void TimerInstance::add_time(double p_time) {
	TimerServer::get_singleton()->timer_add_time(timer, p_time);
}

String TimerInstance::get_time_text(const String &p_format) const {
	const bool countdown = TimerServer::get_singleton()->timer_get_tick_mode(timer) == TimerInfo::TICK_MODE_COUNTDOWN;
	return TimerServer::format_time(get_current_time(), p_format, countdown);
}

void TimerInstance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &TimerInstance::get_rid);
	ClassDB::bind_method(D_METHOD("set_timer_info", "info"), &TimerInstance::set_timer_info);
	ClassDB::bind_method(D_METHOD("get_timer_info"), &TimerInstance::get_timer_info);
	ClassDB::bind_method(D_METHOD("start"), &TimerInstance::start);
	ClassDB::bind_method(D_METHOD("stop"), &TimerInstance::stop);
	ClassDB::bind_method(D_METHOD("pause"), &TimerInstance::pause);
	ClassDB::bind_method(D_METHOD("resume"), &TimerInstance::resume);
	ClassDB::bind_method(D_METHOD("restart"), &TimerInstance::restart);
	ClassDB::bind_method(D_METHOD("set_paused", "paused"), &TimerInstance::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &TimerInstance::is_paused);
	ClassDB::bind_method(D_METHOD("is_running"), &TimerInstance::is_running);
	ClassDB::bind_method(D_METHOD("is_stopped"), &TimerInstance::is_stopped);
	ClassDB::bind_method(D_METHOD("set_current_time", "time_sec"), &TimerInstance::set_current_time);
	ClassDB::bind_method(D_METHOD("get_current_time"), &TimerInstance::get_current_time);
	ClassDB::bind_method(D_METHOD("get_elapsed_time"), &TimerInstance::get_elapsed_time);
	ClassDB::bind_method(D_METHOD("get_time_left"), &TimerInstance::get_time_left);
	ClassDB::bind_method(D_METHOD("add_time", "time_sec"), &TimerInstance::add_time);
	ClassDB::bind_method(D_METHOD("get_time_text", "format"), &TimerInstance::get_time_text, DEFVAL("{mm}:{ss}"));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "timer_info", PROPERTY_HINT_RESOURCE_TYPE, "TimerInfo"), "set_timer_info", "get_timer_info");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_paused", "is_paused");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_time", PROPERTY_HINT_NONE, "suffix:s", PROPERTY_USAGE_NONE), "set_current_time", "get_current_time");

	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("stopped"));
	ADD_SIGNAL(MethodInfo("pause_changed", PropertyInfo(Variant::BOOL, "paused")));
	ADD_SIGNAL(MethodInfo("ticked", PropertyInfo(Variant::FLOAT, "time")));
	ADD_SIGNAL(MethodInfo("alert_reached", PropertyInfo(Variant::FLOAT, "time")));
	ADD_SIGNAL(MethodInfo("timeout"));
}

TimerInstance::TimerInstance() {
	TimerServer *server = TimerServer::get_singleton();
	if (!server) {
		return; // ClassDB default instances before the server exists.
	}
	timer = server->timer_create();
	server->timer_set_object(timer, this);
	server->timer_set_callback(timer, callable_mp(this, &TimerInstance::_on_event));
}

TimerInstance::~TimerInstance() {
	if (timer_info.is_valid()) {
		timer_info->disconnect_changed(callable_mp(this, &TimerInstance::_on_info_changed));
	}
	TimerServer *server = TimerServer::get_singleton();
	if (server && timer.is_valid() && server->timer_is_valid(timer)) {
		server->free_rid(timer);
	}
}
