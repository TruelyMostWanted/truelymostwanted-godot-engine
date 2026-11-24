/**************************************************************************/
/*  test_tts_persona_settings_server.cpp                                  */
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

TEST_FORCE_LINK(test_tts_persona_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/tts_persona_settings.h"
#include "servers/settings/tts_persona_settings_server.h"
#include "servers/tts/tts_server.h"
#include "tests/test_utils.h"

namespace TestTTSPersonaSettingsServer {

// Unit tests run without text to speech: TTSServer keeps its personas, but reports no voices.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		TTSServer *tts_server = TTSServer::get_singleton();
		tts_server->clear_personas();
		tts_server->clear_persona_overrides();
	}
	~TestSetup() {
		TTSServer *tts_server = TTSServer::get_singleton();
		tts_server->clear_personas();
		tts_server->clear_persona_overrides();
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_INI);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("tts_persona_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static Ref<TTSPersona> make_persona(const String &p_name, const String &p_voice, const String &p_language, float p_rate) {
	Ref<TTSPersona> persona;
	persona.instantiate();
	persona->set_persona_name(p_name);
	persona->set_voice(p_voice);
	persona->set_language(p_language);
	persona->set_rate(p_rate);
	return persona;
}

TEST_CASE("[TTSPersonaSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	TTSPersonaSettingsServer *server = TTSPersonaSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("TTSPersonaSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("tts_personas") == server);
	CHECK(server->get_domain() == StringName("tts_personas"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_list_property() == StringName("personas"));
	CHECK(server->get_entry_key() == StringName("persona_name"));

	CHECK(server->get_file_path() == "user://settings/tts_personas.ini");
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_CSV);
	CHECK(server->get_file_path() == "user://settings/tts_personas.csv");
}

TEST_CASE("[TTSPersonaSettingsServer] The player's personas override the game's personas in TTSServer") {
	TestSetup setup;
	TTSServer *tts_server = TTSServer::get_singleton();
	Ref<TTSPersona> game_narrator = make_persona("narrator", "", "en", 1.0);
	tts_server->add_persona(game_narrator);
	tts_server->set_default_persona("narrator");

	TTSPersonaSettingsServer *server = memnew(TTSPersonaSettingsServer);
	// Empty defaults: only what the player changes is stored.
	CHECK(server->get_entry_count() == 0);
	CHECK(server->get_persona_names().is_empty());
	CHECK(server->get_default_persona().is_empty());

	// A new persona of the player reaches TTSServer at once (APPLY_MODE_IMMEDIATE).
	server->add_persona(make_persona("helper", "Some Voice", "fr", 1.5));
	CHECK(server->has_persona("helper"));
	CHECK(tts_server->has_persona("helper"));
	CHECK(tts_server->has_persona_override("helper"));
	CHECK(tts_server->get_persona("helper")->get_language() == "fr");
	CHECK(tts_server->get_persona("helper")->get_rate() == doctest::Approx(1.5));
	const Ref<TTSPersona> stored = server->get_persona("helper");
	REQUIRE(stored.is_valid());
	CHECK(stored->get_voice() == "Some Voice");
	// get_persona() returns a copy.
	stored->set_rate(3.0);
	CHECK(server->get_persona_value("helper", "rate") == Variant(1.5f));

	// Changing a value of the game's persona creates the player's version from the game's values.
	CHECK_FALSE(server->has_persona("narrator"));
	CHECK(server->get_persona_value("narrator", "language") == Variant("en"));
	server->set_persona_value("narrator", "rate", 2.0);
	CHECK(server->has_persona("narrator"));
	CHECK(server->get_persona_names() == PackedStringArray({ "helper", "narrator" }));
	CHECK(tts_server->get_persona("narrator")->get_rate() == doctest::Approx(2.0));
	CHECK(tts_server->get_persona("narrator")->get_language() == "en");
	// The game's persona itself is unchanged.
	CHECK(tts_server->get_base_persona("narrator") == game_narrator);
	CHECK(game_narrator->get_rate() == doctest::Approx(1.0));

	// Values are clamped to the ranges of TTSPersona.
	server->set_persona_value("narrator", "volume", 500);
	CHECK(int(server->get_persona_value("narrator", "volume")) == 100);
	CHECK(tts_server->get_persona("narrator")->get_volume() == 100);
	server->set_persona_value("helper", "pitch", -1.0);
	CHECK(float(server->get_persona_value("helper", "pitch")) == doctest::Approx(0.0));

	ERR_PRINT_OFF;
	server->set_persona_value("nobody", "rate", 2.0);
	server->set_persona_value("narrator", "no_such_value", 2.0);
	server->set_persona_value("narrator", "persona_name", "other");
	server->add_persona(Ref<TTSPersona>());
	server->add_persona(make_persona("", "", "", 1.0));
	server->set_default_persona("nobody");
	ERR_PRINT_ON;
	CHECK(server->get_entry_count() == 2);

	// The player's default persona wins over the game's default persona.
	CHECK(tts_server->get_default_persona_name() == StringName("narrator"));
	server->set_default_persona("helper");
	CHECK(server->get_default_persona() == StringName("helper"));
	CHECK(tts_server->get_default_persona_override() == StringName("helper"));
	CHECK(tts_server->get_default_persona_name() == StringName("helper"));
	server->set_persona_value("narrator", "default", true);
	CHECK(server->get_default_persona() == StringName("narrator"));
	CHECK_FALSE(bool(server->get_persona_value("helper", "default")));
	server->set_default_persona(StringName());
	CHECK(server->get_default_persona().is_empty());
	CHECK(tts_server->get_default_persona_override().is_empty());

	// A shipped persona chosen as default gets an entry.
	tts_server->add_persona(make_persona("guard", "", "de", 0.8));
	server->set_default_persona("guard");
	CHECK(server->has_persona("guard"));
	CHECK(tts_server->get_default_persona_name() == StringName("guard"));

	// Adding the same name again replaces the values, keeps the position and the default flag.
	server->add_persona(make_persona("guard", "", "de", 0.5));
	CHECK(server->get_persona_names() == PackedStringArray({ "helper", "narrator", "guard" }));
	CHECK(server->get_default_persona() == StringName("guard"));
	CHECK(tts_server->get_persona("guard")->get_rate() == doctest::Approx(0.5));

	// The game adding its persona again does not hide the player's version.
	tts_server->add_persona(make_persona("narrator", "", "en", 1.0));
	CHECK(tts_server->get_persona("narrator")->get_rate() == doctest::Approx(2.0));

	// Removing the player's version brings the game's persona back.
	server->remove_persona("narrator");
	server->remove_persona("nobody");
	CHECK_FALSE(server->has_persona("narrator"));
	CHECK(tts_server->get_persona("narrator")->get_rate() == doctest::Approx(1.0));

	// Reset: all personas of the player are gone, the game's personas stay.
	server->reset_to_defaults();
	CHECK(server->get_entry_count() == 0);
	CHECK(tts_server->get_persona_overrides().is_empty());
	CHECK_FALSE(tts_server->has_persona("helper"));
	CHECK(tts_server->has_persona("narrator"));
	CHECK(tts_server->get_default_persona_name() == StringName("narrator"));

	// Without a voice list (no text to speech here), no voice is reported missing.
	server->add_persona(make_persona("helper", "Voice Not On This Device", "fr", 1.0));
	CHECK(server->get_missing_voice_personas().is_empty());
	memdelete(server);
}

