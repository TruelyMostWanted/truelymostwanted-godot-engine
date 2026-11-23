/**************************************************************************/
/*  xml_writer.cpp                                                        */
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

#include "xml_writer.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"

bool XMLWriter::is_valid_name(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		const bool start_char = is_ascii_alphabet_char(c) || c == '_' || c == ':' || c > 127;
		if (i == 0) {
			if (!start_char) {
				return false;
			}
		} else if (!start_char && !is_digit(c) && c != '-' && c != '.') {
			return false;
		}
	}
	return true;
}

String XMLWriter::escape_text(const String &p_text) {
	return p_text.xml_escape(false).replace("\r", "&#13;");
}

String XMLWriter::escape_attribute(const String &p_value) {
	return p_value.xml_escape(true).replace("\n", "&#10;").replace("\r", "&#13;").replace("\t", "&#9;");
}

void XMLWriter::set_indent(const String &p_indent) {
	indent = p_indent;
}

String XMLWriter::get_indent() const {
	return indent;
}

void XMLWriter::_close_start_tag() {
	if (start_tag_open) {
		output += ">";
		start_tag_open = false;
	}
}

// Starts a child node (element or comment) of the current element, on its own line unless the element has text.
void XMLWriter::_begin_node() {
	_close_start_tag();
	const bool inline_node = !stack.is_empty() && stack[stack.size() - 1].has_text;
	if (!stack.is_empty()) {
		stack[stack.size() - 1].has_children = true;
	}
	if (inline_node || indent.is_empty()) {
		return;
	}
	if (!output.is_empty() && !output.ends_with("\n")) {
		output += "\n";
	}
	output += indent.repeat(stack.size());
}

Error XMLWriter::write_declaration(const String &p_version, const String &p_encoding) {
	ERR_FAIL_COND_V_MSG(!output.is_empty(), ERR_ALREADY_EXISTS, "The XML declaration must come first.");
	output += "<?xml version=\"" + escape_attribute(p_version) + "\"";
	if (!p_encoding.is_empty()) {
		output += " encoding=\"" + escape_attribute(p_encoding) + "\"";
	}
	output += "?>";
	if (!indent.is_empty()) {
		output += "\n";
	}
	return OK;
}

Error XMLWriter::begin_element(const String &p_name) {
	ERR_FAIL_COND_V_MSG(!is_valid_name(p_name), ERR_INVALID_PARAMETER, vformat("'%s' is not a valid XML element name.", p_name));
	ERR_FAIL_COND_V_MSG(stack.is_empty() && root_closed, ERR_ALREADY_EXISTS, "An XML document has only one root element.");
	_begin_node();
	output += "<" + p_name;
	Element element;
	element.name = p_name;
	stack.push_back(element);
	start_tag_open = true;
	return OK;
}

Error XMLWriter::add_attribute(const String &p_name, const String &p_value) {
	ERR_FAIL_COND_V_MSG(!start_tag_open, ERR_UNCONFIGURED, "Attributes must be added right after begin_element(), before any content.");
	ERR_FAIL_COND_V_MSG(!is_valid_name(p_name), ERR_INVALID_PARAMETER, vformat("'%s' is not a valid XML attribute name.", p_name));
	Element &element = stack[stack.size() - 1];
	ERR_FAIL_COND_V_MSG(element.attributes.has(p_name), ERR_ALREADY_EXISTS, vformat("The attribute '%s' is already set.", p_name));
	element.attributes.push_back(p_name);
	output += " " + p_name + "=\"" + escape_attribute(p_value) + "\"";
	return OK;
}

Error XMLWriter::write_text(const String &p_text) {
	ERR_FAIL_COND_V_MSG(stack.is_empty(), ERR_UNCONFIGURED, "Text must be inside an element.");
	_close_start_tag();
	output += escape_text(p_text);
	stack[stack.size() - 1].has_text = true;
	return OK;
}

Error XMLWriter::write_cdata(const String &p_text) {
	ERR_FAIL_COND_V_MSG(stack.is_empty(), ERR_UNCONFIGURED, "CDATA must be inside an element.");
	_close_start_tag();
	// "]]>" can't appear inside CDATA: split it over two sections.
	output += "<![CDATA[" + p_text.replace("]]>", "]]]]><![CDATA[>") + "]]>";
	stack[stack.size() - 1].has_text = true;
	return OK;
}

