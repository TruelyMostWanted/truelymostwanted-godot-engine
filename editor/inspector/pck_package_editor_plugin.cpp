/**************************************************************************/
/*  pck_package_editor_plugin.cpp                                         */
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

#include "pck_package_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/pck_package_server.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/editor_export.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"
#include "scene/scene_string_names.h"

enum {
	COLUMN_TARGET,
	COLUMN_SOURCE,
	COLUMN_SIZE,
	COLUMN_ENCRYPT,
	COLUMN_COUNT,
};

// PCKPackageEditorHelper

String PCKPackageEditorHelper::to_target_path(const String &p_source, const String &p_root) {
	// A folder is not a target.
	if (p_source.ends_with("/") || p_source.ends_with("\\")) {
		return String();
	}
	String path = p_source.replace_char('\\', '/').simplify_path();
	String root = p_root.replace_char('\\', '/').simplify_path();
	if (!root.is_empty() && !root.ends_with("/")) {
		root += "/";
	}
	if (!root.is_empty() && path.begins_with(root)) {
		path = path.substr(root.length());
	}
	path = path.trim_prefix("res://");
	// Any other absolute path (`user://`, `C:/`) can't be turned into a target.
	if (path.contains("://") || path.get_slice("/", 0).contains(":")) {
		return String();
	}
	while (path.begins_with("/")) {
		path = path.substr(1);
	}
	if (path.is_empty() || path.ends_with("/")) {
		return String();
	}
	for (const String &segment : path.split("/")) {
		if (segment.is_empty() || segment == "." || segment == "..") {
			return String();
		}
	}
	return path;
}

bool PCKPackageEditorHelper::is_packable(const String &p_target) {
	if (p_target.is_empty() || p_target.ends_with(".import") || p_target.ends_with(".uid")) {
		return false;
	}
	// Hidden files and folders, `.godot/` included.
	for (const String &segment : p_target.split("/")) {
		if (segment.begins_with(".")) {
			return false;
		}
	}
	return true;
}

static void _collect_files(const String &p_dir, PackedStringArray &r_files) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	PackedStringArray files = da->get_files();
	PackedStringArray dirs = da->get_directories();
	files.sort();
	dirs.sort();
	for (const String &file : files) {
		if (!file.begins_with(".")) {
			r_files.push_back(p_dir.path_join(file));
		}
	}
	for (const String &dir : dirs) {
		if (!dir.begins_with(".")) {
			_collect_files(p_dir.path_join(dir), r_files);
		}
	}
}

PackedStringArray PCKPackageEditorHelper::collect_files(const String &p_dir) {
	PackedStringArray result;
	_collect_files(p_dir, result);
	return result;
}