TEST_CASE("[TTSPersonaSettingsServer] A saved persona is available in TTSServer after a restart (INI, JSON, CSV)") {
	TestSetup setup;
	TTSServer *tts_server = TTSServer::get_singleton();
	for (const String &extension : { String("ini"), String("json"), String("csv") }) {
		const String path = temp_file("personas." + extension);
		{
			TTSPersonaSettingsServer *server = memnew(TTSPersonaSettingsServer);
			server->set_file_path(path);
			server->add_persona(make_persona("helper", "Some Voice", "fr", 1.5));
			server->add_persona(make_persona("guard", "", "de", 0.8));
			server->set_persona_value("guard", "volume", 80);
			server->set_default_persona("guard");
			// save() writes the applied values.
			CHECK(server->apply() == OK);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		if (extension == "csv") {
			// One row per persona (and the header).
			const Vector<String> lines = FileAccess::get_file_as_string(path).strip_edges().split("\n");
			CHECK(lines.size() == 3);
			int helper_rows = 0;
			int guard_rows = 0;
			for (const String &line : lines) {
				helper_rows += line.contains("helper") ? 1 : 0;
				guard_rows += line.contains("guard") ? 1 : 0;
			}
			CHECK(helper_rows == 1);
			CHECK(guard_rows == 1);
		}

		// "Restart": TTSServer has only the personas of the game again.
		tts_server->clear_persona_overrides();
		CHECK_FALSE(tts_server->has_persona("helper"));

		TTSPersonaSettingsServer *server = memnew(TTSPersonaSettingsServer);
		server->set_file_path(path);
		server->startup();
		CHECK(server->get_persona_names() == PackedStringArray({ "helper", "guard" }));
		CHECK(tts_server->get_persona_names() == PackedStringArray({ "helper", "guard" }));
		CHECK(tts_server->get_personas().size() == 2);
		const Ref<TTSPersona> helper = tts_server->get_persona("helper");
		REQUIRE(helper.is_valid());
		CHECK(helper->get_voice() == "Some Voice");
		CHECK(helper->get_language() == "fr");
		CHECK(helper->get_rate() == doctest::Approx(1.5));
		const Ref<TTSPersona> guard = tts_server->get_persona("guard");
		REQUIRE(guard.is_valid());
		CHECK(guard->get_volume() == 80);
		CHECK(guard->get_pitch() == doctest::Approx(1.0));
		CHECK(tts_server->get_default_persona_name() == StringName("guard"));
		CHECK(server->get_default_persona() == StringName("guard"));
		CHECK_FALSE(server->is_dirty());
		memdelete(server);
		tts_server->clear_persona_overrides();
	}
}

TEST_CASE("[TTSPersonaSettingsServer] Hand-edited entries are clamped, the first default wins") {
	TestSetup setup;
	TTSServer *tts_server = TTSServer::get_singleton();
	const String path = temp_file("hand_edited.csv");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("persona_name,volume,rate,default\nfirst,250,1,true\nsecond,50,99,true\n");
	}
	TTSPersonaSettingsServer *server = memnew(TTSPersonaSettingsServer);
	server->set_file_path(path);
	server->startup();
	REQUIRE(server->get_entry_count() == 2);
	CHECK(int(server->get_persona_value("first", "volume")) == 100);
	CHECK(float(server->get_persona_value("second", "rate")) == doctest::Approx(10.0));
	CHECK(tts_server->get_default_persona_name() == StringName("first"));
	CHECK(server->get_default_persona() == StringName("first"));
	memdelete(server);
}

} // namespace TestTTSPersonaSettingsServer
