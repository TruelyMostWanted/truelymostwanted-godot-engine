/**************************************************************************/
/*  resource_text_serializer.h                                            */
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
#include "core/io/serializer_options.h"
#include "core/object/object.h"
#include "core/templates/local_vector.h"
#include "core/templates/pair.h"
#include "core/variant/dictionary.h"

// Maps any Resource to a tree of plain values (Dictionary, Array, String, numbers, booleans) and back,
// based on the properties with PROPERTY_USAGE_STORAGE. The text formats JSON, INI and CSV are built on
// this tree. Other formats (for example a TOML module) reuse encode_resource() and decode_resource().
class ResourceTextSerializer : public Object {
	GDCLASS(ResourceTextSerializer, Object);

	static ResourceTextSerializer *singleton;

protected:
	static void _bind_methods();

public:
	// Version of the text layout written into the files (key `format_version`).
	enum {
		TEXT_LAYOUT_VERSION = 1,
	};

	// The built-in text formats. Values are stable (append before FORMAT_MAX, never renumber), also when the module
	// that implements a format is disabled (TOML) or does not exist yet (YAML).
	enum Format {
		FORMAT_JSON,
		FORMAT_INI,
		FORMAT_CSV,
		FORMAT_XML,
		FORMAT_TOML,
		FORMAT_YAML,
		FORMAT_MAX,
	};

	// The values of a SerializerOptions, copied once per call so the formats do not call Object::get().
	struct Options {
		String indent = "  "; // JSON and XML indentation; empty writes one line.
		bool enums_as_names = true; // Enum properties as names instead of numbers.
		Ref<Resource> defaults; // Values of the root resource that are equal in this resource are not written.
		bool include_type_marker = true; // The type of the root resource (`$type`, XML attribute `type`).
		bool sort_keys = false; // Keys of objects, sections and elements in alphabetical order.
		bool full_precision_floats = false; // 32-bit floats of math types and colors with all digits.
		String csv_delimiter = ","; // One character.
		bool csv_headers = true; // CSV header row; without it, the columns are all stored properties in property order.
		bool ini_plain_values = false; // INI values without type syntax (other tools can read them).
		bool xml_declaration = true; // XML: `<?xml version="1.0" encoding="UTF-8"?>` first.
	};

	// One INI section: the path (`root`, `root/entries/0`) and its keys.
	typedef Pair<String, Dictionary> Section;

	static ResourceTextSerializer *get_singleton();

	static void register_settings();
	static Options get_default_options();
	// The options of `p_options`, or of the project settings if it is null.
	static Options get_options(const Ref<SerializerOptions> &p_options);

	// Type names.
	static bool is_resource_type(const String &p_type);
	static String get_type_name(const Object *p_object);
	static bool is_of_type(const Ref<Resource> &p_resource, const String &p_type);
	static Ref<Resource> instantiate_type(const String &p_type, String &r_error);

	// The tree. With `p_plain`, the tree contains only JSON types; without, Godot types such as Vector2 stay.
	// With `r_sections` (INI), nested resources are not inlined but appended as sections.
	static Error encode_resource(const Ref<Resource> &p_resource, const Options &p_options, bool p_plain, Dictionary &r_tree, String &r_error, LocalVector<Section> *r_sections = nullptr);
	static Error decode_resource(const Dictionary &p_tree, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);

	// Type in the file: the `type` key of a JSON object or of the INI section `[root]`, if it names a Resource class.
	static String get_json_marker_type(const Variant &p_data);
	static String get_ini_marker_type(const String &p_text);

	// JSON.
	static Error write_json(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	// Returns ERR_FILE_UNRECOGNIZED if the text is not a resource file and no type hint is given.
	static Error parse_json(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);

	// INI.
	static Error write_ini(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	static Error parse_ini_sections(const String &p_text, LocalVector<Section> &r_sections, String &r_error);
	// The tree of an INI text, in the layout of encode_resource(), without creating a resource.
	static Error parse_ini_tree(const String &p_text, Dictionary &r_tree, String &r_error);
	static Error parse_ini(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);

	// CSV.
	static Error check_csv(const Ref<Resource> &p_resource, String &r_error);
	static Error write_csv(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	static Error parse_csv(const String &p_text, const String &p_type_hint, const Options &p_options, Ref<Resource> &r_resource, String &r_error);
	static Vector<Vector<String>> parse_csv_rows(const String &p_text, char32_t p_delimiter);
	static String write_csv_row(const Vector<String> &p_cells, char32_t p_delimiter);

	// XML: one element per resource (`<resource type="..." format_version="1">`), one child element per property.
	// Implemented in resource_text_serializer_xml.cpp.
	static String get_xml_marker_type(const String &p_text);
	static Error write_xml(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	// Returns ERR_FILE_UNRECOGNIZED if the text is not a resource file and no type hint is given.
	static Error parse_xml(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error);

	// Implementations of the Format values, for serialize() and deserialize(). Core registers JSON, INI, CSV and XML;
	// modules register theirs (the TOML module registers FORMAT_TOML). The last registration wins. Thread-safe.
	// Implemented in resource_text_serializer_xml.cpp.
	typedef Error (*FormatWriteFunction)(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	typedef Error (*FormatParseFunction)(const String &p_text, const String &p_type_hint, const Options &p_options, Ref<Resource> &r_resource, String &r_error);
	static void register_format(Format p_format, FormatWriteFunction p_write, FormatParseFunction p_parse);
	static void unregister_format(Format p_format);
	static void register_builtin_formats();
	static void clear_formats();
	static bool has_format_implementation(Format p_format);
	// Returns ERR_UNAVAILABLE if no module implements the format.
	static Error write_format(Format p_format, const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error);
	static Error parse_format(Format p_format, const String &p_text, const String &p_type_hint, const Options &p_options, Ref<Resource> &r_resource, String &r_error);
	// "JSON", "TOML", ...
	static String get_format_display_name(Format p_format);
	// "json", "toml", ...; empty for FORMAT_MAX.
	static String get_format_file_extension(Format p_format);
	// FORMAT_MAX if the extension belongs to no format. Case-insensitive, a leading dot is ignored, "yml" is YAML.
	static Format get_format_for_extension(const String &p_extension);

	// Bound API.
	Dictionary resource_to_dictionary(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>()) const;
	Ref<Resource> dictionary_to_resource(const Dictionary &p_data, const StringName &p_type_hint = StringName()) const;

	bool can_serialize(const Ref<Resource> &p_resource, Format p_format) const;

	String serialize(const Ref<Resource> &p_resource, Format p_format, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>()) const;
	Ref<Resource> deserialize(const String &p_text, Format p_format, const StringName &p_type_hint = StringName(), const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>()) const;
	bool is_format_available(Format p_format) const;
	PackedInt32Array get_available_formats() const;
	String get_format_extension(Format p_format) const;
	Format get_format_from_extension(const String &p_extension) const;

	ResourceTextSerializer();
	~ResourceTextSerializer();
};

VARIANT_ENUM_CAST(ResourceTextSerializer::Format);
