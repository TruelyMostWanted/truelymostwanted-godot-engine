/**************************************************************************/
/*  csv_table_editor.cpp                                                  */
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

#include "csv_table_editor.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/script/data_schema_lookup.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/main/timer.h"
#include "scene/resources/text_file.h"
#include "servers/display/display_server.h"

static int64_t _cell_key(int p_row, int p_tree_column) {
	return ((int64_t)p_row << 32) | (uint32_t)p_tree_column;
}

static Ref<SerializerOptions> _tsv_options() {
	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_csv_delimiter("\t");
	options->set_csv_headers(false);
	return options;
}

TextEditorBase *CSVTableEditor::_get_text_editor() const {
	const Ref<Resource> edited = text_file.is_valid() ? Ref<Resource>(text_file) : Ref<Resource>(csv);
	return Object::cast_to<TextEditorBase>(ScriptEditor::get_singleton()->get_script_container()->get_resource_editor(edited));
}

Ref<DataField> CSVTableEditor::_get_field(int p_column) const {
	if (schema.is_null()) {
		return Ref<DataField>();
	}
	const PackedStringArray header = csv->get_header();
	if (!header.is_empty()) {
		return p_column < header.size() ? schema->get_field(header[p_column]) : Ref<DataField>();
	}
	const TypedArray<DataField> fields = schema->get_fields();
	return p_column < fields.size() ? Ref<DataField>(fields[p_column]) : Ref<DataField>();
}

void CSVTableEditor::_sync_from_text_file() {
	if (text_file.is_valid() && text_file->get_text() != synced_text) {
		synced_text = text_file->get_text();
		csv->parse(synced_text, true);
	}
}

bool CSVTableEditor::_can_edit() {
	TextEditorBase *text_editor = _get_text_editor();
	const TextEdit *text_edit = text_editor ? Object::cast_to<TextEdit>(text_editor->get_base_editor()) : nullptr;
	if (text_file.is_valid()) {
		// A TextFile is only updated when its text editor saves: unsaved text must not be overwritten.
		if (text_edit && text_edit->get_text() != text_file->get_text()) {
			status_label->set_text(TTR("The text editor has unsaved changes of this file. Save or discard them before editing the table."));
			return false;
		}
		_sync_from_text_file();
	} else if (text_edit) {
		// Takes over text typed since the text editor's last idle parse, so the table edits the current text.
		const bool in_sync = csv->get_parsed_text().is_empty() ? text_edit->get_version() == reloaded_version : text_edit->get_text() == csv->get_parsed_text();
		if (!in_sync) {
			csv->parse(text_edit->get_text(), true);
		}
	}
	if (!csv->get_error_message().is_empty()) {
		status_label->set_text(vformat(TTR("Fix the CSV text first: %s"), csv->get_error_message()));
		return false;
	}
	return true;
}

