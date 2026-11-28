/**************************************************************************/
/*  test_data_schema.cpp                                                  */
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

TEST_FORCE_LINK(test_data_schema)

#include "core/io/csv_format.h"
#include "core/io/data_schema.h"
#include "core/io/ini_format.h"
#include "core/io/json.h"
#include "core/io/xml_format.h"
#include "core/os/os.h"

namespace TestDataSchema {

static Ref<DataField> make_field(const StringName &p_name, Variant::Type p_type) {
	Ref<DataField> field;
	field.instantiate();
	field->set_field_name(p_name);
	field->set_value_type(p_type);
	return field;
}

// Items schema: id (int, unique, required), name (String, not empty), rarity (enum), hp (0..100), code (pattern).
static Ref<DataSchema> make_item_schema() {
	Ref<DataSchema> schema;
	schema.instantiate();
	Ref<DataField> id = make_field("id", Variant::INT);
	id->set_required(true);
	id->set_unique(true);
	schema->add_field(id);
	Ref<DataField> item_name = make_field("name", Variant::STRING);
	item_name->set_allow_empty(false);
	schema->add_field(item_name);
	Ref<DataField> rarity = make_field("rarity", Variant::STRING);
	rarity->set_enum_values(PackedStringArray{ "common", "rare" });
	schema->add_field(rarity);
	Ref<DataField> hp = make_field("hp", Variant::INT);
	hp->set_range(0, 100);
	hp->set_default_value(10);
	schema->add_field(hp);
	Ref<DataField> code = make_field("code", Variant::STRING);
	code->set_pattern("[A-Z]{2}[0-9]+");
	schema->add_field(code);
	return schema;
}

static bool has_issue(const Ref<DataValidation> &p_validation, DataValidation::IssueKind p_kind, const StringName &p_field, int p_row = DataValidation::POSITION_NONE) {
	for (int i = 0; i < p_validation->get_issue_count(); i++) {
		if (p_validation->get_issue_kind(i) == p_kind && p_validation->get_issue_field(i) == p_field && p_validation->get_issue_row(i) == p_row) {
			return true;
		}
	}
	return false;
}

TEST_CASE("[DataSchema] DataField hint parsing and text conversion") {
	Ref<DataField> field = make_field("hp", Variant::INT);
	field->set_range(0, 100);
	CHECK(field->get_hint() == PROPERTY_HINT_RANGE);
	String message;
	CHECK(field->check_hint(50, message) == DataValidation::ISSUE_KIND_MAX);
	CHECK(field->check_hint(101, message) == DataValidation::ISSUE_KIND_OUT_OF_RANGE);
	field->set_hint_string("0,100,1,or_greater");
	CHECK(field->check_hint(101, message) == DataValidation::ISSUE_KIND_MAX);

	Ref<DataField> level = make_field("level", Variant::INT);
	level->set_hint(PROPERTY_HINT_ENUM);
	level->set_hint_string("Low,High:5,Top");
	CHECK(level->check_hint(0, message) == DataValidation::ISSUE_KIND_MAX);
	CHECK(level->check_hint(6, message) == DataValidation::ISSUE_KIND_MAX);
	CHECK(level->check_hint(1, message) == DataValidation::ISSUE_KIND_NOT_IN_ENUM);

	Variant value;
	CHECK(field->convert_text(" 42 ", value));
	CHECK(value == Variant(42));
	CHECK_FALSE(field->convert_text("4x", value));
	CHECK(DataField::text_to_type(Variant::VECTOR2, "Vector2(1, 2)", value));
	CHECK(value == Variant(Vector2(1, 2)));
	CHECK(DataField::text_to_type(Variant::BOOL, "TRUE", value));
	CHECK(value == Variant(true));
	// JSON numbers are floats: a float without fraction is a valid int.
	CHECK(DataField::coerce(Variant::INT, 3.0, value));
	CHECK(value.get_type() == Variant::INT);
	CHECK_FALSE(DataField::coerce(Variant::INT, 3.5, value));
	CHECK(DataField::value_to_text(Vector2(1, 2)) == "Vector2(1, 2)");
}

TEST_CASE("[DataSchema] create_from_dictionary() and field access") {
	Dictionary nested;
	nested["x"] = Variant::INT;
	Dictionary shape;
	shape["name"] = Variant::STRING;
	shape["pos"] = nested;
	shape["tags"] = Array{ Variant::STRING };
	Ref<DataSchema> schema = DataSchema::create_from_dictionary(shape);
	REQUIRE(schema->get_fields().size() == 3);
	CHECK(schema->find_field("pos") == 1);
	CHECK(schema->get_field("name")->is_required());
	CHECK(schema->get_field("pos")->get_schema()->get_field("x")->get_value_type() == Variant::INT);
	CHECK(schema->get_field("tags")->get_element_type() == Variant::STRING);
	CHECK(schema->convert_value("name", "abc") == Variant("abc"));
	schema->remove_field(0);
	CHECK(schema->find_field("name") == -1);
}

TEST_CASE("[DataSchema] Dictionary validation reports every issue kind with field and path") {
	Ref<DataSchema> schema = make_item_schema();
	Dictionary data;
	data["name"] = "";
	data["rarity"] = "epic";
	data["hp"] = 150;
	data["code"] = "x1";
	data["colour"] = "red";
	Ref<DataValidation> result = schema->validate(data);
	CHECK_FALSE(result->is_valid());
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_MISSING_FIELD, "id"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_EMPTY_REQUIRED, "name"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_NOT_IN_ENUM, "rarity"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_OUT_OF_RANGE, "hp"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_PATTERN_MISMATCH, "code"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_UNKNOWN_FIELD, "colour"));
	CHECK(result->get_issue_count() == 6);

	data.erase("colour");
	data["id"] = "abc";
	result = schema->validate(data);
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "id"));
	CHECK_FALSE(has_issue(result, DataValidation::ISSUE_KIND_UNKNOWN_FIELD, "colour"));

	schema->set_extra_fields(DataSchema::EXTRA_FIELDS_ALLOW);
	data["colour"] = "red";
	CHECK_FALSE(has_issue(schema->validate(data), DataValidation::ISSUE_KIND_UNKNOWN_FIELD, "colour"));
}