int PCKPackageEditorHelper::find_file(const Array &p_files, const String &p_target) {
	for (int i = 0; i < p_files.size(); i++) {
		if (p_files[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary entry = p_files[i];
		if (String(entry.get("path", String())) == p_target) {
			return i;
		}
	}
	return -1;
}

Array PCKPackageEditorHelper::add_sources(const Array &p_files, const PackedStringArray &p_sources, const String &p_root, bool p_encrypt, int *r_added) {
	Array result = p_files.duplicate(true);
	int added = 0;
	for (const String &source : p_sources) {
		const String source_path = source.replace_char('\\', '/');
		PackedStringArray source_files;
		if (source_path.ends_with("/") || DirAccess::dir_exists_absolute(source_path)) {
			source_files = collect_files(source_path);
		} else {
			source_files.push_back(source_path);
		}
		for (const String &file : source_files) {
			const String target = to_target_path(file, p_root);
			if (target.is_empty() || !is_packable(target)) {
				continue;
			}
			Dictionary entry;
			entry["path"] = target;
			entry["source"] = file;
			if (p_encrypt) {
				entry["encrypt"] = true;
			}
			const int index = find_file(result, target);
			if (index >= 0) {
				result[index] = entry;
			} else {
				result.push_back(entry);
			}
			added++;
		}
	}
	if (r_added) {
		*r_added = added;
	}
	return result;
}

Array PCKPackageEditorHelper::remove_files(const Array &p_files, const PackedStringArray &p_targets) {
	PackedStringArray targets;
	for (const String &target : p_targets) {
		targets.push_back(to_target_path(target));
	}
	Array result;
	for (int i = 0; i < p_files.size(); i++) {
		if (p_files[i].get_type() == Variant::DICTIONARY) {
			const Dictionary entry = p_files[i];
			if (targets.has(String(entry.get("path", String())))) {
				continue;
			}
		}
		result.push_back(p_files[i].duplicate(true));
	}
	return result;
}

Array PCKPackageEditorHelper::rename_file(const Array &p_files, const String &p_from, const String &p_to, Error *r_error) {
	const String from = to_target_path(p_from);
	const String to = to_target_path(p_to);
	Error err = OK;
	const int index = find_file(p_files, from);
	if (to.is_empty()) {
		err = ERR_INVALID_PARAMETER;
	} else if (index < 0) {
		err = ERR_DOES_NOT_EXIST;
	} else if (from != to && find_file(p_files, to) >= 0) {
		err = ERR_ALREADY_EXISTS;
	}
	if (r_error) {
		*r_error = err;
	}
	Array result = p_files.duplicate(true);
	if (err == OK && from != to) {
		Dictionary entry = result[index];
		entry["path"] = to;
	}
	return result;
}

Array PCKPackageEditorHelper::move_sources(const Array &p_files, const Dictionary &p_moves, int *r_moved) {
	Array result = p_files.duplicate(true);
	int moved = 0;
	for (int i = 0; i < result.size(); i++) {
		Dictionary entry = result[i];
		const Variant *new_source = entry.has("source") ? p_moves.getptr(entry["source"]) : nullptr;
		if (new_source) {
			entry["source"] = *new_source;
			moved++;
		}
	}
	if (r_moved) {
		*r_moved = moved;
	}
	return result;
}

Array PCKPackageEditorHelper::set_encrypt(const Array &p_files, const String &p_target, bool p_encrypt) {
	Array result = p_files.duplicate(true);
	const int index = find_file(result, to_target_path(p_target));
	if (index >= 0) {
		Dictionary entry = result[index];
		if (p_encrypt) {
			entry["encrypt"] = true;
		} else {
			entry.erase("encrypt");
		}
	}
	return result;
}

PackedStringArray PCKPackageEditorHelper::add_removals(const PackedStringArray &p_removals, const PackedStringArray &p_paths) {
	PackedStringArray result = p_removals;
	for (const String &path : p_paths) {
		const String target = to_target_path(path);
		if (!target.is_empty() && !result.has(target)) {
			result.push_back(target);
		}
	}
	return result;
}

PackedStringArray PCKPackageEditorHelper::remove_removals(const PackedStringArray &p_removals, const PackedStringArray &p_targets) {
	PackedStringArray targets;
	for (const String &target : p_targets) {
		targets.push_back(to_target_path(target));
	}
	PackedStringArray result;
	for (const String &removal : p_removals) {
		if (!targets.has(removal)) {
			result.push_back(removal);
		}
	}
	return result;
}

// PCKPackageEditor

void PCKPackageEditor::_queue_update() {
	if (update_queued) {
		return;
	}
	update_queued = true;
	callable_mp(this, &PCKPackageEditor::_update_tree).call_deferred();
}

void PCKPackageEditor::_update_tree() {
	update_queued = false;
	tree->clear();
	tree->set_column_title(COLUMN_TARGET, TTR("Target Path"));
	tree->set_column_title(COLUMN_SOURCE, read_only ? TTR("MD5") : TTR("Source"));
	tree->set_column_title(COLUMN_SIZE, TTR("Size"));
	tree->set_column_title(COLUMN_ENCRYPT, TTR("Encrypt"));

	const Color error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	TreeItem *root = tree->create_item();
	TreeItem *files_item = tree->create_item(root);
	TreeItem *removals_item = nullptr;
	int file_count = 0;
	int removal_count = 0;
	int missing_count = 0;
	uint64_t total_size = 0;

	for (const KeyValue<String, PCKPackage::Entry> &kv : package->get_entry_map()) {
		const PCKPackage::Entry &entry = kv.value;
		if (entry.removal) {
			if (!removals_item) {
				removals_item = tree->create_item(root);
			}
			TreeItem *item = tree->create_item(removals_item);
			item->set_text(COLUMN_TARGET, kv.key);
			item->set_metadata(COLUMN_TARGET, kv.key);
			item->set_text(COLUMN_SOURCE, TTR("Removed from earlier packs"));
			removal_count++;
			continue;
		}

		TreeItem *item = tree->create_item(files_item);
		item->set_text(COLUMN_TARGET, kv.key);
		item->set_metadata(COLUMN_TARGET, kv.key);
		item->set_editable(COLUMN_TARGET, !read_only);
		item->set_tooltip_text(COLUMN_TARGET, "res://" + kv.key);

		String source;
		int64_t size = -1;
		switch (entry.source_type) {
			case PCKPackage::SOURCE_FILE: {
				source = entry.source_path;
				Ref<FileAccess> f = FileAccess::open(entry.source_path, FileAccess::READ);
				if (f.is_valid()) {
					size = f->get_length();
				} else {
					item->set_custom_color(COLUMN_SOURCE, error_color);
					item->set_tooltip_text(COLUMN_SOURCE, vformat(TTR("The source file \"%s\" is missing."), entry.source_path));
					missing_count++;
				}
			} break;
			case PCKPackage::SOURCE_DATA: {
				source = TTR("(data in memory)");
				size = entry.size;
			} break;
			case PCKPackage::SOURCE_PACK: {
				source = vformat(TTR("From pack: %s"), entry.source_path);
				if (!entry.pending) {
					size = entry.size;
				}
			} break;
		}
		if (read_only) {
			source = String::hex_encode_buffer(entry.md5, 16);
		}
		item->set_text(COLUMN_SOURCE, source);
		if (item->get_tooltip_text(COLUMN_SOURCE).is_empty()) {
			item->set_tooltip_text(COLUMN_SOURCE, source);
		}
		if (size >= 0) {
			item->set_text(COLUMN_SIZE, String::humanize_size((uint64_t)size));
			total_size += (uint64_t)size;
		}
		item->set_text_alignment(COLUMN_SIZE, HORIZONTAL_ALIGNMENT_RIGHT);
		item->set_cell_mode(COLUMN_ENCRYPT, TreeItem::CELL_MODE_CHECK);
		item->set_checked(COLUMN_ENCRYPT, entry.encrypted);
		item->set_editable(COLUMN_ENCRYPT, !read_only);
		file_count++;
	}

	files_item->set_text(COLUMN_TARGET, vformat(TTR("Files (%d)"), file_count));
	if (removals_item) {
		removals_item->set_text(COLUMN_TARGET, vformat(TTR("Removals (%d)"), removal_count));
	}
	for (TreeItem *header : { files_item, removals_item }) {
		if (header) {
			for (int i = 0; i < COLUMN_COUNT; i++) {
				header->set_selectable(i, false);
			}
		}
	}

	String summary;
	if (read_only) {
		summary = vformat(TTR("Pack \"%s\" (format %d, engine %s): %d files, %s. Read-only: use \"Create Description...\" to edit a copy."), package->get_pack_path(), package->get_format_version(), package->get_engine_version(), file_count, String::humanize_size(total_size));
	} else {
		summary = vformat(TTR("Pack description: %d files, %d removals. Drop files and folders from the FileSystem dock onto the list."), file_count, removal_count);
		if (missing_count > 0) {
			summary += " " + vformat(TTR("%d source files are missing."), missing_count);
		}
	}
	summary_label->set_text(summary);
}

void PCKPackageEditor::_commit(const String &p_action, const Array &p_files, const PackedStringArray &p_removals) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, UndoRedo::MERGE_DISABLE, package.ptr());
	undo_redo->add_do_property(package.ptr(), "files", p_files);
	undo_redo->add_do_property(package.ptr(), "removals", p_removals);
	undo_redo->add_undo_property(package.ptr(), "files", package->get("files"));
	undo_redo->add_undo_property(package.ptr(), "removals", package->get("removals"));
	undo_redo->commit_action();
}