void CSVTableEditor::_commit_cells(const String &p_action, int p_row, int p_column, const Array &p_cells) {
	if (!_can_edit()) {
		_render();
		return;
	}
	const int row_count = csv->get_row_count();
	ERR_FAIL_INDEX(p_row, row_count + 1);
	int width = 0;
	for (int i = 0; i < p_cells.size(); i++) {
		width = MAX(width, (int)PackedStringArray(p_cells[i]).size());
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(p_action, EditorUndoRedoManager::GLOBAL_HISTORY);
	undo_redo->add_do_method(csv.ptr(), "set_cells", p_row, p_column, p_cells);
	undo_redo->add_undo_method(csv.ptr(), "set_cells", p_row, p_column, csv->get_cells(p_row, p_column, p_cells.size(), width));
	for (int i = row_count; i < p_row + p_cells.size(); i++) {
		undo_redo->add_undo_method(csv.ptr(), "remove_row", row_count);
	}
	undo_redo->commit_action();
}

void CSVTableEditor::_csv_changed() {
	if (!update_queued) {
		update_queued = true;
		callable_mp(this, &CSVTableEditor::_update).call_deferred();
	}
}

void CSVTableEditor::_update() {
	update_queued = false;
	if (csv.is_null()) {
		return;
	}
	// No kept text: the data was changed by the table, undo/redo or a script, not by parsing the text.
	// Show the change in the text editor, so neither side overwrites the other.
	if (csv->get_parsed_text().is_empty()) {
		if (text_file.is_valid()) {
			synced_text = csv->encode_to_text();
			text_file->set_text(synced_text);
		}
		if (TextEditorBase *text_editor = _get_text_editor()) {
			text_editor->reload_text();
			if (const TextEdit *text_edit = Object::cast_to<TextEdit>(text_editor->get_base_editor())) {
				reloaded_version = text_edit->get_version();
			}
		}
	}
	_render();
	if (validate_timer->is_inside_tree()) {
		validate_timer->start();
	}
}

void CSVTableEditor::_render() {
	if (csv.is_null()) {
		tree->clear();
		return;
	}
	updating = true;
	const int row_count = csv->get_row_count();
	const int column_count = MAX(csv->get_column_count(), 1);
	const int page_count = MAX(1, (row_count + PAGE_SIZE - 1) / PAGE_SIZE);
	page = CLAMP(page, 0, page_count - 1);
	const int first = page * PAGE_SIZE;
	const int last = MIN(first + PAGE_SIZE, row_count);

	const PackedStringArray header = csv->get_header();
	if (tree->get_columns() != column_count + 1) {
		tree->clear();
		tree->set_columns(column_count + 1);
	}
	tree->set_column_title(0, "#");
	tree->set_column_expand(0, false);
	tree->set_column_custom_minimum_width(0, 60 * EDSCALE);
	for (int column = 0; column < column_count; column++) {
		const Ref<DataField> field = _get_field(column);
		tree->set_column_title(column + 1, column < header.size() ? header[column] : itos(column + 1));
		tree->set_column_title_tooltip_text(column + 1, field.is_valid() ? vformat("%s\n%s", Variant::get_type_name(field->get_value_type()), field->get_description()).strip_edges() : String());
		tree->set_column_custom_minimum_width(column + 1, 100 * EDSCALE);
		tree->set_column_clip_content(column + 1, true);
	}

	TreeItem *root = tree->get_root() ? tree->get_root() : tree->create_item();
	TreeItem *item = root->get_first_child();
	const Array rows = csv->get_data();
	const bool editable = csv->get_error_message().is_empty();
	const Color issue_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.35);
	for (int row = first; row < last; row++) {
		if (!item) {
			item = tree->create_item(root);
		}
		const PackedStringArray cells = rows[row];
		item->set_text(0, itos(row + 1));
		item->set_metadata(0, row);
		item->set_selectable(0, false);
		for (int column = 0; column <= column_count; column++) {
			const String *issue = cell_issues.getptr(_cell_key(row, column));
			item->set_tooltip_text(column, issue ? *issue : String());
			if (issue) {
				item->set_custom_bg_color(column, issue_color);
			} else {
				item->clear_custom_bg_color(column);
			}
			if (column == 0) {
				continue;
			}
			const String text = column - 1 < cells.size() ? cells[column - 1] : String();
			const Ref<DataField> field = _get_field(column - 1);
			if (field.is_valid() && field->get_value_type() == Variant::BOOL) {
				item->set_cell_mode(column, TreeItem::CELL_MODE_CHECK);
				item->set_checked(column, text.to_lower() == "true" || text == "1");
			} else if (field.is_valid() && field->get_hint() == PROPERTY_HINT_ENUM && (field->get_value_type() != Variant::STRING || field->get_hint_string().split(",").has(text))) {
				// Like the inspector: "Name:value" options for numbers, the option index for strings.
				// A string that is no option stays a text cell below, so the wrong value is visible.
				item->set_cell_mode(column, TreeItem::CELL_MODE_RANGE);
				item->set_text(column, field->get_hint_string());
				item->set_range(column, field->get_value_type() == Variant::STRING ? (int)field->get_hint_string().split(",").find(text) : text.to_int());
			} else {
				item->set_cell_mode(column, TreeItem::CELL_MODE_STRING);
				item->set_text(column, text);
			}
			item->set_editable(column, editable);
		}
		item = item->get_next();
	}
	while (item) {
		TreeItem *next = item->get_next();
		memdelete(item);
		item = next;
	}

	page_label->set_text(row_count == 0 ? TTR("No rows") : vformat(TTR("Rows %d-%d of %d"), first + 1, last, row_count));
	previous_button->set_disabled(page == 0);
	next_button->set_disabled(page >= page_count - 1);
	updating = false;
}

