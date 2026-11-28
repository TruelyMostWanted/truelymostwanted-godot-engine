/**************************************************************************/
/*  data_schema.cpp                                                       */
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

#include "data_schema.h"

#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

static bool _data_schema_is_empty(const Variant &p_value) {
	const Variant::Type type = p_value.get_type();
	return type == Variant::NIL || ((type == Variant::STRING || type == Variant::STRING_NAME) && String(p_value).is_empty());
}

// Reports a repeated value of a unique field. `r_seen` maps typed values to the first row or item.
static void _data_schema_check_unique(HashMap<Variant, int> &r_seen, const Variant &p_typed, int p_index, const StringName &p_name, const String &p_path, int p_row, int p_column, DataValidation *r_validation) {
	if (_data_schema_is_empty(p_typed)) {
		return;
	}
	const int *first = r_seen.getptr(p_typed);
	if (first) {
		r_validation->add_issue(DataValidation::ISSUE_KIND_DUPLICATE_KEY, vformat("\"%s\": the value \"%s\" is already used by entry %d.", p_name, String(p_typed), *first), p_name, p_path, p_row, p_column);
	} else {
		r_seen.insert(p_typed, p_index);
	}
}

const DataField *DataSchema::_get_field_ptr(int p_index) const {
	return Object::cast_to<DataField>((Object *)fields[p_index]);
}

bool DataSchema::_check_value(const DataField *p_field, const StringName &p_name, const Variant &p_value, const String &p_parent_path, int p_row, int p_column, DataValidation *r_validation, Variant &r_typed) {
	if (_data_schema_is_empty(p_value)) {
		if (!p_field->is_allow_empty()) {
			r_validation->add_issue(DataValidation::ISSUE_KIND_EMPTY_REQUIRED, vformat("\"%s\" must not be empty.", p_name), p_name, DataValidation::join_path(p_parent_path, p_name), p_row, p_column);
		}
		r_typed = p_field->get_default_value();
		return false;
	}
	if (!DataField::coerce(p_field->get_value_type(), p_value, r_typed)) {
		r_validation->add_issue(DataValidation::ISSUE_KIND_TYPE_MISMATCH, vformat("\"%s\" must be of type %s, not \"%s\".", p_name, Variant::get_type_name(p_field->get_value_type()), String(p_value)), p_name, DataValidation::join_path(p_parent_path, p_name), p_row, p_column);
		return false;
	}
	String message;
	const DataValidation::IssueKind kind = p_field->check_hint(r_typed, message);
	if (kind != DataValidation::ISSUE_KIND_MAX) {
		r_validation->add_issue(kind, vformat("\"%s\": %s", p_name, message), p_name, DataValidation::join_path(p_parent_path, p_name), p_row, p_column);
	}

	// Nested data: a Dictionary with `schema`, an Array with `element_schema` or `element_type`.
	if (r_typed.get_type() == Variant::DICTIONARY && p_field->get_schema().is_valid()) {
		p_field->get_schema()->_validate_dictionary(r_typed, DataValidation::join_path(p_parent_path, p_name), p_row, r_validation);
	} else if (r_typed.get_type() == Variant::ARRAY) {
		const String path = DataValidation::join_path(p_parent_path, p_name);
		const Ref<DataSchema> element_schema = p_field->get_element_schema();
		if (element_schema.is_valid()) {
			element_schema->_validate_records(r_typed, path, p_row, r_validation);
		} else if (p_field->get_element_type() != Variant::NIL) {
			const Array elements = r_typed;
			Variant element;
			for (int i = 0; i < elements.size() && !r_validation->is_full(); i++) {
				if (!DataField::coerce(p_field->get_element_type(), elements[i], element)) {
					r_validation->add_issue(DataValidation::ISSUE_KIND_TYPE_MISMATCH, vformat("\"%s\": element %d must be of type %s, not \"%s\".", p_name, i, Variant::get_type_name(p_field->get_element_type()), String(elements[i])), p_name, DataValidation::join_path(path, itos(i)), p_row, p_column);
				}
			}
		}
	}
	return kind == DataValidation::ISSUE_KIND_MAX;
}

