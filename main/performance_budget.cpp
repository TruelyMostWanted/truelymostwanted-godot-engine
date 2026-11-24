/**************************************************************************/
/*  performance_budget.cpp                                                */
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

#include "performance_budget.h"

#include "core/object/class_db.h"

/* PerformanceBudgetRule */

void PerformanceBudgetRule::set_column(const String &p_column) {
	column = p_column;
	emit_changed();
}

String PerformanceBudgetRule::get_column() const {
	return column;
}

void PerformanceBudgetRule::set_statistic(Statistic p_statistic) {
	ERR_FAIL_INDEX((int)p_statistic, (int)STATISTIC_MAX);
	statistic = p_statistic;
	notify_property_list_changed();
	emit_changed();
}

PerformanceBudgetRule::Statistic PerformanceBudgetRule::get_statistic() const {
	return statistic;
}

void PerformanceBudgetRule::set_percentile(double p_percentile) {
	percentile = CLAMP(p_percentile, 0.0, 100.0);
	emit_changed();
}

double PerformanceBudgetRule::get_percentile() const {
	return percentile;
}

void PerformanceBudgetRule::set_limit(double p_limit) {
	limit = p_limit;
	emit_changed();
}

double PerformanceBudgetRule::get_limit() const {
	return limit;
}

String PerformanceBudgetRule::get_description() const {
	switch (statistic) {
		case STATISTIC_MINIMUM:
			return column + " min";
		case STATISTIC_MAXIMUM:
			return column + " max";
		case STATISTIC_MEAN:
			return column + " mean";
		case STATISTIC_PERCENTILE:
			// p95, p99.9: no trailing ".0" for whole percentiles.
			return column + " p" + (Math::is_equal_approx(percentile, Math::round(percentile)) ? itos(int64_t(Math::round(percentile))) : String::num(percentile));
		case STATISTIC_HITCH_COUNT:
			return "hitch count";
		default:
			return column;
	}
}

void PerformanceBudgetRule::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "percentile" && statistic != STATISTIC_PERCENTILE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	} else if (p_property.name == "column" && statistic == STATISTIC_HITCH_COUNT) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void PerformanceBudgetRule::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_column", "column"), &PerformanceBudgetRule::set_column);
	ClassDB::bind_method(D_METHOD("get_column"), &PerformanceBudgetRule::get_column);
	ClassDB::bind_method(D_METHOD("set_statistic", "statistic"), &PerformanceBudgetRule::set_statistic);
	ClassDB::bind_method(D_METHOD("get_statistic"), &PerformanceBudgetRule::get_statistic);
	ClassDB::bind_method(D_METHOD("set_percentile", "percentile"), &PerformanceBudgetRule::set_percentile);
	ClassDB::bind_method(D_METHOD("get_percentile"), &PerformanceBudgetRule::get_percentile);
	ClassDB::bind_method(D_METHOD("set_limit", "limit"), &PerformanceBudgetRule::set_limit);
	ClassDB::bind_method(D_METHOD("get_limit"), &PerformanceBudgetRule::get_limit);
	ClassDB::bind_method(D_METHOD("get_description"), &PerformanceBudgetRule::get_description);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "column"), "set_column", "get_column");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "statistic", PROPERTY_HINT_ENUM, "Minimum,Maximum,Mean,Percentile,Hitch Count"), "set_statistic", "get_statistic");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "percentile", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_percentile", "get_percentile");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit", PROPERTY_HINT_RANGE, "0,1000000,0.001,or_greater,or_less"), "set_limit", "get_limit");

	BIND_ENUM_CONSTANT(STATISTIC_MINIMUM);
	BIND_ENUM_CONSTANT(STATISTIC_MAXIMUM);
	BIND_ENUM_CONSTANT(STATISTIC_MEAN);
	BIND_ENUM_CONSTANT(STATISTIC_PERCENTILE);
	BIND_ENUM_CONSTANT(STATISTIC_HITCH_COUNT);
	BIND_ENUM_CONSTANT(STATISTIC_MAX);
}

/* PerformanceBudgetFailure */

