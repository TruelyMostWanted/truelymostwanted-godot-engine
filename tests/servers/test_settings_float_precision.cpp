/**************************************************************************/
/*  test_settings_float_precision.cpp                                     */
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

TEST_FORCE_LINK(test_settings_float_precision)

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_text_serializer.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "servers/settings/list_settings_server.h"
#include "servers/settings/settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#include <cmath>

namespace TestSettingsFloatPrecision {

// Settings with 32-bit members, as most engine settings resources store them.
class FloatPrecisionSettings : public Resource {
	GDCLASS(FloatPrecisionSettings, Resource);

	float volume_db = 0.0f;
	float deadzone = 0.2f;
	float factor = 1.0f;
	float near = 0.05f;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_volume_db", "value"), &FloatPrecisionSettings::set_volume_db);
		ClassDB::bind_method(D_METHOD("get_volume_db"), &FloatPrecisionSettings::get_volume_db);
		ClassDB::bind_method(D_METHOD("set_deadzone", "value"), &FloatPrecisionSettings::set_deadzone);
		ClassDB::bind_method(D_METHOD("get_deadzone"), &FloatPrecisionSettings::get_deadzone);
		ClassDB::bind_method(D_METHOD("set_factor", "value"), &FloatPrecisionSettings::set_factor);
		ClassDB::bind_method(D_METHOD("get_factor"), &FloatPrecisionSettings::get_factor);
		ClassDB::bind_method(D_METHOD("set_near", "value"), &FloatPrecisionSettings::set_near);
		ClassDB::bind_method(D_METHOD("get_near"), &FloatPrecisionSettings::get_near);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.1,suffix:dB"), "set_volume_db", "get_volume_db");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "deadzone", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_deadzone", "get_deadzone");
		// No range hint: three decimals.
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "factor"), "set_factor", "get_factor");
		// A step below 0.001: three decimals.
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near", PROPERTY_HINT_RANGE, "0.0001,10,0.0001,or_greater"), "set_near", "get_near");
	}

public:
	void set_volume_db(float p_value) { volume_db = p_value; }
	float get_volume_db() const { return volume_db; }
	void set_deadzone(float p_value) { deadzone = p_value; }
	float get_deadzone() const { return deadzone; }
	void set_factor(float p_value) { factor = p_value; }
	float get_factor() const { return factor; }
	void set_near(float p_value) { near = p_value; }
	float get_near() const { return near; }
};

// The "engine" of the test domain: a linear volume, read back as dB.
struct FloatPrecisionEngine {
	static inline double volume_linear = 1.0;
};

class FloatPrecisionServer : public SettingsServer {
	GDCLASS(FloatPrecisionServer, SettingsServer);

protected:
	static void _bind_methods() {}

	virtual Ref<Resource> _create_default_settings() const override {
		Ref<FloatPrecisionSettings> default_settings;
		default_settings.instantiate();
		return default_settings;
	}

	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override {
		// linear_to_db(1.0) is not exactly 0.0 for a linear volume that went through a float.
		p_settings->set("volume_db", Math::linear_to_db(double(float(FloatPrecisionEngine::volume_linear) * 0.9999999f)));
	}

public:
	virtual StringName get_domain() const override { return "float_precision"; }

	void set_volume_linear(double p_linear) {
		set_value("volume_db", p_linear > 0.0 ? Math::linear_to_db(p_linear) : -80.0);
	}
};

class FloatPrecisionEntry : public Resource {
	GDCLASS(FloatPrecisionEntry, Resource);