void DataSchema::_validate_dictionary(const Dictionary &p_data, const String &p_path, int p_row, DataValidation *r_validation) const {
	Variant typed;
	for (int i = 0; i < fields.size() && !r_validation->is_full(); i++) {
		const DataField *field = _get_field_ptr(i);
		if (!field) {
			continue;
		}
		const StringName &key = field->get_field_name();
		const Variant *value = p_data.getptr(String(key));
		if (!value) {
			if (field->is_required()) {
				r_validation->add_issue(DataValidation::ISSUE_KIND_MISSING_FIELD, vformat("\"%s\" is missing.", key), key, p_path, p_row);
			}
			continue;
		}
		_check_value(field, key, *value, p_path, p_row, DataValidation::POSITION_NONE, r_validation, typed);
	}
	if (extra_fields == EXTRA_FIELDS_ALLOW) {
		return;
	}
	for (const KeyValue<Variant, Variant> &entry : p_data) {
		if (r_validation->is_full()) {
			return;
		}
		const StringName key = entry.key;
		if (find_field(key) >= 0) {
			continue;
		}
		if (extra_fields == EXTRA_FIELDS_MATCH_TEMPLATE && extra_field_template.is_valid()) {
			_check_value(extra_field_template.ptr(), key, entry.value, p_path, p_row, DataValidation::POSITION_NONE, r_validation, typed);
		} else {
			r_validation->add_issue(DataValidation::ISSUE_KIND_UNKNOWN_FIELD, vformat("Unknown field \"%s\".", key), key, DataValidation::join_path(p_path, key), p_row);
		}
	}
}

void DataSchema::_validate_records(const Array &p_records, const String &p_path, int p_row, DataValidation *r_validation) const {
	LocalVector<HashMap<Variant, int>> seen;
	seen.resize(fields.size());
	Variant typed;
	for (int i = 0; i < p_records.size() && !r_validation->is_full(); i++) {
		const String item_path = DataValidation::join_path(p_path, itos(i));
		// Records of the root list are the rows.
		const int row = p_path.is_empty() ? i : p_row;
		if (p_records[i].get_type() != Variant::DICTIONARY) {
			r_validation->add_issue(DataValidation::ISSUE_KIND_TYPE_MISMATCH, vformat("Entry %d must be a Dictionary.", i), StringName(), item_path, row);
			continue;
		}
		const Dictionary record = p_records[i];
		_validate_dictionary(record, item_path, row, r_validation);
		for (int j = 0; j < fields.size(); j++) {
			const DataField *field = _get_field_ptr(j);
			const Variant *value = field && field->is_unique() ? record.getptr(String(field->get_field_name())) : nullptr;
			if (value && DataField::coerce(field->get_value_type(), *value, typed)) {
				_data_schema_check_unique(seen[j], typed, i, field->get_field_name(), DataValidation::join_path(item_path, field->get_field_name()), row, DataValidation::POSITION_NONE, r_validation);
			}
		}
	}
}

const DataField *DataSchema::get_column_field(const PackedStringArray &p_header, int p_column) const {
	int index = p_column;
	if (!p_header.is_empty()) {
		index = p_column < p_header.size() ? find_field(p_header[p_column]) : -1;
	}
	if (index >= 0 && index < fields.size()) {
		return _get_field_ptr(index);
	}
	return extra_fields == EXTRA_FIELDS_MATCH_TEMPLATE ? extra_field_template.ptr() : nullptr;
}

void DataSchema::validate_table(const Array &p_rows, const PackedStringArray &p_header, DataValidation *r_validation) const {
	ERR_FAIL_NULL(r_validation);
	int column_count = p_header.size();
	if (p_header.is_empty()) {
		for (int i = 0; i < p_rows.size(); i++) {
			const PackedStringArray cells = p_rows[i];
			column_count = MAX(column_count, cells.size());
		}
	} else {
		for (int i = 0; i < fields.size(); i++) {
			const DataField *field = _get_field_ptr(i);
			if (field && field->is_required() && !p_header.has(field->get_field_name())) {
				r_validation->add_issue(DataValidation::ISSUE_KIND_MISSING_FIELD, vformat("The column \"%s\" is missing.", field->get_field_name()), field->get_field_name());
			}
		}
	}

	LocalVector<const DataField *> column_fields;
	LocalVector<StringName> column_names;
	for (int column = 0; column < column_count; column++) {
		const DataField *field = get_column_field(p_header, column);
		const StringName column_name = !p_header.is_empty() ? StringName(p_header[column]) : (field && field != extra_field_template.ptr() ? field->get_field_name() : StringName(itos(column)));
		if (!field && extra_fields != EXTRA_FIELDS_ALLOW) {
			r_validation->add_issue(DataValidation::ISSUE_KIND_UNKNOWN_FIELD, vformat("Unknown column \"%s\".", column_name), column_name, String(), DataValidation::POSITION_NONE, column);
		}
		column_fields.push_back(field);
		column_names.push_back(column_name);
	}

	LocalVector<HashMap<Variant, int>> seen;
	seen.resize(column_count);
	Variant typed;
	for (int row = 0; row < p_rows.size() && !r_validation->is_full(); row++) {
		const PackedStringArray cells = p_rows[row];
		const String row_path = itos(row);
		for (int column = 0; column < column_count; column++) {
			const DataField *field = column_fields[column];
			if (!field) {
				continue;
			}
			const String cell = column < cells.size() ? cells[column] : String();
			if (_check_value(field, column_names[column], cell, row_path, row, column, r_validation, typed) && field->is_unique()) {
				_data_schema_check_unique(seen[column], typed, row, column_names[column], DataValidation::join_path(row_path, column_names[column]), row, column, r_validation);
			}
		}
	}
}

