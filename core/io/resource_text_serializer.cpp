/**************************************************************************/
/*  resource_text_serializer.cpp                                          */
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

#include "resource_text_serializer.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/io/missing_resource.h"
#include "core/io/resource_loader.h"
#include "core/io/text_data_format.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/math/vector4.h"
#include "core/math/vector4i.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant_parser.h"

ResourceTextSerializer *ResourceTextSerializer::singleton = nullptr;

// Reserved keys. Property names cannot start with `$` in GDScript or C#, so these never collide with properties.
static constexpr const char *KEY_TYPE = "$type";
static constexpr const char *KEY_FORMAT_VERSION = "$format_version";
static constexpr const char *KEY_SCRIPT = "$script";
static constexpr const char *SECTION_ROOT = "root";

static constexpr const char *SETTING_INDENT = "filesystem/resources/text_formats/indent";
static constexpr const char *SETTING_ENUMS_AS_NAMES = "filesystem/resources/text_formats/enums_as_names";
static constexpr const char *SETTING_SORT_KEYS = "filesystem/resources/text_formats/sort_keys";
static constexpr const char *SETTING_FULL_PRECISION_FLOATS = "filesystem/resources/text_formats/full_precision_floats";
static constexpr const char *SETTING_CSV_DELIMITER = "filesystem/resources/text_formats/csv_delimiter";
static constexpr const char *SETTING_CSV_HEADERS = "filesystem/resources/text_formats/csv_headers";
static constexpr const char *SETTING_INI_VALUE_STYLE = "filesystem/resources/text_formats/ini_value_style";
// Bool setting before ini_value_style; still read if a project.godot has it and ini_value_style is the default.
static constexpr const char *SETTING_INI_PLAIN_VALUES_LEGACY = "filesystem/resources/text_formats/ini_plain_values";
static constexpr const char *SETTING_XML_DECLARATION = "filesystem/resources/text_formats/xml_declaration";

// Set while a resource is encoded with SerializerOptions.full_precision_floats (see _plain_real()).
static thread_local bool tls_full_precision_floats = false;

namespace {

struct FullPrecisionScope {
	bool previous = false;
	explicit FullPrecisionScope(bool p_enable) {
		previous = tls_full_precision_floats;
		tls_full_precision_floats = p_enable;
	}
	~FullPrecisionScope() {
		tls_full_precision_floats = previous;
	}
};

} // namespace

ResourceTextSerializer *ResourceTextSerializer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Settings and options.

void ResourceTextSerializer::register_settings() {
	GLOBAL_DEF(SETTING_INDENT, "  ");
	GLOBAL_DEF(SETTING_ENUMS_AS_NAMES, true);
	GLOBAL_DEF(SETTING_SORT_KEYS, false);
	GLOBAL_DEF(SETTING_FULL_PRECISION_FLOATS, false);
	GLOBAL_DEF(SETTING_CSV_DELIMITER, ",");
	GLOBAL_DEF(SETTING_CSV_HEADERS, true);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_INI_VALUE_STYLE, PROPERTY_HINT_ENUM, "Plain,Godot Variant"), int(SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT));
	GLOBAL_DEF(SETTING_XML_DECLARATION, true);
}

ResourceTextSerializer::Options ResourceTextSerializer::get_default_options() {
	Options options;
	const ProjectSettings *settings = ProjectSettings::get_singleton();
	if (settings == nullptr) {
		return options;
	}
	if (settings->has_setting(SETTING_INDENT)) {
		options.indent = settings->get_setting_with_override(SETTING_INDENT);
	}
	if (settings->has_setting(SETTING_ENUMS_AS_NAMES)) {
		options.enums_as_names = settings->get_setting_with_override(SETTING_ENUMS_AS_NAMES);
	}
	if (settings->has_setting(SETTING_SORT_KEYS)) {
		options.sort_keys = settings->get_setting_with_override(SETTING_SORT_KEYS);
	}
	if (settings->has_setting(SETTING_FULL_PRECISION_FLOATS)) {
		options.full_precision_floats = settings->get_setting_with_override(SETTING_FULL_PRECISION_FLOATS);
	}
	if (settings->has_setting(SETTING_CSV_DELIMITER)) {
		options.csv_delimiter = settings->get_setting_with_override(SETTING_CSV_DELIMITER);
	}
	if (settings->has_setting(SETTING_CSV_HEADERS)) {
		options.csv_headers = settings->get_setting_with_override(SETTING_CSV_HEADERS);
	}
	const int ini_style = settings->has_setting(SETTING_INI_VALUE_STYLE) ? int(settings->get_setting_with_override(SETTING_INI_VALUE_STYLE)) : int(SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT);
	if (ini_style != SerializerOptions::INI_VALUE_STYLE_GODOT_VARIANT) {
		options.ini_plain_values = ini_style == SerializerOptions::INI_VALUE_STYLE_PLAIN;
	} else if (settings->has_setting(SETTING_INI_PLAIN_VALUES_LEGACY)) {
		options.ini_plain_values = settings->get_setting_with_override(SETTING_INI_PLAIN_VALUES_LEGACY);
	}
	if (settings->has_setting(SETTING_XML_DECLARATION)) {
		options.xml_declaration = settings->get_setting_with_override(SETTING_XML_DECLARATION);
	}
	return options;
}

ResourceTextSerializer::Options ResourceTextSerializer::get_options(const Ref<SerializerOptions> &p_options) {
	if (p_options.is_null()) {
		return get_default_options();
	}
	// Read through the getters once per call, so the formats never call Object::get().
	Options options;
	options.indent = p_options->get_indent();
	options.enums_as_names = p_options->is_enums_as_names();
	options.defaults = p_options->get_defaults();
	options.include_type_marker = p_options->is_include_type_marker();
	options.sort_keys = p_options->is_sort_keys();
	options.full_precision_floats = p_options->is_full_precision_floats();
	options.csv_delimiter = p_options->get_csv_delimiter();
	options.csv_headers = p_options->is_csv_headers();
	options.ini_plain_values = p_options->get_ini_value_style() == SerializerOptions::INI_VALUE_STYLE_PLAIN;
	options.xml_declaration = p_options->is_xml_declaration();
	return options;
}

static char32_t _csv_delimiter(const ResourceTextSerializer::Options &p_options) {
	if (p_options.csv_delimiter.length() != 1) {
		return ',';
	}
	const char32_t c = p_options.csv_delimiter[0];
	if (c == '"' || c == '\n' || c == '\r') {
		return ',';
	}
	return c;
}

/////////////////////////////////////////////////////////////////////////////
// Types.

bool ResourceTextSerializer::is_resource_type(const String &p_type) {
	if (p_type.is_empty()) {
		return false;
	}
	// Renamed classes of old files.
	const String type_name = ClassDB::get_compatibility_remapped_class(p_type);
	if (ClassDB::class_exists(type_name)) {
		return ClassDB::is_parent_class(type_name, Resource::get_class_static());
	}
	if (ScriptServer::is_global_class(type_name)) {
		return ClassDB::is_parent_class(ScriptServer::get_global_class_native_base(type_name), Resource::get_class_static());
	}
	return false;
}

String ResourceTextSerializer::get_type_name(const Object *p_object) {
	ERR_FAIL_NULL_V(p_object, String());
	const Ref<Script> script = p_object->get_script();
	if (script.is_valid() && script->get_global_name() != StringName()) {
		return script->get_global_name();
	}
	return p_object->get_class();
}

bool ResourceTextSerializer::is_of_type(const Ref<Resource> &p_resource, const String &p_type) {
	if (p_resource.is_null()) {
		return false;
	}
	if (p_type.is_empty() || p_resource->is_class(p_type)) {
		return true;
	}
	Ref<Script> script = p_resource->get_script();
	while (script.is_valid()) {
		if (String(script->get_global_name()) == p_type) {
			return true;
		}
		script = script->get_base_script();
	}
	return false;
}

static Ref<Resource> _instantiate_script(const Ref<Script> &p_script, String &r_error) {
	if (p_script.is_null()) {
		r_error = "Invalid script.";
		return Ref<Resource>();
	}
	if (p_script->is_abstract()) {
		r_error = vformat("Script '%s' is abstract.", p_script->get_path());
		return Ref<Resource>();
	}
	const StringName native = p_script->get_instance_base_type();
	if (!ClassDB::is_parent_class(native, Resource::get_class_static()) || !ClassDB::can_instantiate(native)) {
		r_error = vformat("Script '%s' does not extend an instantiable Resource class.", p_script->get_path());
		return Ref<Resource>();
	}
	Object *object = ClassDB::instantiate(native);
	Resource *resource = Object::cast_to<Resource>(object);
	if (resource == nullptr) {
		if (object != nullptr) {
			memdelete(object);
		}
		r_error = vformat("Can't create an instance of '%s'.", native);
		return Ref<Resource>();
	}
	Ref<Resource> ref(resource);
	ref->set_script(p_script);
	return ref;
}

Ref<Resource> ResourceTextSerializer::instantiate_type(const String &p_type, String &r_error) {
	const String type_name = ClassDB::get_compatibility_remapped_class(p_type); // Renamed classes of old files.
	if (ClassDB::class_exists(type_name)) {
		if (!ClassDB::is_parent_class(type_name, Resource::get_class_static())) {
			r_error = vformat("'%s' is not a Resource class.", type_name);
			return Ref<Resource>();
		}
		if (!ClassDB::can_instantiate(type_name)) {
			r_error = vformat("'%s' can't be instantiated (abstract or not exposed).", type_name);
			return Ref<Resource>();
		}
		Object *object = ClassDB::instantiate(type_name);
		Resource *resource = Object::cast_to<Resource>(object);
		if (resource == nullptr) {
			if (object != nullptr) {
				memdelete(object);
			}
			r_error = vformat("Can't create an instance of '%s'.", type_name);
			return Ref<Resource>();
		}
		return Ref<Resource>(resource);
	}
	if (ScriptServer::is_global_class(type_name)) {
		const String path = ScriptServer::get_global_class_path(type_name);
		const Ref<Script> script = ResourceLoader::load(path, Script::get_class_static());
		if (script.is_null()) {
			r_error = vformat("Can't load the script '%s' of class '%s'.", path, type_name);
			return Ref<Resource>();
		}
		return _instantiate_script(script, r_error);
	}
	r_error = vformat("Unknown type '%s'.", type_name);
	return Ref<Resource>();
}

// An instance of the same type, for comparing with defaults.
static Ref<Resource> _instantiate_like(const Ref<Resource> &p_resource, String &r_error) {
	const Ref<Script> script = p_resource->get_script();
	if (script.is_valid()) {
		return _instantiate_script(script, r_error);
	}
	return ResourceTextSerializer::instantiate_type(p_resource->get_class(), r_error);
}

/////////////////////////////////////////////////////////////////////////////
// Properties.

