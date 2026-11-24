/**************************************************************************/
/*  settings_server.cpp                                                   */
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

#include "settings_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_text_serializer.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "servers/settings/settings_server_manager.h"

static constexpr const char *SETTING_STORAGE_FOLDER = "settings/storage/folder";
static constexpr const char *SETTING_STORAGE_FORMAT = "settings/storage/format";
static constexpr const char *SETTING_ONLY_CHANGED = "settings/storage/only_changed_values";
static constexpr const char *SETTING_AUTOSAVE = "settings/storage/autosave";
static constexpr const char *SETTING_CONFIRMATION_TIMEOUT = "settings/apply/confirmation_timeout";
static constexpr const char *SETTING_DEFAULTS_PREFIX = "settings/defaults/";
static constexpr const char *SETTING_PATHS_PREFIX = "settings/paths/";
static constexpr const char *SETTING_PATH_SUFFIX = "_settings_path";

// Metadata key in the settings file (INI and JSON) with the format version of the domain.
static constexpr const char *META_SETTINGS_VERSION = "settings_version";

/////////////////////////////////////////////////////////////////////////////
// Helpers.

void SettingsServer::get_value_infos(const Object *p_object, LocalVector<PropertyInfo> &r_infos) {
	r_infos.clear();
	ERR_FAIL_NULL(p_object);
	List<PropertyInfo> list;
	p_object->get_property_list(&list);
	for (const PropertyInfo &info : list) {
		if (!(info.usage & PROPERTY_USAGE_STORAGE) || (info.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT)) {
			continue;
		}
		if (info.name == "script" || info.name.begins_with("resource_") || info.name.begins_with("metadata/") || info.name.begins_with("__")) {
			continue;
		}
		r_infos.push_back(info);
	}
}

Variant SettingsServer::copy_value(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::OBJECT: {
			const Ref<Resource> resource = p_value;
			if (resource.is_valid() && resource->get_path().is_empty()) {
				return copy_settings(resource);
			}
			return p_value;
		}
		case Variant::ARRAY: {
			const Array source = p_value;
			Array copy = source.duplicate(false);
			for (int i = 0; i < copy.size(); i++) {
				copy[i] = copy_value(source[i]);
			}
			return copy;
		}
		case Variant::DICTIONARY: {
			const Dictionary source = p_value;
			return source.duplicate(true);
		}
		default:
			return p_value.duplicate(true);
	}
}

bool SettingsServer::values_equal(const Variant &p_a, const Variant &p_b) {
	if (p_a.get_type() == Variant::OBJECT && p_b.get_type() == Variant::OBJECT) {
		const Ref<Resource> a = p_a;
		const Ref<Resource> b = p_b;
		if (a.is_null() || b.is_null() || a == b) {
			return p_a == p_b;
		}
		if (a->get_class_name() != b->get_class_name() || a->get_script() != b->get_script()) {
			return false;
		}
		LocalVector<PropertyInfo> infos;
		get_value_infos(a.ptr(), infos);
		for (const PropertyInfo &info : infos) {
			if (!values_equal(a->get(info.name), b->get(info.name))) {
				return false;
			}
		}
		return true;
	}
	if (p_a.get_type() == Variant::ARRAY && p_b.get_type() == Variant::ARRAY) {
		const Array a = p_a;
		const Array b = p_b;
		if (a.size() != b.size()) {
			return false;
		}
		for (int i = 0; i < a.size(); i++) {
			if (!values_equal(a[i], b[i])) {
				return false;
			}
		}
		return true;
	}
	return p_a == p_b;
}

Ref<Resource> SettingsServer::copy_settings(const Ref<Resource> &p_settings) {
	if (p_settings.is_null()) {
		return Ref<Resource>();
	}
	Ref<Resource> copy = p_settings->duplicate(false);
	ERR_FAIL_COND_V(copy.is_null(), Ref<Resource>());
	LocalVector<PropertyInfo> infos;
	get_value_infos(p_settings.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		copy->set(info.name, copy_value(p_settings->get(info.name)));
	}
	return copy;
}

/////////////////////////////////////////////////////////////////////////////
// Project settings.

void SettingsServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::STRING, SETTING_STORAGE_FOLDER, PROPERTY_HINT_DIR), "user://settings");
	// Values of ResourceTextSerializer::Format.
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_STORAGE_FORMAT, PROPERTY_HINT_ENUM, "JSON,INI,CSV,XML,TOML,YAML"), ResourceTextSerializer::FORMAT_INI);
	GLOBAL_DEF(SETTING_ONLY_CHANGED, true);
	GLOBAL_DEF(SETTING_AUTOSAVE, true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, SETTING_CONFIRMATION_TIMEOUT, PROPERTY_HINT_RANGE, "1,120,0.5,or_greater,suffix:s"), 15.0);
}

static Variant _get_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_name);
}

String SettingsServer::get_storage_folder() {
	const String folder = _get_project_setting(SETTING_STORAGE_FOLDER, "user://settings");
	return folder.is_empty() ? String("user://settings") : folder;
}

String SettingsServer::get_path_setting_name(const StringName &p_domain) {
	return String(SETTING_PATHS_PREFIX) + String(p_domain) + SETTING_PATH_SUFFIX;
}

void SettingsServer::register_path_setting(const StringName &p_domain, bool p_csv) {
	ERR_FAIL_COND(p_domain == StringName());
	if (ProjectSettings::get_singleton() == nullptr) {
		return;
	}
	// The extension of the path picks the text format. CSV only for list domains.
	const String extensions = p_csv ? "*.json,*.ini,*.csv,*.xml,*.toml,*.yaml,*.yml" : "*.json,*.ini,*.xml,*.toml,*.yaml,*.yml";
	GLOBAL_DEF(PropertyInfo(Variant::STRING, get_path_setting_name(p_domain), PROPERTY_HINT_SAVE_FILE, extensions), String());
}

ResourceTextSerializer::Format SettingsServer::get_storage_format() {
	const Variant value = _get_project_setting(SETTING_STORAGE_FORMAT, ResourceTextSerializer::FORMAT_INI);
	int format = ResourceTextSerializer::FORMAT_INI;
	if (value.get_type() == Variant::STRING || value.get_type() == Variant::STRING_NAME) {
		// A format name written by hand into project.godot ("json", "ini", ...).
		format = ResourceTextSerializer::get_format_for_extension(value);
	} else if (value.get_type() == Variant::INT) {
		format = value;
	}
	if (format < 0 || format >= ResourceTextSerializer::FORMAT_MAX) {
		return ResourceTextSerializer::FORMAT_INI;
	}
	return ResourceTextSerializer::Format(format);
}

