/**************************************************************************/
/*  resource_format_callable.h                                            */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

// Simple resource formats registered with one call from GDScript, C# or GDExtension
// (`ResourceLoader.register_extension()`, `ResourceSaver.register_extension()`).
// Each registered extension gets one wrapper that is added through the normal
// `add_resource_format_loader()` / `add_resource_format_saver()`, so order, cache, threading and the
// extension index behave exactly like for a hand-written format.
//
// Threads: the callable runs on the thread that loads or saves (a WorkerThreadPool thread for
// `load_threaded_request()`). No lock is held while it runs: the callable of a wrapper never changes,
// registering the extension again creates a new wrapper.

class ResourceFormatLoaderCallable : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderCallable, ResourceFormatLoader);

	String extension; // Lower-case, without the leading dot.
	StringName type;
	Callable load_callable;

	static Mutex registry_mutex;
	static HashMap<String, Ref<ResourceFormatLoaderCallable>> registry;

public:
	// Returns OK, or ERR_INVALID_PARAMETER for an invalid extension, callable or type. Replaces an earlier registration.
	static Error register_extension(const String &p_extension, const Callable &p_load, const StringName &p_type, bool p_at_front = false);
	// Returns OK, or ERR_DOES_NOT_EXIST if the extension was not registered with register_extension().
	static Error unregister_extension(const String &p_extension);
	static bool is_extension_registered(const String &p_extension);
	// Removes every registration; called before the script languages shut down.
	static void unregister_all();

	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
	virtual String get_resource_script_class(const String &p_path) const override;

	// Data files, often written at runtime: no `*.uid` files next to them.
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override { return ResourceUID::INVALID_ID; }
	virtual bool has_custom_uid_support() const override { return true; }
};

class ResourceFormatSaverCallable : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverCallable, ResourceFormatSaver);

	String extension; // Lower-case, without the leading dot.
	StringName type;
	Callable save_callable;

	static Mutex registry_mutex;
	static HashMap<String, Ref<ResourceFormatSaverCallable>> registry;

public:
	static Error register_extension(const String &p_extension, const Callable &p_save, const StringName &p_type, bool p_at_front = false);
	static Error unregister_extension(const String &p_extension);
	static bool is_extension_registered(const String &p_extension);
	static void unregister_all();

	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};