static bool _is_serialized_property(const PropertyInfo &p_info) {
	if (!(p_info.usage & PROPERTY_USAGE_STORAGE) || (p_info.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT)) {
		return false;
	}
	if (p_info.name == "script" || p_info.name == "resource_path" || p_info.name == META_PROPERTY_MISSING_RESOURCES || p_info.name.begins_with("__editor")) {
		return false;
	}
	return true;
}

// Values of the base class Resource that are only written when they are set.
static bool _is_skipped_default(const String &p_name, const Variant &p_value) {
	if (p_name == "resource_local_to_scene") {
		return !bool(p_value);
	}
	if (p_name == "resource_name") {
		return String(p_value).is_empty();
	}
	return false;
}

static void _get_serialized_properties(const Object *p_object, LocalVector<PropertyInfo> &r_properties) {
	List<PropertyInfo> list;
	p_object->get_property_list(&list);
	for (const PropertyInfo &info : list) {
		if (_is_serialized_property(info)) {
			r_properties.push_back(info);
		}
	}
}

void ResourceTextSerializer::get_serialized_properties(const Object *p_object, LocalVector<PropertyInfo> &r_properties) {
	_get_serialized_properties(p_object, r_properties);
}

typedef Pair<String, int64_t> EnumValue;

static bool _get_enum_values(const PropertyInfo &p_info, LocalVector<EnumValue> &r_values) {
	if (p_info.type != Variant::INT) {
		return false;
	}
	if (p_info.hint == PROPERTY_HINT_ENUM && !p_info.hint_string.is_empty()) {
		int64_t next = 0;
		const Vector<String> entries = p_info.hint_string.split(",");
		for (const String &entry : entries) {
			String name = entry;
			int64_t value = next;
			const int colon = entry.rfind_char(':');
			if (colon >= 0 && entry.substr(colon + 1).strip_edges().is_valid_int()) {
				name = entry.substr(0, colon);
				value = entry.substr(colon + 1).strip_edges().to_int();
			}
			name = name.strip_edges();
			if (!name.is_empty()) {
				r_values.push_back(EnumValue(name, value));
			}
			next = value + 1;
		}
		return !r_values.is_empty();
	}
	if ((p_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM) && !(p_info.usage & PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
		const String class_enum = p_info.class_name;
		const int dot = class_enum.rfind_char('.');
		if (dot <= 0) {
			return false;
		}
		const StringName class_name = class_enum.substr(0, dot);
		const StringName enum_name = class_enum.substr(dot + 1);
		if (!ClassDB::class_exists(class_name)) {
			return false;
		}
		List<StringName> constants;
		ClassDB::get_enum_constants(class_name, enum_name, &constants);
		for (const StringName &constant : constants) {
			bool valid = false;
			const int64_t value = ClassDB::get_integer_constant(class_name, constant, &valid);
			if (valid) {
				r_values.push_back(EnumValue(constant, value));
			}
		}
		return !r_values.is_empty();
	}
	return false;
}

bool ResourceTextSerializer::is_enum_property(const PropertyInfo &p_info) {
	LocalVector<EnumValue> values;
	return _get_enum_values(p_info, values);
}

// Element type of an Array property: from the typed array value, and enum names from the hint ("2/2:A,B").
static PropertyInfo _get_element_info(const PropertyInfo &p_array_info, const Variant &p_array, Variant &r_script) {
	PropertyInfo element;
	element.usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NIL_IS_VARIANT;
	r_script = Variant();
	if (p_array.get_type() == Variant::ARRAY) {
		const Array array = p_array;
		if (array.is_typed()) {
			element.type = Variant::Type(array.get_typed_builtin());
			element.class_name = array.get_typed_class_name();
			r_script = array.get_typed_script();
		}
	}
	if (p_array_info.hint == PROPERTY_HINT_ARRAY_TYPE && !p_array_info.hint_string.is_empty()) {
		const String hint = p_array_info.hint_string;
		const int colon = hint.find_char(':');
		const String prefix = colon > 0 ? hint.substr(0, colon) : String();
		const String type_part = prefix.get_slicec('/', 0);
		if (colon > 0 && type_part.is_valid_int()) {
			element.type = Variant::Type(type_part.to_int());
			if (prefix.contains_char('/')) {
				element.hint = PropertyHint(prefix.get_slicec('/', 1).to_int());
			}
			element.hint_string = hint.substr(colon + 1);
		} else if (element.type == Variant::OBJECT && element.class_name == StringName()) {
			element.class_name = hint;
		}
	}
	return element;
}

// Declared type of a resource value: the script class, the resource hint or the class name.
static String _get_declared_type(const PropertyInfo &p_info, const Variant &p_script) {
	const Ref<Script> script = p_script;
	if (script.is_valid() && script->get_global_name() != StringName()) {
		return script->get_global_name();
	}
	if (p_info.hint == PROPERTY_HINT_RESOURCE_TYPE && !p_info.hint_string.is_empty() && !p_info.hint_string.contains_char(',')) {
		return p_info.hint_string;
	}
	return p_info.class_name;
}

/////////////////////////////////////////////////////////////////////////////
// Plain values.

// Shortest decimal form of a real_t, so 0.1f is written as 0.1 and not as 0.10000000149011612,
// unless SerializerOptions.full_precision_floats asks for the exact value.
static double _plain_real(real_t p_value) {
#ifdef REAL_T_IS_DOUBLE
	return p_value;
#else
	if (tls_full_precision_floats) {
		return p_value;
	}
	return String::num_scientific(p_value).to_float();
#endif
}

static double _plain_float32(float p_value) {
	if (tls_full_precision_floats) {
		return p_value;
	}
	return String::num_scientific(p_value).to_float();
}

// A plain float that is exactly a 32-bit float (a C++ `float` property read as a Variant, such as 0.2f read as
// 0.20000000298023224) is written in its shortest 32-bit form (0.2), like the components of vectors. Other values,
// such as 0.1 + 0.2, are 64-bit and written exactly. SerializerOptions.full_precision_floats writes every value exactly.
static double _plain_float(double p_value) {
	if (tls_full_precision_floats || !Math::is_finite(p_value)) {
		return p_value;
	}
	const float as_float32 = float(p_value);
	if (double(as_float32) != p_value) {
		return p_value;
	}
	return String::num_scientific(as_float32).to_float();
}

static bool _is_math_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::RECT2:
		case Variant::RECT2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::TRANSFORM2D:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
		case Variant::PLANE:
		case Variant::QUATERNION:
		case Variant::AABB:
		case Variant::BASIS:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
			return true;
		default:
			return false;
	}
}

static bool _is_packed_type(Variant::Type p_type) {
	return p_type >= Variant::PACKED_BYTE_ARRAY && p_type < Variant::VARIANT_MAX;
}

static Variant::Type _packed_element_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
			return Variant::INT;
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
			return Variant::FLOAT;
		case Variant::PACKED_STRING_ARRAY:
			return Variant::STRING;
		case Variant::PACKED_VECTOR2_ARRAY:
			return Variant::VECTOR2;
		case Variant::PACKED_VECTOR3_ARRAY:
			return Variant::VECTOR3;
		case Variant::PACKED_COLOR_ARRAY:
			return Variant::COLOR;
		case Variant::PACKED_VECTOR4_ARRAY:
			return Variant::VECTOR4;
		default:
			return Variant::NIL;
	}
}

// Components of a math type, in a fixed order (vectors x, y, z, w; rects position then size; matrices row by row or column by column).
static void _math_to_components(const Variant &p_value, LocalVector<double> &r_components, bool &r_integer) {
	r_integer = false;
	switch (p_value.get_type()) {
		case Variant::VECTOR2: {
			const Vector2 v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.x), _plain_real(v.y) };
		} break;
		case Variant::VECTOR2I: {
			const Vector2i v = p_value;
			r_components = LocalVector<double>{ double(v.x), double(v.y) };
			r_integer = true;
		} break;
		case Variant::RECT2: {
			const Rect2 v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.position.x), _plain_real(v.position.y), _plain_real(v.size.x), _plain_real(v.size.y) };
		} break;
		case Variant::RECT2I: {
			const Rect2i v = p_value;
			r_components = LocalVector<double>{ double(v.position.x), double(v.position.y), double(v.size.x), double(v.size.y) };
			r_integer = true;
		} break;
		case Variant::VECTOR3: {
			const Vector3 v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.x), _plain_real(v.y), _plain_real(v.z) };
		} break;
		case Variant::VECTOR3I: {
			const Vector3i v = p_value;
			r_components = LocalVector<double>{ double(v.x), double(v.y), double(v.z) };
			r_integer = true;
		} break;
		case Variant::TRANSFORM2D: {
			const Transform2D v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.columns[0].x), _plain_real(v.columns[0].y), _plain_real(v.columns[1].x), _plain_real(v.columns[1].y), _plain_real(v.columns[2].x), _plain_real(v.columns[2].y) };
		} break;
		case Variant::VECTOR4: {
			const Vector4 v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.x), _plain_real(v.y), _plain_real(v.z), _plain_real(v.w) };
		} break;
		case Variant::VECTOR4I: {
			const Vector4i v = p_value;
			r_components = LocalVector<double>{ double(v.x), double(v.y), double(v.z), double(v.w) };
			r_integer = true;
		} break;
		case Variant::PLANE: {
			const Plane v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.normal.x), _plain_real(v.normal.y), _plain_real(v.normal.z), _plain_real(v.d) };
		} break;
		case Variant::QUATERNION: {
			const Quaternion v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.x), _plain_real(v.y), _plain_real(v.z), _plain_real(v.w) };
		} break;
		case Variant::AABB: {
			const ::AABB v = p_value;
			r_components = LocalVector<double>{ _plain_real(v.position.x), _plain_real(v.position.y), _plain_real(v.position.z), _plain_real(v.size.x), _plain_real(v.size.y), _plain_real(v.size.z) };
		} break;
		case Variant::BASIS: {
			const Basis v = p_value;
			r_components.clear();
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					r_components.push_back(_plain_real(v.rows[i][j]));
				}
			}
		} break;
		case Variant::TRANSFORM3D: {
			const Transform3D v = p_value;
			r_components.clear();
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					r_components.push_back(_plain_real(v.basis.rows[i][j]));
				}
			}
			for (int i = 0; i < 3; i++) {
				r_components.push_back(_plain_real(v.origin[i]));
			}
		} break;
		case Variant::PROJECTION: {
			const Projection v = p_value;
			r_components.clear();
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					r_components.push_back(_plain_real(v.columns[i][j]));
				}
			}
		} break;
		case Variant::COLOR: {
			const Color v = p_value;
			r_components = LocalVector<double>{ _plain_float32(v.r), _plain_float32(v.g), _plain_float32(v.b), _plain_float32(v.a) };
		} break;
		default: {
			r_components.clear();
		} break;
	}
}

