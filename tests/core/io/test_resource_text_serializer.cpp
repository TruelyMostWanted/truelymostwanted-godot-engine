/**************************************************************************/
/*  test_resource_text_serializer.cpp                                     */
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

TEST_FORCE_LINK(test_resource_text_serializer)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "tests/test_utils.h"

namespace TestResourceTextSerializer {

class TextFormatEntry : public Resource {
	GDCLASS(TextFormatEntry, Resource);

	String name;
	int64_t level = 1;
	double speed = 1.0;
	bool enabled = true;
	int mode = 0;
	Vector2 offset;
	Color tint = Color(1, 1, 1, 1);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entry_name", "value"), &TextFormatEntry::set_entry_name);
		ClassDB::bind_method(D_METHOD("get_entry_name"), &TextFormatEntry::get_entry_name);
		ClassDB::bind_method(D_METHOD("set_level", "value"), &TextFormatEntry::set_level);
		ClassDB::bind_method(D_METHOD("get_level"), &TextFormatEntry::get_level);
		ClassDB::bind_method(D_METHOD("set_speed", "value"), &TextFormatEntry::set_speed);
		ClassDB::bind_method(D_METHOD("get_speed"), &TextFormatEntry::get_speed);
		ClassDB::bind_method(D_METHOD("set_enabled", "value"), &TextFormatEntry::set_enabled);
		ClassDB::bind_method(D_METHOD("is_enabled"), &TextFormatEntry::is_enabled);
		ClassDB::bind_method(D_METHOD("set_mode", "value"), &TextFormatEntry::set_mode);
		ClassDB::bind_method(D_METHOD("get_mode"), &TextFormatEntry::get_mode);
		ClassDB::bind_method(D_METHOD("set_offset", "value"), &TextFormatEntry::set_offset);
		ClassDB::bind_method(D_METHOD("get_offset"), &TextFormatEntry::get_offset);
		ClassDB::bind_method(D_METHOD("set_tint", "value"), &TextFormatEntry::set_tint);
		ClassDB::bind_method(D_METHOD("get_tint"), &TextFormatEntry::get_tint);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_entry_name", "get_entry_name");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "set_level", "get_level");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Calm,Angry:5,Happy"), "set_mode", "get_mode");
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
		ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint"), "set_tint", "get_tint");
	}

public:
	void set_entry_name(const String &p_value) { name = p_value; }
	String get_entry_name() const { return name; }
	void set_level(int64_t p_value) { level = p_value; }
	int64_t get_level() const { return level; }
	void set_speed(double p_value) { speed = p_value; }
	double get_speed() const { return speed; }
	void set_enabled(bool p_value) { enabled = p_value; }
	bool is_enabled() const { return enabled; }
	void set_mode(int p_value) { mode = p_value; }
	int get_mode() const { return mode; }
	void set_offset(const Vector2 &p_value) { offset = p_value; }
	Vector2 get_offset() const { return offset; }
	void set_tint(const Color &p_value) { tint = p_value; }
	Color get_tint() const { return tint; }
};

class TextFormatSettings : public Resource {
	GDCLASS(TextFormatSettings, Resource);

