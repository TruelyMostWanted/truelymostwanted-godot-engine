/**************************************************************************/
/*  test_text_data_format.cpp                                             */
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

TEST_FORCE_LINK(test_text_data_format)

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/csv_format.h"
#include "core/io/file_access.h"
#include "core/io/ini_format.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/io/xml_format.h"
#include "core/object/class_db.h"
#include "tests/test_utils.h"

namespace TestTextDataFormat {

class TextDataEntry : public Resource {
	GDCLASS(TextDataEntry, Resource);

	String title;
	int64_t level = 1;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_title", "value"), &TextDataEntry::set_title);
		ClassDB::bind_method(D_METHOD("get_title"), &TextDataEntry::get_title);
		ClassDB::bind_method(D_METHOD("set_level", "value"), &TextDataEntry::set_level);
		ClassDB::bind_method(D_METHOD("get_level"), &TextDataEntry::get_level);
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "level"), "set_level", "get_level");
	}

public:
	void set_title(const String &p_value) { title = p_value; }
	String get_title() const { return title; }
	void set_level(int64_t p_value) { level = p_value; }
	int64_t get_level() const { return level; }
};

static void register_text_data_test_types() {
	static bool registered = false;
	if (!registered) {
		GDREGISTER_CLASS(TextDataEntry);
		registered = true;
	}
}

static void write_file(const String &p_path, const String &p_text) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(p_text);
}

TEST_CASE("[TextDataFormat] Base API through INI") {
	Ref<INI> ini;
	ini.instantiate();
	CHECK(ini->get_data().get_type() == Variant::DICTIONARY);
	CHECK(ini->get_recognized_extensions() == PackedStringArray({ "ini", "cfg" }));

	const String text = "; comment\n[player]\nname=\"Ann\"\nlevel=3\n";
	REQUIRE(ini->parse(text, true) == OK);
	CHECK(ini->get_parsed_text() == text);
	CHECK(ini->get_value("player", "name") == "Ann");
	CHECK(int(ini->get_value("player", "level")) == 3);

	// Writing data drops the kept text, so saving writes the new data.
	ini->set_value("player", "level", 4);
	CHECK(ini->get_parsed_text().is_empty());
	CHECK(ini->encode_to_text().contains("level=4"));
	CHECK_FALSE(ini->encode_to_text().contains("comment"));

	// Without keep_text, nothing is kept; errors have a line.
	CHECK(ini->parse("[broken\n") == ERR_PARSE_ERROR);
	CHECK(ini->get_error_line() == 1);
	CHECK(ini->get_error_column() == 0);
	CHECK(Dictionary(ini->get_data()).is_empty());

	// Setting valid data clears the parse error.
	ini->set_data(Dictionary());
	CHECK(ini->get_error_message().is_empty());
	CHECK(ini->get_error_line() == 0);

	ini->set_value("a", "b", 1);
	ini->clear();
	CHECK(Dictionary(ini->get_data()).is_empty());

	ERR_PRINT_OFF;
	ini->set_data(5); // Wrong type: rejected.
	ERR_PRINT_ON;
	CHECK(ini->get_data().get_type() == Variant::DICTIONARY);
}

TEST_CASE("[INI] ConfigFile API and ConfigFile conversion") {
	Ref<INI> ini;
	ini.instantiate();
	ini->set_value("", "version", 2);
	ini->set_value("video", "size", Vector2i(1280, 720));
	ini->set_value("video", "vsync", true);
	CHECK(ini->has_section("video"));
	CHECK(ini->has_section_key("video", "vsync"));
	CHECK(ini->get_sections() == PackedStringArray({ "", "video" }));
	CHECK(ini->get_section_keys("video") == PackedStringArray({ "size", "vsync" }));
	CHECK(ini->get_value("video", "missing", 7) == Variant(7));

	ini->set_value("video", "vsync", Variant()); // Null erases the key.
	CHECK_FALSE(ini->has_section_key("video", "vsync"));
	ini->erase_section_key("video", "size");
	CHECK_FALSE(ini->has_section("video")); // Like ConfigFile: an empty section is erased.
	ini->erase_section("");
	CHECK(Dictionary(ini->get_data()).is_empty());
}

