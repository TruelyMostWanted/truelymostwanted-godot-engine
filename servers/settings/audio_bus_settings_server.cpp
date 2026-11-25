/**************************************************************************/
/*  audio_bus_settings_server.cpp                                         */
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

#include "audio_bus_settings_server.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/audio/audio_server.h"

static constexpr const char *SETTING_PLAYER_BUSES = "settings/audio_bus/player_buses";
static constexpr const char *SETTING_STORE_VOLUME = "settings/audio_bus/store_volume";
static constexpr const char *SETTING_STORE_MUTE = "settings/audio_bus/store_mute";
static constexpr const char *SETTING_STORE_SOLO = "settings/audio_bus/store_solo";

AudioBusSettingsServer *AudioBusSettingsServer::singleton = nullptr;

AudioBusSettingsServer *AudioBusSettingsServer::get_singleton() {
	return singleton;
}

void AudioBusSettingsServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_PLAYER_BUSES), PackedStringArray());
	GLOBAL_DEF(SETTING_STORE_VOLUME, true);
	GLOBAL_DEF(SETTING_STORE_MUTE, true);
	GLOBAL_DEF(SETTING_STORE_SOLO, false);
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName AudioBusSettingsServer::get_domain() const {
	return SNAME("audio_bus");
}

SettingsServer::ApplyPhase AudioBusSettingsServer::get_apply_phase() const {
	// After AudioServer::init() and load_default_bus_layout(), before any AudioStreamPlayer can play.
	return APPLY_PHASE_SERVERS;
}

StringName AudioBusSettingsServer::get_list_property() const {
	return SNAME("buses");
}

StringName AudioBusSettingsServer::get_entry_key() const {
	return SNAME("bus_name");
}

Ref<Resource> AudioBusSettingsServer::_create_default_settings() const {
	Ref<AudioBusListSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

uint32_t AudioBusSettingsServer::_get_stored_values() const {
	uint32_t values = 0;
	if (store_volume) {
		values |= AudioBusSettingsRow::STORED_VOLUME;
	}
	if (store_mute) {
		values |= AudioBusSettingsRow::STORED_MUTE;
	}
	if (store_solo) {
		values |= AudioBusSettingsRow::STORED_SOLO;
	}
	return values;
}

static Ref<AudioBusSettings> _make_entry_from_engine(int p_bus) {
	const AudioServer *audio_server = AudioServer::get_singleton();
	Ref<AudioBusSettings> entry;
	entry.instantiate();
	entry->set_bus_name(audio_server->get_bus_name(p_bus));
	entry->set_volume_db(audio_server->get_bus_volume_db(p_bus));
	entry->set_mute(audio_server->is_bus_mute(p_bus));
	entry->set_solo(audio_server->is_bus_solo(p_bus));
	return entry;
}

static int _find_bus_entry(const Array &p_list, const String &p_bus) {
	for (int i = 0; i < p_list.size(); i++) {
		const Ref<AudioBusSettings> entry = p_list[i];
		if (entry.is_valid() && entry->get_bus_name() == p_bus) {
			return i;
		}
	}
	return -1;
}

Ref<AudioBusSettings> AudioBusSettingsServer::_get_layout_entry(const String &p_bus) const {
	// The values of the layout as AudioServer had them when the defaults were captured; a bus added since: its current values.
	const int index = _find_bus_entry(layout_buses, p_bus);
	if (index >= 0) {
		return copy_value(layout_buses[index]);
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
	const int bus = audio_server != nullptr ? audio_server->get_bus_index(p_bus) : -1;
	return bus >= 0 ? _make_entry_from_engine(bus) : Ref<AudioBusSettings>();
}

Array AudioBusSettingsServer::_make_layout_entries() const {
	TypedArray<AudioBusSettings> entries;
	const AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr) {
		return entries;
	}
	for (int i = 0; i < audio_server->get_bus_count(); i++) {
		const String bus_name = audio_server->get_bus_name(i);
		if (is_player_bus(bus_name)) {
			entries.push_back(_get_layout_entry(bus_name));
		}
	}
	return entries;
}

// Sets the values that the project does not store to the values of the layout.
static void _set_unstored_values(const Ref<AudioBusSettings> &p_entry, const Ref<AudioBusSettings> &p_layout, uint32_t p_stored) {
	if (p_entry.is_null()) {
		return;
	}
	Ref<AudioBusSettings> layout = p_layout;
	if (layout.is_null()) {
		layout.instantiate();
	}
	if (!(p_stored & AudioBusSettingsRow::STORED_VOLUME) && p_entry->get_volume_db() != layout->get_volume_db()) {
		p_entry->set_volume_db(layout->get_volume_db());
	}
	if (!(p_stored & AudioBusSettingsRow::STORED_MUTE) && p_entry->is_mute() != layout->is_mute()) {
		p_entry->set_mute(layout->is_mute());
	}
	if (!(p_stored & AudioBusSettingsRow::STORED_SOLO) && p_entry->is_solo() != layout->is_solo()) {
		p_entry->set_solo(layout->is_solo());
	}
}

void AudioBusSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	const StringName property = get_list_property();
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || !p_names.has(String(property))) {
		return;
	}
	_connect_audio_server();
	const uint32_t stored = _get_stored_values();
	const Array buses = p_settings->get(property);
	PackedStringArray missing;
	for (int i = 0; i < buses.size(); i++) {
		const Ref<AudioBusSettings> entry = buses[i];
		if (entry.is_null()) {
			continue;
		}
		const String bus_name = entry->get_bus_name();
		// Other buses (mix buses, sends) keep what the layout or the game set.
		if (bus_name.is_empty() || !is_player_bus(bus_name)) {
			continue;
		}
		const int bus = audio_server->get_bus_index(bus_name);
		if (bus < 0) {
			// Kept in the settings, so the value comes back if a later layout has the bus again.
			if (!warned_buses.has(bus_name)) {
				warned_buses.insert(bus_name);
				missing.push_back(bus_name);
			}
			continue;
		}
		if (stored & AudioBusSettingsRow::STORED_VOLUME) {
			audio_server->set_bus_volume_db(bus, entry->get_volume_db());
		}
		if (stored & AudioBusSettingsRow::STORED_MUTE) {
			audio_server->set_bus_mute(bus, entry->is_mute());
		}
		if (stored & AudioBusSettingsRow::STORED_SOLO) {
			audio_server->set_bus_solo(bus, entry->is_solo());
		}
	}
	if (!missing.is_empty()) {
		WARN_PRINT(vformat("Audio bus settings: the bus layout has no bus %s; the stored values are kept but not applied.", String(", ").join(missing)));
	}
}

void AudioBusSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	const AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr) {
		return;
	}
	_connect_audio_server();
	if (layout_buses.is_empty()) {
		// The first read (the defaults): the layout before the player's values.
		for (int i = 0; i < audio_server->get_bus_count(); i++) {
			layout_buses.push_back(_make_entry_from_engine(i));
		}
	}
	const uint32_t stored = _get_stored_values();
	const StringName property = get_list_property();
	const Array previous = p_settings->get(property);
	TypedArray<AudioBusSettings> buses;
	for (int i = 0; i < audio_server->get_bus_count(); i++) {
		const String bus_name = audio_server->get_bus_name(i);
		if (!is_player_bus(bus_name)) {
			continue;
		}
		const Ref<AudioBusSettings> entry = _make_entry_from_engine(i);
		_set_unstored_values(entry, _get_layout_entry(bus_name), stored);
		buses.push_back(entry);
	}
	// Entries of player buses that the current layout does not have stay.
	for (int j = 0; j < previous.size(); j++) {
		const Ref<AudioBusSettings> entry = previous[j];
		if (entry.is_valid() && is_player_bus(entry->get_bus_name()) && audio_server->get_bus_index(entry->get_bus_name()) < 0) {
			buses.push_back(copy_value(entry));
		}
	}
	p_settings->set(property, buses);
	signaled_buses = Array(copy_value(buses));
}

Ref<Resource> AudioBusSettingsServer::_settings_to_storage(const Ref<Resource> &p_settings) const {
	const Ref<AudioBusListSettings> source = p_settings;
	ERR_FAIL_COND_V(source.is_null(), Ref<Resource>());
	if (p_settings != get_default_settings()) {
		// save() converts the data to write first (load() and the format check convert the defaults): from here
		// until `saved` or `save_failed`, the rows write only the stored values.
		AudioBusSettingsRow::set_written_values(_get_stored_values());
	}
	TypedArray<AudioBusSettingsRow> rows;
	const TypedArray<AudioBusSettings> buses = source->get_buses();
	for (int i = 0; i < buses.size(); i++) {
		const Ref<AudioBusSettings> entry = buses[i];
		if (entry.is_null()) {
			continue;
		}
		Ref<AudioBusSettingsRow> row;
		row.instantiate();
		row->set_bus_name(entry->get_bus_name());
		row->set_volume_db(entry->get_volume_db());
		row->set_mute(entry->is_mute());
		row->set_solo(entry->is_solo());
		rows.push_back(row);
	}
	Ref<AudioBusSettingsRows> storage;
	storage.instantiate();
	storage->set_buses(rows);
	return storage;
}

