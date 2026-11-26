/**************************************************************************/
/*  test_audio_settings_server.cpp                                        */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_audio_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "servers/audio/audio_bus_layout.h"
#include "servers/audio/audio_server.h"
#include "servers/settings/audio_bus_settings_server.h"
#include "servers/settings/audio_device_settings_server.h"
#include "servers/settings/audio_settings.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#include <cmath>

namespace TestAudioSettingsServer {

// Test names contain "[Audio]": the test runner then creates an AudioServer with the dummy driver
// (one bus, "Master") for each test case and frees it afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		AudioServer *audio_server = AudioServer::get_singleton();
		CHECK(audio_server != nullptr);
		if (audio_server == nullptr) {
			return;
		}
		// The layout of the game: Master and Music.
		audio_server->add_bus();
		audio_server->set_bus_name(1, "Music");
		audio_server->set_bus_volume_db(1, -6.0);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_INI);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("audio_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static void write_file(const String &p_path, const String &p_text) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(p_text);
}

static Ref<AudioBusSettings> make_bus(const String &p_name, float p_volume_db, bool p_mute = false, bool p_solo = false) {
	Ref<AudioBusSettings> bus;
	bus.instantiate();
	bus->set_bus_name(p_name);
	bus->set_volume_db(p_volume_db);
	bus->set_mute(p_mute);
	bus->set_solo(p_solo);
	return bus;
}

TEST_CASE("[Audio][AudioDeviceSettingsServer] Both domains are engine singletons with the domain names audio_device and audio_bus") {
	TestSetup setup;
	AudioDeviceSettingsServer *audio = AudioDeviceSettingsServer::get_singleton();
	AudioBusSettingsServer *buses = AudioBusSettingsServer::get_singleton();
	REQUIRE(audio != nullptr);
	REQUIRE(buses != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("AudioDeviceSettingsServer"));
	CHECK(Engine::get_singleton()->has_singleton("AudioBusSettingsServer"));

	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	CHECK(audio->get_domain() == StringName("audio_device"));
	CHECK(buses->get_domain() == StringName("audio_bus"));
	CHECK(manager->get_server("audio_device") == audio);
	CHECK(manager->get_server("audio_bus") == buses);
	CHECK(manager->get_server("audio") == nullptr);
	CHECK(manager->get_server("audio_buses") == nullptr);
	CHECK(audio->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(buses->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);

	// The project settings of the bus domain; create_missing_buses is gone.
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	CHECK(project_settings->has_setting("settings/audio_bus/player_buses"));
	CHECK(bool(project_settings->get_setting("settings/audio_bus/store_volume")));
	CHECK(bool(project_settings->get_setting("settings/audio_bus/store_mute")));
	CHECK_FALSE(bool(project_settings->get_setting("settings/audio_bus/store_solo")));
	CHECK_FALSE(project_settings->has_setting("settings/audio/create_missing_buses"));
	CHECK(buses->get_player_buses().is_empty());
	CHECK(buses->is_storing_volume());
	CHECK(buses->is_storing_mute());
	CHECK_FALSE(buses->is_storing_solo());

	CHECK(audio->get_file_path() == "user://settings/audio_device.ini");
	CHECK(buses->get_file_path() == "user://settings/audio_bus.ini");
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_CSV);
	CHECK(buses->get_file_path() == "user://settings/audio_bus.csv");
	ERR_PRINT_OFF;
	// The devices are one record: no CSV.
	CHECK(audio->get_file_path() == "user://settings/audio_device.ini");
	ERR_PRINT_ON;
}