void CSVTableEditor::_validate() {
	if (csv.is_null()) {
		return;
	}
	cell_issues.clear();
	Ref<DataValidation> result;
	if (translation_mode) {
		result = csv->validate_translation();
	} else if (schema.is_valid()) {
		result = csv->validate(schema);
	}
	if (result.is_null()) {
		status_label->set_text(csv->get_error_message());
		_render();
		return;
	}

	HashMap<int, int> missing; // Column -> missing translations.
	for (int i = 0; i < result->get_issue_count(); i++) {
		const int row = result->get_issue_row(i);
		const int column = result->get_issue_column(i);
		if (row != DataValidation::POSITION_NONE) {
			const int64_t key = _cell_key(row, column == DataValidation::POSITION_NONE ? 0 : column + 1);
			const String *previous = cell_issues.getptr(key);
			cell_issues[key] = previous ? *previous + "\n" + result->get_issue_message(i) : result->get_issue_message(i);
		}
		if (result->get_issue_kind(i) == DataValidation::ISSUE_KIND_EMPTY_REQUIRED) {
			missing[column]++;
		}
	}

	String status = result->is_valid() ? TTR("No issues.") : vformat(TTR("%d issue(s). First: %s"), result->get_issue_count(), result->get_issue_message(0));
	if (translation_mode) {
		CSV::TranslationHeader columns;
		Ref<DataValidation> header_issues;
		header_issues.instantiate();
		CSV::check_translation_header(csv->get_header(), columns, header_issues.ptr());
		PackedStringArray counts;
		for (const KeyValue<int, String> &E : columns.locale_columns) {
			counts.push_back(vformat("%s: %d", E.value, missing.has(E.key) ? missing[E.key] : 0));
		}
		if (!counts.is_empty()) {
			status = vformat(TTR("Missing translations: %s. %s"), String(", ").join(counts), status);
		}
	}
	status_label->set_text(status);
	_render();
}

void CSVTableEditor::_cell_edited() {
	if (updating) {
		return;
	}
	TreeItem *item = tree->get_edited();
	const int tree_column = tree->get_edited_column();
	ERR_FAIL_COND(!item || tree_column < 1);
	String value;
	if (item->get_cell_mode(tree_column) == TreeItem::CELL_MODE_CHECK) {
		value = item->is_checked(tree_column) ? "true" : "false";
	} else if (item->get_cell_mode(tree_column) == TreeItem::CELL_MODE_RANGE) {
		const int option = (int)item->get_range(tree_column);
		const Ref<DataField> field = _get_field(tree_column - 1);
		value = field.is_valid() && field->get_value_type() == Variant::STRING ? field->get_hint_string().get_slicec(',', option) : itos(option);
	} else {
		value = item->get_text(tree_column);
	}
	Array cells;
	cells.push_back(PackedStringArray({ value }));
	_commit_cells(TTR("Set CSV Cell"), (int)item->get_metadata(0), tree_column - 1, cells);
}

void CSVTableEditor::_add_row(bool p_insert) {
	if (csv.is_null() || !_can_edit()) {
		return;
	}
	TreeItem *selected = tree->get_selected();
	const int row = p_insert && selected ? (int)selected->get_metadata(0) : csv->get_row_count();
	PackedStringArray cells;
	cells.resize(csv->get_column_count());
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(TTR("Add CSV Row"), EditorUndoRedoManager::GLOBAL_HISTORY);
	undo_redo->add_do_method(csv.ptr(), "insert_row", row, cells);
	undo_redo->add_undo_method(csv.ptr(), "remove_row", row);
	undo_redo->commit_action();
	page = row / PAGE_SIZE;
}

void CSVTableEditor::_remove_rows() {
	if (csv.is_null() || !_can_edit()) {
		return;
	}
	Vector<int> rows;
	for (TreeItem *item = tree->get_next_selected(nullptr); item; item = tree->get_next_selected(item)) {
		rows.push_back((int)item->get_metadata(0));
	}
	if (rows.is_empty()) {
		return;
	}
	rows.sort();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(TTR("Remove CSV Rows"), EditorUndoRedoManager::GLOBAL_HISTORY);
	for (int i = rows.size() - 1; i >= 0; i--) {
		undo_redo->add_do_method(csv.ptr(), "remove_row", rows[i]);
	}
	for (int row : rows) {
		undo_redo->add_undo_method(csv.ptr(), "insert_row", row, csv->get_row(row));
	}
	undo_redo->commit_action();
}