/////////////////////////////////////////////////////////////////////////////
// Identity.

StringName SettingsServer::get_domain() const {
	StringName ret;
	GDVIRTUAL_CALL(_get_domain, ret);
	return ret;
}

int SettingsServer::get_format_version() const {
	int ret = 1;
	GDVIRTUAL_CALL(_get_format_version, ret);
	return ret;
}

SettingsServer::ApplyPhase SettingsServer::get_apply_phase() const {
	ApplyPhase ret = APPLY_PHASE_SERVERS;
	GDVIRTUAL_CALL(_get_apply_phase, ret);
	// APPLY_PHASE_NONE and out-of-range values start the server in the first or last phase.
	return ApplyPhase(CLAMP(int(ret), int(APPLY_PHASE_PRE_DISPLAY), int(APPLY_PHASE_MAX) - 1));
}

/////////////////////////////////////////////////////////////////////////////
// Overridable defaults.

Ref<Resource> SettingsServer::_create_default_settings() const {
	Ref<Resource> ret;
	GDVIRTUAL_CALL(_create_settings, ret);
	return ret;
}

void SettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	GDVIRTUAL_CALL(_apply, p_settings, p_names);
}

void SettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	GDVIRTUAL_CALL(_read_from_engine, p_settings);
}

Variant SettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant ret;
	if (GDVIRTUAL_CALL(_validate_value, p_name, p_value, ret)) {
		return ret;
	}
	return p_value;
}

Dictionary SettingsServer::_migrate_settings(int p_from_version, const Dictionary &p_data) {
	Dictionary ret;
	if (GDVIRTUAL_CALL(_migrate, p_from_version, p_data, ret)) {
		return ret;
	}
	return p_data;
}

ResourceTextSerializer::Format SettingsServer::_get_file_format() {
	const ResourceTextSerializer::Format format = get_storage_format();
	if (format == ResourceTextSerializer::FORMAT_CSV) {
		if (!format_warned) {
			format_warned = true;
			WARN_PRINT(vformat("Settings domain \"%s\" is not a list domain and can't be stored as CSV; INI is used.", get_domain()));
		}
		return ResourceTextSerializer::FORMAT_INI;
	}
	if (!ResourceTextSerializer::has_format_implementation(format)) {
		if (!format_warned) {
			format_warned = true;
			WARN_PRINT(vformat("Settings domain \"%s\": the text format %s is not available (module disabled); INI is used.", get_domain(), ResourceTextSerializer::get_format_display_name(format)));
		}
		return ResourceTextSerializer::FORMAT_INI;
	}
	return format;
}

ResourceTextSerializer::Format SettingsServer::_get_format_for_path(const String &p_path) {
	const ResourceTextSerializer::Format format = ResourceTextSerializer::get_format_for_extension(p_path.get_extension());
	if (format != ResourceTextSerializer::FORMAT_MAX && ResourceTextSerializer::has_format_implementation(format)) {
		return format;
	}
	const ResourceTextSerializer::Format fallback = _get_file_format();
	if (format_warned_path != p_path) {
		format_warned_path = p_path;
		if (format == ResourceTextSerializer::FORMAT_MAX) {
			ERR_PRINT(vformat("Settings domain \"%s\": the extension of \"%s\" belongs to no text format (use .json, .ini, .csv, .xml, .toml, .yaml or .yml); the file is read and written as %s, the format of \"%s\".", get_domain(), p_path, ResourceTextSerializer::get_format_display_name(fallback), SETTING_STORAGE_FORMAT));
		} else {
			ERR_PRINT(vformat("Settings domain \"%s\": the text format %s of \"%s\" is not available (module disabled); the file is read and written as %s, the format of \"%s\".", get_domain(), ResourceTextSerializer::get_format_display_name(format), p_path, ResourceTextSerializer::get_format_display_name(fallback), SETTING_STORAGE_FORMAT));
		}
	}
	return fallback;
}

void SettingsServer::_emit_settings_changed(const PackedStringArray &p_names) {
	if (!p_names.is_empty()) {
		emit_signal(SNAME("settings_changed"), p_names);
	}
}

bool SettingsServer::is_value_supported(const StringName &p_name) const {
	if (!has_setting(p_name)) {
		return false;
	}
	bool ret = true;
	GDVIRTUAL_CALL(_is_value_supported, p_name, ret);
	return ret;
}

/////////////////////////////////////////////////////////////////////////////
// Initialization.

void SettingsServer::_ensure_initialized() const {
	if (!initialized) {
		const_cast<SettingsServer *>(this)->_initialize();
	}
}

void SettingsServer::_initialize() {
	initialized = true;
	defaults = _create_default_settings();
	if (defaults.is_null()) {
		ERR_PRINT(vformat("Settings domain \"%s\" returned no settings resource from _create_settings(); an empty Resource is used.", get_domain()));
		defaults.instantiate();
	}
	// The engine state at this point (project settings, bus layout, ...) becomes the default.
	_read_from_engine_into(defaults);

	// Optional defaults of the project: a resource in `res://` named in `settings/defaults/<domain>`.
	const StringName domain = get_domain();
	if (domain != StringName()) {
		const String key = SETTING_DEFAULTS_PREFIX + String(domain);
		const String path = _get_project_setting(key, String());
		if (!path.is_empty()) {
			const Ref<Resource> project_defaults = ResourceLoader::load(path);
			if (project_defaults.is_valid()) {
				LocalVector<PropertyInfo> infos;
				get_value_infos(defaults.ptr(), infos);
				for (const PropertyInfo &info : infos) {
					bool valid = false;
					const Variant value = project_defaults->get(info.name, &valid);
					if (valid) {
						defaults->set(info.name, copy_value(value));
					}
				}
			} else {
				WARN_PRINT(vformat("Settings domain \"%s\": can't load the defaults \"%s\" (project setting \"%s\").", domain, path, key));
			}
		}
	}

	settings = copy_settings(defaults);
	applied = copy_settings(defaults);
	restart_baseline = copy_settings(defaults);
}

Ref<Resource> SettingsServer::get_pending_settings() const {
	_ensure_initialized();
	return settings;
}

