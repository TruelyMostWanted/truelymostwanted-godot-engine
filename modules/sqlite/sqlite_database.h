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

#include "sqlite_column.h"

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"

struct sqlite3;
struct sqlite3_stmt;

class SQLiteStatement;

class SQLiteDatabase : public Resource {
	GDCLASS(SQLiteDatabase, Resource);

public:
	enum OpenMode {
		OPEN_MODE_READ_ONLY,
		OPEN_MODE_READ_WRITE,
		OPEN_MODE_CREATE,
	};

private:
	sqlite3 *db = nullptr;
	String path;
	OpenMode open_mode = OPEN_MODE_CREATE;
	Error last_error = OK;
	String last_error_message;

	Error _run(const String &p_sql, const Variant &p_params, TypedArray<Dictionary> *r_rows);
	bool _check_table(const String &p_table);

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

	Error open(const String &p_path = String(), OpenMode p_mode = OPEN_MODE_CREATE);
	void close();
	bool is_open() const;
	String get_database_path() const;
	OpenMode get_open_mode() const;

	Error execute(const String &p_sql, const Variant &p_params = Variant());
	TypedArray<Dictionary> query(const String &p_sql, const Variant &p_params = Variant());
	Ref<SQLiteStatement> prepare(const String &p_sql);

	Error begin_transaction();
	Error commit();
	Error rollback();
	bool is_in_transaction() const;

	PackedStringArray get_table_names();
	bool has_table(const String &p_table);
	TypedArray<SQLiteColumn> get_columns(const String &p_table);
	PackedStringArray get_column_names(const String &p_table);
	int get_column_count(const String &p_table);
	int64_t get_row_count(const String &p_table);

	int64_t get_last_insert_rowid() const;
	int64_t get_changes() const;
	Error get_last_error() const;
	String get_last_error_message() const;

	~SQLiteDatabase();
};

VARIANT_ENUM_CAST(SQLiteDatabase::OpenMode);

// Loads `.db`, `.sqlite`, `.sqlite3` and `.db3` files as an unopened SQLiteDatabase; `open()` opens them.
class ResourceFormatLoaderSQLite : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderSQLite, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

	// Database files are data, like JSON files: no `*.uid` file next to them.
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override { return ResourceUID::INVALID_ID; }
	virtual bool has_custom_uid_support() const override { return true; }
};
