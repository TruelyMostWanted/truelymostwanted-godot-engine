/**************************************************************************/
/*  resource_text_serializer_xml.cpp                                      */
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

#include "core/io/resource_text_serializer.h"
#include "core/io/xml_parser.h"
#include "core/io/xml_writer.h"
#include "core/math/math_defs.h"
#include "core/os/mutex.h"

// XML layout of the plain tree of encode_resource():
//   <resource type="Inventory" format_version="1">   root; "$" keys of a map are attributes
//     <owner>Ada</owner>                            String, int, float, bool as text
//     <code kind="string">007</code>                text that would read as a number or boolean
//     <icon kind="null"/>                           null
//     <offset kind="array">0.5, -2.0</offset>       array of numbers and booleans
//     <items kind="array"><item>...</item></items>  other arrays
//     <main><name>Ann</name></main>                 dictionary or nested resource
//     <entry key="metadata/x">...</entry>           key that is not an XML name
//     <extra kind="dictionary"/>                    empty dictionary

static constexpr const char *XML_ROOT = "resource";
static constexpr const char *XML_ITEM = "item";
static constexpr const char *XML_ENTRY = "entry";
static constexpr const char *XML_ATTR_KIND = "kind";
static constexpr const char *XML_ATTR_KEY = "key";
static constexpr const char *XML_ATTR_TYPE = "type";
static constexpr const char *XML_KIND_ARRAY = "array";
static constexpr const char *XML_KIND_DICTIONARY = "dictionary";
static constexpr const char *XML_KIND_NULL = "null";
static constexpr const char *XML_KIND_STRING = "string";

/////////////////////////////////////////////////////////////////////////////
// XML values.

// Floats always have a `.`, an exponent, `inf` or `nan`, so they are read back as floats.
static String _xml_number(double p_value) {
	const String text = String::num_scientific(p_value);
	if (text.contains_char('.') || text.contains_char('e') || text.contains_char('n')) {
		return text;
	}
	return text + ".0";
}

// The value of a text without `kind`: booleans, integers and floats get their type, everything else is a String.
static Variant _xml_infer(const String &p_text) {
	if (p_text == "true") {
		return true;
	}
	if (p_text == "false") {
		return false;
	}
	if (p_text.is_valid_int()) {
		return p_text.to_int();
	}
	if (p_text == "inf") {
		return Math::INF;
	}
	if (p_text == "-inf") {
		return -Math::INF;
	}
	if (p_text == "nan") {
		return Math::NaN;
	}
	if (p_text.is_valid_float()) {
		return p_text.to_float();
	}
	return p_text;
}

static String _xml_scalar_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
			return bool(p_value) ? "true" : "false";
		case Variant::INT:
			return itos(int64_t(p_value));
		case Variant::FLOAT:
			return _xml_number(double(p_value));
		default:
			return String(p_value);
	}
}

static bool _xml_is_element_name(const String &p_key) {
	return XMLWriter::is_valid_name(p_key) && !p_key.contains_char(':');
}

// "$type", "$format_version" and "$script" (and other "$" keys with a text or integer value) are attributes.
static bool _xml_get_attribute_name(const String &p_key, const Variant &p_value, String &r_name) {
	if (!p_key.begins_with("$")) {
		return false;
	}
	r_name = p_key.substr(1);
	if (!_xml_is_element_name(r_name) || r_name == XML_ATTR_KIND || r_name == XML_ATTR_KEY) {
		return false;
	}
	return p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::INT;
}

static bool _xml_is_compact_array(const Array &p_array) {
	if (p_array.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_array.size(); i++) {
		const Variant::Type type = p_array[i].get_type();
		if (type != Variant::BOOL && type != Variant::INT && type != Variant::FLOAT) {
			return false;
		}
	}
	return true;
}

static void _xml_write_value(XMLWriter *p_writer, const Variant &p_value);

