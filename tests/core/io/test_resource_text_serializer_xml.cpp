/**************************************************************************/
/*  test_resource_text_serializer_xml.cpp                                 */
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

TEST_FORCE_LINK(test_resource_text_serializer_xml)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "core/variant/typed_dictionary.h"
#include "tests/test_utils.h"

namespace TestResourceTextSerializerXML {

class XMLTestEntry : public Resource {
	GDCLASS(XMLTestEntry, Resource);

	String name;
	int64_t level = 1;
	double speed = 1.0;
	bool enabled = false;
	int mode = 0;
	Vector2 offset;
	Color tint = Color(1, 1, 1, 1);
	String code;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_entry_name", "value"), &XMLTestEntry::set_entry_name);
		ClassDB::bind_method(D_METHOD("get_entry_name"), &XMLTestEntry::get_entry_name);
		ClassDB::bind_method(D_METHOD("set_level", "value"), &XMLTestEntry::set_level);
		ClassDB::bind_method(D_METHOD("get_level"), &XMLTestEntry::get_level);
		ClassDB::bind_method(D_METHOD("set_speed", "value"), &XMLTestEntry::set_speed);
		ClassDB::bind_method(D_METHOD("get_speed"), &XMLTestEntry::get_speed);
		ClassDB::bind_method(D_METHOD("set_enabled", "value"), &XMLTestEntry::set_enabled);
		ClassDB::bind_method(D_METHOD("is_enabled"), &XMLTestEntry::is_enabled);
		ClassDB::bind_method(D_METHOD("set_mode", "value"), &XMLTestEntry::set_mode);
		ClassDB::bind_method(D_METHOD("get_mode"), &XMLTestEntry::get_mode);
		ClassDB::bind_method(D_METHOD("set_offset", "value"), &XMLTestEntry::set_offset);
		ClassDB::bind_method(D_METHOD("get_offset"), &XMLTestEntry::get_offset);
		ClassDB::bind_method(D_METHOD("set_tint", "value"), &XMLTestEntry::set_tint);
		ClassDB::bind_method(D_METHOD("get_tint"), &XMLTestEntry::get_tint);
		ClassDB::bind_method(D_METHOD("set_code", "value"), &XMLTestEntry::set_code);
		ClassDB::bind_method(D_METHOD("get_code"), &XMLTestEntry::get_code);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_entry_name", "get_entry_name");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "set_level", "get_level");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Calm,Angry:5,Happy"), "set_mode", "get_mode");
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
		ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint"), "set_tint", "get_tint");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "code"), "set_code", "get_code");
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
	void set_code(const String &p_value) { code = p_value; }
	String get_code() const { return code; }
};

class XMLTestSettings : public Resource {
	GDCLASS(XMLTestSettings, Resource);

