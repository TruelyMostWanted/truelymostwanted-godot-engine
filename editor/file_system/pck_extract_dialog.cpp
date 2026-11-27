/**************************************************************************/
/*  pck_extract_dialog.cpp                                                */
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

#include "pck_extract_dialog.h"

#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tree.h"
#include "scene/scene_string_names.h"

enum {
	COLUMN_PATH,
	COLUMN_SIZE,
	COLUMN_STATUS,
};

bool PCKExtractDialog::_is_engine_file(const String &p_key) {
	return p_key.begins_with(".godot/");
}

bool PCKExtractDialog::_is_not_editable(const String &p_key) {
	return p_key == "project.binary" || p_key.ends_with(".remap");
}

void PCKExtractDialog::_open_package() {
	package.unref();
	status_label->set_text(String());
	if (!opened_path.is_empty()) {
		package = PCKPackageServer::get_singleton()->open(opened_path, key_edit->get_text().strip_edges());
		if (package.is_null()) {
			status_label->set_text(TTR("The pack can't be opened. If its directory is encrypted, enter the key and press \"Open\"."));
		}
	}
	_update_tree();
}

void PCKExtractDialog::_update_tree() {
	updating = true;
	tree->clear();
	TreeItem *root = tree->create_item();
	if (package.is_valid()) {
		const bool skip_engine = skip_engine_check->is_pressed();
		for (const KeyValue<String, PCKPackage::Entry> &kv : package->get_entry_map()) {
			if (kv.value.removal) {
				continue;
			}
			TreeItem *item = tree->create_item(root);
			item->set_cell_mode(COLUMN_PATH, TreeItem::CELL_MODE_CHECK);
			item->set_editable(COLUMN_PATH, true);
			item->set_text(COLUMN_PATH, "res://" + kv.key);
			item->set_metadata(COLUMN_PATH, "res://" + kv.key);
			item->set_text(COLUMN_SIZE, String::humanize_size(kv.value.size));
			const bool excluded = skip_engine && (_is_engine_file(kv.key) || _is_not_editable(kv.key) || kv.value.delta);
			item->set_checked(COLUMN_PATH, !excluded);
		}
	}
	updating = false;
	_update_preview();
}

void PCKExtractDialog::_update_preview() {
	const String target = target_edit->get_text().strip_edges();
	const bool skip_engine = skip_engine_check->is_pressed();
	int checked = 0;
	int conflicts = 0;
	int not_editable = 0;
	TreeItem *root = tree->get_root();
	for (TreeItem *item = root ? root->get_first_child() : nullptr; item; item = item->get_next()) {
		const String path = item->get_metadata(COLUMN_PATH);
		const String key = path.trim_prefix("res://");
		String status;
		Color color = get_theme_color(SNAME("font_color"), SNAME("Label"));
		if (skip_engine && _is_engine_file(key)) {
			status = TTR("Engine file");
			color = get_theme_color(SNAME("font_disabled_color"), SNAME("Button"));
		} else if (skip_engine && _is_not_editable(key)) {
			status = TTR("Not editable");
			color = get_theme_color(SNAME("warning_color"), SNAME("Editor"));
		} else if (!target.is_empty() && FileAccess::exists(target.path_join(key))) {
			status = TTR("Exists");
			color = get_theme_color(SNAME("warning_color"), SNAME("Editor"));
		} else {
			status = TTR("New");
		}
		item->set_text(COLUMN_STATUS, status);
		item->set_custom_color(COLUMN_STATUS, color);
		if (!item->is_checked(COLUMN_PATH)) {
			continue;
		}
		checked++;
		if (status == TTR("Exists")) {
			conflicts++;
		} else if (status == TTR("Not editable")) {
			not_editable++;
		}
	}

	String text = vformat(TTR("%d files selected."), checked);
	if (conflicts > 0) {
		static const char *policy_texts[] = { TTRC("They are skipped."), TTRC("They are overwritten."), TTRC("They are written under a new name.") };
		const int policy = CLAMP(policy_option->get_selected_id(), 0, 2);
		text += " " + vformat(TTR("%d of them already exist in the target folder."), conflicts) + " " + TTR(policy_texts[policy]);
	}
	if (not_editable > 0) {
		text += " " + vformat(TTR("%d are converted or remapped files and are skipped."), not_editable);
	}
	preview_label->set_text(text);
	get_ok_button()->set_disabled(package.is_null() || checked == 0 || target.is_empty());
}

