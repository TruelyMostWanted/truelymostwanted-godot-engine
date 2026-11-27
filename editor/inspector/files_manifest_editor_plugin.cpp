/**************************************************************************/
/*  files_manifest_editor_plugin.cpp                                      */
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

#include "files_manifest_editor_plugin.h"

#include "core/core_string_names.h"
#include "core/object/callable_mp.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/scene_string_names.h"

PackedStringArray EditorPropertyFilesManifestPaths::_get_paths() const {
	return get_edited_property_value();
}

void EditorPropertyFilesManifestPaths::_set_paths(const PackedStringArray &p_paths) {
	// An own undo action: the Inspector merges quick edits of the same property into one step.
	Object *edited_object = get_edited_object();
	const StringName edited_property = get_edited_property();
	EditorInspector *inspector = get_parent_inspector();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Set %s"), edited_property), UndoRedo::MERGE_DISABLE, edited_object);
	undo_redo->add_do_property(edited_object, edited_property, p_paths);
	undo_redo->add_undo_property(edited_object, edited_property, _get_paths());
	if (inspector != nullptr) {
		undo_redo->add_do_method(inspector, "_edit_request_change", edited_object, String(edited_property));
		undo_redo->add_undo_method(inspector, "_edit_request_change", edited_object, String(edited_property));
	}
	undo_redo->commit_action();
}

void EditorPropertyFilesManifestPaths::_browse(int p_index) {
	if (dialog == nullptr) {
		dialog = memnew(EditorFileDialog);
		dialog->set_file_mode(folders ? EditorFileDialog::FILE_MODE_OPEN_DIR : EditorFileDialog::FILE_MODE_OPEN_FILE);
		dialog->connect(SNAME("dir_selected"), callable_mp(this, &EditorPropertyFilesManifestPaths::_path_selected));
		dialog->connect(SNAME("file_selected"), callable_mp(this, &EditorPropertyFilesManifestPaths::_path_selected));
		add_child(dialog);
	}
	dialog_index = p_index;
	const PackedStringArray paths = _get_paths();
	// A wildcard path opens at its base folder.
	const String current = p_index >= 0 && p_index < paths.size() ? FilesManifest::get_folder_base(paths[p_index]) : String();
	if (current.begins_with("res://")) {
		if (folders) {
			dialog->set_current_dir(current);
		} else {
			dialog->set_current_path(current);
		}
	}
	dialog->popup_file_dialog();
}

void EditorPropertyFilesManifestPaths::_path_selected(const String &p_path) {
	PackedStringArray paths = _get_paths();
	if (dialog_index >= 0 && dialog_index < paths.size()) {
		paths.set(dialog_index, p_path);
	} else {
		paths.push_back(p_path);
	}
	_set_paths(paths);
}

void EditorPropertyFilesManifestPaths::_path_edited(int p_index) {
	// Removing a focused row while rebuilding emits `focus_exited`.
	if (updating || p_index >= rows->get_child_count()) {
		return;
	}
	PackedStringArray paths = _get_paths();
	LineEdit *edit = Object::cast_to<LineEdit>(rows->get_child(p_index)->get_child(0));
	if (edit == nullptr || p_index >= paths.size() || edit->get_text() == paths[p_index]) {
		return;
	}
	paths.set(p_index, edit->get_text().strip_edges());
	_set_paths(paths);
}

void EditorPropertyFilesManifestPaths::_move(int p_index, int p_offset) {
	PackedStringArray paths = _get_paths();
	const int target = p_index + p_offset;
	if (p_index < 0 || p_index >= paths.size() || target < 0 || target >= paths.size()) {
		return;
	}
	const String path = paths[p_index];
	paths.set(p_index, paths[target]);
	paths.set(target, path);
	_set_paths(paths);
}

