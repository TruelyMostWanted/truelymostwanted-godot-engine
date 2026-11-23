/**************************************************************************/
/*  resource_format_callable.cpp                                          */
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

#include "resource_format_callable.h"

#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/local_vector.h"

/////////////////////////////////////////////////////////////////////////////
// Shared helpers.

// Lower-case, without leading dots.
static String _normalize_extension(const String &p_extension) {
	String extension = p_extension.strip_edges().to_lower();
	while (extension.begins_with(".")) {
		extension = extension.substr(1);
	}
	return extension;
}

static bool _is_valid_extension(const String &p_extension) {
	return p_extension.is_valid_filename() && !p_extension.ends_with(".") && !p_extension.contains("..");
}

// Suffix match, so that multi-dot extensions such as `quiz.json` work.
static bool _path_has_extension(const String &p_path, const String &p_extension) {
	const String suffix = "." + p_extension;
	return p_path.length() > suffix.length() && p_path.right(suffix.length()).nocasecmp_to(suffix) == 0;
}

// Native class of a native class or of a global script class, or an empty StringName.
static StringName _get_native_type(const StringName &p_type) {
	if (ScriptServer::is_global_class(p_type)) {
		return ScriptServer::get_global_class_native_base(p_type);
	}
	return ClassDB::class_exists(p_type) ? p_type : StringName();
}

static bool _is_resource_type(const StringName &p_type) {
	const StringName native_type = _get_native_type(p_type);
	return !native_type.is_empty() && ClassDB::is_parent_class(native_type, Resource::get_class_static());
}

// True if `p_type` is `p_base` or inherits it. Both may be native classes or global script classes.
static bool _type_inherits(const StringName &p_type, const StringName &p_base) {
	StringName current = p_type;
	while (!current.is_empty()) {
		if (current == p_base) {
			return true;
		}
		if (!ScriptServer::is_global_class(current)) {
			return ClassDB::is_parent_class(current, p_base);
		}
		current = ScriptServer::get_global_class_base(current);
	}
	return false;
}

static String _get_error_name(Error p_error) {
	if (p_error >= 0 && p_error < ERR_MAX) {
		return error_names[p_error];
	}
	return itos(p_error);
}

/////////////////////////////////////////////////////////////////////////////
// Loader.

Mutex ResourceFormatLoaderCallable::registry_mutex;
HashMap<String, Ref<ResourceFormatLoaderCallable>> ResourceFormatLoaderCallable::registry;