Ref<DataValidation> DataSchema::validate(const Variant &p_data, const Ref<DataValidationOptions> &p_options) const {
	Ref<DataValidation> result;
	result.instantiate();
	result->apply_options(p_options);
	if (p_data.get_type() == Variant::DICTIONARY) {
		_validate_dictionary(p_data, String(), DataValidation::POSITION_NONE, result.ptr());
	} else if (p_data.get_type() == Variant::ARRAY) {
		const Array items = p_data;
		if (items.is_empty() || items[0].get_type() == Variant::DICTIONARY) {
			_validate_records(items, String(), DataValidation::POSITION_NONE, result.ptr());
		} else {
			validate_table(items, PackedStringArray(), result.ptr());
		}
	} else {
		result->add_issue(DataValidation::ISSUE_KIND_TYPE_MISMATCH, vformat("The data must be a Dictionary or an Array, not %s.", Variant::get_type_name(p_data.get_type())));
	}
	return result;
}

void DataSchema::set_fields(const TypedArray<DataField> &p_fields) {
	fields = p_fields;
	emit_changed();
}

void DataSchema::set_extra_fields(ExtraFields p_extra_fields) {
	ERR_FAIL_INDEX(p_extra_fields, EXTRA_FIELDS_MAX);
	extra_fields = p_extra_fields;
	emit_changed();
}

void DataSchema::set_extra_field_template(const Ref<DataField> &p_template) {
	extra_field_template = p_template;
	emit_changed();
}

void DataSchema::set_file_patterns(const PackedStringArray &p_patterns) {
	file_patterns = p_patterns;
	emit_changed();
}

Ref<DataField> DataSchema::get_field(const StringName &p_name) const {
	const int index = find_field(p_name);
	return index >= 0 ? Ref<DataField>(fields[index]) : Ref<DataField>();
}

int DataSchema::find_field(const StringName &p_name) const {
	for (int i = 0; i < fields.size(); i++) {
		const DataField *field = _get_field_ptr(i);
		if (field && field->get_field_name() == p_name) {
			return i;
		}
	}
	return -1;
}

void DataSchema::add_field(const Ref<DataField> &p_field) {
	ERR_FAIL_COND(p_field.is_null());
	fields.push_back(p_field);
	emit_changed();
}

void DataSchema::remove_field(int p_index) {
	ERR_FAIL_INDEX(p_index, fields.size());
	fields.remove_at(p_index);
	emit_changed();
}

Variant DataSchema::convert_value(const StringName &p_field_name, const String &p_text) const {
	const int index = find_field(p_field_name);
	ERR_FAIL_COND_V_MSG(index < 0, Variant(), vformat("The schema has no field \"%s\".", p_field_name));
	Variant value;
	if (!_get_field_ptr(index)->convert_text(p_text, value)) {
		return Variant();
	}
	return value;
}

Ref<DataSchema> DataSchema::create_from_dictionary(const Dictionary &p_dictionary) {
	Ref<DataSchema> schema;
	schema.instantiate();
	for (const KeyValue<Variant, Variant> &entry : p_dictionary) {
		Ref<DataField> field;
		field.instantiate();
		field->set_field_name(entry.key);
		field->set_required(true);
		// Short form: a Variant.Type, a Dictionary (nested schema) or a one-element Array (element type or schema).
		const Variant &shape = entry.value;
		const Array elements = shape.get_type() == Variant::ARRAY ? shape.operator Array() : Array();
		if (shape.get_type() == Variant::INT) {
			field->set_value_type(Variant::Type((int)shape));
		} else if (shape.get_type() == Variant::DICTIONARY) {
			field->set_value_type(Variant::DICTIONARY);
			field->set_schema(create_from_dictionary(shape));
		} else if (elements.size() == 1) {
			field->set_value_type(Variant::ARRAY);
			if (elements[0].get_type() == Variant::DICTIONARY) {
				field->set_element_schema(create_from_dictionary(elements[0]));
			} else {
				field->set_element_type(Variant::Type((int)elements[0]));
			}
		} else {
			ERR_CONTINUE_MSG(true, vformat("The value of \"%s\" must be a Variant.Type, a Dictionary or an Array with one element.", entry.key));
		}
		schema->add_field(field);
	}
	return schema;
}