static bool _components_to_math(Variant::Type p_type, const LocalVector<double> &p_c, Variant &r_value) {
	const uint32_t n = p_c.size();
	switch (p_type) {
		case Variant::VECTOR2: {
			if (n != 2) {
				return false;
			}
			r_value = Vector2(p_c[0], p_c[1]);
		} break;
		case Variant::VECTOR2I: {
			if (n != 2) {
				return false;
			}
			r_value = Vector2i(int32_t(p_c[0]), int32_t(p_c[1]));
		} break;
		case Variant::RECT2: {
			if (n != 4) {
				return false;
			}
			r_value = Rect2(p_c[0], p_c[1], p_c[2], p_c[3]);
		} break;
		case Variant::RECT2I: {
			if (n != 4) {
				return false;
			}
			r_value = Rect2i(int32_t(p_c[0]), int32_t(p_c[1]), int32_t(p_c[2]), int32_t(p_c[3]));
		} break;
		case Variant::VECTOR3: {
			if (n != 3) {
				return false;
			}
			r_value = Vector3(p_c[0], p_c[1], p_c[2]);
		} break;
		case Variant::VECTOR3I: {
			if (n != 3) {
				return false;
			}
			r_value = Vector3i(int32_t(p_c[0]), int32_t(p_c[1]), int32_t(p_c[2]));
		} break;
		case Variant::TRANSFORM2D: {
			if (n != 6) {
				return false;
			}
			r_value = Transform2D(p_c[0], p_c[1], p_c[2], p_c[3], p_c[4], p_c[5]);
		} break;
		case Variant::VECTOR4: {
			if (n != 4) {
				return false;
			}
			r_value = Vector4(p_c[0], p_c[1], p_c[2], p_c[3]);
		} break;
		case Variant::VECTOR4I: {
			if (n != 4) {
				return false;
			}
			r_value = Vector4i(int32_t(p_c[0]), int32_t(p_c[1]), int32_t(p_c[2]), int32_t(p_c[3]));
		} break;
		case Variant::PLANE: {
			if (n != 4) {
				return false;
			}
			r_value = Plane(p_c[0], p_c[1], p_c[2], p_c[3]);
		} break;
		case Variant::QUATERNION: {
			if (n != 4) {
				return false;
			}
			r_value = Quaternion(p_c[0], p_c[1], p_c[2], p_c[3]);
		} break;
		case Variant::AABB: {
			if (n != 6) {
				return false;
			}
			r_value = ::AABB(Vector3(p_c[0], p_c[1], p_c[2]), Vector3(p_c[3], p_c[4], p_c[5]));
		} break;
		case Variant::BASIS: {
			if (n != 9) {
				return false;
			}
			r_value = Basis(p_c[0], p_c[1], p_c[2], p_c[3], p_c[4], p_c[5], p_c[6], p_c[7], p_c[8]);
		} break;
		case Variant::TRANSFORM3D: {
			if (n != 12) {
				return false;
			}
			r_value = Transform3D(Basis(p_c[0], p_c[1], p_c[2], p_c[3], p_c[4], p_c[5], p_c[6], p_c[7], p_c[8]), Vector3(p_c[9], p_c[10], p_c[11]));
		} break;
		case Variant::PROJECTION: {
			if (n != 16) {
				return false;
			}
			r_value = Projection(Vector4(p_c[0], p_c[1], p_c[2], p_c[3]), Vector4(p_c[4], p_c[5], p_c[6], p_c[7]), Vector4(p_c[8], p_c[9], p_c[10], p_c[11]), Vector4(p_c[12], p_c[13], p_c[14], p_c[15]));
		} break;
		case Variant::COLOR: {
			if (n == 3) {
				r_value = Color(p_c[0], p_c[1], p_c[2]);
			} else if (n == 4) {
				r_value = Color(p_c[0], p_c[1], p_c[2], p_c[3]);
			} else {
				return false;
			}
		} break;
		default:
			return false;
	}
	return true;
}

static bool _to_double(const Variant &p_value, double &r_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
			r_value = bool(p_value) ? 1.0 : 0.0;
			return true;
		case Variant::INT:
			r_value = double(int64_t(p_value));
			return true;
		case Variant::FLOAT:
			r_value = double(p_value);
			return true;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			const String text = String(p_value).strip_edges();
			if (text.is_valid_int() || text.is_valid_float()) {
				r_value = text.to_float();
				return true;
			}
			return false;
		}
		default:
			return false;
	}
}

static bool _to_int(const Variant &p_value, int64_t &r_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
			r_value = bool(p_value) ? 1 : 0;
			return true;
		case Variant::INT:
			r_value = p_value;
			return true;
		case Variant::FLOAT:
			r_value = int64_t(double(p_value));
			return true;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			const String text = String(p_value).strip_edges();
			if (text.is_valid_int()) {
				r_value = text.to_int();
				return true;
			}
			if (text.is_valid_float()) {
				r_value = int64_t(text.to_float());
				return true;
			}
			return false;
		}
		default:
			return false;
	}
}

static bool _to_components(const Variant &p_value, LocalVector<double> &r_components) {
	r_components.clear();
	const Variant::Type type = p_value.get_type();
	if (_is_math_type(type) || type == Variant::COLOR) {
		bool integer = false;
		_math_to_components(p_value, r_components, integer);
		return true;
	}
	if (type == Variant::ARRAY || (_is_packed_type(type) && _packed_element_type(type) != Variant::STRING)) {
		const Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			double value = 0.0;
			if (!_to_double(array[i], value)) {
				return false;
			}
			r_components.push_back(value);
		}
		return true;
	}
	if (type == Variant::STRING || type == Variant::STRING_NAME) {
		String text = String(p_value).strip_edges();
		const int open = text.find_char('(');
		if (open >= 0) {
			const int close = text.rfind_char(')');
			text = text.substr(open + 1, (close > open ? close : text.length()) - open - 1);
		}
		text = text.trim_prefix("[").trim_suffix("]").strip_edges();
		if (text.is_empty()) {
			return false;
		}
		const Vector<String> parts = text.split(",");
		for (const String &part : parts) {
			double value = 0.0;
			if (!_to_double(part, value)) {
				return false;
			}
			r_components.push_back(value);
		}
		return true;
	}
	return false;
}

// "#rrggbbaa" if that is exact, else an array of floats, so no precision is lost.
static Variant _color_to_plain(const Color &p_color) {
	const String html = "#" + p_color.to_html(true);
	if (Color::html(html) == p_color) {
		return html;
	}
	LocalVector<double> components;
	bool integer = false;
	_math_to_components(p_color, components, integer);
	Array array;
	for (double c : components) {
		array.push_back(c);
	}
	return array;
}

static bool _string_to_color(const String &p_text, Color &r_color) {
	const String text = p_text.strip_edges();
	if (Color::html_is_valid(text)) {
		r_color = Color::html(text);
		return true;
	}
	const Color invalid(-1, -1, -1, -1);
	const Color named = Color::named(text, invalid);
	if (named != invalid) {
		r_color = named;
		return true;
	}
	return false;
}

// Splits a plain list ("a, b, \"c, d\"") into its elements. Quoted elements are unescaped.
static Array _split_plain_list(const String &p_text, LocalVector<bool> &r_quoted) {
	Array result;
	r_quoted.clear();
	String text = p_text.strip_edges();
	if (text.is_empty()) {
		return result;
	}
	String current;
	bool in_quotes = false;
	bool quoted = false;
	for (int i = 0; i < text.length(); i++) {
		const char32_t c = text[i];
		if (in_quotes) {
			if (c == '\\' && i + 1 < text.length()) {
				current += c;
				current += text[++i];
			} else if (c == '"') {
				in_quotes = false;
			} else {
				current += c;
			}
		} else if (c == '"' && current.strip_edges().is_empty()) {
			in_quotes = true;
			quoted = true;
			current = String();
		} else if (c == ',') {
			result.push_back(quoted ? current.c_unescape() : current.strip_edges());
			r_quoted.push_back(quoted);
			current = String();
			quoted = false;
		} else {
			current += c;
		}
	}
	result.push_back(quoted ? current.c_unescape() : current.strip_edges());
	r_quoted.push_back(quoted);
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// INI value syntax.

static bool _ini_is_keyword(const String &p_text) {
	return p_text == "true" || p_text == "false" || p_text == "null" || p_text == "inf" || p_text == "-inf" || p_text == "inf_neg" || p_text == "nan";
}

static bool _ini_looks_like_constructor(const String &p_text) {
	const int open = p_text.find_char('(');
	return open > 0 && p_text.substr(0, open).is_valid_ascii_identifier();
}

// Values that the INI reader hands to VariantParser instead of keeping them as text.
static bool _ini_is_typed_syntax(const String &p_text) {
	if (p_text.is_empty()) {
		return false;
	}
	const char32_t first = p_text[0];
	if (first == '"' || first == '[' || first == '{') {
		return true;
	}
	if (p_text.begins_with("&\"") || p_text.begins_with("^\"")) {
		return true;
	}
	return _ini_is_keyword(p_text) || p_text.is_valid_int() || p_text.is_valid_float() || _ini_looks_like_constructor(p_text);
}

static String _ini_quote(const String &p_text) {
	return "\"" + p_text.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
}

static Variant _ini_parse_value(const String &p_raw) {
	const String text = p_raw.strip_edges();
	if (text.is_empty()) {
		return String();
	}
	if (text.is_valid_int()) {
		return text.to_int();
	}
	if (text == "true") {
		return true;
	}
	if (text == "false") {
		return false;
	}
	if (text == "null") {
		return Variant();
	}
	if (text.is_valid_float()) {
		return text.to_float();
	}
	if (_ini_is_typed_syntax(text)) {
		VariantParser::StreamString stream;
		stream.s = text;
		String error;
		int line = 0;
		Variant value;
		if (VariantParser::parse(&stream, value, error, line) == OK) {
			return value;
		}
	}
	return text;
}

static String _ini_number(double p_value) {
	const String text = String::num_scientific(p_value);
	if (text.contains_char('.') || text.contains_char('e') || text.contains_char('n') || text.contains_char('i')) {
		return text;
	}
	return text + ".0";
}

// Variant syntax on one line (strings escaped), readable by ConfigFile.
static String _ini_variant_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::STRING:
			return _ini_quote(p_value);
		case Variant::ARRAY: {
			const Array array = p_value;
			String text = "[";
			for (int i = 0; i < array.size(); i++) {
				if (i > 0) {
					text += ", ";
				}
				text += _ini_variant_text(array[i]);
			}
			return text + "]";
		}
		case Variant::DICTIONARY: {
			const Dictionary dict = p_value;
			String text = "{";
			bool first = true;
			for (const KeyValue<Variant, Variant> &kv : dict) {
				if (!first) {
					text += ", ";
				}
				first = false;
				text += _ini_variant_text(kv.key) + ": " + _ini_variant_text(kv.value);
			}
			return text + "}";
		}
		default: {
			String text;
			VariantWriter::write_to_string(p_value, text, false);
			return text;
		}
	}
}

