/**************************************************************************/
/*  sqlite_table_editor.cpp                                               */
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

#include "sqlite_table_editor.h"

#include "../sqlite_statement.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/undo_redo.h"
#include "editor/editor_string_names.h"
#include "editor/run/editor_run_bar.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"
#include "scene/scene_string_names.h"

static constexpr int PAGE_SIZE = 200;

SQLiteTableEditor *SQLiteTableEditor::singleton = nullptr;

// Identifiers are quoted; values are always bound parameters.
static String _quote(const String &p_identifier) {
	return "\"" + p_identifier.replace("\"", "\"\"") + "\"";
}

static String _cell_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return "NULL";
		case Variant::PACKED_BYTE_ARRAY:
			return vformat("[BLOB: %d bytes]", PackedByteArray(p_value).size());
		default:
			return p_value;
	}
}

// Typed by the column affinity; anything else is stored as text, as SQLite does.
static Variant _parse_cell(const String &p_text, SQLiteColumn::Affinity p_affinity) {
	switch (p_affinity) {
		case SQLiteColumn::AFFINITY_INTEGER:
		case SQLiteColumn::AFFINITY_NUMERIC:
			if (p_text.is_valid_int()) {
				return p_text.to_int();
			}
			[[fallthrough]];
		case SQLiteColumn::AFFINITY_REAL:
			if (p_text.is_valid_float()) {
				return p_text.to_float();
			}
			break;
		default:
			break;
	}
	return p_text;
}

Error SQLiteTableEditor::apply_changes(const Ref<SQLiteDatabase> &p_database, const String &p_table, const Vector<String> &p_key_columns, const Array &p_changes, Variant &r_failed_row, String &r_message) {
	ERR_FAIL_COND_V(p_database.is_null() || p_key_columns.is_empty() || p_changes.size() != 3, ERR_INVALID_PARAMETER);
	const Dictionary changed_rows = p_changes[0];
	const Dictionary deleted_rows = p_changes[1];
	const Array new_rows = p_changes[2];
	r_failed_row = Variant();
	r_message = String();

	const String table_sql = _quote(p_table);
	String where;
	for (const String &key : p_key_columns) {
		// IS also matches NULL keys, which rowid tables allow in the primary key.
		where += (where.is_empty() ? "" : " AND ") + _quote(key) + " IS ?";
	}

	Error err = p_database->begin_transaction();
	for (const KeyValue<Variant, Variant> &row : deleted_rows) {
		if (err != OK) {
			break;
		}
		err = p_database->execute("DELETE FROM " + table_sql + " WHERE " + where + ";", row.key);
		r_failed_row = row.key;
	}
	for (const KeyValue<Variant, Variant> &row : changed_rows) {
		const Dictionary values = row.value;
		if (err != OK) {
			break;
		}
		if (deleted_rows.has(row.key) || values.is_empty()) {
			continue;
		}
		String assignments;
		Array params;
		for (const KeyValue<Variant, Variant> &value : values) {
			assignments += (assignments.is_empty() ? "" : ", ") + _quote(value.key) + " = ?";
			params.push_back(value.value);
		}
		params.append_array(row.key);
		err = p_database->execute("UPDATE " + table_sql + " SET " + assignments + " WHERE " + where + ";", params);
		r_failed_row = row.key;
	}
	for (int i = 0; err == OK && i < new_rows.size(); i++) {
		const Dictionary values = new_rows[i];
		String names;
		String marks;
		Array params;
		for (const KeyValue<Variant, Variant> &value : values) {
			names += (names.is_empty() ? "" : ", ") + _quote(value.key);
			marks += marks.is_empty() ? "?" : ", ?";
			params.push_back(value.value);
		}
		const String sql = values.is_empty() ? "INSERT INTO " + table_sql + " DEFAULT VALUES;" : vformat("INSERT INTO %s (%s) VALUES (%s);", table_sql, names, marks);
		err = p_database->execute(sql, params);
		r_failed_row = i;
	}
	if (err == OK) {
		err = p_database->commit();
		r_failed_row = Variant();
	}
	if (err != OK) {
		r_message = p_database->get_last_error_message();
		p_database->rollback();
		return err;
	}
	r_failed_row = Variant();
	return OK;
}

