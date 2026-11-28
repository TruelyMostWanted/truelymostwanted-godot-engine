/**************************************************************************/
/*  timer.cpp                                                             */
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

#include "timer.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "scene/main/timer_server.h"

// TimerServer is main-thread only: on its path, calls from sub-thread process groups or other threads run deferred.
#define TIMER_DEFER_OFF_MAIN_THREAD(m_deferred_call) \
	if (timer.is_valid() && !Thread::is_main_thread()) { \
		m_deferred_call; \
		return; \
	}

void Timer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!timer.is_valid()) {
				_create_timer();
			}
			_update_suspended();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (timer.is_valid()) {
				TimerServer::get_singleton()->timer_set_suspended(timer, true);
			}
		} break;

		case NOTIFICATION_PAUSED:
		case NOTIFICATION_UNPAUSED:
		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_UNSUSPENDED: {
			_update_suspended();
		} break;

		case NOTIFICATION_PATH_RENAMED: {
			if (timer.is_valid() && (timer_info.is_null() || timer_info->get_timer_name() == StringName())) {
				TimerServer::get_singleton()->timer_set_name(timer, get_name());
			}
		} break;

		case NOTIFICATION_READY: {
			if (autostart) {
#ifdef TOOLS_ENABLED
				if (is_part_of_edited_scene()) {
					break;
				}
#endif
				start();
				autostart = false;
			}
		} break;

		// Godot's own code path, used while the project setting is off.
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!processing || timer_process_callback == TIMER_PROCESS_PHYSICS || !is_processing_internal()) {
				return;
			}
			if (ignore_time_scale) {
				time_left -= Engine::get_singleton()->get_process_step();
			} else {
				time_left -= get_process_delta_time();
			}

			if (time_left < 0) {
				if (!one_shot) {
					time_left += wait_time;
				} else {
					processing = false; // The natural end emits no `stopped`, as on the TimerServer path.
					stop();
				}

				emit_signal(SNAME("timeout"));
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!processing || timer_process_callback == TIMER_PROCESS_IDLE || !is_physics_processing_internal()) {
				return;
			}
			if (ignore_time_scale) {
				time_left -= Engine::get_singleton()->get_process_step();
			} else {
				time_left -= get_physics_process_delta_time();
			}

			if (time_left < 0) {
				if (!one_shot) {
					time_left += wait_time;
				} else {
					processing = false;
					stop();
				}
				emit_signal(SNAME("timeout"));
			}
		} break;
	}
}

// The timer runs on an "always" clock and is suspended while the node can't process, so tree pause, `process_mode`
// and a suspended tree act on it exactly as on Godot's internal processing, without the node being processed.
void Timer::_create_timer() {
	TimerServer *server = TimerServer::get_singleton();
	if (!server || !server->is_used_for_timer_nodes()) {
		return;
	}
	timer = server->timer_create();
	server->timer_set_object(timer, this);
	bool engine_owned = is_internal();
#ifdef TOOLS_ENABLED
	engine_owned = engine_owned || (Engine::get_singleton()->is_editor_hint() && !is_part_of_edited_scene());
#endif
	server->timer_set_engine_owned(timer, engine_owned);
	server->timer_set_name(timer, get_name());
	server->timer_set_process_always(timer, true);
	server->timer_set_suspended(timer, true);
	server->timer_set_total_time(timer, wait_time);
	server->timer_set_loop(timer, !one_shot);
	server->timer_set_tick_mode(timer, tick_mode);
	server->timer_set_ignore_time_scale(timer, ignore_time_scale);
	server->timer_set_process_in_physics(timer, timer_process_callback == TIMER_PROCESS_PHYSICS);
	server->timer_set_paused(timer, paused);
	_apply_timer_info();
	server->timer_set_callback(timer, callable_mp(this, &Timer::_on_event)); // Last: the setup emits no signals.
}

void Timer::_update_suspended() {
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_suspended(timer, !can_process());
	}
}

void Timer::_apply_timer_info() {
	if (timer_info.is_null()) {
		return;
	}
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::_apply_timer_info).call_deferred());
	if (timer_info->get_total_time() > 0.0) {
		wait_time = timer_info->get_total_time();
	}
	one_shot = !timer_info->is_loop();
	tick_mode = timer_info->get_tick_mode();
	ignore_time_scale = timer_info->is_ignoring_time_scale();
	if (timer.is_valid()) {
		// The node follows its own Node.process_mode, so TimerInfo.process_mode is not applied.
		_set_server_total_time(timer_info);
	}
	set_timer_process_callback(timer_info->get_process_callback() == TimerInfo::PROCESS_CALLBACK_PHYSICS ? TIMER_PROCESS_PHYSICS : TIMER_PROCESS_IDLE);
	update_configuration_warnings();
}

