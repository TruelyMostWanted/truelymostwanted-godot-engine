/**************************************************************************/
/*  xml_format.cpp                                                        */
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

#include "xml_format.h"

#include "core/io/resource_text_serializer.h"
#include "core/io/xml_parser.h"
#include "core/io/xml_writer.h"
#include "core/object/class_db.h"

namespace {

struct XMLElementReader {
	Ref<XMLParser> parser;
	String error;
	int error_line = 0;

	bool fail(int p_line, const String &p_message) {
		error = vformat("Line %d: %s", p_line, p_message);
		error_line = p_line;
		return false;
	}

	// Reads the element the parser stands on, up to and including its end tag.
	bool read_element(int p_depth, Dictionary &r_element);
};

// Adjacent text nodes (text and CDATA) become one String.
void _xml_append_text(Array &r_children, const String &p_text) {
	const int last = r_children.size() - 1;
	if (last >= 0 && r_children[last].get_type() == Variant::STRING) {
		r_children.set(last, String(r_children[last]) + p_text);
	} else {
		r_children.push_back(p_text);
	}
}

bool XMLElementReader::read_element(int p_depth, Dictionary &r_element) {
	const String element_name = parser->get_node_name();
	const int line = parser->get_current_line() + 1;
	if (p_depth > Variant::MAX_RECURSION_DEPTH) {
		return fail(line, "the elements are nested too deeply.");
	}

	Dictionary attributes;
	for (int i = 0; i < parser->get_attribute_count(); i++) {
		attributes[parser->get_attribute_name(i)] = parser->get_attribute_value(i);
	}

	Array children;
	String text;
	if (!parser->is_empty()) {
		bool closed = false;
		while (!closed) {
			if (parser->read() != OK) {
				return fail(line, vformat("the element <%s> is not closed.", element_name));
			}
			switch (parser->get_node_type()) {
				case XMLParser::NODE_ELEMENT: {
					Dictionary child;
					if (!read_element(p_depth + 1, child)) {
						return false;
					}
					children.push_back(child);
				} break;
				case XMLParser::NODE_ELEMENT_END: {
					const String end_name = parser->get_node_name().strip_edges();
					if (end_name != element_name) {
						return fail(parser->get_current_line() + 1, vformat("</%s> does not close <%s> (line %d).", end_name, element_name, line));
					}
					closed = true;
				} break;
				case XMLParser::NODE_TEXT: {
					// Whitespace-only text between elements is indentation.
					const String node_text = parser->get_node_data();
					if (!node_text.strip_edges().is_empty()) {
						_xml_append_text(children, node_text);
						text += node_text;
					}
				} break;
				case XMLParser::NODE_CDATA: {
					const String cdata = parser->get_node_name();
					if (!cdata.is_empty()) {
						_xml_append_text(children, cdata);
						text += cdata;
					}
				} break;
				default:
					break; // Comments and processing instructions.
			}
		}
	}

	r_element[XML::KEY_NAME] = element_name;
	r_element[XML::KEY_ATTRIBUTES] = attributes;
	r_element[XML::KEY_CHILDREN] = children;
	r_element[XML::KEY_TEXT] = text;
	return true;
}

void _xml_write_text(const String &p_text, String &r_text) {
	if (p_text.is_empty()) {
		return;
	}
	if (p_text.strip_edges().is_empty()) {
		// Whitespace-only text would be read as indentation; CDATA keeps it. "]]>" can't appear inside CDATA.
		r_text += "<![CDATA[" + p_text.replace("]]>", "]]]]><![CDATA[>") + "]]>";
		return;
	}
	r_text += XMLWriter::escape_text(p_text);
}

Error _xml_write_element(const Dictionary &p_element, const ResourceTextSerializer::Options &p_options, int p_depth, bool p_inline, String &r_text, String &r_error) {
	if (p_depth > Variant::MAX_RECURSION_DEPTH) {
		r_error = "The elements are nested too deeply.";
		return ERR_INVALID_DATA;
	}
	const Variant name_value = p_element.get(XML::KEY_NAME, Variant());
	const String element_name = name_value;
	if (name_value.get_type() != Variant::STRING && name_value.get_type() != Variant::STRING_NAME) {
		r_error = vformat("An element needs the key \"%s\" with the tag name.", XML::KEY_NAME);
		return ERR_INVALID_DATA;
	}
	if (!XMLWriter::is_valid_name(element_name)) {
		r_error = vformat("\"%s\" is not a valid XML element name.", element_name);
		return ERR_INVALID_DATA;
	}

	r_text += "<" + element_name;
	const Variant attributes_value = p_element.get(XML::KEY_ATTRIBUTES, Variant());
	if (attributes_value.get_type() == Variant::DICTIONARY) {
		const Dictionary attributes = attributes_value;
		Vector<String> attribute_names;
		for (const KeyValue<Variant, Variant> &kv : attributes) {
			attribute_names.push_back(kv.key);
		}
		if (p_options.sort_keys) {
			attribute_names.sort();
		}
		for (const String &attribute : attribute_names) {
			if (!XMLWriter::is_valid_name(attribute)) {
				r_error = vformat("\"%s\" is not a valid XML attribute name (element <%s>).", attribute, element_name);
				return ERR_INVALID_DATA;
			}
			const Variant value = attributes.get(attribute, Variant());
			r_text += " " + attribute + "=\"" + XMLWriter::escape_attribute(value) + "\"";
		}
	} else if (attributes_value.get_type() != Variant::NIL) {
		r_error = vformat("The key \"%s\" of <%s> must be a Dictionary.", XML::KEY_ATTRIBUTES, element_name);
		return ERR_INVALID_DATA;
	}

	// The children, or the text without them.
	Array children;
	const Variant children_value = p_element.get(XML::KEY_CHILDREN, Variant());
	if (children_value.get_type() == Variant::ARRAY) {
		children = children_value;
	} else if (children_value.get_type() != Variant::NIL) {
		r_error = vformat("The key \"%s\" of <%s> must be an Array.", XML::KEY_CHILDREN, element_name);
		return ERR_INVALID_DATA;
	} else {
		const String text = p_element.get(XML::KEY_TEXT, String());
		if (!text.is_empty()) {
			children.push_back(text);
		}
	}

	bool has_text = false;
	for (int i = 0; i < children.size(); i++) {
		const Variant::Type type = children[i].get_type();
		if (type == Variant::ARRAY || type == Variant::OBJECT) {
			r_error = vformat("The children of <%s> must be element dictionaries or text.", element_name);
			return ERR_INVALID_DATA;
		}
		if (type != Variant::DICTIONARY && type != Variant::NIL) {
			has_text = true;
		}
	}
	if (children.is_empty()) {
		r_text += "/>";
		return OK;
	}
	r_text += ">";

	// Inside an element with text, indentation would become part of the text.
	const bool child_inline = p_inline || has_text || p_options.indent.is_empty();
	for (int i = 0; i < children.size(); i++) {
		const Variant &child = children[i];
		if (child.get_type() == Variant::DICTIONARY) {
			if (!child_inline) {
				r_text += "\n" + p_options.indent.repeat(p_depth + 1);
			}
			const Error err = _xml_write_element(child, p_options, p_depth + 1, child_inline, r_text, r_error);
			if (err != OK) {
				return err;
			}
		} else if (child.get_type() != Variant::NIL) {
			_xml_write_text(child, r_text);
		}
	}
	if (!child_inline) {
		r_text += "\n" + p_options.indent.repeat(p_depth);
	}
	r_text += "</" + element_name + ">";
	return OK;
}

} // namespace

