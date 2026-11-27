/**************************************************************************/
/*  localization_catalog_editor_plugin.h                                  */
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
#include "editor/inspector/editor_context_menu_plugin.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class ConfirmationDialog;
class LocalizationCatalog;

// Missing keys per locale, the validation result and "Save POT..." above the catalog's properties (#346).
// Editing happens in the CSV Table dock, which opens with the catalog.
class EditorInspectorPluginLocalizationCatalog : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginLocalizationCatalog, EditorInspectorPlugin);

	static constexpr int MAX_LISTED_KEYS = 100;

	void _save_pot(const String &p_path, const Ref<LocalizationCatalog> &p_catalog);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

// Writes the text catalogs of `internationalization/localization/catalog_manifests` as binary `.lcat` into the
// export, remapped from the text file (like FlowGraphExportPlugin).
class LocalizationCatalogExportPlugin : public EditorExportPlugin {
	GDCLASS(LocalizationCatalogExportPlugin, EditorExportPlugin);

	HashMap<String, StringName> catalog_classes; // Text catalog path -> catalog class, from the manifests.

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;

public:
	virtual String get_name() const override { return "LocalizationCatalog"; }
};

// "Use as Text/Resource Localization Catalog" for data files in the FileSystem dock.
class EditorContextMenuPluginLocalizationCatalog : public EditorContextMenuPlugin {
	GDCLASS(EditorContextMenuPluginLocalizationCatalog, EditorContextMenuPlugin);

	void _use_as_catalog(const Dictionary &p_data, const StringName &p_class);

public:
	using EditorContextMenuPlugin::get_options;
	virtual void get_options(const OptionsData &p_data) override;
};

class LocalizationCatalogEditorPlugin : public EditorPlugin {
	GDCLASS(LocalizationCatalogEditorPlugin, EditorPlugin);

	ConfirmationDialog *convert_dialog = nullptr;

	void _convert_project_translations();

protected:
	void _notification(int p_what);

public:
	// Adds the files to the first catalog manifest of the class (one is created if missing), switches CSV files
	// to "Keep File" and saves the project settings.
	static void add_catalog_files(const PackedStringArray &p_paths, const StringName &p_class);

	virtual String get_plugin_name() const override { return "LocalizationCatalog"; }

	LocalizationCatalogEditorPlugin();
};