// Nesting of resources is cut at this depth (a class that holds an instance of itself).
static constexpr int RESOURCE_SCHEMA_MAX_DEPTH = 6;

static Ref<DataSchema> _schema_from_resource(const Ref<Resource> &p_resource, bool p_enums_as_names, int p_depth);

// A sample of the Resource class named by `p_class` (the first of a comma separated list), or null.
static Ref<Resource> _schema_sample(const String &p_class) {
	String error;
	return ResourceTextSerializer::instantiate_type(p_class.get_slicec(',', 0), error);
}

// The element type of an Array property: from the typed array value, else from the hint ("ClassName", "int",
// "<type>:<class>" or "<type>/<hint>:<class>").
static void _schema_set_element(const Ref<DataField> &p_field, const Array &p_live, const PropertyInfo &p_info, bool p_enums_as_names, int p_depth) {
	Variant::Type element_type = Variant::NIL;
	String element_class;
	if (p_live.is_typed()) {
		element_type = Variant::Type(p_live.get_typed_builtin());
		element_class = p_live.get_typed_class_name();
		const Ref<Script> script = p_live.get_typed_script();
		if (script.is_valid() && script->get_global_name() != StringName()) {
			element_class = script->get_global_name();
		}
	} else if ((p_info.hint == PROPERTY_HINT_ARRAY_TYPE || p_info.hint == PROPERTY_HINT_TYPE_STRING) && !p_info.hint_string.is_empty()) {
		const int colon = p_info.hint_string.find_char(':');
		const String type_part = (colon > 0 ? p_info.hint_string.substr(0, colon) : p_info.hint_string).get_slicec('/', 0);
		if (type_part.is_valid_int()) {
			element_type = Variant::Type(type_part.to_int());
			element_class = colon > 0 ? p_info.hint_string.substr(colon + 1) : String();
		} else if (Variant::get_type_by_name(type_part) != Variant::VARIANT_MAX) {
			element_type = Variant::get_type_by_name(type_part);
		} else {
			element_type = Variant::OBJECT;
			element_class = p_info.hint_string;
		}
	}
	if (element_type != Variant::OBJECT) {
		if (DataField::is_data_type(element_type)) {
			p_field->set_element_type(element_type);
		}
		return;
	}
	if (p_depth >= RESOURCE_SCHEMA_MAX_DEPTH) {
		return;
	}
	// Resources in files are paths, so only elements that are written as objects get a schema: the built-in
	// elements of the array, or an empty array of a script class (data classes are usually embedded).
	Ref<Resource> sample;
	if (!p_live.is_empty()) {
		const Ref<Resource> element = p_live[0];
		if (element.is_valid() && element->is_built_in()) {
			sample = element;
		}
	}
	if (sample.is_null() && p_live.is_empty() && ScriptServer::is_global_class(element_class.get_slicec(',', 0))) {
		sample = _schema_sample(element_class);
	}
	if (sample.is_valid()) {
		p_field->set_element_schema(_schema_from_resource(sample, p_enums_as_names, p_depth + 1));
	}
}

static Ref<DataField> _schema_field_from_property(const Object *p_object, const PropertyInfo &p_info, bool p_enums_as_names, int p_depth) {
	Variant::Type type = p_info.type;
	if (type != Variant::OBJECT && !DataField::is_data_type(type)) {
		return Ref<DataField>();
	}
	Ref<DataField> field;
	field.instantiate();
	field->set_field_name(p_info.name);
	if (type == Variant::OBJECT) {
		// Built-in resources are written as objects, resources in files as their path. A resource that is not set
		// can be either, so it stays untyped.
		const Ref<Resource> nested = p_object->get(p_info.name);
		if (nested.is_valid() && nested->is_built_in()) {
			field->set_value_type(Variant::DICTIONARY);
			if (p_depth < RESOURCE_SCHEMA_MAX_DEPTH) {
				field->set_schema(_schema_from_resource(nested, p_enums_as_names, p_depth + 1));
			}
		} else if (nested.is_valid()) {
			field->set_value_type(Variant::STRING);
		}
		return field;
	}
	if (type == Variant::INT && p_enums_as_names && ResourceTextSerializer::is_enum_property(p_info)) {
		type = Variant::STRING; // Written as the names.
	}
	field->set_value_type(type);
	if (p_info.hint == PROPERTY_HINT_ENUM || p_info.hint == PROPERTY_HINT_RANGE) {
		field->set_hint(p_info.hint);
		field->set_hint_string(p_info.hint_string);
	}
	if (type == Variant::ARRAY) {
		_schema_set_element(field, p_object->get(p_info.name), p_info, p_enums_as_names, p_depth);
	}
	return field;
}