Ref<SQLiteDatabase> SQLiteTableEditor::_open(SQLiteDatabase::OpenMode p_mode) {
	Ref<SQLiteDatabase> database;
	database.instantiate();
	if (database->open(path, p_mode) != OK) {
		status_label->set_text(database->get_last_error_message());
		return Ref<SQLiteDatabase>();
	}
	return database;
}

void SQLiteTableEditor::_load_page() {
	rows.clear();
	Ref<SQLiteDatabase> database = columns.is_empty() ? Ref<SQLiteDatabase>() : _open(SQLiteDatabase::OPEN_MODE_READ_ONLY);
	if (database.is_valid()) {
		String where;
		Array params;
		if (!filter_edit->get_text().is_empty()) {
			for (const Ref<SQLiteColumn> &column : columns) {
				where += (where.is_empty() ? " WHERE " : " OR ") + _quote(column->get_name()) + " LIKE ?1";
			}
			params.push_back("%" + filter_edit->get_text() + "%");
		}
		const TypedArray<Dictionary> count = database->query("SELECT COUNT(*) AS n FROM " + _quote(table) + where + ";", params);
		const int64_t total = count.is_empty() ? 0 : int64_t(Dictionary(count[0])["n"]);
		page_count = MAX(1, int((total + PAGE_SIZE - 1) / PAGE_SIZE));
		page = CLAMP(page, 0, page_count - 1);
		page_label->set_text(vformat(TTR("Page %d of %d, %d rows"), page + 1, page_count, total));

		String select;
		for (const String &key : key_columns) {
			select += _quote(key) + ", ";
		}
		String order;
		if (sort_column >= 0) {
			order = " ORDER BY " + _quote(columns[sort_column]->get_name()) + (sort_descending ? " DESC" : " ASC");
		}
		const int limit_index = params.size() + 1;
		params.push_back(PAGE_SIZE);
		params.push_back(int64_t(page) * PAGE_SIZE);
		Ref<SQLiteStatement> statement = database->prepare(vformat("SELECT %s* FROM %s%s%s LIMIT ?%d OFFSET ?%d;", select, _quote(table), where, order, limit_index, limit_index + 1));
		if (statement.is_valid() && statement->bind_values(params) == OK) {
			while (statement->step() == SQLiteStatement::STEP_RESULT_ROW) {
				Array row;
				for (int i = 0; i < statement->get_column_count(); i++) {
					row.push_back(statement->get_column_value(i));
				}
				rows.push_back(row);
			}
		}
		if (database->get_last_error() != OK) {
			status_label->set_text(database->get_last_error_message());
		}
		statement.unref();
		database->close();
	}
	previous_button->set_disabled(page == 0);
	next_button->set_disabled(page >= page_count - 1);
	_render();
}

