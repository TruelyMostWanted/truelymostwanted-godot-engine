/**************************************************************************/
/*  timer_server.h                                                        */
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
#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "scene/resources/timer_info.h"

class TimerInstance;

// Owns every timer (#379): `Timer` nodes and code timers (`TimerInstance`) are RIDs here. Data-oriented: a running
// timer stores only where it started on its clock; the server keeps one min-heap of event times (end, next tick, next
// alert) per clock and per frame compares the heap top with the clock, so a frame without events costs O(1) per clock
// instead of one countdown per timer. Ticked from `SceneTree::process_timers()`. Main thread only.
class TimerServer : public Object {
	GDCLASS(TimerServer, Object);

public:
	enum TimerEvent {
		TIMER_EVENT_STARTED,
		TIMER_EVENT_STOPPED,
		TIMER_EVENT_PAUSED,
		TIMER_EVENT_RESUMED,
		TIMER_EVENT_TICKED,
		TIMER_EVENT_ALERT_REACHED,
		TIMER_EVENT_TIMEOUT,
	};

private:
	static TimerServer *singleton;

	// Clock index bits: 8 clocks, idle/physics x pausable/always x scaled/unscaled.
	enum {
		CLOCK_PHYSICS = 1,
		CLOCK_ALWAYS = 2,
		CLOCK_UNSCALED = 4,
		CLOCK_MAX = 8,
	};

	struct TimerData {
		StringName name;
		ObjectID object;
		Callable callback;
		Ref<RefCounted> hold; // Keeps a RefCounted object alive while the timer runs.
		LocalVector<double> alert_times; // Alerts as elapsed times, ascending.
		PackedFloat64Array alerts; // Alerts as given (shown time).
		double total_time = 1.0;
		double tick_interval = 0.0;
		double time_scale = 1.0;
		double base_elapsed = 0.0; // Elapsed time at base_now.
		double base_now = 0.0; // Clock time of the last re-base.
		double next_tick = 0.0; // Elapsed time of the next tick.
		uint32_t version = 0; // Bumped on every re-base; heap entries of older versions are stale.
		uint32_t alert_index = 0; // First alert not reached yet.
		uint8_t clock = 0;
		TimerInfo::TimerTickMode tick_mode = TimerInfo::TICK_MODE_COUNTDOWN;
		bool loop = false;
		bool running = false;
		bool was_started = false; // Until the first start, a countdown shows its full time.
		bool paused = false;
		bool suspended = false;
		bool queued = false; // Has a valid heap entry.
		bool pending = false; // Started during this frame's node pass; re-based after the clock advance.
		bool engine_owned = false; // Not affected by pause_all(), resume_all() and stop_all().

		bool is_active() const { return running && !paused && !suspended; }
		bool has_end() const { return tick_mode == TimerInfo::TICK_MODE_COUNTDOWN || total_time > 0.0; }
		double to_shown(double p_elapsed) const { return tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? MAX(total_time - p_elapsed, 0.0) : p_elapsed; }
		double to_elapsed(double p_shown) const { return tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? total_time - p_shown : p_shown; }
	};

	struct HeapEntry {
		double time = 0.0;
		RID rid;
		uint32_t version = 0;
		uint64_t seq = 0; // Equal times fire in schedule order.
	};

	struct HeapEntryCompare {
		// Min-heap on time for SortArray's heap functions.
		_FORCE_INLINE_ bool operator()(const HeapEntry &p_a, const HeapEntry &p_b) const { return p_a.time > p_b.time || (p_a.time == p_b.time && p_a.seq > p_b.seq); }
	};

	struct Clock {
		double now = 0.0;
		LocalVector<HeapEntry> heap;
		uint32_t stale = 0; // Entries in heap that no longer match their timer.
	};

	Clock clocks[CLOCK_MAX];
	mutable RID_Owner<TimerData> timer_owner;
	LocalVector<HeapEntry> due; // Reused every frame.
	LocalVector<RID> pending; // Timers started during this frame's node pass.
	uint64_t next_seq = 0;
	bool in_node_pass[2] = {}; // Idle, physics: SceneTree is processing nodes and has not advanced the clocks yet.
	bool used_for_timer_nodes = false; // Project setting `application/run/timer_nodes_use_timer_server`, read at startup.

	double _get_elapsed(const TimerData *p_data) const;
	void _freeze(TimerData *p_data);
	void _schedule(const RID &p_timer, TimerData *p_data);
	void _schedule_started(const RID &p_timer, TimerData *p_data);
	void _seek(TimerData *p_data);
	void _update_alert_times(TimerData *p_data);
	void _compact(Clock &r_clock);
	void _emit(const RID &p_timer, TimerEvent p_event, double p_time);
	void _process_timer(const HeapEntry &p_entry);
	void _set_clock(const RID &p_timer, uint8_t p_bit, bool p_enable);

protected:
	static void _bind_methods();

public:
	static TimerServer *get_singleton();