void PCKPackageEditor::_add_sources(const PackedStringArray &p_sources) {
	int added = 0;
	const Array files = PCKPackageEditorHelper::add_sources(package->get("files"), p_sources, "res://", false, &added);
	if (added == 0) {
		EditorToaster::get_singleton()->popup_str(TTR("No packable files: \".import\" and \".uid\" files, hidden files and \".godot/\" are skipped."), EditorToaster::SEVERITY_WARNING);
		return;
	}
	// A path is either a file or a removal of the pack.
	const PackedStringArray removals = package->get("removals");
	PackedStringArray replaced;
	for (const String &removal : removals) {
		if (PCKPackageEditorHelper::find_file(files, removal) >= 0) {
			replaced.push_back(removal);
		}
	}
	_commit(TTR("Add Files to Pack"), files, PCKPackageEditorHelper::remove_removals(removals, replaced));
}

void PCKPackageEditor::_file_moved(const String &p_old_path, const String &p_new_path) {
	// A folder move reports every file: follow them all in one undo step.
	if (moved_sources.is_empty()) {
		callable_mp(this, &PCKPackageEditor::_follow_moved_sources).call_deferred();
	}
	moved_sources[p_old_path] = p_new_path;
}

void PCKPackageEditor::_follow_moved_sources() {
	int moved = 0;
	const Array files = PCKPackageEditorHelper::move_sources(package->get("files"), moved_sources, &moved);
	moved_sources.clear();
	if (moved > 0) {
		_commit(TTR("Follow Moved Pack Sources"), files, package->get("removals"));
	}
}

