/**************************************************************************/
/*  test_text_format_classes.cpp                                          */
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

TEST_FORCE_LINK(test_text_format_classes)

#include "core/io/csv_format.h"
#include "core/io/ini_format.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/serializer_options.h"
#include "core/io/xml_format.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"

namespace TestTextFormatClasses {

class FormatClassItem : public Resource {
	GDCLASS(FormatClassItem, Resource);

	String title;
	int64_t level = 1;
	Vector2 offset;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_title", "value"), &FormatClassItem::set_title);
		ClassDB::bind_method(D_METHOD("get_title"), &FormatClassItem::get_title);
		ClassDB::bind_method(D_METHOD("set_level", "value"), &FormatClassItem::set_level);
		ClassDB::bind_method(D_METHOD("get_level"), &FormatClassItem::get_level);
		ClassDB::bind_method(D_METHOD("set_offset", "value"), &FormatClassItem::set_offset);
		ClassDB::bind_method(D_METHOD("get_offset"), &FormatClassItem::get_offset);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "set_level", "get_level");
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	}

public:
	void set_title(const String &p_value) { title = p_value; }
	String get_title() const { return title; }
	void set_level(int64_t p_value) { level = p_value; }
	int64_t get_level() const { return level; }
	void set_offset(const Vector2 &p_value) { offset = p_value; }
	Vector2 get_offset() const { return offset; }
};

class FormatClassList : public Resource {
	GDCLASS(FormatClassList, Resource);

	TypedArray<FormatClassItem> items;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_items", "value"), &FormatClassList::set_items);
		ClassDB::bind_method(D_METHOD("get_items"), &FormatClassList::get_items);

		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "items", PROPERTY_HINT_ARRAY_TYPE, "FormatClassItem"), "set_items", "get_items");
	}

public:
	void set_items(const TypedArray<FormatClassItem> &p_value) { items = p_value; }
	TypedArray<FormatClassItem> get_items() const { return items; }
};

static void register_format_class_test_types() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(FormatClassItem);
		GDREGISTER_CLASS(FormatClassList);
		registered = true;
	}
}

static Ref<FormatClassItem> make_item(const String &p_title, int64_t p_level) {
	register_format_class_test_types();
	Ref<FormatClassItem> item;
	item.instantiate();
	item->set_title(p_title);
	item->set_level(p_level);
	item->set_offset(Vector2(0.5, -2));
	return item;
}

static Ref<FormatClassList> make_list() {
	Ref<FormatClassList> list;
	list.instantiate();
	TypedArray<FormatClassItem> items;
	items.push_back(make_item("Ann", 3));
	items.push_back(make_item("Bob, \"the\" 2nd", 7));
	list->set_items(items);
	return list;
}

static void check_item(const Ref<FormatClassItem> &p_item, const String &p_title, int64_t p_level) {
	REQUIRE(p_item.is_valid());
	CHECK(p_item->get_title() == p_title);
	CHECK(p_item->get_level() == p_level);
	CHECK(p_item->get_offset().is_equal_approx(Vector2(0.5, -2)));
}

static Ref<SerializerOptions> make_options() {
	Ref<SerializerOptions> options;
	options.instantiate();
	return options;
}

TEST_CASE("[XML] Parse elements, attributes, mixed content and CDATA") {
	Ref<XML> xml;
	xml.instantiate();
	const String text = "<?xml version=\"1.0\"?>\n<!-- comment -->\n<book id=\"7\" lang=\"en\">\n\t<title>Fish &amp; Chips</title>\n\t<p>Hello <b>big</b> world</p>\n\t<code><![CDATA[a < b]]></code>\n\t<empty/>\n</book>\n";
	REQUIRE(xml->parse(text) == OK);
	const Dictionary root = xml->get_data();
	CHECK(root["name"] == "book");
	const Dictionary attributes = root["attributes"];
	CHECK(attributes["id"] == "7");
	CHECK(attributes["lang"] == "en");
	const Array children = root["children"];
	REQUIRE(children.size() == 4); // Indentation between elements is dropped.

	const Dictionary title = children[0];
	CHECK(title["text"] == "Fish & Chips");

	const Dictionary paragraph = children[1];
	const Array mixed = paragraph["children"];
	REQUIRE(mixed.size() == 3);
	CHECK(mixed[0] == "Hello ");
	CHECK(Dictionary(mixed[1])["name"] == "b");
	CHECK(mixed[2] == " world");
	CHECK(paragraph["text"] == "Hello  world");

	const Dictionary code = children[2];
	CHECK(code["text"] == "a < b");

	const Dictionary empty = children[3];
	CHECK(Array(empty["children"]).is_empty());
	CHECK(Dictionary(empty["attributes"]).is_empty());
}

