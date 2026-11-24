/**************************************************************************/
/*  pck_export_plugin.cpp                                                 */
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

#include "pck_export_plugin.h"

#include "core/io/pck_package.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/export/pck_package_exporter.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/main/scene_tree.h"
#include "scene/scene_string_names.h"

// PCKExportPlugin

void PCKExportPlugin::_add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const {
	const Ref<EditorExportPlatform> platform = get_export_platform();
	if (platform.is_valid()) {
		platform->add_message(p_type, TTR("PCK Packages"), p_message);
	} else if (p_type == EditorExportPlatform::EXPORT_MESSAGE_ERROR) {
		ERR_PRINT("PCK Packages: " + p_message);
	} else if (p_type == EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
		WARN_PRINT("PCK Packages: " + p_message);
	}
}

void PCKExportPlugin::_get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const {
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, OPTION_EXCLUDED_DESCRIPTIONS, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:*.tres,*.res", Variant::STRING, PROPERTY_HINT_FILE)), PackedStringArray()));
}

void PCKExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	export_targets.clear();
	excluded_sources.clear();
	if (!begun) {
		begun = true;
		PCKPackageExporter::notify_export_begin();
	}

	for (const String &target : PCKPackageExporter::get_export_target_paths()) {
		export_targets.insert(target);
	}

	// The sources of excluded descriptions stay in their own pack. While a description is built, they are the content.
	if (PCKPackageExporter::is_building() || get_export_preset().is_null()) {
		return;
	}
	const PackedStringArray descriptions = get_option(OPTION_EXCLUDED_DESCRIPTIONS);
	for (const String &description_path : descriptions) {
		if (description_path.strip_edges().is_empty()) {
			continue;
		}
		Ref<PCKPackage> description = ResourceLoader::load(description_path, "PCKPackage");
		if (description.is_null()) {
			_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, vformat(TTR("Can't load the pack description \"%s\" listed in \"%s\". Its files are exported."), description_path, OPTION_EXCLUDED_DESCRIPTIONS));
			continue;
		}
		const Ref<PCKBuildPlan> plan = PCKPackageExporter::plan_description(description);
		for (const String &source : plan->get_export_sources()) {
			excluded_sources.insert(source);
		}
	}
}

void PCKExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	if (export_targets.has(p_path)) {
		skip();
		return;
	}
	if (excluded_sources.has(p_path)) {
		skip();
		return;
	}
	if (p_path.get_extension().to_lower() == "pck" && PCKPackageExporter::should_encrypt(get_export_preset(), p_path)) {
		_add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, vformat(TTR("\"%s\" is a pack that the preset encrypts. An encrypted pack inside the main pack is decrypted whole every time one of its files is opened. Exclude \"*.pck\" from the encryption filters."), p_path));
	}
}

void PCKExportPlugin::_export_end() {
	export_targets.clear();
	excluded_sources.clear();
	if (begun) {
		begun = false;
		PCKPackageExporter::notify_export_end();
	}
}

// PCKPackageExportDialog

void PCKPackageExportDialog::_browse_description() {
	description_dialog->popup_file_dialog();
}

void PCKPackageExportDialog::_browse_target() {
	target_dialog->popup_file_dialog();
}

void PCKPackageExportDialog::_description_selected(const String &p_path) {
	description_edit->set_text(p_path);
	if (target_edit->get_text().strip_edges().is_empty()) {
		target_edit->set_text("user://" + p_path.get_file().get_basename() + ".pck");
	}
	_update_ok();
}

void PCKPackageExportDialog::_target_selected(const String &p_path) {
	target_edit->set_text(p_path);
	_update_ok();
}

void PCKPackageExportDialog::_update_ok() {
	get_ok_button()->set_disabled(description_edit->get_text().strip_edges().is_empty() || target_edit->get_text().strip_edges().is_empty() || preset_option->get_item_count() == 0);
}

void PCKPackageExportDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		description_browse->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
		target_browse->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
	}
}

void PCKPackageExportDialog::ok_pressed() {
	const String description_path = description_edit->get_text().strip_edges();
	const String target = target_edit->get_text().strip_edges();
	const String preset_name = preset_option->get_item_count() > 0 ? preset_option->get_item_text(preset_option->get_selected()) : String();

	Ref<PCKPackage> description = ResourceLoader::load(description_path, "PCKPackage");
	if (description.is_null()) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Can't load the pack description \"%s\"."), description_path), EditorToaster::SEVERITY_ERROR);
		return;
	}
	const Error err = PCKPackageExporter::export_description(description, preset_name, target, debug_check->is_pressed(), dependencies_check->is_pressed());
	if (err == OK) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Built pack \"%s\" with preset \"%s\"."), target, preset_name), EditorToaster::SEVERITY_INFO);
		hide();
	} else {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Can't build pack \"%s\" (%s). See the Output panel."), target, error_names[err]), EditorToaster::SEVERITY_ERROR);
	}
}

void PCKPackageExportDialog::popup_for_description(const String &p_path) {
	preset_option->clear();
	for (const String &preset_name : PCKPackageExporter::get_preset_names()) {
		preset_option->add_item(preset_name);
	}
	if (!p_path.is_empty()) {
		description_edit->set_text(p_path);
		target_edit->set_text("user://" + p_path.get_file().get_basename() + ".pck");
	}
	_update_ok();
	popup_centered(Size2(500, 0) * EDSCALE);
}