TEST_CASE("[Audio][AudioBusSettingsServer] Defaults come from the bus layout; only stored values are settable") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	CHECK(server->get_entry_count() == 2);
	CHECK(server->get_bus_names() == PackedStringArray({ "Master", "Music" }));
	CHECK(server->has_bus("Music"));
	CHECK_FALSE(server->has_bus("Voice"));
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(-6.0));
	CHECK(server->get_bus_volume_linear("Master") == doctest::Approx(1.0));

	server->set_bus_volume_db("Music", -12.0);
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-12.0));
	server->set_bus_volume_linear("Master", 0.5);
	// -6.0206 dB, snapped to the 0.1 dB step of the volume (#189).
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(-6.0));
	CHECK(double(server->get_entry_value(server->find_entry(String("Master")), "volume_db")) == -6.0);
	server->set_bus_volume_linear("Master", 1.0);
	const double master_db = server->get_entry_value(server->find_entry(String("Master")), "volume_db");
	CHECK(master_db == 0.0);
	CHECK_FALSE(std::signbit(master_db));
	server->set_bus_mute("Music", true);
	CHECK(audio_server->is_bus_mute(1));
	CHECK(server->is_bus_mute("Music"));

	// Solo is not stored by default: a warning, no change.
	ERR_PRINT_OFF;
	server->set_bus_solo("Music", true);
	ERR_PRINT_ON;
	CHECK_FALSE(audio_server->is_bus_solo(1));
	CHECK_FALSE(server->is_bus_solo("Music"));
	server->set_store_solo(true);
	server->set_bus_solo("Music", true);
	CHECK(audio_server->is_bus_solo(1));

	// Clamped to the range of the bus slider.
	server->set_bus_volume_db("Music", 100.0);
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(24.0));
	server->set_bus_volume_linear("Music", 0.0);
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(-80.0));

	ERR_PRINT_OFF;
	server->set_bus_volume_db("Voice", -3.0);
	ERR_PRINT_ON;
	CHECK(server->get_entry_count() == 2);

	// The default stays the layout value.
	const Array default_buses = server->get_default_value("buses");
	CHECK(default_buses.size() == 2);
	server->reset_to_defaults();
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-6.0));
	CHECK_FALSE(audio_server->is_bus_mute(1));
	CHECK_FALSE(audio_server->is_bus_solo(1));
	memdelete(server);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Saved values are applied on startup; the file holds only stored values (INI, JSON, CSV)") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	for (const String &extension : { String("ini"), String("json"), String("csv") }) {
		const String path = temp_file("buses." + extension);
		// Every round starts from the layout of the game.
		audio_server->set_bus_volume_db(1, -6.0);
		audio_server->set_bus_mute(1, false);
		{
			AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
			server->set_file_path(path);
			server->set_bus_volume_db("Music", -20.0);
			server->set_bus_mute("Music", true);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path);
		CHECK(text.contains("volume_db"));
		CHECK(text.contains("mute"));
		// Solo is not stored.
		CHECK_FALSE(text.contains("solo"));
		CHECK(AudioBusSettingsRow::get_written_values() == uint32_t(AudioBusSettingsRow::STORED_ALL));
		if (extension == "csv") {
			// One row per bus.
			const Vector<String> lines = text.strip_edges().split("\n");
			int music_rows = 0;
			for (const String &line : lines) {
				music_rows += line.contains("Music") ? 1 : 0;
			}
			CHECK(music_rows == 1);
		}

		// "Restart": the engine is back at the layout values.
		audio_server->set_bus_volume_db(1, -6.0);
		audio_server->set_bus_mute(1, false);

		AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-20.0));
		CHECK(audio_server->is_bus_mute(1));
		CHECK(server->get_bus_volume_db("Music") == doctest::Approx(-20.0));
		const Array default_buses = server->get_default_value("buses");
		CHECK(default_buses.size() == 2);
		CHECK_FALSE(server->is_dirty());
		CHECK_FALSE(server->has_unsaved_changes());
		memdelete(server);
	}
}

