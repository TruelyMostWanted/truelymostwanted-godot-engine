/**************************************************************************/
/*  toml.h                                                                */
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
#include "core/io/resource_text_serializer.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

// Reads and writes TOML 1.0 (https://toml.io/en/v1.0.0) as a Dictionary tree, and any Resource as TOML
// on top of the property mapping of ResourceTextSerializer.
class TOML : public RefCounted {
	GDCLASS(TOML, RefCounted);

	Dictionary data;
	String error_message;
	int error_line = 0;
	int error_column = 0;

protected:
	static void _bind_methods();

public:
	// Parses a TOML document into a tree of Dictionary, Array, String, int, float and bool, in file order.
	// Dates and times are returned as strings. On error, r_line and r_column (1-based) point at the problem.
	static Error parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line, int &r_column);
	// Writes a tree as TOML. Keys keep their order; tables and arrays of tables follow the plain keys of their parent.
	static Error write_text(const Dictionary &p_data, String &r_text, String &r_error);

	// Resource type in the key "$type" of the document, or an empty string.
	static String get_marker_type(const String &p_text);
	static Error write_resource(const Ref<Resource> &p_resource, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error);
	// Returns ERR_FILE_UNRECOGNIZED if the text is not a TOML resource file and no type hint is given.
	static Error parse_resource(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);

	// Bound API.
	Error parse(const String &p_text);
	Dictionary get_data() const { return data; }
	String get_error_message() const { return error_message; }
	int get_error_line() const { return error_line; }
	int get_error_column() const { return error_column; }

	static Dictionary parse_string(const String &p_text);
	static String stringify(const Dictionary &p_data);
	static String resource_to_toml(const Ref<Resource> &p_resource, const Dictionary &p_options = Dictionary());
	static Ref<Resource> toml_to_resource(const String &p_text, const String &p_type_hint = String());
};