Ref<Resource> SettingsServer::_effective_settings() const {
	if (session_overrides.is_empty()) {
		return settings;
	}
	Ref<Resource> effective = copy_settings(settings);
	for (const KeyValue<StringName, Variant> &kv : session_overrides) {
		effective->set(kv.key, kv.value);
	}
	return effective;
}

bool SettingsServer::_object_has_value(const Ref<Resource> &p_settings, const StringName &p_name) const {
	if (p_settings.is_null()) {
		return false;
	}
	LocalVector<PropertyInfo> infos;
	get_value_infos(p_settings.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		if (info.name == p_name) {
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// Values.

Ref<Resource> SettingsServer::get_settings() const {
	_ensure_initialized();
	return settings;
}

Ref<Resource> SettingsServer::get_applied_settings() const {
	_ensure_initialized();
	return applied;
}

Ref<Resource> SettingsServer::get_default_settings() const {
	_ensure_initialized();
	return defaults;
}

PackedStringArray SettingsServer::get_setting_names() const {
	_ensure_initialized();
	PackedStringArray names;
	LocalVector<PropertyInfo> infos;
	get_value_infos(settings.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		names.push_back(info.name);
	}
	return names;
}

bool SettingsServer::has_setting(const StringName &p_name) const {
	_ensure_initialized();
	return _object_has_value(settings, p_name);
}

Variant SettingsServer::_validate(const StringName &p_name, const Variant &p_value) const {
	return _validate_setting(p_name, validate_property_value(settings.ptr(), p_name, p_value));
}

Variant SettingsServer::validate_property_value(const Object *p_object, const StringName &p_name, const Variant &p_value) const {
	ERR_FAIL_NULL_V(p_object, p_value);
	Variant value = p_value;
	List<PropertyInfo> list;
	p_object->get_property_list(&list);
	for (const PropertyInfo &info : list) {
		if (info.name != p_name) {
			continue;
		}
		if (info.hint == PROPERTY_HINT_RANGE && (value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT)) {
			const Vector<String> parts = info.hint_string.split(",");
			if (parts.size() >= 2 && parts[0].is_valid_float() && parts[1].is_valid_float()) {
				const double min_value = parts[0].to_float();
				const double max_value = parts[1].to_float();
				const bool or_greater = parts.has("or_greater");
				const bool or_less = parts.has("or_less");
				double number = value;
				if (!or_less && number < min_value) {
					number = min_value;
				}
				if (!or_greater && number > max_value) {
					number = max_value;
				}
				value = info.type == Variant::INT ? Variant(int64_t(Math::round(number))) : Variant(number);
			}
		} else if (info.hint == PROPERTY_HINT_ENUM && info.type == Variant::INT && value.get_type() == Variant::INT) {
			const Vector<String> entries = info.hint_string.split(",");
			int64_t next = 0;
			bool known = false;
			for (const String &entry : entries) {
				const int colon = entry.rfind_char(':');
				const int64_t entry_value = colon >= 0 ? entry.substr(colon + 1).to_int() : next;
				next = entry_value + 1;
				if (entry_value == int64_t(value)) {
					known = true;
					break;
				}
			}
			if (!known && !entries.is_empty()) {
				ERR_PRINT(vformat("Settings domain \"%s\": %d is not a valid value for \"%s\"; the value is kept.", get_domain(), int64_t(value), p_name));
				return p_object->get(p_name);
			}
		} else if (info.hint == PROPERTY_HINT_ENUM && info.type == Variant::STRING && value.get_type() == Variant::STRING) {
			const Vector<String> entries = info.hint_string.split(",");
			if (!entries.is_empty() && !entries.has(String(value))) {
				ERR_PRINT(vformat("Settings domain \"%s\": \"%s\" is not a valid value for \"%s\"; the value is kept.", get_domain(), value, p_name));
				return p_object->get(p_name);
			}
		}
		break;
	}
	return value;
}

void SettingsServer::set_value(const StringName &p_name, const Variant &p_value) {
	_ensure_initialized();
	bool has = false;
	const Variant current = settings->get(p_name, &has);
	ERR_FAIL_COND_MSG(!has || !_object_has_value(settings, p_name), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	const Variant value = _validate(p_name, p_value);
	// A runtime change wins over a session override.
	const bool had_override = session_overrides.erase(p_name);
	if (values_equal(current, value)) {
		if (had_override) {
			PackedStringArray names;
			names.push_back(p_name);
			_push_values(names);
		}
		return;
	}
	bool valid = false;
	settings->set(p_name, copy_value(value), &valid);
	ERR_FAIL_COND_MSG(!valid, vformat("Settings domain \"%s\": the value for \"%s\" could not be set.", get_domain(), p_name));
	notify_value_changed(p_name);
}

void SettingsServer::notify_value_changed(const StringName &p_name) {
	emit_signal(SNAME("value_changed"), p_name, settings->get(p_name));
	PackedStringArray names;
	names.push_back(p_name);
	_emit_settings_changed(names);
	if (apply_mode == APPLY_MODE_IMMEDIATE) {
		apply();
	}
}

Variant SettingsServer::get_value(const StringName &p_name) const {
	_ensure_initialized();
	bool valid = false;
	const Variant value = settings->get(p_name, &valid);
	ERR_FAIL_COND_V_MSG(!valid, Variant(), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	return value;
}

Variant SettingsServer::get_applied_value(const StringName &p_name) const {
	_ensure_initialized();
	bool valid = false;
	const Variant value = applied->get(p_name, &valid);
	ERR_FAIL_COND_V_MSG(!valid, Variant(), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	return value;
}

Variant SettingsServer::get_default_value(const StringName &p_name) const {
	_ensure_initialized();
	bool valid = false;
	const Variant value = defaults->get(p_name, &valid);
	ERR_FAIL_COND_V_MSG(!valid, Variant(), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	return value;
}

/////////////////////////////////////////////////////////////////////////////
// Bindings.

void SettingsServer::bind_setting_property(const StringName &p_name, Object *p_target, const StringName &p_property, BitField<SettingFlags> p_flags) {
	ERR_FAIL_NULL(p_target);
	Binding binding;
	binding.kind = BINDING_OBJECT;
	binding.target = p_target->get_instance_id();
	binding.property = p_property;
	binding.flags = p_flags;
	bindings[p_name] = binding;
}

void SettingsServer::bind_setting_singleton_property(const StringName &p_name, const StringName &p_singleton, const StringName &p_property, BitField<SettingFlags> p_flags) {
	ERR_FAIL_COND(p_singleton == StringName());
	Binding binding;
	binding.kind = BINDING_SINGLETON;
	binding.singleton = p_singleton;
	binding.property = p_property;
	binding.flags = p_flags;
	bindings[p_name] = binding;
}

void SettingsServer::bind_setting_callbacks(const StringName &p_name, const Callable &p_setter, const Callable &p_getter, BitField<SettingFlags> p_flags) {
	ERR_FAIL_COND(!p_setter.is_valid());
	Binding binding;
	binding.kind = BINDING_CALLBACKS;
	binding.setter = p_setter;
	binding.getter = p_getter;
	binding.flags = p_flags;
	bindings[p_name] = binding;
}

void SettingsServer::unbind_setting(const StringName &p_name) {
	bindings.erase(p_name);
}

bool SettingsServer::is_setting_bound(const StringName &p_name) const {
	const Binding *binding = bindings.getptr(p_name);
	return binding != nullptr && binding->kind != BINDING_NONE;
}

void SettingsServer::set_setting_flags(const StringName &p_name, BitField<SettingFlags> p_flags) {
	Binding *binding = bindings.getptr(p_name);
	if (binding != nullptr) {
		binding->flags = p_flags;
		return;
	}
	Binding new_binding;
	new_binding.flags = p_flags;
	bindings[p_name] = new_binding;
}

BitField<SettingsServer::SettingFlags> SettingsServer::get_setting_flags(const StringName &p_name) const {
	const Binding *binding = bindings.getptr(p_name);
	return binding != nullptr ? binding->flags : BitField<SettingFlags>(SETTING_FLAG_NONE);
}

bool SettingsServer::_has_flag(const PackedStringArray &p_names, SettingFlags p_flag) const {
	for (const String &name : p_names) {
		if (get_setting_flags(name).has_flag(p_flag)) {
			return true;
		}
	}
	return false;
}

void SettingsServer::_push_values(const PackedStringArray &p_names) {
	const Ref<Resource> effective = _effective_settings();
	PackedStringArray unbound;
	for (const String &name : p_names) {
		const Binding *binding = bindings.getptr(name);
		if (binding != nullptr && binding->flags.has_flag(SETTING_FLAG_NO_ENGINE_TARGET)) {
			continue;
		}
		const Variant value = effective->get(name);
		if (binding == nullptr || binding->kind == BINDING_NONE) {
			unbound.push_back(name);
			continue;
		}
		switch (binding->kind) {
			case BINDING_OBJECT: {
				Object *target = ObjectDB::get_instance(binding->target);
				if (target == nullptr) {
					WARN_PRINT(vformat("Settings domain \"%s\": the target of \"%s\" no longer exists.", get_domain(), name));
				} else {
					target->set(binding->property, value);
				}
			} break;
			case BINDING_SINGLETON: {
				Engine *engine = Engine::get_singleton();
				if (!engine->has_singleton(binding->singleton)) {
					WARN_PRINT(vformat("Settings domain \"%s\": the singleton \"%s\" for \"%s\" does not exist (yet).", get_domain(), binding->singleton, name));
				} else {
					engine->get_singleton_object(binding->singleton)->set(binding->property, value);
				}
			} break;
			case BINDING_CALLBACKS: {
				binding->setter.call(value);
			} break;
			default:
				break;
		}
	}
	if (!unbound.is_empty()) {
		_apply_settings(effective, unbound);
	}
}

void SettingsServer::_read_from_engine_into(const Ref<Resource> &p_settings) {
	for (const KeyValue<StringName, Binding> &kv : bindings) {
		const Binding &binding = kv.value;
		if (binding.flags.has_flag(SETTING_FLAG_NO_ENGINE_TARGET) || !_object_has_value(p_settings, kv.key)) {
			continue;
		}
		bool valid = false;
		Variant value;
		switch (binding.kind) {
			case BINDING_OBJECT: {
				Object *target = ObjectDB::get_instance(binding.target);
				if (target != nullptr) {
					value = target->get(binding.property, &valid);
				}
			} break;
			case BINDING_SINGLETON: {
				Engine *engine = Engine::get_singleton();
				if (engine->has_singleton(binding.singleton)) {
					value = engine->get_singleton_object(binding.singleton)->get(binding.property, &valid);
				}
			} break;
			case BINDING_CALLBACKS: {
				if (binding.getter.is_valid()) {
					value = binding.getter.call();
					valid = true;
				}
			} break;
			default:
				break;
		}
		if (valid) {
			p_settings->set(kv.key, value);
		}
	}
	_read_settings_from_engine(p_settings);
}

/////////////////////////////////////////////////////////////////////////////
// State.

void SettingsServer::set_apply_mode(ApplyMode p_mode) {
	apply_mode = p_mode;
}

SettingsServer::ApplyMode SettingsServer::get_apply_mode() const {
	return apply_mode;
}

PackedStringArray SettingsServer::_get_changed_names(const Ref<Resource> &p_a, const Ref<Resource> &p_b) const {
	PackedStringArray names;
	LocalVector<PropertyInfo> infos;
	get_value_infos(p_a.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		if (!values_equal(p_a->get(info.name), p_b->get(info.name))) {
			names.push_back(info.name);
		}
	}
	return names;
}

bool SettingsServer::is_dirty() const {
	return !get_dirty_setting_names().is_empty();
}

PackedStringArray SettingsServer::get_dirty_setting_names() const {
	_ensure_initialized();
	return _get_changed_names(settings, applied);
}

PackedStringArray SettingsServer::get_restart_required_names() const {
	return restart_required_names;
}

void SettingsServer::revert() {
	_ensure_initialized();
	const PackedStringArray names = _get_changed_names(settings, applied);
	settings = copy_settings(applied);
	for (const String &name : names) {
		emit_signal(SNAME("value_changed"), StringName(name), settings->get(name));
	}
	_emit_settings_changed(names);
}

void SettingsServer::reset_value(const StringName &p_name) {
	_ensure_initialized();
	ERR_FAIL_COND_MSG(!_object_has_value(defaults, p_name), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	set_value(p_name, copy_value(defaults->get(p_name)));
}

void SettingsServer::reset_to_defaults() {
	_ensure_initialized();
	const PackedStringArray names = _get_changed_names(settings, defaults);
	settings = copy_settings(defaults);
	for (const String &name : names) {
		session_overrides.erase(name);
		emit_signal(SNAME("value_changed"), StringName(name), settings->get(name));
	}
	_emit_settings_changed(names);
	emit_signal(SNAME("reset"));
	if (apply_mode == APPLY_MODE_IMMEDIATE) {
		apply();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Session overrides.

void SettingsServer::set_session_override(const StringName &p_name, const Variant &p_value) {
	_ensure_initialized();
	ERR_FAIL_COND_MSG(!_object_has_value(settings, p_name), vformat("Settings domain \"%s\" has no value \"%s\".", get_domain(), p_name));
	session_overrides[p_name] = _validate(p_name, p_value);
	PackedStringArray names;
	names.push_back(p_name);
	_push_values(names);
}

void SettingsServer::clear_session_override(const StringName &p_name) {
	if (!session_overrides.erase(p_name)) {
		return;
	}
	// Back to the applied (stored) value.
	PackedStringArray names;
	names.push_back(p_name);
	const Ref<Resource> pending = settings;
	settings = applied;
	_push_values(names);
	settings = pending;
}

bool SettingsServer::has_session_override(const StringName &p_name) const {
	return session_overrides.has(p_name);
}

/////////////////////////////////////////////////////////////////////////////
// Engine.

void SettingsServer::_update_restart_required_names(const PackedStringArray &p_names) {
	if (in_startup) {
		return;
	}
	bool changed = false;
	for (const String &name : p_names) {
		if (!get_setting_flags(name).has_flag(SETTING_FLAG_RESTART_REQUIRED)) {
			continue;
		}
		// A value set back to the one the engine started with needs no restart any more.
		const bool needs_restart = restart_baseline.is_null() || !values_equal(applied->get(name), restart_baseline->get(name));
		const int64_t index = restart_required_names.find(name);
		if (needs_restart && index < 0) {
			restart_required_names.push_back(name);
			changed = true;
		} else if (!needs_restart && index >= 0) {
			restart_required_names.remove_at(index);
			changed = true;
		}
	}
	if (changed) {
		emit_signal(SNAME("restart_required"), restart_required_names);
	}
}

Error SettingsServer::apply() {
	_ensure_initialized();
	const PackedStringArray names = _get_changed_names(settings, applied);
	if (names.is_empty()) {
		return OK;
	}
	if (!in_startup && confirmation_active) {
		// The player changed more while the confirmation dialog is open: the pending values are kept first, so a
		// later cancel only reverts what this call applies.
		confirm_changes();
	}
	if (!in_startup && !confirmation_active && _has_flag(names, SETTING_FLAG_CONFIRM)) {
		return apply_with_confirmation();
	}
	_push_values(names);
	applied = copy_settings(settings);
	_update_restart_required_names(names);
	emit_signal(SNAME("applied"), names);
	if (!in_startup && !confirmation_active) {
		unsaved = true;
		_queue_autosave();
	}
	return OK;
}

Error SettingsServer::apply_with_confirmation(double p_timeout_sec) {
	_ensure_initialized();
	if (in_startup) {
		// The stored state needs no confirmation.
		return apply();
	}
	const PackedStringArray names = _get_changed_names(settings, applied);
	if (names.is_empty()) {
		return OK;
	}
	if (confirmation_active) {
		// Like apply(): the pending confirmation is kept, then this one starts with its own backup and timeout.
		confirm_changes();
	}
	double timeout = p_timeout_sec;
	if (timeout < 0.0) {
		timeout = _get_project_setting(SETTING_CONFIRMATION_TIMEOUT, 15.0);
	}
	confirmation_backup = copy_settings(applied);
	confirmation_active = true;
	confirmation_deadline_msec = OS::get_singleton()->get_ticks_msec() + uint64_t(MAX(timeout, 0.0) * 1000.0);
	_push_values(names);
	applied = copy_settings(settings);
	_update_restart_required_names(names);
	emit_signal(SNAME("applied"), names);
	_set_confirmation_processing(true);
	emit_signal(SNAME("confirmation_requested"), timeout);
	return OK;
}

void SettingsServer::confirm_changes() {
	if (!confirmation_active) {
		return;
	}
	confirmation_active = false;
	confirmation_backup.unref();
	_set_confirmation_processing(false);
	unsaved = true;
	_queue_autosave();
	emit_signal(SNAME("confirmation_finished"), true);
}

void SettingsServer::cancel_confirmation() {
	if (!confirmation_active) {
		return;
	}
	_set_confirmation_processing(false);
	const Ref<Resource> backup = confirmation_backup;
	confirmation_backup.unref();
	const PackedStringArray names = _get_changed_names(backup, applied);
	settings = copy_settings(backup);
	_push_values(names);
	applied = copy_settings(settings);
	confirmation_active = false;
	_update_restart_required_names(names);
	for (const String &name : names) {
		emit_signal(SNAME("value_changed"), StringName(name), settings->get(name));
	}
	_emit_settings_changed(names);
	if (!names.is_empty()) {
		emit_signal(SNAME("applied"), names);
	}
	emit_signal(SNAME("confirmation_finished"), false);
}

bool SettingsServer::is_confirmation_pending() const {
	return confirmation_active;
}

double SettingsServer::get_confirmation_time_left() const {
	if (!confirmation_active) {
		return 0.0;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	return now >= confirmation_deadline_msec ? 0.0 : double(confirmation_deadline_msec - now) / 1000.0;
}

void SettingsServer::_set_confirmation_processing(bool p_enabled) {
	MainLoop *main_loop = OS::get_singleton()->get_main_loop();
	// The SceneTree emits `process_frame`; without it the game calls confirm_changes() or cancel_confirmation() itself.
	if (main_loop == nullptr || !main_loop->has_signal(SNAME("process_frame"))) {
		return;
	}
	const Callable callable = callable_mp(this, &SettingsServer::_on_process_frame);
	const bool connected = main_loop->is_connected(SNAME("process_frame"), callable);
	if (p_enabled && !connected) {
		main_loop->connect(SNAME("process_frame"), callable);
	} else if (!p_enabled && connected) {
		main_loop->disconnect(SNAME("process_frame"), callable);
	}
}

void SettingsServer::_on_process_frame() {
	if (confirmation_active && OS::get_singleton()->get_ticks_msec() >= confirmation_deadline_msec) {
		cancel_confirmation();
	}
}

void SettingsServer::read_from_engine() {
	_ensure_initialized();
	const Ref<Resource> previous = copy_settings(settings);
	_read_from_engine_into(settings);
	applied = copy_settings(settings);
	_emit_settings_changed(_get_changed_names(previous, settings));
	emit_signal(SNAME("read_from_engine_finished"));
}

/////////////////////////////////////////////////////////////////////////////
// Storage.

void SettingsServer::set_file_path(const String &p_path) {
	file_path = p_path;
}

String SettingsServer::get_file_path() const {
	if (!file_path.is_empty()) {
		return file_path;
	}
	const String project_path = _get_project_file_path();
	if (!project_path.is_empty()) {
		return project_path;
	}
	const StringName domain = get_domain();
	ERR_FAIL_COND_V_MSG(domain == StringName(), String(), "A settings server needs a domain name: implement _get_domain().");
	return get_storage_folder().path_join(String(domain) + "." + ResourceTextSerializer::get_format_file_extension(const_cast<SettingsServer *>(this)->_get_file_format()));
}

String SettingsServer::_get_project_file_path() const {
	const StringName domain = get_domain();
	if (domain == StringName()) {
		return String();
	}
	const String path = _get_project_setting(get_path_setting_name(domain), String());
	return path.strip_edges();
}

bool SettingsServer::has_unsaved_changes() const {
	return unsaved;
}

Error SettingsServer::load() {
	_ensure_initialized();
	const String path = get_file_path();
	if (path.is_empty()) {
		ERR_PRINT(vformat("Settings domain \"%s\": no file path to load from.", get_domain()));
		emit_signal(SNAME("load_failed"), int(ERR_FILE_BAD_PATH), path);
		return ERR_FILE_BAD_PATH;
	}
	const StringName domain = get_domain();
	// The pending values before the load: settings_changed() names the values the load changed.
	const Ref<Resource> previous = settings;

	if (!FileAccess::exists(path)) {
		// The normal first start: the defaults stay. Not a failure.
		print_verbose(vformat("Settings domain \"%s\": no file at \"%s\" yet; the defaults are used.", domain, path));
		settings = copy_settings(defaults);
		_emit_settings_changed(_get_changed_names(previous, settings));
		emit_signal(SNAME("loaded"));
		return ERR_FILE_NOT_FOUND;
	}

	Error err = OK;
	const String text = FileAccess::get_file_as_string(path, &err);
	String error;
	Ref<Resource> loaded;
	PackedStringArray present;
	const String type_name = ResourceTextSerializer::get_type_name(defaults.ptr());
	// The extension decides; an unknown extension is read in the format of `settings/storage/format`.
	const ResourceTextSerializer::Format format = _get_format_for_path(path);

	if (err != OK) {
		error = vformat("can't read the file (%s)", error_names[err]);
	} else if (format == ResourceTextSerializer::FORMAT_CSV) {
		err = ResourceTextSerializer::parse_csv(text, type_name, ResourceTextSerializer::get_default_options(), loaded, error);
		if (err == OK) {
			// CSV holds only the list; the other values keep their defaults.
			LocalVector<PropertyInfo> infos;
			get_value_infos(loaded.ptr(), infos);
			for (const PropertyInfo &info : infos) {
				if (info.type == Variant::ARRAY) {
					present.push_back(info.name);
				}
			}
		}
	} else {
		Dictionary tree;
		if (format == ResourceTextSerializer::FORMAT_JSON) {
			Ref<JSON> json;
			json.instantiate();
			err = json->parse(text);
			if (err != OK) {
				error = vformat("JSON parse error on line %d: %s", json->get_error_line(), json->get_error_message());
			} else if (json->get_data().get_type() != Variant::DICTIONARY) {
				err = ERR_INVALID_DATA;
				error = "the JSON must contain an object";
			} else {
				tree = json->get_data();
			}
		} else if (format == ResourceTextSerializer::FORMAT_INI || format == ResourceTextSerializer::FORMAT_MAX) {
			err = ResourceTextSerializer::parse_ini_tree(text, tree, error);
		} else {
			// XML, TOML, YAML: the format reads the resource, the tree is taken from it. These files hold all values
			// (see save()), so every value of the tree is present in the file.
			const ResourceTextSerializer::Options read_options = ResourceTextSerializer::get_default_options();
			Ref<Resource> parsed;
			err = ResourceTextSerializer::parse_format(format, text, type_name, read_options, parsed, error);
			if (err == OK) {
				err = ResourceTextSerializer::encode_resource(parsed, read_options, true, tree, error);
			}
		}
		if (err == OK) {
			const String version_key = String("metadata/") + META_SETTINGS_VERSION;
			const int current_version = get_format_version();
			int version = current_version;
			if (tree.has(version_key)) {
				version = int(tree[version_key]);
				tree.erase(version_key);
			}
			if (version < current_version) {
				tree = _migrate_settings(version, tree);
			} else if (version > current_version) {
				WARN_PRINT(vformat("Settings domain \"%s\": \"%s\" was written with version %d; this game reads version %d.", domain, path, version, current_version));
			}
			for (const KeyValue<Variant, Variant> &kv : tree) {
				const String key = kv.key;
				if (!key.begins_with("$") && !key.begins_with("metadata/")) {
					present.push_back(key);
				}
			}
			err = ResourceTextSerializer::decode_resource(tree, type_name, loaded, error);
		}
	}

	if (err != OK || loaded.is_null()) {
		// The file is kept as it is until the next save().
		WARN_PRINT(vformat("Settings domain \"%s\": can't load \"%s\" (%s); the defaults are used and the file is not overwritten until the next save.", domain, path, error));
		const Error result_error = err != OK ? err : ERR_FILE_CORRUPT;
		settings = copy_settings(defaults);
		emit_signal(SNAME("load_failed"), int(result_error), path);
		_emit_settings_changed(_get_changed_names(previous, settings));
		emit_signal(SNAME("loaded"));
		return result_error;
	}

	Ref<Resource> result = copy_settings(defaults);
	for (const String &name : present) {
		if (_object_has_value(result, name)) {
			result->set(name, copy_value(loaded->get(name)));
		}
	}
	settings = result;
	// Clamp hand-edited values.
	for (const String &name : present) {
		if (_object_has_value(settings, name)) {
			settings->set(name, _validate(name, settings->get(name)));
		}
	}
	_validate_loaded_settings(settings);
	_emit_settings_changed(_get_changed_names(previous, settings));
	emit_signal(SNAME("loaded"));
	return OK;
}

Error SettingsServer::save() {
	_ensure_initialized();
	save_queued = false;
	const String path = get_file_path();
	if (path.is_empty()) {
		ERR_PRINT(vformat("Settings domain \"%s\": no file path to save to.", get_domain()));
		emit_signal(SNAME("save_failed"), int(ERR_FILE_BAD_PATH), path);
		return ERR_FILE_BAD_PATH;
	}
	const StringName domain = get_domain();
	// While a confirmation is pending, the confirmed state is saved.
	const Ref<Resource> data = confirmation_active ? confirmation_backup : applied;

	ResourceTextSerializer::Options options = ResourceTextSerializer::get_default_options();
	const ResourceTextSerializer::Format format = _get_format_for_path(path);
	String text;
	String error;
	Error err = OK;
	if (format == ResourceTextSerializer::FORMAT_CSV) {
		err = ResourceTextSerializer::write_csv(data, options, text, error);
	} else {
		Ref<Resource> copy = data->duplicate(false);
		copy->set_meta(META_SETTINGS_VERSION, get_format_version());
		const bool tree_format = format == ResourceTextSerializer::FORMAT_JSON || format == ResourceTextSerializer::FORMAT_INI || format == ResourceTextSerializer::FORMAT_MAX;
		// Only JSON and INI are read back as a tree with the missing values taken from the defaults.
		if (tree_format && bool(_get_project_setting(SETTING_ONLY_CHANGED, true))) {
			// The serializer compares a value with its default by `==`, which compares the resources of a list by
			// identity. A value equal to its default by its stored properties (values_equal()) is put into the
			// compared defaults as the very same value, so it is skipped like an unchanged single value.
			Ref<Resource> compared_defaults = defaults->duplicate(false);
			LocalVector<PropertyInfo> infos;
			get_value_infos(data.ptr(), infos);
			for (const PropertyInfo &info : infos) {
				const Variant value = data->get(info.name);
				if (values_equal(value, defaults->get(info.name))) {
					compared_defaults->set(info.name, value);
				}
			}
			options.defaults = compared_defaults;
		}
		if (format == ResourceTextSerializer::FORMAT_JSON) {
			err = ResourceTextSerializer::write_json(copy, options, text, error);
		} else if (tree_format) {
			// Plain INI: players and other tools can edit it.
			options.ini_plain_values = true;
			err = ResourceTextSerializer::write_ini(copy, options, text, error);
		} else {
			err = ResourceTextSerializer::write_format(format, copy, options, text, error);
		}
	}
	String message;
	if (err != OK) {
		message = vformat("can't write \"%s\": %s", path, error);
	} else {
		const String folder = path.get_base_dir();
		if (!folder.is_empty() && !DirAccess::dir_exists_absolute(folder)) {
			err = DirAccess::make_dir_recursive_absolute(folder);
			if (err != OK) {
				message = vformat("can't create the folder \"%s\"", folder);
			}
		}
	}
	if (err == OK) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
		if (file.is_null()) {
			if (err == OK) {
				err = ERR_FILE_CANT_OPEN;
			}
			message = vformat("can't open \"%s\" for writing", path);
		} else if (!file->store_string(text)) {
			err = ERR_FILE_CANT_WRITE;
			message = vformat("can't write \"%s\"", path);
		}
	}
	if (err != OK) {
		ERR_PRINT(vformat("Settings domain \"%s\": %s.", domain, message));
		emit_signal(SNAME("save_failed"), int(err), path);
		return err;
	}
	unsaved = false;
	emit_signal(SNAME("saved"));
	return OK;
}

void SettingsServer::_queue_autosave() {
	if (save_queued || !bool(_get_project_setting(SETTING_AUTOSAVE, true))) {
		return;
	}
	save_queued = true;
	// Once per frame at most, and before the Web export syncs `user://` to IndexedDB.
	callable_mp(this, &SettingsServer::flush_autosave).call_deferred();
}

void SettingsServer::flush_autosave() {
	if (save_queued) {
		save();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Startup.

void SettingsServer::startup() {
	if (started) {
		return;
	}
	started = true;
	in_startup = true;
	// Captures the defaults now: the engine state after the project settings, before the player's file.
	_initialize();
	load();
	apply();
	// The engine runs with these values now: nothing waits for a restart.
	restart_baseline = copy_settings(applied);
	restart_required_names.clear();
	in_startup = false;
}

bool SettingsServer::is_started() const {
	return started;
}

/////////////////////////////////////////////////////////////////////////////
// Bindings.

void SettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_domain"), &SettingsServer::get_domain);
	ClassDB::bind_method(D_METHOD("get_format_version"), &SettingsServer::get_format_version);
	ClassDB::bind_method(D_METHOD("get_apply_phase"), &SettingsServer::get_apply_phase);

	ClassDB::bind_method(D_METHOD("get_settings"), &SettingsServer::get_settings);
	ClassDB::bind_method(D_METHOD("get_applied_settings"), &SettingsServer::get_applied_settings);
	ClassDB::bind_method(D_METHOD("get_default_settings"), &SettingsServer::get_default_settings);
	ClassDB::bind_method(D_METHOD("get_setting_names"), &SettingsServer::get_setting_names);
	ClassDB::bind_method(D_METHOD("has_setting", "name"), &SettingsServer::has_setting);
	ClassDB::bind_method(D_METHOD("set_value", "name", "value"), &SettingsServer::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "name"), &SettingsServer::get_value);
	ClassDB::bind_method(D_METHOD("get_applied_value", "name"), &SettingsServer::get_applied_value);
	ClassDB::bind_method(D_METHOD("get_default_value", "name"), &SettingsServer::get_default_value);
	ClassDB::bind_method(D_METHOD("is_value_supported", "name"), &SettingsServer::is_value_supported);

	ClassDB::bind_method(D_METHOD("bind_setting_property", "name", "target", "property", "flags"), &SettingsServer::bind_setting_property, DEFVAL(SETTING_FLAG_NONE));
	ClassDB::bind_method(D_METHOD("bind_setting_singleton_property", "name", "singleton", "property", "flags"), &SettingsServer::bind_setting_singleton_property, DEFVAL(SETTING_FLAG_NONE));
	ClassDB::bind_method(D_METHOD("bind_setting_callbacks", "name", "setter", "getter", "flags"), &SettingsServer::bind_setting_callbacks, DEFVAL(SETTING_FLAG_NONE));
	ClassDB::bind_method(D_METHOD("unbind_setting", "name"), &SettingsServer::unbind_setting);
	ClassDB::bind_method(D_METHOD("is_setting_bound", "name"), &SettingsServer::is_setting_bound);
	ClassDB::bind_method(D_METHOD("set_setting_flags", "name", "flags"), &SettingsServer::set_setting_flags);
	ClassDB::bind_method(D_METHOD("get_setting_flags", "name"), &SettingsServer::get_setting_flags);

	ClassDB::bind_method(D_METHOD("set_apply_mode", "mode"), &SettingsServer::set_apply_mode);
	ClassDB::bind_method(D_METHOD("get_apply_mode"), &SettingsServer::get_apply_mode);
	ClassDB::bind_method(D_METHOD("is_dirty"), &SettingsServer::is_dirty);
	ClassDB::bind_method(D_METHOD("get_dirty_setting_names"), &SettingsServer::get_dirty_setting_names);
	ClassDB::bind_method(D_METHOD("get_restart_required_names"), &SettingsServer::get_restart_required_names);
	ClassDB::bind_method(D_METHOD("revert"), &SettingsServer::revert);
	ClassDB::bind_method(D_METHOD("reset_value", "name"), &SettingsServer::reset_value);
	ClassDB::bind_method(D_METHOD("reset_to_defaults"), &SettingsServer::reset_to_defaults);

	ClassDB::bind_method(D_METHOD("set_session_override", "name", "value"), &SettingsServer::set_session_override);
	ClassDB::bind_method(D_METHOD("clear_session_override", "name"), &SettingsServer::clear_session_override);
	ClassDB::bind_method(D_METHOD("has_session_override", "name"), &SettingsServer::has_session_override);

	ClassDB::bind_method(D_METHOD("apply"), &SettingsServer::apply);
	ClassDB::bind_method(D_METHOD("apply_with_confirmation", "timeout_sec"), &SettingsServer::apply_with_confirmation, DEFVAL(-1.0));
	ClassDB::bind_method(D_METHOD("confirm_changes"), &SettingsServer::confirm_changes);
	ClassDB::bind_method(D_METHOD("cancel_confirmation"), &SettingsServer::cancel_confirmation);
	ClassDB::bind_method(D_METHOD("is_confirmation_pending"), &SettingsServer::is_confirmation_pending);
	ClassDB::bind_method(D_METHOD("get_confirmation_time_left"), &SettingsServer::get_confirmation_time_left);
	ClassDB::bind_method(D_METHOD("read_from_engine"), &SettingsServer::read_from_engine);

	ClassDB::bind_method(D_METHOD("load"), &SettingsServer::load);
	ClassDB::bind_method(D_METHOD("save"), &SettingsServer::save);
	ClassDB::bind_method(D_METHOD("set_file_path", "path"), &SettingsServer::set_file_path);
	ClassDB::bind_method(D_METHOD("get_file_path"), &SettingsServer::get_file_path);
	ClassDB::bind_method(D_METHOD("has_unsaved_changes"), &SettingsServer::has_unsaved_changes);
	ClassDB::bind_method(D_METHOD("is_started"), &SettingsServer::is_started);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "apply_mode", PROPERTY_HINT_ENUM, "Immediate,Manual"), "set_apply_mode", "get_apply_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_path", PROPERTY_HINT_NONE, ""), "set_file_path", "get_file_path");

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("settings_changed", PropertyInfo(Variant::PACKED_STRING_ARRAY, "names")));
	ADD_SIGNAL(MethodInfo("applied", PropertyInfo(Variant::PACKED_STRING_ARRAY, "names")));
	ADD_SIGNAL(MethodInfo("read_from_engine_finished"));
	ADD_SIGNAL(MethodInfo("loaded"));
	ADD_SIGNAL(MethodInfo("saved"));
	ADD_SIGNAL(MethodInfo("load_failed", PropertyInfo(Variant::INT, "error", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Error"), PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("save_failed", PropertyInfo(Variant::INT, "error", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Error"), PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("reset"));
	ADD_SIGNAL(MethodInfo("confirmation_requested", PropertyInfo(Variant::FLOAT, "timeout_sec")));
	ADD_SIGNAL(MethodInfo("confirmation_finished", PropertyInfo(Variant::BOOL, "kept")));
	ADD_SIGNAL(MethodInfo("restart_required", PropertyInfo(Variant::PACKED_STRING_ARRAY, "names")));

	BIND_ENUM_CONSTANT(APPLY_MODE_IMMEDIATE);
	BIND_ENUM_CONSTANT(APPLY_MODE_MANUAL);

	BIND_ENUM_CONSTANT(APPLY_PHASE_NONE);
	BIND_ENUM_CONSTANT(APPLY_PHASE_PRE_DISPLAY);
	BIND_ENUM_CONSTANT(APPLY_PHASE_SERVERS);
	BIND_ENUM_CONSTANT(APPLY_PHASE_SCENE);
	BIND_ENUM_CONSTANT(APPLY_PHASE_MAX);

	BIND_BITFIELD_FLAG(SETTING_FLAG_NONE);
	BIND_BITFIELD_FLAG(SETTING_FLAG_RESTART_REQUIRED);
	BIND_BITFIELD_FLAG(SETTING_FLAG_CONFIRM);
	BIND_BITFIELD_FLAG(SETTING_FLAG_NO_ENGINE_TARGET);

	GDVIRTUAL_BIND(_get_domain);
	GDVIRTUAL_BIND(_get_format_version);
	GDVIRTUAL_BIND(_get_apply_phase);
	GDVIRTUAL_BIND(_create_settings);
	GDVIRTUAL_BIND(_apply, "settings", "names");
	GDVIRTUAL_BIND(_read_from_engine, "settings");
	GDVIRTUAL_BIND(_validate_value, "name", "value");
	GDVIRTUAL_BIND(_migrate, "from_version", "data");
	GDVIRTUAL_BIND(_is_value_supported, "name");
}

SettingsServer::SettingsServer() {
}

SettingsServer::~SettingsServer() {
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->remove_server(this);
	}
}
