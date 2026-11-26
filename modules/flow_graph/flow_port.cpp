/**************************************************************************/
/*  flow_port.cpp                                                         */
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

#include "flow_port.h"

#include "core/object/class_db.h"

String FlowPort::get_type_hint_string() {
	String hint = "Any";
	for (int i = 1; i < Variant::VARIANT_MAX; i++) {
		hint += "," + Variant::get_type_name(Variant::Type(i));
	}
	return hint;
}

bool FlowPort::is_type_compatible(Variant::Type p_from, Variant::Type p_to) {
	return p_from == p_to || p_from == Variant::NIL || p_to == Variant::NIL || (p_from == Variant::INT && p_to == Variant::FLOAT);
}

Ref<FlowPort> FlowPort::create_flow(const StringName &p_name, const String &p_label) {
	Ref<FlowPort> port;
	port.instantiate();
	port->port_name = p_name;
	port->label = p_label;
	return port;
}

Ref<FlowPort> FlowPort::create_data(const StringName &p_name, Variant::Type p_type, const Variant &p_default_value, const String &p_label) {
	Ref<FlowPort> port = create_flow(p_name, p_label);
	port->kind = PORT_KIND_DATA;
	port->type = p_type;
	port->default_value = p_default_value;
	return port;
}

void FlowPort::set_port_name(const StringName &p_name) {
	port_name = p_name;
	emit_changed();
}

void FlowPort::set_kind(PortKind p_kind) {
	ERR_FAIL_INDEX(p_kind, PORT_KIND_MAX);
	kind = p_kind;
	emit_changed();
}

void FlowPort::set_type(Variant::Type p_type) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	type = p_type;
	emit_changed();
}

void FlowPort::set_class_hint(const StringName &p_class_hint) {
	class_hint = p_class_hint;
	emit_changed();
}

void FlowPort::set_default_value(const Variant &p_value) {
	default_value = p_value;
	emit_changed();
}

void FlowPort::set_label(const String &p_label) {
	label = p_label;
	emit_changed();
}

void FlowPort::_bind_methods() {
	ClassDB::bind_static_method("FlowPort", D_METHOD("create_flow", "name", "label"), &FlowPort::create_flow, DEFVAL(String()));
	ClassDB::bind_static_method("FlowPort", D_METHOD("create_data", "name", "type", "default_value", "label"), &FlowPort::create_data, DEFVAL(Variant()), DEFVAL(String()));

	ClassDB::bind_method(D_METHOD("set_port_name", "name"), &FlowPort::set_port_name);
	ClassDB::bind_method(D_METHOD("get_port_name"), &FlowPort::get_port_name);
	ClassDB::bind_method(D_METHOD("set_kind", "kind"), &FlowPort::set_kind);
	ClassDB::bind_method(D_METHOD("get_kind"), &FlowPort::get_kind);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &FlowPort::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &FlowPort::get_type);
	ClassDB::bind_method(D_METHOD("set_class_hint", "class_hint"), &FlowPort::set_class_hint);
	ClassDB::bind_method(D_METHOD("get_class_hint"), &FlowPort::get_class_hint);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &FlowPort::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &FlowPort::get_default_value);
	ClassDB::bind_method(D_METHOD("set_label", "label"), &FlowPort::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &FlowPort::get_label);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name"), "set_port_name", "get_port_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "kind", PROPERTY_HINT_ENUM, "Flow,Data"), "set_kind", "get_kind");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, get_type_hint_string()), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "class_hint"), "set_class_hint", "get_class_hint");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "default_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_default_value", "get_default_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");

	BIND_ENUM_CONSTANT(PORT_KIND_FLOW);
	BIND_ENUM_CONSTANT(PORT_KIND_DATA);
	BIND_ENUM_CONSTANT(PORT_KIND_MAX);
}

Ref<FlowConnection> FlowConnection::create(int p_from_node_id, const StringName &p_from_port, int p_to_node_id, const StringName &p_to_port, FlowPort::PortKind p_kind) {
	Ref<FlowConnection> connection;
	connection.instantiate();
	connection->from_node_id = p_from_node_id;
	connection->from_port = p_from_port;
	connection->to_node_id = p_to_node_id;
	connection->to_port = p_to_port;
	connection->kind = p_kind;
	return connection;
}

void FlowConnection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_from_node_id"), &FlowConnection::get_from_node_id);
	ClassDB::bind_method(D_METHOD("get_from_port"), &FlowConnection::get_from_port);
	ClassDB::bind_method(D_METHOD("get_to_node_id"), &FlowConnection::get_to_node_id);
	ClassDB::bind_method(D_METHOD("get_to_port"), &FlowConnection::get_to_port);
	ClassDB::bind_method(D_METHOD("get_kind"), &FlowConnection::get_kind);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "from_node_id"), "", "get_from_node_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "from_port"), "", "get_from_port");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "to_node_id"), "", "get_to_node_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "to_port"), "", "get_to_port");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "kind", PROPERTY_HINT_ENUM, "Flow,Data"), "", "get_kind");
}
