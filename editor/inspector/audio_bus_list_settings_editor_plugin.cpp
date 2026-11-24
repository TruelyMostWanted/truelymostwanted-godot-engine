/**************************************************************************/
/*  audio_bus_list_settings_editor_plugin.cpp                             */
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

#include "audio_bus_list_settings_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/core_string_names.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/slider.h"
#include "servers/audio/audio_bus_layout.h"

void AudioBusListSettingsMixer::_read_layout(LocalVector<LayoutBus> &r_buses, String &r_source) {
	r_buses.clear();
	r_source = String();
	const String path = GLOBAL_GET("audio/buses/default_bus_layout");
	Ref<AudioBusLayout> layout;
	if (!path.is_empty() && ResourceLoader::exists(path)) {
		layout = ResourceLoader::load(path);
	}
	if (layout.is_valid()) {
		for (int i = 0;; i++) {
			bool valid = false;
			const String bus_name = layout->get(vformat("bus/%d/name", i), &valid);
			if (!valid) {
				break;
			}
			LayoutBus bus;
			bus.name = bus_name;
			bus.volume_db = layout->get(vformat("bus/%d/volume_db", i));
			bus.mute = layout->get(vformat("bus/%d/mute", i));
			bus.solo = layout->get(vformat("bus/%d/solo", i));
			r_buses.push_back(bus);
		}
		r_source = path;
	}
	if (r_buses.is_empty()) {
		// No layout: AudioServer has only "Master".
		LayoutBus master;
		master.name = "Master";
		r_buses.push_back(master);
		r_source = String();
	}

	const PackedStringArray player_buses = GLOBAL_GET("settings/audio_bus/player_buses");
	if (player_buses.is_empty()) {
		return;
	}
	LocalVector<LayoutBus> filtered;
	for (const LayoutBus &bus : r_buses) {
		if (player_buses.has(bus.name)) {
			filtered.push_back(bus);
		}
	}
	r_buses = filtered;
}

int AudioBusListSettingsMixer::_find_entry(const Array &p_buses, const String &p_bus) const {
	for (int i = 0; i < p_buses.size(); i++) {
		const Ref<AudioBusSettings> entry = p_buses[i];
		if (entry.is_valid() && entry->get_bus_name() == p_bus) {
			return i;
		}
	}
	return -1;
}

static TypedArray<AudioBusSettings> _copy_buses(const TypedArray<AudioBusSettings> &p_buses) {
	TypedArray<AudioBusSettings> copy;
	for (int i = 0; i < p_buses.size(); i++) {
		const Ref<AudioBusSettings> entry = p_buses[i];
		if (entry.is_valid()) {
			copy.push_back(entry->duplicate());
		}
	}
	return copy;
}

void AudioBusListSettingsMixer::_update_rows() {
	if (settings.is_null()) {
		return;
	}
	updating = true;
	const TypedArray<AudioBusSettings> buses = settings->get_buses();
	for (Row &row : rows) {
		const int index = _find_entry(buses, row.bus.name);
		const Ref<AudioBusSettings> entry = index >= 0 ? Ref<AudioBusSettings>(buses[index]) : Ref<AudioBusSettings>();
		const bool listed = entry.is_valid();
		row.listed->set_pressed_no_signal(listed);
		if (row.volume != nullptr) {
			const float volume_db = listed ? entry->get_volume_db() : row.bus.volume_db;
			row.volume->set_value_no_signal(volume_db);
			row.volume->set_editable(listed);
			row.volume_value->set_text(vformat("%.1f dB", volume_db));
		}
		if (row.mute != nullptr) {
			row.mute->set_pressed_no_signal(listed ? entry->is_mute() : row.bus.mute);
			row.mute->set_disabled(!listed);
		}
		if (row.solo != nullptr) {
			row.solo->set_pressed_no_signal(listed ? entry->is_solo() : row.bus.solo);
			row.solo->set_disabled(!listed);
		}
	}

	// Entries the mixer does not show (buses that are not player buses of the project layout) stay in the resource.
	PackedStringArray others;
	for (int i = 0; i < buses.size(); i++) {
		const Ref<AudioBusSettings> entry = buses[i];
		if (entry.is_null()) {
			continue;
		}
		bool shown = false;
		for (const Row &row : rows) {
			shown = shown || row.bus.name == entry->get_bus_name();
		}
		if (!shown) {
			others.push_back(entry->get_bus_name());
		}
	}
	other_buses->set_visible(!others.is_empty());
	other_buses->set_text(vformat(TTR("Also lists buses that are not player buses of the project's bus layout: %s."), String(", ").join(others)));
	updating = false;
}