Ref<Resource> AudioBusSettingsServer::_settings_from_storage(const Ref<Resource> &p_storage) const {
	const Ref<AudioBusSettingsRows> storage = p_storage;
	ERR_FAIL_COND_V(storage.is_null(), Ref<Resource>());
	// Values the project does not store (any more) are ignored; stored values missing in the file come from the layout.
	const uint32_t stored = _get_stored_values();
	TypedArray<AudioBusSettings> buses;
	const TypedArray<AudioBusSettingsRow> rows = storage->get_buses();
	for (int i = 0; i < rows.size(); i++) {
		const Ref<AudioBusSettingsRow> row = rows[i];
		if (row.is_null() || row->get_bus_name().is_empty()) {
			continue;
		}
		Ref<AudioBusSettings> entry = _get_layout_entry(row->get_bus_name());
		if (entry.is_null()) {
			entry.instantiate();
			entry->set_bus_name(row->get_bus_name());
		}
		if ((stored & AudioBusSettingsRow::STORED_VOLUME) && row->has_value(AudioBusSettingsRow::STORED_VOLUME)) {
			entry->set_volume_db(row->get_volume_db());
		}
		if ((stored & AudioBusSettingsRow::STORED_MUTE) && row->has_value(AudioBusSettingsRow::STORED_MUTE)) {
			entry->set_mute(row->is_mute());
		}
		if ((stored & AudioBusSettingsRow::STORED_SOLO) && row->has_value(AudioBusSettingsRow::STORED_SOLO)) {
			entry->set_solo(row->is_solo());
		}
		buses.push_back(entry);
	}
	Ref<AudioBusListSettings> result;
	result.instantiate();
	result->set_buses(buses);
	return result;
}

void AudioBusSettingsServer::_on_save_finished() {
	// Rows read from files accept every value again.
	AudioBusSettingsRow::set_written_values(AudioBusSettingsRow::STORED_ALL);
}

void AudioBusSettingsServer::_on_loaded() {
	if (AudioServer::get_singleton() == nullptr) {
		return;
	}
	const StringName property = get_list_property();
	const Array layout = _make_layout_entries();
	// The layout is the default of this domain, also if the project names a defaults resource for it.
	const Ref<Resource> default_settings = get_default_settings();
	if (!values_equal(default_settings->get(property), layout)) {
		default_settings->set(property, copy_value(layout));
	}

	// One entry per player bus of the layout, in layout order. A file from an older game version has no entries for
	// buses added since: they keep their layout values. Entries of player buses the layout does not have follow.
	const uint32_t stored = _get_stored_values();
	const Ref<Resource> pending = get_pending_settings();
	const Array current = pending->get(property);
	TypedArray<AudioBusSettings> merged;
	HashSet<String> used;
	for (int i = 0; i < layout.size(); i++) {
		const Ref<AudioBusSettings> layout_entry = layout[i];
		const String bus_name = layout_entry->get_bus_name();
		used.insert(bus_name);
		const int index = _find_bus_entry(current, bus_name);
		if (index < 0) {
			merged.push_back(copy_value(layout_entry));
			continue;
		}
		const Ref<AudioBusSettings> entry = copy_value(current[index]);
		_set_unstored_values(entry, layout_entry, stored);
		merged.push_back(entry);
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
	for (int j = 0; j < current.size(); j++) {
		const Ref<AudioBusSettings> entry = current[j];
		if (entry.is_null() || used.has(entry->get_bus_name())) {
			continue;
		}
		// Buses of the layout that are not player buses are dropped; unknown player buses stay in the file.
		if (is_player_bus(entry->get_bus_name()) && audio_server->get_bus_index(entry->get_bus_name()) < 0) {
			merged.push_back(copy_value(entry));
		}
	}
	if (!values_equal(merged, current)) {
		pending->set(property, merged);
	}
}

void AudioBusSettingsServer::_connect_audio_server() {
	// AudioServer does not exist yet when this server is constructed: connected on the first read or apply (startup).
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &AudioBusSettingsServer::_on_bus_layout_changed);
	if (!audio_server->is_connected(SNAME("bus_layout_changed"), callback)) {
		audio_server->connect(SNAME("bus_layout_changed"), callback);
	}
}