void SQLiteTableEditor::_render() {
	tree->clear();
	row_ids.clear();
	TreeItem *root = tree->create_item();
	const Color dim_color = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));
	const Color added_color = get_theme_color(SNAME("success_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.2);
	const Color changed_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.2);
	const Color error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	const int key_count = key_columns.size();

	// New rows first, then the page.
	for (int i = 0; i < inserts.size() + rows.size(); i++) {
		const bool inserted = i < inserts.size();
		const Variant row_id = inserted ? Variant(i) : Variant(rows[i - inserts.size()].slice(0, key_count));
		const Dictionary changed = inserted ? Dictionary(inserts[i]) : Dictionary(updates.get(row_id, Dictionary()));
		const bool deleted = !inserted && deletes.has(row_id);
		const bool failed = row_id == failed_row;
		row_ids.push_back(row_id);

		TreeItem *item = tree->create_item(root);
		for (int c = 0; c < columns.size(); c++) {
			const String &column_name = columns[c]->get_name();
			const bool has_change = changed.has(column_name);
			const bool is_default = inserted && !has_change;
			const Variant value = has_change ? changed[column_name] : (inserted ? Variant() : rows[i - inserts.size()][key_count + c]);
			const String text = is_default ? String("DEFAULT") : _cell_text(value);
			item->set_text(c, text);
			item->set_metadata(c, text);
			item->set_editable(c, !deleted && value.get_type() != Variant::PACKED_BYTE_ARRAY);
			if (is_default || value.get_type() == Variant::NIL) {
				item->set_custom_color(c, dim_color);
			}
			if (failed) {
				item->set_custom_bg_color(c, error_color * Color(1, 1, 1, 0.4));
				item->set_tooltip_text(c, status_label->get_text());
			} else if (deleted) {
				item->set_custom_bg_color(c, error_color * Color(1, 1, 1, 0.2));
				item->set_tooltip_text(c, TTR("Deleted on Save."));
			} else if (inserted) {
				item->set_custom_bg_color(c, added_color);
			} else if (has_change) {
				item->set_custom_bg_color(c, changed_color);
			}
		}
		if (failed) {
			tree->scroll_to_item(item);
		}
	}
}

Array SQLiteTableEditor::_get_changes() const {
	return Array{ updates.duplicate(true), deletes.duplicate(true), inserts.duplicate(true) };
}

void SQLiteTableEditor::_set_changes(const Array &p_changes) {
	updates = p_changes[0];
	deletes = p_changes[1];
	inserts = p_changes[2];
	failed_row = Variant();
	const int count = updates.size() + deletes.size() + inserts.size();
	status_label->set_text(count > 0 ? vformat(TTR("%d rows with unsaved changes."), count) : String());
	_render();
}

void SQLiteTableEditor::_commit(const String &p_action, const Array &p_changes) {
	undo_redo->create_action(p_action);
	undo_redo->add_do_method(callable_mp(this, &SQLiteTableEditor::_set_changes).bind(p_changes));
	undo_redo->add_undo_method(callable_mp(this, &SQLiteTableEditor::_set_changes).bind(_get_changes()));
	undo_redo->commit_action();
}

void SQLiteTableEditor::_set_cell(const Variant &p_row, int p_column, const Variant &p_value) {
	ERR_FAIL_INDEX(p_column, columns.size());
	const Array changes = _get_changes();
	const String &column_name = columns[p_column]->get_name();
	if (p_row.get_type() == Variant::INT) {
		Dictionary values = Array(changes[2])[int(p_row)];
		values[column_name] = p_value;
	} else {
		Dictionary changed_rows = changes[0];
		Dictionary values = changed_rows.get(p_row, Dictionary());
		values[column_name] = p_value;
		changed_rows[p_row] = values;
	}
	_commit(TTR("Edit Cell"), changes);
}

void SQLiteTableEditor::_cell_edited() {
	TreeItem *item = tree->get_edited();
	const int column = tree->get_edited_column();
	ERR_FAIL_NULL(item);
	ERR_FAIL_INDEX(column, columns.size());
	const String text = item->get_text(column);
	if (text == String(item->get_metadata(column))) {
		return; // Unchanged; keeps NULL and DEFAULT cells as they are.
	}
	// Deferred: the tree is rebuilt, and it is still inside its own signal.
	callable_mp(this, &SQLiteTableEditor::_set_cell).call_deferred(row_ids[item->get_index()], column, _parse_cell(text, columns[column]->get_affinity()));
}

void SQLiteTableEditor::_add_row() {
	const Array changes = _get_changes();
	Array(changes[2]).push_back(Dictionary());
	_commit(TTR("Add Row"), changes);
}

