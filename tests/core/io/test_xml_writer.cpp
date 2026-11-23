/**************************************************************************/
/*  test_xml_writer.cpp                                                   */
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

TEST_FORCE_LINK(test_xml_writer)

#include "core/io/xml_parser.h"
#include "core/io/xml_writer.h"

namespace TestXMLWriter {

TEST_CASE("[XMLWriter] Document layout") {
	Ref<XMLWriter> writer;
	writer.instantiate();
	CHECK(writer->write_declaration() == OK);
	CHECK(writer->begin_element("inventory") == OK);
	CHECK(writer->add_attribute("owner", "Ann & Bob") == OK);
	CHECK(writer->begin_element("item") == OK);
	CHECK(writer->add_attribute("id", "sword") == OK);
	CHECK(writer->write_text("Sword of <Light>") == OK);
	CHECK(writer->end_element() == OK);
	CHECK(writer->write_element("empty") == OK);
	CHECK(writer->write_comment(" gold below ") == OK);
	CHECK(writer->write_element("gold", "250") == OK);
	CHECK(writer->get_depth() == 1);
	CHECK(writer->end_element() == OK);
	CHECK(writer->get_depth() == 0);

	const String expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
							"<inventory owner=\"Ann &amp; Bob\">\n"
							"  <item id=\"sword\">Sword of &lt;Light&gt;</item>\n"
							"  <empty/>\n"
							"  <!-- gold below -->\n"
							"  <gold>250</gold>\n"
							"</inventory>\n";
	CHECK(writer->get_output() == expected);
}

TEST_CASE("[XMLWriter] Compact output and escaping") {
	Ref<XMLWriter> writer;
	writer.instantiate();
	writer->set_indent("");
	writer->begin_element("a");
	writer->add_attribute("q", "say \"hi\"\n'now'");
	writer->begin_element("b");
	writer->write_cdata("x ]]> y");
	writer->end_all();
	CHECK(writer->get_output() == "<a q=\"say &quot;hi&quot;&#10;&apos;now&apos;\"><b><![CDATA[x ]]]]><![CDATA[> y]]></b></a>");
}

TEST_CASE("[XMLWriter] Invalid calls") {
	Ref<XMLWriter> writer;
	writer.instantiate();
	ERR_PRINT_OFF;
	CHECK(writer->begin_element("1abc") == ERR_INVALID_PARAMETER);
	CHECK(writer->begin_element("a b") == ERR_INVALID_PARAMETER);
	CHECK(writer->write_text("outside") == ERR_UNCONFIGURED);
	CHECK(writer->end_element() == ERR_DOES_NOT_EXIST);
	CHECK(writer->add_attribute("x", "1") == ERR_UNCONFIGURED);

	CHECK(writer->begin_element("root") == OK);
	CHECK(writer->add_attribute("x", "1") == OK);
	CHECK(writer->add_attribute("x", "2") == ERR_ALREADY_EXISTS);
	CHECK(writer->write_declaration() == ERR_ALREADY_EXISTS);
	CHECK(writer->write_comment("a -- b") == ERR_INVALID_PARAMETER);
	CHECK(writer->save("user://never_written.xml") == ERR_UNCONFIGURED);
	CHECK(writer->write_text("text") == OK);
	CHECK(writer->add_attribute("y", "1") == ERR_UNCONFIGURED);
	CHECK(writer->end_element() == OK);
	CHECK(writer->begin_element("second_root") == ERR_ALREADY_EXISTS);
	ERR_PRINT_ON;

	CHECK(XMLWriter::is_valid_name("ns:item-2.b_c"));
	CHECK(XMLWriter::is_valid_name("_x"));
	CHECK_FALSE(XMLWriter::is_valid_name(""));
	CHECK_FALSE(XMLWriter::is_valid_name("-x"));

	writer->clear();
	CHECK(writer->get_output().is_empty());
	CHECK(writer->begin_element("again") == OK);
}

TEST_CASE("[XMLWriter] Output can be read by XMLParser") {
	Ref<XMLWriter> writer;
	writer.instantiate();
	writer->begin_element("top");
	writer->add_attribute("attr", "a < b & \"c\"");
	writer->write_element("child", "Text <&> more");
	writer->end_all();

	XMLParser parser;
	REQUIRE(parser.open_buffer(writer->get_output_buffer()) == OK);
	REQUIRE(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NODE_ELEMENT);
	CHECK(parser.get_node_name() == "top");
	CHECK(parser.get_named_attribute_value("attr") == "a < b & \"c\"");

	// Skip the whitespace between the elements.
	REQUIRE(parser.read() == OK);
	while (parser.get_node_type() == XMLParser::NODE_TEXT) {
		REQUIRE(parser.read() == OK);
	}
	CHECK(parser.get_node_type() == XMLParser::NODE_ELEMENT);
	CHECK(parser.get_node_name() == "child");
	REQUIRE(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NODE_TEXT);
	CHECK(parser.get_node_data() == "Text <&> more");
	REQUIRE(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NODE_ELEMENT_END);
	CHECK(parser.get_node_name() == "child");
}

} // namespace TestXMLWriter
