/**************************************************************************/
/*  sqlite_database.cpp                                                   */
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

#include "sqlite_database.h"

#include "sqlite_statement.h"
#include "sqlite_vfs.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"

#include <sqlite3.h>

Error SQLiteDatabase::sqlite_result_to_error(int p_result) {
	switch (p_result & 0xff) {
		case SQLITE_OK:
		case SQLITE_ROW:
		case SQLITE_DONE:
			return OK;
		case SQLITE_CANTOPEN:
			return ERR_FILE_CANT_OPEN;
		case SQLITE_READONLY:
		case SQLITE_PERM:
		case SQLITE_AUTH:
			return ERR_FILE_NO_PERMISSION;
		case SQLITE_CORRUPT:
		case SQLITE_NOTADB:
			return ERR_FILE_CORRUPT;
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
			return ERR_BUSY;
		case SQLITE_NOMEM:
			return ERR_OUT_OF_MEMORY;
		case SQLITE_IOERR:
		case SQLITE_FULL:
			return ERR_FILE_CANT_WRITE;
		case SQLITE_RANGE:
		case SQLITE_MISUSE:
		case SQLITE_MISMATCH:
		case SQLITE_TOOBIG:
			return ERR_INVALID_PARAMETER;
		default:
			return ERR_QUERY_FAILED;
	}
}

Error SQLiteDatabase::bind_variant(sqlite3_stmt *p_stmt, int p_index, const Variant &p_value) {
	int result = SQLITE_OK;
	switch (p_value.get_type()) {
		case Variant::NIL: {
			result = sqlite3_bind_null(p_stmt, p_index);
		} break;
		case Variant::BOOL: {
			result = sqlite3_bind_int64(p_stmt, p_index, bool(p_value) ? 1 : 0);
		} break;
		case Variant::INT: {
			result = sqlite3_bind_int64(p_stmt, p_index, int64_t(p_value));
		} break;
		case Variant::FLOAT: {
			result = sqlite3_bind_double(p_stmt, p_index, double(p_value));
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			const CharString text = String(p_value).utf8();
			result = sqlite3_bind_text64(p_stmt, p_index, text.get_data(), text.length(), SQLITE_TRANSIENT, SQLITE_UTF8);
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray bytes = p_value;
			if (bytes.is_empty()) {
				// A null pointer would bind NULL; keep an empty BLOB.
				result = sqlite3_bind_zeroblob(p_stmt, p_index, 0);
			} else {
				result = sqlite3_bind_blob64(p_stmt, p_index, bytes.ptr(), bytes.size(), SQLITE_TRANSIENT);
			}
		} break;
		default: {
			return ERR_INVALID_PARAMETER;
		}
	}
	return sqlite_result_to_error(result);
}

Error SQLiteDatabase::bind_parameters(sqlite3_stmt *p_stmt, const Variant &p_params, String &r_message) {
	const int count = sqlite3_bind_parameter_count(p_stmt);
	if (count == 0) {
		return OK;
	}

	if (p_params.get_type() == Variant::ARRAY) {
		const Array values = p_params;
		if (values.size() != count) {
			r_message = vformat("The statement has %d parameters, but %d values were given.", count, values.size());
			return ERR_INVALID_PARAMETER;
		}
		for (int i = 0; i < count; i++) {
			const Error err = bind_variant(p_stmt, i + 1, values[i]);
			if (err != OK) {
				r_message = vformat("Can't bind parameter %d: unsupported type %s.", i + 1, Variant::get_type_name(values[i].get_type()));
				return err;
			}
		}
		return OK;
	}

	if (p_params.get_type() == Variant::DICTIONARY) {
		const Dictionary values = p_params;
		for (int i = 1; i <= count; i++) {
			const char *parameter_name = sqlite3_bind_parameter_name(p_stmt, i);
			if (parameter_name == nullptr || parameter_name[0] == '?') {
				r_message = vformat("Parameter %d is positional; use an Array for positional parameters.", i);
				return ERR_INVALID_PARAMETER;
			}
			// Strip the prefix (":", "@" or "$").
			const String key = String::utf8(parameter_name + 1);
			Variant value;
			if (values.has(key)) {
				value = values[key];
			} else if (values.has(StringName(key))) {
				value = values[StringName(key)];
			} else {
				r_message = vformat("No value for parameter \"%s\".", key);
				return ERR_INVALID_PARAMETER;
			}
			const Error err = bind_variant(p_stmt, i, value);
			if (err != OK) {
				r_message = vformat("Can't bind parameter \"%s\": unsupported type %s.", key, Variant::get_type_name(value.get_type()));
				return err;
			}
		}
		return OK;
	}

	if (p_params.get_type() == Variant::NIL) {
		r_message = vformat("The statement has %d parameters, but no values were given.", count);
	} else {
		r_message = "Parameters must be an Array or a Dictionary.";
	}
	return ERR_INVALID_PARAMETER;
}