	double volume = 0.5;
	String title = "Default";
	int quality = 1;
	Ref<TextFormatEntry> main;
	TypedArray<TextFormatEntry> entries;
	Dictionary extra;
	PackedInt32Array numbers;
	Vector3i cell;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_volume", "value"), &TextFormatSettings::set_volume);
		ClassDB::bind_method(D_METHOD("get_volume"), &TextFormatSettings::get_volume);
		ClassDB::bind_method(D_METHOD("set_title", "value"), &TextFormatSettings::set_title);
		ClassDB::bind_method(D_METHOD("get_title"), &TextFormatSettings::get_title);
		ClassDB::bind_method(D_METHOD("set_quality", "value"), &TextFormatSettings::set_quality);
		ClassDB::bind_method(D_METHOD("get_quality"), &TextFormatSettings::get_quality);
		ClassDB::bind_method(D_METHOD("set_main", "value"), &TextFormatSettings::set_main);
		ClassDB::bind_method(D_METHOD("get_main"), &TextFormatSettings::get_main);
		ClassDB::bind_method(D_METHOD("set_entries", "value"), &TextFormatSettings::set_entries);
		ClassDB::bind_method(D_METHOD("get_entries"), &TextFormatSettings::get_entries);
		ClassDB::bind_method(D_METHOD("set_extra", "value"), &TextFormatSettings::set_extra);
		ClassDB::bind_method(D_METHOD("get_extra"), &TextFormatSettings::get_extra);
		ClassDB::bind_method(D_METHOD("set_numbers", "value"), &TextFormatSettings::set_numbers);
		ClassDB::bind_method(D_METHOD("get_numbers"), &TextFormatSettings::get_numbers);
		ClassDB::bind_method(D_METHOD("set_cell", "value"), &TextFormatSettings::set_cell);
		ClassDB::bind_method(D_METHOD("get_cell"), &TextFormatSettings::get_cell);

		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume"), "set_volume", "get_volume");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "quality", PROPERTY_HINT_ENUM, "Low,Medium,High"), "set_quality", "get_quality");
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "main", PROPERTY_HINT_RESOURCE_TYPE, "TextFormatEntry"), "set_main", "get_main");
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entries", PROPERTY_HINT_ARRAY_TYPE, "TextFormatEntry"), "set_entries", "get_entries");
		ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "extra"), "set_extra", "get_extra");
		ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "numbers"), "set_numbers", "get_numbers");
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "cell"), "set_cell", "get_cell");
	}

public:
	void set_volume(double p_value) { volume = p_value; }
	double get_volume() const { return volume; }
	void set_title(const String &p_value) { title = p_value; }
	String get_title() const { return title; }
	void set_quality(int p_value) { quality = p_value; }
	int get_quality() const { return quality; }
	void set_main(const Ref<TextFormatEntry> &p_value) { main = p_value; }
	Ref<TextFormatEntry> get_main() const { return main; }
	void set_entries(const TypedArray<TextFormatEntry> &p_value) { entries = p_value; }
	TypedArray<TextFormatEntry> get_entries() const { return entries; }
	void set_extra(const Dictionary &p_value) { extra = p_value; }
	Dictionary get_extra() const { return extra; }
	void set_numbers(const PackedInt32Array &p_value) { numbers = p_value; }
	PackedInt32Array get_numbers() const { return numbers; }
	void set_cell(const Vector3i &p_value) { cell = p_value; }
	Vector3i get_cell() const { return cell; }
};

class TextFormatList : public Resource {
	GDCLASS(TextFormatList, Resource);

	TypedArray<TextFormatEntry> entries;
	int unused = 3;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entries", "value"), &TextFormatList::set_entries);
		ClassDB::bind_method(D_METHOD("get_entries"), &TextFormatList::get_entries);
		ClassDB::bind_method(D_METHOD("set_unused", "value"), &TextFormatList::set_unused);
		ClassDB::bind_method(D_METHOD("get_unused"), &TextFormatList::get_unused);

		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entries", PROPERTY_HINT_ARRAY_TYPE, "TextFormatEntry"), "set_entries", "get_entries");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "unused"), "set_unused", "get_unused");
	}

public:
	void set_entries(const TypedArray<TextFormatEntry> &p_value) { entries = p_value; }
	TypedArray<TextFormatEntry> get_entries() const { return entries; }
	void set_unused(int p_value) { unused = p_value; }
	int get_unused() const { return unused; }
};

static void register_test_classes() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(TextFormatEntry);
		GDREGISTER_CLASS(TextFormatSettings);
		GDREGISTER_CLASS(TextFormatList);
		registered = true;
	}
}

static Ref<TextFormatEntry> make_entry(const String &p_name, int64_t p_level, int p_mode) {
	Ref<TextFormatEntry> entry;
	entry.instantiate();
	entry->set_entry_name(p_name);
	entry->set_level(p_level);
	entry->set_mode(p_mode);
	entry->set_speed(0.1 * double(p_level));
	entry->set_offset(Vector2(0.1, -2.5));
	entry->set_tint(Color(1, 0, 0, 1));
	return entry;
}

