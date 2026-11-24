/**************************************************************************/
/*  test_engine_settings_server.cpp                                       */
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

TEST_FORCE_LINK(test_engine_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "servers/audio/audio_server.h"
#include "servers/settings/engine_settings.h"
#include "servers/settings/engine_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/test_utils.h"

namespace TestEngineSettingsServer {

// Autosave off, simulation values allowed unless a test locks them; the engine values are restored afterwards.
struct TestSetup {
	int max_fps = 0;
	double time_scale = 1.0;
	int physics_ticks = 60;
	int max_physics_steps = 8;
	double jitter_fix = 0.5;
	bool low_processor = false;
	int low_processor_sleep = 6900;

	explicit TestSetup(bool p_allow_simulation = true) {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
		ProjectSettings::get_singleton()->set_setting("settings/engine/allow_simulation_values", p_allow_simulation);
		// The test runner does not define the physics project settings; use the engine defaults, like a project does.
		if (!ProjectSettings::get_singleton()->has_setting("physics/common/physics_ticks_per_second")) {
			ProjectSettings::get_singleton()->set_setting("physics/common/physics_ticks_per_second", 60);
		}
		if (!ProjectSettings::get_singleton()->has_setting("physics/common/max_physics_steps_per_frame")) {
			ProjectSettings::get_singleton()->set_setting("physics/common/max_physics_steps_per_frame", 8);
		}
		const Engine *engine = Engine::get_singleton();
		max_fps = engine->get_max_fps();
		time_scale = engine->get_time_scale();
		physics_ticks = engine->get_physics_ticks_per_second();
		max_physics_steps = engine->get_max_physics_steps_per_frame();
		jitter_fix = engine->get_physics_jitter_fix();
		low_processor = OS::get_singleton()->is_in_low_processor_usage_mode();
		low_processor_sleep = OS::get_singleton()->get_low_processor_usage_mode_sleep_usec();
	}
	~TestSetup() {
		Engine *engine = Engine::get_singleton();
		engine->set_max_fps(max_fps);
		engine->set_time_scale(time_scale);
		engine->set_physics_ticks_per_second(physics_ticks);
		engine->set_max_physics_steps_per_frame(max_physics_steps);
		engine->set_physics_jitter_fix(jitter_fix);
		OS::get_singleton()->set_low_processor_usage_mode(low_processor);
		OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(low_processor_sleep);
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/engine/allow_simulation_values", false);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("engine_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[EngineSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	EngineSettingsServer *server = EngineSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("EngineSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("engine") == server);
	CHECK(server->get_domain() == StringName("engine"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/engine.ini");
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/engine/allow_simulation_values"));

	const PackedStringArray names = server->get_setting_names();
	for (const String &value_name : { "max_fps", "time_scale", "physics_ticks_per_second", "max_physics_steps_per_frame", "physics_jitter_fix", "physics_interpolation", "audio_playback_speed_scale", "low_processor_mode", "low_processor_mode_sleep_usec" }) {
		CHECK_MESSAGE(names.has(value_name), value_name);
		CHECK(server->is_setting_bound(value_name));
	}
	CHECK(Object::cast_to<EngineSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(EngineSettingsServer::get_simulation_setting_names().size() == 5);
	CHECK(EngineSettingsServer::is_simulation_setting("physics_ticks_per_second"));
	CHECK_FALSE(EngineSettingsServer::is_simulation_setting("max_fps"));
}

TEST_CASE("[EngineSettingsServer] Defaults are the project settings, not the engine state") {
	TestSetup setup;
	// A command-line value the engine already has.
	Engine::get_singleton()->set_max_fps(30);
	Engine::get_singleton()->set_time_scale(3.0);

	EngineSettingsServer *server = memnew(EngineSettingsServer);
	CHECK(int(server->get_default_value("max_fps")) == int(GLOBAL_GET("application/run/max_fps")));
	CHECK(double(server->get_default_value("time_scale")) == doctest::Approx(1.0));
	CHECK(int(server->get_default_value("physics_ticks_per_second")) == int(GLOBAL_GET("physics/common/physics_ticks_per_second")));
	CHECK(int(server->get_default_value("max_physics_steps_per_frame")) == int(GLOBAL_GET("physics/common/max_physics_steps_per_frame")));
	CHECK(bool(server->get_default_value("low_processor_mode")) == bool(GLOBAL_GET("application/run/low_processor_mode")));
	CHECK(float(server->get_default_value("audio_playback_speed_scale")) == doctest::Approx(1.0));
	CHECK_FALSE(server->is_dirty());
	memdelete(server);
}

TEST_CASE("[EngineSettingsServer] Each value reaches the engine at once") {
	TestSetup setup;
	Engine *engine = Engine::get_singleton();
	EngineSettingsServer *server = memnew(EngineSettingsServer);

	server->set_max_fps(45);
	CHECK(engine->get_max_fps() == 45);
	server->set_time_scale(0.5);
	CHECK(engine->get_time_scale() == doctest::Approx(0.5));
	server->set_physics_ticks_per_second(120);
	CHECK(engine->get_physics_ticks_per_second() == 120);
	server->set_max_physics_steps_per_frame(4);
	CHECK(engine->get_max_physics_steps_per_frame() == 4);
	server->set_physics_jitter_fix(0.25);
	CHECK(engine->get_physics_jitter_fix() == doctest::Approx(0.25));
	server->set_low_processor_mode_enabled(true);
	CHECK(OS::get_singleton()->is_in_low_processor_usage_mode());
	server->set_low_processor_mode_sleep_usec(16000);
	CHECK(OS::get_singleton()->get_low_processor_usage_mode_sleep_usec() == 16000);

	CHECK(server->get_max_fps() == 45);
	CHECK(server->get_time_scale() == doctest::Approx(0.5));
	CHECK(server->get_physics_ticks_per_second() == 120);
	CHECK(server->is_low_processor_mode_enabled());

	// Reading back from the engine.
	engine->set_max_fps(75);
	server->read_from_engine();
	CHECK(server->get_max_fps() == 75);

	server->reset_to_defaults();
	CHECK(engine->get_time_scale() == doctest::Approx(1.0));
	CHECK(engine->get_physics_ticks_per_second() == int(GLOBAL_GET("physics/common/physics_ticks_per_second")));
	memdelete(server);
}

TEST_CASE("[Audio][EngineSettingsServer] The audio playback speed reaches AudioServer") {
	TestSetup setup;
	AudioServer *audio_server = AudioServer::get_singleton();
	REQUIRE(audio_server != nullptr);
	EngineSettingsServer *server = memnew(EngineSettingsServer);
	server->set_audio_playback_speed_scale(0.75f);
	CHECK(audio_server->get_playback_speed_scale() == doctest::Approx(0.75));
	server->reset_to_defaults();
	CHECK(audio_server->get_playback_speed_scale() == doctest::Approx(1.0));
	memdelete(server);
}

TEST_CASE("[EngineSettingsServer] Values out of range are clamped with a warning") {
	TestSetup setup;
	EngineSettingsServer *server = memnew(EngineSettingsServer);
	ERR_PRINT_OFF;
	server->set_max_fps(-10);
	CHECK(server->get_max_fps() == 0);
	server->set_time_scale(0.0);
	CHECK(server->get_time_scale() == doctest::Approx(0.01));
	server->set_physics_ticks_per_second(5000);
	CHECK(server->get_physics_ticks_per_second() == 1000);
	server->set_max_physics_steps_per_frame(0);
	CHECK(server->get_max_physics_steps_per_frame() == 1);
	server->set_physics_jitter_fix(-1.0);
	CHECK(server->get_physics_jitter_fix() == doctest::Approx(0.0));
	server->set_audio_playback_speed_scale(-2.0f);
	CHECK(server->get_audio_playback_speed_scale() == doctest::Approx(0.01));
	server->set_low_processor_mode_sleep_usec(-5);
	CHECK(server->get_low_processor_mode_sleep_usec() == 0);
	ERR_PRINT_ON;
	CHECK(Engine::get_singleton()->get_physics_ticks_per_second() == 1000);

	// A hand-edited file.
	const String path = temp_file("hand_edited.json");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"max_fps\": -30, \"time_scale\": 500, \"physics_ticks_per_second\": 0, \"low_processor_mode_sleep_usec\": 9999999 }");
	}
	EngineSettingsServer *loaded = memnew(EngineSettingsServer);
	loaded->set_file_path(path);
	ERR_PRINT_OFF;
	CHECK(loaded->load() == OK);
	ERR_PRINT_ON;
	CHECK(loaded->get_max_fps() == 0);
	CHECK(loaded->get_time_scale() == doctest::Approx(100.0));
	CHECK(loaded->get_physics_ticks_per_second() == 1);
	CHECK(loaded->get_low_processor_mode_sleep_usec() == 1000000);
	memdelete(loaded);
	memdelete(server);
}

TEST_CASE("[EngineSettingsServer] Simulation values are locked unless the project allows them") {
	TestSetup setup(false);
	CHECK(EngineSettingsServer::is_simulation_locked());
	Engine *engine = Engine::get_singleton();
	EngineSettingsServer *server = memnew(EngineSettingsServer);
	CHECK_FALSE(server->is_value_supported("time_scale"));
	CHECK_FALSE(server->is_value_supported("physics_ticks_per_second"));
	CHECK(server->is_value_supported("max_fps"));
	CHECK(server->is_value_supported("physics_interpolation"));

	ERR_PRINT_OFF;
	server->set_time_scale(2.0);
	server->set_physics_ticks_per_second(30);
	ERR_PRINT_ON;
	CHECK(server->get_time_scale() == doctest::Approx(1.0));
	CHECK(server->get_physics_ticks_per_second() == int(server->get_default_value("physics_ticks_per_second")));
	CHECK(engine->get_time_scale() == doctest::Approx(1.0));
	server->set_max_fps(50);
	CHECK(engine->get_max_fps() == 50);

	// A hand-edited file can't change them either.
	const String path = temp_file("locked.json");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"time_scale\": 4.0, \"max_fps\": 40 }");
	}
	EngineSettingsServer *loaded = memnew(EngineSettingsServer);
	loaded->set_file_path(path);
	ERR_PRINT_OFF;
	loaded->startup();
	ERR_PRINT_ON;
	CHECK(loaded->get_time_scale() == doctest::Approx(1.0));
	CHECK(engine->get_time_scale() == doctest::Approx(1.0));
	CHECK(loaded->get_max_fps() == 40);
	CHECK(engine->get_max_fps() == 40);
	memdelete(loaded);
	memdelete(server);
}

TEST_CASE("[EngineSettingsServer] Saved values are applied on startup (INI, JSON)") {
	TestSetup setup;
	Engine *engine = Engine::get_singleton();
	for (const String &extension : { String("ini"), String("json") }) {
		const String path = temp_file("engine." + extension);
		{
			EngineSettingsServer *server = memnew(EngineSettingsServer);
			server->set_file_path(path);
			server->set_max_fps(60);
			server->set_time_scale(0.8);
			server->set_physics_ticks_per_second(90);
			server->set_low_processor_mode_enabled(true);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		// "Restart": the engine is back at the project values.
		engine->set_max_fps(0);
		engine->set_time_scale(1.0);
		engine->set_physics_ticks_per_second(60);
		OS::get_singleton()->set_low_processor_usage_mode(false);

		EngineSettingsServer *server = memnew(EngineSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK(engine->get_max_fps() == 60);
		CHECK(engine->get_time_scale() == doctest::Approx(0.8));
		CHECK(engine->get_physics_ticks_per_second() == 90);
		CHECK(OS::get_singleton()->is_in_low_processor_usage_mode());
		CHECK_FALSE(server->is_dirty());
		memdelete(server);
	}
}

TEST_CASE("[EngineSettingsServer] Session overrides reach the engine but are never stored") {
	TestSetup setup;
	Engine *engine = Engine::get_singleton();
	EngineSettingsServer *server = memnew(EngineSettingsServer);
	server->set_max_fps(120);
	server->set_session_override("max_fps", 30);
	CHECK(engine->get_max_fps() == 30);
	CHECK(server->has_session_override("max_fps"));
	CHECK(server->get_max_fps() == 120);

	// Reading the engine back keeps the stored value.
	server->read_from_engine();
	CHECK(server->get_max_fps() == 120);

	server->clear_session_override("max_fps");
	CHECK(engine->get_max_fps() == 120);
	memdelete(server);
}

} // namespace TestEngineSettingsServer
