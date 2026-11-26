/**************************************************************************/
/*  data_validation.h                                                     */
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

// Settings of one validation run (DataSchema.validate(), TextDataFormat.validate()).
class DataValidationOptions : public Resource {
	GDCLASS(DataValidationOptions, Resource);

	int max_issues = 1000;
	bool stop_on_first = false;
	bool keep_positions = false;

protected:
	static void _bind_methods();

public:
	void set_max_issues(int p_max_issues);
	int get_max_issues() const { return max_issues; }
	void set_stop_on_first(bool p_stop_on_first);
	bool is_stop_on_first() const { return stop_on_first; }
	void set_keep_positions(bool p_keep_positions);
	bool is_keep_positions() const { return keep_positions; }
};

// The result of a validation: a list of issues, each with a kind, a message and the position of the value
// (field, path, row, column, line; POSITION_NONE where a position does not apply).
class DataValidation : public RefCounted {
	GDCLASS(DataValidation, RefCounted);

public:
	enum IssueKind {
		ISSUE_KIND_MISSING_FIELD,
		ISSUE_KIND_UNKNOWN_FIELD,
		ISSUE_KIND_TYPE_MISMATCH,
		ISSUE_KIND_OUT_OF_RANGE,
		ISSUE_KIND_NOT_IN_ENUM,
		ISSUE_KIND_PATTERN_MISMATCH,
		ISSUE_KIND_EMPTY_REQUIRED,
		ISSUE_KIND_DUPLICATE_KEY,
		ISSUE_KIND_PARSE_ERROR,
		ISSUE_KIND_INVALID_LOCALE,
		ISSUE_KIND_MAX,
	};

	static constexpr int POSITION_NONE = -1;

private:
	struct Issue {
		IssueKind kind = ISSUE_KIND_MAX;
		String message;
		StringName field;
		String path;
		int row = POSITION_NONE;
		int column = POSITION_NONE;
		int line = POSITION_NONE;
	};

	LocalVector<Issue> issues;
	int max_issues = 1000;

protected:
	static void _bind_methods();

public:
	// Limit taken from the options (1 with stop_on_first). Issues beyond it are dropped.
	void apply_options(const Ref<DataValidationOptions> &p_options);
	bool is_full() const { return max_issues >= 0 && (int)issues.size() >= max_issues; }

	void add_issue(IssueKind p_kind, const String &p_message, const StringName &p_field = StringName(), const String &p_path = String(), int p_row = POSITION_NONE, int p_column = POSITION_NONE, int p_line = POSITION_NONE);
	void set_issue_line(int p_index, int p_line);
	// Fills the line of every issue that has a path but no line by searching the keys of the path in `p_text`.
	void resolve_lines(const String &p_text);

	bool is_valid() const { return issues.is_empty(); }
	int get_issue_count() const { return issues.size(); }
	IssueKind get_issue_kind(int p_index) const;
	String get_issue_message(int p_index) const;
	StringName get_issue_field(int p_index) const;
	String get_issue_path(int p_index) const;
	int get_issue_row(int p_index) const;
	int get_issue_column(int p_index) const;
	int get_issue_line(int p_index) const;
	PackedInt32Array get_invalid_rows() const;

	// Joins a path and a key with "/", escaping "~" as "~0" and "/" as "~1" in the key.
	static String join_path(const String &p_path, const String &p_key);
};

VARIANT_ENUM_CAST(DataValidation::IssueKind);