static Ref<TextFormatSettings> make_settings() {
	register_test_classes();
	Ref<TextFormatSettings> settings;
	settings.instantiate();
	settings->set_volume(0.8);
	settings->set_title("Main \"menu\", line\nbreak");
	settings->set_quality(2);
	settings->set_main(make_entry("Ann", 3, 5));
	TypedArray<TextFormatEntry> entries;
	entries.push_back(make_entry("Bob", 1, 0));
	entries.push_back(make_entry("Cleo, the 2nd", 2, 6));
	settings->set_entries(entries);
	Dictionary extra;
	extra["difficulty"] = "hard";
	extra["lives"] = 3;
	settings->set_extra(extra);
	PackedInt32Array numbers;
	numbers.push_back(4);
	numbers.push_back(-7);
	settings->set_numbers(numbers);
	settings->set_cell(Vector3i(1, -2, 3));
	return settings;
}

static void check_entry(const Ref<TextFormatEntry> &p_entry, const String &p_name, int64_t p_level, int p_mode) {
	REQUIRE(p_entry.is_valid());
	CHECK(p_entry->get_entry_name() == p_name);
	CHECK(p_entry->get_level() == p_level);
	CHECK(p_entry->get_mode() == p_mode);
	CHECK(p_entry->get_speed() == doctest::Approx(0.1 * double(p_level)));
	CHECK(p_entry->get_offset().is_equal_approx(Vector2(0.1, -2.5)));
	CHECK(p_entry->get_tint() == Color(1, 0, 0, 1));
}

static void check_settings(const Ref<Resource> &p_resource) {
	const Ref<TextFormatSettings> settings = p_resource;
	REQUIRE(settings.is_valid());
	CHECK(settings->get_volume() == 0.8);
	CHECK(settings->get_title() == "Main \"menu\", line\nbreak");
	CHECK(settings->get_quality() == 2);
	check_entry(settings->get_main(), "Ann", 3, 5);
	const TypedArray<TextFormatEntry> entries = settings->get_entries();
	REQUIRE(entries.size() == 2);
	check_entry(entries[0], "Bob", 1, 0);
	check_entry(entries[1], "Cleo, the 2nd", 2, 6);
	const Dictionary extra = settings->get_extra();
	CHECK(String(extra.get("difficulty", "")) == "hard");
	CHECK(int(extra.get("lives", 0)) == 3);
	const PackedInt32Array numbers = settings->get_numbers();
	REQUIRE(numbers.size() == 2);
	CHECK(numbers[0] == 4);
	CHECK(numbers[1] == -7);
	CHECK(settings->get_cell() == Vector3i(1, -2, 3));
}

TEST_CASE("[ResourceTextSerializer] JSON round trip as text") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	const Ref<TextFormatSettings> settings = make_settings();

	const String text = serializer->serialize(settings, "json");
	CHECK(text.contains("\"$type\": \"TextFormatSettings\""));
	CHECK(text.contains("\"$format_version\": 1"));
	// Enums as names, colors as #rrggbbaa, keys in property order.
	CHECK(text.contains("\"quality\": \"High\""));
	CHECK(text.contains("\"mode\": \"Angry\""));
	CHECK(text.contains("\"#ff0000ff\""));
	CHECK(text.find("\"volume\"") < text.find("\"title\""));
	CHECK(text.find("\"title\"") < text.find("\"entries\""));
	// Nested resources of the declared type have no type key.
	CHECK(text.count("\"$type\"") == 1);

	check_settings(serializer->deserialize(text, "json"));

	const String numbers = serializer->serialize(settings, "json", { { "enums_as_names", false }, { "indent", "" } });
	CHECK(numbers.contains("\"quality\":2"));
	CHECK_FALSE(numbers.contains("\n"));
	check_settings(serializer->deserialize(numbers, "json"));
}