void EditorPropertyFilesManifestPaths::_remove(int p_index) {
	PackedStringArray paths = _get_paths();
	if (p_index >= 0 && p_index < paths.size()) {
		paths.remove_at(p_index);
		_set_paths(paths);
	}
}

void EditorPropertyFilesManifestPaths::_add_row(int p_index) {
	HBoxContainer *row = memnew(HBoxContainer);
	rows->add_child(row);

	LineEdit *edit = memnew(LineEdit);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	edit->connect(SceneStringName(text_submitted), callable_mp(this, &EditorPropertyFilesManifestPaths::_path_edited).bind(p_index).unbind(1));
	edit->connect(SceneStringName(focus_exited), callable_mp(this, &EditorPropertyFilesManifestPaths::_path_edited).bind(p_index));
	row->add_child(edit);

	struct RowButton {
		const char *icon;
		String tooltip;
		Callable callback;
	};
	const RowButton buttons[] = {
		{ folders ? "Folder" : "File", folders ? TTR("Choose a folder.") : TTR("Choose a file."), callable_mp(this, &EditorPropertyFilesManifestPaths::_browse).bind(p_index) },
		{ "MoveUp", TTR("Move up: loaded earlier."), callable_mp(this, &EditorPropertyFilesManifestPaths::_move).bind(p_index, -1) },
		{ "MoveDown", TTR("Move down: loaded later."), callable_mp(this, &EditorPropertyFilesManifestPaths::_move).bind(p_index, 1) },
		{ "Remove", TTR("Remove."), callable_mp(this, &EditorPropertyFilesManifestPaths::_remove).bind(p_index) },
	};
	for (const RowButton &info : buttons) {
		Button *button = memnew(Button);
		button->set_flat(true);
		button->set_button_icon(get_editor_theme_icon(info.icon));
		button->set_tooltip_text(info.tooltip);
		button->connect(SceneStringName(pressed), info.callback);
		row->add_child(button);
	}
}

void EditorPropertyFilesManifestPaths::update_property() {
	// Rows are updated in place and only added or removed at the end: a commit on `focus_exited` must not
	// free the field or button that is about to get the click.
	updating = true;
	const PackedStringArray paths = _get_paths();
	for (int i = rows->get_child_count() - 1; i >= paths.size(); i--) {
		Node *row = rows->get_child(i);
		rows->remove_child(row);
		row->queue_free();
	}
	for (int i = rows->get_child_count(); i < paths.size(); i++) {
		_add_row(i);
	}

	const bool locked = is_read_only();
	for (int i = 0; i < paths.size(); i++) {
		Node *row = rows->get_child(i);
		LineEdit *edit = Object::cast_to<LineEdit>(row->get_child(0));
		if (edit->get_text() != paths[i]) {
			edit->set_text(paths[i]);
		}
		edit->set_tooltip_text(paths[i]);
		edit->set_editable(!locked);
		const bool disabled[] = { false, i == 0, i == paths.size() - 1, false };
		for (int j = 0; j < 4; j++) {
			Object::cast_to<Button>(row->get_child(j + 1))->set_disabled(locked || disabled[j]);
		}
	}
	add_button->set_disabled(locked);
	updating = false;
}

void EditorPropertyFilesManifestPaths::_set_read_only(bool p_read_only) {
	update_property();
}

void EditorPropertyFilesManifestPaths::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		// The row icons come from the theme: build the rows again.
		for (int i = rows->get_child_count() - 1; i >= 0; i--) {
			Node *row = rows->get_child(i);
			rows->remove_child(row);
			row->queue_free();
		}
		update_property();
	}
}

