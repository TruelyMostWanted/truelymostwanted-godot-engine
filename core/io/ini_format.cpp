/**************************************************************************/
/*  ini_format.cpp                                                        */
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

#include "ini_format.h"

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/class_db.h"

Error INI::parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line) {
	r_data = Dictionary();
	r_error = String();
	r_line = 0;
	LocalVector<ResourceTextSerializer::Section> sections;
	const Error err = ResourceTextSerializer::parse_ini_sections(p_text, sections, r_error, &r_line);
	if (err != OK) {
		return err;
	}
	Dictionary result;
	for (const ResourceTextSerializer::Section &section : sections) {
		const Variant existing = result.get(section.first, Variant());
		if (existing.get_type() == Variant::DICTIONARY) {
			Dictionary keys = existing;
			keys.merge(section.second, true);
		} else {
			result[section.first] = section.second.duplicate();
		}
	}
	r_data = result;
	return OK;
}

Error INI::write_text(const Dictionary &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) {
	const ResourceTextSerializer::Options options = ResourceTextSerializer::get_options(p_options);
	Vector<String> section_names;
	for (const KeyValue<Variant, Variant> &kv : p_data) {
		if (kv.value.get_type() != Variant::DICTIONARY) {
			r_error = vformat("The section \"%s\" must be a Dictionary of keys.", String(kv.key));
			return ERR_INVALID_DATA;
		}
		const String section_name = kv.key;
		if (section_name.contains_char('\n') || section_name.contains_char(']')) {
			r_error = vformat("\"%s\" is not a valid INI section name.", section_name);
			return ERR_INVALID_DATA;
		}
		section_names.push_back(section_name);
	}
	if (options.sort_keys) {
		section_names.sort();
	}

	String text;
	// Pass 0 writes the keys without a section (section ""), without a header; pass 1 the sections.
	bool first = true;
	for (int pass = 0; pass < 2; pass++) {
		for (const String &section_name : section_names) {
			if (section_name.is_empty() != (pass == 0)) {
				continue;
			}
			const Dictionary keys = p_data.get(section_name, Dictionary());
			if (pass == 0 && keys.is_empty()) {
				continue;
			}
			if (!first) {
				text += "\n";
			}
			first = false;
			if (pass == 1) {
				text += "[" + section_name + "]\n";
			}
			Vector<String> key_names;
			for (const KeyValue<Variant, Variant> &kv : keys) {
				key_names.push_back(kv.key);
			}
			if (options.sort_keys) {
				key_names.sort();
			}
			for (const String &key : key_names) {
				if (key.is_empty()) {
					r_error = vformat("The section \"%s\" has an empty key.", section_name);
					return ERR_INVALID_DATA;
				}
				text += key.property_name_encode() + "=" + ResourceTextSerializer::ini_value_to_text(keys.get(key, Variant()), options.ini_plain_values) + "\n";
			}
		}
	}
	r_text = text;
	return OK;
}

Error INI::_parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) {
	Dictionary parsed;
	r_column = 0;
	const Error err = parse_text(p_text, parsed, r_error, r_line);
	r_data = parsed;
	return err;
}

Error INI::_encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const {
	return write_text(p_data, p_options, r_text, r_error);
}

void INI::_get_extensions(List<String> *r_extensions) const {
	// `.cfg` stays with ConfigFile files (project.godot, .import, presets).
	r_extensions->push_back("ini");
}

Dictionary INI::_get_sections_dictionary() const {
	return get_data();
}

void INI::set_value(const String &p_section, const String &p_key, const Variant &p_value) {
	Dictionary sections = _get_sections_dictionary();
	if (p_value.get_type() == Variant::NIL) {
		// Like ConfigFile: null erases the key, and the section once it is empty.
		if (!sections.has(p_section)) {
			return;
		}
		Dictionary keys = sections[p_section];
		keys.erase(p_key);
		if (keys.is_empty()) {
			sections.erase(p_section);
		}
	} else {
		if (!sections.has(p_section) || sections[p_section].get_type() != Variant::DICTIONARY) {
			sections[p_section] = Dictionary();
		}
		Dictionary keys = sections[p_section];
		keys[p_key] = p_value;
	}
	_mark_data_changed();
}