void AudioBusSettingsServer::_on_bus_layout_changed() {
	// A new layout (set_bus_layout(), add_bus(), remove_bus(), ...) resets the bus values: the applied values of the
	// player buses are pushed again; other buses keep the values of the new layout. Before startup the layout becomes
	// the defaults instead.
	if (!is_started()) {
		return;
	}
	const Ref<Resource> applied_settings = get_applied_settings();
	if (applied_settings.is_null()) {
		return;
	}
	PackedStringArray names;
	names.push_back(get_list_property());
	_apply_settings(applied_settings, names);
	emit_signal(SNAME("bus_layout_changed"));
}

/////////////////////////////////////////////////////////////////////////////
// Player buses and stored values.

void AudioBusSettingsServer::set_player_buses(const PackedStringArray &p_buses) {
	player_buses = p_buses;
}

PackedStringArray AudioBusSettingsServer::get_player_buses() const {
	return player_buses;
}

bool AudioBusSettingsServer::is_player_bus(const StringName &p_bus) const {
	if (String(p_bus).is_empty()) {
		return false;
	}
	return player_buses.is_empty() || player_buses.has(String(p_bus));
}

void AudioBusSettingsServer::set_store_volume(bool p_enabled) {
	store_volume = p_enabled;
}

bool AudioBusSettingsServer::is_storing_volume() const {
	return store_volume;
}

void AudioBusSettingsServer::set_store_mute(bool p_enabled) {
	store_mute = p_enabled;
}

bool AudioBusSettingsServer::is_storing_mute() const {
	return store_mute;
}

void AudioBusSettingsServer::set_store_solo(bool p_enabled) {
	store_solo = p_enabled;
}

bool AudioBusSettingsServer::is_storing_solo() const {
	return store_solo;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

PackedStringArray AudioBusSettingsServer::get_bus_names() const {
	PackedStringArray names;
	const TypedArray<Resource> entries = get_entries();
	for (int i = 0; i < entries.size(); i++) {
		const Ref<Resource> entry = entries[i];
		if (entry.is_valid()) {
			names.push_back(entry->get(SNAME("bus_name")));
		}
	}
	return names;
}

bool AudioBusSettingsServer::has_bus(const StringName &p_bus) const {
	if (find_entry(String(p_bus)) >= 0) {
		return true;
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
	return audio_server != nullptr && audio_server->get_bus_index(p_bus) >= 0;
}

bool AudioBusSettingsServer::_check_settable(const StringName &p_bus, bool p_stored, const char *p_value) const {
	if (!p_stored) {
		WARN_PRINT(vformat("Audio bus settings: the %s of a bus is not stored; enable the project setting \"settings/audio_bus/store_%s\" to let the player change it.", p_value, p_value));
		return false;
	}
	if (!is_player_bus(p_bus)) {
		WARN_PRINT(vformat("Audio bus settings: \"%s\" is not a player bus (project setting \"%s\"); its values come from the bus layout.", p_bus, SETTING_PLAYER_BUSES));
		return false;
	}
	return true;
}

void AudioBusSettingsServer::_set_bus_value(const StringName &p_bus, const StringName &p_name, const Variant &p_value) {
	const int index = find_entry(String(p_bus));
	if (index >= 0) {
		set_entry_value(index, p_name, p_value);
		return;
	}
	// A player bus of the layout without an entry yet (a layout set by the game): the entry starts from the layout values.
	const Ref<AudioBusSettings> entry = _get_layout_entry(p_bus);
	ERR_FAIL_COND_MSG(entry.is_null(), vformat("Audio bus settings: there is no bus \"%s\".", p_bus));
	entry->set(p_name, p_value);
	add_entry(entry);
}

Variant AudioBusSettingsServer::_get_bus_value(const StringName &p_bus, const StringName &p_name) const {
	const int index = find_entry(String(p_bus));
	if (index >= 0) {
		return get_entry_value(index, p_name);
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
	const int bus = audio_server != nullptr ? audio_server->get_bus_index(p_bus) : -1;
	ERR_FAIL_COND_V_MSG(bus < 0, Variant(), vformat("Audio bus settings: there is no bus \"%s\".", p_bus));
	return _make_entry_from_engine(bus)->get(p_name);
}

void AudioBusSettingsServer::set_bus_volume_db(const StringName &p_bus, float p_volume_db) {
	if (!_check_settable(p_bus, store_volume, "volume")) {
		return;
	}
	const float volume_db = Math::is_nan(p_volume_db) ? 0.0f : CLAMP(p_volume_db, AudioBusSettings::MIN_VOLUME_DB, AudioBusSettings::MAX_VOLUME_DB);
	_set_bus_value(p_bus, SNAME("volume_db"), volume_db);
}

float AudioBusSettingsServer::get_bus_volume_db(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("volume_db"));
	return value.get_type() == Variant::NIL ? 0.0f : float(value);
}

void AudioBusSettingsServer::set_bus_volume_linear(const StringName &p_bus, float p_volume_linear) {
	set_bus_volume_db(p_bus, p_volume_linear > 0.0f ? Math::linear_to_db(p_volume_linear) : AudioBusSettings::MIN_VOLUME_DB);
}

float AudioBusSettingsServer::get_bus_volume_linear(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("volume_db"));
	if (value.get_type() == Variant::NIL) {
		return 0.0f;
	}
	const float volume_db = value;
	return volume_db <= AudioBusSettings::MIN_VOLUME_DB ? 0.0f : Math::db_to_linear(volume_db);
}

void AudioBusSettingsServer::set_bus_mute(const StringName &p_bus, bool p_mute) {
	if (!_check_settable(p_bus, store_mute, "mute")) {
		return;
	}
	_set_bus_value(p_bus, SNAME("mute"), p_mute);
}

bool AudioBusSettingsServer::is_bus_mute(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("mute"));
	return value.get_type() != Variant::NIL && bool(value);
}

