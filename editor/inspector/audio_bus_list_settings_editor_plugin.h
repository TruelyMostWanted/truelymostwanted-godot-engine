/**************************************************************************/
/*  audio_bus_list_settings_editor_plugin.h                               */
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

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "servers/settings/audio_settings.h"

class CheckBox;
class GridContainer;
class HSlider;
class Label;

// A small mixer for an AudioBusListSettings (#185): one row per player bus of the project's bus layout
// (`audio/buses/default_bus_layout`, filtered by `settings/audio_bus/player_buses`) with the values the project
// stores (`settings/audio_bus/store_*`). Shown instead of the `buses` array, so bus names can't be typed freely.
// Listed buses that are not player buses can only be removed. The rows follow changes of the project settings and of
// the bus layout file (#404).
class AudioBusListSettingsMixer : public VBoxContainer {
	GDCLASS(AudioBusListSettingsMixer, VBoxContainer);

	struct LayoutBus {
		String name;
		float volume_db = 0.0f;
		bool mute = false;
		bool solo = false;
	};

	struct Row {
		LayoutBus bus;
		CheckBox *listed = nullptr;
		HSlider *volume = nullptr;
		Label *volume_value = nullptr;
		CheckBox *mute = nullptr;
		CheckBox *solo = nullptr;
	};

	Ref<AudioBusListSettings> settings;
	LocalVector<Row> rows;
	VBoxContainer *other_buses = nullptr;
	String layout_key; // What the rows were built from; _build() rebuilds them only if it changes.
	bool updating = false;

	static void _read_layout(LocalVector<LayoutBus> &r_buses, String &r_source);
	int _find_entry(const Array &p_buses, const String &p_bus) const;
	void _build();
	void _update_rows();
	void _remove_other_bus(const String &p_bus);
	void _commit(const Array &p_buses, const String &p_action, bool p_merge);
	void _set_value(int p_row, const StringName &p_name, const Variant &p_value, const String &p_action, bool p_merge);

	void _listed_toggled(bool p_listed, int p_row);
	void _volume_changed(double p_value, int p_row);
	void _mute_toggled(bool p_mute, int p_row);
	void _solo_toggled(bool p_solo, int p_row);

protected:
	void _notification(int p_what);

public:
	AudioBusListSettingsMixer(const Ref<AudioBusListSettings> &p_settings);
};

class EditorInspectorPluginAudioBusListSettings : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAudioBusListSettings, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_category(Object *p_object, const String &p_category) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

class AudioBusListSettingsEditorPlugin : public EditorPlugin {
	GDCLASS(AudioBusListSettingsEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "AudioBusListSettings"; }

	AudioBusListSettingsEditorPlugin();
};
