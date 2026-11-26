/**************************************************************************/
/*  pck_extract_dialog.h                                                  */
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
#include "core/io/pck_package_server.h"
#include "scene/gui/dialogs.h"

class Button;
class CheckBox;
class EditorFileDialog;
class Label;
class LineEdit;
class OptionButton;
class Tree;
class TreeItem;
struct EditorProgress;

// "Extract Pack…": lists the files of a pack with checkboxes and writes the checked ones into a folder
// with `PCKPackageServer::extract()`. Opened from the FileSystem dock and from the Inspector of a `.pck`.
class PCKExtractDialog : public ConfirmationDialog {
	GDCLASS(PCKExtractDialog, ConfirmationDialog);

	static inline PCKExtractDialog *singleton = nullptr;

	Ref<PCKPackage> package;
	String opened_path;

	Label *pack_label = nullptr;
	LineEdit *key_edit = nullptr;
	Button *preset_key_button = nullptr;
	Button *reopen_button = nullptr;
	Label *status_label = nullptr;
	Tree *tree = nullptr;
	LineEdit *target_edit = nullptr;
	Button *target_browse = nullptr;
	EditorFileDialog *target_dialog = nullptr;
	OptionButton *policy_option = nullptr;
	CheckBox *skip_engine_check = nullptr;
	Label *preview_label = nullptr;

	EditorProgress *progress = nullptr;
	bool updating = false;

	static bool _is_engine_file(const String &p_key);
	static bool _is_not_editable(const String &p_key);

	void _open_package();
	void _update_tree();
	void _update_preview();
	void _tree_item_edited();
	void _set_all_checked(bool p_checked);
	void _use_preset_key();
	void _browse_target();
	void _target_selected(const String &p_dir);
	void _target_changed(const String &p_text);
	void _option_changed(int p_index);
	void _skip_engine_toggled(bool p_pressed);
	void _extract_progress(const String &p_path, int p_done, int p_total);
	void _show_result(const Ref<PCKExtractResult> &p_result, const String &p_target);

protected:
	void _notification(int p_what);
	virtual void ok_pressed() override;

public:
	static PCKExtractDialog *get_singleton() { return singleton; }

	// Opens the pack at `p_pack_path` (header and directory only) and shows the dialog.
	void popup_for_path(const String &p_pack_path);
	// Shows the dialog for a package that is already open, or for a pack description.
	void popup_for_package(const Ref<PCKPackage> &p_package);

	PCKExtractDialog();
	~PCKExtractDialog();
};
