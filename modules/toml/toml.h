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
#include "core/io/text_data_format.h"
#include "core/variant/dictionary.h"

// Reads and writes TOML 1.0 (https://toml.io/en/v1.0.0) as a Dictionary tree, and any Resource as TOML
// on top of the property mapping of ResourceTextSerializer. As a Resource (TextDataFormat), `data` is the
// Dictionary tree of one file; tables are addressed by dotted paths ("server.limits"). Inside a table
// name, "." and the backslash are escaped with a backslash (see escape_table_name()).
class TOML : public TextDataFormat {
	GDCLASS(TOML, TextDataFormat);

	static bool _resolve_table(Dictionary p_root, const String &p_path, bool p_create, Dictionary &r_table);
	static void _collect_tables(const Dictionary &p_table, const String &p_prefix, PackedStringArray &r_paths);

protected:
	static void _bind_methods();

	virtual Error _parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) override;
	virtual Error _encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const override;
	virtual Variant _get_empty_data() const override { return Dictionary(); }
	virtual Variant::Type _get_data_type() const override { return Variant::DICTIONARY; }
	virtual void _get_extensions(List<String> *r_extensions) const override { r_extensions->push_back("toml"); }

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

	// Splits a dotted table path ("a.b\\.c" -> ["a", "b.c"]); an empty path is the root table.
	static Vector<String> split_table_path(const String &p_path);
	static String escape_table_name(const String &p_name);

	void set_value(const String &p_table_path, const String &p_key, const Variant &p_value);
	Variant get_value(const String &p_table_path, const String &p_key, const Variant &p_default = Variant()) const;
	bool has_table(const String &p_table_path) const;
	bool has_table_key(const String &p_table_path, const String &p_key) const;
	PackedStringArray get_tables() const;
	PackedStringArray get_table_keys(const String &p_table_path) const;
	void erase_table(const String &p_table_path);
	void erase_table_key(const String &p_table_path, const String &p_key);

	static Dictionary parse_string(const String &p_text);
	static String stringify(const Dictionary &p_data);
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());

	TOML();
};