	String title;
	Ref<XMLTestEntry> main;
	TypedArray<XMLTestEntry> entries;
	TypedDictionary<String, int> scores;
	Dictionary extra;
	PackedInt32Array numbers;
	PackedVector2Array points;
	Variant anything;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_title", "value"), &XMLTestSettings::set_title);
		ClassDB::bind_method(D_METHOD("get_title"), &XMLTestSettings::get_title);
		ClassDB::bind_method(D_METHOD("set_main", "value"), &XMLTestSettings::set_main);
		ClassDB::bind_method(D_METHOD("get_main"), &XMLTestSettings::get_main);
		ClassDB::bind_method(D_METHOD("set_entries", "value"), &XMLTestSettings::set_entries);
		ClassDB::bind_method(D_METHOD("get_entries"), &XMLTestSettings::get_entries);
		ClassDB::bind_method(D_METHOD("set_scores", "value"), &XMLTestSettings::set_scores);
		ClassDB::bind_method(D_METHOD("get_scores"), &XMLTestSettings::get_scores);
		ClassDB::bind_method(D_METHOD("set_extra", "value"), &XMLTestSettings::set_extra);
		ClassDB::bind_method(D_METHOD("get_extra"), &XMLTestSettings::get_extra);
		ClassDB::bind_method(D_METHOD("set_numbers", "value"), &XMLTestSettings::set_numbers);
		ClassDB::bind_method(D_METHOD("get_numbers"), &XMLTestSettings::get_numbers);
		ClassDB::bind_method(D_METHOD("set_points", "value"), &XMLTestSettings::set_points);
		ClassDB::bind_method(D_METHOD("get_points"), &XMLTestSettings::get_points);
		ClassDB::bind_method(D_METHOD("set_anything", "value"), &XMLTestSettings::set_anything);
		ClassDB::bind_method(D_METHOD("get_anything"), &XMLTestSettings::get_anything);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "main", PROPERTY_HINT_RESOURCE_TYPE, "XMLTestEntry"), "set_main", "get_main");
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entries", PROPERTY_HINT_ARRAY_TYPE, "XMLTestEntry"), "set_entries", "get_entries");
		ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "scores", PROPERTY_HINT_DICTIONARY_TYPE, "String;int"), "set_scores", "get_scores");
		ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "extra"), "set_extra", "get_extra");
		ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "numbers"), "set_numbers", "get_numbers");
		ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "points"), "set_points", "get_points");
		ADD_PROPERTY(PropertyInfo(Variant::NIL, "anything", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_anything", "get_anything");
	}

public:
	void set_title(const String &p_value) { title = p_value; }
	String get_title() const { return title; }
	void set_main(const Ref<XMLTestEntry> &p_value) { main = p_value; }
	Ref<XMLTestEntry> get_main() const { return main; }
	void set_entries(const TypedArray<XMLTestEntry> &p_value) { entries = p_value; }
	TypedArray<XMLTestEntry> get_entries() const { return entries; }
	void set_scores(const TypedDictionary<String, int> &p_value) { scores = p_value; }
	TypedDictionary<String, int> get_scores() const { return scores; }
	void set_extra(const Dictionary &p_value) { extra = p_value; }
	Dictionary get_extra() const { return extra; }
	void set_numbers(const PackedInt32Array &p_value) { numbers = p_value; }
	PackedInt32Array get_numbers() const { return numbers; }
	void set_points(const PackedVector2Array &p_value) { points = p_value; }
	PackedVector2Array get_points() const { return points; }
	void set_anything(const Variant &p_value) { anything = p_value; }
	Variant get_anything() const { return anything; }
};

static void register_xml_test_classes() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(XMLTestEntry);
		GDREGISTER_CLASS(XMLTestSettings);
		registered = true;
	}
}

static Ref<XMLTestEntry> make_xml_entry(const String &p_name, int64_t p_level) {
	register_xml_test_classes();
	Ref<XMLTestEntry> entry;
	entry.instantiate();
	entry->set_entry_name(p_name);
	entry->set_level(p_level);
	entry->set_speed(0.5);
	entry->set_enabled(true);
	entry->set_mode(5);
	entry->set_offset(Vector2(0.5, -2));
	entry->set_tint(Color(1, 0, 0, 1));
	entry->set_code("007");
	return entry;
}

static Ref<XMLTestSettings> make_xml_settings() {
	register_xml_test_classes();
	Ref<XMLTestSettings> settings;
	settings.instantiate();
	settings->set_title("Fish & <Chips> \"quoted\"\nsecond line\r\n");
	settings->set_main(make_xml_entry("Ann", 3));
	TypedArray<XMLTestEntry> entries;
	entries.push_back(make_xml_entry("Bob", 1));
	entries.push_back(make_xml_entry(" ", 2)); // Whitespace only: written as CDATA.
	settings->set_entries(entries);
	TypedDictionary<String, int> scores;
	scores["ann"] = 10;
	scores["my key"] = 20; // Not an XML name: <entry key="my key">.
	settings->set_scores(scores);
	Dictionary extra;
	extra["difficulty"] = "hard";
	extra["lives"] = 3;
	extra["ratio"] = 1.0;
	extra["empty"] = Dictionary();
	extra["none"] = Variant();
	settings->set_extra(extra);
	PackedInt32Array numbers;
	numbers.push_back(4);
	numbers.push_back(-7);
	settings->set_numbers(numbers);
	PackedVector2Array points;
	points.push_back(Vector2(1, 2));
	points.push_back(Vector2(-0.25, 8));
	settings->set_points(points);
	Array anything;
	anything.push_back(1);
	anything.push_back(2.5);
	anything.push_back("3");
	anything.push_back(true);
	anything.push_back(Variant());
	anything.push_back(Array());
	settings->set_anything(anything);
	return settings;
}

