/**************************************************************************/
/*  test_timer_server.cpp                                                 */
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

TEST_FORCE_LINK(test_timer_server)

#include "core/object/callable_mp.h"
#include "scene/main/scene_tree.h"
#include "scene/main/timer.h"
#include "scene/main/timer_server.h"
#include "scene/main/window.h"
#include "tests/signal_watcher.h"

namespace TestTimerServer {

static int event_count[TimerServer::TIMER_EVENT_TIMEOUT + 1] = {};
static double event_time[TimerServer::TIMER_EVENT_TIMEOUT + 1] = {};

static void reset_events() {
	for (int idx = 0; idx <= TimerServer::TIMER_EVENT_TIMEOUT; idx++) {
		event_count[idx] = 0;
		event_time[idx] = 0.0;
	}
}

static void on_event(int p_event, double p_time) {
	event_count[p_event]++;
	event_time[p_event] = p_time;
}

static Array emissions_without_args() {
	Array emissions;
	emissions.push_back(Array());
	return emissions;
}

static Array emissions_with(const Variant &p_arg) {
	Array args;
	args.push_back(p_arg);
	Array emissions;
	emissions.push_back(args);
	return emissions;
}

static RID make_timer(double p_total, bool p_loop = false) {
	TimerServer *server = TimerServer::get_singleton();
	const RID rid = server->timer_create();
	server->timer_set_callback(rid, callable_mp_static(&on_event));
	server->timer_set_total_time(rid, p_total);
	server->timer_set_loop(rid, p_loop);
	return rid;
}

static void step(double p_delta) {
	TimerServer::get_singleton()->process(p_delta, p_delta, false, false);
}

TEST_CASE("[TimerServer] Countdown: time left, current time and timeout") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID rid = make_timer(1.0);
	CHECK(server->timer_is_valid(rid));
	CHECK_FALSE(server->timer_is_running(rid));

	server->timer_start(rid);
	CHECK(event_count[TimerServer::TIMER_EVENT_STARTED] == 1);
	CHECK(server->timer_is_running(rid));
	step(0.25);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.75));
	CHECK(server->timer_get_current_time(rid) == doctest::Approx(0.75));
	CHECK(server->timer_get_elapsed_time(rid) == doctest::Approx(0.25));
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 0);

	step(0.8);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 1);
	CHECK_FALSE(server->timer_is_running(rid));
	CHECK(server->timer_get_time_left(rid) == 0.0);

	step(5.0);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 1);
	server->free_rid(rid);
	CHECK_FALSE(server->timer_is_valid(rid));
}

TEST_CASE("[TimerServer] Loop carries the remainder over, one timeout per frame") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID rid = make_timer(1.0, true);
	server->timer_start(rid);
	step(1.25);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 1);
	CHECK(server->timer_is_running(rid));
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.75));

	step(2.5); // 2.75 s past the start of this loop: one timeout now, the next one in the next frame.
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 2);
	step(0.0);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 3);
	server->timer_stop(rid);
	CHECK(event_count[TimerServer::TIMER_EVENT_STOPPED] == 1);
	server->free_rid(rid);
}

TEST_CASE("[TimerServer] Stopwatch counts up, with and without an end") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID endless = make_timer(0.0);
	server->timer_set_tick_mode(endless, TimerInfo::TICK_MODE_STOPWATCH);
	server->timer_start(endless);
	step(5.0);
	CHECK(server->timer_get_current_time(endless) == doctest::Approx(5.0));
	CHECK(server->timer_is_running(endless));
	server->timer_stop(endless);
	CHECK(server->timer_get_current_time(endless) == doctest::Approx(5.0)); // Keeps the final time.

	const RID limited = make_timer(2.0);
	server->timer_set_tick_mode(limited, TimerInfo::TICK_MODE_STOPWATCH);
	server->timer_start(limited);
	step(1.5);
	CHECK(server->timer_get_current_time(limited) == doctest::Approx(1.5));
	CHECK(server->timer_get_time_left(limited) == doctest::Approx(0.5));
	step(1.0);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 1);
	CHECK(event_time[TimerServer::TIMER_EVENT_TIMEOUT] == doctest::Approx(2.0));
	server->free_rid(endless);
	server->free_rid(limited);
}

TEST_CASE("[TimerServer] Pause, resume, time scale, set and add time") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID rid = make_timer(1.0);
	server->timer_start(rid);
	step(0.3);
	server->timer_set_paused(rid, true);
	CHECK(event_count[TimerServer::TIMER_EVENT_PAUSED] == 1);
	step(10.0);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.7));
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 0);
	server->timer_set_paused(rid, false);
	CHECK(event_count[TimerServer::TIMER_EVENT_RESUMED] == 1);
	step(0.2);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.5));

	server->timer_set_time_scale(rid, 2.0);
	step(0.1);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.3));
	server->timer_add_time(rid, 1.0);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(1.3));
	server->timer_set_current_time(rid, 0.2);
	CHECK(server->timer_get_time_left(rid) == doctest::Approx(0.2));
	step(0.1);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 1);
	server->free_rid(rid);
}

