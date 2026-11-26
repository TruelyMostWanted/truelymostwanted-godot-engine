/**************************************************************************/
/*  test_sqlite.h                                                         */
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

#include "../sqlite_database.h"
#include "../sqlite_statement.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestSQLite {

[[maybe_unused]] static String _fresh_db_path(const String &p_name) {
	const String db_path = TestUtils::get_temp_path(p_name);
	if (FileAccess::exists(db_path)) {
		DirAccess::remove_absolute(db_path);
	}
	return db_path;
}

TEST_CASE("[SQLite] Open, create, execute and query") {
	const String db_path = _fresh_db_path("sqlite_basic.db");
	Ref<SQLiteDatabase> db;
	db.instantiate();

	CHECK_FALSE(db->is_open());
	CHECK(db->execute("SELECT 1;") == ERR_UNCONFIGURED);

	REQUIRE(db->open(db_path) == OK);
	CHECK(db->is_open());
	CHECK(db->get_path() == db_path);
	CHECK(db->get_open_mode() == SQLiteDatabase::OPEN_MODE_CREATE);

	CHECK(db->execute("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, weight REAL, data BLOB, flag INTEGER);") == OK);
	PackedByteArray blob;
	blob.push_back(1);
	blob.push_back(2);
	blob.push_back(255);
	CHECK(db->execute("INSERT INTO items (name, weight, data, flag) VALUES (?, ?, ?, ?);", varray("sword", 2.5, blob, true)) == OK);
	CHECK(db->get_last_insert_rowid() == 1);
	CHECK(db->get_changes() == 1);

	Dictionary named;
	named["name"] = "shield";
	named["weight"] = 4;
	CHECK(db->execute("INSERT INTO items (name, weight, data) VALUES (:name, @weight, NULL);", named) == OK);
	CHECK(db->get_last_insert_rowid() == 2);

	const TypedArray<Dictionary> rows = db->query("SELECT * FROM items ORDER BY id;");
	REQUIRE(rows.size() == 2);
	const Dictionary first = rows[0];
	CHECK(first["id"] == Variant(1));
	CHECK(first["name"] == Variant("sword"));
	CHECK(double(first["weight"]) == doctest::Approx(2.5));
	CHECK(first["data"] == Variant(blob));
	CHECK(first["flag"] == Variant(1));
	const Dictionary second = rows[1];
	CHECK(second["name"] == Variant("shield"));
	CHECK(second["data"].get_type() == Variant::NIL);

	const TypedArray<Dictionary> filtered = db->query("SELECT name FROM items WHERE weight > ?;", varray(3));
	REQUIRE(filtered.size() == 1);
	CHECK(Dictionary(filtered[0])["name"] == Variant("shield"));

	// JSON, math functions and FTS5 are compiled in.
	const TypedArray<Dictionary> extras = db->query("SELECT json_extract('{\"a\": 7}', '$.a') AS a, floor(2.7) AS f;");
	REQUIRE(extras.size() == 1);
	CHECK(Dictionary(extras[0])["a"] == Variant(7));
	CHECK(double(Dictionary(extras[0])["f"]) == doctest::Approx(2.0));
	CHECK(db->execute("CREATE VIRTUAL TABLE notes USING fts5(body); INSERT INTO notes VALUES ('the quick brown fox');") == OK);
	CHECK(db->query("SELECT * FROM notes WHERE notes MATCH 'quick';").size() == 1);

	db->close();
	CHECK_FALSE(db->is_open());

	// The data survives reopening.
	REQUIRE(db->open(db_path, SQLiteDatabase::OPEN_MODE_READ_WRITE) == OK);
	CHECK(db->query("SELECT id FROM items;").size() == 2);
	db->close();
	DirAccess::remove_absolute(db_path);
}

