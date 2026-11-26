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

#include "core/io/data_schema.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/xml_parser.h"
#include "core/io/xml_writer.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

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

Error XML::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	Dictionary parsed;
	r_column = 0;
	const Error err = parse_text(p_text, parsed, r_error, r_line);
	r_data = parsed;
	return err;
}

Error XML::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	return write_text(p_data, p_options, r_text, r_error);
}

void XML::_get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("xml");
}

// An element as validation data: attributes and child elements are the keys (repeated children, and children
// whose field is an Array, become an Array), other text is the key "#text". An element with neither
// attributes nor children is its text.
static Variant _xml_validation_value(const Dictionary &p_element, const Ref<DataSchema> &p_schema) {
	const Dictionary attributes = p_element.get(XML::KEY_ATTRIBUTES, Dictionary());
	const Array children = p_element.get(XML::KEY_CHILDREN, Array());
	Dictionary result = attributes.duplicate();
	HashSet<String> lists;
	bool has_elements = false;
	for (const Variant &child : children) {
		if (child.get_type() != Variant::DICTIONARY) {
			continue;
		}
		has_elements = true;
		const Dictionary child_element = child;
		const String child_name = child_element.get(XML::KEY_NAME, String());
		const Ref<DataField> field = p_schema.is_valid() ? p_schema->get_field(child_name) : Ref<DataField>();
		const bool is_list = field.is_valid() && field->get_value_type() == Variant::ARRAY;
		const Variant value = _xml_validation_value(child_element, field.is_null() ? Ref<DataSchema>() : (is_list ? field->get_element_schema() : field->get_schema()));
		if (lists.has(child_name)) {
			Array list = result[child_name];
			list.push_back(value);
		} else if (is_list || result.has(child_name)) {
			Array list;
			if (result.has(child_name)) {
				list.push_back(result[child_name]);
			}
			list.push_back(value);
			result[child_name] = list;
			lists.insert(child_name);
		} else {
			result[child_name] = value;
		}
	}
	const String text = p_element.get(XML::KEY_TEXT, String());
	if (attributes.is_empty() && !has_elements) {
		return text;
	}
	if (!text.strip_edges().is_empty()) {
		result["#text"] = text;
	}
	return result;
}

