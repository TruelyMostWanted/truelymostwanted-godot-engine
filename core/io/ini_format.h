/**************************************************************************/
/*  ini_format.h                                                          */
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

#include "core/io/config_file.h"
#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/variant/dictionary.h"

// An INI file as a Resource: `data` is a Dictionary of sections (section name -> Dictionary of key -> value),
// with the API of ConfigFile. Keys before the first section belong to the section "". Values use Godot Variant
// syntax (strings quoted), or plain text with SerializerOptions.ini_value_style. The static functions also read
// and write any Resource as INI (format FORMAT_INI of ResourceTextSerializer).
class INI : public TextDataFormat {
	GDCLASS(INI, TextDataFormat);

	Dictionary _get_sections_dictionary() const;
	Error _load_from_file(Ref<FileAccess> p_file);
	Error _save_to_file(Ref<FileAccess> p_file);

protected:
	static void _bind_methods();

	virtual Error _parse_data(const String &p_text, const Ref<SerializerOptions> &p_options, Variant &r_data, String &r_error, int &r_line, int &r_column) override;
	virtual Error _encode_data(const Variant &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error) const override;
	virtual Variant _get_empty_data() const override { return Dictionary(); }
	virtual Variant::Type _get_data_type() const override { return Variant::DICTIONARY; }
	virtual void _get_extensions(List<String> *r_extensions) const override;

public:
	// On error, r_line is the 1-based line of the problem. Repeated sections are merged.
	static Error parse_text(const String &p_text, Dictionary &r_data, String &r_error, int &r_line);
	static Error write_text(const Dictionary &p_data, const Ref<SerializerOptions> &p_options, String &r_text, String &r_error);

	// ConfigFile API.
	void set_value(const String &p_section, const String &p_key, const Variant &p_value);
	Variant get_value(const String &p_section, const String &p_key, const Variant &p_default = Variant()) const;
	bool has_section(const String &p_section) const;
	bool has_section_key(const String &p_section, const String &p_key) const;
	PackedStringArray get_sections() const;
	PackedStringArray get_section_keys(const String &p_section) const;
	void erase_section(const String &p_section);
	void erase_section_key(const String &p_section, const String &p_key);

	Error load_encrypted(const String &p_path, const Vector<uint8_t> &p_key);
	Error load_encrypted_pass(const String &p_path, const String &p_password);
	Error save_encrypted(const String &p_path, const Vector<uint8_t> &p_key);
	Error save_encrypted_pass(const String &p_path, const String &p_password);

	Ref<ConfigFile> to_config_file() const;
	Error from_config_file(const Ref<ConfigFile> &p_config);

	static Dictionary parse_string(const String &p_text);
	static String stringify(const Dictionary &p_data, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static String serialize(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());
	static Ref<Resource> deserialize(const String &p_text, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>());

	INI();
};