TEST_CASE("[INI] to_config_file() and from_config_file()") {
	Ref<INI> ini;
	ini.instantiate();
	ini->set_value("player", "name", "Ann");
	ini->set_value("player", "pos", Vector2(1, 2));

	const Ref<ConfigFile> config = ini->to_config_file();
	REQUIRE(config.is_valid());
	CHECK(config->get_value("player", "name") == "Ann");
	CHECK(config->get_value("player", "pos") == Variant(Vector2(1, 2)));

	config->set_value("audio", "volume", 0.5);
	Ref<INI> back;
	back.instantiate();
	REQUIRE(back->from_config_file(config) == OK);
	CHECK(back->get_value("player", "name") == "Ann");
	CHECK(double(back->get_value("audio", "volume")) == doctest::Approx(0.5));

	// The Godot syntax of INI and ConfigFile is the same.
	Ref<ConfigFile> parsed;
	parsed.instantiate();
	REQUIRE(parsed->parse(back->encode_to_text()) == OK);
	CHECK(parsed->get_value("player", "pos") == Variant(Vector2(1, 2)));
}

TEST_CASE("[SerializerOptions] ini_value_style") {
	Ref<SerializerOptions> options;
	options.instantiate();
	CHECK(options->get_ini_value_style() == SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT);
	Ref<INI> ini;
	ini.instantiate();
	ini->set_value("s", "name", "Ann");
	ini->set_options(options);
	CHECK(ini->encode_to_text().contains("name=\"Ann\""));
	options->set_ini_value_style(SerializerOptions::INI_VALUE_STYLE_PLAIN);
	CHECK(ini->encode_to_text().contains("name=Ann\n"));
}

TEST_CASE("[ResourceTextSerializer] Project setting ini_value_style and the old bool ini_plain_values") {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	REQUIRE(settings != nullptr);
	const String style_key = "filesystem/resources/text_formats/ini_value_style";
	const String legacy_key = "filesystem/resources/text_formats/ini_plain_values";
	const Variant previous = settings->get_setting(style_key, int(SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT));

	settings->set_setting(style_key, int(SerializerOptions::INI_VALUE_STYLE_PLAIN));
	CHECK(ResourceTextSerializer::get_default_options().ini_plain_values);
	settings->set_setting(style_key, int(SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT));
	CHECK_FALSE(ResourceTextSerializer::get_default_options().ini_plain_values);
	// An old project.godot with the bool is still read while ini_value_style is the default.
	settings->set_setting(legacy_key, true);
	CHECK(ResourceTextSerializer::get_default_options().ini_plain_values);

	settings->set_setting(legacy_key, Variant());
	settings->set_setting(style_key, previous);
}

TEST_CASE("[CSV] Rows, cells and columns by index") {
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse("id,name,team\n1,Ann,red\n2,Bob,blue\n3,Cleo,red\n") == OK);
	CHECK(csv->get_header() == PackedStringArray({ "id", "name", "team" }));
	CHECK(csv->get_row_count() == 3);
	CHECK(csv->get_column_count() == 3);
	const int team = csv->find_column("team");
	CHECK(team == 2);
	CHECK(csv->find_column("missing") == -1);
	CHECK(csv->find_rows(team, "red") == PackedInt32Array({ 0, 2 }));
	CHECK(csv->get_column(1) == PackedStringArray({ "Ann", "Bob", "Cleo" }));
	CHECK(csv->get_row(1) == PackedStringArray({ "2", "Bob", "blue" }));
	CHECK(csv->get_row_as_dictionary(0)["name"] == "Ann");

	csv->set_cell(0, 4, "x"); // Extends the row.
	CHECK(csv->get_row(0).size() == 5);
	CHECK(csv->get_cell(0, 3) == "");
	csv->append_row(PackedStringArray({ "4", "Dan", "blue" }));
	csv->insert_row(0, PackedStringArray({ "0", "Zed", "green" }));
	CHECK(csv->get_cell(0, 1) == "Zed");
	csv->remove_row(0);
	csv->set_row(0, PackedStringArray({ "1", "Ann", "red" }));
	CHECK(csv->get_row_count() == 4);
	CHECK(csv->encode_to_text() == "id,name,team\n1,Ann,red\n2,Bob,blue\n3,Cleo,red\n4,Dan,blue\n");

	// Without headers, every line is a row.
	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_csv_headers(false);
	options->set_csv_delimiter(";");
	csv->set_options(options);
	REQUIRE(csv->parse("a;b\n1;2\n") == OK);
	CHECK(csv->get_header().is_empty());
	CHECK(csv->get_row_count() == 2);
	CHECK(csv->encode_to_text() == "a;b\n1;2\n");
}