Ref<DataValidation> XML::_validate_data(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const {
	const Dictionary root = get_data();
	Variant value = root.is_empty() ? Variant(Dictionary()) : _xml_validation_value(root, p_schema);
	if (value.get_type() != Variant::DICTIONARY) {
		// A root element with only text.
		Dictionary text_only;
		text_only["#text"] = value;
		value = text_only;
	}
	return p_schema->validate(value, p_options);
}

Ref<XMLElement> XML::get_root() {
	const Dictionary tree = get_data();
	if (tree.is_empty()) {
		return Ref<XMLElement>();
	}
	if (root_view.is_null()) {
		root_view = XMLElement::create_view(tree, get_instance_id());
	}
	return root_view;
}

Ref<XMLElement> XML::create_root(const String &p_name) {
	Dictionary tree;
	tree[KEY_NAME] = p_name;
	tree[KEY_ATTRIBUTES] = Dictionary();
	tree[KEY_CHILDREN] = Array();
	tree[KEY_TEXT] = String();
	set_data(tree);
	return get_root();
}

Ref<XMLElement> XML::find_first(const String &p_path) {
	const Ref<XMLElement> root = get_root();
	return root.is_valid() ? root->find_first(p_path) : Ref<XMLElement>();
}

TypedArray<XMLElement> XML::find_all(const String &p_path) {
	const Ref<XMLElement> root = get_root();
	return root.is_valid() ? root->find_all(p_path) : TypedArray<XMLElement>();
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

XML::XML() {
	_get_data_ref() = Dictionary();
}

void XML::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_root"), &XML::get_root);
	ClassDB::bind_method(D_METHOD("create_root", "name"), &XML::create_root);
	ClassDB::bind_method(D_METHOD("find_first", "path"), &XML::find_first);
	ClassDB::bind_method(D_METHOD("find_all", "path"), &XML::find_all);

	ClassDB::bind_static_method("XML", D_METHOD("parse_string", "text"), &XML::parse_string);
	ClassDB::bind_static_method("XML", D_METHOD("stringify", "data", "options"), &XML::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("XML", D_METHOD("serialize", "resource", "options"), &XML::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("XML", D_METHOD("deserialize", "text", "type_hint", "options"), &XML::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}

/////////////////////////////////////////////////////////////////////////////
// XMLElement.

XMLElement::XMLElement() {
	element[XML::KEY_NAME] = String();
	element[XML::KEY_ATTRIBUTES] = Dictionary();
	element[XML::KEY_CHILDREN] = Array();
	element[XML::KEY_TEXT] = String();
}

Ref<XMLElement> XMLElement::create_view(const Dictionary &p_element, ObjectID p_owner) {
	Ref<XMLElement> view;
	view.instantiate();
	view->element = p_element;
	view->owner_id = p_owner;
	return view;
}

void XMLElement::_changed() {
	XML *owner = ObjectDB::get_instance<XML>(owner_id);
	if (owner) {
		owner->notify_tree_changed();
	}
}

Array XMLElement::_get_children_array() {
	if (element.get(XML::KEY_CHILDREN, Variant()).get_type() != Variant::ARRAY) {
		element[XML::KEY_CHILDREN] = Array();
	}
	return element[XML::KEY_CHILDREN];
}

void XMLElement::set_name(const String &p_name) {
	element[XML::KEY_NAME] = p_name;
	_changed();
}

String XMLElement::get_name() const {
	return element.get(XML::KEY_NAME, String());
}

void XMLElement::set_text(const String &p_text) {
	// Replaces all text nodes; child elements stay.
	Array children = _get_children_array();
	for (int i = children.size() - 1; i >= 0; i--) {
		if (children[i].get_type() == Variant::STRING) {
			children.remove_at(i);
		}
	}
	if (!p_text.is_empty()) {
		children.insert(0, p_text);
	}
	element[XML::KEY_TEXT] = p_text;
	_changed();
}

String XMLElement::get_text() const {
	return element.get(XML::KEY_TEXT, String());
}

String XMLElement::get_attribute(const String &p_name, const String &p_default) const {
	const Variant attributes = element.get(XML::KEY_ATTRIBUTES, Variant());
	if (attributes.get_type() != Variant::DICTIONARY) {
		return p_default;
	}
	return Dictionary(attributes).get(p_name, p_default);
}

void XMLElement::set_attribute(const String &p_name, const String &p_value) {
	ERR_FAIL_COND_MSG(p_name.is_empty(), "An attribute name can't be empty.");
	if (element.get(XML::KEY_ATTRIBUTES, Variant()).get_type() != Variant::DICTIONARY) {
		element[XML::KEY_ATTRIBUTES] = Dictionary();
	}
	Dictionary attributes = element[XML::KEY_ATTRIBUTES];
	attributes[p_name] = p_value;
	_changed();
}

bool XMLElement::has_attribute(const String &p_name) const {
	const Variant attributes = element.get(XML::KEY_ATTRIBUTES, Variant());
	return attributes.get_type() == Variant::DICTIONARY && Dictionary(attributes).has(p_name);
}

void XMLElement::remove_attribute(const String &p_name) {
	const Variant attributes = element.get(XML::KEY_ATTRIBUTES, Variant());
	if (attributes.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary values = attributes;
	if (values.erase(p_name)) {
		_changed();
	}
}

Dictionary XMLElement::get_attributes() const {
	const Variant attributes = element.get(XML::KEY_ATTRIBUTES, Variant());
	return attributes.get_type() == Variant::DICTIONARY ? Dictionary(attributes).duplicate() : Dictionary();
}

TypedArray<XMLElement> XMLElement::get_children() const {
	TypedArray<XMLElement> result;
	const Variant children = element.get(XML::KEY_CHILDREN, Variant());
	if (children.get_type() != Variant::ARRAY) {
		return result;
	}
	const Array nodes = children;
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i].get_type() == Variant::DICTIONARY) {
			result.push_back(create_view(nodes[i], owner_id));
		}
	}
	return result;
}

void XMLElement::add_child(const Ref<XMLElement> &p_element) {
	ERR_FAIL_COND(p_element.is_null());
	ERR_FAIL_COND_MSG(p_element->element.is_same_instance(element), "An element can't be its own child.");
	Array children = _get_children_array();
	children.push_back(p_element->element);
	// The child now belongs to this tree: its changes mark this document as changed.
	p_element->owner_id = owner_id;
	_changed();
}

void XMLElement::remove_child(const Ref<XMLElement> &p_element) {
	ERR_FAIL_COND(p_element.is_null());
	Array children = _get_children_array();
	for (int i = 0; i < children.size(); i++) {
		if (children[i].get_type() == Variant::DICTIONARY && Dictionary(children[i]).is_same_instance(p_element->element)) {
			children.remove_at(i);
			_changed();
			return;
		}
	}
	ERR_FAIL_MSG("The element is not a child of this element.");
}

TypedArray<XMLElement> XMLElement::get_children_by_name(const String &p_name) const {
	TypedArray<XMLElement> result;
	const Variant children = element.get(XML::KEY_CHILDREN, Variant());
	if (children.get_type() != Variant::ARRAY) {
		return result;
	}
	const Array nodes = children;
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i].get_type() == Variant::DICTIONARY && String(Dictionary(nodes[i]).get(XML::KEY_NAME, String())) == p_name) {
			result.push_back(create_view(nodes[i], owner_id));
		}
	}
	return result;
}