TEST_CASE("[DataSchema] Options limit the issues") {
	Ref<DataSchema> schema = make_item_schema();
	Dictionary data;
	data["hp"] = 500;
	data["code"] = "?";
	Ref<DataValidationOptions> options;
	options.instantiate();
	options->set_stop_on_first(true);
	CHECK(schema->validate(data, options)->get_issue_count() == 1);
	options->set_stop_on_first(false);
	options->set_max_issues(2);
	CHECK(schema->validate(data, options)->get_issue_count() == 2);
}

TEST_CASE("[DataSchema] JSON records, nested schemas and arrays") {
	const String json = R"([
	{"id": 1, "name": "Sword", "stats": {"atk": 5}, "tags": ["a", "b"], "drops": [{"item": "gold", "count": 3}]},
	{"id": 1, "name": "Axe", "stats": {"atk": "high"}, "tags": [1], "drops": [{"item": "gem", "count": "many"}]}
])";
	Dictionary stats;
	stats["atk"] = Variant::INT;
	Dictionary drop;
	drop["item"] = Variant::STRING;
	drop["count"] = Variant::INT;
	Dictionary shape;
	shape["id"] = Variant::INT;
	shape["name"] = Variant::STRING;
	shape["stats"] = stats;
	shape["tags"] = Array{ Variant::STRING };
	shape["drops"] = Array{ drop };
	Ref<DataSchema> schema = DataSchema::create_from_dictionary(shape);
	schema->get_field("id")->set_unique(true);

	Ref<DataValidation> result = schema->validate(JSON::parse_string(json));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_DUPLICATE_KEY, "id", 1));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "atk", 1));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "tags", 1));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "count", 1));
	CHECK(result->get_issue_count() == 4);
	CHECK(result->get_invalid_rows() == PackedInt32Array{ 1 });
	bool found_path = false;
	for (int i = 0; i < result->get_issue_count(); i++) {
		found_path = found_path || result->get_issue_path(i) == "1/drops/0/count";
	}
	CHECK(found_path);

	// Lines for JSON from the text.
	result->resolve_lines(json);
	for (int i = 0; i < result->get_issue_count(); i++) {
		CHECK(result->get_issue_line(i) == 3);
	}
}