void PCKExtractDialog::_tree_item_edited() {
	if (!updating) {
		_update_preview();
	}
}

void PCKExtractDialog::_set_all_checked(bool p_checked) {
	TreeItem *root = tree->get_root();
	for (TreeItem *item = root ? root->get_first_child() : nullptr; item; item = item->get_next()) {
		item->set_checked(COLUMN_PATH, p_checked);
	}
	_update_preview();
}

void PCKExtractDialog::_use_preset_key() {
	EditorExport *exporter = EditorExport::get_singleton();
	if (exporter) {
		for (int i = 0; i < exporter->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> preset = exporter->get_export_preset(i);
			if (preset.is_null()) {
				continue;
			}
			const Vector<uint8_t> key = preset->resolve_script_encryption_key();
			if (key.size() == 32) {
				key_edit->set_text(String::hex_encode_buffer(key.ptr(), key.size()));
				status_label->set_text(vformat(TTR("Key of the export preset \"%s\"."), preset->get_name()));
				_open_package();
				return;
			}
		}
	}
	status_label->set_text(TTR("No export preset has an encryption key."));
}

void PCKExtractDialog::_browse_target() {
	const String target = target_edit->get_text().strip_edges();
	target_dialog->set_current_dir(target.begins_with("res://") ? target : String("res://"));
	target_dialog->popup_file_dialog();
}

void PCKExtractDialog::_target_selected(const String &p_dir) {
	target_edit->set_text(p_dir);
	_update_preview();
}

void PCKExtractDialog::_target_changed(const String &p_text) {
	_update_preview();
}

void PCKExtractDialog::_option_changed(int p_index) {
	_update_preview();
}

void PCKExtractDialog::_skip_engine_toggled(bool p_pressed) {
	// Only the engine files follow the toggle: the checks of the other files stay as the user set them.
	TreeItem *root = tree->get_root();
	for (TreeItem *item = root ? root->get_first_child() : nullptr; item; item = item->get_next()) {
		const String key = String(item->get_metadata(COLUMN_PATH)).trim_prefix("res://");
		const PCKPackage::Entry *entry = package.is_valid() ? package->get_entry_map().getptr(key) : nullptr;
		if (_is_engine_file(key) || _is_not_editable(key) || (entry && entry->delta)) {
			item->set_checked(COLUMN_PATH, !p_pressed);
		}
	}
	_update_preview();
}

void PCKExtractDialog::_extract_progress(const String &p_path, int p_done, int p_total) {
	if (progress) {
		progress->step(p_path, p_done);
	}
}

void PCKExtractDialog::_show_result(const Ref<PCKExtractResult> &p_result, const String &p_target) {
	String text = vformat(TTR("%d files were written to \"%s\"."), p_result->get_written().size(), p_target);
	if (!p_result->get_renamed().is_empty()) {
		text += "\n" + vformat(TTR("%d of them under a new name."), p_result->get_renamed().size());
	}
	if (!p_result->get_skipped().is_empty()) {
		text += "\n" + vformat(TTR("%d files were skipped."), p_result->get_skipped().size());
	}
	if (!p_result->get_not_editable().is_empty()) {
		text += "\n\n" + TTR("Converted or remapped files (not editable, skipped):") + "\n" + String("\n").join(p_result->get_not_editable());
	}
	if (!p_result->get_dropped_uids().is_empty()) {
		text += "\n\n" + TTR("UIDs already used by other files (dropped, new UIDs are assigned on the next scan):") + "\n" + String("\n").join(p_result->get_dropped_uids());
	}
	if (!p_result->get_errors().is_empty()) {
		text += "\n\n" + TTR("Errors:") + "\n" + String("\n").join(p_result->get_errors());
	}
	EditorNode::get_singleton()->show_warning(text, TTR("Extract Pack"));
}

