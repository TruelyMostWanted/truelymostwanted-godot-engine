/**************************************************************************/
/*  sqlite_statement.cpp                                                  */
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

#include "sqlite_statement.h"

#include "core/object/class_db.h"

#include <sqlite3.h>

#include <cstring>

bool SQLiteStatement::_is_valid() const {
	// A statement is unusable once its database was closed (or reopened).
	return stmt && database.is_valid() && database->get_handle() == db;
}

Error SQLiteStatement::_bind_result(int p_result) {
	return database->set_result(p_result);
}

void SQLiteStatement::setup(const Ref<SQLiteDatabase> &p_database, sqlite3_stmt *p_stmt) {
	database = p_database;
	db = p_database.is_valid() ? p_database->get_handle() : nullptr;
	stmt = p_stmt;
	if (database.is_valid() && stmt) {
		database->register_statement(this);
	}
}

void SQLiteStatement::finalize() {
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = nullptr;
	}
}

String SQLiteStatement::get_sql() const {
	ERR_FAIL_NULL_V(stmt, String());
	return String::utf8(sqlite3_sql(stmt));
}

int SQLiteStatement::get_bind_parameter_count() const {
	ERR_FAIL_NULL_V(stmt, 0);
	return sqlite3_bind_parameter_count(stmt);
}

int SQLiteStatement::get_bind_parameter_index(const String &p_name) const {
	ERR_FAIL_NULL_V(stmt, 0);
	return sqlite3_bind_parameter_index(stmt, p_name.utf8().get_data());
}

Error SQLiteStatement::bind_null(int p_index) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	return _bind_result(sqlite3_bind_null(stmt, p_index));
}

Error SQLiteStatement::bind_int(int p_index, int64_t p_value) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	return _bind_result(sqlite3_bind_int64(stmt, p_index, p_value));
}

Error SQLiteStatement::bind_float(int p_index, double p_value) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	return _bind_result(sqlite3_bind_double(stmt, p_index, p_value));
}

Error SQLiteStatement::bind_text(int p_index, const String &p_value) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	const CharString text = p_value.utf8();
	return _bind_result(sqlite3_bind_text64(stmt, p_index, text.get_data(), text.length(), SQLITE_TRANSIENT, SQLITE_UTF8));
}

Error SQLiteStatement::bind_blob(int p_index, const PackedByteArray &p_value) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	if (p_value.is_empty()) {
		return _bind_result(sqlite3_bind_zeroblob(stmt, p_index, 0));
	}
	return _bind_result(sqlite3_bind_blob64(stmt, p_index, p_value.ptr(), p_value.size(), SQLITE_TRANSIENT));
}

Error SQLiteStatement::bind_value(int p_index, const Variant &p_value) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	const Error err = SQLiteDatabase::bind_variant(stmt, p_index, p_value);
	if (err != OK) {
		if (p_index < 1 || p_index > sqlite3_bind_parameter_count(stmt)) {
			return database->set_error(err, vformat("Parameter index %d is out of range.", p_index));
		}
		return database->set_error(err, vformat("Can't bind parameter %d: unsupported type %s.", p_index, Variant::get_type_name(p_value.get_type())));
	}
	return database->set_error(OK, String());
}

Error SQLiteStatement::bind_values(const Variant &p_params) {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	String message;
	const Error err = SQLiteDatabase::bind_parameters(stmt, p_params, message);
	return database->set_error(err, message);
}

Error SQLiteStatement::clear_bindings() {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	return _bind_result(sqlite3_clear_bindings(stmt));
}

SQLiteStatement::StepResult SQLiteStatement::step() {
	ERR_FAIL_COND_V_MSG(!_is_valid(), STEP_RESULT_ERROR, "The statement's database is closed.");
	if (finished) {
		// SQLite would restart the statement on its own; reset() makes that explicit.
		return STEP_RESULT_DONE;
	}
	const int result = sqlite3_step(stmt);
	database->set_result(result);
	if (result == SQLITE_ROW) {
		return STEP_RESULT_ROW;
	}
	if (result == SQLITE_DONE) {
		finished = true;
		return STEP_RESULT_DONE;
	}
	return STEP_RESULT_ERROR;
}

Error SQLiteStatement::reset() {
	ERR_FAIL_COND_V_MSG(!_is_valid(), ERR_UNCONFIGURED, "The statement's database is closed.");
	// sqlite3_reset() repeats the error of the last step; the reset itself succeeds.
	sqlite3_reset(stmt);
	finished = false;
	return database->set_error(OK, String());
}

int SQLiteStatement::get_column_count() const {
	ERR_FAIL_NULL_V(stmt, 0);
	return sqlite3_column_count(stmt);
}

String SQLiteStatement::get_column_name(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), String());
	return String::utf8(sqlite3_column_name(stmt, p_column));
}

