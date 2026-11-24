/**************************************************************************/
/*  xml_writer.h                                                          */
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

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/templates/vector.h"

// Writes XML text element by element: the counterpart of XMLParser.
class XMLWriter : public RefCounted {
	GDCLASS(XMLWriter, RefCounted);

	struct Element {
		String name;
		Vector<String> attributes;
		bool has_children = false;
		bool has_text = false;
	};

	String output;
	LocalVector<Element> stack;
	String indent = "  ";
	bool start_tag_open = false;
	bool root_closed = false;

	void _close_start_tag();
	void _begin_node();

protected:
	static void _bind_methods();

public:
	static bool is_valid_name(const String &p_name);
	static String escape_text(const String &p_text);
	static String escape_attribute(const String &p_value);

	void set_indent(const String &p_indent);
	String get_indent() const;

	Error write_declaration(const String &p_version = "1.0", const String &p_encoding = "UTF-8");
	Error begin_element(const String &p_name);
	Error add_attribute(const String &p_name, const String &p_value);
	Error write_text(const String &p_text);
	Error write_cdata(const String &p_text);
	Error write_comment(const String &p_text);
	Error write_element(const String &p_name, const String &p_text = String());
	Error end_element();
	Error end_all();

	int get_depth() const;
	String get_output() const;
	PackedByteArray get_output_buffer() const;
	Error save(const String &p_path) const;
	void clear();
};