void PCKPackageEditor::_popup_file_dialog(int p_action) {
	file_action = FileAction(p_action);
	file_dialog->clear_filters();
	switch (file_action) {
		case FILE_ACTION_ADD_FILES: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
			file_dialog->set_title(TTR("Add Files to Pack"));
		} break;
		case FILE_ACTION_ADD_FOLDER: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
			file_dialog->set_title(TTR("Add Folder to Pack"));
		} break;
		case FILE_ACTION_ADD_REMOVALS: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
			file_dialog->set_title(TTR("Remove Files of Earlier Packs"));
		} break;
		case FILE_ACTION_BUILD_PATH: {
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
			file_dialog->set_title(TTR("Build Pack"));
			file_dialog->add_filter("*.pck", TTR("Godot Pack"));
			file_dialog->set_current_path(ProjectSettings::get_singleton()->globalize_path(build_path_edit->get_text().strip_edges()));
		} break;
		case FILE_ACTION_CREATE_DESCRIPTION: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
			file_dialog->set_title(TTR("Create Pack Description"));
			file_dialog->add_filter("*.tres", TTR("Text Resource"));
			file_dialog->add_filter("*.res", TTR("Binary Resource"));
			const String pack_path = ProjectSettings::get_singleton()->localize_path(package->get_pack_path());
			file_dialog->set_current_path(pack_path.begins_with("res://") ? pack_path.get_basename() + ".tres" : String("res://pack_description.tres"));
		} break;
	}
	file_dialog->popup_file_dialog();
}

void PCKPackageEditor::_files_selected(const PackedStringArray &p_paths) {
	if (file_action == FILE_ACTION_ADD_FILES) {
		_add_sources(p_paths);
	} else if (file_action == FILE_ACTION_ADD_REMOVALS) {
		// A path is either a file or a removal of the pack.
		const Array files = PCKPackageEditorHelper::remove_files(package->get("files"), p_paths);
		_commit(TTR("Add Removals to Pack"), files, PCKPackageEditorHelper::add_removals(package->get("removals"), p_paths));
	}
}

void PCKPackageEditor::_file_selected(const String &p_path) {
	if (file_action == FILE_ACTION_BUILD_PATH) {
		build_path_edit->set_text(ProjectSettings::get_singleton()->localize_path(p_path));
	} else if (file_action == FILE_ACTION_CREATE_DESCRIPTION) {
		_create_description(p_path);
	}
}

void PCKPackageEditor::_dir_selected(const String &p_dir) {
	if (file_action == FILE_ACTION_ADD_FOLDER) {
		_add_sources(PackedStringArray({ p_dir }));
	}
}

void PCKPackageEditor::_remove_selected() {
	if (read_only) {
		return;
	}
	PackedStringArray files;
	PackedStringArray removals;
	for (TreeItem *item = tree->get_next_selected(nullptr); item; item = tree->get_next_selected(item)) {
		const Variant meta = item->get_metadata(COLUMN_TARGET);
		if (meta.get_type() != Variant::STRING) {
			continue;
		}
		const String target = meta;
		const PCKPackage::Entry *entry = package->get_entry_map().getptr(target);
		if (entry && entry->removal) {
			removals.push_back(target);
		} else {
			files.push_back(target);
		}
	}
	if (files.is_empty() && removals.is_empty()) {
		return;
	}
	_commit(TTR("Remove Files from Pack"), PCKPackageEditorHelper::remove_files(package->get("files"), files), PCKPackageEditorHelper::remove_removals(package->get("removals"), removals));
}

