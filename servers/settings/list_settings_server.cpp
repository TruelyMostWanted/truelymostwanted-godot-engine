/**************************************************************************/
/*  list_settings_server.cpp                                              */
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

#include "list_settings_server.h"

#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"

StringName ListSettingsServer::get_list_property() const {
	StringName ret;
	if (GDVIRTUAL_CALL(_get_list_property, ret) && ret != StringName()) {
		return ret;
	}
	const Ref<Resource> pending = get_pending_settings();
	LocalVector<PropertyInfo> infos;
	get_value_infos(pending.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		if (info.type == Variant::ARRAY) {
			return info.name;
		}
	}
	return StringName();
}

StringName ListSettingsServer::get_entry_key() const {
	StringName ret;
	GDVIRTUAL_CALL(_get_entry_key, ret);
	return ret;
}

ResourceTextSerializer::Format ListSettingsServer::_get_file_format() {
	if (_get_configured_format() != ResourceTextSerializer::FORMAT_CSV) {
		return SettingsServer::_get_file_format();
	}
	String error;
	const Ref<Resource> default_settings = get_default_settings();
	const Ref<Resource> storage = default_settings.is_valid() ? _settings_to_storage(default_settings) : Ref<Resource>();
	if (storage.is_valid() && ResourceTextSerializer::check_csv(storage, error) == OK) {
		return ResourceTextSerializer::FORMAT_CSV;
	}
	if (!format_warned_csv) {
		format_warned_csv = true;
		WARN_PRINT(vformat("Settings domain \"%s\" can't be stored as CSV (%s); INI is used.", get_domain(), error));
	}
	return ResourceTextSerializer::FORMAT_INI;
}

Array ListSettingsServer::_get_list() const {
	const StringName property = get_list_property();
	ERR_FAIL_COND_V_MSG(property == StringName(), Array(), vformat("Settings domain \"%s\" has no Array property for its entries.", get_domain()));
	return get_pending_settings()->get(property);
}

void ListSettingsServer::_set_list(const Array &p_list) {
	const StringName property = get_list_property();
	ERR_FAIL_COND(property == StringName());
	get_pending_settings()->set(property, p_list);
	notify_value_changed(property);
}

int ListSettingsServer::get_entry_count() const {
	return _get_list().size();
}

Ref<Resource> ListSettingsServer::get_entry(int p_index) const {
	const Array list = _get_list();
	ERR_FAIL_INDEX_V(p_index, list.size(), Ref<Resource>());
	return list[p_index];
}

TypedArray<Resource> ListSettingsServer::get_entries() const {
	const Array list = _get_list();
	TypedArray<Resource> entries;
	for (int i = 0; i < list.size(); i++) {
		entries.push_back(list[i]);
	}
	return entries;
}

int ListSettingsServer::find_entry(const Variant &p_key) const {
	const StringName key = get_entry_key();
	ERR_FAIL_COND_V_MSG(key == StringName(), -1, vformat("Settings domain \"%s\" has no entry key: implement _get_entry_key().", get_domain()));
	const Array list = _get_list();
	for (int i = 0; i < list.size(); i++) {
		const Ref<Resource> entry = list[i];
		if (entry.is_valid() && entry->get(key) == p_key) {
			return i;
		}
	}
	return -1;
}

void ListSettingsServer::add_entry(const Ref<Resource> &p_entry, int p_at_index) {
	ERR_FAIL_COND(p_entry.is_null());
	Array list = _get_list().duplicate(false);
	if (p_at_index < 0 || p_at_index >= list.size()) {
		list.push_back(p_entry);
	} else {
		list.insert(p_at_index, p_entry);
	}
	_set_list(list);
}

void ListSettingsServer::remove_entry(int p_index) {
	Array list = _get_list().duplicate(false);
	ERR_FAIL_INDEX(p_index, list.size());
	list.remove_at(p_index);
	_set_list(list);
}

void ListSettingsServer::move_entry(int p_from, int p_to) {
	Array list = _get_list().duplicate(false);
	ERR_FAIL_INDEX(p_from, list.size());
	ERR_FAIL_INDEX(p_to, list.size());
	if (p_from == p_to) {
		return;
	}
	const Variant entry = list[p_from];
	list.remove_at(p_from);
	list.insert(p_to, entry);
	_set_list(list);
}