static bool _ini_plain_string_needs_quotes(const String &p_text) {
	if (p_text.is_empty()) {
		return false;
	}
	if (p_text != p_text.strip_edges() || p_text.contains_char('\n') || p_text.contains_char('\r')) {
		return true;
	}
	return _ini_is_typed_syntax(p_text);
}

// Plain syntax: text without quotes, numbers, true/false, lists as "a, b". Containers that do not fit are written as JSON.
static String _ini_plain_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return "null";
		case Variant::BOOL:
			return bool(p_value) ? "true" : "false";
		case Variant::INT:
			return itos(p_value);
		case Variant::FLOAT:
			return _ini_number(p_value);
		case Variant::STRING: {
			const String text = p_value;
			return _ini_plain_string_needs_quotes(text) ? _ini_quote(text) : text;
		}
		case Variant::ARRAY: {
			const Array array = p_value;
			bool simple = true;
			for (int i = 0; i < array.size() && simple; i++) {
				const Variant &element = array[i];
				switch (element.get_type()) {
					case Variant::BOOL:
					case Variant::INT:
					case Variant::FLOAT:
						break;
					case Variant::STRING: {
						const String text = element;
						simple = !text.is_empty() && !text.contains_char(',') && !text.contains_char('"') && !_ini_plain_string_needs_quotes(text);
					} break;
					default:
						simple = false;
						break;
				}
			}
			if (!simple) {
				return JSON::stringify(array, "", false, true);
			}
			String text;
			for (int i = 0; i < array.size(); i++) {
				if (i > 0) {
					text += ", ";
				}
				text += _ini_plain_text(array[i]);
			}
			return text;
		}
		case Variant::DICTIONARY:
			return JSON::stringify(p_value, "", false, true);
		default:
			return _ini_variant_text(p_value);
	}
}

// Section paths use `/` between parts; parts are escaped so keys such as "metadata/x" stay one part.
static String _section_part_encode(const String &p_part) {
	return p_part.replace("%", "%25").replace("/", "%2F").replace("]", "%5D");
}

static String _section_part_decode(const String &p_part) {
	return p_part.replace("%2F", "/").replace("%5D", "]").replace("%25", "%");
}

/////////////////////////////////////////////////////////////////////////////
// Encoder: Resource -> tree.

namespace {

struct TreeEncoder {
	bool plain = true;
	bool enums_as_names = true;
	bool include_type_marker = true; // `$type` of the root.
	Ref<Resource> defaults; // Root values equal to these are skipped.
	LocalVector<ResourceTextSerializer::Section> *sections = nullptr;
	HashSet<const Resource *> stack;
	String error;

	Dictionary encode_resource(const Ref<Resource> &p_resource, const String &p_declared, bool p_root, const String &p_path);
	Variant encode_value(const Variant &p_value, const PropertyInfo &p_info, const Variant &p_script, const String &p_path, bool &r_omit);
};

Dictionary TreeEncoder::encode_resource(const Ref<Resource> &p_resource, const String &p_declared, bool p_root, const String &p_path) {
	Dictionary result;
	if (stack.has(p_resource.ptr())) {
		error = vformat("The resource at '%s' contains itself (a cycle); cycles can't be written as text.", p_path);
		return result;
	}
	stack.insert(p_resource.ptr());

	if (sections != nullptr) {
		// The section is added before its children, so parents come first in the file.
		sections->push_back(ResourceTextSerializer::Section(p_path, result));
	}

	const String type = ResourceTextSerializer::get_type_name(p_resource.ptr());
	if (p_root ? include_type_marker : type != p_declared) {
		result[KEY_TYPE] = type;
	}
	if (p_root) {
		result[KEY_FORMAT_VERSION] = int(ResourceTextSerializer::TEXT_LAYOUT_VERSION);
	}
	const Ref<Script> script = p_resource->get_script();
	if (script.is_valid() && script->get_global_name() == StringName()) {
		result[KEY_SCRIPT] = script->get_path();
	}

	LocalVector<PropertyInfo> properties;
	_get_serialized_properties(p_resource.ptr(), properties);
	for (const PropertyInfo &info : properties) {
		const Variant value = p_resource->get(info.name);
		if (_is_skipped_default(info.name, value)) {
			continue;
		}
		if (p_root && defaults.is_valid() && !info.name.begins_with("metadata/")) {
			bool has_default = false;
			const Variant default_value = defaults->get(info.name, &has_default);
			if (has_default && default_value == value) {
				continue;
			}
		}
		bool omit = false;
		const Variant encoded = encode_value(value, info, Variant(), p_path + "/" + _section_part_encode(info.name), omit);
		if (!error.is_empty()) {
			break;
		}
		if (!omit) {
			result[info.name] = encoded;
		}
	}

	stack.erase(p_resource.ptr());
	return result;
}

Variant TreeEncoder::encode_value(const Variant &p_value, const PropertyInfo &p_info, const Variant &p_script, const String &p_path, bool &r_omit) {
	r_omit = false;
	const Variant::Type type = p_value.get_type();
	switch (type) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::STRING:
			return p_value;
		case Variant::FLOAT:
			return _plain_float(p_value);
		case Variant::INT: {
			if (enums_as_names) {
				LocalVector<EnumValue> values;
				if (_get_enum_values(p_info, values)) {
					const int64_t number = p_value;
					for (const EnumValue &value : values) {
						if (value.second == number) {
							return value.first;
						}
					}
				}
			}
			return p_value;
		}
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			return plain ? Variant(String(p_value)) : p_value;
		case Variant::COLOR:
			return plain ? _color_to_plain(p_value) : p_value;
		case Variant::OBJECT: {
			const Ref<Resource> resource = p_value;
			if (resource.is_null()) {
				if (p_value.get_validated_object() != nullptr) {
					WARN_PRINT(vformat("'%s' holds an object that is not a Resource; it is written as null.", p_path));
				}
				return Variant();
			}
			if (!resource->is_built_in()) {
				// A resource in its own file is written as its path.
				return resource->get_path();
			}
			const Dictionary nested = encode_resource(resource, _get_declared_type(p_info, p_script), false, p_path);
			if (sections != nullptr) {
				r_omit = true;
				return Variant();
			}
			return nested;
		}
		case Variant::ARRAY: {
			const Array array = p_value;
			Variant element_script;
			const PropertyInfo element_info = _get_element_info(p_info, p_value, element_script);
			Array result;
			int omitted = 0;
			for (int i = 0; i < array.size(); i++) {
				bool omit = false;
				const Variant element = encode_value(array[i], element_info, element_script, p_path + "/" + itos(i), omit);
				if (!error.is_empty()) {
					return Variant();
				}
				if (omit) {
					omitted++;
				}
				result.push_back(element);
			}
			if (omitted > 0 && omitted == array.size()) {
				// All elements are INI sections.
				r_omit = true;
				return Variant();
			}
			return result;
		}
		case Variant::DICTIONARY: {
			const Dictionary dict = p_value;
			PropertyInfo value_info;
			Variant value_script;
			if (dict.is_typed_value()) {
				value_info.type = Variant::Type(dict.get_typed_value_builtin());
				value_info.class_name = dict.get_typed_value_class_name();
				value_script = dict.get_typed_value_script();
			}
			Dictionary result;
			int omitted = 0;
			for (const KeyValue<Variant, Variant> &kv : dict) {
				bool omit = false;
				const Variant key = plain ? Variant(String(kv.key)) : kv.key;
				const Variant value = encode_value(kv.value, value_info, value_script, p_path + "/" + _section_part_encode(String(kv.key)), omit);
				if (!error.is_empty()) {
					return Variant();
				}
				if (omit) {
					omitted++;
				} else {
					result[key] = value;
				}
			}
			if (omitted > 0 && result.is_empty()) {
				r_omit = true;
				return Variant();
			}
			return result;
		}
		case Variant::RID:
		case Variant::CALLABLE:
		case Variant::SIGNAL: {
			WARN_PRINT(vformat("'%s' has the type %s, which can't be written as text; it is skipped.", p_path, Variant::get_type_name(type)));
			r_omit = true;
			return Variant();
		}
		default:
			break;
	}

	if (!plain) {
		return p_value;
	}
	if (_is_math_type(type)) {
		LocalVector<double> components;
		bool integer = false;
		_math_to_components(p_value, components, integer);
		Array result;
		for (double c : components) {
			if (integer) {
				result.push_back(int64_t(c));
			} else {
				result.push_back(c);
			}
		}
		return result;
	}
	if (_is_packed_type(type)) {
		const Array array = p_value;
		PropertyInfo element_info;
		element_info.type = _packed_element_type(type);
		Array result;
		for (int i = 0; i < array.size(); i++) {
			bool omit = false;
			result.push_back(encode_value(array[i], element_info, Variant(), p_path + "/" + itos(i), omit));
		}
		return result;
	}
	return p_value;
}

/////////////////////////////////////////////////////////////////////////////
// Decoder: tree -> Resource.

enum DecodeResult {
	DECODE_OK,
	DECODE_SKIP, // Empty value: keep the default without a warning.
	DECODE_FAILED, // Value does not fit: keep the default with a warning.
};

struct TreeDecoder {
	String error;
	int depth = 0;

	Ref<Resource> decode_resource(const Dictionary &p_data, const String &p_declared, const Variant &p_script, const String &p_where);
	DecodeResult decode_value(const Variant &p_data, const PropertyInfo &p_info, const Variant &p_current, const Variant &p_script, const String &p_where, Variant &r_value);
	Variant decode_variant(const Variant &p_data, const String &p_where);
};