TEST_CASE("[XML] XMLElement view over the stored tree") {
	Ref<XML> xml;
	xml.instantiate();
	REQUIRE(xml->parse("<items version=\"2\"><item id=\"a\">Sword</item><group><item id=\"b\">Shield</item></group></items>", true) == OK);
	const Ref<XMLElement> root = xml->get_root();
	REQUIRE(root.is_valid());
	CHECK(xml->get_root() == root); // Built once.
	CHECK(root->get_name() == "items");
	CHECK(root->get_attribute("version") == "2");
	CHECK(root->get_attribute("missing", "none") == "none");
	CHECK(root->get_children().size() == 2);
	CHECK(root->get_children_by_name("item").size() == 1);
	const Ref<XMLElement> shield = xml->find_first("group/item");
	REQUIRE(shield.is_valid());
	CHECK(shield->get_text() == "Shield");
	CHECK(xml->find_all("item").size() == 1);
	CHECK(xml->find_first("nothing/here").is_null());

	// Writes go into the tree and drop the kept text.
	shield->set_attribute("id", "c");
	CHECK(xml->get_parsed_text().is_empty());
	Ref<XMLElement> extra;
	extra.instantiate();
	extra->set_name("item");
	extra->set_text("Bow");
	root->add_child(extra);
	CHECK(xml->find_all("item").size() == 2);
	const String text = xml->encode_to_text();
	CHECK(text.contains("id=\"c\""));
	CHECK(text.contains("<item>Bow</item>"));
	root->remove_child(extra);
	CHECK(xml->find_all("item").size() == 1);

	Ref<XML> created;
	created.instantiate();
	const Ref<XMLElement> config = created->create_root("config");
	config->set_attribute("name", "test");
	CHECK(created->encode_to_text().contains("<config name=\"test\""));
}