// As in Godot's Timer, a new wait_time keeps the time left of the running cycle and applies from the next cycle.
void Timer::_set_server_total_time(const Ref<TimerInfo> &p_info) {
	TimerServer *server = TimerServer::get_singleton();
	const bool keep = server->timer_is_running(timer) && server->timer_get_tick_mode(timer) == TimerInfo::TICK_MODE_COUNTDOWN && tick_mode == TimerInfo::TICK_MODE_COUNTDOWN;
	const double left = server->timer_get_time_left(timer);
	if (p_info.is_valid()) {
		server->timer_set_info(timer, p_info, false);
	}
	if (p_info.is_null() || tick_mode == TimerInfo::TICK_MODE_COUNTDOWN) {
		server->timer_set_total_time(timer, wait_time); // A TimerInfo without total_time keeps the node's wait_time.
	}
	if (keep) {
		server->timer_set_current_time(timer, left);
	}
}

void Timer::_on_event(int p_event, double p_time) {
	switch (p_event) {
		case TimerServer::TIMER_EVENT_STARTED:
			emit_signal(SNAME("started"));
			break;
		case TimerServer::TIMER_EVENT_STOPPED:
			emit_signal(SNAME("stopped"));
			break;
		case TimerServer::TIMER_EVENT_PAUSED:
		case TimerServer::TIMER_EVENT_RESUMED:
			paused = p_event == TimerServer::TIMER_EVENT_PAUSED;
			emit_signal(SNAME("pause_changed"), paused);
			break;
		case TimerServer::TIMER_EVENT_TICKED:
			emit_signal(SNAME("ticked"), p_time);
			break;
		case TimerServer::TIMER_EVENT_ALERT_REACHED:
			emit_signal(SNAME("alert_reached"), p_time);
			break;
		case TimerServer::TIMER_EVENT_TIMEOUT:
			if (one_shot) {
				autostart = false; // As Godot's stop() did at the end of a one-shot timer.
			}
			emit_signal(SNAME("timeout"));
			break;
	}
}

void Timer::set_wait_time(double p_time) {
	ERR_FAIL_COND_MSG(p_time <= 0, "Time should be greater than zero.");
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_wait_time).call_deferred(p_time));
	wait_time = p_time;
	if (timer.is_valid()) {
		_set_server_total_time(Ref<TimerInfo>());
	}
	update_configuration_warnings();
}

double Timer::get_wait_time() const {
	return wait_time;
}

void Timer::set_one_shot(bool p_one_shot) {
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_one_shot).call_deferred(p_one_shot));
	one_shot = p_one_shot;
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_loop(timer, !one_shot);
	}
}

bool Timer::is_one_shot() const {
	return one_shot;
}

void Timer::set_autostart(bool p_start) {
	autostart = p_start;
}

bool Timer::has_autostart() const {
	return autostart;
}

void Timer::start(double p_time) {
	ERR_FAIL_COND_MSG(!is_inside_tree(), "Unable to start the timer because it's not inside the scene tree. Either add it or set autostart to true.");

	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::start).call_deferred(p_time));
	if (p_time > 0) {
		set_wait_time(p_time);
	}
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_start(timer);
		return;
	}
	time_left = wait_time;
	_set_process(true);
	emit_signal(SNAME("started"));
}

void Timer::stop() {
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::stop).call_deferred());
	autostart = false;
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_stop(timer);
		return;
	}
	const bool was_running = processing;
	time_left = -1;
	_set_process(false);
	if (was_running) {
		emit_signal(SNAME("stopped"));
	}
}

void Timer::set_paused(bool p_paused) {
	if (paused == p_paused) {
		return;
	}
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_paused).call_deferred(p_paused));

	paused = p_paused;
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_paused(timer, paused); // Emits `pause_changed` through _on_event().
		return;
	}
	_set_process(processing);
	emit_signal(SNAME("pause_changed"), paused);
}

bool Timer::is_paused() const {
	return paused;
}

void Timer::set_ignore_time_scale(bool p_ignore) {
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_ignore_time_scale).call_deferred(p_ignore));
	ignore_time_scale = p_ignore;
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_ignore_time_scale(timer, ignore_time_scale);
	}
}

bool Timer::is_ignoring_time_scale() {
	return ignore_time_scale;
}

bool Timer::is_stopped() const {
	if (timer.is_valid()) {
		return !TimerServer::get_singleton()->timer_is_running(timer);
	}
	return get_time_left() <= 0;
}

double Timer::get_time_left() const {
	if (timer.is_valid()) {
		return TimerServer::get_singleton()->timer_get_time_left(timer);
	}
	return time_left > 0 ? time_left : 0;
}