TEST_CASE("[XML] Stringify and parse again gives the same data") {
	const String text = "<root a=\"1 &amp; 2\" b=\"line&#10;break\"><p>Hello <b>big</b> world</p><list><item>1</item><item>2</item></list><space><![CDATA[  ]]></space></root>";
	const Dictionary data = XML::parse_string(text);
	REQUIRE_FALSE(data.is_empty());
	for (const String &indent : { String(), String("  ") }) {
		Ref<SerializerOptions> options = make_options();
		options->set_indent(indent);
		const String written = XML::stringify(data, options);
		CHECK(written.begins_with("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
		CHECK(XML::parse_string(written) == data);
	}
	// Mixed content is written on one line, so no indentation becomes part of the text.
	Ref<SerializerOptions> options = make_options();
	options->set_xml_declaration(false);
	CHECK(XML::stringify(data, options).contains("<p>Hello <b>big</b> world</p>"));
	CHECK(XML::stringify(data, options).begins_with("<root"));
}

TEST_CASE("[XML] Stringify uses text without children and sorts attributes") {
	Dictionary element;
	element["name"] = "note";
	Dictionary attributes;
	attributes["z"] = 1;
	attributes["a"] = "x";
	element["attributes"] = attributes;
	element["text"] = "Hi <you>";
	Ref<SerializerOptions> options = make_options();
	options->set_xml_declaration(false);
	options->set_indent("");
	options->set_sort_keys(true);
	CHECK(XML::stringify(element, options) == "<note a=\"x\" z=\"1\">Hi &lt;you&gt;</note>");
}

TEST_CASE("[XML] Errors with line numbers") {
	Ref<XML> xml;
	xml.instantiate();
	CHECK(xml->parse("<a>\n<b>\n</a>") == ERR_PARSE_ERROR);
	CHECK(xml->get_error_line() == 3);
	CHECK(xml->get_error_message().contains("</a>"));

	CHECK(xml->parse("<a>\n<b/>") == ERR_PARSE_ERROR);
	CHECK(xml->get_error_line() == 1);
	CHECK(xml->parse("<a/>\n<b/>") == ERR_PARSE_ERROR);
	CHECK(xml->get_error_line() == 2);
	CHECK(xml->parse("") == ERR_PARSE_ERROR);
	CHECK(xml->get_data().is_empty());

	Dictionary invalid;
	invalid["name"] = "1st";
	ERR_PRINT_OFF;
	CHECK(XML::stringify(invalid).is_empty());
	CHECK(XML::parse_string("<a>").is_empty());
	ERR_PRINT_ON;
}

TEST_CASE("[INI] Parse sections, keys before the first section and typed values") {
	Ref<INI> ini;
	ini.instantiate();
	const String text = "; comment\ntop=1\n\n[player]\nname=\"Ann\"\npos=Vector2(1, 2)\nplain=just text\nflags=[1, 2]\n\n[player]\nlevel=3\n";
	REQUIRE(ini->parse(text) == OK);
	const Dictionary data = ini->get_data();
	CHECK(Dictionary(data[""])["top"] == Variant(1));
	const Dictionary player = data["player"];
	CHECK(player["name"] == "Ann");
	CHECK(player["pos"] == Variant(Vector2(1, 2)));
	CHECK(player["plain"] == "just text");
	CHECK(player["level"] == Variant(3)); // Repeated sections are merged.
	CHECK(player.size() == 5);
}

TEST_CASE("[INI] Stringify and parse again, typed and plain") {
	Dictionary data;
	Dictionary global;
	global["version"] = 2;
	data[""] = global;
	Dictionary audio;
	audio["volume"] = 0.5;
	audio["device"] = "Speakers (USB)";
	audio["mute"] = false;
	data["audio"] = audio;
	Dictionary video;
	video["size"] = Vector2i(1280, 720);
	data["video"] = video;

	const String typed = INI::stringify(data, make_options());
	CHECK(typed.begins_with("version=2\n"));
	CHECK(typed.contains("[audio]\n"));
	CHECK(typed.contains("device=\"Speakers (USB)\""));
	CHECK(INI::parse_string(typed) == data);

	Ref<SerializerOptions> plain = make_options();
	plain->set_ini_plain_values(true);
	const String plain_text = INI::stringify(data, plain);
	CHECK(plain_text.contains("mute=false"));
	const Dictionary plain_data = INI::parse_string(plain_text);
	CHECK(Dictionary(plain_data["audio"])["device"] == "Speakers (USB)");
	CHECK(Dictionary(plain_data["audio"])["volume"] == Variant(0.5));
}

TEST_CASE("[INI] Errors with line numbers") {
	Ref<INI> ini;
	ini.instantiate();
	CHECK(ini->parse("a=1\n[broken\n") == ERR_PARSE_ERROR);
	CHECK(ini->get_error_line() == 2);
	CHECK(ini->parse("[s]\nno value here\n") == ERR_PARSE_ERROR);
	CHECK(ini->get_error_line() == 2);
	CHECK_FALSE(ini->get_error_message().is_empty());

	Dictionary invalid;
	invalid["section"] = 5;
	ERR_PRINT_OFF;
	CHECK(INI::stringify(invalid).is_empty());
	ERR_PRINT_ON;
}

TEST_CASE("[CSV] Parse with and without headers") {
	const String text = "name,note\nAnn,\"a, b\"\r\nBob,\"line\nbreak \"\"quoted\"\"\"\nCleo\n";
	Ref<SerializerOptions> headers = make_options();
	headers->set_csv_headers(true);
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse(text, headers) == OK);
	const Array records = csv->get_data();
	REQUIRE(records.size() == 3);
	CHECK(Dictionary(records[0])["note"] == "a, b");
	CHECK(Dictionary(records[1])["note"] == "line\nbreak \"quoted\"");
	CHECK(Dictionary(records[2])["name"] == "Cleo");
	CHECK(Dictionary(records[2])["note"] == ""); // Missing cells are empty.

	Ref<SerializerOptions> rows_only = make_options();
	rows_only->set_csv_headers(false);
	rows_only->set_csv_delimiter(";");
	const Array rows = CSV::parse_string("007;x\n1;2\n", rows_only);
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].get_type() == Variant::PACKED_STRING_ARRAY);
	CHECK(PackedStringArray(rows[0])[0] == "007"); // Cells stay strings.
	CHECK(PackedStringArray(rows[1])[1] == "2");
}