TEST_CASE("[TimerServer] Ticks and alerts") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID rid = make_timer(10.0);
	server->timer_set_tick_interval(rid, 1.0);
	PackedFloat64Array alerts;
	alerts.push_back(5.0);
	alerts.push_back(3.0);
	server->timer_set_alerts(rid, alerts);
	server->timer_start(rid);
	step(0.5);
	CHECK(event_count[TimerServer::TIMER_EVENT_TICKED] == 0);
	step(0.5);
	CHECK(event_count[TimerServer::TIMER_EVENT_TICKED] == 1);
	CHECK(event_time[TimerServer::TIMER_EVENT_TICKED] == doctest::Approx(9.0));
	step(4.0);
	CHECK(event_count[TimerServer::TIMER_EVENT_ALERT_REACHED] == 1);
	CHECK(event_time[TimerServer::TIMER_EVENT_ALERT_REACHED] == doctest::Approx(5.0));
	CHECK(event_count[TimerServer::TIMER_EVENT_TICKED] == 2); // One tick per frame at most.
	step(3.0);
	CHECK(event_count[TimerServer::TIMER_EVENT_ALERT_REACHED] == 2);
	CHECK(event_time[TimerServer::TIMER_EVENT_ALERT_REACHED] == doctest::Approx(3.0));
	server->free_rid(rid);
}

TEST_CASE("[TimerServer] Clocks: physics, unscaled and tree pause") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	const RID physics = make_timer(1.0);
	server->timer_set_process_in_physics(physics, true);
	const RID unscaled = make_timer(1.0);
	server->timer_set_ignore_time_scale(unscaled, true);
	const RID always = make_timer(1.0);
	server->timer_set_process_always(always, true);
	server->timer_start(physics);
	server->timer_start(unscaled);
	server->timer_start(always);

	server->process(0.5, 0.25, false, false);
	CHECK(server->timer_get_time_left(physics) == doctest::Approx(1.0));
	CHECK(server->timer_get_time_left(unscaled) == doctest::Approx(0.75));
	CHECK(server->timer_get_time_left(always) == doctest::Approx(0.5));

	server->process(0.5, 0.5, true, false);
	CHECK(server->timer_get_time_left(physics) == doctest::Approx(0.5));

	server->process(0.25, 0.25, false, true); // Tree paused: only "always" clocks advance.
	CHECK(server->timer_get_time_left(unscaled) == doctest::Approx(0.75));
	CHECK(server->timer_get_time_left(always) == doctest::Approx(0.25));

	server->timer_set_suspended(always, true);
	server->process(1.0, 1.0, false, true);
	CHECK(server->timer_get_time_left(always) == doctest::Approx(0.25));
	server->free_rid(physics);
	server->free_rid(unscaled);
	server->free_rid(always);
}

TEST_CASE("[TimerServer] Many timers: heap order and stale entries") {
	TimerServer *server = TimerServer::get_singleton();
	reset_events();
	LocalVector<RID> rids;
	for (int idx = 0; idx < 100; idx++) {
		const RID rid = make_timer((idx + 1) * 0.01);
		server->timer_start(rid);
		rids.push_back(rid);
	}
	for (int idx = 0; idx < 300; idx++) {
		server->timer_set_paused(rids[idx % 100], true);
		server->timer_set_paused(rids[idx % 100], false);
	}
	step(0.505);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 50);
	step(0.5);
	CHECK(event_count[TimerServer::TIMER_EVENT_TIMEOUT] == 100);
	for (const RID &rid : rids) {
		server->free_rid(rid);
	}
}

TEST_CASE("[TimerServer] TimerInstance, TimerInfo, lookup and pause_all") {
	TimerServer *server = TimerServer::get_singleton();
	Ref<TimerInfo> info;
	info.instantiate();
	info->set_timer_name("round");
	info->set_total_time(2.0);
	const int count = server->get_timer_count();
	Ref<TimerInstance> instance = server->create_timer(info);
	CHECK(server->get_timer_count() == count + 1);
	CHECK(server->get_timer("round") == instance->get_rid());
	CHECK(server->timer_get_object(instance->get_rid()) == instance.ptr());

	SIGNAL_WATCH(instance.ptr(), SNAME("timeout"));
	SIGNAL_WATCH(instance.ptr(), SNAME("pause_changed"));
	instance->start();
	step(0.5);
	server->pause_all();
	CHECK(instance->is_paused());
	SIGNAL_CHECK(SNAME("pause_changed"), emissions_with(true));
	server->resume_all();
	CHECK_FALSE(instance->is_paused());
	CHECK(instance->get_time_text() == "00:02");

	info->set_total_time(0.5); // Applies live: 0.5 s have passed, so the timer ends in the next frame.
	step(0.0);
	SIGNAL_CHECK(SNAME("timeout"), emissions_without_args());
	CHECK(instance->is_stopped());
	SIGNAL_UNWATCH(instance.ptr(), SNAME("timeout"));
	SIGNAL_UNWATCH(instance.ptr(), SNAME("pause_changed"));

	const RID rid = instance->get_rid();
	instance.unref();
	CHECK_FALSE(server->timer_is_valid(rid));
}