static void check_xml_entry(const Ref<XMLTestEntry> &p_entry, const String &p_name, int64_t p_level) {
	REQUIRE(p_entry.is_valid());
	CHECK(p_entry->get_entry_name() == p_name);
	CHECK(p_entry->get_level() == p_level);
	CHECK(p_entry->get_speed() == 0.5);
	CHECK(p_entry->is_enabled());
	CHECK(p_entry->get_mode() == 5);
	CHECK(p_entry->get_offset() == Vector2(0.5, -2));
	CHECK(p_entry->get_tint() == Color(1, 0, 0, 1));
	CHECK(p_entry->get_code() == "007");
}

static void check_xml_settings(const Ref<Resource> &p_resource) {
	const Ref<XMLTestSettings> settings = p_resource;
	REQUIRE(settings.is_valid());
	CHECK(settings->get_title() == "Fish & <Chips> \"quoted\"\nsecond line\r\n");
	check_xml_entry(settings->get_main(), "Ann", 3);
	const TypedArray<XMLTestEntry> entries = settings->get_entries();
	REQUIRE(entries.size() == 2);
	check_xml_entry(entries[0], "Bob", 1);
	check_xml_entry(entries[1], " ", 2);

	const Dictionary scores = settings->get_scores();
	CHECK(scores.is_typed());
	CHECK(scores.size() == 2);
	CHECK(int(scores.get("ann", 0)) == 10);
	CHECK(int(scores.get("my key", 0)) == 20);

	const Dictionary extra = settings->get_extra();
	CHECK(extra.get("difficulty", Variant()) == Variant("hard"));
	CHECK(extra.get("lives", Variant()).get_type() == Variant::INT);
	CHECK(int(extra.get("lives", 0)) == 3);
	CHECK(extra.get("ratio", Variant()).get_type() == Variant::FLOAT);
	CHECK(extra.get("empty", Variant()).get_type() == Variant::DICTIONARY);
	CHECK(extra.has("none"));
	CHECK(extra.get("none", 0).get_type() == Variant::NIL);

	const PackedInt32Array numbers = settings->get_numbers();
	REQUIRE(numbers.size() == 2);
	CHECK(numbers[0] == 4);
	CHECK(numbers[1] == -7);
	const PackedVector2Array points = settings->get_points();
	REQUIRE(points.size() == 2);
	CHECK(points[1] == Vector2(-0.25, 8));

	// A Variant property keeps int, float, String, bool and null apart.
	const Array anything = settings->get_anything();
	REQUIRE(anything.size() == 6);
	CHECK(anything[0].get_type() == Variant::INT);
	CHECK(anything[1].get_type() == Variant::FLOAT);
	CHECK(anything[2].get_type() == Variant::STRING);
	CHECK(anything[2] == Variant("3"));
	CHECK(anything[3].get_type() == Variant::BOOL);
	CHECK(anything[4].get_type() == Variant::NIL);
	CHECK(anything[5].get_type() == Variant::ARRAY);
}