Ref<Resource> TreeDecoder::decode_resource(const Dictionary &p_data, const String &p_declared, const Variant &p_script, const String &p_where) {
	if (depth > Variant::MAX_RECURSION_DEPTH) {
		error = vformat("'%s' is nested too deeply.", p_where);
		return Ref<Resource>();
	}

	String type;
	const Variant type_value = p_data.get(KEY_TYPE, Variant());
	if (type_value.get_type() == Variant::STRING || type_value.get_type() == Variant::STRING_NAME) {
		type = type_value;
		if (!ResourceTextSerializer::is_resource_type(type)) {
			if (depth == 0 && p_declared.is_empty()) {
				error = vformat("'%s' is not a known Resource type.", type);
				return Ref<Resource>();
			}
			WARN_PRINT(vformat("'%s': '%s' is not a known Resource type; the declared type is used.", p_where, type));
			type = String();
		}
	}

	String instantiate_error;
	Ref<Resource> resource;
	const Ref<Script> declared_script = p_script;
	if (!type.is_empty()) {
		resource = ResourceTextSerializer::instantiate_type(type, instantiate_error);
	} else if (declared_script.is_valid()) {
		resource = _instantiate_script(declared_script, instantiate_error);
	} else if (!p_declared.is_empty()) {
		resource = ResourceTextSerializer::instantiate_type(p_declared, instantiate_error);
	} else {
		instantiate_error = vformat("'%s' has no type: add the key \"%s\" or pass a type hint.", p_where, KEY_TYPE);
	}
	if (resource.is_null()) {
		if (depth == 0) {
			error = instantiate_error;
		} else {
			WARN_PRINT(vformat("'%s': %s", p_where, instantiate_error));
		}
		return Ref<Resource>();
	}

	const Variant script_path = p_data.get(KEY_SCRIPT, Variant());
	if (script_path.get_type() == Variant::STRING && resource->get_script_instance() == nullptr) {
		const Ref<Script> script = ResourceLoader::load(script_path, Script::get_class_static());
		if (script.is_valid()) {
			resource->set_script(script);
		} else {
			WARN_PRINT(vformat("'%s': can't load the script '%s'.", p_where, script_path));
		}
	}

	LocalVector<PropertyInfo> properties;
	_get_serialized_properties(resource.ptr(), properties);
	HashMap<String, PropertyInfo> by_name;
	for (const PropertyInfo &info : properties) {
		by_name.insert(info.name, info);
	}

	depth++;
	for (const KeyValue<Variant, Variant> &kv : p_data) {
		const String key = kv.key;
		if (key == KEY_TYPE || key == KEY_SCRIPT) {
			continue;
		}
		if (key == KEY_FORMAT_VERSION) {
			int64_t version = 0;
			if (_to_int(kv.value, version) && version > ResourceTextSerializer::TEXT_LAYOUT_VERSION) {
				WARN_PRINT(vformat("'%s' was written with format version %d; this engine reads version %d.", p_where, version, int(ResourceTextSerializer::TEXT_LAYOUT_VERSION)));
			}
			continue;
		}
		const String where = p_where + "/" + key;
		const PropertyInfo *found = by_name.getptr(key);
		if (found == nullptr) {
			if (key.begins_with("metadata/")) {
				resource->set_meta(key.substr(9), decode_variant(kv.value, where));
			} else {
				WARN_PRINT(vformat("'%s': unknown key, skipped (%s has no stored property '%s').", where, ResourceTextSerializer::get_type_name(resource.ptr()), key));
			}
			continue;
		}
		const PropertyInfo &info = *found;
		Variant value;
		const DecodeResult result = decode_value(kv.value, info, resource->get(info.name), Variant(), where, value);
		if (!error.is_empty()) {
			depth--;
			return Ref<Resource>();
		}
		if (result == DECODE_FAILED) {
			WARN_PRINT(vformat("'%s': the value does not fit the type %s; the default is kept.", where, Variant::get_type_name(info.type)));
			continue;
		}
		if (result == DECODE_SKIP) {
			continue;
		}
		bool valid = false;
		resource->set(info.name, value, &valid);
		if (!valid) {
			WARN_PRINT(vformat("'%s': the value could not be set.", where));
		}
	}
	depth--;
	return resource;
}

Variant TreeDecoder::decode_variant(const Variant &p_data, const String &p_where) {
	if (p_data.get_type() == Variant::DICTIONARY) {
		const Dictionary dict = p_data;
		const Variant type = dict.get(KEY_TYPE, Variant());
		if (type.get_type() == Variant::STRING && ResourceTextSerializer::is_resource_type(type)) {
			depth++;
			const Ref<Resource> resource = decode_resource(dict, String(), Variant(), p_where);
			depth--;
			return resource;
		}
		Dictionary result;
		for (const KeyValue<Variant, Variant> &kv : dict) {
			result[kv.key] = decode_variant(kv.value, p_where + "/" + String(kv.key));
		}
		return result;
	}
	if (p_data.get_type() == Variant::ARRAY) {
		const Array array = p_data;
		Array result;
		for (int i = 0; i < array.size(); i++) {
			result.push_back(decode_variant(array[i], p_where + "/" + itos(i)));
		}
		return result;
	}
	return p_data;
}

DecodeResult TreeDecoder::decode_value(const Variant &p_data, const PropertyInfo &p_info, const Variant &p_current, const Variant &p_script, const String &p_where, Variant &r_value) {
	const Variant::Type target = p_info.type;
	const Variant::Type source = p_data.get_type();
	const bool is_text = source == Variant::STRING || source == Variant::STRING_NAME;
	const bool empty_text = is_text && String(p_data).strip_edges().is_empty();

	switch (target) {
		case Variant::NIL: {
			r_value = decode_variant(p_data, p_where);
			return DECODE_OK;
		}
		case Variant::BOOL: {
			if (source == Variant::BOOL) {
				r_value = p_data;
				return DECODE_OK;
			}
			if (source == Variant::INT || source == Variant::FLOAT) {
				r_value = double(p_data) != 0.0;
				return DECODE_OK;
			}
			if (is_text) {
				if (empty_text) {
					return DECODE_SKIP;
				}
				const String text = String(p_data).strip_edges().to_lower();
				if (text == "true" || text == "yes" || text == "on" || text == "1") {
					r_value = true;
					return DECODE_OK;
				}
				if (text == "false" || text == "no" || text == "off" || text == "0") {
					r_value = false;
					return DECODE_OK;
				}
			}
			return DECODE_FAILED;
		}
		case Variant::INT: {
			if (is_text) {
				if (empty_text) {
					return DECODE_SKIP;
				}
				LocalVector<EnumValue> values;
				if (_get_enum_values(p_info, values)) {
					const String text = String(p_data).strip_edges();
					for (const EnumValue &value : values) {
						if (value.first == text) {
							r_value = value.second;
							return DECODE_OK;
						}
					}
					for (const EnumValue &value : values) {
						if (value.first.nocasecmp_to(text) == 0) {
							r_value = value.second;
							return DECODE_OK;
						}
					}
				}
			}
			int64_t number = 0;
			if (_to_int(p_data, number)) {
				r_value = number;
				return DECODE_OK;
			}
			return DECODE_FAILED;
		}
		case Variant::FLOAT: {
			if (empty_text) {
				return DECODE_SKIP;
			}
			double number = 0.0;
			if (_to_double(p_data, number)) {
				r_value = number;
				return DECODE_OK;
			}
			return DECODE_FAILED;
		}
		case Variant::STRING: {
			if (source == Variant::NIL) {
				r_value = String();
			} else if (source == Variant::DICTIONARY || source == Variant::ARRAY) {
				return DECODE_FAILED;
			} else {
				r_value = String(p_data);
			}
			return DECODE_OK;
		}
		case Variant::STRING_NAME: {
			if (source == Variant::DICTIONARY || source == Variant::ARRAY) {
				return DECODE_FAILED;
			}
			r_value = source == Variant::NIL ? StringName() : StringName(String(p_data));
			return DECODE_OK;
		}
		case Variant::NODE_PATH: {
			if (source == Variant::DICTIONARY || source == Variant::ARRAY) {
				return DECODE_FAILED;
			}
			r_value = source == Variant::NIL ? NodePath() : NodePath(String(p_data));
			return DECODE_OK;
		}
		case Variant::COLOR: {
			if (source == Variant::COLOR) {
				r_value = p_data;
				return DECODE_OK;
			}
			if (is_text) {
				if (empty_text) {
					return DECODE_SKIP;
				}
				Color color;
				if (_string_to_color(p_data, color)) {
					r_value = color;
					return DECODE_OK;
				}
			}
			LocalVector<double> components;
			if (_to_components(p_data, components) && _components_to_math(Variant::COLOR, components, r_value)) {
				return DECODE_OK;
			}
			return DECODE_FAILED;
		}
		case Variant::OBJECT: {
			if (source == Variant::NIL || empty_text || (is_text && String(p_data).strip_edges() == "null")) {
				r_value = Variant();
				return DECODE_OK;
			}
			if (source == Variant::OBJECT) {
				r_value = p_data;
				return DECODE_OK;
			}
			const String declared = _get_declared_type(p_info, p_script);
			const Ref<Script> declared_script = p_script;
			if (declared_script.is_null() && !declared.is_empty() && !ResourceTextSerializer::is_resource_type(declared)) {
				// Only resources can be read from text.
				return DECODE_FAILED;
			}
			if (is_text) {
				const String path = String(p_data).strip_edges();
				const Ref<Resource> resource = ResourceLoader::load(path, ResourceTextSerializer::is_resource_type(declared) ? declared : String());
				if (resource.is_null()) {
					WARN_PRINT(vformat("'%s': can't load the resource '%s'.", p_where, path));
					return DECODE_SKIP;
				}
				r_value = resource;
				return DECODE_OK;
			}
			if (source == Variant::DICTIONARY) {
				const Ref<Resource> resource = decode_resource(p_data, declared, p_script, p_where);
				if (!error.is_empty()) {
					return DECODE_FAILED;
				}
				if (resource.is_null()) {
					return DECODE_SKIP;
				}
				r_value = resource;
				return DECODE_OK;
			}
			return DECODE_FAILED;
		}
		case Variant::ARRAY: {
			Array input;
			LocalVector<bool> quoted;
			if (source == Variant::ARRAY) {
				input = p_data;
			} else if (_is_packed_type(source)) {
				input = p_data;
			} else if (is_text) {
				input = _split_plain_list(p_data, quoted);
			} else if (source == Variant::NIL) {
				input = Array();
			} else {
				input.push_back(p_data);
			}

			Variant element_script;
			const PropertyInfo element_info = _get_element_info(p_info, p_current, element_script);
			Array result;
			if (p_current.get_type() == Variant::ARRAY) {
				const Array current = p_current;
				if (current.is_typed()) {
					result.set_typed(current.get_typed_builtin(), current.get_typed_class_name(), current.get_typed_script());
				}
			}
			for (int i = 0; i < input.size(); i++) {
				Variant element = input[i];
				if (is_text && element_info.type == Variant::NIL && i < int(quoted.size()) && !quoted[i]) {
					// Untyped list in plain text: numbers and booleans get their type back.
					element = _ini_parse_value(element);
				}
				Variant value;
				const DecodeResult element_result = decode_value(element, element_info, Variant(), element_script, p_where + "/" + itos(i), value);
				if (!error.is_empty()) {
					return DECODE_FAILED;
				}
				if (element_result != DECODE_OK) {
					WARN_PRINT(vformat("'%s/%d': the element does not fit the type %s; it is skipped.", p_where, i, Variant::get_type_name(element_info.type)));
					continue;
				}
				result.push_back(value);
			}
			r_value = result;
			return DECODE_OK;
		}
		case Variant::DICTIONARY: {
			Dictionary input;
			if (source == Variant::DICTIONARY) {
				input = p_data;
			} else if (is_text) {
				if (empty_text) {
					r_value = Dictionary();
					return DECODE_OK;
				}
				const Variant parsed = _ini_parse_value(p_data);
				if (parsed.get_type() != Variant::DICTIONARY) {
					return DECODE_FAILED;
				}
				input = parsed;
			} else if (source == Variant::NIL) {
				r_value = Dictionary();
				return DECODE_OK;
			} else {
				return DECODE_FAILED;
			}

			PropertyInfo key_info;
			PropertyInfo value_info;
			Variant value_script;
			Dictionary result;
			if (p_current.get_type() == Variant::DICTIONARY) {
				const Dictionary current = p_current;
				if (current.is_typed()) {
					result.set_typed(current.get_typed_key_builtin(), current.get_typed_key_class_name(), current.get_typed_key_script(), current.get_typed_value_builtin(), current.get_typed_value_class_name(), current.get_typed_value_script());
					key_info.type = Variant::Type(current.get_typed_key_builtin());
					key_info.class_name = current.get_typed_key_class_name();
					value_info.type = Variant::Type(current.get_typed_value_builtin());
					value_info.class_name = current.get_typed_value_class_name();
					value_script = current.get_typed_value_script();
				}
			}
			for (const KeyValue<Variant, Variant> &kv : input) {
				Variant key;
				Variant value;
				const String where = p_where + "/" + String(kv.key);
				if (decode_value(kv.key, key_info, Variant(), Variant(), where, key) != DECODE_OK) {
					WARN_PRINT(vformat("'%s': the key does not fit the type %s; it is skipped.", where, Variant::get_type_name(key_info.type)));
					continue;
				}
				if (decode_value(kv.value, value_info, Variant(), value_script, where, value) != DECODE_OK) {
					if (!error.is_empty()) {
						return DECODE_FAILED;
					}
					WARN_PRINT(vformat("'%s': the value does not fit the type %s; it is skipped.", where, Variant::get_type_name(value_info.type)));
					continue;
				}
				result[key] = value;
			}
			r_value = result;
			return DECODE_OK;
		}
		case Variant::RID:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
			return DECODE_FAILED;
		default:
			break;
	}

	if (source == target) {
		r_value = p_data;
		return DECODE_OK;
	}
	if (_is_math_type(target)) {
		if (empty_text) {
			return DECODE_SKIP;
		}
		LocalVector<double> components;
		if (_to_components(p_data, components) && _components_to_math(target, components, r_value)) {
			return DECODE_OK;
		}
		return DECODE_FAILED;
	}
	if (_is_packed_type(target)) {
		Array input;
		LocalVector<bool> quoted;
		if (source == Variant::ARRAY || _is_packed_type(source)) {
			input = p_data;
		} else if (is_text) {
			const Variant::Type element_type = _packed_element_type(target);
			if (element_type == Variant::VECTOR2 || element_type == Variant::VECTOR3 || element_type == Variant::VECTOR4 || element_type == Variant::COLOR) {
				// A list of vectors can't be written as one plain list; it comes as JSON.
				const Variant parsed = _ini_parse_value(p_data);
				if (parsed.get_type() != Variant::ARRAY && !_is_packed_type(parsed.get_type())) {
					return empty_text ? DECODE_SKIP : DECODE_FAILED;
				}
				input = parsed;
			} else {
				input = _split_plain_list(p_data, quoted);
			}
		} else if (source == Variant::NIL) {
			input = Array();
		} else {
			return DECODE_FAILED;
		}
		PropertyInfo element_info;
		element_info.type = _packed_element_type(target);
		Array elements;
		for (int i = 0; i < input.size(); i++) {
			Variant value;
			if (decode_value(input[i], element_info, Variant(), Variant(), p_where + "/" + itos(i), value) != DECODE_OK) {
				return DECODE_FAILED;
			}
			elements.push_back(value);
		}
		const Variant elements_variant = elements;
		const Variant *args[1] = { &elements_variant };
		Callable::CallError call_error;
		Variant::construct(target, r_value, args, 1, call_error);
		return call_error.error == Callable::CallError::CALL_OK ? DECODE_OK : DECODE_FAILED;
	}
	return DECODE_FAILED;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// Tree API.

// SerializerOptions.sort_keys: sorts every Dictionary of the tree in place.
static void _sort_tree_keys(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dictionary = p_value;
		dictionary.sort();
		for (const KeyValue<Variant, Variant> &kv : dictionary) {
			_sort_tree_keys(kv.value);
		}
	} else if (p_value.get_type() == Variant::ARRAY) {
		const Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			_sort_tree_keys(array[i]);
		}
	}
}