void SQLiteTableEditor::_delete_row() {
	TreeItem *item = tree->get_selected();
	if (item == nullptr) {
		return;
	}
	const Variant row_id = row_ids[item->get_index()];
	const Array changes = _get_changes();
	if (row_id.get_type() == Variant::INT) {
		Array(changes[2]).remove_at(int(row_id));
	} else {
		Dictionary deleted_rows = changes[1];
		if (!deleted_rows.erase(row_id)) {
			deleted_rows[row_id] = true;
		}
	}
	_commit(TTR("Delete Row"), changes);
}

void SQLiteTableEditor::_set_null() {
	TreeItem *item = tree->get_selected();
	if (item != nullptr) {
		_set_cell(row_ids[item->get_index()], tree->get_selected_column(), Variant());
	}
}

void SQLiteTableEditor::_save() {
	if (updates.is_empty() && deletes.is_empty() && inserts.is_empty()) {
		return;
	}
	Ref<SQLiteDatabase> database = _open(SQLiteDatabase::OPEN_MODE_READ_WRITE);
	if (database.is_null()) {
		return;
	}
	Variant failed;
	String message;
	const Error err = apply_changes(database, table, key_columns, _get_changes(), failed, message);
	database->close();
	if (err != OK) {
		// Nothing was written; the edits stay pending so they can be fixed.
		status_label->set_text(vformat(TTR("Not saved: %s"), message));
		failed_row = failed;
		_render();
		return;
	}
	_revert();
	_load_page();
	// The module uses no file locks: a running game reads res:// databases as unchanging files.
	status_label->set_text(EditorRunBar::get_singleton()->is_playing() ? TTR("Saved. Restart the running game: it may still read the old data.") : TTR("Saved."));
}

void SQLiteTableEditor::_revert() {
	_set_changes(Array{ Dictionary(), Dictionary(), Array() });
	undo_redo->clear_history();
}

void SQLiteTableEditor::_filter_changed(const String &p_text) {
	page = 0;
	_load_page();
}

void SQLiteTableEditor::_change_page(int p_delta) {
	page += p_delta;
	_load_page();
}

void SQLiteTableEditor::_sort(int p_column, int p_mouse_button) {
	if (p_mouse_button != int(MouseButton::LEFT) || p_column >= columns.size()) {
		return;
	}
	sort_descending = p_column == sort_column && !sort_descending;
	if (sort_column >= 0) {
		tree->set_column_title(sort_column, columns[sort_column]->get_name());
	}
	sort_column = p_column;
	tree->set_column_title(p_column, columns[p_column]->get_name() + String::utf8(sort_descending ? " ▼" : " ▲"));
	_load_page();
}

void SQLiteTableEditor::edit(const String &p_path, const String &p_table) {
	const String global_path = ProjectSettings::get_singleton()->globalize_path(p_path);
	if (global_path != path || p_table != table) {
		if (!updates.is_empty() || !deletes.is_empty() || !inserts.is_empty()) {
			status_label->set_text(TTR("Save or revert the edits of this table first."));
			popup_centered_clamped(Size2(900, 600) * EDSCALE, 0.8);
			return;
		}
		path = global_path;
		table = p_table;
		columns.clear();
		key_columns.clear();
		status_label->set_text(String());
		Ref<SQLiteDatabase> database = _open(SQLiteDatabase::OPEN_MODE_READ_ONLY);
		if (database.is_valid()) {
			const TypedArray<SQLiteColumn> list = database->get_columns(table);
			for (int i = 0; i < list.size(); i++) {
				columns.push_back(list[i]);
				const int key_index = columns[i]->get_primary_key_index();
				if (key_index > 0) {
					if (key_columns.size() < key_index) {
						key_columns.resize(key_index);
					}
					key_columns.write[key_index - 1] = columns[i]->get_name();
				}
			}
			database->close();
		}
		if (key_columns.is_empty()) {
			key_columns.push_back("rowid");
		}

		tree->set_columns(MAX(1, columns.size()));
		for (int c = 0; c < columns.size(); c++) {
			tree->set_column_title(c, columns[c]->get_name());
			tree->set_column_title_tooltip_text(c, columns[c]->get_declared_type());
			tree->set_column_clip_content(c, true);
			tree->set_column_custom_minimum_width(c, 80 * EDSCALE);
		}
		page = 0;
		sort_column = -1;
		sort_descending = false;
		filter_edit->set_text(String());
		undo_redo->clear_history();
		set_title(vformat(TTR("Edit Table: %s"), table));
	}
	_load_page();
	popup_centered_clamped(Size2(900, 600) * EDSCALE, 0.8);
}

