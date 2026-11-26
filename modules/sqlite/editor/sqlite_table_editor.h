/**************************************************************************/
/*  sqlite_table_editor.h                                                 */
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

#include "../sqlite_column.h"
#include "../sqlite_database.h"

#include "scene/gui/dialogs.h"

class Button;
class Label;
class LineEdit;
class Tree;
class UndoRedo;

// Edits the rows of one table, one page at a time. Edits stay pending until Save.
class SQLiteTableEditor : public AcceptDialog {
	GDCLASS(SQLiteTableEditor, AcceptDialog);

	static SQLiteTableEditor *singleton;

	String path; // Globalized: the module's file system treats res:// as read-only.
	String table;
	Vector<Ref<SQLiteColumn>> columns;
	Vector<String> key_columns; // The primary key columns, or "rowid".
	Vector<Array> rows; // The current page: key values, then column values.
	Vector<Variant> row_ids; // Per tree item: the key Array, or the index in inserts.
	int page = 0;
	int page_count = 1;
	int sort_column = -1;
	bool sort_descending = false;

	// Pending edits, in the format of apply_changes().
	Dictionary updates;
	Dictionary deletes;
	Array inserts;
	Variant failed_row; // The key Array or insert index of the row that failed to save.
	UndoRedo *undo_redo = nullptr;

	LineEdit *filter_edit = nullptr;
	Button *undo_button = nullptr;
	Button *redo_button = nullptr;
	Button *previous_button = nullptr;
	Button *next_button = nullptr;
	Label *page_label = nullptr;
	Label *status_label = nullptr;
	Tree *tree = nullptr;

	Ref<SQLiteDatabase> _open(SQLiteDatabase::OpenMode p_mode);
	void _load_page();
	void _render();
	Array _get_changes() const;
	void _set_changes(const Array &p_changes);
	void _commit(const String &p_action, const Array &p_changes);
	void _set_cell(const Variant &p_row, int p_column, const Variant &p_value);

	void _cell_edited();
	void _add_row();
	void _delete_row();
	void _set_null();
	void _save();
	void _revert();
	void _filter_changed(const String &p_text);
	void _change_page(int p_delta);
	void _sort(int p_column, int p_mouse_button);

public:
	static SQLiteTableEditor *get_singleton() { return singleton; }

	// Writes pending edits in one transaction. p_changes is [updates, deletes, inserts]:
	// - updates: key Array -> Dictionary of column name -> value,
	// - deletes: key Array -> true,
	// - inserts: Array of Dictionary of column name -> value (missing columns get their default).
	// A key Array holds the values of p_key_columns. On an error everything is rolled back;
	// r_failed_row is then the key or insert index of the failing row, and r_message is SQLite's message.
	static Error apply_changes(const Ref<SQLiteDatabase> &p_database, const String &p_table, const Vector<String> &p_key_columns, const Array &p_changes, Variant &r_failed_row, String &r_message);

	void edit(const String &p_path, const String &p_table);

	SQLiteTableEditor();
	~SQLiteTableEditor();
};