void PCKExtractDialog::ok_pressed() {
	if (package.is_null()) {
		return;
	}
	const String target = target_edit->get_text().strip_edges();
	PackedStringArray paths;
	TreeItem *root = tree->get_root();
	for (TreeItem *item = root ? root->get_first_child() : nullptr; item; item = item->get_next()) {
		if (item->is_checked(COLUMN_PATH)) {
			paths.push_back(String(item->get_metadata(COLUMN_PATH)));
		}
	}
	if (paths.is_empty() || target.is_empty()) {
		return;
	}

	PCKPackageServer *server = PCKPackageServer::get_singleton();
	const Callable progress_callable = callable_mp(this, &PCKExtractDialog::_extract_progress);
	progress = memnew(EditorProgress("extract_pck", TTR("Extracting Pack"), paths.size()));
	server->connect(SNAME("extract_progress"), progress_callable);
	const Ref<PCKExtractResult> result = server->extract(package, target, paths, PCKPackageServer::ExtractPolicy(policy_option->get_selected_id()), skip_engine_check->is_pressed());
	server->disconnect(SNAME("extract_progress"), progress_callable);
	memdelete(progress);
	progress = nullptr;

	hide();
	if (target.begins_with("res://") && EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->scan();
	}
	_show_result(result, target);
}

void PCKExtractDialog::popup_for_path(const String &p_pack_path) {
	opened_path = p_pack_path;
	key_edit->set_text(String());
	key_edit->set_editable(true);
	reopen_button->set_disabled(false);
	preset_key_button->set_disabled(false);
	pack_label->set_text(vformat(TTR("Pack: %s"), p_pack_path));
	_open_package();
	popup_centered_clamped(Size2(800, 600) * EDSCALE, 0.8);
}

void PCKExtractDialog::popup_for_package(const Ref<PCKPackage> &p_package) {
	ERR_FAIL_COND(p_package.is_null());
	opened_path = p_package->get_pack_path();
	package = p_package;
	key_edit->set_text(String());
	// The key of an opened package is already set; reopening only makes sense for a pack file.
	key_edit->set_editable(!opened_path.is_empty());
	reopen_button->set_disabled(opened_path.is_empty());
	preset_key_button->set_disabled(opened_path.is_empty());
	status_label->set_text(String());
	pack_label->set_text(opened_path.is_empty() ? TTR("Pack description") : vformat(TTR("Pack: %s"), opened_path));
	_update_tree();
	popup_centered_clamped(Size2(800, 600) * EDSCALE, 0.8);
}

void PCKExtractDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			target_browse->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
		} break;
	}
}