TEST_CASE("[SQLite] Errors") {
	const String db_path = _fresh_db_path("sqlite_errors.db");
	Ref<SQLiteDatabase> db;
	db.instantiate();

	CHECK(db->open(db_path, SQLiteDatabase::OPEN_MODE_READ_WRITE) == ERR_FILE_NOT_FOUND);
	CHECK(db->open(db_path, SQLiteDatabase::OPEN_MODE_READ_ONLY) == ERR_FILE_NOT_FOUND);
	CHECK(db->open("res://sqlite_test_never_written.db", SQLiteDatabase::OPEN_MODE_CREATE) == ERR_FILE_NO_PERMISSION);
	CHECK_FALSE(db->get_last_error_message().is_empty());

	REQUIRE(db->open(db_path) == OK);
	CHECK(db->get_last_error() == OK);
	CHECK(db->get_last_error_message().is_empty());

	CHECK(db->execute("SELEC nonsense;") == ERR_QUERY_FAILED);
	CHECK(db->get_last_error() == ERR_QUERY_FAILED);
	CHECK(db->get_last_error_message().contains("syntax error"));
	CHECK(db->query("SELECT * FROM missing_table;").is_empty());
	CHECK(db->get_last_error() == ERR_QUERY_FAILED);

	CHECK(db->execute("CREATE TABLE t (a INTEGER NOT NULL);") == OK);
	// Parameter count and type checks.
	CHECK(db->execute("INSERT INTO t VALUES (?);") == ERR_INVALID_PARAMETER);
	CHECK(db->execute("INSERT INTO t VALUES (?);", varray(1, 2)) == ERR_INVALID_PARAMETER);
	CHECK(db->execute("INSERT INTO t VALUES (?);", varray(Vector2(1, 2))) == ERR_INVALID_PARAMETER);
	CHECK(db->execute("INSERT INTO t VALUES (:a);", Dictionary()) == ERR_INVALID_PARAMETER);
	CHECK(db->execute("INSERT INTO t VALUES (?);", 5) == ERR_INVALID_PARAMETER);
	// Constraint violation.
	CHECK(db->execute("INSERT INTO t VALUES (NULL);") == ERR_QUERY_FAILED);
	CHECK(db->get_last_error_message().contains("NOT NULL"));
	db->close();

	// A file that is not a database.
	const String text_path = _fresh_db_path("sqlite_not_a_db.db");
	{
		Ref<FileAccess> f = FileAccess::open(text_path, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("This is not an SQLite database, just some text that is long enough to fill a header.");
	}
	CHECK(db->open(text_path, SQLiteDatabase::OPEN_MODE_READ_ONLY) == ERR_FILE_CORRUPT);
	CHECK_FALSE(db->is_open());

	DirAccess::remove_absolute(text_path);
	DirAccess::remove_absolute(db_path);
}

TEST_CASE("[SQLite] Read-only databases") {
	const String db_path = _fresh_db_path("sqlite_read_only.db");
	Ref<SQLiteDatabase> db;
	db.instantiate();
	REQUIRE(db->open(db_path) == OK);
	CHECK(db->execute("CREATE TABLE t (a INTEGER); INSERT INTO t VALUES (1), (2), (3);") == OK);
	db->close();

	REQUIRE(db->open(db_path, SQLiteDatabase::OPEN_MODE_READ_ONLY) == OK);
	CHECK(db->query("SELECT a FROM t;").size() == 3);
	CHECK(db->execute("INSERT INTO t VALUES (4);") == ERR_FILE_NO_PERMISSION);
	db->close();
	DirAccess::remove_absolute(db_path);
}

TEST_CASE("[SQLite] Transactions") {
	const String db_path = _fresh_db_path("sqlite_transactions.db");
	Ref<SQLiteDatabase> db;
	db.instantiate();
	REQUIRE(db->open(db_path) == OK);
	CHECK(db->execute("CREATE TABLE t (a INTEGER);") == OK);

	CHECK_FALSE(db->is_in_transaction());
	CHECK(db->begin_transaction() == OK);
	CHECK(db->is_in_transaction());
	CHECK(db->execute("INSERT INTO t VALUES (1);") == OK);
	CHECK(db->rollback() == OK);
	CHECK_FALSE(db->is_in_transaction());
	CHECK(db->query("SELECT * FROM t;").is_empty());

	CHECK(db->begin_transaction() == OK);
	for (int i = 0; i < 100; i++) {
		CHECK(db->execute("INSERT INTO t VALUES (?);", varray(i)) == OK);
	}
	CHECK(db->commit() == OK);
	CHECK(db->query("SELECT * FROM t;").size() == 100);
	CHECK(db->commit() == ERR_QUERY_FAILED); // No transaction is active.

	db->close();
	REQUIRE(db->open(db_path, SQLiteDatabase::OPEN_MODE_READ_ONLY) == OK);
	const TypedArray<Dictionary> sum = db->query("SELECT sum(a) AS s FROM t;");
	REQUIRE(sum.size() == 1);
	CHECK(Dictionary(sum[0])["s"] == Variant(4950));
	db->close();
	DirAccess::remove_absolute(db_path);
}

TEST_CASE("[SQLite] Prepared statements") {
	const String db_path = _fresh_db_path("sqlite_statements.db");
	Ref<SQLiteDatabase> db;
	db.instantiate();
	REQUIRE(db->open(db_path) == OK);
	CHECK(db->execute("CREATE TABLE t (i INTEGER, f REAL, s TEXT, b BLOB, n INTEGER);") == OK);

	CHECK(db->prepare("SELEC x;").is_null());
	CHECK(db->get_last_error() == ERR_QUERY_FAILED);
	CHECK(db->prepare("  -- only a comment").is_null());
	CHECK(db->get_last_error() == ERR_INVALID_PARAMETER);

	Ref<SQLiteStatement> insert = db->prepare("INSERT INTO t VALUES (?1, ?2, ?3, ?4, :n);");
	REQUIRE(insert.is_valid());
	CHECK(insert->get_bind_parameter_count() == 5);
	CHECK(insert->get_bind_parameter_index(":n") == 5);
	CHECK(insert->get_sql().begins_with("INSERT"));

	PackedByteArray blob;
	blob.push_back(42);
	for (int i = 0; i < 3; i++) {
		CHECK(insert->bind_int(1, i) == OK);
		CHECK(insert->bind_float(2, i * 0.5) == OK);
		CHECK(insert->bind_text(3, vformat("row %d", i)) == OK);
		CHECK(insert->bind_blob(4, blob) == OK);
		CHECK(insert->bind_null(5) == OK);
		CHECK(insert->step() == SQLiteStatement::STEP_RESULT_DONE);
		CHECK(insert->reset() == OK);
	}
	CHECK(insert->bind_int(6, 1) == ERR_INVALID_PARAMETER);
	CHECK(insert->bind_value(1, Vector3()) == ERR_INVALID_PARAMETER);
	CHECK(insert->bind_values(varray(10, 1.5, "values", PackedByteArray(), Variant())) == OK);
	CHECK(insert->step() == SQLiteStatement::STEP_RESULT_DONE);

	Ref<SQLiteStatement> select = db->prepare("SELECT i, f, s, b, n FROM t ORDER BY i;");
	REQUIRE(select.is_valid());
	CHECK(select->get_column_count() == 5);
	CHECK(select->get_column_name(2) == "s");

	CHECK(select->step() == SQLiteStatement::STEP_RESULT_ROW);
	CHECK(select->get_column_type(0) == SQLiteStatement::COLUMN_TYPE_INTEGER);
	CHECK(select->get_column_type(1) == SQLiteStatement::COLUMN_TYPE_FLOAT);
	CHECK(select->get_column_type(2) == SQLiteStatement::COLUMN_TYPE_TEXT);
	CHECK(select->get_column_type(3) == SQLiteStatement::COLUMN_TYPE_BLOB);
	CHECK(select->get_column_type(4) == SQLiteStatement::COLUMN_TYPE_NULL);
	CHECK(select->get_column_int(0) == 0);
	CHECK(select->get_column_text(2) == "row 0");
	CHECK(select->get_column_blob(3) == blob);
	CHECK(select->get_column_value(4).get_type() == Variant::NIL);

	int count = 1;
	while (select->step() == SQLiteStatement::STEP_RESULT_ROW) {
		count++;
	}
	CHECK(count == 4);
	CHECK(select->step() == SQLiteStatement::STEP_RESULT_DONE);
	CHECK(select->step() == SQLiteStatement::STEP_RESULT_DONE); // Stays done until reset().
	CHECK(select->reset() == OK);
	CHECK(select->step() == SQLiteStatement::STEP_RESULT_ROW);
	CHECK(select->get_column_int(0) == 0);

	// The last row was bound with bind_values(): an empty BLOB stays a BLOB.
	Ref<SQLiteStatement> last = db->prepare("SELECT * FROM t WHERE i = 10;");
	REQUIRE(last.is_valid());
	CHECK(last->step() == SQLiteStatement::STEP_RESULT_ROW);
	const Dictionary row = last->get_row();
	CHECK(row["s"] == Variant("values"));
	CHECK(double(row["f"]) == doctest::Approx(1.5));
	CHECK(last->get_column_type(3) == SQLiteStatement::COLUMN_TYPE_BLOB);
	CHECK(PackedByteArray(row["b"]).is_empty());

	// Statements stop working when their database is closed.
	db->close();
	ERR_PRINT_OFF;
	CHECK(select->step() == SQLiteStatement::STEP_RESULT_ERROR);
	CHECK(insert->bind_int(1, 1) == ERR_UNCONFIGURED);
	ERR_PRINT_ON;

	insert.unref();
	select.unref();
	last.unref();
	DirAccess::remove_absolute(db_path);
}

} // namespace TestSQLite