void CSVTableEditor::_copy() {
	// The bounding box of the selected cells, as tab-separated text like spreadsheets copy it.
	int first_row = INT_MAX;
	int last_row = -1;
	int first_column = INT_MAX;
	int last_column = -1;
	for (TreeItem *item = tree->get_next_selected(nullptr); item; item = tree->get_next_selected(item)) {
		for (int column = 1; column < tree->get_columns(); column++) {
			if (item->is_selected(column)) {
				const int row = (int)item->get_metadata(0);
				first_row = MIN(first_row, row);
				last_row = MAX(last_row, row);
				first_column = MIN(first_column, column - 1);
				last_column = MAX(last_column, column - 1);
			}
		}
	}
	if (last_row >= 0) {
		DisplayServer::get_singleton()->clipboard_set(CSV::stringify(csv->get_cells(first_row, first_column, last_row - first_row + 1, last_column - first_column + 1), _tsv_options()));
	}
}

void CSVTableEditor::_paste() {
	TreeItem *item = tree->get_selected();
	const int tree_column = tree->get_selected_column();
	if (csv.is_null() || !item || tree_column < 1) {
		return;
	}
	const Array cells = CSV::parse_string(DisplayServer::get_singleton()->clipboard_get(), _tsv_options());
	if (!cells.is_empty()) {
		_commit_cells(TTR("Paste CSV Cells"), (int)item->get_metadata(0), tree_column - 1, cells);
	}
}

void CSVTableEditor::_save() {
	if (csv.is_null() || !_can_edit()) {
		return;
	}
	TextEditorBase *text_editor = _get_text_editor();
	if (text_file.is_valid()) {
		const String path = text_file->get_path();
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		ERR_FAIL_COND_MSG(file.is_null(), vformat("Cannot save '%s'.", path));
		file->store_string(text_file->get_text());
		file.unref();
		const uint64_t modified_time = FileAccess::get_modified_time(path);
		text_file->set_last_modified_time(modified_time);
		if (text_editor) {
			text_editor->edited_file_data.last_modified_time = modified_time; // No "changed on disk" prompt.
		}
		EditorFileSystem::get_singleton()->update_file(path);
	} else {
		EditorNode::get_singleton()->save_resource(csv);
	}
	if (text_editor) {
		text_editor->tag_saved_version();
	}
}

void CSVTableEditor::_change_page(int p_delta) {
	page += p_delta;
	_render();
}

void CSVTableEditor::_schema_changed(const Ref<Resource> &p_schema) {
	schema = p_schema;
	_validate();
}

void CSVTableEditor::_tree_gui_input(const Ref<InputEvent> &p_event) {
	if (p_event->is_action_pressed(SNAME("ui_copy"), false, true)) {
		_copy();
		tree->accept_event();
	} else if (p_event->is_action_pressed(SNAME("ui_paste"), false, true)) {
		_paste();
		tree->accept_event();
	}
}

void CSVTableEditor::edit(const Ref<Resource> &p_resource) {
	if (csv.is_valid()) {
		csv->disconnect_changed(callable_mp(this, &CSVTableEditor::_csv_changed));
	}
	text_file = p_resource;
	csv = p_resource;
	schema.unref();
	cell_issues.clear();
	page = 0;
	if (text_file.is_valid()) {
		// Translation CSVs open as TextFile: parse them with the delimiter of their import settings.
		Ref<ConfigFile> import_config;
		import_config.instantiate();
		Ref<SerializerOptions> options;
		options.instantiate();
		if (import_config->load(text_file->get_path() + ".import") == OK) {
			const int delimiter = (int)import_config->get_value("params", "delimiter", 0);
			options->set_csv_delimiter(delimiter == 1 ? ";" : (delimiter == 2 ? "\t" : ","));
		}
		csv.instantiate();
		csv->set_options(options);
		synced_text = String();
		_sync_from_text_file();
	}
	if (csv.is_null()) {
		tree->clear();
		return;
	}
	csv->connect_changed(callable_mp(this, &CSVTableEditor::_csv_changed));

	const String path = p_resource->get_path();
	translation_mode = ResourceLoader::get_resource_type(path) == "Translation";
	if (!translation_mode) {
		schema = EditorDataSchemaLookup::find_schema(path);
	}
	schema_picker->set_visible(!translation_mode);
	schema_picker->set_edited_resource(schema);
	file_label->set_text(path.get_file());
	_validate();
}

