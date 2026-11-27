/**************************************************************************/
/*  input_action_editor_plugin.cpp                                        */
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

#include "input_action_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input_action.h"
#include "core/object/callable_mp.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_toaster.h"
#include "scene/gui/button.h"
#include "scene/scene_string_names.h"

void EditorInspectorPluginInputAction::_read_from_input_map(ObjectID p_action) {
	InputAction *action = ObjectDB::get_instance<InputAction>(p_action);
	if (action == nullptr) {
		return;
	}
	if (action->get_action().is_empty()) {
		EditorToaster::get_singleton()->popup_str(TTR("Set the action name first."), EditorToaster::SEVERITY_WARNING);
		return;
	}
	const String setting = "input/" + String(action->get_action());
	const Dictionary project_action = ProjectSettings::get_singleton()->has_setting(setting) ? Dictionary(GLOBAL_GET(setting)) : Dictionary();
	if (project_action.is_empty()) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("The project's input map has no action \"%s\"."), action->get_action()), EditorToaster::SEVERITY_WARNING);
		return;
	}
	// Copies: the events of the project settings stay untouched.
	TypedArray<InputEvent> events;
	const Array project_events = project_action.get("events", Array());
	for (int i = 0; i < project_events.size(); i++) {
		const Ref<InputEvent> event = project_events[i];
		if (event.is_valid()) {
			events.push_back(event->duplicate());
		}
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Read Action \"%s\" from Input Map"), action->get_action()), UndoRedo::MERGE_DISABLE, action);
	undo_redo->add_do_property(action, "deadzone", float(project_action.get("deadzone", InputAction::DEFAULT_DEADZONE)));
	undo_redo->add_do_property(action, "events", events);
	undo_redo->add_undo_property(action, "deadzone", action->get_deadzone());
	undo_redo->add_undo_property(action, "events", action->get_events());
	undo_redo->commit_action();
}

bool EditorInspectorPluginInputAction::can_handle(Object *p_object) {
	return Object::cast_to<InputAction>(p_object) != nullptr;
}

void EditorInspectorPluginInputAction::parse_category(Object *p_object, const String &p_category) {
	// Below the class header of InputAction, above its properties.
	if (p_category != "InputAction") {
		return;
	}
	Button *read = memnew(Button);
	read->set_text(TTR("Read from InputMap"));
	read->set_tooltip_text(TTR("Copies the deadzone and the events of the action named in \"action\" from the project's input map (Project Settings > Input Map)."));
	read->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorPluginInputAction::_read_from_input_map).bind(p_object->get_instance_id()));
	add_custom_control(read);
}

InputActionEditorPlugin::InputActionEditorPlugin() {
	Ref<EditorInspectorPluginInputAction> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