void AudioBusListSettingsMixer::_commit(const Array &p_buses, const String &p_action, bool p_merge) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, p_merge ? UndoRedo::MERGE_ENDS : UndoRedo::MERGE_DISABLE, settings.ptr());
	undo_redo->add_do_property(settings.ptr(), "buses", p_buses);
	undo_redo->add_undo_property(settings.ptr(), "buses", settings->get_buses());
	undo_redo->commit_action();
}

void AudioBusListSettingsMixer::_set_value(int p_row, const StringName &p_name, const Variant &p_value, const String &p_action, bool p_merge) {
	if (updating || settings.is_null() || p_row < 0 || p_row >= int(rows.size())) {
		return;
	}
	const TypedArray<AudioBusSettings> buses = _copy_buses(settings->get_buses());
	const int index = _find_entry(buses, rows[p_row].bus.name);
	if (index < 0) {
		return;
	}
	const Ref<AudioBusSettings> entry = buses[index];
	entry->set(p_name, p_value);
	_commit(buses, p_action, p_merge);
}

void AudioBusListSettingsMixer::_listed_toggled(bool p_listed, int p_row) {
	if (updating || settings.is_null() || p_row < 0 || p_row >= int(rows.size())) {
		return;
	}
	const LayoutBus &bus = rows[p_row].bus;
	TypedArray<AudioBusSettings> buses = _copy_buses(settings->get_buses());
	const int index = _find_entry(buses, bus.name);
	if (p_listed && index < 0) {
		// Starts with the values of the layout.
		Ref<AudioBusSettings> entry;
		entry.instantiate();
		entry->set_bus_name(bus.name);
		entry->set_volume_db(bus.volume_db);
		entry->set_mute(bus.mute);
		entry->set_solo(bus.solo);
		buses.push_back(entry);
		_commit(buses, vformat(TTR("Add Bus \"%s\" to Audio Bus Settings"), bus.name), false);
	} else if (!p_listed && index >= 0) {
		buses.remove_at(index);
		_commit(buses, vformat(TTR("Remove Bus \"%s\" from Audio Bus Settings"), bus.name), false);
	}
}

void AudioBusListSettingsMixer::_volume_changed(double p_value, int p_row) {
	if (p_row >= 0 && p_row < int(rows.size()) && rows[p_row].volume_value != nullptr) {
		rows[p_row].volume_value->set_text(vformat("%.1f dB", p_value));
	}
	// Dragging the slider is one undo step.
	_set_value(p_row, SNAME("volume_db"), p_value, vformat(TTR("Set Volume of Bus \"%s\""), p_row >= 0 && p_row < int(rows.size()) ? rows[p_row].bus.name : String()), true);
}

void AudioBusListSettingsMixer::_mute_toggled(bool p_mute, int p_row) {
	_set_value(p_row, SNAME("mute"), p_mute, TTR("Toggle Bus Mute"), false);
}

void AudioBusListSettingsMixer::_solo_toggled(bool p_solo, int p_row) {
	_set_value(p_row, SNAME("solo"), p_solo, TTR("Toggle Bus Solo"), false);
}

void AudioBusListSettingsMixer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			const Callable callback = callable_mp(this, &AudioBusListSettingsMixer::_update_rows);
			if (settings.is_valid() && !settings->is_connected(CoreStringName(changed), callback)) {
				settings->connect(CoreStringName(changed), callback);
			}
			_update_rows();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			const Callable callback = callable_mp(this, &AudioBusListSettingsMixer::_update_rows);
			if (settings.is_valid() && settings->is_connected(CoreStringName(changed), callback)) {
				settings->disconnect(CoreStringName(changed), callback);
			}
		} break;
	}
}