Error XML::parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line) {
	r_data = Dictionary();
	r_error = String();
	r_line = 0;
	// XMLParser keeps the type of the previous node when the input ends in one or two
	// whitespace characters, so a trailing "\n\n" after a root `<a/>` would read as a second root.
	// Trailing whitespace carries no data; dropping it keeps the line numbers.
	const PackedByteArray buffer = p_text.strip_edges(false, true).to_utf8_buffer();
	if (buffer.is_empty()) {
		r_error = "Line 1: the text is empty.";
		r_line = 1;
		return ERR_PARSE_ERROR;
	}
	XMLElementReader reader;
	reader.parser.instantiate();
	if (reader.parser->open_buffer(buffer) != OK) {
		r_error = "Line 1: the text cannot be read as XML.";
		r_line = 1;
		return ERR_PARSE_ERROR;
	}
	bool found_root = false;
	while (reader.parser->read() == OK) {
		if (reader.parser->get_node_type() != XMLParser::NODE_ELEMENT) {
			continue;
		}
		if (found_root) {
			reader.fail(reader.parser->get_current_line() + 1, "an XML document has only one root element.");
			break;
		}
		Dictionary root;
		if (!reader.read_element(0, root)) {
			break;
		}
		r_data = root;
		found_root = true;
	}
	if (!reader.error.is_empty()) {
		r_data = Dictionary();
		r_error = reader.error;
		r_line = reader.error_line;
		return ERR_PARSE_ERROR;
	}
	if (!found_root) {
		r_error = "Line 1: the XML has no root element.";
		r_line = 1;
		return ERR_PARSE_ERROR;
	}
	return OK;
}

Error XML::write_text(const Dictionary &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) {
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	String text;
	if (options.xml_declaration) {
		text += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
		if (!options.indent.is_empty()) {
			text += "\n";
		}
	}
	const Error err = _xml_write_element(p_data, options, 0, false, text, r_error);
	if (err != OK) {
		return err;
	}
	if (!options.indent.is_empty()) {
		text += "\n";
	}
	r_text = text;
	return OK;
}

Error XML::parse(const String &p_text) {
	return parse_text(p_text, data, error_message, error_line);
}

Dictionary XML::parse_string(const String &p_text) {
	Dictionary result;
	String error;
	int line = 0;
	if (parse_text(p_text, result, error, line) != OK) {
		ERR_PRINT("XML parse error: " + error);
		return Dictionary();
	}
	return result;
}

String XML::stringify(const Dictionary &p_data, const Ref<SerializerOptions> &p_options) {
	String text;
	String error;
	if (write_text(p_data, p_options, text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

String XML::serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, String());
	return serializer->serialize(p_resource, ResourceTextSerializer::FORMAT_XML, p_options);
}

Ref<Resource> XML::deserialize(const String &p_text, const StringName &p_type_hint, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, Ref<Resource>());
	return serializer->deserialize(p_text, ResourceTextSerializer::FORMAT_XML, p_type_hint, p_options);
}

void XML::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "text"), &XML::parse);
	ClassDB::bind_method(D_METHOD("get_data"), &XML::get_data);
	ClassDB::bind_method(D_METHOD("get_error_message"), &XML::get_error_message);
	ClassDB::bind_method(D_METHOD("get_error_line"), &XML::get_error_line);

	ClassDB::bind_static_method("XML", D_METHOD("parse_string", "text"), &XML::parse_string);
	ClassDB::bind_static_method("XML", D_METHOD("stringify", "data", "options"), &XML::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("XML", D_METHOD("serialize", "resource", "options"), &XML::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("XML", D_METHOD("deserialize", "text", "type_hint", "options"), &XML::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
