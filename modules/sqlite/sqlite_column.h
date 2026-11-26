/**************************************************************************/
/*  sqlite_column.h                                                       */
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

// One column of a table, as `SQLiteDatabase::get_columns()` returns it. Read-only.
class SQLiteColumn : public RefCounted {
	GDCLASS(SQLiteColumn, RefCounted);

public:
	enum Affinity {
		AFFINITY_INTEGER,
		AFFINITY_REAL,
		AFFINITY_TEXT,
		AFFINITY_BLOB,
		AFFINITY_NUMERIC,
	};

private:
	friend class SQLiteDatabase;

	String name;
	String declared_type;
	bool not_null = false;
	String default_value;
	int primary_key_index = 0;

protected:
	static void _bind_methods();

public:
	// The affinity SQLite gives a declared type (https://sqlite.org/datatype3.html, section 3.1).
	static Affinity get_affinity_for_type(const String &p_declared_type);

	String get_name() const { return name; }
	String get_declared_type() const { return declared_type; }
	Affinity get_affinity() const { return get_affinity_for_type(declared_type); }
	bool is_not_null() const { return not_null; }
	String get_default_value() const { return default_value; }
	int get_primary_key_index() const { return primary_key_index; }
};

VARIANT_ENUM_CAST(SQLiteColumn::Affinity);
