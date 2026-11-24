/**************************************************************************/
/*  ini_format.h                                                          */
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

// Reads and writes INI text as a Dictionary of sections (section name -> Dictionary of key -> value), and any
// Resource as INI (format FORMAT_INI of ResourceTextSerializer). Keys before the first section belong to the
// section "". Values use Godot Variant syntax (strings quoted), or plain text with SerializerOptions.ini_plain_values.
class INI : public RefCounted {
	GDCLASS(INI, RefCounted);

	Dictionary data;
	String error_message;
	int error_line = 0;

protected:
	static void _bind_methods();

public:
	// On error, r_line is the 1-based line of the problem. Repeated sections are merged.
	static Error parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line);
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