void PCKPackageEditor::_tree_item_edited() {
	TreeItem *item = tree->get_edited();
	if (!item || read_only) {
		return;
	}
	const String target = item->get_metadata(COLUMN_TARGET);
	const int column = tree->get_edited_column();
	if (column == COLUMN_TARGET) {
		const String new_target = item->get_text(COLUMN_TARGET).strip_edges();
		Error err = OK;
		const Array files = PCKPackageEditorHelper::rename_file(package->get("files"), target, new_target, &err);
		if (err != OK) {
			if (err == ERR_ALREADY_EXISTS) {
				EditorToaster::get_singleton()->popup_str(vformat(TTR("The pack already has a file \"%s\"."), new_target), EditorToaster::SEVERITY_WARNING);
			} else {
				EditorToaster::get_singleton()->popup_str(vformat(TTR("Invalid target path \"%s\"."), new_target), EditorToaster::SEVERITY_WARNING);
			}
			_queue_update();
			return;
		}
		if (PCKPackageEditorHelper::to_target_path(new_target) == target) {
			_queue_update();
			return;
		}
		_commit(TTR("Rename File in Pack"), files, package->get("removals"));
	} else if (column == COLUMN_ENCRYPT) {
		const Array files = PCKPackageEditorHelper::set_encrypt(package->get("files"), target, item->is_checked(COLUMN_ENCRYPT));
		_commit(TTR("Set Encryption of File in Pack"), files, package->get("removals"));
	}
}

void PCKPackageEditor::_popup_build_dialog() {
	if (build_path_edit->get_text().strip_edges().is_empty()) {
		const String resource_path = package->get_path();
		const bool is_file = resource_path.begins_with("res://") && !resource_path.contains("::");
		build_path_edit->set_text(is_file ? resource_path.get_basename() + ".pck" : String("res://package.pck"));
	}
	build_status_label->set_text(String());
	build_dialog->popup_centered(Size2(560, 0) * EDSCALE);
}

void PCKPackageEditor::_use_preset_key() {
	EditorExport *exporter = EditorExport::get_singleton();
	if (exporter) {
		for (int i = 0; i < exporter->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> preset = exporter->get_export_preset(i);
			if (preset.is_null()) {
				continue;
			}
			const Vector<uint8_t> preset_key = preset->resolve_script_encryption_key();
			if (preset_key.size() == 32) {
				build_key_edit->set_text(String::hex_encode_buffer(preset_key.ptr(), preset_key.size()));
				build_status_label->set_text(vformat(TTR("Key of the export preset \"%s\"."), preset->get_name()));
				return;
			}
		}
	}
	build_status_label->set_text(TTR("No export preset has an encryption key."));
}

void PCKPackageEditor::_build_progress(const String &p_path, int p_done, int p_total) {
	if (build_progress) {
		build_progress->step(p_path, p_done);
	}
}

void PCKPackageEditor::_build() {
	const String target = build_path_edit->get_text().strip_edges();
	// The key is used once and never kept.
	const String key_text = build_key_edit->get_text().strip_edges();
	build_key_edit->clear();
	if (target.is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("Choose the path of the pack to build."));
		return;
	}
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	const Callable progress_callable = callable_mp(this, &PCKPackageEditor::_build_progress);
	build_progress = memnew(EditorProgress("build_pck", TTR("Building Pack"), package->get_entry_map().size()));
	server->connect(SNAME("build_progress"), progress_callable);
	const Error err = server->build(package, target, key_text);
	server->disconnect(SNAME("build_progress"), progress_callable);
	memdelete(build_progress);
	build_progress = nullptr;
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Building the pack \"%s\" failed: %s.\nSee the Output panel for details."), target, error_names[err]));
		return;
	}
	const String local_path = ProjectSettings::get_singleton()->localize_path(target);
	if (local_path.begins_with("res://") && EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(local_path);
	}
	EditorToaster::get_singleton()->popup_str(vformat(TTR("Built pack \"%s\"."), local_path));
}