TEST_CASE("[ResourceTextSerializer] Dictionary tree") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<TextFormatSettings> settings = make_settings();
	const Dictionary tree = serializer->resource_to_dictionary(settings);
	CHECK(String(tree["$type"]) == "TextFormatSettings");
	CHECK(String(tree["quality"]) == "High");
	const Array cell = tree["cell"];
	REQUIRE(cell.size() == 3);
	CHECK(int(cell[1]) == -2);
	check_settings(serializer->dictionary_to_resource(tree));

	// Without "$type", the type hint gives the class.
	Dictionary untyped = tree.duplicate();
	untyped.erase("$type");
	check_settings(serializer->dictionary_to_resource(untyped, "TextFormatSettings"));
}

TEST_CASE("[ResourceTextSerializer] JSON through ResourceSaver and ResourceLoader") {
	const Ref<TextFormatSettings> settings = make_settings();
	const String path = TestUtils::get_temp_path("text_formats_settings.json");
	CHECK(ResourceSaver::save(Ref<Resource>(settings), path) == OK);
	CHECK(ResourceLoader::get_resource_type(path) == "TextFormatSettings");

	const Ref<Resource> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	check_settings(loaded);
	const Ref<Resource> hinted = ResourceLoader::load(path, "TextFormatSettings", ResourceFormatLoader::CACHE_MODE_IGNORE);
	check_settings(hinted);
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceTextSerializer] Plain JSON files still load as JSON") {
	register_test_classes();
	const String path = TestUtils::get_temp_path("text_formats_plain.json");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{\"type\": \"Image\", \"name\": \"not a resource file\"}");
	}
	CHECK(ResourceLoader::get_resource_type(path) == "JSON");
	const Ref<JSON> json = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(json.is_valid());
	const Dictionary data = json->get_data();
	CHECK(String(data["name"]) == "not a resource file");

	// With a type hint the same file is read as that type; unknown keys are skipped.
	ERR_PRINT_OFF;
	const Ref<TextFormatEntry> entry = ResourceLoader::load(path, "TextFormatEntry", ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;
	REQUIRE(entry.is_valid());
	CHECK(entry->get_entry_name() == "not a resource file");
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceTextSerializer] Hand-edited file with missing and unknown keys") {
	register_test_classes();
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const String text = "{\"$type\": \"TextFormatEntry\", \"name\": \"Dana\", \"level\": \"7\", \"enabled\": \"no\", \"mode\": \"happy\", \"colour\": \"blue\"}";
	ERR_PRINT_OFF;
	const Ref<TextFormatEntry> entry = serializer->deserialize(text, "json");
	ERR_PRINT_ON;
	REQUIRE(entry.is_valid());
	CHECK(entry->get_entry_name() == "Dana");
	CHECK(entry->get_level() == 7);
	CHECK_FALSE(entry->is_enabled());
	CHECK(entry->get_mode() == 6);
	// Missing keys keep the defaults of the class.
	CHECK(entry->get_speed() == 1.0);
	CHECK(entry->get_tint() == Color(1, 1, 1, 1));

	// A value of the wrong type keeps the default too.
	ERR_PRINT_OFF;
	const Ref<TextFormatEntry> wrong = serializer->deserialize("{\"$type\": \"TextFormatEntry\", \"level\": \"many\"}", "json");
	ERR_PRINT_ON;
	REQUIRE(wrong.is_valid());
	CHECK(wrong->get_level() == 1);
}

TEST_CASE("[ResourceTextSerializer] Wrong types and errors") {
	register_test_classes();
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_PRINT_OFF;
	CHECK(serializer->deserialize("{\"name\": \"no type\"}", "json").is_null());
	CHECK(serializer->deserialize("{\"$type\": \"NoSuchClass\"}", "json").is_null());
	CHECK(serializer->deserialize("not json", "json").is_null());
	// The file holds a TextFormatEntry, but a TextFormatSettings is expected.
	CHECK(serializer->deserialize("{\"$type\": \"TextFormatEntry\"}", "json", "TextFormatSettings").is_null());

	// A resource that contains itself can't be written.
	Ref<Resource> cyclic;
	cyclic.instantiate();
	cyclic->set_meta("self", cyclic);
	CHECK(serializer->serialize(cyclic, "json").is_empty());
	cyclic->remove_meta("self");
	ERR_PRINT_ON;
}

