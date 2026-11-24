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

static constexpr const char *SETTING_CREATE_MISSING_BUSES = "settings/audio/create_missing_buses";

AudioBusSettingsServer *AudioBusSettingsServer::singleton = nullptr;

AudioBusSettingsServer *AudioBusSettingsServer::get_singleton() {
	return singleton;
}

void AudioBusSettingsServer::register_settings() {
	GLOBAL_DEF(SETTING_CREATE_MISSING_BUSES, false);
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName AudioBusSettingsServer::get_domain() const {
	return SNAME("audio_buses");
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

Ref<AudioBusSettings> AudioBusSettingsServer::_make_entry_from_engine(int p_bus) {
	const AudioServer *audio_server = AudioServer::get_singleton();
	Ref<AudioBusSettings> entry;
	entry.instantiate();
	entry->set_bus_name(audio_server->get_bus_name(p_bus));
	entry->set_volume_db(audio_server->get_bus_volume_db(p_bus));
	entry->set_mute(audio_server->is_bus_mute(p_bus));
	entry->set_solo(audio_server->is_bus_solo(p_bus));
	return entry;
}

void AudioBusSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	const StringName property = get_list_property();
	AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr || !p_names.has(String(property))) {
		return;
	}
	_connect_audio_server();
	const Array buses = p_settings->get(property);
	PackedStringArray missing;
	pushing_buses = true;
	for (int i = 0; i < buses.size(); i++) {
		const Ref<Resource> entry = buses[i];
		if (entry.is_null()) {
			continue;
		}
		const String bus_name = entry->get(SNAME("bus_name"));
		if (bus_name.is_empty()) {
			continue;
		}
		int bus = audio_server->get_bus_index(bus_name);
		if (bus < 0) {
			if (!create_missing_buses) {
				// Kept in the settings, so the value comes back if a later layout has the bus again.
				if (!warned_buses.has(bus_name)) {
					warned_buses.insert(bus_name);
					missing.push_back(bus_name);
				}
				continue;
			}
			audio_server->add_bus();
			bus = audio_server->get_bus_count() - 1;
			audio_server->set_bus_name(bus, bus_name);
		}
		audio_server->set_bus_volume_db(bus, entry->get(SNAME("volume_db")));
		audio_server->set_bus_mute(bus, entry->get(SNAME("mute")));
		audio_server->set_bus_solo(bus, entry->get(SNAME("solo")));
	}
	pushing_buses = false;
	if (!missing.is_empty()) {
		WARN_PRINT(vformat("Audio bus settings: the bus layout has no bus %s; the stored values are kept but not applied. Enable the project setting \"%s\" to create them.", String(", ").join(missing), SETTING_CREATE_MISSING_BUSES));
	}
}

void AudioBusSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	const AudioServer *audio_server = AudioServer::get_singleton();
	if (audio_server == nullptr) {
		return;
	}
	_connect_audio_server();
	TypedArray<AudioBusSettings> buses;
	for (int i = 0; i < audio_server->get_bus_count(); i++) {
		buses.push_back(_make_entry_from_engine(i));
	}
	p_settings->set(get_list_property(), buses);
	signaled_buses = Array(copy_value(buses));
}