void PCKPackageEditor::_create_description(const String &p_path) {
	Ref<PCKPackage> description;
	description.instantiate();
	description->set_base_pack(ProjectSettings::get_singleton()->localize_path(package->get_pack_path()));
	description->set_alignment(package->get_alignment());
	description->set_encrypted_directory(package->is_encrypted_directory());
	// Every file of the opened pack becomes a `from_pack` entry of the base pack.
	description->set("files", package->get("files"));
	description->set("removals", package->get("removals"));
	const Error err = ResourceSaver::save(description, p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Saving the pack description \"%s\" failed: %s."), p_path, error_names[err]));
		return;
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	}
	const Ref<Resource> saved = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_REPLACE);
	if (saved.is_valid()) {
		// Deferred: editing another resource frees this editor.
		callable_mp(EditorInterface::get_singleton(), &EditorInterface::edit_resource).call_deferred(saved);
	}
}

bool PCKPackageEditor::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (read_only || p_data.get_type() != Variant::DICTIONARY) {
		return false;
	}
	const Dictionary drag_data = p_data;
	const String type = drag_data.get("type", String());
	return (type == "files" || type == "files_and_dirs") && drag_data.has("files");
}

void PCKPackageEditor::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!_can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}
	const Dictionary drag_data = p_data;
	_add_sources(drag_data.get("files", PackedStringArray()));
}

void PCKPackageEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Sources moved, renamed, added or deleted outside of the Inspector.
			if (EditorFileSystem::get_singleton()) {
				EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &PCKPackageEditor::_queue_update));
			}
			if (!read_only && FileSystemDock::get_singleton()) {
				FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &PCKPackageEditor::_file_moved));
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (EditorFileSystem::get_singleton()) {
				EditorFileSystem::get_singleton()->disconnect("filesystem_changed", callable_mp(this, &PCKPackageEditor::_queue_update));
			}
			if (!read_only && FileSystemDock::get_singleton()) {
				FileSystemDock::get_singleton()->disconnect("files_moved", callable_mp(this, &PCKPackageEditor::_file_moved));
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			add_files_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			add_folder_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
			remove_button->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			build_button->set_button_icon(get_editor_theme_icon(SNAME("PCKPackage")));
			_queue_update();
		} break;
	}
}