Error ResourceFormatLoaderCallable::register_extension(const String &p_extension, const Callable &p_load, const StringName &p_type, bool p_at_front) {
	const String extension = _normalize_extension(p_extension);
	ERR_FAIL_COND_V_MSG(!_is_valid_extension(extension), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': it is not a valid file extension.", p_extension));
	ERR_FAIL_COND_V_MSG(!p_load.is_valid(), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': the load callable is not valid.", extension));
	const StringName type = p_type.is_empty() ? Resource::get_class_static() : p_type;
	ERR_FAIL_COND_V_MSG(!_is_resource_type(type), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': the type '%s' is neither a Resource class nor a global script class that extends Resource.", extension, type));

	Ref<ResourceFormatLoaderCallable> format;
	format.instantiate();
	format->extension = extension;
	format->type = type;
	format->load_callable = p_load;

	Ref<ResourceFormatLoaderCallable> previous;
	{
		// The loader list changes under the registry lock, so that concurrent registrations keep both in step.
		// ResourceLoader prints nothing here, because the registry only removes formats that it added itself.
		MutexLock lock(registry_mutex);
		Ref<ResourceFormatLoaderCallable> *existing = registry.getptr(extension);
		if (existing) {
			previous = *existing;
			*existing = format;
			::ResourceLoader::remove_resource_format_loader(previous);
		} else {
			registry.insert(extension, format);
		}
		::ResourceLoader::add_resource_format_loader(format, p_at_front);
	}
	return OK; // `previous` and its callable are released here, without a lock held.
}

Error ResourceFormatLoaderCallable::unregister_extension(const String &p_extension) {
	const String extension = _normalize_extension(p_extension);
	Ref<ResourceFormatLoaderCallable> previous;
	{
		MutexLock lock(registry_mutex);
		Ref<ResourceFormatLoaderCallable> *existing = registry.getptr(extension);
		if (existing) {
			previous = *existing;
			registry.erase(extension);
			::ResourceLoader::remove_resource_format_loader(previous);
		}
	}
	return previous.is_valid() ? OK : ERR_DOES_NOT_EXIST;
}

bool ResourceFormatLoaderCallable::is_extension_registered(const String &p_extension) {
	const String extension = _normalize_extension(p_extension);
	MutexLock lock(registry_mutex);
	return registry.has(extension);
}

void ResourceFormatLoaderCallable::unregister_all() {
	LocalVector<Ref<ResourceFormatLoaderCallable>> removed;
	{
		MutexLock lock(registry_mutex);
		for (const KeyValue<String, Ref<ResourceFormatLoaderCallable>> &E : registry) {
			::ResourceLoader::remove_resource_format_loader(E.value);
			removed.push_back(E.value);
		}
		registry.clear();
	}
	// `removed` and the callables are released here, without a lock held.
}

Ref<Resource> ResourceFormatLoaderCallable::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_UNAVAILABLE;
	}
	ERR_FAIL_COND_V_MSG(!load_callable.is_valid(), Ref<Resource>(), vformat("Can't load '%s': the load callable of the resource extension '%s' is no longer valid (was its object freed?).", p_path, extension));

	// Called on this thread, without a lock held.
	const Variant path = p_path;
	const Variant *args[1] = { &path };
	Variant result;
	Callable::CallError call_error;
	load_callable.callp(args, 1, result, call_error);
	ERR_FAIL_COND_V_MSG(call_error.error != Callable::CallError::CALL_OK, Ref<Resource>(), vformat("Can't load '%s': calling the load callable of the resource extension '%s' failed: %s.", p_path, extension, Variant::get_callable_error_text(load_callable, args, 1, call_error)));

	Error err = ERR_FILE_CORRUPT;
	Ref<Resource> resource;
	String reason;
	if (result.get_type() == Variant::INT) {
		err = Error(int(result));
		if (err == OK) {
			err = ERR_INVALID_DATA;
			reason = "the load callable returned OK instead of a resource";
		} else {
			reason = _get_error_name(err);
		}
	} else if (result.get_type() == Variant::OBJECT) {
		resource = result;
		if (resource.is_valid()) {
			err = OK;
		} else if (result.get_validated_object() == nullptr) {
			reason = "the load callable returned null";
		} else {
			err = ERR_INVALID_DATA;
			reason = "the load callable returned an object that is not a Resource";
		}
	} else if (result.get_type() == Variant::NIL) {
		reason = "the load callable returned null";
	} else {
		err = ERR_INVALID_DATA;
		reason = vformat("the load callable returned a %s instead of a Resource or an Error", Variant::get_type_name(result.get_type()));
	}

	if (r_error) {
		*r_error = err;
	}
	if (err == ERR_FILE_UNRECOGNIZED) {
		return Ref<Resource>(); // Not a file of this format: the next loader may take it.
	}
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("Can't load '%s' with the resource extension '%s': %s.", p_path, extension, reason));
	return resource;
}

void ResourceFormatLoaderCallable::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(extension);
}

bool ResourceFormatLoaderCallable::recognize_path(const String &p_path, const String &p_for_type) const {
	return _path_has_extension(p_path, extension) && (p_for_type.is_empty() || handles_type(p_for_type));
}

bool ResourceFormatLoaderCallable::handles_type(const String &p_type) const {
	return _type_inherits(type, p_type);
}

String ResourceFormatLoaderCallable::get_resource_type(const String &p_path) const {
	if (!_path_has_extension(p_path, extension)) {
		return String();
	}
	return _get_native_type(type);
}

String ResourceFormatLoaderCallable::get_resource_script_class(const String &p_path) const {
	if (!_path_has_extension(p_path, extension) || !ScriptServer::is_global_class(type)) {
		return String();
	}
	return type;
}

/////////////////////////////////////////////////////////////////////////////
// Saver.

Mutex ResourceFormatSaverCallable::registry_mutex;
HashMap<String, Ref<ResourceFormatSaverCallable>> ResourceFormatSaverCallable::registry;

