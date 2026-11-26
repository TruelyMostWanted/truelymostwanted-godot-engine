/**************************************************************************/
/*  flow_graph_data.cpp                                                   */
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

#include "flow_graph_data.h"

#include "flow_port.h"

#include "core/object/class_db.h"

void FlowFrame::set_title(const String &p_title) {
	title = p_title;
	emit_changed();
}

void FlowFrame::set_rect(const Rect2 &p_rect) {
	rect = p_rect;
	emit_changed();
}

void FlowFrame::set_color(const Color &p_color) {
	color = p_color;
	emit_changed();
}

void FlowFrame::set_node_ids(const PackedInt32Array &p_node_ids) {
	node_ids = p_node_ids;
	emit_changed();
}

void FlowFrame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &FlowFrame::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &FlowFrame::get_title);
	ClassDB::bind_method(D_METHOD("set_rect", "rect"), &FlowFrame::set_rect);
	ClassDB::bind_method(D_METHOD("get_rect"), &FlowFrame::get_rect);
	ClassDB::bind_method(D_METHOD("set_color", "color"), &FlowFrame::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &FlowFrame::get_color);
	ClassDB::bind_method(D_METHOD("set_node_ids", "node_ids"), &FlowFrame::set_node_ids);
	ClassDB::bind_method(D_METHOD("get_node_ids"), &FlowFrame::get_node_ids);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "rect", PROPERTY_HINT_NONE, "suffix:px"), "set_rect", "get_rect");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "node_ids"), "set_node_ids", "get_node_ids");
}

void FlowVariable::set_variable_name(const StringName &p_name) {
	variable_name = p_name;
	emit_changed();
}

void FlowVariable::set_type(Variant::Type p_type) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	type = p_type;
	emit_changed();
}

void FlowVariable::set_default_value(const Variant &p_value) {
	default_value = p_value;
	emit_changed();
}

void FlowVariable::set_description(const String &p_description) {
	description = p_description;
	emit_changed();
}

void FlowVariable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_variable_name", "name"), &FlowVariable::set_variable_name);
	ClassDB::bind_method(D_METHOD("get_variable_name"), &FlowVariable::get_variable_name);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &FlowVariable::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &FlowVariable::get_type);
	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &FlowVariable::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &FlowVariable::get_default_value);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &FlowVariable::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &FlowVariable::get_description);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name"), "set_variable_name", "get_variable_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, FlowPort::get_type_hint_string()), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "default_value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), "set_default_value", "get_default_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
}

void FlowBlackboard::set_variables(const Dictionary &p_variables) {
	variables.clear();
	variables.assign(p_variables);
	emit_changed();
}

void FlowBlackboard::set_variable(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND_MSG(p_name.is_empty(), "A variable name can't be empty.");
	variables[p_name] = p_value;
	emit_signal(SNAME("variable_changed"), p_name, p_value);
	emit_changed();
}

Variant FlowBlackboard::get_variable(const StringName &p_name, const Variant &p_default) const {
	return variables.get(p_name, p_default);
}

bool FlowBlackboard::has_variable(const StringName &p_name) const {
	return variables.has(p_name);
}

void FlowBlackboard::remove_variable(const StringName &p_name) {
	if (variables.erase(p_name)) {
		emit_signal(SNAME("variable_changed"), p_name, Variant());
		emit_changed();
	}
}

TypedArray<StringName> FlowBlackboard::get_variable_names() const {
	TypedArray<StringName> result;
	for (const KeyValue<Variant, Variant> &kv : variables) {
		result.push_back(kv.key);
	}
	return result;
}

void FlowBlackboard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_variables", "variables"), &FlowBlackboard::set_variables);
	ClassDB::bind_method(D_METHOD("get_variables"), &FlowBlackboard::get_variables);
	ClassDB::bind_method(D_METHOD("set_variable", "name", "value"), &FlowBlackboard::set_variable);
	ClassDB::bind_method(D_METHOD("get_variable", "name", "default"), &FlowBlackboard::get_variable, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_variable", "name"), &FlowBlackboard::has_variable);
	ClassDB::bind_method(D_METHOD("remove_variable", "name"), &FlowBlackboard::remove_variable);
	ClassDB::bind_method(D_METHOD("get_variable_names"), &FlowBlackboard::get_variable_names);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "variables", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;Variant"), "set_variables", "get_variables");

	ADD_SIGNAL(MethodInfo("variable_changed", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
}

FlowBlackboard::FlowBlackboard() {
	variables.set_typed(Variant::STRING_NAME, StringName(), Variant(), Variant::NIL, StringName(), Variant());
}