Variant SQLiteDatabase::column_to_variant(sqlite3_stmt *p_stmt, int p_column) {
	switch (sqlite3_column_type(p_stmt, p_column)) {
		case SQLITE_INTEGER:
			return (int64_t)sqlite3_column_int64(p_stmt, p_column);
		case SQLITE_FLOAT:
			return sqlite3_column_double(p_stmt, p_column);
		case SQLITE_TEXT: {
			const char *text = reinterpret_cast<const char *>(sqlite3_column_text(p_stmt, p_column));
			return String::utf8(text, sqlite3_column_bytes(p_stmt, p_column));
		}
		case SQLITE_BLOB: {
			PackedByteArray bytes;
			const int size = sqlite3_column_bytes(p_stmt, p_column);
			const void *blob = sqlite3_column_blob(p_stmt, p_column);
			if (size > 0 && blob) {
				bytes.resize(size);
				memcpy(bytes.ptrw(), blob, size);
			}
			return bytes;
		}
		default:
			return Variant();
	}
}

Error SQLiteDatabase::set_result(int p_result) {
	last_error = sqlite_result_to_error(p_result);
	if (last_error == OK) {
		last_error_message = String();
	} else if (db) {
		last_error_message = String::utf8(sqlite3_errmsg(db));
	} else {
		last_error_message = String::utf8(sqlite3_errstr(p_result));
	}
	return last_error;
}

Error SQLiteDatabase::set_error(Error p_error, const String &p_message) {
	last_error = p_error;
	last_error_message = p_message;
	return last_error;
}

Error SQLiteDatabase::open(const String &p_path, OpenMode p_mode) {
	close();

	String file_path = p_path;
	OpenMode mode = p_mode;
	if (file_path.is_empty()) {
		// The file this resource was loaded from; res:// can only be read.
		file_path = get_path();
		if (file_path.is_empty()) {
			return set_error(ERR_INVALID_PARAMETER, "No path given, and the database was not loaded from a file.");
		}
		if (SQLiteVFS::is_read_only_path(file_path.utf8().get_data())) {
			mode = OPEN_MODE_READ_ONLY;
		}
	}

	const CharString utf8_path = file_path.utf8();
	if (mode != OPEN_MODE_READ_ONLY && SQLiteVFS::is_read_only_path(utf8_path.get_data())) {
		return set_error(ERR_FILE_NO_PERMISSION, "Databases in res:// are read-only. Open them with OPEN_MODE_READ_ONLY, or copy them to user:// first.");
	}
	if (mode != OPEN_MODE_CREATE && !FileAccess::exists(file_path)) {
		return set_error(ERR_FILE_NOT_FOUND, vformat("Database file not found: \"%s\".", file_path));
	}

	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (mode == OPEN_MODE_READ_ONLY) {
		flags = SQLITE_OPEN_READONLY;
	} else if (mode == OPEN_MODE_READ_WRITE) {
		flags = SQLITE_OPEN_READWRITE;
	}

	sqlite3 *handle = nullptr;
	const int result = sqlite3_open_v2(utf8_path.get_data(), &handle, flags, nullptr);
	if (result != SQLITE_OK) {
		const String message = handle ? String::utf8(sqlite3_errmsg(handle)) : String::utf8(sqlite3_errstr(result));
		sqlite3_close_v2(handle);
		return set_error(sqlite_result_to_error(result), vformat("Can't open database \"%s\": %s", file_path, message));
	}

	// SQLite opens lazily; read the header now so that a file that is not a
	// database fails here and not on the first query.
	const int check = sqlite3_exec(handle, "PRAGMA schema_version;", nullptr, nullptr, nullptr);
	if (check != SQLITE_OK) {
		const String message = String::utf8(sqlite3_errmsg(handle));
		sqlite3_close_v2(handle);
		return set_error(sqlite_result_to_error(check), vformat("Can't open database \"%s\": %s", file_path, message));
	}

#ifdef WEB_ENABLED
	if (mode != OPEN_MODE_READ_ONLY) {
		// The Web keeps the rollback journal in memory; the database file itself
		// is persisted to IndexedDB on every commit (see SQLiteVFS).
		sqlite3_exec(handle, "PRAGMA journal_mode=MEMORY;", nullptr, nullptr, nullptr);
	}
#endif

	db = handle;
	path = file_path;
	open_mode = mode;
	return set_error(OK, String());
}

