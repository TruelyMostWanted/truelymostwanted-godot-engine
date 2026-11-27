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

#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class XML;

// A view of one element of an XML document tree. It holds the element Dictionary of the tree by reference,
// so changes go straight into the tree of the XML resource. Views are created on demand; they are not stored.
class XMLElement : public RefCounted {
	GDCLASS(XMLElement, RefCounted);

	Dictionary element;
	ObjectID owner_id;

	void _changed();
	Array _get_children_array();
	void _find(const Vector<String> &p_segments, int p_index, bool p_first, TypedArray<XMLElement> &r_found) const;

protected:
	static void _bind_methods();

public:
	static Ref<XMLElement> create_view(const Dictionary &p_element, ObjectID p_owner);
	Dictionary get_element_dictionary() const { return element; }

	void set_name(const String &p_name);
	String get_name() const;
	void set_text(const String &p_text);
	String get_text() const;

	String get_attribute(const String &p_name, const String &p_default = String()) const;
	void set_attribute(const String &p_name, const String &p_value);
	bool has_attribute(const String &p_name) const;
	void remove_attribute(const String &p_name);
	Dictionary get_attributes() const;

	TypedArray<XMLElement> get_children() const;
	void add_child(const Ref<XMLElement> &p_element);
	void remove_child(const Ref<XMLElement> &p_element);
	TypedArray<XMLElement> get_children_by_name(const String &p_name) const;
	// Paths are element names separated by "/", relative to this element ("items/item").
	Ref<XMLElement> find_first(const String &p_path) const;
	TypedArray<XMLElement> find_all(const String &p_path) const;

	XMLElement();
};

// An XML file as a Resource: `data` is the document tree, the element Dictionary of the root element with:
// - "name": the tag name (String),
// - "attributes": Dictionary of attribute name -> value (String),
// - "children": Array of element dictionaries and text nodes (String), in document order,
// - "text": the text of the direct children joined (String; read only, stringify() uses it without "children").
// get_root() returns an XMLElement view of that tree. The static functions also read and write any Resource as
// XML (format FORMAT_XML of ResourceTextSerializer).
class XML : public TextDataFormat {
	GDCLASS(XML, TextDataFormat);

	Ref<XMLElement> root_view;

protected:
	static void _bind_methods();

	virtual Error _parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) override;
	virtual Error _encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const override;
	virtual Variant _get_empty_data() const override { return Dictionary(); }
	virtual Variant::Type _get_data_type() const override { return Variant::DICTIONARY; }
	virtual void _get_extensions(List<String> *r_extensions) const override;
	virtual void _data_replaced() override { root_view.unref(); }
	virtual Ref<DataValidation> _validate_data(const Ref<DataSchema> &p_schema, const Ref<DataValidationOptions> &p_options) const override;

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

	// Called by XMLElement views after they changed the tree.
	void notify_tree_changed() { _mark_data_changed(); }

	Ref<XMLElement> get_root();
	Ref<XMLElement> create_root(const String &p_name);
	// Paths are element names separated by "/", relative to the root element.
	Ref<XMLElement> find_first(const String &p_path);
	TypedArray<XMLElement> find_all(const String &p_path);

	static Dictionary parse_string(const String &p_text);
	static String stringify(const Dictionary &p_data, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());

	XML();
};
