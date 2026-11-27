/**************************************************************************/
/*  files_manifest_editor_plugin.h                                        */
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

#include "core/io/files_manifest.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class Button;
class EditorFileDialog;
class ItemList;
class Label;

// Edits `folder_paths` or `file_paths` of a FilesManifest: one row per path (path field, browse, move up/down
// inside the list, remove) and an add button. The list order is the load order; every change is its own undo step.
class EditorPropertyFilesManifestPaths : public EditorProperty {
	GDCLASS(EditorPropertyFilesManifestPaths, EditorProperty);

	bool folders = false;
	VBoxContainer *rows = nullptr;
	Button *add_button = nullptr;
	EditorFileDialog *dialog = nullptr;
	int dialog_index = -1; // The row the dialog replaces, -1 adds a path.
	bool updating = false;

	PackedStringArray _get_paths() const;
	void _set_paths(const PackedStringArray &p_paths);
	void _add_row(int p_index);
	void _browse(int p_index);
	void _path_selected(const String &p_path);
	void _path_edited(int p_index);
	void _move(int p_index, int p_offset);
	void _remove(int p_index);

protected:
	void _notification(int p_what);
	virtual void _set_read_only(bool p_read_only) override;

public:
	virtual void update_property() override;

	EditorPropertyFilesManifestPaths(bool p_folders);
};

// A collapsible list of the files a FilesManifest resolves to, in load order, below its properties. Lists the
// entries that resolve to nothing. Only computed while open: the type filter reads every file.
class FilesManifestPreview : public VBoxContainer {
	GDCLASS(FilesManifestPreview, VBoxContainer);

	Ref<FilesManifest> manifest;
	Button *toggle = nullptr;
	Label *nothing = nullptr;
	ItemList *list = nullptr;

	void _update_preview();

protected:
	void _notification(int p_what);

public:
	FilesManifestPreview(const Ref<FilesManifest> &p_manifest);
};

class EditorInspectorPluginFilesManifest : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginFilesManifest, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void parse_end(Object *p_object) override;
};

class FilesManifestEditorPlugin : public EditorPlugin {
	GDCLASS(FilesManifestEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "FilesManifest"; }

	FilesManifestEditorPlugin();
};
