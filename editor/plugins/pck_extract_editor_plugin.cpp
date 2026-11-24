/**************************************************************************/
/*  pck_extract_editor_plugin.cpp                                         */
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

#include "pck_extract_editor_plugin.h"

#include "core/io/pck_package.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/file_system/pck_extract_dialog.h"
#include "scene/gui/button.h"
#include "scene/resources/texture.h"
#include "scene/scene_string_names.h"

void EditorContextMenuPluginPck::_extract(const Dictionary &p_data) {
	const PackedStringArray selected = p_data.get("selected_files", PackedStringArray());
	if (selected.size() == 1 && PckExtractDialog::get_singleton()) {
		PckExtractDialog::get_singleton()->popup_for_path(selected[0]);
	}
}

void EditorContextMenuPluginPck::get_options(const OptionsData &p_data) {
	EditorContextMenuPlugin::get_options(p_data);
	const PackedStringArray selected = p_data.get("selected_files", PackedStringArray());
	if (selected.size() != 1 || selected[0].get_extension().to_lower() != "pck") {
		return;
	}
	Ref<Texture2D> icon = EditorNode::get_singleton()->get_gui_base()->get_editor_theme_icon(SNAME("Load"));
	add_context_menu_item(TTR("Extract Pack..."), callable_mp(this, &EditorContextMenuPluginPck::_extract), icon);
}

void EditorInspectorPluginPckExtract::_extract_pressed(Object *p_package) {
	Ref<PCKPackage> package(Object::cast_to<PCKPackage>(p_package));
	if (package.is_valid() && PckExtractDialog::get_singleton()) {
		PckExtractDialog::get_singleton()->popup_for_package(package);
	}
}

bool EditorInspectorPluginPckExtract::can_handle(Object *p_object) {
	const PCKPackage *package = Object::cast_to<PCKPackage>(p_object);
	return package && !package->is_description();
}

void EditorInspectorPluginPckExtract::parse_begin(Object *p_object) {
	Button *button = memnew(Button(TTR("Extract...")));
	button->set_tooltip_text(TTR("Write the files of this pack into a folder of the project."));
	button->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorPluginPckExtract::_extract_pressed).bind(p_object));
	add_custom_control(button);
}

PckExtractEditorPlugin::PckExtractEditorPlugin() {
	dialog = memnew(PckExtractDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(dialog);

	context_menu_plugin.instantiate();
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, context_menu_plugin);

	Ref<EditorInspectorPluginPckExtract> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}
