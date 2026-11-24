/**************************************************************************/
/*  test_settings_server.cpp                                              */
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

TEST_FORCE_LINK(test_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/variant/typed_array.h"
#include "servers/settings/list_settings_server.h"
#include "servers/settings/settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestSettingsServer {

// The "engine" of the test domain: a value pushed through a callback binding and one through _apply_settings().
struct TestEngine {
	static inline double volume = 0.5;
	static inline int quality = 1;

	static void set_volume(double p_volume) { volume = p_volume; }
	static double get_volume() { return volume; }
};

class TestDomainSettings : public Resource {
	GDCLASS(TestDomainSettings, Resource);

	double volume = 0.5;
	int quality = 1;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_volume", "value"), &TestDomainSettings::set_volume);
		ClassDB::bind_method(D_METHOD("get_volume"), &TestDomainSettings::get_volume);
		ClassDB::bind_method(D_METHOD("set_quality", "value"), &TestDomainSettings::set_quality);
		ClassDB::bind_method(D_METHOD("get_quality"), &TestDomainSettings::get_quality);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "quality", PROPERTY_HINT_ENUM, "Low,Medium,High"), "set_quality", "get_quality");
	}

public:
	void set_volume(double p_value) { volume = p_value; }
	double get_volume() const { return volume; }
	void set_quality(int p_value) { quality = p_value; }
	int get_quality() const { return quality; }
};

class TestDomainServer : public SettingsServer {
	GDCLASS(TestDomainServer, SettingsServer);

public:
	String domain = "test_domain";
	int version = 1;
	ApplyPhase phase = APPLY_PHASE_SERVERS;
	int apply_calls = 0;
	PackedStringArray last_applied;

protected:
	static void _bind_methods() {}

	virtual Ref<Resource> _create_default_settings() const override {
		Ref<TestDomainSettings> default_settings;
		default_settings.instantiate();
		return default_settings;
	}

	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override {
		apply_calls++;
		last_applied = p_names;
		if (p_names.has("quality")) {
			TestEngine::quality = p_settings->get("quality");
		}
	}

	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override {
		p_settings->set("quality", TestEngine::quality);
	}

	virtual Dictionary _migrate_settings(int p_from_version, const Dictionary &p_data) override {
		Dictionary data = p_data;
		// Version 1 stored the volume in percent under another name.
		if (p_from_version < 2 && data.has("volume_percent")) {
			data["volume"] = double(data["volume_percent"]) / 100.0;
			data.erase("volume_percent");
		}
		return data;
	}

public:
	virtual StringName get_domain() const override { return domain; }
	virtual int get_format_version() const override { return version; }
	virtual ApplyPhase get_apply_phase() const override { return phase; }

	TestDomainServer() {
		bind_setting_callbacks("volume", callable_mp_static(&TestEngine::set_volume), callable_mp_static(&TestEngine::get_volume));
	}
};

class TestListEntry : public Resource {
	GDCLASS(TestListEntry, Resource);

	String entry_name;
	int64_t level = 1;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entry_name", "value"), &TestListEntry::set_entry_name);
		ClassDB::bind_method(D_METHOD("get_entry_name"), &TestListEntry::get_entry_name);
		ClassDB::bind_method(D_METHOD("set_level", "value"), &TestListEntry::set_level);
		ClassDB::bind_method(D_METHOD("get_level"), &TestListEntry::get_level);
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "entry_name"), "set_entry_name", "get_entry_name");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "level", PROPERTY_HINT_RANGE, "0,10"), "set_level", "get_level");
	}

public:
	void set_entry_name(const String &p_value) { entry_name = p_value; }
	String get_entry_name() const { return entry_name; }
	void set_level(int64_t p_value) { level = p_value; }
	int64_t get_level() const { return level; }
};

class TestListSettings : public Resource {
	GDCLASS(TestListSettings, Resource);