TEST_CASE("[TimerServer] A running TimerInstance stays alive until it ends") {
	TimerServer *server = TimerServer::get_singleton();
	Ref<TimerInfo> info;
	info.instantiate();
	info->set_total_time(1.0);
	RID rid;
	{
		Ref<TimerInstance> instance = server->create_timer(info);
		rid = instance->get_rid();
		instance->start();
	}
	CHECK(server->timer_is_valid(rid));
	step(1.5);
	CHECK_FALSE(server->timer_is_valid(rid));
}

TEST_CASE("[TimerServer] format_time") {
	CHECK(TimerServer::format_time(75.4) == "01:15");
	CHECK(TimerServer::format_time(4503.25) == "75:03");
	CHECK(TimerServer::format_time(3723.456, "{h}:{mm}:{ss}.{cs}") == "1:02:03.45");
	CHECK(TimerServer::format_time(3723.456, "{s}.{ms}") == "3723.456");
	CHECK(TimerServer::format_time(59.99, "{mm}:{ss}") == "00:59");
	CHECK(TimerServer::format_time(59.01, "{mm}:{ss}", true) == "01:00");
	CHECK(TimerServer::format_time(0.2, "{s}", true) == "1");
	CHECK(TimerServer::format_time(-3.0, "{s}") == "0");
}

TEST_CASE("[SceneTree][TimerServer] Timer node: tree pause, process mode, TimerInfo and new API") {
	TimerServer *server = TimerServer::get_singleton();
	SceneTree *tree = SceneTree::get_singleton();
	Timer *timer = memnew(Timer);
	CHECK_FALSE(timer->get_timer_rid().is_valid());
	tree->get_root()->add_child(timer);
	const RID rid = timer->get_timer_rid();
	CHECK(rid.is_valid());
	CHECK(server->timer_get_object(rid) == timer);

	SUBCASE("Tree pause and process_mode") {
		SIGNAL_WATCH(timer, SNAME("timeout"));
		timer->start(1.0);
		tree->set_pause(true);
		tree->process(2.0);
		SIGNAL_CHECK_FALSE(SNAME("timeout"));
		CHECK(timer->get_time_left() == doctest::Approx(1.0));
		timer->set_process_mode(Node::PROCESS_MODE_ALWAYS);
		tree->process(2.0);
		SIGNAL_CHECK(SNAME("timeout"), emissions_without_args());
		tree->set_pause(false);
		SIGNAL_UNWATCH(timer, SNAME("timeout"));
	}

	SUBCASE("pause_all acts on Timer nodes") {
		SIGNAL_WATCH(timer, SNAME("pause_changed"));
		timer->start(1.0);
		server->pause_all();
		CHECK(timer->is_paused());
		SIGNAL_CHECK(SNAME("pause_changed"), emissions_with(true));
		server->stop_all();
		CHECK(timer->is_stopped());
		timer->resume();
		SIGNAL_UNWATCH(timer, SNAME("pause_changed"));
	}

	SUBCASE("TimerInfo applies to the node") {
		Ref<TimerInfo> info;
		info.instantiate();
		info->set_total_time(3.0);
		info->set_loop(true);
		info->set_tick_mode(TimerInfo::TICK_MODE_STOPWATCH);
		timer->set_timer_info(info);
		CHECK(timer->get_wait_time() == doctest::Approx(3.0));
		CHECK_FALSE(timer->is_one_shot());
		CHECK(timer->get_tick_mode() == TimerInfo::TICK_MODE_STOPWATCH);
		timer->start();
		tree->process(1.25);
		CHECK(timer->get_current_time() == doctest::Approx(1.25));
		CHECK(timer->get_time_text("{s}.{cs}") == "1.25");
		timer->add_time(1.0);
		CHECK(timer->get_current_time() == doctest::Approx(2.25));
		timer->stop();
		timer->set_timer_info(Ref<TimerInfo>());
	}

	SUBCASE("State survives reparenting") {
		timer->start(5.0);
		tree->process(1.0);
		tree->get_root()->remove_child(timer);
		tree->process(1.0);
		tree->get_root()->add_child(timer);
		CHECK(timer->get_time_left() == doctest::Approx(4.0));
		CHECK(timer->get_timer_rid() == rid);
		timer->stop();
	}

	memdelete(timer);
	CHECK_FALSE(server->timer_is_valid(rid));
}

} // namespace TestTimerServer