PCKExtractDialog::PCKExtractDialog() {
	singleton = this;
	set_title(TTR("Extract Pack"));
	set_ok_button_text(TTR("Extract"));

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	pack_label = memnew(Label);
	pack_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	main_vb->add_child(pack_label);

	HBoxContainer *key_hb = memnew(HBoxContainer);
	main_vb->add_child(key_hb);
	Label *key_label = memnew(Label(TTR("Key:")));
	key_hb->add_child(key_label);
	key_edit = memnew(LineEdit);
	key_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	key_edit->set_secret(true);
	key_edit->set_placeholder(TTR("Empty: the key compiled into the editor"));
	key_edit->set_accessibility_name(TTRC("Key"));
	key_hb->add_child(key_edit);
	preset_key_button = memnew(Button(TTR("Use Key from Export Preset")));
	preset_key_button->connect(SceneStringName(pressed), callable_mp(this, &PCKExtractDialog::_use_preset_key));
	key_hb->add_child(preset_key_button);
	reopen_button = memnew(Button(TTR("Open")));
	reopen_button->connect(SceneStringName(pressed), callable_mp(this, &PCKExtractDialog::_open_package));
	key_hb->add_child(reopen_button);

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	main_vb->add_child(status_label);

	HBoxContainer *select_hb = memnew(HBoxContainer);
	main_vb->add_child(select_hb);
	Button *select_all = memnew(Button(TTR("Select All")));
	select_all->connect(SceneStringName(pressed), callable_mp(this, &PCKExtractDialog::_set_all_checked).bind(true));
	select_hb->add_child(select_all);
	Button *select_none = memnew(Button(TTR("Select None")));
	select_none->connect(SceneStringName(pressed), callable_mp(this, &PCKExtractDialog::_set_all_checked).bind(false));
	select_hb->add_child(select_none);

	tree = memnew(Tree);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_columns(3);
	tree->set_column_titles_visible(true);
	tree->set_column_title(COLUMN_PATH, TTR("File"));
	tree->set_column_title(COLUMN_SIZE, TTR("Size"));
	tree->set_column_title(COLUMN_STATUS, TTR("Status"));
	tree->set_column_expand(COLUMN_PATH, true);
	tree->set_column_expand(COLUMN_SIZE, false);
	tree->set_column_expand(COLUMN_STATUS, false);
	tree->set_column_custom_minimum_width(COLUMN_SIZE, 90 * EDSCALE);
	tree->set_column_custom_minimum_width(COLUMN_STATUS, 120 * EDSCALE);
	tree->set_hide_root(true);
	tree->set_accessibility_name(TTRC("Files"));
	tree->connect("item_edited", callable_mp(this, &PCKExtractDialog::_tree_item_edited));
	main_vb->add_child(tree);

	HBoxContainer *target_hb = memnew(HBoxContainer);
	main_vb->add_child(target_hb);
	Label *target_label = memnew(Label(TTR("Target Folder:")));
	target_hb->add_child(target_label);
	target_edit = memnew(LineEdit);
	target_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	target_edit->set_text("res://");
	target_edit->set_accessibility_name(TTRC("Target Folder"));
	target_edit->connect(SceneStringName(text_changed), callable_mp(this, &PCKExtractDialog::_target_changed));
	target_hb->add_child(target_edit);
	target_browse = memnew(Button);
	target_browse->set_accessibility_name(TTRC("Browse"));
	target_browse->connect(SceneStringName(pressed), callable_mp(this, &PCKExtractDialog::_browse_target));
	target_hb->add_child(target_browse);

	HBoxContainer *options_hb = memnew(HBoxContainer);
	main_vb->add_child(options_hb);
	Label *policy_label = memnew(Label(TTR("Existing Files:")));
	options_hb->add_child(policy_label);
	policy_option = memnew(OptionButton);
	policy_option->add_item(TTR("Skip"), PCKPackageServer::EXTRACT_SKIP);
	policy_option->add_item(TTR("Overwrite"), PCKPackageServer::EXTRACT_OVERWRITE);
	policy_option->add_item(TTR("Rename"), PCKPackageServer::EXTRACT_RENAME);
	policy_option->set_accessibility_name(TTRC("Existing Files"));
	policy_option->connect(SceneStringName(item_selected), callable_mp(this, &PCKExtractDialog::_option_changed));
	options_hb->add_child(policy_option);
	skip_engine_check = memnew(CheckBox(TTR("Skip Engine Files")));
	skip_engine_check->set_pressed(true);
	skip_engine_check->set_tooltip_text(TTR("Skip files below .godot/, .import files without their source, .remap files and project.binary.\nExported packs contain imported and converted files, not the sources."));
	skip_engine_check->connect(SceneStringName(toggled), callable_mp(this, &PCKExtractDialog::_skip_engine_toggled));
	options_hb->add_child(skip_engine_check);

	preview_label = memnew(Label);
	preview_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	preview_label->set_custom_minimum_size(Size2(400, 0) * EDSCALE);
	main_vb->add_child(preview_label);

	target_dialog = memnew(EditorFileDialog);
	target_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	target_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	target_dialog->connect("dir_selected", callable_mp(this, &PCKExtractDialog::_target_selected));
	add_child(target_dialog);
}

PCKExtractDialog::~PCKExtractDialog() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