TEST_CASE("[Audio][AudioBusSettingsServer] Values in a file that are not stored are ignored; stored values missing in a file come from the layout") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	// The layout mutes Music and solos Master.
	audio_server->set_bus_mute(1, true);
	audio_server->set_bus_solo(0, true);
	const String path = temp_file("hand_edited.csv");
	// A file from another game version: solo (not stored) and no mute column (stored).
	write_file(path, "bus_name,volume_db,solo\nMaster,-3,false\nMusic,-20,true\n");

	// A second server that does not store the volume; its defaults are captured before the first one applies.
	AudioBusSettingsServer *without_volume = memnew(AudioBusSettingsServer);
	without_volume->set_store_volume(false);
	CHECK(without_volume->get_entry_count() == 2);

	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(-3.0));
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-20.0));
	// Mute from the layout, solo untouched.
	CHECK(server->is_bus_mute("Music"));
	CHECK(audio_server->is_bus_mute(1));
	CHECK(audio_server->is_bus_solo(0));
	CHECK_FALSE(audio_server->is_bus_solo(1));
	CHECK(server->is_bus_solo("Master"));
	CHECK_FALSE(server->is_bus_solo("Music"));

	// With the volume not stored, the file's volumes are ignored too.
	without_volume->set_file_path(path);
	CHECK(without_volume->load() == OK);
	CHECK(without_volume->get_bus_volume_db("Music") == doctest::Approx(-6.0));
	memdelete(without_volume);
	memdelete(server);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Player buses: other buses keep the layout, also after set_bus_layout()") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	const Ref<AudioBusLayout> game_layout = audio_server->generate_bus_layout();
	// A second layout of the game: Master at -3 dB, Music at -10 dB.
	audio_server->set_bus_volume_db(0, -3.0);
	audio_server->set_bus_volume_db(1, -10.0);
	const Ref<AudioBusLayout> underwater_layout = audio_server->generate_bus_layout();
	audio_server->set_bus_layout(game_layout);

	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	server->set_player_buses({ "Music" });
	server->set_file_path(temp_file("player_buses.ini"));
	server->startup();
	CHECK(server->is_player_bus("Music"));
	CHECK_FALSE(server->is_player_bus("Master"));
	CHECK(server->get_bus_names() == PackedStringArray({ "Music" }));

	ERR_PRINT_OFF;
	server->set_bus_volume_db("Master", -30.0);
	ERR_PRINT_ON;
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(0.0));
	server->set_bus_volume_db("Music", -20.0);
	REQUIRE(audio_server->get_bus_volume_db(1) == doctest::Approx(-20.0));

	// The game switches the layout: Music gets the player's value again, Master keeps the mix of the new layout.
	audio_server->set_bus_layout(underwater_layout);
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-20.0));
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(-3.0));
	CHECK_FALSE(server->is_dirty());

	// Saved: only the player bus is in the file.
	CHECK(server->save() == OK);
	CHECK_FALSE(FileAccess::get_file_as_string(server->get_file_path()).contains("Master"));

	// A layout without "Music": the entry stays and is skipped with a warning; no bus is created.
	Ref<AudioBusLayout> master_only;
	master_only.instantiate();
	ERR_PRINT_OFF;
	audio_server->set_bus_layout(master_only);
	ERR_PRINT_ON;
	CHECK(audio_server->get_bus_count() == 1);
	CHECK(server->find_entry("Music") >= 0);
	memdelete(server);

	// The freed server is no longer connected to the signal.
	audio_server->set_bus_layout(game_layout);
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-6.0));
}