	TypedArray<TestListEntry> entries;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entries", "value"), &TestListSettings::set_entries);
		ClassDB::bind_method(D_METHOD("get_entries"), &TestListSettings::get_entries);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entries", PROPERTY_HINT_ARRAY_TYPE, "TestListEntry"), "set_entries", "get_entries");
	}

public:
	void set_entries(const TypedArray<TestListEntry> &p_value) { entries = p_value; }
	TypedArray<TestListEntry> get_entries() const { return entries; }
};

class TestListServer : public ListSettingsServer {
	GDCLASS(TestListServer, ListSettingsServer);

public:
	int apply_calls = 0;

protected:
	static void _bind_methods() {}

	virtual Ref<Resource> _create_default_settings() const override {
		Ref<TestListSettings> default_settings;
		default_settings.instantiate();
		return default_settings;
	}

	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override {
		apply_calls++;
	}

public:
	virtual StringName get_domain() const override { return "test_list"; }
	virtual StringName get_entry_key() const override { return "entry_name"; }
};

static void register_test_classes() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(TestDomainSettings);
		GDREGISTER_CLASS(TestDomainServer);
		GDREGISTER_CLASS(TestListEntry);
		GDREGISTER_CLASS(TestListSettings);
		GDREGISTER_CLASS(TestListServer);
		registered = true;
	}
}

// Autosave is switched off in the tests unless a test enables it.
struct TestSetup {
	TestSetup() {
		register_test_classes();
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/storage/format", ResourceTextSerializer::FORMAT_INI);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static Ref<TestListEntry> make_entry(const String &p_name, int64_t p_level) {
	Ref<TestListEntry> entry;
	entry.instantiate();
	entry->set_entry_name(p_name);
	entry->set_level(p_level);
	return entry;
}

TEST_CASE("[SettingsServer] Manager singleton, registration and project settings") {
	TestSetup setup;
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	REQUIRE(manager != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("SettingsServerManager"));
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/storage/folder"));
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/startup/apply_on_startup"));
	CHECK(manager->get_storage_folder() == "user://settings");
	CHECK(manager->get_storage_format() == ResourceTextSerializer::FORMAT_INI);

	TestDomainServer *server = memnew(TestDomainServer);
	CHECK(manager->register_server(server) == OK);
	CHECK(manager->has_server("test_domain"));
	CHECK(manager->get_server("test_domain") == server);
	CHECK(manager->get_domains().has("test_domain"));
	CHECK(server->get_file_path() == "user://settings/test_domain.ini");

	// The format is an enum; a name written by hand into project.godot is accepted too.
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", ResourceTextSerializer::FORMAT_JSON);
	CHECK(manager->get_storage_format() == ResourceTextSerializer::FORMAT_JSON);
	CHECK(server->get_file_path() == "user://settings/test_domain.json");
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", "json");
	CHECK(manager->get_storage_format() == ResourceTextSerializer::FORMAT_JSON);
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", ResourceTextSerializer::FORMAT_INI);

	TestDomainServer *duplicate = memnew(TestDomainServer);
	ERR_PRINT_OFF;
	CHECK(manager->register_server(duplicate) == ERR_ALREADY_EXISTS);
	ERR_PRINT_ON;
	memdelete(duplicate);
	CHECK(manager->get_server("test_domain") == server);

	// The destructor unregisters the server.
	memdelete(server);
	CHECK_FALSE(manager->has_server("test_domain"));
}

TEST_CASE("[SettingsServer] Defaults come from the engine state, values are validated") {
	TestSetup setup;
	TestEngine::volume = 0.75;
	TestEngine::quality = 2;
	TestDomainServer *server = memnew(TestDomainServer);
	CHECK(double(server->get_default_value("volume")) == doctest::Approx(0.75));
	CHECK(int(server->get_default_value("quality")) == 2);
	CHECK(server->get_setting_names().size() == 2);
	CHECK(server->has_setting("volume"));
	CHECK_FALSE(server->has_setting("resource_name"));

	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_value("volume", 3.0);
	CHECK(double(server->get_value("volume")) == doctest::Approx(1.0));
	ERR_PRINT_OFF;
	server->set_value("quality", 7);
	server->set_value("missing", 1);
	ERR_PRINT_ON;
	CHECK(int(server->get_value("quality")) == 2);
	memdelete(server);
}

TEST_CASE("[SettingsServer] Immediate apply pushes only changed values") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	SIGNAL_WATCH(server, "value_changed");
	SIGNAL_WATCH(server, "applied");