EditorPropertyFilesManifestPaths::EditorPropertyFilesManifestPaths(bool p_folders) {
	folders = p_folders;

	VBoxContainer *box = memnew(VBoxContainer);
	add_child(box);
	set_bottom_editor(box);

	rows = memnew(VBoxContainer);
	box->add_child(rows);

	add_button = memnew(Button);
	add_button->set_text(folders ? TTR("Folder...") : TTR("File..."));
	add_button->set_tooltip_text(folders ? TTR("Add a folder. Folder paths may contain \"*\" for one folder level, e.g. \"user://mods/*/translations\".") : TTR("Add a file."));
	add_button->set_h_size_flags(SIZE_SHRINK_BEGIN);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertyFilesManifestPaths::_browse).bind(-1));
	box->add_child(add_button);
}

void FilesManifestPreview::_update_preview() {
	list->set_visible(toggle->is_pressed());
	if (!toggle->is_pressed() || manifest.is_null()) {
		toggle->set_text(TTR("Preview"));
		nothing->hide();
		return;
	}

	LocalVector<int> counts;
	const PackedStringArray paths = manifest->get_paths(nullptr, &counts);
	toggle->set_text(vformat(TTR("Preview: %d files in load order"), paths.size()));
	list->clear();
	for (int i = 0; i < paths.size(); i++) {
		list->add_item(vformat("%d. %s", i + 1, paths[i]), Ref<Texture2D>(), false);
	}

	PackedStringArray entries = manifest->get_folder_paths();
	entries.append_array(manifest->get_file_paths());
	PackedStringArray empty;
	for (uint32_t i = 0; i < counts.size() && int(i) < entries.size(); i++) {
		if (counts[i] == 0) {
			empty.push_back(entries[i].strip_edges().is_empty() ? TTR("(empty path)") : entries[i]);
		}
	}
	nothing->set_visible(!empty.is_empty());
	nothing->set_text(TTR("Resolve to nothing:") + "\n" + String("\n").join(empty));
}

void FilesManifestPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (manifest.is_valid() && !manifest->is_connected(CoreStringName(changed), callable_mp(this, &FilesManifestPreview::_update_preview))) {
				manifest->connect(CoreStringName(changed), callable_mp(this, &FilesManifestPreview::_update_preview));
			}
			_update_preview();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (manifest.is_valid() && manifest->is_connected(CoreStringName(changed), callable_mp(this, &FilesManifestPreview::_update_preview))) {
				manifest->disconnect(CoreStringName(changed), callable_mp(this, &FilesManifestPreview::_update_preview));
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			nothing->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

FilesManifestPreview::FilesManifestPreview(const Ref<FilesManifest> &p_manifest) {
	manifest = p_manifest;

	toggle = memnew(Button);
	toggle->set_toggle_mode(true);
	toggle->set_flat(true);
	toggle->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	toggle->set_tooltip_text(TTR("The files of the manifest in load order: folders first (in list order, the files of a folder sorted by path), then single files. Extension and type filters are applied."));
	toggle->connect(SceneStringName(toggled), callable_mp(this, &FilesManifestPreview::_update_preview).unbind(1));
	add_child(toggle);

	nothing = memnew(Label);
	nothing->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	nothing->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	nothing->hide();
	add_child(nothing);

	list = memnew(ItemList);
	list->set_custom_minimum_size(Size2(0, 160 * EDSCALE));
	list->hide();
	add_child(list);
}

bool EditorInspectorPluginFilesManifest::can_handle(Object *p_object) {
	return Object::cast_to<FilesManifest>(p_object) != nullptr;
}

bool EditorInspectorPluginFilesManifest::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path != "folder_paths" && p_path != "file_paths") {
		return false;
	}
	add_property_editor(p_path, memnew(EditorPropertyFilesManifestPaths(p_path == "folder_paths")));
	return true;
}

void EditorInspectorPluginFilesManifest::parse_end(Object *p_object) {
	const Ref<FilesManifest> manifest(Object::cast_to<FilesManifest>(p_object));
	if (manifest.is_valid()) {
		add_custom_control(memnew(FilesManifestPreview(manifest)));
	}
}

FilesManifestEditorPlugin::FilesManifestEditorPlugin() {
	Ref<EditorInspectorPluginFilesManifest> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