CSVTableEditor::CSVTableEditor() {
	singleton = this;
	set_name(TTRC("CSV Table"));
	set_icon_name("TextFile");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_csv_table_bottom_panel", TTRC("Toggle CSV Table Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	main_vbox->add_child(toolbar);
	file_label = memnew(Label);
	toolbar->add_child(file_label);

	Button *add_button = memnew(Button(TTR("Add Row")));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_add_row).bind(false));
	toolbar->add_child(add_button);
	Button *insert_button = memnew(Button(TTR("Insert Row")));
	insert_button->set_tooltip_text(TTR("Insert an empty row above the selected row."));
	insert_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_add_row).bind(true));
	toolbar->add_child(insert_button);
	Button *remove_button = memnew(Button(TTR("Remove Rows")));
	remove_button->set_tooltip_text(TTR("Remove the rows of the selected cells."));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_remove_rows));
	toolbar->add_child(remove_button);
	Button *save_button = memnew(Button(TTR("Save")));
	save_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_save));
	toolbar->add_child(save_button);

	toolbar->add_child(memnew(Label(TTR("Schema:"))));
	schema_picker = memnew(EditorResourcePicker);
	schema_picker->set_base_type("DataSchema");
	schema_picker->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
	schema_picker->set_tooltip_text(TTR("Types the cells and checks them. Found by the schema's file patterns in the project setting \"filesystem/data/schema_files\"; a schema picked here is not saved."));
	schema_picker->connect("resource_changed", callable_mp(this, &CSVTableEditor::_schema_changed));
	toolbar->add_child(schema_picker);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(SIZE_EXPAND_FILL);
	toolbar->add_child(spacer);
	previous_button = memnew(Button(TTR("Previous")));
	previous_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_change_page).bind(-1));
	toolbar->add_child(previous_button);
	page_label = memnew(Label);
	toolbar->add_child(page_label);
	next_button = memnew(Button(TTR("Next")));
	next_button->connect(SceneStringName(pressed), callable_mp(this, &CSVTableEditor::_change_page).bind(1));
	toolbar->add_child(next_button);

	tree = memnew(Tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_hide_root(true);
	tree->set_column_titles_visible(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->connect("item_edited", callable_mp(this, &CSVTableEditor::_cell_edited));
	tree->connect(SceneStringName(gui_input), callable_mp(this, &CSVTableEditor::_tree_gui_input));
	tree->connect(SceneStringName(mouse_entered), callable_mp(this, &CSVTableEditor::_sync_from_text_file));
	main_vbox->add_child(tree);

	status_label = memnew(Label);
	status_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	status_label->set_h_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_child(status_label);

	// Validation runs once after a burst of edits, not per edit (large files).
	validate_timer = memnew(Timer);
	validate_timer->set_one_shot(true);
	validate_timer->set_wait_time(0.5);
	validate_timer->connect("timeout", callable_mp(this, &CSVTableEditor::_validate));
	add_child(validate_timer);
}

CSVTableEditor::~CSVTableEditor() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void CSVTableEditorPlugin::edit(Object *p_object) {
	table_editor->edit(Ref<Resource>(Object::cast_to<Resource>(p_object)));
}

bool CSVTableEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<CSV>(p_object)) {
		return true;
	}
	const TextFile *text_file = Object::cast_to<TextFile>(p_object);
	return text_file && text_file->get_path().get_extension().to_lower() == "csv";
}

void CSVTableEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		table_editor->make_visible();
	} else {
		table_editor->close();
	}
}

CSVTableEditorPlugin::CSVTableEditorPlugin() {
	table_editor = memnew(CSVTableEditor);
	table_editor->set_custom_minimum_size(Size2(0, 250) * EDSCALE);
	EditorDockManager::get_singleton()->add_dock(table_editor);
	table_editor->close();
}