void AudioBusSettingsServer::set_bus_solo(const StringName &p_bus, bool p_solo) {
	if (!_check_settable(p_bus, store_solo, "solo")) {
		return;
	}
	_set_bus_value(p_bus, SNAME("solo"), p_solo);
}

bool AudioBusSettingsServer::is_bus_solo(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("solo"));
	return value.get_type() != Variant::NIL && bool(value);
}

/////////////////////////////////////////////////////////////////////////////
// Presets.

Error AudioBusSettingsServer::apply_preset(const Ref<AudioBusListSettings> &p_preset) {
	ERR_FAIL_COND_V_MSG(p_preset.is_null(), ERR_INVALID_PARAMETER, "Audio bus settings: the preset is null.");
	const StringName property = get_list_property();
	const uint32_t stored = _get_stored_values();
	Array list = copy_value(Array(get_settings()->get(property)));
	const TypedArray<AudioBusSettings> preset_buses = p_preset->get_buses();
	for (int i = 0; i < preset_buses.size(); i++) {
		const Ref<AudioBusSettings> preset_entry = preset_buses[i];
		if (preset_entry.is_null() || !is_player_bus(preset_entry->get_bus_name())) {
			continue;
		}
		int index = _find_bus_entry(list, preset_entry->get_bus_name());
		if (index < 0) {
			const Ref<AudioBusSettings> layout_entry = _get_layout_entry(preset_entry->get_bus_name());
			if (layout_entry.is_null()) {
				continue; // Neither stored nor in the layout.
			}
			list.push_back(layout_entry);
			index = list.size() - 1;
		}
		const Ref<AudioBusSettings> entry = list[index];
		if (stored & AudioBusSettingsRow::STORED_VOLUME) {
			entry->set_volume_db(preset_entry->get_volume_db());
		}
		if (stored & AudioBusSettingsRow::STORED_MUTE) {
			entry->set_mute(preset_entry->is_mute());
		}
		if (stored & AudioBusSettingsRow::STORED_SOLO) {
			entry->set_solo(preset_entry->is_solo());
		}
	}
	current_preset = p_preset;
	// One change of the list: applied at once in APPLY_MODE_IMMEDIATE.
	set_value(property, list);
	return OK;
}