	server->set_value("volume", 0.25);
	CHECK(TestEngine::volume == doctest::Approx(0.25));
	CHECK(server->apply_calls == 0); // Bound by callbacks: _apply_settings() is not needed.
	const Array changed_args = { { StringName("volume"), 0.25 } };
	Array applied_args;
	applied_args.push_back(Array({ Variant(PackedStringArray({ "volume" })) }));
	SIGNAL_CHECK("value_changed", changed_args);
	SIGNAL_CHECK("applied", applied_args);

	server->set_value("quality", 0);
	CHECK(TestEngine::quality == 0);
	CHECK(server->apply_calls == 1);
	CHECK(server->last_applied == PackedStringArray({ "quality" }));
	CHECK_FALSE(server->is_dirty());
	CHECK(server->has_unsaved_changes());

	SIGNAL_UNWATCH(server, "value_changed");
	SIGNAL_UNWATCH(server, "applied");
	memdelete(server);
}

TEST_CASE("[SettingsServer] revert() after set_value() gives back the applied state") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_value("volume", 0.25);
	CHECK(server->apply() == OK);
	server->set_value("volume", 0.9);
	server->set_value("quality", 2);
	CHECK(server->is_dirty());
	CHECK(server->get_dirty_setting_names().size() == 2);
	CHECK(TestEngine::volume == doctest::Approx(0.25));

	server->revert();
	CHECK_FALSE(server->is_dirty());
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.25));
	CHECK(int(server->get_value("quality")) == 1);

	server->reset_to_defaults();
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.5));
	CHECK(server->is_dirty());
	server->apply();
	CHECK(TestEngine::volume == doctest::Approx(0.5));
	memdelete(server);
}

TEST_CASE("[SettingsServer] Save, restart and apply on startup (INI, JSON and XML)") {
	TestSetup setup;
	for (const String &extension : { String("ini"), String("json"), String("xml") }) {
		const String path = temp_file("startup." + extension);
		// Every round starts from the project defaults of the engine.
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;
		{
			TestDomainServer *server = memnew(TestDomainServer);
			server->set_file_path(path);
			server->set_value("volume", 0.25);
			server->set_value("quality", 2);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		// "Restart": the engine is back at its project defaults.
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;

		const String text = FileAccess::get_file_as_string(path);
		CHECK(text.contains("settings_version"));
		CHECK(text.contains("High"));

		TestDomainServer *server = memnew(TestDomainServer);
		server->set_file_path(path);
		server->startup();
		CHECK(server->is_started());
		CHECK(TestEngine::volume == doctest::Approx(0.25));
		CHECK(TestEngine::quality == 2);
		CHECK(double(server->get_applied_value("volume")) == doctest::Approx(0.25));
		CHECK(double(server->get_default_value("volume")) == doctest::Approx(0.5));
		CHECK_FALSE(server->is_dirty());
		// Applying at startup does not write the file back.
		CHECK_FALSE(server->has_unsaved_changes());
		memdelete(server);
	}
}

TEST_CASE("[SettingsServer] Only changed values are written") {
	TestSetup setup;
	const String path = temp_file("only_changed.ini");
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);
	server->set_value("quality", 2);
	CHECK(server->save() == OK);
	const String text = FileAccess::get_file_as_string(path);
	CHECK(text.contains("quality"));
	CHECK_FALSE(text.contains("volume"));

	ProjectSettings::get_singleton()->set_setting("settings/storage/only_changed_values", false);
	CHECK(server->save() == OK);
	CHECK(FileAccess::get_file_as_string(path).contains("volume"));
	ProjectSettings::get_singleton()->set_setting("settings/storage/only_changed_values", true);
	memdelete(server);
}

