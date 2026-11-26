/**************************************************************************/
/*  text_insertion_values.cpp                                             */
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

#include "text_insertion_values.h"

#include "core/object/class_db.h"

void TextInsertionValues::set_values(const Dictionary &p_values) {
	values.clear();
	for (const KeyValue<Variant, Variant> &E : p_values) {
		values.insert(E.key.operator StringName(), E.value);
	}
	emit_changed();
}

Dictionary TextInsertionValues::get_values() const {
	Dictionary result;
	result.set_typed(Variant::STRING_NAME, StringName(), Variant(), Variant::NIL, StringName(), Variant());
	for (const KeyValue<StringName, Variant> &E : values) {
		result[E.key] = E.value;
	}
	return result;
}

void TextInsertionValues::set_priority(int p_priority) {
	priority = p_priority;
	emit_changed();
}

void TextInsertionValues::set_value(const StringName &p_name, const Variant &p_value) {
	values.insert(p_name, p_value);
	emit_changed();
}

Variant TextInsertionValues::get_value(const StringName &p_name, const Variant &p_default) const {
	const Variant *value = values.getptr(p_name);
	return value ? *value : p_default;
}

void TextInsertionValues::remove_value(const StringName &p_name) {
	if (values.erase(p_name)) {
		emit_changed();
	}
}

void TextInsertionValues::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_values", "values"), &TextInsertionValues::set_values);
	ClassDB::bind_method(D_METHOD("get_values"), &TextInsertionValues::get_values);
	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &TextInsertionValues::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &TextInsertionValues::get_priority);
	ClassDB::bind_method(D_METHOD("set_value", "name", "value"), &TextInsertionValues::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "name", "default"), &TextInsertionValues::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "name"), &TextInsertionValues::has_value);
	ClassDB::bind_method(D_METHOD("remove_value", "name"), &TextInsertionValues::remove_value);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "values", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;Variant"), "set_values", "get_values");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority"), "set_priority", "get_priority");
}
