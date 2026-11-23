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
#include "servers/audio/audio_server.h"
#include "servers/settings/audio_bus_settings_server.h"
#include "servers/settings/audio_settings.h"
#include "servers/settings/audio_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/test_utils.h"

namespace TestAudioSettingsServer {

// Test names contain "[Audio]": the test runner then creates an AudioServer with the dummy driver
// (one bus, "Master") for each test case and frees it afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
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
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/storage/format", "ini");
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

static Ref<AudioBusSettings> make_bus(const String &p_name, float p_volume_db) {
	Ref<AudioBusSettings> bus;
	bus.instantiate();
	bus->set_bus_name(p_name);
	bus->set_volume_db(p_volume_db);
	return bus;
}

TEST_CASE("[Audio][AudioSettingsServer] Both domains are engine singletons registered with the manager") {
	TestSetup setup;
	AudioSettingsServer *audio = AudioSettingsServer::get_singleton();
	AudioBusSettingsServer *buses = AudioBusSettingsServer::get_singleton();
	REQUIRE(audio != nullptr);
	REQUIRE(buses != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("AudioSettingsServer"));
	CHECK(Engine::get_singleton()->has_singleton("AudioBusSettingsServer"));

	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	CHECK(manager->get_server("audio") == audio);
	CHECK(manager->get_server("audio_buses") == buses);
	CHECK(audio->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(buses->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/audio/create_missing_buses"));

	CHECK(audio->get_file_path() == "user://settings/audio.ini");
	CHECK(buses->get_file_path() == "user://settings/audio_buses.ini");
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", "csv");
	CHECK(buses->get_file_path() == "user://settings/audio_buses.csv");
	ERR_PRINT_OFF;
	// The devices are one record: no CSV.
	CHECK(audio->get_file_path() == "user://settings/audio.ini");
	ERR_PRINT_ON;
}

TEST_CASE("[Audio][AudioBusSettingsServer] Defaults come from the bus layout, values reach AudioServer") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
	CHECK(server->get_domain() == StringName("audio_buses"));
	CHECK(server->get_entry_count() == 2);
	CHECK(server->get_bus_names() == PackedStringArray({ "Master", "Music" }));
	CHECK(server->has_bus("Music"));
	CHECK_FALSE(server->has_bus("Voice"));
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(-6.0));
	CHECK(server->get_bus_volume_linear("Master") == doctest::Approx(1.0));

	server->set_bus_volume_db("Music", -12.0);
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-12.0));
	server->set_bus_volume_linear("Master", 0.5);
	CHECK(audio_server->get_bus_volume_db(0) == doctest::Approx(Math::linear_to_db(0.5)).epsilon(0.001));
	CHECK(server->get_bus_volume_linear("Master") == doctest::Approx(0.5).epsilon(0.001));
	server->set_bus_mute("Music", true);
	CHECK(audio_server->is_bus_mute(1));
	server->set_bus_solo("Music", true);
	CHECK(audio_server->is_bus_solo(1));
	CHECK(server->is_bus_mute("Music"));
	CHECK(server->is_bus_solo("Music"));

	// Clamped to the range of the bus slider.
	server->set_bus_volume_db("Music", 100.0);
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(24.0));
	server->set_bus_volume_linear("Music", 0.0);
	CHECK(server->get_bus_volume_db("Music") == doctest::Approx(-80.0));
	CHECK(server->get_bus_volume_linear("Music") == doctest::Approx(0.0));

	ERR_PRINT_OFF;
	server->set_bus_volume_db("Voice", -3.0);
	ERR_PRINT_ON;
	CHECK(server->get_entry_count() == 2);

	// The default stays the layout value.
	CHECK(Array(server->get_default_value("buses")).size() == 2);
	server->reset_to_defaults();
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-6.0));
	CHECK_FALSE(audio_server->is_bus_mute(1));
	memdelete(server);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Saved volumes are applied on startup (INI, JSON, CSV)") {
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
		if (extension == "csv") {
			// One row per bus.
			const Vector<String> lines = FileAccess::get_file_as_string(path).strip_edges().split("\n");
			int master_rows = 0;
			int music_rows = 0;
			for (const String &line : lines) {
				master_rows += line.contains("Master") ? 1 : 0;
				music_rows += line.contains("Music") ? 1 : 0;
			}
			CHECK(master_rows == 1);
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
		CHECK(Array(server->get_default_value("buses")).size() == 2);
		CHECK_FALSE(server->is_dirty());
		CHECK_FALSE(server->has_unsaved_changes());
		memdelete(server);
	}
}