TEST_CASE("[DataSchema] CSV validation by header with rows, columns and lines") {
	Ref<CSV> csv;
	csv.instantiate();
	// Row 1 has a quoted cell over two lines, so row 2 starts on line 5.
	REQUIRE(csv->parse("id,name,rarity,hp,code,extra\n1,Sword,common,10,AB1,x\n2,\"Big\nAxe\",rare,500,AB2,\n2,,epic,x,ab,\n", true) == OK);
	Ref<DataSchema> schema = make_item_schema();
	Ref<DataValidationOptions> options;
	options.instantiate();
	options->set_keep_positions(true);
	Ref<DataValidation> result = csv->validate(schema, options);

	CHECK(has_issue(result, DataValidation::ISSUE_KIND_UNKNOWN_FIELD, "extra"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_OUT_OF_RANGE, "hp", 1));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_DUPLICATE_KEY, "id", 2));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_EMPTY_REQUIRED, "name", 2));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_NOT_IN_ENUM, "rarity", 2));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "hp", 2));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_PATTERN_MISMATCH, "code", 2));
	CHECK(result->get_issue_count() == 7);
	for (int i = 0; i < result->get_issue_count(); i++) {
		const int row = result->get_issue_row(i);
		const int expected_line = row == DataValidation::POSITION_NONE ? 1 : (row == 1 ? 3 : 5);
		CHECK(result->get_issue_line(i) == expected_line);
		if (result->get_issue_field(i) == StringName("hp")) {
			CHECK(result->get_issue_column(i) == 3);
		}
	}
	// No lines without keep_positions.
	CHECK(csv->validate(schema)->get_issue_line(0) == DataValidation::POSITION_NONE);

	// A missing required column.
	csv->parse("name\nSword\n");
	CHECK(has_issue(csv->validate(schema), DataValidation::ISSUE_KIND_MISSING_FIELD, "id"));
	// A parse error is the only issue.
	ERR_PRINT_OFF;
	csv->parse("id\n\"open\n");
	ERR_PRINT_ON;
	result = csv->validate(schema);
	REQUIRE(result->get_issue_count() == 1);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_PARSE_ERROR);
}

TEST_CASE("[DataSchema] CSV typed rows and columns") {
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse("id,name,hp\n1,Sword,20\n2,Axe,\n") == OK);
	Ref<DataSchema> schema = make_item_schema();
	const Dictionary row = csv->get_typed_row(1, schema);
	CHECK(row["id"] == Variant(2));
	CHECK(row["hp"] == Variant(10)); // Empty cell: default_value.
	CHECK(csv->get_typed_column(0, schema) == Variant(PackedInt64Array{ 1, 2 }));
	CHECK(csv->get_typed_column(1, schema).get_type() == Variant::PACKED_STRING_ARRAY);

	Dictionary values;
	values["id"] = 3;
	values["name"] = "Bow";
	values["hp"] = 30.0;
	CHECK(csv->append_typed_row(values, schema) == OK);
	CHECK(csv->get_row(2) == PackedStringArray{ "3", "Bow", "30" });
	values["hp"] = "lots";
	CHECK(csv->append_typed_row(values, schema) == ERR_INVALID_DATA);
	CHECK(csv->get_row_count() == 3);
}

TEST_CASE("[DataSchema] INI sections as fields, template for every section, lines") {
	Ref<INI> ini;
	ini.instantiate();
	REQUIRE(ini->parse("[goblin]\nhp=10\nname=\"Gob\"\n\n[orc]\nhp=\"strong\"\n", true) == OK);
	Dictionary section;
	section["hp"] = Variant::INT;
	section["name"] = Variant::STRING;
	Ref<DataField> every_section = make_field("section", Variant::DICTIONARY);
	every_section->set_schema(DataSchema::create_from_dictionary(section));
	Ref<DataSchema> schema;
	schema.instantiate();
	schema->set_extra_fields(DataSchema::EXTRA_FIELDS_MATCH_TEMPLATE);
	schema->set_extra_field_template(every_section);

	Ref<DataValidationOptions> options;
	options.instantiate();
	options->set_keep_positions(true);
	Ref<DataValidation> result = ini->validate(schema, options);
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_MISSING_FIELD, "name"));
	CHECK(has_issue(result, DataValidation::ISSUE_KIND_TYPE_MISMATCH, "hp"));
	REQUIRE(result->get_issue_count() == 2);
	for (int i = 0; i < result->get_issue_count(); i++) {
		if (result->get_issue_kind(i) == DataValidation::ISSUE_KIND_TYPE_MISMATCH) {
			CHECK(result->get_issue_path(i) == "orc/hp");
			CHECK(result->get_issue_line(i) == 6);
		} else {
			CHECK(result->get_issue_path(i) == "orc");
			CHECK(result->get_issue_line(i) == 5);
		}
	}
}

TEST_CASE("[DataSchema] XML attributes and child elements as fields") {
	Ref<XML> xml;
	xml.instantiate();
	REQUIRE(xml->parse("<items><item id=\"1\"><name>Sword</name></item><item id=\"x\"/></items>") == OK);
	Dictionary item;
	item["id"] = Variant::INT;
	Ref<DataSchema> schema = DataSchema::create_from_dictionary(item);
	schema->get_field("id")->set_required(true);
	Ref<DataField> name_field = make_field("name", Variant::STRING);
	schema->add_field(name_field);
	Ref<DataField> items = make_field("item", Variant::ARRAY);
	items->set_element_schema(schema);
	Ref<DataSchema> root;
	root.instantiate();
	root->add_field(items);
	Ref<DataValidation> result = xml->validate(root);
	REQUIRE(result->get_issue_count() == 1);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_TYPE_MISMATCH);
	CHECK(result->get_issue_path(0) == "item/1/id");
}