SQLiteStatement::ColumnType SQLiteStatement::get_column_type(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), COLUMN_TYPE_NULL);
	switch (sqlite3_column_type(stmt, p_column)) {
		case SQLITE_INTEGER:
			return COLUMN_TYPE_INTEGER;
		case SQLITE_FLOAT:
			return COLUMN_TYPE_FLOAT;
		case SQLITE_TEXT:
			return COLUMN_TYPE_TEXT;
		case SQLITE_BLOB:
			return COLUMN_TYPE_BLOB;
		default:
			return COLUMN_TYPE_NULL;
	}
}

int64_t SQLiteStatement::get_column_int(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), 0);
	return sqlite3_column_int64(stmt, p_column);
}

double SQLiteStatement::get_column_float(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), 0.0);
	return sqlite3_column_double(stmt, p_column);
}

String SQLiteStatement::get_column_text(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), String());
	const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, p_column));
	if (text == nullptr) {
		return String();
	}
	return String::utf8(text, sqlite3_column_bytes(stmt, p_column));
}

PackedByteArray SQLiteStatement::get_column_blob(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), PackedByteArray());
	PackedByteArray bytes;
	const void *blob = sqlite3_column_blob(stmt, p_column);
	const int size = sqlite3_column_bytes(stmt, p_column);
	if (blob && size > 0) {
		bytes.resize(size);
		memcpy(bytes.ptrw(), blob, size);
	}
	return bytes;
}

Variant SQLiteStatement::get_column_value(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, get_column_count(), Variant());
	return SQLiteDatabase::column_to_variant(stmt, p_column);
}

Dictionary SQLiteStatement::get_row() const {
	Dictionary row;
	const int count = get_column_count();
	for (int i = 0; i < count; i++) {
		row[String::utf8(sqlite3_column_name(stmt, i))] = SQLiteDatabase::column_to_variant(stmt, i);
	}
	return row;
}

void SQLiteStatement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_sql"), &SQLiteStatement::get_sql);
	ClassDB::bind_method(D_METHOD("get_bind_parameter_count"), &SQLiteStatement::get_bind_parameter_count);
	ClassDB::bind_method(D_METHOD("get_bind_parameter_index", "name"), &SQLiteStatement::get_bind_parameter_index);

	ClassDB::bind_method(D_METHOD("bind_null", "index"), &SQLiteStatement::bind_null);
	ClassDB::bind_method(D_METHOD("bind_int", "index", "value"), &SQLiteStatement::bind_int);
	ClassDB::bind_method(D_METHOD("bind_float", "index", "value"), &SQLiteStatement::bind_float);
	ClassDB::bind_method(D_METHOD("bind_text", "index", "value"), &SQLiteStatement::bind_text);
	ClassDB::bind_method(D_METHOD("bind_blob", "index", "value"), &SQLiteStatement::bind_blob);
	ClassDB::bind_method(D_METHOD("bind_value", "index", "value"), &SQLiteStatement::bind_value);
	ClassDB::bind_method(D_METHOD("bind_values", "params"), &SQLiteStatement::bind_values);
	ClassDB::bind_method(D_METHOD("clear_bindings"), &SQLiteStatement::clear_bindings);

	ClassDB::bind_method(D_METHOD("step"), &SQLiteStatement::step);
	ClassDB::bind_method(D_METHOD("reset"), &SQLiteStatement::reset);

	ClassDB::bind_method(D_METHOD("get_column_count"), &SQLiteStatement::get_column_count);
	ClassDB::bind_method(D_METHOD("get_column_name", "column"), &SQLiteStatement::get_column_name);
	ClassDB::bind_method(D_METHOD("get_column_type", "column"), &SQLiteStatement::get_column_type);
	ClassDB::bind_method(D_METHOD("get_column_int", "column"), &SQLiteStatement::get_column_int);
	ClassDB::bind_method(D_METHOD("get_column_float", "column"), &SQLiteStatement::get_column_float);
	ClassDB::bind_method(D_METHOD("get_column_text", "column"), &SQLiteStatement::get_column_text);
	ClassDB::bind_method(D_METHOD("get_column_blob", "column"), &SQLiteStatement::get_column_blob);
	ClassDB::bind_method(D_METHOD("get_column_value", "column"), &SQLiteStatement::get_column_value);
	ClassDB::bind_method(D_METHOD("get_row"), &SQLiteStatement::get_row);

	BIND_ENUM_CONSTANT(STEP_RESULT_ROW);
	BIND_ENUM_CONSTANT(STEP_RESULT_DONE);
	BIND_ENUM_CONSTANT(STEP_RESULT_ERROR);

	BIND_ENUM_CONSTANT(COLUMN_TYPE_INTEGER);
	BIND_ENUM_CONSTANT(COLUMN_TYPE_FLOAT);
	BIND_ENUM_CONSTANT(COLUMN_TYPE_TEXT);
	BIND_ENUM_CONSTANT(COLUMN_TYPE_BLOB);
	BIND_ENUM_CONSTANT(COLUMN_TYPE_NULL);
}

SQLiteStatement::~SQLiteStatement() {
	if (stmt) {
		database->unregister_statement(this);
		finalize();
	}
}
