/**************************************************************************/
/*  text_insertion_values.h                                               */
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
#include "core/templates/hash_map.h"

// A set of named values for TextInsertionServer placeholders, edited in the inspector and saved as a resource.
class TextInsertionValues : public Resource {
	GDCLASS(TextInsertionValues, Resource);

	HashMap<StringName, Variant> values;
	int priority = 0;

protected:
	static void _bind_methods();

public:
	void set_values(const Dictionary &p_values);
	Dictionary get_values() const;

	void set_priority(int p_priority);
	int get_priority() const { return priority; }

	void set_value(const StringName &p_name, const Variant &p_value);
	Variant get_value(const StringName &p_name, const Variant &p_default = Variant()) const;
	bool has_value(const StringName &p_name) const { return values.has(p_name); }
	void remove_value(const StringName &p_name);

	const Variant *get_value_ptr(const StringName &p_name) const { return values.getptr(p_name); }
};