Error ResourceTextSerializer::encode_resource(const Ref<Resource> &p_resource, const Options &p_options, bool p_plain, Dictionary &r_tree, String &r_error, LocalVector<Section> *r_sections) {
	if (p_resource.is_null()) {
		r_error = "The resource is null.";
		return ERR_INVALID_PARAMETER;
	}
	const FullPrecisionScope precision(p_options.full_precision_floats);
	TreeEncoder encoder;
	encoder.plain = p_plain;
	encoder.enums_as_names = p_options.enums_as_names;
	encoder.include_type_marker = p_options.include_type_marker;
	encoder.defaults = p_options.defaults;
	encoder.sections = r_sections;
	r_tree = encoder.encode_resource(p_resource, String(), true, SECTION_ROOT);
	if (!encoder.error.is_empty()) {
		r_error = encoder.error;
		return ERR_INVALID_DATA;
	}
	if (p_options.sort_keys) {
		// INI sections share their Dictionary with the tree, so sorting the tree sorts most of them too.
		_sort_tree_keys(r_tree);
		if (r_sections != nullptr) {
			for (Section &section : *r_sections) {
				section.second.sort();
			}
		}
	}
	return OK;
}

Error ResourceTextSerializer::decode_resource(const Dictionary &p_tree, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	TreeDecoder decoder;
	r_resource = decoder.decode_resource(p_tree, hint, Variant(), SECTION_ROOT);
	if (!decoder.error.is_empty() || r_resource.is_null()) {
		r_error = decoder.error.is_empty() ? String("The resource could not be created.") : decoder.error;
		r_resource.unref();
		return ERR_INVALID_DATA;
	}
	if (!hint.is_empty() && !is_of_type(r_resource, hint)) {
		r_error = vformat("The file contains a %s, but a %s was expected.", get_type_name(r_resource.ptr()), hint);
		r_resource.unref();
		return ERR_INVALID_DATA;
	}
	return OK;
}

/////////////////////////////////////////////////////////////////////////////
// JSON.

String ResourceTextSerializer::get_json_marker_type(const Variant &p_data) {
	if (p_data.get_type() != Variant::DICTIONARY) {
		return String();
	}
	const Dictionary dict = p_data;
	const Variant type = dict.get(KEY_TYPE, Variant());
	if (type.get_type() != Variant::STRING) {
		return String();
	}
	const String type_name = type;
	if (type_name == "JSON" || !is_resource_type(type_name)) {
		return String();
	}
	return type_name;
}

Error ResourceTextSerializer::write_json(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error) {
	Dictionary tree;
	const Error err = encode_resource(p_resource, p_options, true, tree, r_error);
	if (err != OK) {
		return err;
	}
	r_text = JSON::stringify(tree, p_options.indent, p_options.sort_keys, true);
	if (!p_options.indent.is_empty()) {
		r_text += "\n";
	}
	return OK;
}