TEST_CASE("[ResourceTextSerializer][XML] Layout of the text") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	const Ref<XMLTestEntry> entry = make_xml_entry("Ann", 3);

	const String text = serializer->serialize(entry, ResourceTextSerializer::FORMAT_XML);
	CHECK(text ==
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<resource type=\"XMLTestEntry\" format_version=\"1\">\n"
			"  <name>Ann</name>\n"
			"  <level>3</level>\n"
			"  <speed>0.5</speed>\n"
			"  <enabled>true</enabled>\n"
			"  <mode>Angry</mode>\n"
			"  <offset kind=\"array\">0.5, -2.0</offset>\n"
			"  <tint>#ff0000ff</tint>\n"
			"  <code kind=\"string\">007</code>\n"
			"</resource>\n");
	check_xml_entry(serializer->deserialize(text, ResourceTextSerializer::FORMAT_XML), "Ann", 3);

	// An empty indent writes one line.
	Ref<SerializerOptions> line_options;
	line_options.instantiate();
	line_options->set_indent("");
	line_options->set_enums_as_names(false);
	const String line = serializer->serialize(entry, ResourceTextSerializer::FORMAT_XML, line_options);
	CHECK_FALSE(line.contains("\n"));
	CHECK(line.contains("<mode>5</mode>"));
	check_xml_entry(serializer->deserialize(line, ResourceTextSerializer::FORMAT_XML), "Ann", 3);
}

TEST_CASE("[ResourceTextSerializer][XML] Round trip keeps every stored property") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<XMLTestSettings> settings = make_xml_settings();

	const String text = serializer->serialize(settings, ResourceTextSerializer::FORMAT_XML);
	CHECK(text.contains("<resource type=\"XMLTestSettings\" format_version=\"1\">"));
	CHECK(text.contains("<title>Fish &amp; &lt;Chips&gt; \"quoted\"\nsecond line&#13;\n</title>"));
	CHECK(text.contains("<entries kind=\"array\">"));
	CHECK(text.contains("<name><![CDATA[ ]]></name>"));
	CHECK(text.contains("<entry key=\"my key\">20</entry>"));
	CHECK(text.contains("<ratio>1.0</ratio>"));
	CHECK(text.contains("<empty kind=\"dictionary\"/>"));
	CHECK(text.contains("<none kind=\"null\"/>"));
	CHECK(text.contains("<numbers kind=\"array\">4, -7</numbers>"));
	CHECK(text.contains("<item kind=\"string\">3</item>"));
	// Nested resources of the declared type have no type attribute.
	CHECK(text.count("type=\"") == 1);

	check_xml_settings(serializer->deserialize(text, ResourceTextSerializer::FORMAT_XML));
	Ref<SerializerOptions> indent_options;
	indent_options.instantiate();
	indent_options->set_indent("");
	check_xml_settings(serializer->deserialize(serializer->serialize(settings, ResourceTextSerializer::FORMAT_XML, indent_options), ResourceTextSerializer::FORMAT_XML));
	indent_options->set_indent("\t");
	check_xml_settings(serializer->deserialize(serializer->serialize(settings, ResourceTextSerializer::FORMAT_XML, indent_options), ResourceTextSerializer::FORMAT_XML));
}