Variant INI::get_value(const String &p_section, const String &p_key, const Variant &p_default) const {
	const Dictionary sections = _get_sections_dictionary();
	const Variant keys = sections.get(p_section, Variant());
	if (keys.get_type() != Variant::DICTIONARY) {
		ERR_FAIL_COND_V_MSG(p_default.get_type() == Variant::NIL, Variant(), vformat("Couldn't find the given section \"%s\" and key \"%s\", and no default was given.", p_section, p_key));
		return p_default;
	}
	const Dictionary section_keys = keys;
	if (!section_keys.has(p_key)) {
		ERR_FAIL_COND_V_MSG(p_default.get_type() == Variant::NIL, Variant(), vformat("Couldn't find the given section \"%s\" and key \"%s\", and no default was given.", p_section, p_key));
		return p_default;
	}
	return section_keys[p_key];
}

bool INI::has_section(const String &p_section) const {
	return _get_sections_dictionary().has(p_section);
}

bool INI::has_section_key(const String &p_section, const String &p_key) const {
	const Variant keys = _get_sections_dictionary().get(p_section, Variant());
	if (keys.get_type() != Variant::DICTIONARY) {
		return false;
	}
	return Dictionary(keys).has(p_key);
}

PackedStringArray INI::get_sections() const {
	PackedStringArray result;
	for (const KeyValue<Variant, Variant> &kv : _get_sections_dictionary()) {
		result.push_back(kv.key);
	}
	return result;
}

PackedStringArray INI::get_section_keys(const String &p_section) const {
	PackedStringArray result;
	const Variant keys = _get_sections_dictionary().get(p_section, Variant());
	ERR_FAIL_COND_V_MSG(keys.get_type() != Variant::DICTIONARY, result, vformat("Cannot get keys from nonexistent section \"%s\".", p_section));
	for (const KeyValue<Variant, Variant> &kv : Dictionary(keys)) {
		result.push_back(kv.key);
	}
	return result;
}

void INI::erase_section(const String &p_section) {
	Dictionary sections = _get_sections_dictionary();
	ERR_FAIL_COND_MSG(!sections.has(p_section), vformat("Cannot erase nonexistent section \"%s\".", p_section));
	sections.erase(p_section);
	_mark_data_changed();
}

void INI::erase_section_key(const String &p_section, const String &p_key) {
	Dictionary sections = _get_sections_dictionary();
	const Variant keys = sections.get(p_section, Variant());
	ERR_FAIL_COND_MSG(keys.get_type() != Variant::DICTIONARY, vformat("Cannot erase key \"%s\" from nonexistent section \"%s\".", p_key, p_section));
	Dictionary section_keys = keys;
	ERR_FAIL_COND_MSG(!section_keys.has(p_key), vformat("Cannot erase nonexistent key \"%s\" from section \"%s\".", p_key, p_section));
	section_keys.erase(p_key);
	_mark_data_changed();
}

Error INI::_load_from_file(Ref<FileAccess> p_file) {
	return parse(p_file->get_as_utf8_string());
}

Error INI::_save_to_file(Ref<FileAccess> p_file) {
	String save_text;
	String save_error;
	const Error err = get_save_text(save_text, save_error);
	ERR_FAIL_COND_V_MSG(err != OK, err, save_error);
	p_file->store_string(save_text);
	return OK;
}

Error INI::load_encrypted(const String &p_path, const Vector<uint8_t> &p_key) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}
	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_READ);
	if (err != OK) {
		return err;
	}
	return _load_from_file(fae);
}

Error INI::load_encrypted_pass(const String &p_path, const String &p_password) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}
	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse_password(f, p_password, FileAccessEncrypted::MODE_READ);
	if (err != OK) {
		return err;
	}
	return _load_from_file(fae);
}

Error INI::save_encrypted(const String &p_path, const Vector<uint8_t> &p_key) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (err != OK) {
		return err;
	}
	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_WRITE_AES256);
	if (err != OK) {
		return err;
	}
	return _save_to_file(fae);
}