void XMLElement::_find(const Vector<String> &p_segments, int p_index, bool p_first, TypedArray<XMLElement> &r_found) const {
	if (p_index >= p_segments.size()) {
		r_found.push_back(create_view(element, owner_id));
		return;
	}
	const TypedArray<XMLElement> matches = get_children_by_name(p_segments[p_index]);
	for (int i = 0; i < matches.size(); i++) {
		const Ref<XMLElement> match = matches[i];
		match->_find(p_segments, p_index + 1, p_first, r_found);
		if (p_first && !r_found.is_empty()) {
			return;
		}
	}
}

Ref<XMLElement> XMLElement::find_first(const String &p_path) const {
	TypedArray<XMLElement> found;
	_find(p_path.split("/", false), 0, true, found);
	return found.is_empty() ? Ref<XMLElement>() : Ref<XMLElement>(found[0]);
}

TypedArray<XMLElement> XMLElement::find_all(const String &p_path) const {
	TypedArray<XMLElement> found;
	_find(p_path.split("/", false), 0, false, found);
	return found;
}

void XMLElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_name", "name"), &XMLElement::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &XMLElement::get_name);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &XMLElement::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &XMLElement::get_text);
	ClassDB::bind_method(D_METHOD("get_attribute", "name", "default"), &XMLElement::get_attribute, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("set_attribute", "name", "value"), &XMLElement::set_attribute);
	ClassDB::bind_method(D_METHOD("has_attribute", "name"), &XMLElement::has_attribute);
	ClassDB::bind_method(D_METHOD("remove_attribute", "name"), &XMLElement::remove_attribute);
	ClassDB::bind_method(D_METHOD("get_attributes"), &XMLElement::get_attributes);
	ClassDB::bind_method(D_METHOD("get_children"), &XMLElement::get_children);
	ClassDB::bind_method(D_METHOD("add_child", "element"), &XMLElement::add_child);
	ClassDB::bind_method(D_METHOD("remove_child", "element"), &XMLElement::remove_child);
	ClassDB::bind_method(D_METHOD("get_children_by_name", "name"), &XMLElement::get_children_by_name);
	ClassDB::bind_method(D_METHOD("find_first", "path"), &XMLElement::find_first);
	ClassDB::bind_method(D_METHOD("find_all", "path"), &XMLElement::find_all);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
}
