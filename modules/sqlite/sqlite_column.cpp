/**************************************************************************/
/*  sqlite_column.cpp                                                     */
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

#include "sqlite_column.h"

#include "core/object/class_db.h"

SQLiteColumn::Affinity SQLiteColumn::get_affinity_for_type(const String &p_declared_type) {
	const String type = p_declared_type.to_upper();
	if (type.contains("INT")) {
		return AFFINITY_INTEGER;
	}
	if (type.contains("CHAR") || type.contains("CLOB") || type.contains("TEXT")) {
		return AFFINITY_TEXT;
	}
	if (type.is_empty() || type.contains("BLOB")) {
		return AFFINITY_BLOB;
	}
	if (type.contains("REAL") || type.contains("FLOA") || type.contains("DOUB")) { // codespell:ignore
		return AFFINITY_REAL;
	}
	return AFFINITY_NUMERIC;
}

void SQLiteColumn::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &SQLiteColumn::get_name);
	ClassDB::bind_method(D_METHOD("get_declared_type"), &SQLiteColumn::get_declared_type);
	ClassDB::bind_method(D_METHOD("get_affinity"), &SQLiteColumn::get_affinity);
	ClassDB::bind_method(D_METHOD("is_not_null"), &SQLiteColumn::is_not_null);
	ClassDB::bind_method(D_METHOD("get_default_value"), &SQLiteColumn::get_default_value);
	ClassDB::bind_method(D_METHOD("get_primary_key_index"), &SQLiteColumn::get_primary_key_index);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "declared_type"), "", "get_declared_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "affinity", PROPERTY_HINT_ENUM, "Integer,Real,Text,Blob,Numeric"), "", "get_affinity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "not_null"), "", "is_not_null");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_value"), "", "get_default_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "primary_key_index"), "", "get_primary_key_index");

	BIND_ENUM_CONSTANT(AFFINITY_INTEGER);
	BIND_ENUM_CONSTANT(AFFINITY_REAL);
	BIND_ENUM_CONSTANT(AFFINITY_TEXT);
	BIND_ENUM_CONSTANT(AFFINITY_BLOB);
	BIND_ENUM_CONSTANT(AFFINITY_NUMERIC);
}