// Writes the keys of a map into the element that was just begun.
static void _xml_write_map(XMLWriter *p_writer, const Dictionary &p_map) {
	int children = 0;
	for (const KeyValue<Variant, Variant> &kv : p_map) {
		String name;
		if (_xml_get_attribute_name(String(kv.key), kv.value, name)) {
			p_writer->add_attribute(name, _xml_scalar_text(kv.value));
		} else {
			children++;
		}
	}
	if (children == 0) {
		p_writer->add_attribute(XML_ATTR_KIND, XML_KIND_DICTIONARY);
		return;
	}
	for (const KeyValue<Variant, Variant> &kv : p_map) {
		const String key = kv.key;
		String name;
		if (_xml_get_attribute_name(key, kv.value, name)) {
			continue;
		}
		if (_xml_is_element_name(key)) {
			p_writer->begin_element(key);
		} else {
			p_writer->begin_element(XML_ENTRY);
			p_writer->add_attribute(XML_ATTR_KEY, key);
		}
		_xml_write_value(p_writer, kv.value);
		p_writer->end_element();
	}
}

// Writes a value into the element that was just begun.
static void _xml_write_value(XMLWriter *p_writer, const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL: {
			p_writer->add_attribute(XML_ATTR_KIND, XML_KIND_NULL);
		} break;
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT: {
			p_writer->write_text(_xml_scalar_text(p_value));
		} break;
		case Variant::ARRAY: {
			p_writer->add_attribute(XML_ATTR_KIND, XML_KIND_ARRAY);
			const Array array = p_value;
			if (_xml_is_compact_array(array)) {
				String text;
				for (int i = 0; i < array.size(); i++) {
					if (i > 0) {
						text += ", ";
					}
					text += _xml_scalar_text(array[i]);
				}
				p_writer->write_text(text);
			} else {
				for (int i = 0; i < array.size(); i++) {
					p_writer->begin_element(XML_ITEM);
					_xml_write_value(p_writer, array[i]);
					p_writer->end_element();
				}
			}
		} break;
		case Variant::DICTIONARY: {
			_xml_write_map(p_writer, p_value);
		} break;
		default: {
			const String text = p_value;
			if (text.is_empty()) {
				break;
			}
			if (text.strip_edges().is_empty()) {
				// XMLParser drops short whitespace-only text; CDATA is kept as it is.
				p_writer->write_cdata(text);
				break;
			}
			if (_xml_infer(text).get_type() != Variant::STRING) {
				p_writer->add_attribute(XML_ATTR_KIND, XML_KIND_STRING);
			}
			p_writer->write_text(text);
		} break;
	}
}

namespace {

struct XMLTreeReader {
	Ref<XMLParser> parser;
	String error;

	// Reads the element the parser stands on, up to and including its end tag.
	bool read_element(int p_depth, String &r_key, Variant &r_value);
};

bool XMLTreeReader::read_element(int p_depth, String &r_key, Variant &r_value) {
	const String name = parser->get_node_name();
	const int line = parser->get_current_line() + 1;
	if (p_depth > Variant::MAX_RECURSION_DEPTH) {
		error = vformat("Line %d: the elements are nested too deeply.", line);
		return false;
	}

	String kind;
	r_key = name;
	Dictionary map;
	for (int i = 0; i < parser->get_attribute_count(); i++) {
		const String attribute = parser->get_attribute_name(i);
		if (attribute == XML_ATTR_KIND) {
			kind = parser->get_attribute_value(i);
		} else if (attribute == XML_ATTR_KEY) {
			r_key = parser->get_attribute_value(i);
		} else if (!attribute.begins_with("xmlns") && !attribute.contains_char(':')) {
			map["$" + attribute] = parser->get_attribute_value(i);
		}
	}
	const bool is_array = kind == XML_KIND_ARRAY;

	String text;
	bool has_children = false;
	Array items;
	if (!parser->is_empty()) {
		bool closed = false;
		while (!closed) {
			if (parser->read() != OK) {
				error = vformat("Line %d: the element <%s> is not closed.", line, name);
				return false;
			}
			switch (parser->get_node_type()) {
				case XMLParser::NODE_ELEMENT: {
					has_children = true;
					String key;
					Variant value;
					if (!read_element(p_depth + 1, key, value)) {
						return false;
					}
					if (is_array) {
						items.push_back(value);
					} else {
						map[key] = value;
					}
				} break;
				case XMLParser::NODE_ELEMENT_END: {
					const String end_name = parser->get_node_name().strip_edges();
					if (end_name != name) {
						error = vformat("Line %d: </%s> does not close <%s> (line %d).", parser->get_current_line() + 1, end_name, name, line);
						return false;
					}
					closed = true;
				} break;
				case XMLParser::NODE_TEXT: {
					text += parser->get_node_data();
				} break;
				case XMLParser::NODE_CDATA: {
					text += parser->get_node_name();
				} break;
				default:
					break; // Comments and processing instructions.
			}
		}
	}

	if (is_array) {
		if (!has_children) {
			const String list = text.strip_edges();
			if (!list.is_empty()) {
				const Vector<String> parts = list.split(",");
				for (const String &part : parts) {
					items.push_back(_xml_infer(part.strip_edges()));
				}
			}
		}
		r_value = items;
	} else if (kind == XML_KIND_DICTIONARY || has_children || (kind.is_empty() && !map.is_empty())) {
		r_value = map;
	} else if (kind == XML_KIND_NULL) {
		r_value = Variant();
	} else if (kind == XML_KIND_STRING) {
		r_value = text;
	} else {
		r_value = _xml_infer(text);
	}
	return true;
}

} // namespace

