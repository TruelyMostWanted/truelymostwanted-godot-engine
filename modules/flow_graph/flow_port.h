/**************************************************************************/
/*  flow_port.h                                                           */
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
#include "core/variant/binder_common.h"

// A named input or output of a FlowNode. Flow ports carry the execution, data ports carry values.
class FlowPort : public Resource {
	GDCLASS(FlowPort, Resource);

public:
	enum PortKind {
		PORT_KIND_FLOW,
		PORT_KIND_DATA,
		PORT_KIND_MAX,
	};

private:
	StringName port_name;
	PortKind kind = PORT_KIND_FLOW;
	Variant::Type type = Variant::NIL;
	StringName class_hint;
	Variant default_value;
	String label;

protected:
	static void _bind_methods();

public:
	// Property hint of a Variant type selection where TYPE_NIL reads "Any".
	static String get_type_hint_string();
	// Whether a data value of type p_from can flow into an input of type p_to.
	static bool is_type_compatible(Variant::Type p_from, Variant::Type p_to);

	static Ref<FlowPort> create_flow(const StringName &p_name, const String &p_label = String());
	static Ref<FlowPort> create_data(const StringName &p_name, Variant::Type p_type, const Variant &p_default_value = Variant(), const String &p_label = String());

	void set_port_name(const StringName &p_name);
	StringName get_port_name() const { return port_name; }
	void set_kind(PortKind p_kind);
	PortKind get_kind() const { return kind; }
	void set_type(Variant::Type p_type);
	Variant::Type get_type() const { return type; }
	void set_class_hint(const StringName &p_class_hint);
	StringName get_class_hint() const { return class_hint; }
	void set_default_value(const Variant &p_value);
	Variant get_default_value() const { return default_value; }
	void set_label(const String &p_label);
	String get_label() const { return label; }
};

// Read-only view of one connection of a FlowGraph, built on demand by the graph.
class FlowConnection : public RefCounted {
	GDCLASS(FlowConnection, RefCounted);

	int from_node_id = -1;
	StringName from_port;
	int to_node_id = -1;
	StringName to_port;
	FlowPort::PortKind kind = FlowPort::PORT_KIND_FLOW;

protected:
	static void _bind_methods();

public:
	static Ref<FlowConnection> create(int p_from_node_id, const StringName &p_from_port, int p_to_node_id, const StringName &p_to_port, FlowPort::PortKind p_kind);

	int get_from_node_id() const { return from_node_id; }
	StringName get_from_port() const { return from_port; }
	int get_to_node_id() const { return to_node_id; }
	StringName get_to_port() const { return to_port; }
	FlowPort::PortKind get_kind() const { return kind; }
};

VARIANT_ENUM_CAST(FlowPort::PortKind);