	String entry_name;
	float gain_db = 0.0f;
	float rate = 1.0f;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entry_name", "value"), &FloatPrecisionEntry::set_entry_name);
		ClassDB::bind_method(D_METHOD("get_entry_name"), &FloatPrecisionEntry::get_entry_name);
		ClassDB::bind_method(D_METHOD("set_gain_db", "value"), &FloatPrecisionEntry::set_gain_db);
		ClassDB::bind_method(D_METHOD("get_gain_db"), &FloatPrecisionEntry::get_gain_db);
		ClassDB::bind_method(D_METHOD("set_rate", "value"), &FloatPrecisionEntry::set_rate);
		ClassDB::bind_method(D_METHOD("get_rate"), &FloatPrecisionEntry::get_rate);
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "entry_name"), "set_entry_name", "get_entry_name");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gain_db", PROPERTY_HINT_RANGE, "-80,24,0.1"), "set_gain_db", "get_gain_db");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rate", PROPERTY_HINT_RANGE, "0.1,10,0.01"), "set_rate", "get_rate");
	}

public:
	void set_entry_name(const String &p_value) { entry_name = p_value; }
	String get_entry_name() const { return entry_name; }
	void set_gain_db(float p_value) { gain_db = p_value; }
	float get_gain_db() const { return gain_db; }
	void set_rate(float p_value) { rate = p_value; }
	float get_rate() const { return rate; }
};

class FloatPrecisionList : public Resource {
	GDCLASS(FloatPrecisionList, Resource);

	TypedArray<FloatPrecisionEntry> entries;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entries", "value"), &FloatPrecisionList::set_entries);
		ClassDB::bind_method(D_METHOD("get_entries"), &FloatPrecisionList::get_entries);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entries", PROPERTY_HINT_ARRAY_TYPE, "FloatPrecisionEntry"), "set_entries", "get_entries");
	}

public:
	void set_entries(const TypedArray<FloatPrecisionEntry> &p_value) { entries = p_value; }
	TypedArray<FloatPrecisionEntry> get_entries() const { return entries; }
};

class FloatPrecisionListServer : public ListSettingsServer {
	GDCLASS(FloatPrecisionListServer, ListSettingsServer);

protected:
	static void _bind_methods() {}

	virtual Ref<Resource> _create_default_settings() const override {
		Ref<FloatPrecisionList> default_settings;
		default_settings.instantiate();
		return default_settings;
	}

public:
	virtual StringName get_domain() const override { return "float_precision_list"; }
	virtual StringName get_entry_key() const override { return "entry_name"; }
};

static void register_test_classes() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(FloatPrecisionSettings);
		GDREGISTER_CLASS(FloatPrecisionServer);
		GDREGISTER_CLASS(FloatPrecisionEntry);
		GDREGISTER_CLASS(FloatPrecisionList);
		GDREGISTER_CLASS(FloatPrecisionListServer);
		registered = true;
	}
}