TEST_CASE("[Audio][AudioBusSettingsServer] Buses missing in the layout stay in the file; buses added later keep their layout values") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	const String path = temp_file("missing.ini");
	{
		AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_file_path(path);
		server->set_bus_volume_db("Music", -15.0);
		server->add_entry(make_bus("Voice", -9.0));
		ERR_PRINT_OFF;
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(audio_server->get_bus_index("Voice") == -1);
		CHECK(server->save() == OK);
		memdelete(server);
	}
	// A game update adds a bus between the existing ones.
	audio_server->add_bus(1);
	audio_server->set_bus_name(1, "Sfx");
	audio_server->set_bus_volume_db(1, -3.0);
	const int music = audio_server->get_bus_index("Music");
	audio_server->set_bus_volume_db(music, -6.0);

	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	server->set_file_path(path);
	ERR_PRINT_OFF;
	server->startup();
	ERR_PRINT_ON;
	CHECK(server->get_bus_names() == PackedStringArray({ "Master", "Sfx", "Music", "Voice" }));
	CHECK(audio_server->get_bus_count() == 3);
	CHECK(audio_server->get_bus_volume_db(music) == doctest::Approx(-15.0));
	CHECK(server->get_bus_volume_db("Sfx") == doctest::Approx(-3.0));
	CHECK(server->save() == OK);
	CHECK(FileAccess::get_file_as_string(path).contains("Voice"));
	memdelete(server);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Presets: buses not listed stay unchanged, a single change makes it Custom") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	server->set_file_path(temp_file("preset.ini"));
	server->startup();
	server->set_bus_volume_db("Master", -2.0);
	CHECK(server->is_custom());
	CHECK(server->get_current_preset().is_null());

	// The preset lists Music only, with a solo value that the project does not store.
	Ref<AudioBusListSettings> preset;
	preset.instantiate();
	TypedArray<AudioBusSettings> preset_buses;
	preset_buses.push_back(make_bus("Music", -18.0, true, true));
	preset_buses.push_back(make_bus("Voice", -1.0));
	preset->set_buses(preset_buses);

	CHECK_FALSE(server->matches_preset(preset));
	CHECK(server->apply_preset(preset) == OK);
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-18.0));
	CHECK(audio_server->is_bus_mute(1));
	CHECK_FALSE(audio_server->is_bus_solo(1));
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(-2.0));
	CHECK(server->get_entry_count() == 2);
	CHECK(server->matches_preset(preset));
	CHECK(server->get_current_preset() == preset);
	CHECK_FALSE(server->is_custom());

	// A change of a bus the preset does not list keeps the preset.
	server->set_bus_volume_db("Master", -4.0);
	CHECK_FALSE(server->is_custom());
	// A change of a listed value: Custom, until it matches again.
	server->set_bus_volume_db("Music", -10.0);
	CHECK(server->is_custom());
	CHECK(server->get_current_preset().is_null());
	server->set_bus_volume_db("Music", -18.0);
	CHECK(server->get_current_preset() == preset);

	ERR_PRINT_OFF;
	CHECK(server->apply_preset(Ref<AudioBusListSettings>()) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[Audio][AudioDeviceSettingsServer] Devices: unknown names fall back to \"Default\" without an error") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioDeviceSettingsServer *server = memnew(AudioDeviceSettingsServer);
	CHECK(server->get_domain() == StringName("audio_device"));
	CHECK(server->get_output_device() == "Default");
	CHECK(server->get_input_device() == "Default");
	// The dummy driver has only "Default".
	CHECK_FALSE(AudioDeviceSettingsServer::is_device_selection_supported());
	CHECK_FALSE(server->is_value_supported("output_device"));
	CHECK_FALSE(server->is_value_supported("input_device"));

	// Set at runtime: replaced, with a verbose message only.
	server->set_output_device("A headset that was unplugged");
	CHECK(server->get_output_device() == "Default");
	CHECK(audio_server->get_output_device() == "Default");

	// Loaded from a file written on another machine.
	const String path = temp_file("devices.json");
	write_file(path, "{ \"output_device\": \"Speakers (Missing)\", \"input_device\": \"Microphone (Missing)\" }");
	server->set_file_path(path);
	CHECK(server->load() == OK);
	CHECK(server->get_output_device() == "Default");
	CHECK(server->get_input_device() == "Default");
	memdelete(server);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Typed bus signals fire once per applied change of a stored value") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	server->set_file_path(temp_file("signals.ini"));
	server->startup();
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "bus_volume_changed");
	SIGNAL_WATCH(server, "bus_mute_changed");
	SIGNAL_WATCH(server, "bus_solo_changed");
	SIGNAL_WATCH(server, "bus_layout_changed");

	// Pending changes do not reach AudioServer: no signal.
	server->set_bus_volume_db("Music", -12.0);
	server->set_bus_mute("Master", true);
	SIGNAL_CHECK_FALSE("bus_volume_changed");
	SIGNAL_CHECK_FALSE("bus_mute_changed");

	// apply(): one signal per changed bus value.
	CHECK(server->apply() == OK);
	const Array volume_args = { { StringName("Music"), -12.0 } };
	SIGNAL_CHECK("bus_volume_changed", volume_args);
	const Array mute_args = { { StringName("Master"), true } };
	SIGNAL_CHECK("bus_mute_changed", mute_args);
	SIGNAL_CHECK_FALSE("bus_layout_changed");

	// A solo value set through the generic API is not stored: never emitted.
	server->set_entry_value(server->find_entry("Music"), "solo", true);
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("bus_solo_changed");
	CHECK_FALSE(audio_server->is_bus_solo(1));

	// A new bus in the layout: the values are pushed again and the layout signal fires.
	audio_server->add_bus();
	// One call without arguments. Built explicitly, `Array({ Array() })` collapses to one level.
	Array layout_calls;
	layout_calls.push_back(Array());
	SIGNAL_CHECK("bus_layout_changed", layout_calls);

	SIGNAL_UNWATCH(server, "bus_volume_changed");
	SIGNAL_UNWATCH(server, "bus_mute_changed");
	SIGNAL_UNWATCH(server, "bus_solo_changed");
	SIGNAL_UNWATCH(server, "bus_layout_changed");
	memdelete(server);
}

} // namespace TestAudioSettingsServer
