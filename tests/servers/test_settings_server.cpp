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
	String legacy_name;
	int apply_calls = 0;
	PackedStringArray last_applied;
	// "name=value" per _notify_value_applied() call.
	PackedStringArray notified;

protected:
	static void _bind_methods() {}

	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override {
		notified.push_back(vformat("%s=%s", p_name, p_value));
	}

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

	virtual String _get_legacy_file_name() const override {
		return legacy_name;
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
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_INI);
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
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/general/folder"));
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
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_JSON);
	CHECK(manager->get_storage_format() == ResourceTextSerializer::FORMAT_JSON);
	CHECK(server->get_file_path() == "user://settings/test_domain.json");
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", "json");
	CHECK(manager->get_storage_format() == ResourceTextSerializer::FORMAT_JSON);
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_INI);

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

	ProjectSettings::get_singleton()->set_setting("settings/general/only_changed_values", false);
	CHECK(server->save() == OK);
	CHECK(FileAccess::get_file_as_string(path).contains("volume"));
	ProjectSettings::get_singleton()->set_setting("settings/general/only_changed_values", true);
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

TEST_CASE("[SettingsServer] The file of the old domain name is read once") {
	TestSetup setup;
	const String path = temp_file("renamed_domain.ini");
	const String legacy_path = temp_file("old_domain.ini");
	{
		Ref<FileAccess> file = FileAccess::open(legacy_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		// The old file names the class of the settings before the rename.
		file->store_string("[root]\n$type=OldDomainSettings\nquality=2\n");
	}
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);
	// Without a legacy name, the old file is not read.
	CHECK(server->load() == ERR_FILE_NOT_FOUND);
	CHECK(int(server->get_value("quality")) == 1);
	server->legacy_name = "old_domain";
	CHECK(server->load() == OK);
	CHECK(int(server->get_value("quality")) == 2);
	// load() alone does not write the new file.
	CHECK_FALSE(FileAccess::exists(path));
	memdelete(server);

	TestDomainServer *started = memnew(TestDomainServer);
	started->legacy_name = "old_domain";
	started->set_file_path(path);
	started->startup();
	CHECK(int(started->get_applied_value("quality")) == 2);
	// startup() writes the new file once; the old file stays.
	CHECK(FileAccess::exists(path));
	CHECK(FileAccess::exists(legacy_path));
	CHECK(FileAccess::get_file_as_string(path).contains("TestDomainSettings"));
	memdelete(started);

	// From now on the new file is read, not the old one.
	{
		Ref<FileAccess> file = FileAccess::open(legacy_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("[root]\nquality=0\n");
	}
	TestEngine::quality = 1;
	TestDomainServer *restarted = memnew(TestDomainServer);
	restarted->legacy_name = "old_domain";
	restarted->set_file_path(path);
	CHECK(restarted->load() == OK);
	CHECK(int(restarted->get_value("quality")) == 2);
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
	ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
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
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_CSV);
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
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_CSV);
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

static bool format_setting_offers(const String &p_setting, const String &p_format) {
	List<PropertyInfo> infos;
	ProjectSettings::get_singleton()->get_property_list(&infos);
	for (const PropertyInfo &info : infos) {
		if (info.name == p_setting) {
			return info.hint == PROPERTY_HINT_ENUM && info.hint_string.contains(p_format);
		}
	}
	return false;
}

// Resets the project settings of a domain to the values a project without them has.
static void reset_domain_settings(const StringName &p_domain) {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	project_settings->set_setting(SettingsServer::get_domain_setting_name(p_domain, "file_name"), String(p_domain));
	project_settings->set_setting(SettingsServer::get_domain_setting_name(p_domain, "override_format"), false);
	project_settings->set_setting(SettingsServer::get_domain_setting_name(p_domain, "format"), ResourceTextSerializer::FORMAT_INI);
}