void SQLiteDatabase::close() {
	if (db) {
		// close_v2 keeps the connection alive until all statements are finalized.
		sqlite3_close_v2(db);
		db = nullptr;
	}
	path = String();
}

bool SQLiteDatabase::is_open() const {
	return db != nullptr;
}

String SQLiteDatabase::get_database_path() const {
	return path;
}

SQLiteDatabase::OpenMode SQLiteDatabase::get_open_mode() const {
	return open_mode;
}

Error SQLiteDatabase::_run(const String &p_sql, const Variant &p_params, TypedArray<Dictionary> *r_rows) {
	if (!db) {
		return set_error(ERR_UNCONFIGURED, "The database is not open.");
	}

	const CharString sql = p_sql.utf8();
	const char *tail = sql.get_data();
	const char *end = tail + sql.length();

	while (tail < end) {
		sqlite3_stmt *stmt = nullptr;
		const char *next = nullptr;
		int result = sqlite3_prepare_v2(db, tail, int(end - tail), &stmt, &next);
		if (result != SQLITE_OK) {
			return set_result(result);
		}
		tail = next;
		if (stmt == nullptr) {
			// Only whitespace or a comment was left.
			continue;
		}

		String message;
		const Error bind_error = bind_parameters(stmt, p_params, message);
		if (bind_error != OK) {
			sqlite3_finalize(stmt);
			return set_error(bind_error, message);
		}

		const int column_count = sqlite3_column_count(stmt);
		Vector<String> column_names;
		if (r_rows) {
			column_names.resize(column_count);
			for (int i = 0; i < column_count; i++) {
				column_names.write[i] = String::utf8(sqlite3_column_name(stmt, i));
			}
		}

		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			if (r_rows) {
				Dictionary row;
				for (int i = 0; i < column_count; i++) {
					row[column_names[i]] = column_to_variant(stmt, i);
				}
				r_rows->push_back(row);
			}
		}

		if (result != SQLITE_DONE) {
			const Error err = set_result(result);
			sqlite3_finalize(stmt);
			return err;
		}
		sqlite3_finalize(stmt);
	}

	return set_error(OK, String());
}

Error SQLiteDatabase::execute(const String &p_sql, const Variant &p_params) {
	return _run(p_sql, p_params, nullptr);
}

TypedArray<Dictionary> SQLiteDatabase::query(const String &p_sql, const Variant &p_params) {
	TypedArray<Dictionary> rows;
	if (_run(p_sql, p_params, &rows) != OK) {
		return TypedArray<Dictionary>();
	}
	return rows;
}