void Timer::set_timer_process_callback(TimerProcessCallback p_callback) {
	if (timer_process_callback == p_callback) {
		return;
	}
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_timer_process_callback).call_deferred(p_callback));

	if (timer.is_valid()) {
		timer_process_callback = p_callback;
		TimerServer::get_singleton()->timer_set_process_in_physics(timer, timer_process_callback == TIMER_PROCESS_PHYSICS);
		return;
	}
	switch (timer_process_callback) {
		case TIMER_PROCESS_PHYSICS:
			if (is_physics_processing_internal()) {
				set_physics_process_internal(false);
				set_process_internal(true);
			}
			break;
		case TIMER_PROCESS_IDLE:
			if (is_processing_internal()) {
				set_process_internal(false);
				set_physics_process_internal(true);
			}
			break;
	}
	timer_process_callback = p_callback;
}

Timer::TimerProcessCallback Timer::get_timer_process_callback() const {
	return timer_process_callback;
}

void Timer::_set_process(bool p_process, bool p_force) {
	switch (timer_process_callback) {
		case TIMER_PROCESS_PHYSICS:
			set_physics_process_internal(p_process && !paused);
			break;
		case TIMER_PROCESS_IDLE:
			set_process_internal(p_process && !paused);
			break;
	}
	processing = p_process;
}

RID Timer::get_timer_rid() const {
	return timer;
}

void Timer::set_timer_info(const Ref<TimerInfo> &p_info) {
	if (timer_info == p_info) {
		return;
	}
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_timer_info).call_deferred(p_info));
	const Callable changed = callable_mp(this, &Timer::_apply_timer_info);
	if (timer_info.is_valid()) {
		timer_info->disconnect_changed(changed);
	}
	timer_info = p_info;
	if (timer_info.is_valid()) {
		timer_info->connect_changed(changed);
		_apply_timer_info();
	} else if (timer.is_valid()) {
		TimerServer *server = TimerServer::get_singleton();
		server->timer_set_tick_interval(timer, 0.0);
		server->timer_set_alerts(timer, PackedFloat64Array());
		server->timer_set_time_scale(timer, 1.0);
		server->timer_set_name(timer, get_name());
	}
	notify_property_list_changed();
}

Ref<TimerInfo> Timer::get_timer_info() const {
	return timer_info;
}

void Timer::set_tick_mode(TimerInfo::TimerTickMode p_mode) {
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_tick_mode).call_deferred(p_mode));
	tick_mode = p_mode;
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_tick_mode(timer, tick_mode);
	}
}

TimerInfo::TimerTickMode Timer::get_tick_mode() const {
	return tick_mode;
}

void Timer::set_current_time(double p_time) {
	TIMER_DEFER_OFF_MAIN_THREAD(callable_mp(this, &Timer::set_current_time).call_deferred(p_time));
	if (timer.is_valid()) {
		TimerServer::get_singleton()->timer_set_current_time(timer, p_time);
	} else if (processing) {
		time_left = tick_mode == TimerInfo::TICK_MODE_COUNTDOWN ? p_time : wait_time - p_time;
	}
}

double Timer::get_current_time() const {
	if (timer.is_valid()) {
		return TimerServer::get_singleton()->timer_get_current_time(timer);
	}
	if (tick_mode == TimerInfo::TICK_MODE_COUNTDOWN) {
		return get_time_left();
	}
	return processing ? wait_time - get_time_left() : 0.0;
}

void Timer::pause() {
	set_paused(true);
}

void Timer::resume() {
	set_paused(false);
}

void Timer::restart() {
	set_paused(false);
	start();
}

void Timer::add_time(double p_time) {
	set_current_time(get_current_time() + p_time);
}

String Timer::get_time_text(const String &p_format) const {
	return TimerServer::format_time(get_current_time(), p_format, tick_mode == TimerInfo::TICK_MODE_COUNTDOWN);
}

PackedStringArray Timer::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (wait_time < 0.05 - CMP_EPSILON) {
		warnings.push_back(RTR("Very low timer wait times (< 0.05 seconds) may behave in significantly different ways depending on the rendered or physics frame rate.\nConsider using a script's process loop instead of relying on a Timer for very low wait times."));
	}

	return warnings;
}

void Timer::_validate_property(PropertyInfo &p_property) const {
	if (timer_info.is_null()) {
		return;
	}
	// Values that come from timer_info are shown read-only.
	if (p_property.name == "wait_time" || p_property.name == "one_shot" || p_property.name == "tick_mode" || p_property.name == "ignore_time_scale" || p_property.name == "process_callback") {
		p_property.usage |= PROPERTY_USAGE_READ_ONLY;
	}
}

