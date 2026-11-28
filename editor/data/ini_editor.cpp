/**************************************************************************/
/*  ini_editor.cpp                                                        */
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

#include "ini_editor.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/variant/variant_utility.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"

// The types offered in the Type column; a value of another type adds its own.
static const Variant::Type VALUE_TYPES[] = { Variant::BOOL, Variant::INT, Variant::FLOAT, Variant::STRING, Variant::VECTOR2, Variant::VECTOR3, Variant::COLOR, Variant::ARRAY, Variant::DICTIONARY };

enum KeyColumn {
	KEY_COLUMN_KEY,
	KEY_COLUMN_TYPE,
	KEY_COLUMN_VALUE,
};

static String _get_unique_name(const Dictionary &p_dictionary, const String &p_base) {
	String result = p_base;
	for (int i = 2; p_dictionary.has(result); i++) {
		result = p_base + "_" + itos(i);
	}
	return result;
}

// Text of a value: Strings as they are, every other type in Variant syntax.
static String _value_to_text(const Variant &p_value) {
	return p_value.get_type() == Variant::STRING ? String(p_value) : VariantUtilityFunctions::var_to_str(p_value);
}

// A String typed in a plain file or a cell becomes the target type when it is written in that type's syntax.
static Variant _convert(const Variant &p_value, Variant::Type p_type) {
	if (p_value.get_type() == Variant::STRING && p_type != Variant::STRING) {
		const Variant parsed = VariantUtilityFunctions::str_to_var(p_value);
		if (parsed.get_type() == p_type) {
			return parsed;
		}
	}
	return VariantUtilityFunctions::type_convert(p_value, p_type);
}

// Replaces a key of a Dictionary by another one at the same position.
static Dictionary _rename_key(const Dictionary &p_dictionary, const String &p_from, const String &p_to) {
	Dictionary result;
	for (const KeyValue<Variant, Variant> &kv : p_dictionary) {
		result[String(kv.key) == p_from ? Variant(p_to) : kv.key] = kv.value;
	}
	return result;
}

TextEditorBase *INIEditor::_get_text_editor() const {
	return Object::cast_to<TextEditorBase>(ScriptEditor::get_singleton()->get_script_container()->get_resource_editor(ini));
}

bool INIEditor::_can_edit() {
	if (ini.is_null()) {
		return false;
	}
	TextEditorBase *text_editor = _get_text_editor();
	const TextEdit *text_edit = text_editor ? Object::cast_to<TextEdit>(text_editor->get_base_editor()) : nullptr;
	if (text_edit) {
		// Takes over text typed since the text editor's last idle parse, so the dock edits the current text.
		const bool in_sync = ini->get_parsed_text().is_empty() ? text_edit->get_version() == reloaded_version : text_edit->get_text() == ini->get_parsed_text();
		if (!in_sync) {
			ini->parse(text_edit->get_text(), true);
		}
	}
	if (!ini->get_error_message().is_empty()) {
		status_label->set_text(vformat(TTR("Fix the INI text first (line %d): %s"), ini->get_error_line(), ini->get_error_message()));
		return false;
	}
	return true;
}

void INIEditor::_commit(const String &p_action, const Dictionary &p_data, const String &p_section) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action);
	undo_redo->add_do_method(ini.ptr(), "set_data", p_data);
	undo_redo->add_undo_method(ini.ptr(), "set_data", Dictionary(ini->get_data()).duplicate(true));
	section = p_section;
	undo_redo->commit_action(); // `changed` renders the dock deferred, not while a tree emits a signal.
}

void INIEditor::_ini_changed() {
	status_label->set_text(String());
	if (!update_queued) {
		update_queued = true;
		callable_mp(this, &INIEditor::_update).call_deferred();
	}
}