static Error _xml_open(const String &p_text, Ref<XMLParser> &r_parser) {
	const PackedByteArray buffer = p_text.to_utf8_buffer();
	if (buffer.is_empty()) {
		return ERR_FILE_EOF;
	}
	r_parser.instantiate();
	return r_parser->open_buffer(buffer);
}

// Moves the parser to the root element. Returns false if there is none.
static bool _xml_find_root(const Ref<XMLParser> &p_parser) {
	while (p_parser->read() == OK) {
		if (p_parser->get_node_type() == XMLParser::NODE_ELEMENT) {
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// XML.

String ResourceTextSerializer::get_xml_marker_type(const String &p_text) {
	if (!p_text.contains("<resource")) {
		return String(); // Fast path for other XML files.
	}
	Ref<XMLParser> parser;
	if (_xml_open(p_text, parser) != OK || !_xml_find_root(parser)) {
		return String();
	}
	if (parser->get_node_name() != XML_ROOT) {
		return String();
	}
	const String type = parser->get_named_attribute_value_safe(XML_ATTR_TYPE);
	return is_resource_type(type) ? type : String();
}

Error ResourceTextSerializer::write_xml(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error) {
	Dictionary tree;
	const Error err = encode_resource(p_resource, p_options, true, tree, r_error);
	if (err != OK) {
		return err;
	}
	Ref<XMLWriter> writer;
	writer.instantiate();
	writer->set_indent(p_options.indent);
	writer->write_declaration();
	writer->begin_element(XML_ROOT);
	_xml_write_map(writer.ptr(), tree);
	writer->end_all();
	r_text = writer->get_output();
	return OK;
}

Error ResourceTextSerializer::parse_xml(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	if (hint.is_empty() && get_xml_marker_type(p_text).is_empty()) {
		r_error = vformat("The XML has no root element <%s> with a \"%s\" attribute that names a Resource class.", XML_ROOT, XML_ATTR_TYPE);
		return ERR_FILE_UNRECOGNIZED;
	}

	XMLTreeReader reader;
	if (_xml_open(p_text, reader.parser) != OK || !_xml_find_root(reader.parser)) {
		r_error = "The XML has no root element.";
		return ERR_PARSE_ERROR;
	}
	String key;
	Variant tree;
	if (!reader.read_element(0, key, tree)) {
		r_error = "XML parse error: " + reader.error;
		return ERR_PARSE_ERROR;
	}
	if (tree.get_type() != Variant::DICTIONARY) {
		r_error = "The root element of the XML must contain one element per property.";
		return ERR_INVALID_DATA;
	}
	return decode_resource(tree, hint, r_resource, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// Format registry.

namespace {

struct TextFormat {
	String name;
	ResourceTextSerializer::FormatWriteFunction write = nullptr;
	ResourceTextSerializer::FormatParseFunction parse = nullptr;
};

} // namespace

static Mutex text_formats_mutex;
static LocalVector<TextFormat> text_formats;

// Format names are lower-case, without a leading dot: "json", not ".JSON".
static String _format_key(const StringName &p_name) {
	return String(p_name).strip_edges().trim_prefix(".").to_lower();
}

static bool _get_text_format(const StringName &p_name, TextFormat &r_format) {
	const String key = _format_key(p_name);
	MutexLock lock(text_formats_mutex);
	for (const TextFormat &format : text_formats) {
		if (format.name == key) {
			r_format = format;
			return true;
		}
	}
	return false;
}

static PackedStringArray _get_text_format_names() {
	PackedStringArray names;
	MutexLock lock(text_formats_mutex);
	for (const TextFormat &format : text_formats) {
		names.push_back(format.name);
	}
	return names;
}

void ResourceTextSerializer::register_format(const StringName &p_name, FormatWriteFunction p_write, FormatParseFunction p_parse) {
	const String key = _format_key(p_name);
	ERR_FAIL_COND_MSG(key.is_empty() || p_write == nullptr || p_parse == nullptr, "A text format needs a name, a write function and a parse function.");
	MutexLock lock(text_formats_mutex);
	for (TextFormat &format : text_formats) {
		if (format.name == key) {
			// The last registration wins, so a module can replace a format.
			format.write = p_write;
			format.parse = p_parse;
			return;
		}
	}
	TextFormat format;
	format.name = key;
	format.write = p_write;
	format.parse = p_parse;
	text_formats.push_back(format);
}

void ResourceTextSerializer::unregister_format(const StringName &p_name) {
	const String key = _format_key(p_name);
	MutexLock lock(text_formats_mutex);
	for (uint32_t i = 0; i < text_formats.size(); i++) {
		if (text_formats[i].name == key) {
			text_formats.remove_at(i);
			return;
		}
	}
}

static Error _parse_json_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	return ResourceTextSerializer::parse_json(p_text, p_type_hint, r_resource, r_error);
}

static Error _parse_ini_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	return ResourceTextSerializer::parse_ini(p_text, p_type_hint, r_resource, r_error);
}

static Error _parse_xml_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	return ResourceTextSerializer::parse_xml(p_text, p_type_hint, r_resource, r_error);
}

void ResourceTextSerializer::register_builtin_formats() {
	register_format("json", &ResourceTextSerializer::write_json, &_parse_json_format);
	register_format("ini", &ResourceTextSerializer::write_ini, &_parse_ini_format);
	register_format("csv", &ResourceTextSerializer::write_csv, &ResourceTextSerializer::parse_csv);
	register_format("xml", &ResourceTextSerializer::write_xml, &_parse_xml_format);
}

void ResourceTextSerializer::clear_formats() {
	MutexLock lock(text_formats_mutex);
	text_formats.clear();
}

Error ResourceTextSerializer::write_format(const StringName &p_format, const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error) {
	TextFormat format;
	if (!_get_text_format(p_format, format)) {
		r_error = vformat("Unknown text format \"%s\". Known formats: %s.", p_format, String(", ").join(_get_text_format_names()));
		return ERR_UNAVAILABLE;
	}
	// Called without the lock: a format may print or load other resources.
	return format.write(p_resource, p_options, r_text, r_error);
}

Error ResourceTextSerializer::parse_format(const StringName &p_format, const String &p_text, const String &p_type_hint, const Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	TextFormat format;
	if (!_get_text_format(p_format, format)) {
		r_error = vformat("Unknown text format \"%s\". Known formats: %s.", p_format, String(", ").join(_get_text_format_names()));
		return ERR_UNAVAILABLE;
	}
	return format.parse(p_text, p_type_hint, p_options, r_resource, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// Bound API.

String ResourceTextSerializer::serialize(const Ref<Resource> &p_resource, const StringName &p_format, const Dictionary &p_options) const {
	String text;
	String error;
	if (write_format(p_format, p_resource, get_options(p_options), text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

Ref<Resource> ResourceTextSerializer::deserialize(const String &p_text, const StringName &p_format, const String &p_type_hint, const Dictionary &p_options) const {
	Ref<Resource> resource;
	String error;
	if (parse_format(p_format, p_text, p_type_hint, get_options(p_options), resource, error) != OK) {
		ERR_PRINT(error);
		return Ref<Resource>();
	}
	return resource;
}

PackedStringArray ResourceTextSerializer::get_formats() const {
	return _get_text_format_names();
}

bool ResourceTextSerializer::has_format(const StringName &p_format) const {
	TextFormat format;
	return _get_text_format(p_format, format);
}