TEST_CASE("[SettingsServer] Missing and broken files fall back to the defaults") {
	TestSetup setup;
	const String path = temp_file("broken.json");
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);
	CHECK(server->load() == ERR_FILE_NOT_FOUND);
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.5));

	const String broken = "{ \"volume\": 0.25, ";
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string(broken);
	}
	ERR_PRINT_OFF;
	CHECK(server->load() != OK);
	ERR_PRINT_ON;
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.5));
	// Not overwritten until the next save().
	CHECK(FileAccess::get_file_as_string(path) == broken);
	memdelete(server);

	TestDomainServer *restarted = memnew(TestDomainServer);
	restarted->set_file_path(path);
	ERR_PRINT_OFF;
	restarted->startup();
	ERR_PRINT_ON;
	CHECK(FileAccess::get_file_as_string(path) == broken);
	CHECK(restarted->save() == OK);
	CHECK(FileAccess::get_file_as_string(path) != broken);
	memdelete(restarted);
}

TEST_CASE("[SettingsServer] Hand-edited values are clamped, old versions are migrated") {
	TestSetup setup;
	const String path = temp_file("migrate.ini");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("[root]\n$type=TestDomainSettings\nmetadata/settings_version=1\nvolume_percent=40\n");
	}
	TestDomainServer *server = memnew(TestDomainServer);
	server->version = 2;
	server->set_file_path(path);
	CHECK(server->load() == OK);
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.4));

	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("[root]\n$type=TestDomainSettings\nvolume=5\n");
	}
	CHECK(server->load() == OK);
	CHECK(double(server->get_value("volume")) == doctest::Approx(1.0));
	memdelete(server);
}

TEST_CASE("[SettingsServer] Confirmation keeps or reverts the applied values") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_setting_flags("quality", SettingsServer::SETTING_FLAG_CONFIRM);
	SIGNAL_WATCH(server, "confirmation_finished");

	// A flagged value goes through the confirmation in immediate mode.
	server->set_value("quality", 2);
	CHECK(server->is_confirmation_pending());
	CHECK(TestEngine::quality == 2);
	server->cancel_confirmation();
	CHECK_FALSE(server->is_confirmation_pending());
	CHECK(TestEngine::quality == 1);
	CHECK(int(server->get_value("quality")) == 1);
	const Array reverted_args = { { false } };
	SIGNAL_CHECK("confirmation_finished", reverted_args);

	server->set_value("quality", 0);
	CHECK(server->is_confirmation_pending());
	CHECK(server->get_confirmation_time_left() > 0.0);
	server->confirm_changes();
	CHECK(TestEngine::quality == 0);
	CHECK(int(server->get_applied_value("quality")) == 0);
	const Array kept_args = { { true } };
	SIGNAL_CHECK("confirmation_finished", kept_args);

	SIGNAL_UNWATCH(server, "confirmation_finished");
	memdelete(server);
}

TEST_CASE("[SettingsServer] Session overrides are applied but never saved") {
	TestSetup setup;
	const String path = temp_file("override.ini");
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);
	server->set_value("volume", 0.25);
	server->set_session_override("volume", 0.1);
	CHECK(server->has_session_override("volume"));
	CHECK(TestEngine::volume == doctest::Approx(0.1));
	CHECK(double(server->get_value("volume")) == doctest::Approx(0.25));
	CHECK(server->save() == OK);
	CHECK(FileAccess::get_file_as_string(path).contains("0.25"));

	server->clear_session_override("volume");
	CHECK(TestEngine::volume == doctest::Approx(0.25));
	memdelete(server);
}