struct TestSetup {
	TestSetup() {
		register_test_classes();
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
		FloatPrecisionEngine::volume_linear = 1.0;
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/general/only_changed_values", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("settings_float_precision").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

static Ref<FloatPrecisionEntry> make_entry(const String &p_name, float p_gain_db, float p_rate) {
	Ref<FloatPrecisionEntry> entry;
	entry.instantiate();
	entry->set_entry_name(p_name);
	entry->set_gain_db(p_gain_db);
	entry->set_rate(p_rate);
	return entry;
}

// The text of a float in a file must not carry the noise of a 32-bit value or of a conversion.
static void check_no_float_noise(const String &p_text) {
	CHECK_FALSE(p_text.contains("0000000"));
	CHECK_FALSE(p_text.contains("9999999"));
	CHECK_FALSE(p_text.contains("e-"));
	CHECK_FALSE(p_text.contains("-0.0,"));
	CHECK_FALSE(p_text.contains("=-0.0\n"));
}

TEST_CASE("[SettingsServer] snap_float(): step, offset, three decimals and -0.0") {
	CHECK(SettingsServer::snap_float(0.1 + 0.2) == 0.3);
	CHECK(SettingsServer::snap_float(double(0.2f)) == 0.2);
	CHECK(SettingsServer::snap_float(1.23456) == 1.235);
	CHECK(SettingsServer::snap_float(-6.0206, 0.1) == -6.0);
	CHECK(SettingsServer::snap_float(0.333, 0.05) == 0.35);
	// Relative to the range minimum, like Range.
	CHECK(SettingsServer::snap_float(0.12, 0.1, 0.05) == 0.15);
	// A step below 0.001 counts as 0.001.
	CHECK(SettingsServer::snap_float(0.00049, 0.0001) == 0.0);
	CHECK(SettingsServer::snap_float(0.0123456, 0.0) == 0.012);

	const double negative_zero = SettingsServer::snap_float(-0.0000014);
	CHECK(negative_zero == 0.0);
	CHECK_FALSE(std::signbit(negative_zero));
	CHECK_FALSE(std::signbit(SettingsServer::snap_float(-0.0)));
	CHECK_FALSE(std::signbit(SettingsServer::snap_float(-0.04, 0.1)));

	CHECK(Math::is_inf(SettingsServer::snap_float(Math::INF)));
	CHECK(Math::is_nan(SettingsServer::snap_float(Math::NaN)));
	// 3 * 0.1 is the double nearest to 0.3 and written as 0.3.
	CHECK(String::num_scientific(SettingsServer::snap_float(0.1 * 3.0, 0.1)) == "0.3");
}

TEST_CASE("[SettingsServer] Float values are snapped when set; getters and signals see the snapped value") {
	TestSetup setup;
	FloatPrecisionServer *server = memnew(FloatPrecisionServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	// The default read from the engine (linear 1.0 through a float) is 0.0 dB, not -0.0000008 dB or -0.0.
	const Variant default_db = server->get_default_value("volume_db");
	CHECK(default_db.get_type() == Variant::FLOAT);
	CHECK(double(default_db) == 0.0);
	CHECK_FALSE(std::signbit(double(default_db)));
	// A 32-bit member (0.2f) is returned as 0.2.
	CHECK(double(server->get_value("deadzone")) == 0.2);
	CHECK(double(server->get_default_value("deadzone")) == 0.2);
	CHECK(double(server->get_value("near")) == 0.05);

	SIGNAL_WATCH(server, "value_changed");
	server->set_value("deadzone", 0.123456);
	CHECK(double(server->get_value("deadzone")) == 0.12);
	const Array changed_args = { { StringName("deadzone"), 0.12 } };
	SIGNAL_CHECK("value_changed", changed_args);

	// The same value again (the member holds 0.12f) is no change.
	server->set_value("deadzone", 0.12);
	SIGNAL_CHECK_FALSE("value_changed");
	SIGNAL_UNWATCH(server, "value_changed");

	server->set_value("volume_db", -6.0206);
	CHECK(double(server->get_value("volume_db")) == -6.0);
	server->set_value("factor", 1.23456789);
	CHECK(double(server->get_value("factor")) == 1.235);
	server->set_value("near", 0.0123);
	CHECK(double(server->get_value("near")) == 0.012);
	server->set_value("factor", -0.0001);
	CHECK(double(server->get_value("factor")) == 0.0);
	CHECK_FALSE(std::signbit(double(server->get_value("factor"))));
	// An int for a float setting becomes a float.
	server->set_value("factor", 2);
	CHECK(server->get_value("factor").get_type() == Variant::FLOAT);
	CHECK(double(server->get_value("factor")) == 2.0);
	memdelete(server);
}

TEST_CASE("[SettingsServer] Linear volume to dB is snapped to the step of the range") {
	TestSetup setup;
	FloatPrecisionServer *server = memnew(FloatPrecisionServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->set_volume_linear(1.0);
	CHECK(double(server->get_value("volume_db")) == 0.0);
	server->set_volume_linear(0.5); // -6.0206 dB.
	CHECK(double(server->get_value("volume_db")) == -6.0);
	server->set_volume_linear(0.99999); // -0.0000869 dB.
	CHECK(double(server->get_value("volume_db")) == 0.0);
	CHECK_FALSE(std::signbit(double(server->get_value("volume_db"))));
	memdelete(server);
}

TEST_CASE("[SettingsServer] Files hold float values without noise (INI, JSON, CSV)") {
	TestSetup setup;
	ProjectSettings::get_singleton()->set_setting("settings/general/only_changed_values", false);
	for (const String &extension : { String("ini"), String("json") }) {
		const String path = temp_file("values." + extension);
		{
			FloatPrecisionServer *server = memnew(FloatPrecisionServer);
			server->set_file_path(path);
			server->set_value("deadzone", 0.3);
			server->set_value("volume_db", -6.0206);
			server->set_value("factor", 0.1 + 0.2);
			CHECK(server->apply() == OK);
			CHECK(server->save() == OK);
			memdelete(server);
		}
		const String text = FileAccess::get_file_as_string(path);
		CHECK(text.contains("0.3"));
		CHECK(text.contains("-6"));
		CHECK_FALSE(text.contains("-6.02"));
		CHECK(text.contains("0.05"));
		check_no_float_noise(text);

		FloatPrecisionServer *server = memnew(FloatPrecisionServer);
		server->set_file_path(path);
		CHECK(server->load() == OK);
		CHECK(double(server->get_value("deadzone")) == 0.3);
		CHECK(double(server->get_value("volume_db")) == -6.0);
		CHECK(double(server->get_value("factor")) == 0.3);
		memdelete(server);
	}

	const String csv_path = temp_file("entries.csv");
	{
		FloatPrecisionListServer *server = memnew(FloatPrecisionListServer);
		server->set_file_path(csv_path);
		server->add_entry(make_entry("Master", 0.0f, 1.0f));
		server->add_entry(make_entry("Music", -6.0206f, 0.33333f));
		CHECK(server->apply() == OK);
		CHECK(server->save() == OK);
		memdelete(server);
	}
	const String csv = FileAccess::get_file_as_string(csv_path);
	CHECK(csv.contains("Music,-6.0,0.33") || csv.contains("Music,-6,0.33"));
	check_no_float_noise(csv);
}

TEST_CASE("[SettingsServer] List entries: float fields are snapped when added, set and loaded") {
	TestSetup setup;
	FloatPrecisionListServer *server = memnew(FloatPrecisionListServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	server->add_entry(make_entry("Voice", -3.14159f, 1.23456f));
	CHECK(double(server->get_entry_value(0, "gain_db")) == -3.1);
	CHECK(double(server->get_entry_value(0, "rate")) == 1.23);

	server->set_entry_value(0, "gain_db", Math::linear_to_db(0.5));
	CHECK(double(server->get_entry_value(0, "gain_db")) == -6.0);
	server->set_entry_value(0, "gain_db", -0.0001);
	CHECK(double(server->get_entry_value(0, "gain_db")) == 0.0);
	CHECK_FALSE(std::signbit(double(server->get_entry_value(0, "gain_db"))));
	server->set_entry_value(0, "rate", 0.2);
	CHECK(double(server->get_entry_value(0, "rate")) == 0.2);

	// A hand-edited file is snapped when it is loaded.
	const String path = temp_file("hand_edited.json");
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string("{\"$type\": \"FloatPrecisionList\", \"entries\": [{\"$type\": \"FloatPrecisionEntry\", \"entry_name\": \"Music\", \"gain_db\": -12.3456789, \"rate\": 0.20000000298023224}]}");
	file->close();
	file.unref();
	server->set_file_path(path);
	CHECK(server->load() == OK);
	REQUIRE(server->get_entry_count() == 1);
	CHECK(double(server->get_entry_value(0, "gain_db")) == -12.3);
	CHECK(double(server->get_entry_value(0, "rate")) == 0.2);
	memdelete(server);
}

} // namespace TestSettingsFloatPrecision