TEST_CASE("[Audio][AudioBusSettingsServer] Buses missing in the layout are skipped or created") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	const String path = temp_file("missing.ini");
	{
		AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_file_path(path);
		server->add_entry(make_bus("Voice", -9.0));
		ERR_PRINT_OFF;
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(audio_server->get_bus_index("Voice") == -1);
		CHECK(server->save() == OK);
		memdelete(server);
	}

	// Skipped with a warning; the entry stays.
	AudioBusSettingsServer *skipping = memnew(AudioBusSettingsServer);
	CHECK_FALSE(skipping->is_create_missing_buses_enabled());
	skipping->set_file_path(path);
	ERR_PRINT_OFF;
	skipping->startup();
	ERR_PRINT_ON;
	CHECK(audio_server->get_bus_count() == 2);
	CHECK(skipping->find_entry("Voice") >= 0);
	memdelete(skipping);

	// Created at the end of the layout.
	AudioBusSettingsServer *creating = memnew(AudioBusSettingsServer);
	creating->set_create_missing_buses(true);
	creating->set_file_path(path);
	creating->startup();
	REQUIRE(audio_server->get_bus_index("Voice") == 2);
	CHECK(audio_server->get_bus_volume_db(2) == doctest::Approx(-9.0));
	memdelete(creating);
}

TEST_CASE("[Audio][AudioBusSettingsServer] Buses added to the layout later keep their layout values") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	const String path = temp_file("older_version.ini");
	{
		AudioBusSettingsServer *server = memnew(AudioBusSettingsServer);
		server->set_file_path(path);
		server->set_bus_volume_db("Music", -15.0);
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
	server->startup();
	CHECK(server->get_bus_names() == PackedStringArray({ "Master", "Sfx", "Music" }));
	CHECK(audio_server->get_bus_volume_db(music) == doctest::Approx(-15.0));
	CHECK(audio_server->get_bus_volume_db(1) == doctest::Approx(-3.0));
	CHECK(server->get_bus_volume_db("Sfx") == doctest::Approx(-3.0));
	memdelete(server);
}

TEST_CASE("[Audio][AudioSettingsServer] Devices: unknown names fall back to \"Default\" without an error") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	AudioSettingsServer *server = memnew(AudioSettingsServer);
	CHECK(server->get_domain() == StringName("audio"));
	CHECK(server->get_output_device() == "Default");
	CHECK(server->get_input_device() == "Default");
	// The dummy driver has only "Default".
	CHECK_FALSE(AudioSettingsServer::is_device_selection_supported());
	CHECK_FALSE(server->is_value_supported("output_device"));
	CHECK_FALSE(server->is_value_supported("input_device"));

	// Set at runtime: replaced, with a verbose message only.
	server->set_output_device("A headset that was unplugged");
	CHECK(server->get_output_device() == "Default");
	CHECK(audio_server->get_output_device() == "Default");
	server->set_output_device("Default");
	CHECK(server->get_output_device() == "Default");

	// Loaded from a file written on another machine.
	const String path = temp_file("devices.json");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"output_device\": \"Speakers (Missing)\", \"input_device\": \"Microphone (Missing)\" }");
	}
	server->set_file_path(path);
	CHECK(server->load() == OK);
	CHECK(server->get_output_device() == "Default");
	CHECK(server->get_input_device() == "Default");
	memdelete(server);
}

} // namespace TestAudioSettingsServer
