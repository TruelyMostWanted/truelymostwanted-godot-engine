/**************************************************************************/
/*  flow_graph_data.h                                                     */
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
#include "core/variant/typed_array.h"

// A titled, colored rectangle of the graph editor that groups nodes. Ignored at runtime.
class FlowFrame : public Resource {
	GDCLASS(FlowFrame, Resource);

	String title;
	Rect2 rect;
	Color color = Color(0.3, 0.3, 0.3, 0.75);
	PackedInt32Array node_ids;

protected:
	static void _bind_methods();

public:
	void set_title(const String &p_title);
	String get_title() const { return title; }
	void set_rect(const Rect2 &p_rect);
	Rect2 get_rect() const { return rect; }
	void set_color(const Color &p_color);
	Color get_color() const { return color; }
	void set_node_ids(const PackedInt32Array &p_node_ids);
	PackedInt32Array get_node_ids() const { return node_ids; }
};

// Declaration of a variable of a FlowGraph: name, type and default value. The values live in a FlowBlackboard.
class FlowVariable : public Resource {
	GDCLASS(FlowVariable, Resource);

	StringName variable_name;
	Variant::Type type = Variant::NIL;
	Variant default_value;
	String description;

protected:
	static void _bind_methods();

public:
	void set_variable_name(const StringName &p_name);
	StringName get_variable_name() const { return variable_name; }
	void set_type(Variant::Type p_type);
	Variant::Type get_type() const { return type; }
	void set_default_value(const Variant &p_value);
	Variant get_default_value() const { return default_value; }
	void set_description(const String &p_description);
	String get_description() const { return description; }
};

// Variable values of a running graph. One per playback, or shared between graphs (quest or dialogue state).
class FlowBlackboard : public Resource {
	GDCLASS(FlowBlackboard, Resource);

	Dictionary variables;

protected:
	static void _bind_methods();

public:
	void set_variables(const Dictionary &p_variables);
	Dictionary get_variables() const { return variables; }

	void set_variable(const StringName &p_name, const Variant &p_value);
	Variant get_variable(const StringName &p_name, const Variant &p_default = Variant()) const;
	bool has_variable(const StringName &p_name) const;
	void remove_variable(const StringName &p_name);
	TypedArray<StringName> get_variable_names() const;

	FlowBlackboard();
};