TEST_CASE("[CSV] Stringify and parse again") {
	Array records;
	Dictionary first;
	first["name"] = "Ann";
	first["note"] = "a, \"b\"";
	records.push_back(first);
	Dictionary second;
	second["name"] = "Bob";
	second["level"] = 3;
	records.push_back(second);

	Ref<SerializerOptions> headers = make_options();
	headers->set_csv_headers(true);
	const String text = CSV::stringify(records, headers);
	CHECK(text.begins_with("name,note,level\n"));
	CHECK(text.contains("Ann,\"a, \"\"b\"\"\",\n"));
	const Array back = CSV::parse_string(text, headers);
	REQUIRE(back.size() == 2);
	CHECK(Dictionary(back[0])["note"] == "a, \"b\"");
	CHECK(Dictionary(back[1])["level"] == "3");

	Array rows;
	rows.push_back(PackedStringArray({ "a", "b c", "" }));
	rows.push_back(PackedStringArray({ "1", "2", "3" }));
	Ref<SerializerOptions> plain = make_options();
	plain->set_csv_headers(false);
	plain->set_csv_delimiter("\t");
	const String tabbed = CSV::stringify(rows, plain);
	CHECK(tabbed == "a\tb c\t\n1\t2\t3\n");
	CHECK(CSV::parse_string(tabbed, plain) == rows);
}

TEST_CASE("[CSV] Errors with line numbers") {
	Ref<SerializerOptions> headers = make_options();
	headers->set_csv_headers(true);
	Ref<CSV> csv;
	csv.instantiate();
	CHECK(csv->parse("a,b\n1,2\n3,\"open\n", headers) == ERR_PARSE_ERROR);
	CHECK(csv->get_error_line() == 3);
	CHECK(csv->parse("a,b\n1,2,3\n", headers) == ERR_PARSE_ERROR);
	CHECK(csv->get_error_line() == 2);

	Array mixed;
	mixed.push_back(Dictionary());
	mixed.push_back(5);
	ERR_PRINT_OFF;
	CHECK(CSV::stringify(mixed).is_empty());
	ERR_PRINT_ON;
}

TEST_CASE("[TextFormatClasses] serialize() and deserialize() match ResourceTextSerializer") {
	register_format_class_test_types();
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	REQUIRE(serializer != nullptr);
	const Ref<FormatClassItem> item = make_item("Ann & <Bob>", 3);
	const Ref<FormatClassList> list = make_list();

	CHECK(JSON::serialize(item) == serializer->serialize(item, ResourceTextSerializer::FORMAT_JSON));
	CHECK(XML::serialize(item) == serializer->serialize(item, ResourceTextSerializer::FORMAT_XML));
	CHECK(INI::serialize(item) == serializer->serialize(item, ResourceTextSerializer::FORMAT_INI));
	CHECK(CSV::serialize(list) == serializer->serialize(list, ResourceTextSerializer::FORMAT_CSV));
	CHECK_FALSE(XML::serialize(item).is_empty());

	check_item(JSON::deserialize(JSON::serialize(item)), "Ann & <Bob>", 3);
	check_item(XML::deserialize(XML::serialize(item)), "Ann & <Bob>", 3);
	check_item(INI::deserialize(INI::serialize(item)), "Ann & <Bob>", 3);
	check_item(XML::deserialize(XML::serialize(item), "FormatClassItem"), "Ann & <Bob>", 3);

	const Ref<FormatClassList> loaded = CSV::deserialize(CSV::serialize(list), "FormatClassList");
	REQUIRE(loaded.is_valid());
	const TypedArray<FormatClassItem> items = loaded->get_items();
	REQUIRE(items.size() == 2);
	check_item(items[0], "Ann", 3);
	check_item(items[1], "Bob, \"the\" 2nd", 7);

	// The options are passed through.
	Ref<SerializerOptions> options = make_options();
	options->set_indent("");
	CHECK(JSON::serialize(item, options) == serializer->serialize(item, ResourceTextSerializer::FORMAT_JSON, options));
	CHECK_FALSE(JSON::serialize(item, options).contains("\n"));
}

} // namespace TestTextFormatClasses