Error INI::save_encrypted_pass(const String &p_path, const String &p_password) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (err != OK) {
		return err;
	}
	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse_password(f, p_password, FileAccessEncrypted::MODE_WRITE_AES256);
	if (err != OK) {
		return err;
	}
	return _save_to_file(fae);
}

Ref<ConfigFile> INI::to_config_file() const {
	Ref<ConfigFile> config;
	config.instantiate();
	for (const KeyValue<Variant, Variant> &section : _get_sections_dictionary()) {
		if (section.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		for (const KeyValue<Variant, Variant> &key : Dictionary(section.value)) {
			if (key.value.get_type() != Variant::NIL) {
				config->set_value(section.key, key.key, key.value);
			}
		}
	}
	return config;
}

Error INI::from_config_file(const Ref<ConfigFile> &p_config) {
	ERR_FAIL_COND_V(p_config.is_null(), ERR_INVALID_PARAMETER);
	Dictionary sections;
	for (const String &section : p_config->get_sections()) {
		Dictionary keys;
		for (const String &key : p_config->get_section_keys(section)) {
			keys[key] = p_config->get_value(section, key);
		}
		sections[section] = keys;
	}
	set_data(sections);
	return OK;
}

Dictionary INI::parse_string(const String &p_text) {
	Dictionary result;
	String error;
	int line = 0;
	if (parse_text(p_text, result, error, line) != OK) {
		ERR_PRINT("INI parse error: " + error);
		return Dictionary();
	}
	return result;
}

String INI::stringify(const Dictionary &p_data, const Ref<SerializerOptions> &p_options) {
	String text;
	String error;
	if (write_text(p_data, p_options, text, error) != OK) {
		ERR_PRINT(error);
		return String();
	}
	return text;
}

String INI::serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, String());
	return serializer->serialize(p_resource, ResourceTextSerializer::FORMAT_INI, p_options);
}

Ref<Resource> INI::deserialize(const String &p_text, const StringName &p_type_hint, const Ref<SerializerOptions> &p_options) {
	const ResourceTextSerializer *serializer = ResourceTextSerializer::get_singleton();
	ERR_FAIL_NULL_V(serializer, Ref<Resource>());
	return serializer->deserialize(p_text, ResourceTextSerializer::FORMAT_INI, p_type_hint, p_options);
}

INI::INI() {
	_get_data_ref() = Dictionary();
}

void INI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "section", "key", "value"), &INI::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "section", "key", "default"), &INI::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_section", "section"), &INI::has_section);
	ClassDB::bind_method(D_METHOD("has_section_key", "section", "key"), &INI::has_section_key);
	ClassDB::bind_method(D_METHOD("get_sections"), &INI::get_sections);
	ClassDB::bind_method(D_METHOD("get_section_keys", "section"), &INI::get_section_keys);
	ClassDB::bind_method(D_METHOD("erase_section", "section"), &INI::erase_section);
	ClassDB::bind_method(D_METHOD("erase_section_key", "section", "key"), &INI::erase_section_key);
	ClassDB::bind_method(D_METHOD("load_encrypted", "path", "key"), &INI::load_encrypted);
	ClassDB::bind_method(D_METHOD("load_encrypted_pass", "path", "password"), &INI::load_encrypted_pass);
	ClassDB::bind_method(D_METHOD("save_encrypted", "path", "key"), &INI::save_encrypted);
	ClassDB::bind_method(D_METHOD("save_encrypted_pass", "path", "password"), &INI::save_encrypted_pass);
	ClassDB::bind_method(D_METHOD("to_config_file"), &INI::to_config_file);
	ClassDB::bind_method(D_METHOD("from_config_file", "config"), &INI::from_config_file);

	ClassDB::bind_static_method("INI", D_METHOD("parse_string", "text"), &INI::parse_string);
	ClassDB::bind_static_method("INI", D_METHOD("stringify", "data", "options"), &INI::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("INI", D_METHOD("serialize", "resource", "options"), &INI::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("INI", D_METHOD("deserialize", "text", "type_hint", "options"), &INI::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