void INIEditor::_update() {
	update_queued = false;
	if (ini.is_null()) {
		return;
	}
	// No kept text: the data was changed by the dock, undo/redo or a script, not by parsing the text.
	// Show the change in the text editor, so neither side overwrites the other.
	if (ini->get_parsed_text().is_empty()) {
		const String text = ini->encode_to_text();
		TextEditorBase *text_editor = _get_text_editor();
		const TextEdit *text_edit = text_editor ? Object::cast_to<TextEdit>(text_editor->get_base_editor()) : nullptr;
		if (text_edit && text_edit->get_text() != text) {
			text_editor->reload_text();
			reloaded_version = text_edit->get_version();
		}
	}
	// The text editor parses the reloaded text again: same data, and a render would close an open cell editor.
	if (VariantUtilityFunctions::var_to_str(ini->get_data()) != rendered_text) {
		_render();
	}
}

void INIEditor::_render() {
	section_tree->clear();
	TreeItem *root = section_tree->create_item();
	const Dictionary file_data = ini.is_valid() ? Dictionary(ini->get_data()) : Dictionary();
	rendered_text = VariantUtilityFunctions::var_to_str(file_data);
	if (!file_data.has(section) && !file_data.is_empty()) {
		section = file_data.get_key_at_index(0);
	}
	section_tree->set_block_signals(true); // select() emits item_selected; the keys are rendered once below.
	for (const KeyValue<Variant, Variant> &kv : file_data) {
		const String name = kv.key;
		TreeItem *item = section_tree->create_item(root);
		item->set_metadata(0, name);
		if (name.is_empty()) {
			item->set_text(0, TTR("(no section)"));
			item->set_tooltip_text(0, TTR("Keys before the first section header."));
			item->set_custom_color(0, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
		} else {
			item->set_text(0, name);
			item->set_editable(0, true);
		}
		if (name == section) {
			item->select(0);
		}
	}
	section_tree->set_block_signals(false);
	_section_selected();
	if (edit_section || !edit_key.is_empty()) {
		callable_mp(this, &INIEditor::_edit_added).call_deferred();
	}
}

void INIEditor::_edit_added() {
	// After the last render of the action: a later render would close the cell editor again.
	if (edit_section && section_tree->get_selected()) {
		section_tree->edit_selected(true);
	}
	for (TreeItem *item = key_tree->get_root()->get_first_child(); item && !edit_key.is_empty(); item = item->get_next()) {
		if (String(item->get_metadata(KEY_COLUMN_KEY)) == edit_key) {
			key_tree->set_selected(item, KEY_COLUMN_KEY);
			key_tree->edit_selected(true);
		}
	}
	edit_section = false;
	edit_key = String();
}

void INIEditor::_section_selected() {
	const TreeItem *selected = section_tree->get_selected();
	if (selected) {
		section = selected->get_metadata(0);
	}
	key_tree->clear();
	const Dictionary file_data = ini.is_valid() ? Dictionary(ini->get_data()) : Dictionary();
	const Dictionary keys = file_data.get(section, Dictionary());
	remove_section_button->set_disabled(!file_data.has(section));
	add_key_button->set_disabled(ini.is_null());
	remove_key_button->set_disabled(keys.is_empty());
	if (ini.is_null()) {
		empty_label->set_text(TTR("Open an INI file with \"Open in INI Dock\" in the inspector."));
	} else if (file_data.is_empty()) {
		empty_label->set_text(TTR("The file has no keys yet. Add a section or a key."));
	} else if (keys.is_empty()) {
		empty_label->set_text(vformat(TTR("The section \"%s\" has no keys yet. Add one with Add Key."), section));
	}
	empty_label->set_visible(keys.is_empty());

	const Ref<SerializerOptions> options = ini.is_valid() ? ini->get_options() : Ref<SerializerOptions>();
	const bool plain = options.is_valid() && options->get_ini_value_style() == SerializerOptions::INI_VALUE_STYLE_PLAIN;
	TreeItem *root = key_tree->create_item();
	for (const KeyValue<Variant, Variant> &kv : keys) {
		TreeItem *item = key_tree->create_item(root);
		item->set_metadata(KEY_COLUMN_KEY, kv.key);
		item->set_text(KEY_COLUMN_KEY, kv.key);
		item->set_editable(KEY_COLUMN_KEY, true);

		const Variant::Type type = kv.value.get_type();
		String type_names;
		int type_index = -1;
		for (int i = 0; i < (int)std_size(VALUE_TYPES); i++) {
			type_names += (i > 0 ? "," : "") + Variant::get_type_name(VALUE_TYPES[i]);
			if (VALUE_TYPES[i] == type) {
				type_index = i;
			}
		}
		if (type_index < 0) {
			type_index = (int)std_size(VALUE_TYPES);
			type_names += "," + Variant::get_type_name(type);
		}
		item->set_cell_mode(KEY_COLUMN_TYPE, TreeItem::CELL_MODE_RANGE);
		item->set_text(KEY_COLUMN_TYPE, type_names);
		item->set_range(KEY_COLUMN_TYPE, type_index);
		// Plain files store every value as text.
		item->set_editable(KEY_COLUMN_TYPE, !plain);
		if (plain) {
			item->set_tooltip_text(KEY_COLUMN_TYPE, TTR("Plain INI files store every value as text (SerializerOptions.ini_value_style)."));
		}

		if (type == Variant::BOOL) {
			item->set_cell_mode(KEY_COLUMN_VALUE, TreeItem::CELL_MODE_CHECK);
			item->set_checked(KEY_COLUMN_VALUE, kv.value);
			item->set_text(KEY_COLUMN_VALUE, kv.value ? "true" : "false");
		} else {
			const String text = _value_to_text(kv.value);
			item->set_text(KEY_COLUMN_VALUE, text);
			item->set_tooltip_text(KEY_COLUMN_VALUE, text);
		}
		item->set_editable(KEY_COLUMN_VALUE, true);
	}
}

void INIEditor::_section_edited() {
	TreeItem *item = section_tree->get_edited();
	const String from = item->get_metadata(0);
	const String to = item->get_text(0).strip_edges();
	if (to == from) {
		return;
	}
	const Dictionary file_data = ini->get_data();
	if (to.is_empty() || to.contains_char(']') || to.contains_char('\n')) {
		status_label->set_text(TTR("A section name can't be empty or contain \"]\"."));
	} else if (file_data.has(to)) {
		status_label->set_text(vformat(TTR("A section named \"%s\" already exists."), to));
	} else if (_can_edit()) {
		_commit(TTR("Rename INI Section"), _rename_key(Dictionary(ini->get_data()).duplicate(true), from, to), to);
		return;
	}
	callable_mp(this, &INIEditor::_render).call_deferred(); // Restores the name; not while the tree is emitting.
}

void INIEditor::_key_edited() {
	TreeItem *item = key_tree->get_edited();
	const int column = key_tree->get_edited_column();
	const String key = item->get_metadata(KEY_COLUMN_KEY);
	if (!_can_edit() || !ini->has_section_key(section, key)) {
		callable_mp(this, &INIEditor::_render).call_deferred();
		return;
	}
	Dictionary file_data = Dictionary(ini->get_data()).duplicate(true);
	Dictionary keys = file_data[section];
	const Variant value = keys[key];
	String error;
	if (column == KEY_COLUMN_KEY) {
		const String to = item->get_text(KEY_COLUMN_KEY).strip_edges();
		if (to == key) {
			return;
		}
		if (to.is_empty() || to.contains_char('=') || to.contains_char('\n')) {
			error = TTR("A key name can't be empty or contain \"=\".");
		} else if (keys.has(to)) {
			error = vformat(TTR("The key \"%s\" already exists in this section."), to);
		} else {
			file_data[section] = _rename_key(keys, key, to);
			_commit(TTR("Rename INI Key"), file_data, section);
			return;
		}
	} else if (column == KEY_COLUMN_TYPE) {
		const Variant::Type type = Variant::get_type_by_name(item->get_text(KEY_COLUMN_TYPE).get_slicec(',', (int)item->get_range(KEY_COLUMN_TYPE)));
		if (type == value.get_type()) {
			return;
		}
		keys[key] = _convert(value, type);
		_commit(TTR("Change INI Value Type"), file_data, section);
		return;
	} else {
		const String text = item->get_text(KEY_COLUMN_VALUE);
		Variant new_value;
		switch (value.get_type()) {
			case Variant::BOOL:
				new_value = item->is_checked(KEY_COLUMN_VALUE);
				break;
			case Variant::STRING:
				new_value = text;
				break;
			case Variant::INT:
				if (text.strip_edges().is_valid_int()) {
					new_value = text.strip_edges().to_int();
				}
				break;
			case Variant::FLOAT:
				if (text.strip_edges().is_valid_float()) {
					new_value = text.strip_edges().to_float();
				}
				break;
			default: {
				const Variant parsed = VariantUtilityFunctions::str_to_var(text);
				if (parsed.get_type() == value.get_type()) {
					new_value = parsed;
				}
			} break;
		}
		if (new_value.get_type() == Variant::NIL) {
			error = vformat(TTR("\"%s\" is not a valid %s value."), text, Variant::get_type_name(value.get_type()));
		} else if (new_value != value) {
			keys[key] = new_value;
			_commit(TTR("Set INI Value"), file_data, section);
			return;
		}
	}
	callable_mp(this, &INIEditor::_render).call_deferred(); // Restores the cell.
	status_label->set_text(error);
}

void INIEditor::_add_section() {
	if (!_can_edit()) {
		return;
	}
	Dictionary file_data = Dictionary(ini->get_data()).duplicate(true);
	const String name = _get_unique_name(file_data, "new_section");
	file_data[name] = Dictionary();
	edit_section = true;
	_commit(TTR("Add INI Section"), file_data, name);
}

void INIEditor::_remove_section() {
	if (!_can_edit() || !ini->has_section(section)) {
		return;
	}
	Dictionary file_data = Dictionary(ini->get_data()).duplicate(true);
	file_data.erase(section);
	_commit(TTR("Remove INI Section"), file_data, section);
}

void INIEditor::_add_key() {
	if (!_can_edit()) {
		return;
	}
	Dictionary file_data = Dictionary(ini->get_data()).duplicate(true);
	if (!file_data.has(section)) {
		file_data[section] = Dictionary();
	}
	Dictionary keys = file_data[section];
	const String key = _get_unique_name(keys, "new_key");
	keys[key] = String();
	edit_key = key;
	_commit(TTR("Add INI Key"), file_data, section);
}

void INIEditor::_remove_keys() {
	if (!_can_edit() || !ini->has_section(section)) {
		return;
	}
	Dictionary file_data = Dictionary(ini->get_data()).duplicate(true);
	Dictionary keys = file_data[section];
	// The section stays, also when it has no keys left.
	for (TreeItem *item = key_tree->get_next_selected(nullptr); item; item = key_tree->get_next_selected(item)) {
		keys.erase(item->get_metadata(KEY_COLUMN_KEY));
	}
	if (keys.size() != ini->get_section_keys(section).size()) {
		_commit(TTR("Remove INI Keys"), file_data, section);
	}
}

void INIEditor::_save() {
	if (!_can_edit()) {
		return;
	}
	EditorNode::get_singleton()->save_resource(ini);
	TextEditorBase *text_editor = _get_text_editor();
	if (text_editor) {
		text_editor->tag_saved_version();
	}
}

void INIEditor::_resource_saved(Object *p_resource) {
	if (p_resource && p_resource == ini.ptr()) {
		status_label->set_text(vformat(TTR("Saved \"%s\"."), ini->get_path()));
	}
}

void INIEditor::_tree_gui_input(const Ref<InputEvent> &p_event, bool p_sections) {
	Tree *tree = p_sections ? section_tree : key_tree;
	if (ED_IS_SHORTCUT("editor/save_scene", p_event)) {
		_save();
		tree->accept_event();
		return;
	}
	const Ref<InputEventKey> key_event = p_event;
	if (key_event.is_null() || !key_event->is_pressed() || key_event->is_echo()) {
		return;
	}
	if (key_event->get_keycode() == Key::KEY_DELETE) {
		if (p_sections) {
			_remove_section();
		} else {
			_remove_keys();
		}
		tree->accept_event();
	} else if (key_event->get_keycode() == Key::F2 && tree->get_selected()) {
		tree->set_selected(tree->get_selected(), 0);
		tree->edit_selected(true);
		tree->accept_event();
	}
}

void INIEditor::_inspector_object_changed() {
	// Opened with "Open in INI Dock", the dock is not closed by the editor when another file is inspected.
	const Resource *inspected = Object::cast_to<Resource>(InspectorDock::get_inspector_singleton()->get_edited_object());
	if (inspected && inspected != ini.ptr() && !inspected->get_path().is_empty()) {
		edit(Ref<INI>());
		close();
	}
}

void INIEditor::_file_removed(const String &p_file) {
	if (ini.is_valid() && p_file == ini->get_path()) {
		edit(Ref<INI>());
		close();
	}
}

void INIEditor::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (InspectorDock::get_singleton()) {
			InspectorDock::get_inspector_singleton()->connect(SNAME("edited_object_changed"), callable_mp(this, &INIEditor::_inspector_object_changed));
		}
		if (FileSystemDock::get_singleton()) {
			FileSystemDock::get_singleton()->connect(SNAME("file_removed"), callable_mp(this, &INIEditor::_file_removed));
		}
		EditorNode::get_singleton()->connect(SNAME("resource_saved"), callable_mp(this, &INIEditor::_resource_saved));
	}
}

