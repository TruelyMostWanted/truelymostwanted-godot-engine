/**************************************************************************/
/*  performance_budget.h                                                  */
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
#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "main/performance_summary.h"

// One limit of a PerformanceBudget: a statistic of a recorded column must not exceed a value.
class PerformanceBudgetRule : public Resource {
	GDCLASS(PerformanceBudgetRule, Resource);

public:
	enum Statistic {
		STATISTIC_MINIMUM,
		STATISTIC_MAXIMUM,
		STATISTIC_MEAN,
		STATISTIC_PERCENTILE,
		STATISTIC_HITCH_COUNT,
		STATISTIC_MAX,
	};

private:
	String column = "frame_time";
	Statistic statistic = STATISTIC_PERCENTILE;
	double percentile = 95.0;
	double limit = 16667.0;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_column(const String &p_column);
	String get_column() const;

	void set_statistic(Statistic p_statistic);
	Statistic get_statistic() const;

	void set_percentile(double p_percentile);
	double get_percentile() const;

	void set_limit(double p_limit);
	double get_limit() const;

	// Short text such as "frame_time p95", used in failure messages.
	String get_description() const;
};

// One failed rule of a budget check.
class PerformanceBudgetFailure : public RefCounted {
	GDCLASS(PerformanceBudgetFailure, RefCounted);

public:
	enum Reason {
		REASON_OVER_LIMIT,
		REASON_COLUMN_MISSING,
		REASON_NO_SAMPLES,
		REASON_MAX,
	};

private:
	Ref<PerformanceBudgetRule> rule;
	Reason reason = REASON_OVER_LIMIT;
	double actual_value = 0.0;
	double limit = 0.0;

protected:
	static void _bind_methods();

public:
	void setup(const Ref<PerformanceBudgetRule> &p_rule, Reason p_reason, double p_actual_value);

	Ref<PerformanceBudgetRule> get_rule() const;
	Reason get_reason() const;
	double get_actual_value() const;
	double get_limit() const;
	String get_message() const;
};

// Result of PerformanceBudget.check_summary() and PerformanceRecorder.check_budget().
class PerformanceBudgetResult : public RefCounted {
	GDCLASS(PerformanceBudgetResult, RefCounted);

	LocalVector<Ref<PerformanceBudgetFailure>> failures;
	int checked_rule_count = 0;

protected:
	static void _bind_methods();

public:
	void add_failure(const Ref<PerformanceBudgetFailure> &p_failure);
	void set_checked_rule_count(int p_count);

	bool is_passed() const;
	int get_checked_rule_count() const;
	int get_failure_count() const;
	TypedArray<PerformanceBudgetFailure> get_failures() const;
	int get_exit_code() const;
	String to_text() const;
};

// A list of limits for a recorded run, editable in the Inspector and saved as a resource.
class PerformanceBudget : public Resource {
	GDCLASS(PerformanceBudget, Resource);

	LocalVector<Ref<PerformanceBudgetRule>> rules;

protected:
	static void _bind_methods();

public:
	void set_rules(const TypedArray<PerformanceBudgetRule> &p_rules);
	TypedArray<PerformanceBudgetRule> get_rules() const;

	void add_rule(const Ref<PerformanceBudgetRule> &p_rule);
	int get_rule_count() const;

	Ref<PerformanceBudgetResult> check_summary(const Ref<PerformanceSummary> &p_summary) const;
};

VARIANT_ENUM_CAST(PerformanceBudgetRule::Statistic);
VARIANT_ENUM_CAST(PerformanceBudgetFailure::Reason);