TEST_CASE("[ResourceTextSerializer] INI round trip") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<TextFormatSettings> settings = make_settings();

	SUBCASE("Variant syntax") {
		const String text = serializer->serialize(settings, "ini");
		CHECK(text.begins_with("[root]\n$type=\"TextFormatSettings\"\n"));
		CHECK(text.contains("[root/main]"));
		CHECK(text.contains("[root/entries/0]"));
		CHECK(text.contains("[root/entries/1]"));
		CHECK(text.contains("quality=\"High\""));
		CHECK(text.contains("cell=Vector3i(1, -2, 3)"));
		check_settings(serializer->deserialize(text, "ini"));
	}

	SUBCASE("Plain values") {
		const String text = serializer->serialize(settings, "ini", { { "plain", true } });
		CHECK(text.contains("$type=TextFormatSettings\n"));
		CHECK(text.contains("quality=High\n"));
		CHECK(text.contains("cell=1, -2, 3\n"));
		CHECK(text.contains("name=Ann\n"));
		CHECK(text.contains("tint=#ff0000ff\n"));
		check_settings(serializer->deserialize(text, "ini"));
	}

	SUBCASE("Through ResourceSaver and ResourceLoader") {
		const String path = TestUtils::get_temp_path("text_formats_settings.ini");
		CHECK(ResourceSaver::save(Ref<Resource>(settings), path) == OK);
		CHECK(ResourceLoader::get_resource_type(path) == "TextFormatSettings");
		check_settings(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE));
		DirAccess::remove_absolute(path);
	}
}

TEST_CASE("[ResourceTextSerializer] Hand-written plain INI") {
	register_test_classes();
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const String text = "; Settings written by hand\n"
						"[root]\n"
						"$type=TextFormatSettings\n"
						"volume = 0.25\n"
						"title=Hello World\n"
						"quality=low\n"
						"unknown_key=1\n"
						"\n"
						"[root/entries/0]\n"
						"name=Eve\n"
						"offset=(3, 4)\n";
	ERR_PRINT_OFF;
	const Ref<TextFormatSettings> settings = serializer->deserialize(text, "ini");
	ERR_PRINT_ON;
	REQUIRE(settings.is_valid());
	CHECK(settings->get_volume() == 0.25);
	CHECK(settings->get_title() == "Hello World");
	CHECK(settings->get_quality() == 0);
	const TypedArray<TextFormatEntry> entries = settings->get_entries();
	REQUIRE(entries.size() == 1);
	const Ref<TextFormatEntry> entry = entries[0];
	CHECK(entry->get_entry_name() == "Eve");
	CHECK(entry->get_offset() == Vector2(3, 4));
	CHECK(entry->get_level() == 1);

	// Without [root], the keys before the first section and the type hint are used.
	const Ref<TextFormatEntry> loose = serializer->deserialize("name=Finn\nlevel=4\n", "ini", "TextFormatEntry");
	REQUIRE(loose.is_valid());
	CHECK(loose->get_entry_name() == "Finn");
	CHECK(loose->get_level() == 4);
}