TEST_CASE("[SettingsServer] Restart flags and values without an engine target") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_setting_flags("quality", SettingsServer::SETTING_FLAG_RESTART_REQUIRED | SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET);
	SIGNAL_WATCH(server, "restart_required");
	server->set_value("quality", 2);
	CHECK(TestEngine::quality == 1);
	CHECK(server->apply_calls == 0);
	CHECK(server->get_restart_required_names() == PackedStringArray({ "quality" }));
	Array restart_args;
	restart_args.push_back(Array({ Variant(PackedStringArray({ "quality" })) }));
	SIGNAL_CHECK("restart_required", restart_args);
	SIGNAL_UNWATCH(server, "restart_required");
	memdelete(server);
}

TEST_CASE("[SettingsServer] Autosave writes once after apply") {
	TestSetup setup;
	ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
	const String path = temp_file("autosave.ini");
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);
	server->set_value("volume", 0.25);
	server->set_value("quality", 2);
	CHECK_FALSE(FileAccess::exists(path));
	MessageQueue::get_singleton()->flush();
	CHECK(FileAccess::exists(path));
	CHECK_FALSE(server->has_unsaved_changes());
	memdelete(server);
}

TEST_CASE("[SettingsServer] List domain: entries and the same values in CSV, JSON and INI") {
	TestSetup setup;
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", ResourceTextSerializer::FORMAT_CSV);
	TestListServer *server = memnew(TestListServer);
	CHECK(server->get_file_path() == "user://settings/test_list.csv");
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	CHECK(server->get_list_property() == StringName("entries"));
	server->add_entry(make_entry("a", 1));
	server->add_entry(make_entry("b", 2));
	server->add_entry(make_entry("c", 3));
	CHECK(server->get_entry_count() == 3);
	server->move_entry(2, 0);
	CHECK(server->find_entry("c") == 0);
	server->remove_entry(1);
	CHECK(server->find_entry("a") == -1);
	server->set_entry_value(1, "level", 5);
	CHECK(int64_t(server->get_entry_value(1, "level")) == 5);
	CHECK(server->is_dirty());
	CHECK(server->apply() == OK);
	CHECK(server->apply_calls == 1);
	CHECK_FALSE(server->is_dirty());

	for (const String &extension : { String("csv"), String("json"), String("ini") }) {
		const String path = temp_file("list." + extension);
		server->set_file_path(path);
		CHECK(server->save() == OK);

		TestListServer *loaded = memnew(TestListServer);
		loaded->set_file_path(path);
		CHECK(loaded->load() == OK);
		REQUIRE(loaded->get_entry_count() == 2);
		CHECK(String(loaded->get_entry_value(0, "entry_name")) == "c");
		CHECK(int64_t(loaded->get_entry_value(0, "level")) == 3);
		CHECK(String(loaded->get_entry_value(1, "entry_name")) == "b");
		CHECK(int64_t(loaded->get_entry_value(1, "level")) == 5);
		memdelete(loaded);
	}
	memdelete(server);
}

TEST_CASE("[SettingsServer] Single domains fall back from CSV to INI") {
	TestSetup setup;
	ProjectSettings::get_singleton()->set_setting("settings/storage/format", ResourceTextSerializer::FORMAT_CSV);
	TestDomainServer *server = memnew(TestDomainServer);
	ERR_PRINT_OFF;
	CHECK(server->get_file_path().get_extension() == "ini");
	ERR_PRINT_ON;
	memdelete(server);
}