void AudioBusSettingsServer::_merge_layout_buses() {
	// A file from an older game version has no entries for buses added since: they keep their layout values.
	const StringName property = get_list_property();
	const Ref<Resource> pending = get_pending_settings();
	const Array layout = get_default_settings()->get(property);
	const Array current = pending->get(property);
	if (layout.is_empty()) {
		return;
	}
	TypedArray<AudioBusSettings> merged;
	HashSet<String> used;
	for (int i = 0; i < layout.size(); i++) {
		const Ref<Resource> layout_entry = layout[i];
		if (layout_entry.is_null()) {
			continue;
		}
		const String bus_name = layout_entry->get(SNAME("bus_name"));
		used.insert(bus_name);
		Variant stored;
		for (int j = 0; j < current.size(); j++) {
			const Ref<Resource> entry = current[j];
			if (entry.is_valid() && String(entry->get(SNAME("bus_name"))) == bus_name) {
				stored = current[j];
				break;
			}
		}
		merged.push_back(stored.get_type() != Variant::NIL ? stored : copy_value(layout_entry));
	}
	// Entries of buses that the layout does not have (see create_missing_buses) follow at the end.
	for (int j = 0; j < current.size(); j++) {
		const Ref<Resource> entry = current[j];
		if (entry.is_valid() && !used.has(String(entry->get(SNAME("bus_name"))))) {
			merged.push_back(current[j]);
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
	// A new layout (set_bus_layout(), add_bus(), remove_bus(), ...) resets the bus values: the applied values are
	// pushed again. Before startup the layout becomes the defaults instead; buses added by _apply_settings() itself
	// (create_missing_buses) are skipped, so the startup apply does not run twice.
	if (pushing_buses || !is_started()) {
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
// Typed API.

void AudioBusSettingsServer::set_create_missing_buses(bool p_enabled) {
	create_missing_buses = p_enabled;
}

bool AudioBusSettingsServer::is_create_missing_buses_enabled() const {
	return create_missing_buses;
}

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

void AudioBusSettingsServer::_set_bus_value(const StringName &p_bus, const StringName &p_name, const Variant &p_value) {
	const int index = find_entry(String(p_bus));
	if (index >= 0) {
		set_entry_value(index, p_name, p_value);
		return;
	}
	// A bus of the layout without an entry yet: the entry starts from the engine values.
	const AudioServer *audio_server = AudioServer::get_singleton();
	const int bus = audio_server != nullptr ? audio_server->get_bus_index(p_bus) : -1;
	ERR_FAIL_COND_MSG(bus < 0, vformat("Audio bus settings: there is no bus \"%s\".", p_bus));
	const Ref<AudioBusSettings> entry = _make_entry_from_engine(bus);
	entry->set(p_name, p_value);
	add_entry(entry, bus);
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
	_set_bus_value(p_bus, SNAME("mute"), p_mute);
}

bool AudioBusSettingsServer::is_bus_mute(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("mute"));
	return value.get_type() != Variant::NIL && bool(value);
}

void AudioBusSettingsServer::set_bus_solo(const StringName &p_bus, bool p_solo) {
	_set_bus_value(p_bus, SNAME("solo"), p_solo);
}

bool AudioBusSettingsServer::is_bus_solo(const StringName &p_bus) const {
	const Variant value = _get_bus_value(p_bus, SNAME("solo"));
	return value.get_type() != Variant::NIL && bool(value);
}

void AudioBusSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	ListSettingsServer::_notify_value_applied(p_name, p_value);
	if (p_name != get_list_property()) {
		return;
	}
	const AudioServer *audio_server = AudioServer::get_singleton();
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
		// Values of buses that the layout does not have did not reach AudioServer.
		if (bus_name.is_empty() || (audio_server != nullptr && audio_server->get_bus_index(bus_name) < 0)) {
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
		if (old_entry.is_null() || !Math::is_equal_approx(float(old_entry->get(SNAME("volume_db"))), volume_db)) {
			emit_signal(SNAME("bus_volume_changed"), StringName(bus_name), volume_db);
		}
		if (old_entry.is_null() || bool(old_entry->get(SNAME("mute"))) != mute) {
			emit_signal(SNAME("bus_mute_changed"), StringName(bus_name), mute);
		}
		if (old_entry.is_null() || bool(old_entry->get(SNAME("solo"))) != solo) {
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

	ClassDB::bind_method(D_METHOD("set_create_missing_buses", "enabled"), &AudioBusSettingsServer::set_create_missing_buses);
	ClassDB::bind_method(D_METHOD("is_create_missing_buses_enabled"), &AudioBusSettingsServer::is_create_missing_buses_enabled);
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

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "create_missing_buses"), "set_create_missing_buses", "is_create_missing_buses_enabled");
}

AudioBusSettingsServer::AudioBusSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings != nullptr && project_settings->has_setting(SETTING_CREATE_MISSING_BUSES)) {
		create_missing_buses = project_settings->get_setting_with_override(SETTING_CREATE_MISSING_BUSES);
	}
	connect(SNAME("loaded"), callable_mp(this, &AudioBusSettingsServer::_merge_layout_buses));
}

AudioBusSettingsServer::~AudioBusSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
