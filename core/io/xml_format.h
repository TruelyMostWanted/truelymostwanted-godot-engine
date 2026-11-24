/**************************************************************************/
/*  xml_format.h                                                          */
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

#include "core/io/resource.h"
#include "core/io/serializer_options.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

// Reads and writes XML as a tree of element dictionaries, and any Resource as XML (format FORMAT_XML of
// ResourceTextSerializer). An element is a Dictionary with:
// - "name": the tag name (String),
// - "attributes": Dictionary of attribute name -> value (String),
// - "children": Array of element dictionaries and text nodes (String), in document order,
// - "text": the text of the direct children joined (String; read only, stringify() uses it without "children").
class XML : public RefCounted {
	GDCLASS(XML, RefCounted);

	Dictionary data;
	String error_message;
	int error_line = 0;

protected:
	static void _bind_methods();

public:
	static constexpr const char *KEY_NAME = "name";
	static constexpr const char *KEY_ATTRIBUTES = "attributes";
	static constexpr const char *KEY_CHILDREN = "children";
	static constexpr const char *KEY_TEXT = "text";

	// Parses an XML document into the element dictionary of its root element. Whitespace-only text between
	// elements is dropped, CDATA is kept as text. Comments, processing instructions and the declaration are
	// not kept. On error, r_line is the 1-based line of the problem.
	static Error parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line);
	// Writes an element dictionary as an XML document.
	static Error write_text(const Dictionary &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error);

	// Bound API.
	Error parse(const String &p_text);
	Dictionary get_data() const { return data; }
	String get_error_message() const { return error_message; }
	int get_error_line() const { return error_line; }

	static Dictionary parse_string(const String &p_text);
	static String stringify(const Dictionary &p_data, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
};