TEST_CASE("[SettingsServer] Manager startup applies registered domains") {
	TestSetup setup;
	const String path = temp_file("manager.ini");
	{
		TestDomainServer *writer = memnew(TestDomainServer);
		writer->set_file_path(path);
		writer->set_value("quality", 0);
		CHECK(writer->save() == OK);
		memdelete(writer);
	}
	TestEngine::quality = 1;

	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	TestDomainServer *server = memnew(TestDomainServer);
	server->domain = "test_startup";
	server->set_file_path(path);
	CHECK(manager->register_server(server) == OK);
	manager->startup(SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->is_started());
	CHECK(TestEngine::quality == 0);
	CHECK(manager->get_startup_phase() >= SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(manager->get_startup_phase() != SettingsServer::APPLY_PHASE_NONE);

	// Registered after its phase: started at once.
	TestDomainServer *late = memnew(TestDomainServer);
	late->domain = "test_late";
	late->set_file_path(temp_file("late.ini"));
	CHECK(manager->register_server(late) == OK);
	CHECK(late->is_started());

	server->set_value("quality", 2);
	CHECK(manager->save_all() == OK);
	manager->reset_all_to_defaults();
	CHECK(int(server->get_value("quality")) == 1);
	memdelete(late);
	memdelete(server);
}

TEST_CASE("[SettingsServer] apply() during a pending confirmation keeps it first") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_setting_flags("quality", SettingsServer::SETTING_FLAG_CONFIRM);
	SIGNAL_WATCH(server, "confirmation_finished");

	server->set_value("quality", 2);
	CHECK(server->apply() == OK);
	CHECK(server->is_confirmation_pending());

	// A second change while the dialog is open: the pending confirmation is kept first.
	server->set_value("volume", 0.25);
	CHECK(server->apply() == OK);
	CHECK_FALSE(server->is_confirmation_pending());
	const Array kept_args = { { true } };
	SIGNAL_CHECK("confirmation_finished", kept_args);
	CHECK(TestEngine::volume == doctest::Approx(0.25));

	// A new confirmation only reverts its own values.
	server->set_value("quality", 0);
	CHECK(server->apply_with_confirmation() == OK);
	CHECK(server->is_confirmation_pending());
	server->set_value("quality", 1);
	CHECK(server->apply_with_confirmation() == OK);
	CHECK(server->is_confirmation_pending());
	SIGNAL_CHECK("confirmation_finished", kept_args);
	server->cancel_confirmation();
	CHECK(TestEngine::quality == 0);
	CHECK(int(server->get_applied_value("quality")) == 0);
	CHECK(TestEngine::volume == doctest::Approx(0.25));

	SIGNAL_UNWATCH(server, "confirmation_finished");
	memdelete(server);
}

TEST_CASE("[SettingsServer] Restart values are cleared on startup and when set back") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(temp_file("restart.ini"));
	server->startup();
	server->set_setting_flags("quality", SettingsServer::SETTING_FLAG_RESTART_REQUIRED);
	SIGNAL_WATCH(server, "restart_required");

	server->set_value("quality", 2);
	CHECK(server->get_restart_required_names() == PackedStringArray({ "quality" }));
	Array restart_args;
	restart_args.push_back(Array({ Variant(PackedStringArray({ "quality" })) }));
	SIGNAL_CHECK("restart_required", restart_args);

	// Another restart value keeps the name listed and emits nothing.
	server->set_value("quality", 0);
	CHECK(server->get_restart_required_names() == PackedStringArray({ "quality" }));
	SIGNAL_CHECK_FALSE("restart_required");

	// Back to the value the engine started with: no restart needed.
	server->set_value("quality", 1);
	CHECK(server->get_restart_required_names().is_empty());
	Array cleared_args;
	cleared_args.push_back(Array({ Variant(PackedStringArray()) }));
	SIGNAL_CHECK("restart_required", cleared_args);

	SIGNAL_UNWATCH(server, "restart_required");
	server->set_value("quality", 2);
	CHECK_FALSE(server->get_restart_required_names().is_empty());
	memdelete(server);

	// A new start: nothing waits for a restart.
	TestEngine::quality = 1;
	TestDomainServer *restarted = memnew(TestDomainServer);
	restarted->set_setting_flags("quality", SettingsServer::SETTING_FLAG_RESTART_REQUIRED);
	restarted->set_file_path(temp_file("restart_again.ini"));
	restarted->set_value("quality", 2);
	CHECK_FALSE(restarted->get_restart_required_names().is_empty());
	restarted->startup();
	CHECK(restarted->get_restart_required_names().is_empty());
	memdelete(restarted);
}