SQLiteTableEditor::SQLiteTableEditor() {
	singleton = this;
	set_ok_button_text(TTR("Close"));
	undo_redo = memnew(UndoRedo);

	VBoxContainer *main_box = memnew(VBoxContainer);
	add_child(main_box);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	main_box->add_child(toolbar);
	const auto add_tool_button = [toolbar](const String &p_text, const String &p_tooltip, const Callable &p_callable) {
		Button *button = memnew(Button(p_text));
		button->set_tooltip_text(p_tooltip);
		button->connect(SceneStringName(pressed), p_callable);
		toolbar->add_child(button);
		return button;
	};
	add_tool_button(TTR("Add Row"), TTR("Add a new row. Cells left at DEFAULT get the column default."), callable_mp(this, &SQLiteTableEditor::_add_row));
	add_tool_button(TTR("Delete Row"), TTR("Mark the selected row for deletion, or unmark it."), callable_mp(this, &SQLiteTableEditor::_delete_row));
	add_tool_button(TTR("Set NULL"), TTR("Set the selected cell to NULL."), callable_mp(this, &SQLiteTableEditor::_set_null));
	add_tool_button(TTR("Undo"), TTR("Undo the last pending edit."), callable_mp(undo_redo, &UndoRedo::undo))->set_shortcut(ED_GET_SHORTCUT("ui_undo"));
	add_tool_button(TTR("Redo"), TTR("Redo the last undone edit."), callable_mp(undo_redo, &UndoRedo::redo))->set_shortcut(ED_GET_SHORTCUT("ui_redo"));
	filter_edit = memnew(LineEdit);
	filter_edit->set_placeholder(TTR("Filter rows"));
	filter_edit->set_clear_button_enabled(true);
	filter_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter_edit->connect(SceneStringName(text_changed), callable_mp(this, &SQLiteTableEditor::_filter_changed));
	toolbar->add_child(filter_edit);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_column_titles_visible(true);
	tree->set_select_mode(Tree::SELECT_SINGLE);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->connect("item_edited", callable_mp(this, &SQLiteTableEditor::_cell_edited));
	tree->connect("column_title_clicked", callable_mp(this, &SQLiteTableEditor::_sort));
	main_box->add_child(tree);

	HBoxContainer *footer = memnew(HBoxContainer);
	main_box->add_child(footer);
	status_label = memnew(Label);
	status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	footer->add_child(status_label);
	previous_button = memnew(Button(TTR("Previous")));
	previous_button->connect(SceneStringName(pressed), callable_mp(this, &SQLiteTableEditor::_change_page).bind(-1));
	footer->add_child(previous_button);
	page_label = memnew(Label);
	footer->add_child(page_label);
	next_button = memnew(Button(TTR("Next")));
	next_button->connect(SceneStringName(pressed), callable_mp(this, &SQLiteTableEditor::_change_page).bind(1));
	footer->add_child(next_button);

	add_button(TTR("Revert"))->connect(SceneStringName(pressed), callable_mp(this, &SQLiteTableEditor::_revert));
	add_button(TTR("Save"))->connect(SceneStringName(pressed), callable_mp(this, &SQLiteTableEditor::_save));
}

SQLiteTableEditor::~SQLiteTableEditor() {
	memdelete(undo_redo);
	if (singleton == this) {
		singleton = nullptr;
	}
}