Error ResourceFormatSaverCallable::register_extension(const String &p_extension, const Callable &p_save, const StringName &p_type, bool p_at_front) {
	const String extension = _normalize_extension(p_extension);
	ERR_FAIL_COND_V_MSG(!_is_valid_extension(extension), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': it is not a valid file extension.", p_extension));
	ERR_FAIL_COND_V_MSG(!p_save.is_valid(), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': the save callable is not valid.", extension));
	const StringName type = p_type.is_empty() ? Resource::get_class_static() : p_type;
	ERR_FAIL_COND_V_MSG(!_is_resource_type(type), ERR_INVALID_PARAMETER, vformat("Can't register the resource extension '%s': the type '%s' is neither a Resource class nor a global script class that extends Resource.", extension, type));

	Ref<ResourceFormatSaverCallable> format;
	format.instantiate();
	format->extension = extension;
	format->type = type;
	format->save_callable = p_save;

	Ref<ResourceFormatSaverCallable> previous;
	{
		// See ResourceFormatLoaderCallable::register_extension().
		MutexLock lock(registry_mutex);
		Ref<ResourceFormatSaverCallable> *existing = registry.getptr(extension);
		if (existing) {
			previous = *existing;
			*existing = format;
			::ResourceSaver::remove_resource_format_saver(previous);
		} else {
			registry.insert(extension, format);
		}
		::ResourceSaver::add_resource_format_saver(format, p_at_front);
	}
	return OK;
}

Error ResourceFormatSaverCallable::unregister_extension(const String &p_extension) {
	const String extension = _normalize_extension(p_extension);
	Ref<ResourceFormatSaverCallable> previous;
	{
		MutexLock lock(registry_mutex);
		Ref<ResourceFormatSaverCallable> *existing = registry.getptr(extension);
		if (existing) {
			previous = *existing;
			registry.erase(extension);
			::ResourceSaver::remove_resource_format_saver(previous);
		}
	}
	return previous.is_valid() ? OK : ERR_DOES_NOT_EXIST;
}

bool ResourceFormatSaverCallable::is_extension_registered(const String &p_extension) {
	const String extension = _normalize_extension(p_extension);
	MutexLock lock(registry_mutex);
	return registry.has(extension);
}

void ResourceFormatSaverCallable::unregister_all() {
	LocalVector<Ref<ResourceFormatSaverCallable>> removed;
	{
		MutexLock lock(registry_mutex);
		for (const KeyValue<String, Ref<ResourceFormatSaverCallable>> &E : registry) {
			::ResourceSaver::remove_resource_format_saver(E.value);
			removed.push_back(E.value);
		}
		registry.clear();
	}
}

Error ResourceFormatSaverCallable::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	ERR_FAIL_COND_V_MSG(!save_callable.is_valid(), ERR_UNAVAILABLE, vformat("Can't save '%s': the save callable of the resource extension '%s' is no longer valid (was its object freed?).", p_path, extension));

	// Called on this thread, without a lock held.
	const Variant resource = p_resource;
	const Variant path = p_path;
	const Variant *args[2] = { &resource, &path };
	Variant result;
	Callable::CallError call_error;
	save_callable.callp(args, 2, result, call_error);
	ERR_FAIL_COND_V_MSG(call_error.error != Callable::CallError::CALL_OK, ERR_UNAVAILABLE, vformat("Can't save '%s': calling the save callable of the resource extension '%s' failed: %s.", p_path, extension, Variant::get_callable_error_text(save_callable, args, 2, call_error)));

	if (result.get_type() == Variant::NIL) {
		return OK; // A function without a return value.
	}
	ERR_FAIL_COND_V_MSG(result.get_type() != Variant::INT, ERR_INVALID_DATA, vformat("Can't save '%s' with the resource extension '%s': the save callable returned a %s instead of an Error.", p_path, extension, Variant::get_type_name(result.get_type())));
	const Error err = Error(int(result));
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't save '%s' with the resource extension '%s': %s.", p_path, extension, _get_error_name(err)));
	return OK;
}

bool ResourceFormatSaverCallable::recognize(const Ref<Resource> &p_resource) const {
	if (p_resource.is_null()) {
		return false;
	}
	if (!ScriptServer::is_global_class(type)) {
		return p_resource->is_class(type);
	}
	for (Ref<Script> script = p_resource->get_script(); script.is_valid(); script = script->get_base_script()) {
		if (script->get_global_name() == type) {
			return true;
		}
	}
	return false;
}

void ResourceFormatSaverCallable::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource)) {
		p_extensions->push_back(extension);
	}
}

bool ResourceFormatSaverCallable::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	return _path_has_extension(p_path, extension);
}
