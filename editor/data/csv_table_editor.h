/**************************************************************************/
/*  csv_table_editor.h                                                    */
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

#include "core/io/csv_format.h"
#include "core/io/data_schema.h"
#include "editor/docks/editor_dock.h"
#include "editor/plugins/editor_plugin.h"

class Button;
class EditorResourcePicker;
class Label;
class TextEditorBase;
class TextFile;
class Timer;
class Tree;

// Edits a CSV file as a table in a bottom dock. Only one page of rows has tree items, so large files stay fast.
// Data CSVs (CSV resources) get typed cells and issues from a DataSchema; translation CSVs (opened as TextFile)
// get the issues of CSV.validate_translation(). Edits go through EditorUndoRedoManager and are shown in an open
// text editor of the same file right away.
class CSVTableEditor : public EditorDock {
	GDCLASS(CSVTableEditor, EditorDock);

	static constexpr int PAGE_SIZE = 500;

	Ref<CSV> csv;
	Ref<TextFile> text_file; // Translation CSVs: the TextFile of the script editor, kept in sync with `csv`.
	String synced_text; // Text of `text_file` that `csv` was parsed from or written to.
	uint32_t reloaded_version = UINT32_MAX; // Version of the text editor's text right after the table reloaded it.
	Ref<DataSchema> schema;
	bool translation_mode = false;
	HashMap<int64_t, String> cell_issues; // (row << 32) | tree column -> messages.
	int page = 0;
	bool updating = false;
	bool update_queued = false;

	Label *file_label = nullptr;
	EditorResourcePicker *schema_picker = nullptr;
	Button *previous_button = nullptr;
	Button *next_button = nullptr;
	Label *page_label = nullptr;
	Label *status_label = nullptr;
	Tree *tree = nullptr;
	Timer *validate_timer = nullptr;

	TextEditorBase *_get_text_editor() const;
	Ref<DataField> _get_field(int p_column) const;
	void _sync_from_text_file();
	bool _can_edit();
	void _commit_cells(const String &p_action, int p_row, int p_column, const Array &p_cells);

	void _csv_changed();
	void _update();
	void _render();
	void _validate();
	void _cell_edited();
	void _add_row(bool p_insert);
	void _remove_rows();
	void _copy();
	void _paste();
	void _save();
	void _change_page(int p_delta);
	void _schema_changed(const Ref<Resource> &p_schema);
	void _tree_gui_input(const Ref<InputEvent> &p_event);

public:
	void edit(const Ref<Resource> &p_resource);

	CSVTableEditor();
};

class CSVTableEditorPlugin : public EditorPlugin {
	GDCLASS(CSVTableEditorPlugin, EditorPlugin);

	CSVTableEditor *table_editor = nullptr;

public:
	virtual String get_plugin_name() const override { return "CSVTable"; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	CSVTableEditorPlugin();
};