bool AudioBusSettingsServer::matches_preset(const Ref<AudioBusListSettings> &p_preset) const {
	if (p_preset.is_null()) {
		return false;
	}
	const uint32_t stored = _get_stored_values();
	const Array list = get_settings()->get(get_list_property());
	const TypedArray<AudioBusSettings> preset_buses = p_preset->get_buses();
	for (int i = 0; i < preset_buses.size(); i++) {
		const Ref<AudioBusSettings> preset_entry = preset_buses[i];
		if (preset_entry.is_null() || !is_player_bus(preset_entry->get_bus_name())) {
			continue;
		}
		const int index = _find_bus_entry(list, preset_entry->get_bus_name());
		if (index < 0) {
			continue;
		}
		const Ref<AudioBusSettings> entry = list[index];
		if ((stored & AudioBusSettingsRow::STORED_VOLUME) && !Math::is_equal_approx(entry->get_volume_db(), preset_entry->get_volume_db())) {
			return false;
		}
		if ((stored & AudioBusSettingsRow::STORED_MUTE) && entry->is_mute() != preset_entry->is_mute()) {
			return false;
		}
		if ((stored & AudioBusSettingsRow::STORED_SOLO) && entry->is_solo() != preset_entry->is_solo()) {
			return false;
		}
	}
	return true;
}

Ref<AudioBusListSettings> AudioBusSettingsServer::get_current_preset() const {
	// A change of a single value makes the current preset "Custom" until the values match the preset again.
	if (current_preset.is_valid() && matches_preset(current_preset)) {
		return current_preset;
	}
	return Ref<AudioBusListSettings>();
}

bool AudioBusSettingsServer::is_custom() const {
	return get_current_preset().is_null();
}

/////////////////////////////////////////////////////////////////////////////
// Signals.

void AudioBusSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	ListSettingsServer::_notify_value_applied(p_name, p_value);
	if (p_name != get_list_property()) {
		return;
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
	const uint32_t stored = _get_stored_values();
	const Array buses = p_value;
	const Array previous = signaled_buses;
	signaled_buses = Array(copy_value(buses));
	PackedStringArray previous_names;
	Array previous_entries;
	for (int j = 0; j < previous.size(); j++) {
		const Ref<Resource> entry = previous[j];
		if (entry.is_valid()) {
			previous_names.push_back(entry->get(SNAME("bus_name")));
			previous_entries.push_back(entry);
		}
	}
	PackedStringArray bus_names;
	for (int i = 0; i < buses.size(); i++) {
		const Ref<Resource> entry = buses[i];
		if (entry.is_null()) {
			continue;
		}
		const String bus_name = entry->get(SNAME("bus_name"));
		bus_names.push_back(bus_name);
		// Values of buses that the layout does not have, or that are not player buses, did not reach AudioServer.
		if (!is_player_bus(bus_name) || (audio_server != nullptr && audio_server->get_bus_index(bus_name) < 0)) {
			continue;
		}
		Ref<Resource> old_entry;
		const int64_t old_index = previous_names.find(bus_name);
		if (old_index >= 0) {
			old_entry = previous_entries[old_index];
		}
		const float volume_db = entry->get(SNAME("volume_db"));
		const bool mute = entry->get(SNAME("mute"));
		const bool solo = entry->get(SNAME("solo"));
		if ((stored & AudioBusSettingsRow::STORED_VOLUME) && (old_entry.is_null() || !Math::is_equal_approx(float(old_entry->get(SNAME("volume_db"))), volume_db))) {
			// The 32-bit member reads back as -12.300000190734863; the signal carries -12.3 (#189).
			emit_signal(SNAME("bus_volume_changed"), StringName(bus_name), snap_property_value(entry.ptr(), SNAME("volume_db"), double(volume_db)));
		}
		if ((stored & AudioBusSettingsRow::STORED_MUTE) && (old_entry.is_null() || bool(old_entry->get(SNAME("mute"))) != mute)) {
			emit_signal(SNAME("bus_mute_changed"), StringName(bus_name), mute);
		}
		if ((stored & AudioBusSettingsRow::STORED_SOLO) && (old_entry.is_null() || bool(old_entry->get(SNAME("solo"))) != solo)) {
			emit_signal(SNAME("bus_solo_changed"), StringName(bus_name), solo);
		}
	}
	if (!(bus_names == previous_names)) {
		emit_signal(SNAME("bus_layout_changed"));
	}
}

void AudioBusSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("bus_volume_changed", PropertyInfo(Variant::STRING_NAME, "bus_name"), PropertyInfo(Variant::FLOAT, "volume_db")));
	ADD_SIGNAL(MethodInfo("bus_mute_changed", PropertyInfo(Variant::STRING_NAME, "bus_name"), PropertyInfo(Variant::BOOL, "muted")));
	ADD_SIGNAL(MethodInfo("bus_solo_changed", PropertyInfo(Variant::STRING_NAME, "bus_name"), PropertyInfo(Variant::BOOL, "solo")));
	ADD_SIGNAL(MethodInfo("bus_layout_changed"));

	ClassDB::bind_method(D_METHOD("set_player_buses", "buses"), &AudioBusSettingsServer::set_player_buses);
	ClassDB::bind_method(D_METHOD("get_player_buses"), &AudioBusSettingsServer::get_player_buses);
	ClassDB::bind_method(D_METHOD("is_player_bus", "bus"), &AudioBusSettingsServer::is_player_bus);
	ClassDB::bind_method(D_METHOD("set_store_volume", "enabled"), &AudioBusSettingsServer::set_store_volume);
	ClassDB::bind_method(D_METHOD("is_storing_volume"), &AudioBusSettingsServer::is_storing_volume);
	ClassDB::bind_method(D_METHOD("set_store_mute", "enabled"), &AudioBusSettingsServer::set_store_mute);
	ClassDB::bind_method(D_METHOD("is_storing_mute"), &AudioBusSettingsServer::is_storing_mute);
	ClassDB::bind_method(D_METHOD("set_store_solo", "enabled"), &AudioBusSettingsServer::set_store_solo);
	ClassDB::bind_method(D_METHOD("is_storing_solo"), &AudioBusSettingsServer::is_storing_solo);

	ClassDB::bind_method(D_METHOD("get_bus_names"), &AudioBusSettingsServer::get_bus_names);
	ClassDB::bind_method(D_METHOD("has_bus", "bus"), &AudioBusSettingsServer::has_bus);
	ClassDB::bind_method(D_METHOD("set_bus_volume_db", "bus", "volume_db"), &AudioBusSettingsServer::set_bus_volume_db);
	ClassDB::bind_method(D_METHOD("get_bus_volume_db", "bus"), &AudioBusSettingsServer::get_bus_volume_db);
	ClassDB::bind_method(D_METHOD("set_bus_volume_linear", "bus", "volume_linear"), &AudioBusSettingsServer::set_bus_volume_linear);
	ClassDB::bind_method(D_METHOD("get_bus_volume_linear", "bus"), &AudioBusSettingsServer::get_bus_volume_linear);
	ClassDB::bind_method(D_METHOD("set_bus_mute", "bus", "mute"), &AudioBusSettingsServer::set_bus_mute);
	ClassDB::bind_method(D_METHOD("is_bus_mute", "bus"), &AudioBusSettingsServer::is_bus_mute);
	ClassDB::bind_method(D_METHOD("set_bus_solo", "bus", "solo"), &AudioBusSettingsServer::set_bus_solo);
	ClassDB::bind_method(D_METHOD("is_bus_solo", "bus"), &AudioBusSettingsServer::is_bus_solo);

	ClassDB::bind_method(D_METHOD("apply_preset", "preset"), &AudioBusSettingsServer::apply_preset);
	ClassDB::bind_method(D_METHOD("matches_preset", "preset"), &AudioBusSettingsServer::matches_preset);
	ClassDB::bind_method(D_METHOD("get_current_preset"), &AudioBusSettingsServer::get_current_preset);
	ClassDB::bind_method(D_METHOD("is_custom"), &AudioBusSettingsServer::is_custom);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "player_buses"), "set_player_buses", "get_player_buses");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "store_volume"), "set_store_volume", "is_storing_volume");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "store_mute"), "set_store_mute", "is_storing_mute");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "store_solo"), "set_store_solo", "is_storing_solo");
}

AudioBusSettingsServer::AudioBusSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings != nullptr) {
		player_buses = project_settings->get_setting_with_override(SETTING_PLAYER_BUSES);
		store_volume = project_settings->get_setting_with_override(SETTING_STORE_VOLUME);
		store_mute = project_settings->get_setting_with_override(SETTING_STORE_MUTE);
		store_solo = project_settings->get_setting_with_override(SETTING_STORE_SOLO);
	}
	connect(SNAME("loaded"), callable_mp(this, &AudioBusSettingsServer::_on_loaded));
	connect(SNAME("saved"), callable_mp(this, &AudioBusSettingsServer::_on_save_finished));
	connect(SNAME("save_failed"), callable_mp(this, &AudioBusSettingsServer::_on_save_finished).unbind(2));
}

AudioBusSettingsServer::~AudioBusSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