PCKPackageExportDialog::PCKPackageExportDialog() {
	set_title(TTR("Build Pack from Description"));
	set_ok_button_text(TTR("Build"));
	set_hide_on_ok(false);

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	Label *info = memnew(Label(TTR("Builds a pack from a PCKPackage description with the export pipeline of a preset: res:// files are packed in exported form for the preset's platform.")));
	info->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	info->set_custom_minimum_size(Size2(400, 0) * EDSCALE);
	main_vb->add_child(info);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	main_vb->add_child(grid);

	grid->add_child(memnew(Label(TTR("Description:"))));
	HBoxContainer *description_hb = memnew(HBoxContainer);
	description_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(description_hb);
	description_edit = memnew(LineEdit);
	description_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	description_edit->set_accessibility_name(TTRC("Description"));
	description_edit->connect(SceneStringName(text_changed), callable_mp(this, &PCKPackageExportDialog::_update_ok).unbind(1));
	description_hb->add_child(description_edit);
	description_browse = memnew(Button);
	description_browse->set_accessibility_name(TTRC("Browse"));
	description_browse->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageExportDialog::_browse_description));
	description_hb->add_child(description_browse);

	grid->add_child(memnew(Label(TTR("Export Preset:"))));
	preset_option = memnew(OptionButton);
	preset_option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preset_option->set_accessibility_name(TTRC("Export Preset"));
	grid->add_child(preset_option);

	grid->add_child(memnew(Label(TTR("Target Pack:"))));
	HBoxContainer *target_hb = memnew(HBoxContainer);
	target_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	grid->add_child(target_hb);
	target_edit = memnew(LineEdit);
	target_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	target_edit->set_accessibility_name(TTRC("Target Pack"));
	target_edit->connect(SceneStringName(text_changed), callable_mp(this, &PCKPackageExportDialog::_update_ok).unbind(1));
	target_hb->add_child(target_edit);
	target_browse = memnew(Button);
	target_browse->set_accessibility_name(TTRC("Browse"));
	target_browse->connect(SceneStringName(pressed), callable_mp(this, &PCKPackageExportDialog::_browse_target));
	target_hb->add_child(target_browse);

	debug_check = memnew(CheckBox(TTR("Debug")));
	main_vb->add_child(debug_check);
	dependencies_check = memnew(CheckBox(TTR("Include Dependencies")));
	dependencies_check->set_pressed(true);
	dependencies_check->set_tooltip_text(TTR("Also pack the files the listed resources depend on. Turn it off if the main game already ships them."));
	main_vb->add_child(dependencies_check);

	description_dialog = memnew(EditorFileDialog);
	description_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	description_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	description_dialog->add_filter("*.tres,*.res", TTR("Pack Descriptions"));
	description_dialog->connect("file_selected", callable_mp(this, &PCKPackageExportDialog::_description_selected));
	add_child(description_dialog);

	target_dialog = memnew(EditorFileDialog);
	target_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	target_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	target_dialog->add_filter("*.pck", TTR("Godot Packs"));
	target_dialog->connect("file_selected", callable_mp(this, &PCKPackageExportDialog::_target_selected));
	add_child(target_dialog);
}

// PCKPackageExporterEditorPlugin

void PCKPackageExporterEditorPlugin::_open_dialog() {
	dialog->popup_for_description();
}

void PCKPackageExporterEditorPlugin::_filesystem_changed() {
	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (!PCKPackageExporter::has_command_line_request() || !file_system || file_system->is_scanning()) {
		return;
	}
	// The request is cleared before it runs, so later scans don't run it again.
	const Error err = PCKPackageExporter::run_command_line_request();
	if (err != OK) {
		ERR_PRINT(vformat("--export-pack-description failed: %s.", error_names[err]));
	}
	get_tree()->quit(err == OK ? EXIT_SUCCESS : EXIT_FAILURE);
}

void PCKPackageExporterEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_export_plugin(export_plugin);
			add_tool_menu_item(TTR("Build Pack from Description..."), callable_mp(this, &PCKPackageExporterEditorPlugin::_open_dialog));
			// The command line request is made after the editor entered the tree: always listen, it is a cheap check.
			EditorFileSystem *file_system = EditorFileSystem::get_singleton();
			const Callable on_changed = callable_mp(this, &PCKPackageExporterEditorPlugin::_filesystem_changed);
			if (file_system && !file_system->is_connected("filesystem_changed", on_changed)) {
				file_system->connect("filesystem_changed", on_changed);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_tool_menu_item(TTR("Build Pack from Description..."));
			remove_export_plugin(export_plugin);
			EditorFileSystem *file_system = EditorFileSystem::get_singleton();
			const Callable on_changed = callable_mp(this, &PCKPackageExporterEditorPlugin::_filesystem_changed);
			if (file_system && file_system->is_connected("filesystem_changed", on_changed)) {
				file_system->disconnect("filesystem_changed", on_changed);
			}
		} break;
	}
}

PCKPackageExporterEditorPlugin::PCKPackageExporterEditorPlugin() {
	export_plugin.instantiate();
	dialog = memnew(PCKPackageExportDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(dialog);
}