Error ResourceTextSerializer::parse_json(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	Ref<JSON> json;
	json.instantiate();
	const Error err = json->parse(p_text);
	if (err != OK) {
		r_error = vformat("JSON parse error on line %d: %s", json->get_error_line(), json->get_error_message());
		return hint.is_empty() ? ERR_FILE_UNRECOGNIZED : ERR_PARSE_ERROR;
	}
	const Variant data = json->get_data();
	if (hint.is_empty() && get_json_marker_type(data).is_empty()) {
		r_error = vformat("The JSON has no key \"%s\" with a Resource type.", KEY_TYPE);
		return ERR_FILE_UNRECOGNIZED;
	}
	if (data.get_type() != Variant::DICTIONARY) {
		r_error = "The JSON must contain an object.";
		return ERR_INVALID_DATA;
	}
	return decode_resource(data, hint, r_resource, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// INI.

Error ResourceTextSerializer::write_ini(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error) {
	LocalVector<Section> sections;
	Dictionary tree;
	const Error err = encode_resource(p_resource, p_options, p_options.ini_plain_values, tree, r_error, &sections);
	if (err != OK) {
		return err;
	}
	String text;
	for (uint32_t i = 0; i < sections.size(); i++) {
		if (i > 0) {
			text += "\n";
		}
		text += "[" + sections[i].first + "]\n";
		for (const KeyValue<Variant, Variant> &kv : sections[i].second) {
			const String value = p_options.ini_plain_values ? _ini_plain_text(kv.value) : _ini_variant_text(kv.value);
			text += String(kv.key).property_name_encode() + "=" + value + "\n";
		}
	}
	r_text = text;
	return OK;
}

String ResourceTextSerializer::ini_value_to_text(const Variant &p_value, bool p_plain) {
	return p_plain ? _ini_plain_text(p_value) : _ini_variant_text(p_value);
}

Variant ResourceTextSerializer::ini_text_to_value(const String &p_text) {
	return _ini_parse_value(p_text);
}

Error ResourceTextSerializer::parse_ini_sections(const String &p_text, LocalVector<Section> &r_sections, String &r_error, int *r_error_line) {
	r_sections.clear();
	int current = -1;
	const Vector<String> lines = p_text.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (i == 0 && !line.is_empty() && line[0] == 0xFEFF) {
			line = line.substr(1).strip_edges();
		}
		if (line.is_empty() || line[0] == ';' || line[0] == '#') {
			continue;
		}
		if (line[0] == '[') {
			if (!line.ends_with("]")) {
				r_error = vformat("Line %d: ']' expected.", i + 1);
				if (r_error_line != nullptr) {
					*r_error_line = i + 1;
				}
				return ERR_PARSE_ERROR;
			}
			r_sections.push_back(Section(line.substr(1, line.length() - 2).strip_edges(), Dictionary()));
			current = int(r_sections.size()) - 1;
			continue;
		}
		const int equal = line.find_char('=');
		if (equal <= 0) {
			r_error = vformat("Line %d: 'key=value' expected.", i + 1);
			if (r_error_line != nullptr) {
				*r_error_line = i + 1;
			}
			return ERR_PARSE_ERROR;
		}
		String key = line.substr(0, equal).strip_edges();
		if (key.begins_with("\"")) {
			const Variant parsed = _ini_parse_value(key);
			if (parsed.get_type() == Variant::STRING) {
				key = parsed;
			}
		}
		if (current < 0) {
			// Keys before the first section.
			r_sections.push_back(Section(String(), Dictionary()));
			current = int(r_sections.size()) - 1;
		}
		r_sections[current].second[key] = _ini_parse_value(line.substr(equal + 1));
	}
	return OK;
}

static Variant _container_get(const Variant &p_container, const String &p_key) {
	if (p_container.get_type() == Variant::DICTIONARY) {
		const Dictionary dict = p_container;
		return dict.get(p_key, Variant());
	}
	if (p_container.get_type() == Variant::ARRAY && p_key.is_valid_int()) {
		const Array array = p_container;
		const int64_t index = p_key.to_int();
		if (index >= 0 && index < array.size()) {
			return array[index];
		}
	}
	return Variant();
}

static bool _container_set(const Variant &p_container, const String &p_key, const Variant &p_value) {
	if (p_container.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_container;
		dict[p_key] = p_value;
		return true;
	}
	if (p_container.get_type() == Variant::ARRAY && p_key.is_valid_int()) {
		Array array = p_container;
		const int64_t index = p_key.to_int();
		if (index < 0 || index > 1000000) {
			return false;
		}
		if (index >= array.size()) {
			array.resize(index + 1);
		}
		array[index] = p_value;
		return true;
	}
	return false;
}

static Dictionary _ini_sections_to_tree(const LocalVector<ResourceTextSerializer::Section> &p_sections) {
	Dictionary root;
	bool has_root = false;
	for (const ResourceTextSerializer::Section &section : p_sections) {
		if (section.first == SECTION_ROOT) {
			root = section.second;
			has_root = true;
			break;
		}
	}
	for (const ResourceTextSerializer::Section &section : p_sections) {
		if (section.first.is_empty()) {
			// Keys before the first section belong to the root.
			root.merge(section.second, !has_root);
		}
	}
	for (const ResourceTextSerializer::Section &section : p_sections) {
		if (section.first.is_empty() || section.first == SECTION_ROOT) {
			continue;
		}
		String relative = section.first;
		if (relative.begins_with(String(SECTION_ROOT) + "/")) {
			relative = relative.substr(String(SECTION_ROOT).length() + 1);
		}
		Vector<String> parts = relative.split("/", false);
		for (int i = 0; i < parts.size(); i++) {
			parts.write[i] = _section_part_decode(parts[i]);
		}
		if (parts.is_empty()) {
			continue;
		}
		Variant container = root;
		bool valid = true;
		for (int i = 0; i < parts.size() - 1 && valid; i++) {
			Variant child = _container_get(container, parts[i]);
			if (child.get_type() != Variant::DICTIONARY && child.get_type() != Variant::ARRAY) {
				child = parts[i + 1].is_valid_int() ? Variant(Array()) : Variant(Dictionary());
				valid = _container_set(container, parts[i], child);
			}
			container = child;
		}
		if (!valid || !_container_set(container, parts[parts.size() - 1], section.second)) {
			WARN_PRINT(vformat("INI section [%s] does not fit the structure; it is skipped.", section.first));
		}
	}
	return root;
}

String ResourceTextSerializer::get_ini_marker_type(const String &p_text) {
	LocalVector<Section> sections;
	String error;
	if (parse_ini_sections(p_text, sections, error) != OK) {
		return String();
	}
	for (const Section &section : sections) {
		if (section.first == SECTION_ROOT) {
			const Variant type = section.second.get(KEY_TYPE, Variant());
			if (type.get_type() == Variant::STRING && is_resource_type(type)) {
				return type;
			}
			break;
		}
	}
	return String();
}

Error ResourceTextSerializer::parse_ini_tree(const String &p_text, Dictionary &r_tree, String &r_error) {
	LocalVector<Section> sections;
	const Error err = parse_ini_sections(p_text, sections, r_error);
	if (err != OK) {
		return err;
	}
	r_tree = _ini_sections_to_tree(sections);
	return OK;
}

Error ResourceTextSerializer::parse_ini(const String &p_text, const String &p_type_hint, Ref<Resource> &r_resource, String &r_error) {
	LocalVector<Section> sections;
	const Error err = parse_ini_sections(p_text, sections, r_error);
	if (err != OK) {
		return err;
	}
	return decode_resource(_ini_sections_to_tree(sections), p_type_hint, r_resource, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// CSV.

Vector<Vector<String>> ResourceTextSerializer::parse_csv_rows(const String &p_text, char32_t p_delimiter) {
	// An unclosed quote takes the rest of the text as its cell.
	Vector<Vector<String>> rows;
	String error;
	int error_line = 0;
	parse_csv_rows(p_text, p_delimiter, rows, error, error_line);
	return rows;
}

Error ResourceTextSerializer::parse_csv_rows(const String &p_text, char32_t p_delimiter, Vector<Vector<String>> &r_rows, String &r_error, int &r_error_line) {
	r_rows.clear();
	Vector<String> row;
	String cell;
	bool in_quotes = false;
	bool row_has_content = false;
	int line = 1;
	int quote_line = 0;
	int start = 0;
	if (!p_text.is_empty() && p_text[0] == 0xFEFF) {
		start = 1; // UTF-8 byte order mark.
	}
	const int length = p_text.length();
	for (int i = start; i < length; i++) {
		const char32_t c = p_text[i];
		if (c == '\n') {
			line++;
		}
		if (in_quotes) {
			if (c == '"') {
				if (i + 1 < length && p_text[i + 1] == '"') {
					cell += '"';
					i++;
				} else {
					in_quotes = false;
				}
			} else {
				cell += c;
			}
			continue;
		}
		if (c == '"' && cell.is_empty()) {
			in_quotes = true;
			quote_line = line;
			row_has_content = true;
		} else if (c == p_delimiter) {
			row.push_back(cell);
			cell = String();
			row_has_content = true;
		} else if (c == '\n' || c == '\r') {
			if (c == '\r' && i + 1 < length && p_text[i + 1] == '\n') {
				i++;
				line++;
			}
			if (row_has_content || !cell.is_empty()) {
				row.push_back(cell);
				r_rows.push_back(row);
			}
			row.clear();
			cell = String();
			row_has_content = false;
		} else {
			cell += c;
		}
	}
	if (row_has_content || !cell.is_empty()) {
		row.push_back(cell);
		r_rows.push_back(row);
	}
	if (in_quotes) {
		r_error = vformat("Line %d: the quoted cell is not closed.", quote_line);
		r_error_line = quote_line;
		return ERR_PARSE_ERROR;
	}
	return OK;
}

char32_t ResourceTextSerializer::get_csv_delimiter(const Options &p_options) {
	return _csv_delimiter(p_options);
}

String ResourceTextSerializer::write_csv_row(const Vector<String> &p_cells, char32_t p_delimiter) {
	String line;
	for (int i = 0; i < p_cells.size(); i++) {
		if (i > 0) {
			line += p_delimiter;
		}
		const String &cell = p_cells[i];
		if (cell.contains_char(p_delimiter) || cell.contains_char('"') || cell.contains_char('\n') || cell.contains_char('\r') || cell != cell.strip_edges()) {
			line += "\"" + cell.replace("\"", "\"\"") + "\"";
		} else {
			line += cell;
		}
	}
	return line + "\n";
}

static bool _is_simple_csv_type(Variant::Type p_type) {
	return p_type != Variant::OBJECT && p_type != Variant::ARRAY && p_type != Variant::DICTIONARY && !_is_packed_type(p_type) && p_type != Variant::RID && p_type != Variant::CALLABLE && p_type != Variant::SIGNAL;
}

// The one typed resource array of a list resource.
static Error _find_csv_list(const Ref<Resource> &p_resource, PropertyInfo &r_list, String &r_error) {
	LocalVector<PropertyInfo> properties;
	_get_serialized_properties(p_resource.ptr(), properties);
	int found = 0;
	for (const PropertyInfo &info : properties) {
		if (info.type != Variant::ARRAY) {
			continue;
		}
		const Array array = p_resource->get(info.name);
		if (array.is_typed() && array.get_typed_builtin() == Variant::OBJECT) {
			r_list = info;
			found++;
		}
	}
	if (found != 1) {
		r_error = vformat("CSV needs exactly one typed array of resources (for example Array[Entry]) in %s; found %d.", ResourceTextSerializer::get_type_name(p_resource.ptr()), found);
		return ERR_INVALID_DATA;
	}
	return OK;
}

// Columns of a CSV without header row: the stored properties of the element class, in property order.
static bool _is_csv_positional_column(const PropertyInfo &p_info) {
	return !(p_info.name == "resource_local_to_scene" || p_info.name == "resource_name" || p_info.name.begins_with("metadata/"));
}

static Ref<Resource> _instantiate_element(const Array &p_list, String &r_error) {
	const Ref<Script> script = p_list.get_typed_script();
	if (script.is_valid()) {
		return _instantiate_script(script, r_error);
	}
	return ResourceTextSerializer::instantiate_type(p_list.get_typed_class_name(), r_error);
}

static String _csv_cell_text(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			return String();
		case Variant::BOOL:
			return bool(p_value) ? "true" : "false";
		case Variant::INT:
			return itos(p_value);
		case Variant::FLOAT:
			return _ini_number(p_value);
		case Variant::ARRAY: {
			const Array array = p_value;
			String text;
			for (int i = 0; i < array.size(); i++) {
				if (i > 0) {
					text += ", ";
				}
				text += _csv_cell_text(array[i]);
			}
			return text;
		}
		default:
			return p_value;
	}
}

Error ResourceTextSerializer::check_csv(const Ref<Resource> &p_resource, String &r_error) {
	if (p_resource.is_null()) {
		r_error = "The resource is null.";
		return ERR_INVALID_PARAMETER;
	}
	PropertyInfo list_info;
	Error err = _find_csv_list(p_resource, list_info, r_error);
	if (err != OK) {
		return err;
	}
	const String type = get_type_name(p_resource.ptr());

	// Other properties are not lost only if they keep their defaults.
	String instantiate_error;
	const Ref<Resource> fresh = _instantiate_like(p_resource, instantiate_error);
	if (fresh.is_null()) {
		r_error = instantiate_error;
		return ERR_INVALID_DATA;
	}
	LocalVector<PropertyInfo> properties;
	_get_serialized_properties(p_resource.ptr(), properties);
	for (const PropertyInfo &info : properties) {
		if (info.name == list_info.name) {
			continue;
		}
		const Variant value = p_resource->get(info.name);
		if (_is_skipped_default(info.name, value)) {
			continue;
		}
		if (info.name.begins_with("metadata/") || !bool(Variant::evaluate(Variant::OP_EQUAL, value, fresh->get(info.name)))) {
			r_error = vformat("%s can't be saved as CSV: the property '%s' is not the list and would be lost. Use JSON or INI.", type, info.name);
			return ERR_INVALID_DATA;
		}
	}

	const Array list = p_resource->get(list_info.name);
	Variant element_script;
	const PropertyInfo element_info = _get_element_info(list_info, list, element_script);
	const String element_type = _get_declared_type(element_info, element_script);
	// write_csv() instantiates the element class for the columns: an abstract one (Array[InputEvent]) can't be written (#404).
	String element_error;
	if (_instantiate_element(list, element_error).is_null()) {
		r_error = vformat("%s can't be saved as CSV: %s", type, element_error);
		return ERR_INVALID_DATA;
	}
	for (int i = 0; i < list.size(); i++) {
		const Ref<Resource> element = list[i];
		if (element.is_null()) {
			r_error = vformat("%s can't be saved as CSV: element %d of '%s' is null.", type, i, list_info.name);
			return ERR_INVALID_DATA;
		}
		if (get_type_name(element.ptr()) != element_type || element->get_script().get_validated_object() != list.get_typed_script().get_validated_object()) {
			r_error = vformat("%s can't be saved as CSV: element %d of '%s' is a %s, not a %s.", type, i, list_info.name, get_type_name(element.ptr()), element_type);
			return ERR_INVALID_DATA;
		}
		LocalVector<PropertyInfo> element_properties;
		_get_serialized_properties(element.ptr(), element_properties);
		for (const PropertyInfo &info : element_properties) {
			const Variant value = element->get(info.name);
			const Variant::Type value_type = info.type == Variant::NIL ? value.get_type() : info.type;
			if (info.name.begins_with("metadata/") || !_is_simple_csv_type(value_type)) {
				r_error = vformat("%s can't be saved as CSV: the property '%s' of element %d is not a simple value (%s). Use JSON or INI.", type, info.name, i, info.name.begins_with("metadata/") ? String("metadata") : Variant::get_type_name(value_type));
				return ERR_INVALID_DATA;
			}
		}
	}
	return OK;
}

Error ResourceTextSerializer::write_csv(const Ref<Resource> &p_resource, const Options &p_options, String &r_text, String &r_error) {
	Error err = check_csv(p_resource, r_error);
	if (err != OK) {
		return err;
	}
	PropertyInfo list_info;
	err = _find_csv_list(p_resource, list_info, r_error);
	if (err != OK) {
		return err;
	}
	const Array list = p_resource->get(list_info.name);
	const char32_t delimiter = _csv_delimiter(p_options);
	const FullPrecisionScope precision(p_options.full_precision_floats);

	// Columns: the stored properties of the element class.
	String instantiate_error;
	const Ref<Resource> sample = _instantiate_element(list, instantiate_error);
	if (sample.is_null()) {
		r_error = instantiate_error;
		return ERR_INVALID_DATA;
	}
	LocalVector<PropertyInfo> columns;
	LocalVector<PropertyInfo> properties;
	_get_serialized_properties(sample.ptr(), properties);
	for (const PropertyInfo &info : properties) {
		if (!p_options.csv_headers) {
			// Without a header the reader maps the cells by position: always the same columns.
			if (_is_csv_positional_column(info)) {
				columns.push_back(info);
			}
			continue;
		}
		bool used = !(info.name == "resource_local_to_scene" || info.name == "resource_name");
		for (int i = 0; i < list.size() && !used; i++) {
			const Ref<Resource> element = list[i];
			used = !_is_skipped_default(info.name, element->get(info.name));
		}
		if (used) {
			columns.push_back(info);
		}
	}

	TreeEncoder encoder;
	encoder.plain = true;
	encoder.enums_as_names = p_options.enums_as_names;

	String text;
	if (p_options.csv_headers) {
		Vector<String> header;
		for (const PropertyInfo &info : columns) {
			header.push_back(info.name);
		}
		text += write_csv_row(header, delimiter);
	}
	for (int i = 0; i < list.size(); i++) {
		const Ref<Resource> element = list[i];
		Vector<String> cells;
		for (const PropertyInfo &info : columns) {
			bool omit = false;
			const Variant value = encoder.encode_value(element->get(info.name), info, Variant(), vformat("%s/%d/%s", list_info.name, i, info.name), omit);
			cells.push_back(omit ? String() : _csv_cell_text(value));
		}
		text += write_csv_row(cells, delimiter);
	}
	r_text = text;
	return OK;
}

Error ResourceTextSerializer::parse_csv(const String &p_text, const String &p_type_hint, const Options &p_options, Ref<Resource> &r_resource, String &r_error) {
	const String hint = p_type_hint == "Resource" ? String() : p_type_hint;
	if (hint.is_empty()) {
		r_error = "A CSV file has no type of its own: load it with a type hint, for example ResourceLoader.load(path, \"MyList\").";
		return ERR_INVALID_PARAMETER;
	}
	String instantiate_error;
	const Ref<Resource> probe = instantiate_type(hint, instantiate_error);
	if (probe.is_null()) {
		r_error = instantiate_error;
		return ERR_INVALID_PARAMETER;
	}
	PropertyInfo list_info;
	const Error err = _find_csv_list(probe, list_info, r_error);
	if (err != OK) {
		return err;
	}

	const Vector<Vector<String>> rows = parse_csv_rows(p_text, _csv_delimiter(p_options));
	Array items;
	Vector<String> header;
	int first_row = 1;
	if (!p_options.csv_headers) {
		// No header row: the columns are the stored properties of the element class.
		String element_error;
		const Ref<Resource> sample = _instantiate_element(probe->get(list_info.name), element_error);
		if (sample.is_null()) {
			r_error = element_error;
			return ERR_INVALID_DATA;
		}
		LocalVector<PropertyInfo> properties;
		_get_serialized_properties(sample.ptr(), properties);
		for (const PropertyInfo &info : properties) {
			if (_is_csv_positional_column(info)) {
				header.push_back(info.name);
			}
		}
		first_row = 0;
	} else if (!rows.is_empty()) {
		header = rows[0];
		for (int c = 0; c < header.size(); c++) {
			header.write[c] = header[c].strip_edges();
		}
	}
	if (!rows.is_empty()) {
		bool warned = false;
		for (int r = first_row; r < rows.size(); r++) {
			const Vector<String> &row = rows[r];
			if (row.size() > header.size() && !warned) {
				WARN_PRINT(vformat("CSV row %d has more cells than the header; extra cells are skipped.", r + 1));
				warned = true;
			}
			Dictionary item;
			for (int c = 0; c < MIN(row.size(), header.size()); c++) {
				if (!header[c].is_empty()) {
					item[header[c]] = row[c];
				}
			}
			items.push_back(item);
		}
	}

	Dictionary tree;
	tree[KEY_TYPE] = hint;
	tree[list_info.name] = items;
	return decode_resource(tree, hint, r_resource, r_error);
}

/////////////////////////////////////////////////////////////////////////////
// Bound API.

Dictionary ResourceTextSerializer::resource_to_dictionary(const Ref<Resource> &p_resource, const Ref<SerializerOptions> &p_options) const {
	Dictionary tree;
	String error;
	if (encode_resource(p_resource, get_options(p_options), true, tree, error) != OK) {
		ERR_PRINT(error);
		return Dictionary();
	}
	return tree;
}

Ref<Resource> ResourceTextSerializer::dictionary_to_resource(const Dictionary &p_data, const StringName &p_type_hint) const {
	Ref<Resource> resource;
	String error;
	if (decode_resource(p_data, p_type_hint, resource, error) != OK) {
		ERR_PRINT(error);
		return Ref<Resource>();
	}
	return resource;
}

bool ResourceTextSerializer::can_serialize(const Ref<Resource> &p_resource, Format p_format) const {
	if (p_resource.is_null() || !has_format_implementation(p_format)) {
		return false;
	}
	if (p_format == FORMAT_CSV) {
		// CSV holds only one list of simple values.
		String error;
		return check_csv(p_resource, error) == OK;
	}
	return true;
}

void ResourceTextSerializer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("resource_to_dictionary", "resource", "options"), &ResourceTextSerializer::resource_to_dictionary, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_method(D_METHOD("dictionary_to_resource", "data", "type_hint"), &ResourceTextSerializer::dictionary_to_resource, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("can_serialize", "resource", "format"), &ResourceTextSerializer::can_serialize);

	ClassDB::bind_method(D_METHOD("serialize", "resource", "format", "options"), &ResourceTextSerializer::serialize, DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_method(D_METHOD("deserialize", "text", "format", "type_hint", "options"), &ResourceTextSerializer::deserialize, DEFVAL(StringName()), DEFVAL(Ref<SerializerOptions>()));
	ClassDB::bind_method(D_METHOD("is_format_available", "format"), &ResourceTextSerializer::is_format_available);
	ClassDB::bind_method(D_METHOD("get_available_formats"), &ResourceTextSerializer::get_available_formats);
	ClassDB::bind_method(D_METHOD("get_format_extension", "format"), &ResourceTextSerializer::get_format_extension);
	ClassDB::bind_method(D_METHOD("get_format_from_extension", "extension"), &ResourceTextSerializer::get_format_from_extension);

	BIND_ENUM_CONSTANT(FORMAT_JSON);
	BIND_ENUM_CONSTANT(FORMAT_INI);
	BIND_ENUM_CONSTANT(FORMAT_CSV);
	BIND_ENUM_CONSTANT(FORMAT_XML);
	BIND_ENUM_CONSTANT(FORMAT_TOML);
	BIND_ENUM_CONSTANT(FORMAT_YAML);
	BIND_ENUM_CONSTANT(FORMAT_MAX);

	BIND_CONSTANT(TEXT_LAYOUT_VERSION);
}

ResourceTextSerializer::ResourceTextSerializer() {
	singleton = this;
	register_builtin_formats();
}

ResourceTextSerializer::~ResourceTextSerializer() {
	clear_formats();
	singleton = nullptr;
}

Ref<TextDataFormat> ResourceTextSerializer::create_data_format(Format p_format, const Ref<SerializerOptions> &p_options) {
	static const char *format_classes[FORMAT_MAX] = { nullptr, "INI", "CSV", "XML", "TOML", "YAML" };
	ERR_FAIL_INDEX_V((int)p_format, (int)FORMAT_MAX, Ref<TextDataFormat>());
	if (!format_classes[p_format]) {
		return Ref<TextDataFormat>();
	}
	const StringName format_class = format_classes[p_format];
	ERR_FAIL_COND_V_MSG(!ClassDB::class_exists(format_class), Ref<TextDataFormat>(), vformat("The %s format is not available in this build.", format_class));
	Ref<TextDataFormat> format = Object::cast_to<TextDataFormat>(ClassDB::instantiate(format_class));
	ERR_FAIL_COND_V(format.is_null(), format);
	if (p_options.is_valid()) {
		format->set_options(p_options);
	}
	return format;
}