Ref<SQLiteStatement> SQLiteDatabase::prepare(const String &p_sql) {
	if (!db) {
		set_error(ERR_UNCONFIGURED, "The database is not open.");
		return Ref<SQLiteStatement>();
	}

	const CharString sql = p_sql.utf8();
	sqlite3_stmt *stmt = nullptr;
	const int result = sqlite3_prepare_v3(db, sql.get_data(), sql.length(), SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
	if (result != SQLITE_OK) {
		set_result(result);
		return Ref<SQLiteStatement>();
	}
	if (stmt == nullptr) {
		set_error(ERR_INVALID_PARAMETER, "The SQL text contains no statement.");
		return Ref<SQLiteStatement>();
	}

	Ref<SQLiteStatement> statement;
	statement.instantiate();
	statement->setup(Ref<SQLiteDatabase>(this), stmt);
	set_error(OK, String());
	return statement;
}

Error SQLiteDatabase::begin_transaction() {
	return execute("BEGIN;");
}

Error SQLiteDatabase::commit() {
	return execute("COMMIT;");
}

Error SQLiteDatabase::rollback() {
	return execute("ROLLBACK;");
}

bool SQLiteDatabase::is_in_transaction() const {
	return db && sqlite3_get_autocommit(db) == 0;
}

// User tables and virtual tables of the main database, without SQLite's own tables and the shadow tables of virtual tables.
static const char *TABLE_LIST_SQL = "SELECT name FROM pragma_table_list WHERE schema = 'main' AND type IN ('table', 'virtual') AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\'";

PackedStringArray SQLiteDatabase::get_table_names() {
	PackedStringArray names;
	const TypedArray<Dictionary> rows = query(String(TABLE_LIST_SQL) + " ORDER BY name;");
	for (int i = 0; i < rows.size(); i++) {
		names.push_back(String(Dictionary(rows[i])["name"]));
	}
	return names;
}

bool SQLiteDatabase::has_table(const String &p_table) {
	return !query(String(TABLE_LIST_SQL) + " AND name = ? COLLATE NOCASE;", varray(p_table)).is_empty();
}

bool SQLiteDatabase::_check_table(const String &p_table) {
	if (has_table(p_table)) {
		return true;
	}
	if (last_error == OK) {
		set_error(ERR_DOES_NOT_EXIST, vformat("No table \"%s\" in the database.", p_table));
	}
	return false;
}

TypedArray<SQLiteColumn> SQLiteDatabase::get_columns(const String &p_table) {
	TypedArray<SQLiteColumn> columns;
	if (!_check_table(p_table)) {
		return columns;
	}
	// A table-valued pragma takes the name as a bound parameter: no quoting needed.
	const TypedArray<Dictionary> rows = query("SELECT name, type, \"notnull\", dflt_value, pk FROM pragma_table_info(?) ORDER BY cid;", varray(p_table));
	for (int i = 0; i < rows.size(); i++) {
		const Dictionary row = rows[i];
		Ref<SQLiteColumn> column;
		column.instantiate();
		column->name = String(row["name"]);
		column->declared_type = String(row["type"]);
		column->not_null = int64_t(row["notnull"]) != 0;
		column->default_value = row["dflt_value"].get_type() == Variant::NIL ? String() : String(row["dflt_value"]);
		column->primary_key_index = int(row["pk"]);
		columns.push_back(column);
	}
	return columns;
}

PackedStringArray SQLiteDatabase::get_column_names(const String &p_table) {
	PackedStringArray names;
	const TypedArray<SQLiteColumn> columns = get_columns(p_table);
	for (int i = 0; i < columns.size(); i++) {
		names.push_back(Ref<SQLiteColumn>(columns[i])->get_name());
	}
	return names;
}

int SQLiteDatabase::get_column_count(const String &p_table) {
	const TypedArray<SQLiteColumn> columns = get_columns(p_table);
	return last_error == OK ? columns.size() : -1;
}

int64_t SQLiteDatabase::get_row_count(const String &p_table) {
	if (!_check_table(p_table)) {
		return -1;
	}
	// The name is quoted as an identifier; COUNT(*) reads the whole table.
	const TypedArray<Dictionary> rows = query("SELECT COUNT(*) AS count FROM \"" + p_table.replace("\"", "\"\"") + "\";");
	if (rows.is_empty()) {
		return -1;
	}
	return int64_t(Dictionary(rows[0])["count"]);
}

int64_t SQLiteDatabase::get_last_insert_rowid() const {
	return db ? (int64_t)sqlite3_last_insert_rowid(db) : 0;
}

int64_t SQLiteDatabase::get_changes() const {
	return db ? (int64_t)sqlite3_changes64(db) : 0;
}

Error SQLiteDatabase::get_last_error() const {
	return last_error;
}

String SQLiteDatabase::get_last_error_message() const {
	return last_error_message;
}

void SQLiteDatabase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "mode"), &SQLiteDatabase::open, DEFVAL(String()), DEFVAL(OPEN_MODE_CREATE));
	ClassDB::bind_method(D_METHOD("close"), &SQLiteDatabase::close);
	ClassDB::bind_method(D_METHOD("is_open"), &SQLiteDatabase::is_open);
	ClassDB::bind_method(D_METHOD("get_database_path"), &SQLiteDatabase::get_database_path);
	ClassDB::bind_method(D_METHOD("get_open_mode"), &SQLiteDatabase::get_open_mode);

	ClassDB::bind_method(D_METHOD("execute", "sql", "params"), &SQLiteDatabase::execute, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("query", "sql", "params"), &SQLiteDatabase::query, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("prepare", "sql"), &SQLiteDatabase::prepare);

	ClassDB::bind_method(D_METHOD("begin_transaction"), &SQLiteDatabase::begin_transaction);
	ClassDB::bind_method(D_METHOD("commit"), &SQLiteDatabase::commit);
	ClassDB::bind_method(D_METHOD("rollback"), &SQLiteDatabase::rollback);
	ClassDB::bind_method(D_METHOD("is_in_transaction"), &SQLiteDatabase::is_in_transaction);

	ClassDB::bind_method(D_METHOD("get_table_names"), &SQLiteDatabase::get_table_names);
	ClassDB::bind_method(D_METHOD("has_table", "table"), &SQLiteDatabase::has_table);
	ClassDB::bind_method(D_METHOD("get_columns", "table"), &SQLiteDatabase::get_columns);
	ClassDB::bind_method(D_METHOD("get_column_names", "table"), &SQLiteDatabase::get_column_names);
	ClassDB::bind_method(D_METHOD("get_column_count", "table"), &SQLiteDatabase::get_column_count);
	ClassDB::bind_method(D_METHOD("get_row_count", "table"), &SQLiteDatabase::get_row_count);

	ClassDB::bind_method(D_METHOD("get_last_insert_rowid"), &SQLiteDatabase::get_last_insert_rowid);
	ClassDB::bind_method(D_METHOD("get_changes"), &SQLiteDatabase::get_changes);
	ClassDB::bind_method(D_METHOD("get_last_error"), &SQLiteDatabase::get_last_error);
	ClassDB::bind_method(D_METHOD("get_last_error_message"), &SQLiteDatabase::get_last_error_message);

	BIND_ENUM_CONSTANT(OPEN_MODE_READ_ONLY);
	BIND_ENUM_CONSTANT(OPEN_MODE_READ_WRITE);
	BIND_ENUM_CONSTANT(OPEN_MODE_CREATE);
}

SQLiteDatabase::~SQLiteDatabase() {
	close();
}

// ResourceFormatLoaderSQLite

Ref<Resource> ResourceFormatLoaderSQLite::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	// `open()` opens the file, not the loader: loading is instant and does not hold the file.
	if (!FileAccess::exists(p_path)) {
		if (r_error) {
			*r_error = ERR_FILE_NOT_FOUND;
		}
		return Ref<Resource>();
	}
	if (r_error) {
		*r_error = OK;
	}
	Ref<SQLiteDatabase> database;
	database.instantiate();
	return database;
}

void ResourceFormatLoaderSQLite::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("db");
	p_extensions->push_back("sqlite");
	p_extensions->push_back("sqlite3");
	p_extensions->push_back("db3");
}

bool ResourceFormatLoaderSQLite::handles_type(const String &p_type) const {
	return p_type == "SQLiteDatabase";
}

String ResourceFormatLoaderSQLite::get_resource_type(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (extension == "db" || extension == "sqlite" || extension == "sqlite3" || extension == "db3") {
		return "SQLiteDatabase";
	}
	return String();
}