void PerformanceBudgetFailure::setup(const Ref<PerformanceBudgetRule> &p_rule, Reason p_reason, double p_actual_value) {
	rule = p_rule;
	reason = p_reason;
	actual_value = p_actual_value;
	limit = p_rule.is_valid() ? p_rule->get_limit() : 0.0;
}

Ref<PerformanceBudgetRule> PerformanceBudgetFailure::get_rule() const {
	return rule;
}

PerformanceBudgetFailure::Reason PerformanceBudgetFailure::get_reason() const {
	return reason;
}

double PerformanceBudgetFailure::get_actual_value() const {
	return actual_value;
}

double PerformanceBudgetFailure::get_limit() const {
	return limit;
}

String PerformanceBudgetFailure::get_message() const {
	const String description = rule.is_valid() ? rule->get_description() : String("?");
	const String column_name = rule.is_valid() ? rule->get_column() : String("?");
	switch (reason) {
		case REASON_OVER_LIMIT:
			return vformat("%s = %s exceeds the limit %s.", description, String::num(actual_value), String::num(limit));
		case REASON_COLUMN_MISSING:
			return vformat("%s: the column \"%s\" was not recorded.", description, column_name);
		case REASON_NO_SAMPLES:
			return vformat("%s: the column \"%s\" has no samples.", description, column_name);
		default:
			return description;
	}
}

void PerformanceBudgetFailure::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rule"), &PerformanceBudgetFailure::get_rule);
	ClassDB::bind_method(D_METHOD("get_reason"), &PerformanceBudgetFailure::get_reason);
	ClassDB::bind_method(D_METHOD("get_actual_value"), &PerformanceBudgetFailure::get_actual_value);
	ClassDB::bind_method(D_METHOD("get_limit"), &PerformanceBudgetFailure::get_limit);
	ClassDB::bind_method(D_METHOD("get_message"), &PerformanceBudgetFailure::get_message);

	BIND_ENUM_CONSTANT(REASON_OVER_LIMIT);
	BIND_ENUM_CONSTANT(REASON_COLUMN_MISSING);
	BIND_ENUM_CONSTANT(REASON_NO_SAMPLES);
	BIND_ENUM_CONSTANT(REASON_MAX);
}

/* PerformanceBudgetResult */

void PerformanceBudgetResult::add_failure(const Ref<PerformanceBudgetFailure> &p_failure) {
	ERR_FAIL_COND(p_failure.is_null());
	failures.push_back(p_failure);
}

void PerformanceBudgetResult::set_checked_rule_count(int p_count) {
	checked_rule_count = p_count;
}

bool PerformanceBudgetResult::is_passed() const {
	return failures.is_empty();
}

int PerformanceBudgetResult::get_checked_rule_count() const {
	return checked_rule_count;
}

int PerformanceBudgetResult::get_failure_count() const {
	return (int)failures.size();
}

TypedArray<PerformanceBudgetFailure> PerformanceBudgetResult::get_failures() const {
	TypedArray<PerformanceBudgetFailure> result;
	for (const Ref<PerformanceBudgetFailure> &failure : failures) {
		result.push_back(failure);
	}
	return result;
}

int PerformanceBudgetResult::get_exit_code() const {
	return failures.is_empty() ? 0 : 1;
}

String PerformanceBudgetResult::to_text() const {
	if (failures.is_empty()) {
		return vformat("Performance budget passed (%d rules).", checked_rule_count);
	}
	String text = vformat("Performance budget failed (%d of %d rules):", (int)failures.size(), checked_rule_count);
	for (const Ref<PerformanceBudgetFailure> &failure : failures) {
		text += "\n- " + failure->get_message();
	}
	return text;
}

void PerformanceBudgetResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_passed"), &PerformanceBudgetResult::is_passed);
	ClassDB::bind_method(D_METHOD("get_checked_rule_count"), &PerformanceBudgetResult::get_checked_rule_count);
	ClassDB::bind_method(D_METHOD("get_failure_count"), &PerformanceBudgetResult::get_failure_count);
	ClassDB::bind_method(D_METHOD("get_failures"), &PerformanceBudgetResult::get_failures);
	ClassDB::bind_method(D_METHOD("get_exit_code"), &PerformanceBudgetResult::get_exit_code);
	ClassDB::bind_method(D_METHOD("to_text"), &PerformanceBudgetResult::to_text);
}