void ListSettingsServer::clear_entries() {
	Array list = _get_list().duplicate(false);
	if (list.is_empty()) {
		return;
	}
	list.clear();
	_set_list(list);
}

void ListSettingsServer::set_entry_value(int p_index, const StringName &p_name, const Variant &p_value) {
	const Array list = _get_list();
	ERR_FAIL_INDEX(p_index, list.size());
	const Ref<Resource> entry = list[p_index];
	ERR_FAIL_COND(entry.is_null());
	bool valid = false;
	const Variant current = entry->get(p_name, &valid);
	ERR_FAIL_COND_MSG(!valid, vformat("Settings domain \"%s\": the entries have no value \"%s\".", get_domain(), p_name));
	// Entry values are validated like single values: range and enum hints of the entry, then _validate_entry_value().
	const Variant value = _validate_entry(p_index, entry, p_name, p_value);
	if (values_equal(current, value)) {
		return;
	}
	entry->set(p_name, value);
	notify_value_changed(get_list_property());
}

Variant ListSettingsServer::_validate_entry(int p_index, const Ref<Resource> &p_entry, const StringName &p_name, const Variant &p_value) const {
	Variant value = validate_property_value(p_entry.ptr(), p_name, p_value);
	Variant ret;
	if (GDVIRTUAL_CALL(_validate_entry_value, p_index, p_name, value, ret)) {
		value = ret;
	}
	return value;
}

void ListSettingsServer::_validate_loaded_settings(const Ref<Resource> &p_settings) {
	// Hand-edited entries are clamped like hand-edited single values.
	const StringName property = get_list_property();
	if (property == StringName() || p_settings.is_null()) {
		return;
	}
	const Array list = p_settings->get(property);
	for (int i = 0; i < list.size(); i++) {
		const Ref<Resource> entry = list[i];
		if (entry.is_null()) {
			continue;
		}
		LocalVector<PropertyInfo> infos;
		get_value_infos(entry.ptr(), infos);
		for (const PropertyInfo &info : infos) {
			entry->set(info.name, _validate_entry(i, entry, info.name, entry->get(info.name)));
		}
	}
}

Variant ListSettingsServer::get_entry_value(int p_index, const StringName &p_name) const {
	const Array list = _get_list();
	ERR_FAIL_INDEX_V(p_index, list.size(), Variant());
	const Ref<Resource> entry = list[p_index];
	ERR_FAIL_COND_V(entry.is_null(), Variant());
	return entry->get(p_name);
}

void ListSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_list_property"), &ListSettingsServer::get_list_property);
	ClassDB::bind_method(D_METHOD("get_entry_key"), &ListSettingsServer::get_entry_key);
	ClassDB::bind_method(D_METHOD("get_entry_count"), &ListSettingsServer::get_entry_count);
	ClassDB::bind_method(D_METHOD("get_entry", "index"), &ListSettingsServer::get_entry);
	ClassDB::bind_method(D_METHOD("get_entries"), &ListSettingsServer::get_entries);
	ClassDB::bind_method(D_METHOD("find_entry", "key"), &ListSettingsServer::find_entry);
	ClassDB::bind_method(D_METHOD("add_entry", "entry", "at_index"), &ListSettingsServer::add_entry, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("remove_entry", "index"), &ListSettingsServer::remove_entry);
	ClassDB::bind_method(D_METHOD("move_entry", "from", "to"), &ListSettingsServer::move_entry);
	ClassDB::bind_method(D_METHOD("clear_entries"), &ListSettingsServer::clear_entries);
	ClassDB::bind_method(D_METHOD("set_entry_value", "index", "name", "value"), &ListSettingsServer::set_entry_value);
	ClassDB::bind_method(D_METHOD("get_entry_value", "index", "name"), &ListSettingsServer::get_entry_value);

	GDVIRTUAL_BIND(_get_list_property);
	GDVIRTUAL_BIND(_get_entry_key);
	GDVIRTUAL_BIND(_validate_entry_value, "index", "name", "value");
}