void Timer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_wait_time", "time_sec"), &Timer::set_wait_time);
	ClassDB::bind_method(D_METHOD("get_wait_time"), &Timer::get_wait_time);

	ClassDB::bind_method(D_METHOD("set_one_shot", "enable"), &Timer::set_one_shot);
	ClassDB::bind_method(D_METHOD("is_one_shot"), &Timer::is_one_shot);

	ClassDB::bind_method(D_METHOD("set_autostart", "enable"), &Timer::set_autostart);
	ClassDB::bind_method(D_METHOD("has_autostart"), &Timer::has_autostart);

	ClassDB::bind_method(D_METHOD("start", "time_sec"), &Timer::start, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("stop"), &Timer::stop);

	ClassDB::bind_method(D_METHOD("set_paused", "paused"), &Timer::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &Timer::is_paused);

	ClassDB::bind_method(D_METHOD("set_ignore_time_scale", "ignore"), &Timer::set_ignore_time_scale);
	ClassDB::bind_method(D_METHOD("is_ignoring_time_scale"), &Timer::is_ignoring_time_scale);

	ClassDB::bind_method(D_METHOD("is_stopped"), &Timer::is_stopped);

	ClassDB::bind_method(D_METHOD("get_time_left"), &Timer::get_time_left);

	ClassDB::bind_method(D_METHOD("set_timer_process_callback", "callback"), &Timer::set_timer_process_callback);
	ClassDB::bind_method(D_METHOD("get_timer_process_callback"), &Timer::get_timer_process_callback);

	// Truely (#379).
	ClassDB::bind_method(D_METHOD("get_timer_rid"), &Timer::get_timer_rid);
	ClassDB::bind_method(D_METHOD("set_timer_info", "info"), &Timer::set_timer_info);
	ClassDB::bind_method(D_METHOD("get_timer_info"), &Timer::get_timer_info);
	ClassDB::bind_method(D_METHOD("set_tick_mode", "mode"), &Timer::set_tick_mode);
	ClassDB::bind_method(D_METHOD("get_tick_mode"), &Timer::get_tick_mode);
	ClassDB::bind_method(D_METHOD("set_current_time", "time_sec"), &Timer::set_current_time);
	ClassDB::bind_method(D_METHOD("get_current_time"), &Timer::get_current_time);
	ClassDB::bind_method(D_METHOD("pause"), &Timer::pause);
	ClassDB::bind_method(D_METHOD("resume"), &Timer::resume);
	ClassDB::bind_method(D_METHOD("restart"), &Timer::restart);
	ClassDB::bind_method(D_METHOD("add_time", "time_sec"), &Timer::add_time);
	ClassDB::bind_method(D_METHOD("get_time_text", "format"), &Timer::get_time_text, DEFVAL("{mm}:{ss}"));

	ADD_SIGNAL(MethodInfo("timeout"));
	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("stopped"));
	ADD_SIGNAL(MethodInfo("pause_changed", PropertyInfo(Variant::BOOL, "paused")));
	ADD_SIGNAL(MethodInfo("ticked", PropertyInfo(Variant::FLOAT, "time")));
	ADD_SIGNAL(MethodInfo("alert_reached", PropertyInfo(Variant::FLOAT, "time")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_callback", PROPERTY_HINT_ENUM, "Physics,Idle"), "set_timer_process_callback", "get_timer_process_callback");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wait_time", PROPERTY_HINT_RANGE, "0.001,4096,0.001,or_greater,exp,suffix:s"), "set_wait_time", "get_wait_time");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "is_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autostart"), "set_autostart", "has_autostart");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_paused", "is_paused");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_time_scale"), "set_ignore_time_scale", "is_ignoring_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_left", PROPERTY_HINT_NONE, "suffix:s", PROPERTY_USAGE_NONE), "", "get_time_left");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_mode", PROPERTY_HINT_ENUM, "Countdown,Stopwatch"), "set_tick_mode", "get_tick_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_time", PROPERTY_HINT_NONE, "suffix:s", PROPERTY_USAGE_NONE), "set_current_time", "get_current_time");
	// Last, so its values apply over the node's own when a scene loads.
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "timer_info", PROPERTY_HINT_RESOURCE_TYPE, "TimerInfo"), "set_timer_info", "get_timer_info");

	BIND_ENUM_CONSTANT(TIMER_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(TIMER_PROCESS_IDLE);
}

Timer::~Timer() {
	if (timer_info.is_valid()) {
		timer_info->disconnect_changed(callable_mp(this, &Timer::_apply_timer_info));
	}
	TimerServer *server = TimerServer::get_singleton();
	if (timer.is_valid() && server && server->timer_is_valid(timer)) {
		server->free_rid(timer);
	}
}