Error XMLWriter::write_comment(const String &p_text) {
	ERR_FAIL_COND_V_MSG(p_text.contains("--") || p_text.ends_with("-"), ERR_INVALID_PARAMETER, "An XML comment can't contain \"--\" or end with \"-\".");
	_begin_node();
	output += "<!--" + p_text + "-->";
	return OK;
}

Error XMLWriter::write_element(const String &p_name, const String &p_text) {
	const Error err = begin_element(p_name);
	if (err != OK) {
		return err;
	}
	if (!p_text.is_empty()) {
		write_text(p_text);
	}
	return end_element();
}

Error XMLWriter::end_element() {
	ERR_FAIL_COND_V_MSG(stack.is_empty(), ERR_DOES_NOT_EXIST, "There is no open element to end.");
	const Element element = stack[stack.size() - 1];
	stack.resize(stack.size() - 1);
	if (start_tag_open) {
		output += "/>";
		start_tag_open = false;
	} else {
		if (element.has_children && !element.has_text && !indent.is_empty()) {
			output += "\n" + indent.repeat(stack.size());
		}
		output += "</" + element.name + ">";
	}
	if (stack.is_empty()) {
		root_closed = true;
		if (!indent.is_empty()) {
			output += "\n";
		}
	}
	return OK;
}

Error XMLWriter::end_all() {
	while (!stack.is_empty()) {
		const Error err = end_element();
		if (err != OK) {
			return err;
		}
	}
	return OK;
}

int XMLWriter::get_depth() const {
	return stack.size();
}

String XMLWriter::get_output() const {
	if (start_tag_open) {
		// Show the open start tag as it will be written.
		return output + ">";
	}
	return output;
}

PackedByteArray XMLWriter::get_output_buffer() const {
	return get_output().to_utf8_buffer();
}

Error XMLWriter::save(const String &p_path) const {
	ERR_FAIL_COND_V_MSG(!stack.is_empty(), ERR_UNCONFIGURED, vformat("Can't save '%s': %d element(s) are still open; call end_all() first.", p_path, int(stack.size())));
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't open the file '%s' for writing.", p_path));
	file->store_string(output);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

void XMLWriter::clear() {
	output = String();
	stack.clear();
	start_tag_open = false;
	root_closed = false;
}

void XMLWriter::_bind_methods() {
	ClassDB::bind_static_method("XMLWriter", D_METHOD("is_valid_name", "name"), &XMLWriter::is_valid_name);
	ClassDB::bind_static_method("XMLWriter", D_METHOD("escape_text", "text"), &XMLWriter::escape_text);
	ClassDB::bind_static_method("XMLWriter", D_METHOD("escape_attribute", "value"), &XMLWriter::escape_attribute);

	ClassDB::bind_method(D_METHOD("set_indent", "indent"), &XMLWriter::set_indent);
	ClassDB::bind_method(D_METHOD("get_indent"), &XMLWriter::get_indent);

	ClassDB::bind_method(D_METHOD("write_declaration", "version", "encoding"), &XMLWriter::write_declaration, DEFVAL("1.0"), DEFVAL("UTF-8"));
	ClassDB::bind_method(D_METHOD("begin_element", "name"), &XMLWriter::begin_element);
	ClassDB::bind_method(D_METHOD("add_attribute", "name", "value"), &XMLWriter::add_attribute);
	ClassDB::bind_method(D_METHOD("write_text", "text"), &XMLWriter::write_text);
	ClassDB::bind_method(D_METHOD("write_cdata", "text"), &XMLWriter::write_cdata);
	ClassDB::bind_method(D_METHOD("write_comment", "text"), &XMLWriter::write_comment);
	ClassDB::bind_method(D_METHOD("write_element", "name", "text"), &XMLWriter::write_element, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("end_element"), &XMLWriter::end_element);
	ClassDB::bind_method(D_METHOD("end_all"), &XMLWriter::end_all);

	ClassDB::bind_method(D_METHOD("get_depth"), &XMLWriter::get_depth);
	ClassDB::bind_method(D_METHOD("get_output"), &XMLWriter::get_output);
	ClassDB::bind_method(D_METHOD("get_output_buffer"), &XMLWriter::get_output_buffer);
	ClassDB::bind_method(D_METHOD("save", "path"), &XMLWriter::save);
	ClassDB::bind_method(D_METHOD("clear"), &XMLWriter::clear);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "indent"), "set_indent", "get_indent");
}
