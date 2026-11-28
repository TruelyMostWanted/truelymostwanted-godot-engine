/**************************************************************************/
/*  timer_info.h                                                          */
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

#include "core/io/resource.h"
#include "core/variant/binder_common.h"

// Shareable description of a timer (#379): countdown or stopwatch, length, loop, ticks, alerts, clock. Applied to a
// `Timer` node through its `timer_info` property or to a code timer through `TimerServer.create_timer()`. Holds no
// running state: that lives in `TimerServer`.
class TimerInfo : public Resource {
	GDCLASS(TimerInfo, Resource);

public:
	enum TimerTickMode {
		TICK_MODE_COUNTDOWN,
		TICK_MODE_STOPWATCH,
	};

	enum TimerProcessMode {
		PROCESS_MODE_PAUSABLE,
		PROCESS_MODE_ALWAYS,
	};

	enum TimerProcessCallback {
		PROCESS_CALLBACK_IDLE,
		PROCESS_CALLBACK_PHYSICS,
	};

private:
	StringName timer_name;
	String description;
	TimerTickMode tick_mode = TICK_MODE_COUNTDOWN;
	double total_time = 1.0;
	bool loop = false;
	double tick_interval = 0.0;
	PackedFloat64Array alerts;
	double time_scale = 1.0;
	bool ignore_time_scale = false;
	TimerProcessMode process_mode = PROCESS_MODE_PAUSABLE;
	TimerProcessCallback process_callback = PROCESS_CALLBACK_IDLE;

protected:
	static void _bind_methods();

public:
	void set_timer_name(const StringName &p_name);
	StringName get_timer_name() const;

	void set_description(const String &p_description);
	String get_description() const;

	void set_tick_mode(TimerTickMode p_mode);
	TimerTickMode get_tick_mode() const;

	void set_total_time(double p_time);
	double get_total_time() const;

	void set_loop(bool p_loop);
	bool is_loop() const;

	void set_tick_interval(double p_interval);
	double get_tick_interval() const;

	void set_alerts(const PackedFloat64Array &p_alerts);
	PackedFloat64Array get_alerts() const;

	void set_time_scale(double p_scale);
	double get_time_scale() const;

	void set_ignore_time_scale(bool p_ignore);
	bool is_ignoring_time_scale() const;

	void set_process_mode(TimerProcessMode p_mode);
	TimerProcessMode get_process_mode() const;

	void set_process_callback(TimerProcessCallback p_callback);
	TimerProcessCallback get_process_callback() const;
};

VARIANT_ENUM_CAST(TimerInfo::TimerTickMode);
VARIANT_ENUM_CAST(TimerInfo::TimerProcessMode);
VARIANT_ENUM_CAST(TimerInfo::TimerProcessCallback);