TEST_CASE("[ResourceTextSerializer][XML] Through ResourceSaver and ResourceLoader") {
	const Ref<XMLTestSettings> settings = make_xml_settings();
	const String path = TestUtils::get_temp_path("text_formats_settings.xml");
	CHECK(ResourceSaver::save(Ref<Resource>(settings), path) == OK);
	CHECK(ResourceLoader::get_resource_type(path) == "XMLTestSettings");

	check_xml_settings(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE));
	check_xml_settings(ResourceLoader::load(path, "XMLTestSettings", ResourceFormatLoader::CACHE_MODE_IGNORE));
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceTextSerializer][XML] Other XML files are left to other loaders") {
	register_xml_test_classes();
	const String path = TestUtils::get_temp_path("text_formats_other.xml");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("<?xml version=\"1.0\"?>\n<class name=\"Node\" type=\"XMLTestEntry\">\n  <name>Not a resource</name>\n</class>\n");
	}
	CHECK(ResourceLoader::get_resource_type(path).is_empty());
	CHECK(ResourceTextSerializer::get_xml_marker_type(FileAccess::get_file_as_string(path)).is_empty());

	Ref<Resource> resource;
	String error;
	CHECK(ResourceTextSerializer::parse_xml(FileAccess::get_file_as_string(path), String(), resource, error) == ERR_FILE_UNRECOGNIZED);
	CHECK(resource.is_null());

	// <resource> with a type that is not a Resource class is not a marker either.
	CHECK(ResourceTextSerializer::get_xml_marker_type("<resource type=\"Node\"/>").is_empty());
	CHECK(ResourceTextSerializer::get_xml_marker_type("<resource type=\"XMLTestEntry\"/>") == "XMLTestEntry");

	// With a type hint any root element is read as that type.
	ERR_PRINT_OFF;
	const Ref<XMLTestEntry> entry = ResourceLoader::load(path, "XMLTestEntry", ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;
	REQUIRE(entry.is_valid());
	CHECK(entry->get_entry_name() == "Not a resource");
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceTextSerializer][XML] Hand-written file") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	register_xml_test_classes();
	const String text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<!-- Edited by hand. -->\n"
						"<resource type=\"XMLTestSettings\">\n"
						"\t<title><![CDATA[<b>bold</b>]]></title>\n"
						"\t<main type=\"XMLTestEntry\">\n"
						"\t\t<name>Zoe</name>\n"
						"\t\t<speed>2</speed>\n"
						"\t\t<enabled>yes</enabled>\n"
						"\t\t<offset>1, 2</offset>\n"
						"\t\t<tint>red</tint>\n"
						"\t</main>\n"
						"\t<entry key=\"numbers\" kind=\"array\"><item>1</item><item>2</item></entry>\n"
						"</resource>\n";
	const Ref<XMLTestSettings> settings = serializer->deserialize(text, ResourceTextSerializer::FORMAT_XML);
	REQUIRE(settings.is_valid());
	CHECK(settings->get_title() == "<b>bold</b>");
	const Ref<XMLTestEntry> main = settings->get_main();
	REQUIRE(main.is_valid());
	CHECK(main->get_entry_name() == "Zoe");
	CHECK(main->get_speed() == 2.0);
	CHECK(main->is_enabled());
	CHECK(main->get_offset() == Vector2(1, 2));
	CHECK(main->get_tint() == Color(1, 0, 0, 1));
	CHECK(main->get_level() == 1); // Missing: the default is kept.
	CHECK(settings->get_numbers().size() == 2);

	ERR_PRINT_OFF;
	CHECK(serializer->deserialize("<resource type=\"XMLTestEntry\"><name>Open</resource>", ResourceTextSerializer::FORMAT_XML).is_null());
	CHECK(serializer->deserialize("<resource type=\"XMLTestEntry\"><name>Open</name>", ResourceTextSerializer::FORMAT_XML).is_null());
	CHECK(serializer->deserialize("<other><name>x</name></other>", ResourceTextSerializer::FORMAT_XML).is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[ResourceTextSerializer] serialize() and deserialize() for all formats") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<XMLTestSettings> settings = make_xml_settings();

	const PackedInt32Array formats = serializer->get_available_formats();
	CHECK(formats.has(ResourceTextSerializer::FORMAT_JSON));
	CHECK(formats.has(ResourceTextSerializer::FORMAT_INI));
	CHECK(formats.has(ResourceTextSerializer::FORMAT_CSV));
	CHECK(formats.has(ResourceTextSerializer::FORMAT_XML));
	CHECK_FALSE(formats.has(ResourceTextSerializer::FORMAT_YAML));
	CHECK(formats.find(ResourceTextSerializer::FORMAT_JSON) < formats.find(ResourceTextSerializer::FORMAT_XML));
	CHECK(serializer->is_format_available(ResourceTextSerializer::FORMAT_XML));
	CHECK_FALSE(serializer->is_format_available(ResourceTextSerializer::FORMAT_YAML));
	CHECK_FALSE(serializer->is_format_available(ResourceTextSerializer::FORMAT_MAX));

	// Extensions.
	CHECK(serializer->get_format_extension(ResourceTextSerializer::FORMAT_JSON) == "json");
	CHECK(serializer->get_format_extension(ResourceTextSerializer::FORMAT_TOML) == "toml");
	CHECK(serializer->get_format_extension(ResourceTextSerializer::FORMAT_MAX).is_empty());
	CHECK(serializer->get_format_from_extension(".JSON") == ResourceTextSerializer::FORMAT_JSON);
	CHECK(serializer->get_format_from_extension("xml") == ResourceTextSerializer::FORMAT_XML);
	CHECK(serializer->get_format_from_extension("yml") == ResourceTextSerializer::FORMAT_YAML);
	CHECK(serializer->get_format_from_extension("tres") == ResourceTextSerializer::FORMAT_MAX);

	// The same text as the static C++ functions with the same options.
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(Ref<SerializerOptions>());
	String expected;
	String error;
	REQUIRE(ResourceTextSerializer::write_json(settings, options, expected, error) == OK);
	CHECK(serializer->serialize(settings, ResourceTextSerializer::FORMAT_JSON) == expected);
	REQUIRE(ResourceTextSerializer::write_ini(settings, options, expected, error) == OK);
	CHECK(serializer->serialize(settings, ResourceTextSerializer::FORMAT_INI) == expected);
	REQUIRE(ResourceTextSerializer::write_xml(settings, options, expected, error) == OK);
	CHECK(serializer->serialize(settings, ResourceTextSerializer::FORMAT_XML) == expected);

	// JSON has one number type, so only the XML round trip is checked in full here.
	CHECK(serializer->deserialize(serializer->serialize(settings, ResourceTextSerializer::FORMAT_JSON), ResourceTextSerializer::FORMAT_JSON).is_valid());
	CHECK(serializer->deserialize(serializer->serialize(settings, ResourceTextSerializer::FORMAT_INI), ResourceTextSerializer::FORMAT_INI).is_valid());

	ERR_PRINT_OFF;
	CHECK(serializer->serialize(settings, ResourceTextSerializer::FORMAT_YAML).is_empty());
	CHECK(serializer->deserialize("a: 1", ResourceTextSerializer::FORMAT_YAML).is_null());
	ERR_PRINT_ON;
}