static Ref<DataSchema> _schema_from_resource(const Ref<Resource> &p_resource, bool p_enums_as_names, int p_depth) {
	Ref<DataSchema> schema;
	schema.instantiate();
	schema->set_extra_fields(DataSchema::EXTRA_FIELDS_ALLOW);
	LocalVector<PropertyInfo> properties;
	ResourceTextSerializer::get_serialized_properties(p_resource.ptr(), properties);
	for (const PropertyInfo &info : properties) {
		if (ClassDB::has_property("Resource", info.name)) {
			continue; // The name and the scene flag of every resource are noise in a data schema.
		}
		const Ref<DataField> field = _schema_field_from_property(p_resource.ptr(), info, p_enums_as_names, p_depth);
		if (field.is_valid()) {
			schema->add_field(field);
		}
	}
	return schema;
}

Ref<DataSchema> DataSchema::create_from_resource(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	ERR_FAIL_COND_V(p_resource.is_null(), Ref<DataSchema>());
	return _schema_from_resource(p_resource, ResourceTextSerializer::get_options(p_options).enums_as_names, 0);
}

void DataSchema::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fields", "fields"), &DataSchema::set_fields);
	ClassDB::bind_method(D_METHOD("get_fields"), &DataSchema::get_fields);
	ClassDB::bind_method(D_METHOD("set_extra_fields", "extra_fields"), &DataSchema::set_extra_fields);
	ClassDB::bind_method(D_METHOD("get_extra_fields"), &DataSchema::get_extra_fields);
	ClassDB::bind_method(D_METHOD("set_extra_field_template", "template"), &DataSchema::set_extra_field_template);
	ClassDB::bind_method(D_METHOD("get_extra_field_template"), &DataSchema::get_extra_field_template);
	ClassDB::bind_method(D_METHOD("set_file_patterns", "patterns"), &DataSchema::set_file_patterns);
	ClassDB::bind_method(D_METHOD("get_file_patterns"), &DataSchema::get_file_patterns);
	ClassDB::bind_method(D_METHOD("get_field", "name"), &DataSchema::get_field);
	ClassDB::bind_method(D_METHOD("find_field", "name"), &DataSchema::find_field);
	ClassDB::bind_method(D_METHOD("add_field", "field"), &DataSchema::add_field);
	ClassDB::bind_method(D_METHOD("remove_field", "index"), &DataSchema::remove_field);
	ClassDB::bind_method(D_METHOD("validate", "data", "options"), &DataSchema::validate, DEFVAL(Ref<DataValidationOptions>()));
	ClassDB::bind_method(D_METHOD("convert_value", "field_name", "text"), &DataSchema::convert_value);
	ClassDB::bind_static_method("DataSchema", D_METHOD("create_from_dictionary", "dictionary"), &DataSchema::create_from_dictionary);
	ClassDB::bind_static_method("DataSchema", D_METHOD("create_from_resource", "resource", "options"), &DataSchema::create_from_resource, DEFVAL(Ref<SerializerOptions>()));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "fields", PROPERTY_HINT_ARRAY_TYPE, "DataField"), "set_fields", "get_fields");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "extra_fields", PROPERTY_HINT_ENUM, "Reject,Allow,Match Template"), "set_extra_fields", "get_extra_fields");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "extra_field_template", PROPERTY_HINT_RESOURCE_TYPE, "DataField"), "set_extra_field_template", "get_extra_field_template");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "file_patterns"), "set_file_patterns", "get_file_patterns");

	BIND_ENUM_CONSTANT(EXTRA_FIELDS_REJECT);
	BIND_ENUM_CONSTANT(EXTRA_FIELDS_ALLOW);
	BIND_ENUM_CONSTANT(EXTRA_FIELDS_MATCH_TEMPLATE);
	BIND_ENUM_CONSTANT(EXTRA_FIELDS_MAX);
}