void INIEditor::edit(const Ref<INI> &p_ini) {
	if (ini == p_ini) {
		return;
	}
	if (ini.is_valid()) {
		ini->disconnect_changed(callable_mp(this, &INIEditor::_ini_changed));
	}
	ini = p_ini;
	section = String();
	reloaded_version = UINT32_MAX;
	status_label->set_text(String());
	file_label->set_text(ini.is_valid() ? ini->get_path().get_file() : String());
	if (ini.is_valid()) {
		ini->connect_changed(callable_mp(this, &INIEditor::_ini_changed));
		if (!ini->get_error_message().is_empty()) {
			status_label->set_text(vformat(TTR("Fix the INI text first (line %d): %s"), ini->get_error_line(), ini->get_error_message()));
		}
	}
	_render();
}

INIEditor::INIEditor() {
	singleton = this;
	set_name(TTRC("INI"));
	set_icon_name("TextFile");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_ini_bottom_panel", TTRC("Toggle INI Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	main_vbox->add_child(toolbar);
	file_label = memnew(Label);
	toolbar->add_child(file_label);

	Button *add_section_button = memnew(Button(TTR("Add Section")));
	add_section_button->connect(SceneStringName(pressed), callable_mp(this, &INIEditor::_add_section));
	toolbar->add_child(add_section_button);
	remove_section_button = memnew(Button(TTR("Remove Section")));
	remove_section_button->set_tooltip_text(TTR("Remove the selected section with all its keys (Delete in the section list)."));
	remove_section_button->connect(SceneStringName(pressed), callable_mp(this, &INIEditor::_remove_section));
	toolbar->add_child(remove_section_button);
	add_key_button = memnew(Button(TTR("Add Key")));
	add_key_button->set_tooltip_text(TTR("Add a key with an empty String value to the selected section."));
	add_key_button->connect(SceneStringName(pressed), callable_mp(this, &INIEditor::_add_key));
	toolbar->add_child(add_key_button);
	remove_key_button = memnew(Button(TTR("Remove Keys")));
	remove_key_button->set_tooltip_text(TTR("Remove the selected keys (Delete in the key table)."));
	remove_key_button->connect(SceneStringName(pressed), callable_mp(this, &INIEditor::_remove_keys));
	toolbar->add_child(remove_key_button);
	Button *save_button = memnew(Button(TTR("Save")));
	save_button->set_tooltip_text(TTR("Save the file (Ctrl+S in the dock)."));
	save_button->connect(SceneStringName(pressed), callable_mp(this, &INIEditor::_save));
	toolbar->add_child(save_button);

	HSplitContainer *split = memnew(HSplitContainer);
	split->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_child(split);

	section_tree = memnew(Tree);
	section_tree->set_custom_minimum_size(Size2(180 * EDSCALE, 0));
	section_tree->set_hide_root(true);
	section_tree->set_column_titles_visible(true);
	section_tree->set_column_title(0, TTR("Sections"));
	section_tree->connect(SceneStringName(item_selected), callable_mp(this, &INIEditor::_section_selected));
	section_tree->connect("item_edited", callable_mp(this, &INIEditor::_section_edited));
	section_tree->connect(SceneStringName(gui_input), callable_mp(this, &INIEditor::_tree_gui_input).bind(true));
	split->add_child(section_tree);

	key_tree = memnew(Tree);
	key_tree->set_h_size_flags(SIZE_EXPAND_FILL);
	key_tree->set_columns(3);
	key_tree->set_hide_root(true);
	key_tree->set_column_titles_visible(true);
	key_tree->set_select_mode(Tree::SELECT_MULTI);
	key_tree->set_column_title(KEY_COLUMN_KEY, TTR("Key"));
	key_tree->set_column_title(KEY_COLUMN_TYPE, TTR("Type"));
	key_tree->set_column_title(KEY_COLUMN_VALUE, TTR("Value"));
	key_tree->set_column_expand_ratio(KEY_COLUMN_KEY, 1);
	key_tree->set_column_expand(KEY_COLUMN_TYPE, false);
	key_tree->set_column_custom_minimum_width(KEY_COLUMN_TYPE, 120 * EDSCALE);
	key_tree->set_column_expand_ratio(KEY_COLUMN_VALUE, 2);
	key_tree->set_column_clip_content(KEY_COLUMN_VALUE, true);
	key_tree->connect("item_edited", callable_mp(this, &INIEditor::_key_edited));
	key_tree->connect(SceneStringName(gui_input), callable_mp(this, &INIEditor::_tree_gui_input).bind(false));
	split->add_child(key_tree);
	// Shown on the empty table, which stays visible so a new key is edited right away.
	empty_label = memnew(Label);
	empty_label->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	empty_label->set_mouse_filter(MOUSE_FILTER_IGNORE);
	empty_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	empty_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	empty_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	key_tree->add_child(empty_label);

	status_label = memnew(Label);
	status_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	status_label->set_h_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_child(status_label);
}

INIEditor::~INIEditor() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void INIEditorPlugin::edit(Object *p_object) {
	ini_editor->edit(Ref<INI>(Object::cast_to<INI>(p_object)));
}

bool INIEditorPlugin::handles(Object *p_object) const {
	// Opened from the inspector by default; with the editor setting also when the file opens (double-click).
	const INI *ini = Object::cast_to<INI>(p_object);
	return ini && ini->is_recognized_path(ini->get_path()) && bool(EDITOR_GET("docks/ini_editor/open_on_double_click"));
}

void INIEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		ini_editor->make_visible();
	}
}

INIEditorPlugin::INIEditorPlugin() {
	ini_editor = memnew(INIEditor);
	ini_editor->set_custom_minimum_size(Size2(0, 250) * EDSCALE);
	EditorDockManager::get_singleton()->add_dock(ini_editor);
	ini_editor->close();
}
