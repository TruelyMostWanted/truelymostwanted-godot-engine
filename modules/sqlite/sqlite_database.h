/**************************************************************************/
/*  sqlite_database.h                                                     */
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

#include "core/object/ref_counted.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"

struct sqlite3;
struct sqlite3_stmt;

class SQLiteStatement;

class SQLiteDatabase : public RefCounted {
	GDCLASS(SQLiteDatabase, RefCounted);

public:
	enum OpenMode {
		OPEN_MODE_READ_ONLY,
		OPEN_MODE_READ_WRITE,
		OPEN_MODE_READ_WRITE_CREATE,
	};

private:
	sqlite3 *db = nullptr;
	String path;
	OpenMode open_mode = OPEN_MODE_READ_WRITE_CREATE;
	Error last_error = OK;
	String last_error_message;

	Error _run(const String &p_sql, const Variant &p_params, TypedArray<Dictionary> *r_rows);

protected:
	static void _bind_methods();

public:
	// Maps an SQLite result code to an engine Error.
	static Error sqlite_result_to_error(int p_result);

	// Binds an Array (positional) or a Dictionary (named) to a prepared statement.
	// Returns OK, or an error with a message in r_message.
	static Error bind_parameters(sqlite3_stmt *p_stmt, const Variant &p_params, String &r_message);
	static Error bind_variant(sqlite3_stmt *p_stmt, int p_index, const Variant &p_value);
	static Variant column_to_variant(sqlite3_stmt *p_stmt, int p_column);

	// Records the result of an operation; used by SQLiteStatement too.
	Error set_result(int p_result);
	Error set_error(Error p_error, const String &p_message);
	sqlite3 *get_handle() const { return db; }

	Error open(const String &p_path, OpenMode p_mode = OPEN_MODE_READ_WRITE_CREATE);
	void close();
	bool is_open() const;
	String get_path() const;
	OpenMode get_open_mode() const;

	Error execute(const String &p_sql, const Variant &p_params = Variant());
	TypedArray<Dictionary> query(const String &p_sql, const Variant &p_params = Variant());
	Ref<SQLiteStatement> prepare(const String &p_sql);

	Error begin_transaction();
	Error commit();
	Error rollback();
	bool is_in_transaction() const;

	int64_t get_last_insert_rowid() const;
	int64_t get_changes() const;
	Error get_last_error() const;
	String get_last_error_message() const;

	~SQLiteDatabase();
};

VARIANT_ENUM_CAST(SQLiteDatabase::OpenMode);
