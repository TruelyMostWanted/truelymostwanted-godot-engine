/**************************************************************************/
/*  pck_package_editor_plugin.h                                           */
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

#include "core/io/pck_package.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class Button;
class ConfirmationDialog;
class EditorFileDialog;
class Label;
class LineEdit;
class Tree;

// Edits of a pack description, as pure functions on its storage properties: `files` (an array of dictionaries
// with `path` and one of `source`, `data` or `from_pack`, plus `encrypt`) and `removals` (target paths).
// The Inspector applies the results with undo/redo; without editor state, they are unit-tested.
class PCKPackageEditorHelper {
public:
	// Turns a source path into a target path inside the pack: `\` becomes `/`, the path is simplified,
	// `p_root` (or `res://`) and leading slashes are removed. Returns an empty string for an invalid target
	// (empty, a folder, or with `..`).
	static String to_target_path(const String &p_source, const String &p_root = "res://");
	// False for files that must not be packed as sources: `.import` and `.uid` sidecars, hidden files and
	// folders, and everything below `.godot/`.
	static bool is_packable(const String &p_target);
	// All files below `p_dir`, recursively and sorted; hidden files and folders are skipped.
	static PackedStringArray collect_files(const String &p_dir);

	static int find_file(const Array &p_files, const String &p_target);
	// Adds files and folders (recursively) as `source` entries; the target is the path relative to `p_root`.
	// An existing target is replaced in place. Non-packable files are skipped. `r_added` counts the added files.
	static Array add_sources(const Array &p_files, const PackedStringArray &p_sources, const String &p_root, bool p_encrypt, int *r_added = nullptr);
	static Array remove_files(const Array &p_files, const PackedStringArray &p_targets);
	// Fails with ERR_INVALID_PARAMETER (invalid target), ERR_DOES_NOT_EXIST or ERR_ALREADY_EXISTS; the array is then unchanged.
	static Array rename_file(const Array &p_files, const String &p_from, const String &p_to, Error *r_error = nullptr);
	static Array set_encrypt(const Array &p_files, const String &p_target, bool p_encrypt);
	static PackedStringArray add_removals(const PackedStringArray &p_removals, const PackedStringArray &p_paths);
	static PackedStringArray remove_removals(const PackedStringArray &p_removals, const PackedStringArray &p_targets);
};

// The file list of a `PCKPackage` in the Inspector. A description is editable (drag & drop from the FileSystem
// dock, rename, remove, removals, encrypt, "Build Pack…"); a package opened from a `.pck` is read-only.
class PCKPackageEditor : public VBoxContainer {
	GDCLASS(PCKPackageEditor, VBoxContainer);

	enum FileAction {
		FILE_ACTION_ADD_FILES,
		FILE_ACTION_ADD_FOLDER,
		FILE_ACTION_ADD_REMOVALS,
		FILE_ACTION_BUILD_PATH,
		FILE_ACTION_CREATE_DESCRIPTION,
	};

	Ref<PCKPackage> package;
	bool read_only = false;
	bool update_queued = false;

	Label *summary_label = nullptr;
	Button *add_files_button = nullptr;
	Button *add_folder_button = nullptr;
	Button *remove_button = nullptr;
	Button *add_removal_button = nullptr;
	Button *build_button = nullptr;
	Button *create_description_button = nullptr;
	Tree *tree = nullptr;

	EditorFileDialog *file_dialog = nullptr;
	FileAction file_action = FILE_ACTION_ADD_FILES;

	ConfirmationDialog *build_dialog = nullptr;
	LineEdit *build_path_edit = nullptr;
	LineEdit *build_key_edit = nullptr;
	Label *build_status_label = nullptr;

	void _queue_update();
	void _update_tree();
	void _commit(const String &p_action, const Array &p_files, const PackedStringArray &p_removals);
	void _add_sources(const PackedStringArray &p_sources);

	void _popup_file_dialog(int p_action);
	void _files_selected(const PackedStringArray &p_paths);
	void _file_selected(const String &p_path);
	void _dir_selected(const String &p_dir);
	void _remove_selected();
	void _tree_item_edited();

	void _popup_build_dialog();
	void _use_preset_key();
	void _build();
	void _create_description(const String &p_path);

	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

protected:
	void _notification(int p_what);

public:
	PCKPackageEditor(const Ref<PCKPackage> &p_package);
};

// Puts the `PCKPackageEditor` at the top of the Inspector of a `PCKPackage`. For a package opened from a `.pck`,
// the editable properties are hidden, so that saving can't rebuild the pack by accident.
class EditorInspectorPluginPCKPackage : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginPCKPackage, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

class PCKPackageEditorPlugin : public EditorPlugin {
	GDCLASS(PCKPackageEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "PCKPackage"; }

	PCKPackageEditorPlugin();
};