/* PerformanceBudget */

void PerformanceBudget::set_rules(const TypedArray<PerformanceBudgetRule> &p_rules) {
	rules.clear();
	for (int i = 0; i < p_rules.size(); i++) {
		// Empty slots are kept, so the Inspector can add an element and then pick its resource.
		rules.push_back(Ref<PerformanceBudgetRule>(p_rules[i]));
	}
	emit_changed();
}

TypedArray<PerformanceBudgetRule> PerformanceBudget::get_rules() const {
	TypedArray<PerformanceBudgetRule> result;
	for (const Ref<PerformanceBudgetRule> &rule : rules) {
		result.push_back(rule);
	}
	return result;
}

void PerformanceBudget::add_rule(const Ref<PerformanceBudgetRule> &p_rule) {
	ERR_FAIL_COND(p_rule.is_null());
	rules.push_back(p_rule);
	emit_changed();
}

int PerformanceBudget::get_rule_count() const {
	return (int)rules.size();
}

Ref<PerformanceBudgetResult> PerformanceBudget::check_summary(const Ref<PerformanceSummary> &p_summary) const {
	Ref<PerformanceBudgetResult> result;
	result.instantiate();
	int checked = 0;
	for (const Ref<PerformanceBudgetRule> &rule : rules) {
		if (rule.is_null()) {
			continue;
		}
		checked++;
		Ref<PerformanceBudgetFailure> failure;

		if (rule->get_statistic() == PerformanceBudgetRule::STATISTIC_HITCH_COUNT) {
			const double hitches = p_summary.is_valid() ? (double)p_summary->get_hitch_count() : 0.0;
			if (hitches > rule->get_limit()) {
				failure.instantiate();
				failure->setup(rule, PerformanceBudgetFailure::REASON_OVER_LIMIT, hitches);
			}
		} else {
			const Ref<PerformanceColumnSummary> column = p_summary.is_valid() ? p_summary->find_column(rule->get_column()) : Ref<PerformanceColumnSummary>();
			if (column.is_null()) {
				failure.instantiate();
				failure->setup(rule, PerformanceBudgetFailure::REASON_COLUMN_MISSING, 0.0);
			} else if (column->get_sample_count() == 0) {
				failure.instantiate();
				failure->setup(rule, PerformanceBudgetFailure::REASON_NO_SAMPLES, 0.0);
			} else {
				double actual = 0.0;
				switch (rule->get_statistic()) {
					case PerformanceBudgetRule::STATISTIC_MINIMUM:
						actual = column->get_minimum();
						break;
					case PerformanceBudgetRule::STATISTIC_MAXIMUM:
						actual = column->get_maximum();
						break;
					case PerformanceBudgetRule::STATISTIC_MEAN:
						actual = column->get_mean();
						break;
					default:
						actual = column->get_percentile(rule->get_percentile());
						break;
				}
				if (actual > rule->get_limit()) {
					failure.instantiate();
					failure->setup(rule, PerformanceBudgetFailure::REASON_OVER_LIMIT, actual);
				}
			}
		}

		if (failure.is_valid()) {
			result->add_failure(failure);
		}
	}
	result->set_checked_rule_count(checked);
	return result;
}

void PerformanceBudget::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rules", "rules"), &PerformanceBudget::set_rules);
	ClassDB::bind_method(D_METHOD("get_rules"), &PerformanceBudget::get_rules);
	ClassDB::bind_method(D_METHOD("add_rule", "rule"), &PerformanceBudget::add_rule);
	ClassDB::bind_method(D_METHOD("get_rule_count"), &PerformanceBudget::get_rule_count);
	ClassDB::bind_method(D_METHOD("check_summary", "summary"), &PerformanceBudget::check_summary);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rules", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("PerformanceBudgetRule")), "set_rules", "get_rules");
}