	// Advances the clocks of this frame type and fires due events. Called by SceneTree::process_timers().
	void process(double p_delta, double p_unscaled_delta, bool p_physics_frame, bool p_tree_paused);
	// Called by SceneTree right before it processes nodes: timers started from then on until process() skip this
	// frame's delta, as a Timer node that starts processing during the node pass does in Godot.
	void begin_node_pass(bool p_physics_frame);

	// Whether Timer nodes run on this server (project setting, default off). Tests may change it.
	void set_used_for_timer_nodes(bool p_enable);
	bool is_used_for_timer_nodes() const;
	// Engine-owned timers (internal and editor Timer nodes) are skipped by pause_all(), resume_all() and stop_all().
	void timer_set_engine_owned(const RID &p_timer, bool p_engine_owned);

	RID timer_create();
	void free_rid(const RID &p_timer);
	bool timer_is_valid(const RID &p_timer) const;

	void timer_set_info(const RID &p_timer, const Ref<TimerInfo> &p_info, bool p_include_clock = true);

	void timer_set_name(const RID &p_timer, const StringName &p_name);
	StringName timer_get_name(const RID &p_timer) const;
	void timer_set_object(const RID &p_timer, Object *p_object);
	Object *timer_get_object(const RID &p_timer) const;
	void timer_set_callback(const RID &p_timer, const Callable &p_callback);

	void timer_set_tick_mode(const RID &p_timer, TimerInfo::TimerTickMode p_mode);
	TimerInfo::TimerTickMode timer_get_tick_mode(const RID &p_timer) const;
	void timer_set_total_time(const RID &p_timer, double p_time);
	double timer_get_total_time(const RID &p_timer) const;
	void timer_set_loop(const RID &p_timer, bool p_loop);
	bool timer_is_loop(const RID &p_timer) const;
	void timer_set_tick_interval(const RID &p_timer, double p_interval);
	void timer_set_alerts(const RID &p_timer, const PackedFloat64Array &p_alerts);
	void timer_set_time_scale(const RID &p_timer, double p_scale);
	void timer_set_process_in_physics(const RID &p_timer, bool p_enable);
	void timer_set_process_always(const RID &p_timer, bool p_enable);
	void timer_set_ignore_time_scale(const RID &p_timer, bool p_ignore);
	void timer_set_suspended(const RID &p_timer, bool p_suspended);

	void timer_start(const RID &p_timer);
	void timer_stop(const RID &p_timer);
	void timer_set_paused(const RID &p_timer, bool p_paused);
	bool timer_is_paused(const RID &p_timer) const;
	bool timer_is_running(const RID &p_timer) const;
	void timer_set_current_time(const RID &p_timer, double p_time);
	double timer_get_current_time(const RID &p_timer) const;
	double timer_get_elapsed_time(const RID &p_timer) const;
	double timer_get_time_left(const RID &p_timer) const;
	void timer_add_time(const RID &p_timer, double p_time);

	Ref<TimerInstance> create_timer(const Ref<TimerInfo> &p_info);
	TypedArray<RID> get_timers() const;
	RID get_timer(const StringName &p_name) const;
	int get_timer_count() const;
	void pause_all();
	void resume_all();
	void stop_all();

	static String format_time(double p_seconds, const String &p_format = "{mm}:{ss}", bool p_round_up = false);

	TimerServer();
	~TimerServer();
};

// A timer created in code (`TimerServer.create_timer()`): the same API and signals as the `Timer` node, without a scene.
class TimerInstance : public RefCounted {
	GDCLASS(TimerInstance, RefCounted);

	RID timer;
	Ref<TimerInfo> timer_info;

	void _on_event(int p_event, double p_time);
	void _on_info_changed();

protected:
	static void _bind_methods();

public:
	RID get_rid() const;

	void set_timer_info(const Ref<TimerInfo> &p_info);
	Ref<TimerInfo> get_timer_info() const;

	void start();
	void stop();
	void pause();
	void resume();
	void restart();
	void set_paused(bool p_paused);
	bool is_paused() const;
	bool is_running() const;
	bool is_stopped() const;
	void set_current_time(double p_time);
	double get_current_time() const;
	double get_elapsed_time() const;
	double get_time_left() const;
	void add_time(double p_time);
	String get_time_text(const String &p_format = "{mm}:{ss}") const;

	TimerInstance();
	~TimerInstance();
};

VARIANT_ENUM_CAST(TimerServer::TimerEvent);