TEST_CASE("[DataSchema] resolve_lines() for many JSON records, in and out of order") {
	// One record per line: record n is on line n + 2.
	String json = "[\n";
	for (int i = 0; i < 2990; i++) {
		json += vformat("{\"id\": %d, \"hp\": %d},\n", i, i % 3 == 0 ? 500 : 5);
	}
	json += "{\"id\": 2990, \"hp\": 5}\n]";
	Dictionary shape;
	shape["id"] = Variant::INT;
	shape["hp"] = Variant::INT;
	Ref<DataSchema> schema = DataSchema::create_from_dictionary(shape);
	schema->get_field("hp")->set_range(0, 100);

	Ref<DataValidation> result = schema->validate(JSON::parse_string(json));
	REQUIRE(result->get_issue_count() == 997);
	// Issues out of order and a key that is not in the text.
	result->add_issue(DataValidation::ISSUE_KIND_OUT_OF_RANGE, "x", "hp", "3/hp", 3);
	result->add_issue(DataValidation::ISSUE_KIND_MISSING_FIELD, "x", "mana", "4/mana", 4);
	result->add_issue(DataValidation::ISSUE_KIND_OUT_OF_RANGE, "x", "hp", "5000/hp", 5000);

	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	result->resolve_lines(json);
	MESSAGE(vformat("Lines of 1000 issues resolved in %d ms.", OS::get_singleton()->get_ticks_msec() - start));
	bool lines_match = true;
	for (int i = 0; i < 997; i++) {
		lines_match = lines_match && result->get_issue_line(i) == result->get_issue_row(i) + 2;
	}
	CHECK(lines_match);
	CHECK(result->get_issue_line(997) == 5);
	CHECK(result->get_issue_line(998) == DataValidation::POSITION_NONE);
	CHECK(result->get_issue_line(999) == DataValidation::POSITION_NONE);
}

TEST_CASE("[DataSchema] Many CSV rows validate") {
	String text = "id,name,hp\n";
	for (int i = 0; i < 20000; i++) {
		text += vformat("%d,Item %d,%d\n", i, i, i % 100);
	}
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse(text) == OK);
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	Ref<DataValidation> result = csv->validate(make_item_schema());
	MESSAGE(vformat("20000 rows validated in %d ms.", OS::get_singleton()->get_ticks_msec() - start));
	CHECK(result->is_valid());
}

TEST_CASE("[DataSchema] create_from_resource describes the stored properties") {
	// The sample is a DataSchema: an enum, a typed array of resources, a nested resource and a packed array.
	Ref<DataSchema> sample;
	sample.instantiate();
	sample->add_field(make_field("id", Variant::INT));
	const Ref<DataSchema> schema = DataSchema::create_from_resource(sample);
	REQUIRE(schema.is_valid());
	CHECK(schema->get_extra_fields() == DataSchema::EXTRA_FIELDS_ALLOW);

	const Ref<DataField> extra = schema->get_field("extra_fields");
	REQUIRE(extra.is_valid());
	CHECK(extra->get_value_type() == Variant::STRING); // Enums are written as names.
	CHECK(extra->get_hint() == PROPERTY_HINT_ENUM);
	const Ref<DataField> fields = schema->get_field("fields");
	REQUIRE(fields.is_valid());
	CHECK(fields->get_value_type() == Variant::ARRAY);
	REQUIRE(fields->get_element_schema().is_valid());
	CHECK(fields->get_element_schema()->get_field("field_name").is_valid());
	const Ref<DataField> field_template = schema->get_field("extra_field_template");
	REQUIRE(field_template.is_valid());
	CHECK(field_template->get_value_type() == Variant::DICTIONARY);
	CHECK(field_template->get_schema().is_valid());
	CHECK(schema->get_field("file_patterns")->get_value_type() == Variant::PACKED_STRING_ARRAY);
	CHECK_FALSE(schema->get_field("fields")->is_required());

	// Without enums as names the enum stays a number.
	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_enums_as_names(false);
	CHECK(DataSchema::create_from_resource(sample, options)->get_field("extra_fields")->get_value_type() == Variant::INT);

	// Data as the text formats write it validates; an unknown enum name does not.
	Dictionary data;
	data["type"] = "DataSchema";
	data["extra_fields"] = "Allow";
	CHECK(schema->validate(data)->is_valid());
	data["extra_fields"] = "Maybe";
	CHECK_FALSE(schema->validate(data)->is_valid());
}

} // namespace TestDataSchema