TEST_CASE("[SettingsServer] Domain project settings: file name, format override, runtime path wins") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	REQUIRE(manager != nullptr);
	TestDomainServer *server = memnew(TestDomainServer);
	server->domain = "test_paths";
	const String file_name_setting = SettingsServer::get_domain_setting_name("test_paths", "file_name");
	const String override_setting = SettingsServer::get_domain_setting_name("test_paths", "override_format");
	const String format_setting = SettingsServer::get_domain_setting_name("test_paths", "format");
	CHECK(file_name_setting == "settings/test_paths/file_name");
	CHECK(override_setting == "settings/test_paths/override_format");
	CHECK(format_setting == "settings/test_paths/format");
	REQUIRE(manager->register_server(server) == OK);

	// Defined on registration: the file name defaults to the domain name, the format is not overridden.
	CHECK(project_settings->has_setting(file_name_setting));
	CHECK(project_settings->has_setting(override_setting));
	CHECK(project_settings->has_setting(format_setting));
	CHECK(String(project_settings->get_setting(file_name_setting)) == "test_paths");
	CHECK_FALSE(bool(project_settings->get_setting(override_setting)));
	CHECK(format_setting_offers(format_setting, "JSON"));
	CHECK(format_setting_offers(format_setting, "YAML"));
	CHECK_FALSE(format_setting_offers(format_setting, "CSV"));
	CHECK(server->get_file_path() == "user://settings/test_paths.ini");

	// Path composition: <folder>/<file_name>.<extension of the effective format>.
	project_settings->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_JSON);
	CHECK(server->get_file_path() == "user://settings/test_paths.json");
	project_settings->set_setting(file_name_setting, "player_one");
	CHECK(server->get_file_path() == "user://settings/player_one.json");

	// The format of the domain is only used with override_format.
	project_settings->set_setting(format_setting, ResourceTextSerializer::FORMAT_INI);
	CHECK(server->get_file_path() == "user://settings/player_one.json");
	project_settings->set_setting(override_setting, true);
	CHECK(server->get_file_path() == "user://settings/player_one.ini");
	project_settings->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_INI);
	project_settings->set_setting(format_setting, ResourceTextSerializer::FORMAT_JSON);
	CHECK(server->get_file_path() == "user://settings/player_one.json");

	// A relative subfolder in the file name; save() creates it.
	const String folder = TestUtils::get_temp_path("settings_server_folder");
	project_settings->set_setting("settings/general/folder", folder);
	project_settings->set_setting(file_name_setting, "profiles/test_paths");
	const String composed_path = folder.path_join("profiles/test_paths.json");
	CHECK(server->get_file_path() == composed_path);
	if (FileAccess::exists(composed_path)) {
		DirAccess::remove_absolute(composed_path);
	}
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_value("quality", 2);
	CHECK(server->apply() == OK);
	CHECK(server->save() == OK);
	CHECK(FileAccess::exists(composed_path));
	CHECK(FileAccess::get_file_as_string(composed_path).strip_edges().begins_with("{"));

	// A path set at runtime wins; there the extension picks the format.
	const String runtime_path = temp_file("runtime_path.ini");
	server->set_file_path(runtime_path);
	CHECK(server->get_file_path() == runtime_path);
	CHECK(server->save() == OK);
	CHECK_FALSE(FileAccess::get_file_as_string(runtime_path).strip_edges().begins_with("{"));
	server->set_file_path(String());
	CHECK(server->get_file_path() == composed_path);

	project_settings->set_setting("settings/general/folder", "user://settings");
	reset_domain_settings("test_paths");
	CHECK(server->get_file_path() == "user://settings/test_paths.ini");
	memdelete(server);

	// List domains can be stored as CSV.
	TestListServer *list = memnew(TestListServer);
	REQUIRE(manager->register_server(list) == OK);
	CHECK(format_setting_offers(SettingsServer::get_domain_setting_name("test_list", "format"), "CSV"));
	memdelete(list);
}

