/**************************************************************************/
/*  pck_export_plugin.h                                                   */
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

#include "editor/export/editor_export_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class EditorFileDialog;
class Label;
class LineEdit;
class OptionButton;

// Keeps `.pck` files out of exports where they don't belong:
// - every file that is the export target of a preset (`res://build/game.pck` is never exported into the next build);
// - the `res://` sources of the descriptions listed in the preset option `pck_packages/excluded_descriptions`
//   (content shipped as a separate pack), except while such a description is being built.
// It warns when the preset encrypts a `.pck` file: a nested encrypted pack is decrypted whole on every open.
class PCKExportPlugin : public EditorExportPlugin {
	GDCLASS(PCKExportPlugin, EditorExportPlugin);

	HashSet<String> export_targets;
	HashSet<String> excluded_sources;
	bool begun = false;

	void _add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const;

protected:
	virtual String get_name() const override { return "PCKPackages"; }

	virtual void _get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const override;
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual void _export_end() override;

public:
	static constexpr const char *OPTION_EXCLUDED_DESCRIPTIONS = "pck_packages/excluded_descriptions";
};

// Project > Tools > "Build Pack from Description...".
class PCKPackageExportDialog : public ConfirmationDialog {
	GDCLASS(PCKPackageExportDialog, ConfirmationDialog);

	LineEdit *description_edit = nullptr;
	Button *description_browse = nullptr;
	OptionButton *preset_option = nullptr;
	LineEdit *target_edit = nullptr;
	Button *target_browse = nullptr;
	CheckBox *debug_check = nullptr;
	CheckBox *dependencies_check = nullptr;
	Label *hint_label = nullptr; // Why "Build" is disabled.
	EditorFileDialog *description_dialog = nullptr;
	EditorFileDialog *target_dialog = nullptr;

	void _browse_description();
	void _browse_target();
	void _description_selected(const String &p_path);
	void _target_selected(const String &p_path);
	void _update_ok();

protected:
	void _notification(int p_what);
	virtual void ok_pressed() override;

public:
	void popup_for_description(const String &p_path = String());

	PCKPackageExportDialog();
};

// Registers PCKExportPlugin, adds the Tools menu item, and runs `--export-pack-description` after the first scan.
class PCKPackageExporterEditorPlugin : public EditorPlugin {
	GDCLASS(PCKPackageExporterEditorPlugin, EditorPlugin);

	Ref<PCKExportPlugin> export_plugin;
	PCKPackageExportDialog *dialog = nullptr;

	void _open_dialog();
	void _filesystem_changed();

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "PCKPackageExporter"; }

	PCKPackageExporterEditorPlugin();
};