PCKPackageEditor::PCKPackageEditor(const Ref<PCKPackage> &p_package) {
	package = p_package;
	read_only = !package->is_description();

	summary_label = memnew(Label);
	summary_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	summary_label->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
	add_child(summary_label);

	HFlowContainer *toolbar = memnew(HFlowContainer);
	add_child(toolbar);

	add_files_button = memnew(Button(TTR("Add Files...")));
	add_files_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_file_dialog).bind(FILE_ACTION_ADD_FILES));
	toolbar->add_child(add_files_button);

	add_folder_button = memnew(Button(TTR("Add Folder...")));
	add_folder_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_file_dialog).bind(FILE_ACTION_ADD_FOLDER));
	toolbar->add_child(add_folder_button);

	remove_button = memnew(Button(TTR("Remove")));
	remove_button->set_tooltip_text(TTR("Remove the selected files and removals from the description."));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_remove_selected));
	toolbar->add_child(remove_button);

	add_removal_button = memnew(Button(TTR("Add Removal...")));
	add_removal_button->set_tooltip_text(TTR("Files that the pack deletes from earlier packs when it is mounted."));
	add_removal_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_file_dialog).bind(FILE_ACTION_ADD_REMOVALS));
	toolbar->add_child(add_removal_button);

	build_button = memnew(Button(TTR("Build Pack...")));
	build_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_build_dialog));
	toolbar->add_child(build_button);

	create_description_button = memnew(Button(TTR("Create Description...")));
	create_description_button->set_tooltip_text(TTR("Save an editable description of this pack: every file is taken from this pack."));
	create_description_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_file_dialog).bind(FILE_ACTION_CREATE_DESCRIPTION));
	toolbar->add_child(create_description_button);

	for (Button *button : { add_files_button, add_folder_button, remove_button, add_removal_button, build_button }) {
		button->set_visible(!read_only);
	}
	create_description_button->set_visible(read_only);

	tree = memnew(Tree);
	tree->set_columns(COLUMN_COUNT);
	tree->set_column_titles_visible(true);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_column_expand_ratio(COLUMN_TARGET, 3);
	tree->set_column_expand_ratio(COLUMN_SOURCE, 2);
	tree->set_column_clip_content(COLUMN_TARGET, true);
	tree->set_column_clip_content(COLUMN_SOURCE, true);
	// Narrow fixed columns: Target and Source get the rest of the dock width.
	tree->set_column_expand(COLUMN_SIZE, false);
	tree->set_column_custom_minimum_width(COLUMN_SIZE, 60 * EDSCALE);
	tree->set_column_clip_content(COLUMN_SIZE, true);
	tree->set_column_expand(COLUMN_ENCRYPT, false);
	tree->set_column_custom_minimum_width(COLUMN_ENCRYPT, 56 * EDSCALE);
	tree->set_column_clip_content(COLUMN_ENCRYPT, true);
	tree->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->connect("item_edited", callable_mp(this, &PCKPackageEditor::_tree_item_edited));
	SET_DRAG_FORWARDING_CDU(tree, PCKPackageEditor);
	add_child(tree);

	file_dialog = memnew(EditorFileDialog);
	file_dialog->connect("files_selected", callable_mp(this, &PCKPackageEditor::_files_selected));
	file_dialog->connect("file_selected", callable_mp(this, &PCKPackageEditor::_file_selected));
	file_dialog->connect("dir_selected", callable_mp(this, &PCKPackageEditor::_dir_selected));
	add_child(file_dialog);

	build_dialog = memnew(ConfirmationDialog);
	build_dialog->set_title(TTR("Build Pack"));
	build_dialog->set_ok_button_text(TTR("Build"));
	build_dialog->connect(SceneStringName(confirmed), callable_mp(this, &PCKPackageEditor::_build));
	add_child(build_dialog);

	VBoxContainer *build_vb = memnew(VBoxContainer);
	build_dialog->add_child(build_vb);

	Label *path_label = memnew(Label(TTR("Pack file:")));
	build_vb->add_child(path_label);
	HBoxContainer *path_hb = memnew(HBoxContainer);
	build_vb->add_child(path_hb);
	build_path_edit = memnew(LineEdit);
	build_path_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	build_path_edit->set_accessibility_name(TTRC("Pack file"));
	path_hb->add_child(build_path_edit);
	build_dialog->register_text_enter(build_path_edit);
	Button *browse_button = memnew(Button(TTR("Browse...")));
	browse_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_popup_file_dialog).bind(FILE_ACTION_BUILD_PATH));
	path_hb->add_child(browse_button);

	Label *key_label = memnew(Label(TTR("Key (64 hexadecimal characters; empty for the key compiled into the engine). It is never stored:")));
	key_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	key_label->set_custom_minimum_size(Size2(400, 0) * EDSCALE);
	build_vb->add_child(key_label);
	HBoxContainer *key_hb = memnew(HBoxContainer);
	build_vb->add_child(key_hb);
	build_key_edit = memnew(LineEdit);
	build_key_edit->set_secret(true);
	build_key_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	build_key_edit->set_accessibility_name(TTRC("Pack key"));
	key_hb->add_child(build_key_edit);
	Button *preset_key_button = memnew(Button(TTR("Use Key from Export Preset")));
	preset_key_button->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageEditor::_use_preset_key));
	key_hb->add_child(preset_key_button);

	build_status_label = memnew(Label);
	build_status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	build_status_label->set_custom_minimum_size(Size2(400, 0) * EDSCALE);
	build_vb->add_child(build_status_label);

	package->connect_changed(callable_mp(this, &PCKPackageEditor::_queue_update));
}

// EditorInspectorPluginPCKPackage

bool EditorInspectorPluginPCKPackage::can_handle(Object *p_object) {
	return Object::cast_to<PCKPackage>(p_object) != nullptr;
}

void EditorInspectorPluginPCKPackage::parse_category(Object *p_object, const String &p_category) {
	// Below the class header of PCKPackage, like the class's own properties.
	if (p_category != "PCKPackage") {
		return;
	}
	Ref<PCKPackage> package(Object::cast_to<PCKPackage>(p_object));
	ERR_FAIL_COND(package.is_null());
	add_custom_control(memnew(PCKPackageEditor(package)));
}

bool EditorInspectorPluginPCKPackage::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	const PCKPackage *package = Object::cast_to<PCKPackage>(p_object);
	if (!package) {
		return false;
	}
	if (package->is_description()) {
		// Only an opened pack has these.
		return p_path == "pack_path" || p_path == "format_version" || p_path == "engine_version";
	}
	// An opened pack is read-only: an edit followed by a save would rebuild it.
	return p_path == "base_pack" || p_path == "alignment" || p_path == "encrypted_directory";
}

// PCKPackageEditorPlugin

PCKPackageEditorPlugin::PCKPackageEditorPlugin() {
	Ref<EditorInspectorPluginPCKPackage> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