TEST_CASE("[SettingsServer] An invalid file name of the domain is rejected") {
	TestSetup setup;
	CHECK(SettingsServer::is_valid_file_name("audio"));
	CHECK(SettingsServer::is_valid_file_name("profiles/audio"));
	CHECK(SettingsServer::is_valid_file_name("profiles\\audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name(""));
	CHECK_FALSE(SettingsServer::is_valid_file_name("../audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("profiles/../../audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("./audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("profiles//audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("profiles/"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("/audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("user://audio"));
	CHECK_FALSE(SettingsServer::is_valid_file_name("C:/audio"));

	SettingsServer::register_domain_settings("test_invalid_name", false);
	const String file_name_setting = SettingsServer::get_domain_setting_name("test_invalid_name", "file_name");
	TestDomainServer *server = memnew(TestDomainServer);
	server->domain = "test_invalid_name";
	for (const String &file_name : { String("../outside"), String("/absolute"), String("res://inside_pack"), String("a//b") }) {
		ProjectSettings::get_singleton()->set_setting(file_name_setting, file_name);
		ERR_PRINT_OFF;
		CHECK(server->get_file_path() == "user://settings/test_invalid_name.ini");
		ERR_PRINT_ON;
	}
	// An empty file name is the domain name.
	ProjectSettings::get_singleton()->set_setting(file_name_setting, String());
	CHECK(server->get_file_path() == "user://settings/test_invalid_name.ini");
	memdelete(server);
	reset_domain_settings("test_invalid_name");
}

TEST_CASE("[SettingsServer] The extension of the runtime path picks the format") {
	TestSetup setup;
	for (const String &extension : { String("json"), String("ini"), String("xml"), String("toml"), String("yaml"), String("yml") }) {
		const ResourceTextSerializer::Format format = ResourceTextSerializer::get_format_for_extension(extension);
		if (!ResourceTextSerializer::has_format_implementation(format)) {
			continue; // The TOML or YAML module is not in this build.
		}
		const String path = temp_file("by_extension." + extension);
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;
		{
			TestDomainServer *server = memnew(TestDomainServer);
			server->domain = "test_formats";
			server->set_file_path(path);
			server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
			CHECK(server->get_file_path() == path);
			server->set_value("volume", 0.25);
			server->set_value("quality", 2);
			CHECK(server->apply() == OK);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path).strip_edges();
		if (format == ResourceTextSerializer::FORMAT_JSON) {
			CHECK(text.begins_with("{"));
		} else if (format == ResourceTextSerializer::FORMAT_XML) {
			CHECK(text.begins_with("<"));
		} else {
			CHECK_FALSE(text.begins_with("{"));
			CHECK_FALSE(text.begins_with("<"));
		}
		TestEngine::volume = 0.5;
		TestEngine::quality = 1;
		TestDomainServer *server = memnew(TestDomainServer);
		server->domain = "test_formats";
		server->set_file_path(path);
		CHECK(server->load() == OK);
		CHECK(double(server->get_value("volume")) == doctest::Approx(0.25));
		CHECK(int(server->get_value("quality")) == 2);
		memdelete(server);
	}
}

TEST_CASE("[SettingsServer] An unknown extension falls back to the format of the domain") {
	TestSetup setup;
	const String path = temp_file("unknown_extension.cfg");
	ProjectSettings::get_singleton()->set_setting("settings/general/default_format", ResourceTextSerializer::FORMAT_JSON);

	TestDomainServer *server = memnew(TestDomainServer);
	server->domain = "test_unknown_extension";
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	CHECK(server->get_file_path() == path);
	server->set_value("quality", 0);
	CHECK(server->apply() == OK);
	ERR_PRINT_OFF;
	CHECK(server->save() == OK);
	ERR_PRINT_ON;
	CHECK(FileAccess::get_file_as_string(path).strip_edges().begins_with("{"));
	memdelete(server);

	TestEngine::quality = 1;
	TestDomainServer *loaded = memnew(TestDomainServer);
	loaded->domain = "test_unknown_extension";
	loaded->set_file_path(path);
	ERR_PRINT_OFF;
	CHECK(loaded->load() == OK);
	ERR_PRINT_ON;
	CHECK(int(loaded->get_value("quality")) == 0);
	memdelete(loaded);
}

TEST_CASE("[SettingsServer] load_failed and save_failed report the error and the path") {
	TestSetup setup;
	TestDomainServer *server = memnew(TestDomainServer);
	SIGNAL_WATCH(server, "load_failed");
	SIGNAL_WATCH(server, "save_failed");
	SIGNAL_WATCH(server, "loaded");

	// A missing file is the normal first start, not a failure.
	const String path = temp_file("failures.json");
	server->set_file_path(path);
	CHECK(server->load() == ERR_FILE_NOT_FOUND);
	SIGNAL_CHECK_FALSE("load_failed");
	SIGNAL_DISCARD("loaded");

	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{ \"volume\": ");
	}
	ERR_PRINT_OFF;
	const Error load_error = server->load();
	ERR_PRINT_ON;
	CHECK(load_error != OK);
	Array load_args;
	load_args.push_back(Array({ Variant(int(load_error)), Variant(path) }));
	SIGNAL_CHECK("load_failed", load_args);
	// `loaded` follows: the defaults are in use.
	Array loaded_args;
	loaded_args.push_back(Array());
	SIGNAL_CHECK("loaded", loaded_args);

	// A folder that can't be created: its parent is a file.
	const String blocker = temp_file("blocker.txt");
	{
		Ref<FileAccess> file = FileAccess::open(blocker, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("not a folder");
	}
	const String bad_path = blocker.path_join("sub").path_join("failures.ini");
	server->set_file_path(bad_path);
	ERR_PRINT_OFF;
	const Error save_error = server->save();
	ERR_PRINT_ON;
	CHECK(save_error != OK);
	Array save_args;
	save_args.push_back(Array({ Variant(int(save_error)), Variant(bad_path) }));
	SIGNAL_CHECK("save_failed", save_args);

	server->set_file_path(path);
	CHECK(server->save() == OK);
	SIGNAL_CHECK_FALSE("save_failed");

	SIGNAL_UNWATCH(server, "load_failed");
	SIGNAL_UNWATCH(server, "save_failed");
	SIGNAL_UNWATCH(server, "loaded");
	memdelete(server);
}

TEST_CASE("[SettingsServer] settings_changed names all values changed by one operation") {
	TestSetup setup;
	const String path = temp_file("settings_changed.ini");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("[root]\n$type=TestDomainSettings\nvolume=0.25\nquality=High\n");
	}
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_file_path(path);
	SIGNAL_WATCH(server, "settings_changed");

	server->set_value("volume", 0.75);
	Array set_args;
	set_args.push_back(Array({ Variant(PackedStringArray({ "volume" })) }));
	SIGNAL_CHECK("settings_changed", set_args);

	// An unchanged value emits nothing.
	server->set_value("volume", 0.75);
	SIGNAL_CHECK_FALSE("settings_changed");

	// load() changes both values at once (volume 0.75 -> 0.25, quality 1 -> 2).
	CHECK(server->load() == OK);
	Array load_args;
	load_args.push_back(Array({ Variant(PackedStringArray({ "volume", "quality" })) }));
	SIGNAL_CHECK("settings_changed", load_args);

	// revert() goes back to the applied defaults.
	server->revert();
	SIGNAL_CHECK("settings_changed", load_args);

	SIGNAL_UNWATCH(server, "settings_changed");
	memdelete(server);
}

TEST_CASE("[SettingsServer] _notify_value_applied() runs once per value that reaches the engine") {
	TestSetup setup;
	const String path = temp_file("notify_applied.ini");
	TestDomainServer *server = memnew(TestDomainServer);
	server->set_file_path(path);

	// Immediate mode: once per change, with the applied value already readable.
	server->set_value("volume", 0.25);
	CHECK(server->notified == PackedStringArray({ "volume=0.25" }));
	CHECK(double(server->get_applied_value("volume")) == doctest::Approx(0.25));
	// An unchanged value does not reach the engine again.
	server->set_value("volume", 0.25);
	CHECK(server->notified.size() == 1);

	// Manual mode: pending changes are not applied; apply() notifies each changed value once.
	server->notified.clear();
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_value("volume", 0.75);
	server->set_value("quality", 2);
	CHECK(server->notified.is_empty());
	server->revert();
	CHECK(server->notified.is_empty());
	server->set_value("quality", 2);
	CHECK(server->apply() == OK);
	CHECK(server->notified == PackedStringArray({ "quality=2" }));

	// reset_to_defaults() in manual mode waits for apply().
	server->notified.clear();
	server->reset_to_defaults();
	CHECK(server->notified.is_empty());
	CHECK(server->apply() == OK);
	CHECK(server->notified.size() == 2);
	CHECK(server->notified.has("volume=0.5"));
	CHECK(server->notified.has("quality=1"));

	// Confirmation: notified when applied and for the restored value on cancel, not on confirm.
	server->notified.clear();
	server->set_apply_mode(SettingsServer::APPLY_MODE_IMMEDIATE);
	server->set_setting_flags("quality", SettingsServer::SETTING_FLAG_CONFIRM);
	server->set_value("quality", 0);
	CHECK(server->notified == PackedStringArray({ "quality=0" }));
	server->cancel_confirmation();
	CHECK(server->notified == PackedStringArray({ "quality=0", "quality=1" }));
	server->notified.clear();
	server->set_value("quality", 2);
	server->confirm_changes();
	CHECK(server->notified == PackedStringArray({ "quality=2" }));

	// Session overrides reach the engine too.
	server->notified.clear();
	server->set_session_override("volume", 0.1);
	CHECK(server->notified == PackedStringArray({ "volume=0.1" }));
	server->clear_session_override("volume");
	CHECK(server->notified == PackedStringArray({ "volume=0.1", "volume=0.5" }));

	// Startup: load + apply notifies the values of the file that differ from the defaults.
	CHECK(server->save() == OK);
	memdelete(server);
	// A fresh engine: the defaults are captured from it.
	TestEngine::volume = 0.5;
	TestEngine::quality = 1;
	TestDomainServer *restarted = memnew(TestDomainServer);
	restarted->set_file_path(path);
	restarted->startup();
	CHECK(restarted->notified == PackedStringArray({ "quality=2" }));
	memdelete(restarted);
}

} // namespace TestSettingsServer
