/**************************************************************************/
/*  timer_info.cpp                                                        */
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

#include "timer_info.h"

#include "core/object/class_db.h"

void TimerInfo::set_timer_name(const StringName &p_name) {
	if (timer_name == p_name) {
		return;
	}
	timer_name = p_name;
	emit_changed();
}

StringName TimerInfo::get_timer_name() const {
	return timer_name;
}

void TimerInfo::set_description(const String &p_description) {
	description = p_description;
}

String TimerInfo::get_description() const {
	return description;
}

void TimerInfo::set_tick_mode(TimerTickMode p_mode) {
	if (tick_mode == p_mode) {
		return;
	}
	tick_mode = p_mode;
	emit_changed();
}

TimerInfo::TimerTickMode TimerInfo::get_tick_mode() const {
	return tick_mode;
}

void TimerInfo::set_total_time(double p_time) {
	ERR_FAIL_COND_MSG(p_time < 0, "Total time can't be negative.");
	if (total_time == p_time) {
		return;
	}
	total_time = p_time;
	emit_changed();
}

double TimerInfo::get_total_time() const {
	return total_time;
}

void TimerInfo::set_loop(bool p_loop) {
	if (loop == p_loop) {
		return;
	}
	loop = p_loop;
	emit_changed();
}

bool TimerInfo::is_loop() const {
	return loop;
}

void TimerInfo::set_tick_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Tick interval can't be negative.");
	if (tick_interval == p_interval) {
		return;
	}
	tick_interval = p_interval;
	emit_changed();
}

double TimerInfo::get_tick_interval() const {
	return tick_interval;
}

void TimerInfo::set_alerts(const PackedFloat64Array &p_alerts) {
	alerts = p_alerts;
	emit_changed();
}

PackedFloat64Array TimerInfo::get_alerts() const {
	return alerts;
}

void TimerInfo::set_time_scale(double p_scale) {
	ERR_FAIL_COND_MSG(p_scale < 0, "Time scale can't be negative.");
	if (time_scale == p_scale) {
		return;
	}
	time_scale = p_scale;
	emit_changed();
}

double TimerInfo::get_time_scale() const {
	return time_scale;
}

void TimerInfo::set_ignore_time_scale(bool p_ignore) {
	if (ignore_time_scale == p_ignore) {
		return;
	}
	ignore_time_scale = p_ignore;
	emit_changed();
}

bool TimerInfo::is_ignoring_time_scale() const {
	return ignore_time_scale;
}

void TimerInfo::set_process_mode(TimerProcessMode p_mode) {
	if (process_mode == p_mode) {
		return;
	}
	process_mode = p_mode;
	emit_changed();
}

TimerInfo::TimerProcessMode TimerInfo::get_process_mode() const {
	return process_mode;
}

void TimerInfo::set_process_callback(TimerProcessCallback p_callback) {
	if (process_callback == p_callback) {
		return;
	}
	process_callback = p_callback;
	emit_changed();
}

TimerInfo::TimerProcessCallback TimerInfo::get_process_callback() const {
	return process_callback;
}

void TimerInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_timer_name", "name"), &TimerInfo::set_timer_name);
	ClassDB::bind_method(D_METHOD("get_timer_name"), &TimerInfo::get_timer_name);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &TimerInfo::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &TimerInfo::get_description);
	ClassDB::bind_method(D_METHOD("set_tick_mode", "mode"), &TimerInfo::set_tick_mode);
	ClassDB::bind_method(D_METHOD("get_tick_mode"), &TimerInfo::get_tick_mode);
	ClassDB::bind_method(D_METHOD("set_total_time", "time_sec"), &TimerInfo::set_total_time);
	ClassDB::bind_method(D_METHOD("get_total_time"), &TimerInfo::get_total_time);
	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &TimerInfo::set_loop);
	ClassDB::bind_method(D_METHOD("is_loop"), &TimerInfo::is_loop);
	ClassDB::bind_method(D_METHOD("set_tick_interval", "interval_sec"), &TimerInfo::set_tick_interval);
	ClassDB::bind_method(D_METHOD("get_tick_interval"), &TimerInfo::get_tick_interval);
	ClassDB::bind_method(D_METHOD("set_alerts", "alerts"), &TimerInfo::set_alerts);
	ClassDB::bind_method(D_METHOD("get_alerts"), &TimerInfo::get_alerts);
	ClassDB::bind_method(D_METHOD("set_time_scale", "scale"), &TimerInfo::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &TimerInfo::get_time_scale);
	ClassDB::bind_method(D_METHOD("set_ignore_time_scale", "ignore"), &TimerInfo::set_ignore_time_scale);
	ClassDB::bind_method(D_METHOD("is_ignoring_time_scale"), &TimerInfo::is_ignoring_time_scale);
	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &TimerInfo::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &TimerInfo::get_process_mode);
	ClassDB::bind_method(D_METHOD("set_process_callback", "callback"), &TimerInfo::set_process_callback);
	ClassDB::bind_method(D_METHOD("get_process_callback"), &TimerInfo::get_process_callback);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "timer_name"), "set_timer_name", "get_timer_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_mode", PROPERTY_HINT_ENUM, "Countdown,Stopwatch"), "set_tick_mode", "get_tick_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_time", PROPERTY_HINT_RANGE, "0,4096,0.001,or_greater,exp,suffix:s"), "set_total_time", "get_total_time");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "is_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tick_interval", PROPERTY_HINT_RANGE, "0,60,0.001,or_greater,suffix:s"), "set_tick_interval", "get_tick_interval");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "alerts"), "set_alerts", "get_alerts");
	ADD_GROUP("Clock", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_scale", PROPERTY_HINT_RANGE, "0,16,0.01,or_greater"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_time_scale"), "set_ignore_time_scale", "is_ignoring_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Pausable,Always"), "set_process_mode", "get_process_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_callback", PROPERTY_HINT_ENUM, "Idle,Physics"), "set_process_callback", "get_process_callback");

	BIND_ENUM_CONSTANT(TICK_MODE_COUNTDOWN);
	BIND_ENUM_CONSTANT(TICK_MODE_STOPWATCH);
	BIND_ENUM_CONSTANT(PROCESS_MODE_PAUSABLE);
	BIND_ENUM_CONSTANT(PROCESS_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_IDLE);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_PHYSICS);
}