static Error _write_test_format(const Ref<Resource> &p_resource, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error) {
	r_text = "test:" + ResourceTextSerializer::get_type_name(p_resource.ptr());
	return OK;
}

static Error _parse_test_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	r_resource = ResourceTextSerializer::instantiate_type(p_text.trim_prefix("test:"), r_error);
	return r_resource.is_valid() ? OK : ERR_PARSE_ERROR;
}

TEST_CASE("[ResourceTextSerializer] register_format() adds a format") {
	ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	const Ref<XMLTestEntry> entry = make_xml_entry("Ann", 3);

	// YAML has no implementation in core, so its slot is free for the test.
	REQUIRE_FALSE(serializer->is_format_available(ResourceTextSerializer::FORMAT_YAML));
	ResourceTextSerializer::register_format(ResourceTextSerializer::FORMAT_YAML, &_write_test_format, &_parse_test_format);
	CHECK(serializer->get_available_formats().has(ResourceTextSerializer::FORMAT_YAML));
	CHECK(serializer->serialize(entry, ResourceTextSerializer::FORMAT_YAML) == "test:XMLTestEntry");
	const Ref<XMLTestEntry> back = serializer->deserialize("test:XMLTestEntry", ResourceTextSerializer::FORMAT_YAML);
	CHECK(back.is_valid());

	ResourceTextSerializer::unregister_format(ResourceTextSerializer::FORMAT_YAML);
	CHECK_FALSE(serializer->is_format_available(ResourceTextSerializer::FORMAT_YAML));
	CHECK(serializer->is_format_available(ResourceTextSerializer::FORMAT_JSON));
}

} // namespace TestResourceTextSerializerXML
