/**************************************************************************/
/*  yaml.h                                                                */
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
#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/variant/variant.h"

// Reads and writes the YAML 1.2 subset that maps to Variant (https://yaml.org/spec/1.2.2/), and any Resource
// as YAML on top of the property mapping of ResourceTextSerializer (format FORMAT_YAML).
// Not supported (rejected with an error): anchors and aliases, merge keys, tags other than !!str, !!int,
// !!float, !!bool and !!null and complex keys.
// As a Resource (TextDataFormat), `data` is the tree of the first document; further documents ("---") are
// kept beside it (get_document()). Values are addressed by paths of keys and array indices separated by "/"
// ("enemies/0/name"); in a key, "~" is written as "~0" and "/" as "~1" (JSON Pointer, RFC 6901).
class YAML : public TextDataFormat {
	GDCLASS(YAML, TextDataFormat);

	Array extra_documents; // Documents after the first one (`data`).

	// Finds the container that holds the last part of the path. With p_create, missing dictionaries are created.
	bool _resolve_parent(const Vector<String> &p_parts, bool p_create, Variant &r_parent) const;
	static bool _get_child(const Variant &p_node, const String &p_part, Variant &r_child);

protected:
	static void _bind_methods();

	virtual Error _parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) override;
	virtual Error _encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const override;
	virtual void _get_extensions(List<String> *r_extensions) const override;
	virtual void _cleared() override { extra_documents.clear(); }

public:
	// Parses YAML into a tree of Dictionary, Array, String, int, float, bool and null, in file order; r_data is
	// the first document, r_documents (if given) gets all documents. Mapping keys are always strings. On error,
	// r_line and r_column (1-based) point at the problem.
	static Error parse_text(const String &p_text, Variant &r_data, String &r_error, int &r_line, int &r_column, Array *r_documents = nullptr);
	// Writes a tree as YAML: block style with `indent` spaces, or flow style on one line if `indent` is empty.
	static Error write_text(const Variant &p_data, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error);

	// Resource type in the key "$type" of the root mapping, or an empty string.
	static String get_marker_type(const String &p_text);
	static Error write_resource(const Ref<Resource> &p_resource, const ResourceTextSerializer::Options &p_options, String &r_text, String &r_error);
	// Returns ERR_FILE_UNRECOGNIZED if the text is not a YAML resource file and no type hint is given.
	static Error parse_resource(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);
	// The parse function registered for ResourceTextSerializer::FORMAT_YAML.
	static Error parse_format(const String &p_text, const String &p_type_hint, const ResourceTextSerializer::Options &p_options, Ref<Resource> &r_resource, String &r_error);

	// Splits a path ("a/b~1c/0" -> ["a", "b/c", "0"]); an empty path is the whole document.
	static Vector<String> split_path(const String &p_path);
	static String escape_key(const String &p_key);

	Variant get_value(const String &p_path, const Variant &p_default = Variant()) const;
	void set_value(const String &p_path, const Variant &p_value);
	bool has_value(const String &p_path) const;
	void erase_value(const String &p_path);

	int get_document_count() const { return 1 + extra_documents.size(); }
	Variant get_document(int p_index) const;
	void add_document(const Variant &p_data);
	void remove_document(int p_index);

	static Variant parse_string(const String &p_text);
	static String stringify(const Variant &p_data, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());

	YAML();
};
