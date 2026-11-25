/**************************************************************************/
/*  sqlite_statement.h                                                    */
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

#include "sqlite_database.h"

#include "core/object/ref_counted.h"
#include "core/variant/binder_common.h"

struct sqlite3;
struct sqlite3_stmt;

class SQLiteStatement : public RefCounted {
	GDCLASS(SQLiteStatement, RefCounted);

public:
	enum StepResult {
		STEP_RESULT_ROW,
		STEP_RESULT_DONE,
		STEP_RESULT_ERROR,
	};

	enum ColumnType {
		COLUMN_TYPE_INTEGER,
		COLUMN_TYPE_FLOAT,
		COLUMN_TYPE_TEXT,
		COLUMN_TYPE_BLOB,
		COLUMN_TYPE_NULL,
	};

private:
	Ref<SQLiteDatabase> database;
	sqlite3 *db = nullptr; // The connection the statement belongs to.
	sqlite3_stmt *stmt = nullptr;

	bool _is_valid() const;
	Error _bind_result(int p_result);

protected:
	static void _bind_methods();

public:
	// Takes ownership of p_stmt. Called by SQLiteDatabase::prepare().
	void setup(const Ref<SQLiteDatabase> &p_database, sqlite3_stmt *p_stmt);

	String get_sql() const;
	int get_bind_parameter_count() const;
	int get_bind_parameter_index(const String &p_name) const;

	Error bind_null(int p_index);
	Error bind_int(int p_index, int64_t p_value);
	Error bind_float(int p_index, double p_value);
	Error bind_text(int p_index, const String &p_value);
	Error bind_blob(int p_index, const PackedByteArray &p_value);
	Error bind_value(int p_index, const Variant &p_value);
	Error bind_values(const Variant &p_params);
	Error clear_bindings();

	StepResult step();
	Error reset();

	int get_column_count() const;
	String get_column_name(int p_column) const;
	ColumnType get_column_type(int p_column) const;
	int64_t get_column_int(int p_column) const;
	double get_column_float(int p_column) const;
	String get_column_text(int p_column) const;
	PackedByteArray get_column_blob(int p_column) const;
	Variant get_column_value(int p_column) const;
	Dictionary get_row() const;

	~SQLiteStatement();
};

VARIANT_ENUM_CAST(SQLiteStatement::StepResult);
VARIANT_ENUM_CAST(SQLiteStatement::ColumnType);