AudioBusListSettingsMixer::AudioBusListSettingsMixer(const Ref<AudioBusListSettings> &p_settings) {
	settings = p_settings;

	LocalVector<LayoutBus> layout;
	String source;
	_read_layout(layout, source);
	const bool store_volume = GLOBAL_GET("settings/audio_bus/store_volume");
	const bool store_mute = GLOBAL_GET("settings/audio_bus/store_mute");
	const bool store_solo = GLOBAL_GET("settings/audio_bus/store_solo");

	Label *title = memnew(Label);
	title->set_text(source.is_empty() ? TTR("Player Buses (no bus layout: Master only)") : vformat(TTR("Player Buses of %s"), source.get_file()));
	title->set_tooltip_text(TTR("The buses come from the project setting \"audio/buses/default_bus_layout\", filtered by \"settings/audio_bus/player_buses\". The values shown follow \"settings/audio_bus/store_*\".\nA checked bus is listed in this resource. Used as a preset, buses that are not listed keep their values."));
	title->set_mouse_filter(MOUSE_FILTER_PASS);
	add_child(title);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(1 + (store_volume ? 2 : 0) + (store_mute ? 1 : 0) + (store_solo ? 1 : 0));
	add_child(grid);

	for (uint32_t i = 0; i < layout.size(); i++) {
		Row row;
		row.bus = layout[i];

		row.listed = memnew(CheckBox);
		row.listed->set_text(row.bus.name);
		row.listed->set_tooltip_text(TTR("Listed in this resource."));
		row.listed->connect(SNAME("toggled"), callable_mp(this, &AudioBusListSettingsMixer::_listed_toggled).bind(int(i)));
		grid->add_child(row.listed);

		if (store_volume) {
			row.volume = memnew(HSlider);
			row.volume->set_min(AudioBusSettings::MIN_VOLUME_DB);
			row.volume->set_max(AudioBusSettings::MAX_VOLUME_DB);
			row.volume->set_step(0.1);
			row.volume->set_h_size_flags(SIZE_EXPAND_FILL);
			row.volume->set_v_size_flags(SIZE_SHRINK_CENTER);
			row.volume->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
			row.volume->connect(SNAME("value_changed"), callable_mp(this, &AudioBusListSettingsMixer::_volume_changed).bind(int(i)));
			grid->add_child(row.volume);

			row.volume_value = memnew(Label);
			row.volume_value->set_custom_minimum_size(Size2(64 * EDSCALE, 0));
			row.volume_value->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
			grid->add_child(row.volume_value);
		}
		if (store_mute) {
			row.mute = memnew(CheckBox);
			row.mute->set_text(TTR("Mute"));
			row.mute->connect(SNAME("toggled"), callable_mp(this, &AudioBusListSettingsMixer::_mute_toggled).bind(int(i)));
			grid->add_child(row.mute);
		}
		if (store_solo) {
			row.solo = memnew(CheckBox);
			row.solo->set_text(TTR("Solo"));
			row.solo->connect(SNAME("toggled"), callable_mp(this, &AudioBusListSettingsMixer::_solo_toggled).bind(int(i)));
			grid->add_child(row.solo);
		}
		rows.push_back(row);
	}

	if (!store_volume && !store_mute && !store_solo) {
		Label *nothing = memnew(Label);
		nothing->set_text(TTR("The project stores no bus values (settings/audio_bus/store_*)."));
		nothing->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		add_child(nothing);
	}

	other_buses = memnew(Label);
	other_buses->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	other_buses->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	other_buses->hide();
	add_child(other_buses);
}

bool EditorInspectorPluginAudioBusListSettings::can_handle(Object *p_object) {
	return Object::cast_to<AudioBusListSettings>(p_object) != nullptr;
}

void EditorInspectorPluginAudioBusListSettings::parse_begin(Object *p_object) {
	Ref<AudioBusListSettings> settings(Object::cast_to<AudioBusListSettings>(p_object));
	if (settings.is_null()) {
		return;
	}
	add_custom_control(memnew(AudioBusListSettingsMixer(settings)));
}

bool EditorInspectorPluginAudioBusListSettings::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	// The mixer replaces the array: no free-text bus names.
	return p_path == "buses" && Object::cast_to<AudioBusListSettings>(p_object) != nullptr;
}

AudioBusListSettingsEditorPlugin::AudioBusListSettingsEditorPlugin() {
	Ref<EditorInspectorPluginAudioBusListSettings> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
