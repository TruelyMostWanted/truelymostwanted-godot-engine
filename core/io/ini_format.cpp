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

Error INI::parse(const String &p_text) {
	return parse_text(p_text, data, error_message, error_line);
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

void INI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "text"), &INI::parse);
	ClassDB::bind_method(D_METHOD("get_data"), &INI::get_data);
	ClassDB::bind_method(D_METHOD("get_error_message"), &INI::get_error_message);
	ClassDB::bind_method(D_METHOD("get_error_line"), &INI::get_error_line);

	ClassDB::bind_static_method("INI", D_METHOD("parse_string", "text"), &INI::parse_string);
	ClassDB::bind_static_method("INI", D_METHOD("stringify", "data", "options"), &INI::stringify, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("INI", D_METHOD("serialize", "resource", "options"), &INI::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_static_method("INI", D_METHOD("deserialize", "text", "type_hint", "options"), &INI::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
}
