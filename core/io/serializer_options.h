/**************************************************************************/
/*  serializer_options.h                                                  */
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

// Typed options of the text formats of ResourceTextSerializer (JSON, INI, CSV, XML, TOML).
// A Resource, so a set of options can be edited in the Inspector and saved as a preset (.tres).
// Formats registered by modules or extensions subclass it for their own options.
class SerializerOptions : public Resource {
	GDCLASS(SerializerOptions, Resource);

	String indent = "  ";
	bool enums_as_names = true;
	Ref<Resource> defaults;
	bool include_type_marker = true;
	bool sort_keys = false;
	bool full_precision_floats = false;
	String csv_delimiter = ",";
	bool csv_headers = true;
	bool ini_plain_values = false;
	bool xml_declaration = true;

protected:
	static void _bind_methods();

public:
	void set_indent(const String &p_indent);
	String get_indent() const;

	void set_enums_as_names(bool p_enable);
	bool is_enums_as_names() const;

	void set_defaults(const Ref<Resource> &p_defaults);
	Ref<Resource> get_defaults() const;

	void set_include_type_marker(bool p_enable);
	bool is_include_type_marker() const;

	void set_sort_keys(bool p_enable);
	bool is_sort_keys() const;

	void set_full_precision_floats(bool p_enable);
	bool is_full_precision_floats() const;

	void set_csv_delimiter(const String &p_delimiter);
	String get_csv_delimiter() const;

	void set_csv_headers(bool p_enable);
	bool is_csv_headers() const;

	void set_ini_plain_values(bool p_enable);
	bool is_ini_plain_values() const;

	void set_xml_declaration(bool p_enable);
	bool is_xml_declaration() const;
};