TEST_CASE("[TextDataFormat] Plain files load as format resources, marked files as their type") {
	register_text_data_test_types();

	const String ini_path = TestUtils::get_temp_path("text_data_plain.ini");
	write_file(ini_path, "; settings\n[video]\nvsync=true\n");
	CHECK(ResourceLoader::get_resource_type(ini_path) == "INI");
	const Ref<INI> ini = ResourceLoader::load(ini_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(ini.is_valid());
	CHECK(bool(ini->get_value("video", "vsync")) == true);
	// Not the editor: the text is not kept.
	CHECK(ini->get_parsed_text().is_empty());

	// Saving writes plain INI, not a type-marked file.
	ini->set_value("video", "fps", 60);
	REQUIRE(ResourceSaver::save(ini, ini_path) == OK);
	const String saved = FileAccess::get_file_as_string(ini_path);
	CHECK_FALSE(saved.contains("$type"));
	CHECK(saved.contains("fps=60"));

	const String csv_path = TestUtils::get_temp_path("text_data_plain.csv");
	write_file(csv_path, "name,level\nAnn,3\n");
	const Ref<CSV> csv = ResourceLoader::load(csv_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(csv.is_valid());
	CHECK(csv->get_cell(0, 1) == "3");
	csv->append_row(PackedStringArray({ "Bob", "4" }));
	REQUIRE(ResourceSaver::save(csv, csv_path) == OK);
	CHECK(FileAccess::get_file_as_string(csv_path) == "name,level\nAnn,3\nBob,4\n");

	const String xml_path = TestUtils::get_temp_path("text_data_plain.xml");
	write_file(xml_path, "<list><entry/></list>");
	CHECK(ResourceLoader::get_resource_type(xml_path) == "XML");
	const Ref<XML> xml = ResourceLoader::load(xml_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(xml.is_valid());
	CHECK(xml->get_root()->get_name() == "list");
	REQUIRE(ResourceSaver::save(xml, xml_path) == OK);
	CHECK_FALSE(FileAccess::get_file_as_string(xml_path).contains("type="));

	// A type-marked file keeps loading as its resource type.
	Ref<TextDataEntry> entry;
	entry.instantiate();
	entry->set_title("Ann");
	const String marked_path = TestUtils::get_temp_path("text_data_marked.ini");
	REQUIRE(ResourceSaver::save(entry, marked_path) == OK);
	CHECK(FileAccess::get_file_as_string(marked_path).contains("$type"));
	CHECK(ResourceLoader::get_resource_type(marked_path) == "TextDataEntry");
	const Ref<TextDataEntry> loaded = ResourceLoader::load(marked_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_title() == "Ann");

	// A plain JSON file still loads as JSON.
	const String json_path = TestUtils::get_temp_path("text_data_plain.json");
	write_file(json_path, "{\"a\": 1}");
	CHECK(ResourceLoader::get_resource_type(json_path) == "JSON");
	const Ref<JSON> json = ResourceLoader::load(json_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(json.is_valid());
}

TEST_CASE("[TextDataFormat] Type check reads only the head of big files") {
	register_text_data_test_types();

	// A big plain file: the type comes from the extension, the marker search stops after 4 KB.
	String big = "[data]\n";
	for (int i = 0; i < 20000; i++) {
		big += vformat("key_%d=\"value with $type in the text\"\n", i);
	}
	const String big_path = TestUtils::get_temp_path("text_data_big.ini");
	write_file(big_path, big);
	CHECK(ResourceLoader::get_resource_type(big_path) == "INI");
	const Ref<INI> ini = ResourceLoader::load(big_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(ini.is_valid());
	CHECK(ini->get_section_keys("data").size() == 20000);

	// A big marked file: the marker is at the start, so it is found in the head.
	String marked = "[root]\n$type=\"TextDataEntry\"\ntitle=\"" + String("x").repeat(20000) + "\"\n";
	const String marked_path = TestUtils::get_temp_path("text_data_big_marked.ini");
	write_file(marked_path, marked);
	CHECK(ResourceLoader::get_resource_type(marked_path) == "TextDataEntry");
	const Ref<TextDataEntry> entry = ResourceLoader::load(marked_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(entry.is_valid());
	CHECK(entry->get_title().length() == 20000);
}

TEST_CASE("[CSV] Blocks of cells for copy, paste and undo") {
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse("name,hp,speed\nknight,100,4\narcher,60\n") == OK);

	// Missing cells read as empty, rows past the end are left out.
	const Array block = csv->get_cells(0, 1, 5, 2);
	REQUIRE(block.size() == 2);
	CHECK(PackedStringArray(block[0]) == PackedStringArray({ "100", "4" }));
	CHECK(PackedStringArray(block[1]) == PackedStringArray({ "60", "" }));

	// A pasted tab-separated block overwrites cells and appends rows.
	Ref<SerializerOptions> tsv_options;
	tsv_options.instantiate();
	tsv_options->set_csv_delimiter("\t");
	tsv_options->set_csv_headers(false);
	const Array tsv = CSV::parse_string("70\t5\r\n50\t3\r\n", tsv_options);
	csv->set_cells(1, 1, tsv);
	CHECK(csv->get_row_count() == 3);
	CHECK(csv->get_row(1) == PackedStringArray({ "archer", "70", "5" }));
	CHECK(csv->get_row(2) == PackedStringArray({ "", "50", "3" }));

	// Writing the old block back and removing the appended row restores the data (the table editor's undo).
	Array old_block;
	old_block.push_back(PackedStringArray({ "60", "" }));
	csv->set_cells(1, 1, old_block);
	csv->remove_row(2);
	CHECK(csv->get_cell(1, 1) == "60");
	CHECK(csv->get_row_count() == 2);

	ERR_PRINT_OFF;
	csv->set_cells(5, 0, tsv);
	ERR_PRINT_ON;
	CHECK(csv->get_row_count() == 2);
}

TEST_CASE("[TextDataFormat] Instance load() and save()") {
	const String path = TestUtils::get_temp_path("text_data_instance.ini");
	Ref<INI> ini;
	ini.instantiate();
	ini->set_value("a", "b", "c");
	REQUIRE(ini->save(path) == OK);
	Ref<INI> other;
	other.instantiate();
	REQUIRE(other->load(path) == OK);
	CHECK(other->get_value("a", "b") == "c");
	CHECK(other->get_path().is_empty()); // Not a resource-cache load.
	ERR_PRINT_OFF;
	CHECK(other->load(TestUtils::get_temp_path("text_data_missing.ini")) != OK);
	ERR_PRINT_ON;
}

} // namespace TestTextDataFormat