TEST_CASE("[ResourceTextSerializer] CSV round trip") {
	register_test_classes();
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	Ref<TextFormatList> list;
	list.instantiate();
	TypedArray<TextFormatEntry> entries;
	entries.push_back(make_entry("Bob", 1, 0));
	entries.push_back(make_entry("Cleo, \"the\" 2nd", 2, 6));
	list->set_entries(entries);
	CHECK(serializer->can_save_as_csv(list));

	const String text = serializer->serialize(list, "csv");
	CHECK(text.begins_with("name,level,speed,enabled,mode,offset,tint\n"));
	CHECK(text.contains("\"Cleo, \"\"the\"\" 2nd\""));
	CHECK(text.contains("Happy"));

	const Ref<TextFormatList> loaded = serializer->deserialize(text, "csv", "TextFormatList");
	REQUIRE(loaded.is_valid());
	const TypedArray<TextFormatEntry> loaded_entries = loaded->get_entries();
	REQUIRE(loaded_entries.size() == 2);
	check_entry(loaded_entries[0], "Bob", 1, 0);
	check_entry(loaded_entries[1], "Cleo, \"the\" 2nd", 2, 6);

	SUBCASE("Other delimiter") {
		const String semicolons = serializer->serialize(list, "csv", { { "delimiter", ";" } });
		CHECK(semicolons.begins_with("name;level;"));
		const Ref<TextFormatList> back = serializer->deserialize(semicolons, "csv", "TextFormatList", { { "delimiter", ";" } });
		REQUIRE(back.is_valid());
		CHECK(back->get_entries().size() == 2);
	}

	SUBCASE("Through ResourceSaver and ResourceLoader") {
		const String path = TestUtils::get_temp_path("text_formats_list.csv");
		CHECK(ResourceSaver::save(Ref<Resource>(list), path) == OK);
		const Ref<TextFormatList> from_file = ResourceLoader::load(path, "TextFormatList", ResourceFormatLoader::CACHE_MODE_IGNORE);
		REQUIRE(from_file.is_valid());
		CHECK(from_file->get_entries().size() == 2);
		// CSV has no type in the file: without a hint it can't be loaded.
		ERR_PRINT_OFF;
		CHECK(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE).is_null());
		ERR_PRINT_ON;
		DirAccess::remove_absolute(path);
	}

	SUBCASE("Missing columns keep defaults") {
		const Ref<TextFormatList> partial = serializer->deserialize("name\r\nGus\r\n\r\nHal\r\n", "csv", "TextFormatList");
		REQUIRE(partial.is_valid());
		const TypedArray<TextFormatEntry> partial_entries = partial->get_entries();
		REQUIRE(partial_entries.size() == 2);
		const Ref<TextFormatEntry> hal = partial_entries[1];
		CHECK(hal->get_entry_name() == "Hal");
		CHECK(hal->get_level() == 1);
	}
}

TEST_CASE("[ResourceTextSerializer] Resources that do not fit CSV") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<TextFormatSettings> settings = make_settings();
	CHECK_FALSE(serializer->can_save_as_csv(settings));

	const String path = TestUtils::get_temp_path("text_formats_unfit.csv");
	ERR_PRINT_OFF;
	CHECK(serializer->serialize(settings, "csv").is_empty());
	CHECK(ResourceSaver::save(Ref<Resource>(settings), path) == ERR_INVALID_DATA);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(path));

	// A list resource whose other property differs from its default would lose data.
	Ref<TextFormatList> list;
	list.instantiate();
	list->set_unused(9);
	CHECK_FALSE(serializer->can_save_as_csv(list));
	list->set_unused(3);
	CHECK(serializer->can_save_as_csv(list));
}

TEST_CASE("[ResourceTextSerializer] Precision of floats and colors") {
	register_test_classes();
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	Ref<TextFormatEntry> entry;
	entry.instantiate();
	entry->set_speed(0.1 + 0.2);
	entry->set_tint(Color(0.123f, 0.5f, 1.5f, 1.0f)); // Not exact as #rrggbbaa.
	entry->set_offset(Vector2(0.1, 1e-7));

	const Ref<TextFormatEntry> json = serializer->deserialize(serializer->serialize(entry, "json"), "json");
	REQUIRE(json.is_valid());
	CHECK(json->get_speed() == entry->get_speed());
	CHECK(json->get_tint() == entry->get_tint());
	CHECK(json->get_offset() == entry->get_offset());

	const Ref<TextFormatEntry> ini = serializer->deserialize(serializer->serialize(entry, "ini", { { "plain", true } }), "ini");
	REQUIRE(ini.is_valid());
	CHECK(ini->get_speed() == entry->get_speed());
	CHECK(ini->get_tint() == entry->get_tint());
	CHECK(ini->get_offset() == entry->get_offset());
}

} // namespace TestResourceTextSerializer