TEST_CASE("[SettingsServer] List domain: unchanged entries are not written") {
	TestSetup setup;
	TestListServer *server = memnew(TestListServer);
	// Defaults with one entry; the pending and applied lists hold copies of it (other resources, same values).
	TypedArray<TestListEntry> default_entries;
	default_entries.push_back(make_entry("a", 1));
	server->get_default_settings()->set("entries", default_entries);
	server->reset_to_defaults();
	const String path = temp_file("list_defaults.ini");
	server->set_file_path(path);
	REQUIRE(server->get_entry_count() == 1);
	const Ref<Resource> default_entry = default_entries[0];
	CHECK(server->get_entry(0) != default_entry);
	CHECK(server->save() == OK);
	CHECK_FALSE(FileAccess::get_file_as_string(path).contains("entries"));

	server->set_entry_value(0, "level", 4);
	CHECK(server->save() == OK);
	CHECK(FileAccess::get_file_as_string(path).contains("entries"));
	memdelete(server);
}

TEST_CASE("[SettingsServer] List domain: entry values are validated") {
	TestSetup setup;
	TestListServer *server = memnew(TestListServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->add_entry(make_entry("a", 1));
	server->set_entry_value(0, "level", 50);
	CHECK(int64_t(server->get_entry_value(0, "level")) == 10);
	server->set_entry_value(0, "level", -3);
	CHECK(int64_t(server->get_entry_value(0, "level")) == 0);

	// Hand-edited entries are clamped on load.
	const String path = temp_file("list_clamp.json");
	server->set_entry_value(0, "level", 7);
	CHECK(server->apply() == OK);
	server->set_file_path(path);
	CHECK(server->save() == OK);
	const String text = FileAccess::get_file_as_string(path);
	REQUIRE(text.contains("7"));
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string(text.replace("7", "99"));
	}
	TestListServer *loaded = memnew(TestListServer);
	loaded->set_file_path(path);
	CHECK(loaded->load() == OK);
	REQUIRE(loaded->get_entry_count() == 1);
	CHECK(int64_t(loaded->get_entry_value(0, "level")) == 10);
	memdelete(loaded);
	memdelete(server);
}

TEST_CASE("[SettingsServer] Domains registered after their phase start at once") {
	TestSetup setup;
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	manager->startup(SettingsServer::APPLY_PHASE_SERVERS);
	REQUIRE(int(manager->get_startup_phase()) >= int(SettingsServer::APPLY_PHASE_SERVERS));

	// Like a script domain from `settings/startup/custom_servers` that asks for an earlier phase.
	TestDomainServer *early = memnew(TestDomainServer);
	early->domain = "test_early_phase";
	early->phase = SettingsServer::APPLY_PHASE_PRE_DISPLAY;
	early->set_file_path(temp_file("early_phase.ini"));
	CHECK(manager->register_server(early) == OK);
	CHECK(early->is_started());

	if (manager->get_startup_phase() < SettingsServer::APPLY_PHASE_SCENE) {
		TestDomainServer *later = memnew(TestDomainServer);
		later->domain = "test_scene_phase";
		later->phase = SettingsServer::APPLY_PHASE_SCENE;
		later->set_file_path(temp_file("scene_phase.ini"));
		CHECK(manager->register_server(later) == OK);
		CHECK_FALSE(later->is_started());
		memdelete(later);
	}
	memdelete(early);
}

} // namespace TestSettingsServer
