/**************************************************************************/
/*  resource_loader.h                                                     */
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
#include "core/io/resource_loader_constants.h"
#include "core/object/gdvirtual.gen.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"

namespace CoreBind {
class ResourceLoader;
}

class ConditionVariable;

template <int Tag>
class SafeBinaryMutex;

class ResourceFormatLoader : public RefCounted {
	GDCLASS(ResourceFormatLoader, RefCounted);

public:
	using CacheMode = ResourceLoaderConstants::CacheMode;
	static constexpr CacheMode CACHE_MODE_IGNORE = ResourceLoaderConstants::CACHE_MODE_IGNORE;
	static constexpr CacheMode CACHE_MODE_REUSE = ResourceLoaderConstants::CACHE_MODE_REUSE;
	static constexpr CacheMode CACHE_MODE_REPLACE = ResourceLoaderConstants::CACHE_MODE_REPLACE;
	static constexpr CacheMode CACHE_MODE_IGNORE_DEEP = ResourceLoaderConstants::CACHE_MODE_IGNORE_DEEP;
	static constexpr CacheMode CACHE_MODE_REPLACE_DEEP = ResourceLoaderConstants::CACHE_MODE_REPLACE_DEEP;

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(Vector<String>, _get_recognized_extensions)
	GDVIRTUAL2RC(bool, _recognize_path, String, StringName)
	GDVIRTUAL1RC(bool, _handles_type, StringName)
	GDVIRTUAL1RC(String, _get_resource_type, String)
	GDVIRTUAL1RC(String, _get_resource_script_class, String)
	GDVIRTUAL1RC(ResourceUID::ID, _get_resource_uid, String)
	GDVIRTUAL2RC(Vector<String>, _get_dependencies, String, bool)
	GDVIRTUAL1RC(Vector<String>, _get_classes_used, String)
	GDVIRTUAL2RC(Error, _rename_dependencies, String, Dictionary)
	GDVIRTUAL1RC(bool, _exists, String)

	GDVIRTUAL4RC_REQUIRED(Variant, _load, String, String, bool, CacheMode)
#ifndef DISABLE_DEPRECATED
	GDVIRTUAL4RC_COMPAT(_load_bind_compat_367, _load, Variant, String, String, bool, int)
#endif

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE);
	virtual bool exists(const String &p_path) const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const;
	// True if recognize_path() matches only the suffixes from get_recognized_extensions(). ResourceLoader then finds
	// the loader through its extension index (the list is read when the loader is added, when
	// `ResourceLoader::rebuild_extension_index()` is called, and for C++ lists when ClassDB's resource extensions change);
	// otherwise the loader is asked for every path.
	virtual bool recognizes_path_by_extension() const;
	// True if the loader is implemented by a script, C# or GDExtension, so its extension list comes from script code.
	bool has_scripted_extensions() const;
	virtual bool handles_type(const String &p_type) const;
	virtual void get_classes_used(const String &p_path, HashSet<StringName> *r_classes);
	virtual String get_resource_type(const String &p_path) const;
	virtual String get_resource_script_class(const String &p_path) const;
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const;
	virtual bool has_custom_uid_support() const;
	virtual void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false);
	virtual Error rename_dependencies(const String &p_path, const HashMap<String, String> &p_map);
	virtual bool is_import_valid(const String &p_path) const { return true; }
	virtual bool is_imported(const String &p_path) const { return false; }
	virtual int get_import_order(const String &p_path) const { return 0; }
	virtual String get_import_group_file(const String &p_path) const { return ""; } //no group

	virtual ~ResourceFormatLoader() {}
};

typedef void (*ResourceLoadErrorNotify)(const String &p_text);
typedef void (*DependencyErrorNotify)(const String &p_loading, const String &p_which, const String &p_type);

typedef Error (*ResourceLoaderImport)(const String &p_path);
typedef void (*ResourceLoadedCallback)(Ref<Resource> p_resource, const String &p_path);

// Immutable snapshot of the registered format loaders, in lookup order (first match wins).
// Copying it is cheap: the list and its index are copy-on-write and shared with every copy until a change replaces them.
struct ResourceLoaderFormatSnapshot {
	// What the index knows about one loader, read by `read_entry()` outside any lock.
	struct Entry {
		Vector<String> keys; // Lower-case extension keys (the part after the last dot), without duplicates.
		bool by_extension = false; // See `ResourceFormatLoader::recognizes_path_by_extension()`.
		bool scripted = false; // The list comes from a script, C# or GDExtension; never re-read implicitly.
	};

	Vector<Ref<ResourceFormatLoader>> formats;
	Vector<Entry> entries; // One per format, same order.
	uint32_t class_db_version = 0; // `ClassDB::get_resource_base_extensions_version()` when the C++ lists were read.

	// Index by extension, built by `build_index()` or `index_entries()`. For each lower-case extension key (the part after the last dot
	// of a recognized extension), the positions of the loaders that recognize it, ascending.
	Vector<String> index_extensions; // Sorted.
	Vector<uint32_t> index_offsets; // Positions of `index_extensions[i]` are `index_positions[index_offsets[i]..index_offsets[i + 1]]`.
	Vector<uint32_t> index_positions;
	// Loaders that do not recognize paths by extension only (see `ResourceFormatLoader::recognizes_path_by_extension()`), ascending.
	Vector<uint32_t> always_asked;

	// Asks the loader for its extensions. May run script code: never call it while holding a lock.
	static Entry read_entry(const Ref<ResourceFormatLoader> &p_format_loader);
	// Reads the entry of every format, then builds the index.
	void build_index();
	// Builds the index from `entries` without calling any loader.
	void index_entries();

	// The loaders that may recognize a path, in registration order: the indexed loaders for the path's last extension
	// and the always-asked loaders. Each candidate must still be checked with `recognize_path()`.
	// The range refers to this snapshot, which must outlive it.
	class Candidates {
		const ResourceLoaderFormatSnapshot *snapshot = nullptr;
		const uint32_t *indexed = nullptr;
		uint32_t indexed_count = 0;

	public:
		class Iterator {
			const ResourceLoaderFormatSnapshot *snapshot = nullptr;
			const uint32_t *indexed = nullptr;
			const uint32_t *indexed_end = nullptr;
			const uint32_t *asked = nullptr;
			const uint32_t *asked_end = nullptr;

			_FORCE_INLINE_ bool _take_indexed() const { return asked == asked_end || (indexed != indexed_end && *indexed < *asked); }

		public:
			Iterator(const ResourceLoaderFormatSnapshot *p_snapshot, const uint32_t *p_indexed, const uint32_t *p_indexed_end, const uint32_t *p_asked, const uint32_t *p_asked_end) :
					snapshot(p_snapshot), indexed(p_indexed), indexed_end(p_indexed_end), asked(p_asked), asked_end(p_asked_end) {}

			const Ref<ResourceFormatLoader> &operator*() const { return snapshot->formats[_take_indexed() ? *indexed : *asked]; }
			Iterator &operator++() {
				if (_take_indexed()) {
					indexed++;
				} else {
					asked++;
				}
				return *this;
			}
			bool operator!=(const Iterator &p_other) const { return indexed != p_other.indexed || asked != p_other.asked; }
		};

		Candidates(const ResourceLoaderFormatSnapshot *p_snapshot, const uint32_t *p_indexed, uint32_t p_indexed_count) :
				snapshot(p_snapshot), indexed(p_indexed), indexed_count(p_indexed_count) {}

		Iterator begin() const;
		Iterator end() const;
	};

	Candidates candidates_for(const String &p_path) const;

	const Ref<ResourceFormatLoader> *begin() const { return formats.ptr(); }
	const Ref<ResourceFormatLoader> *end() const { return formats.ptr() + formats.size(); }
};

class ResourceLoader {
	friend class LoadToken;
	friend class CoreBind::ResourceLoader;

	struct ThreadLoadTask;

public:
	using CacheMode = ResourceLoaderConstants::CacheMode;
	static constexpr CacheMode CACHE_MODE_IGNORE = ResourceLoaderConstants::CACHE_MODE_IGNORE;
	static constexpr CacheMode CACHE_MODE_REUSE = ResourceLoaderConstants::CACHE_MODE_REUSE;
	static constexpr CacheMode CACHE_MODE_REPLACE = ResourceLoaderConstants::CACHE_MODE_REPLACE;
	static constexpr CacheMode CACHE_MODE_IGNORE_DEEP = ResourceLoaderConstants::CACHE_MODE_IGNORE_DEEP;
	static constexpr CacheMode CACHE_MODE_REPLACE_DEEP = ResourceLoaderConstants::CACHE_MODE_REPLACE_DEEP;

	enum ThreadLoadStatus {
		THREAD_LOAD_INVALID_RESOURCE,
		THREAD_LOAD_IN_PROGRESS,
		THREAD_LOAD_FAILED,
		THREAD_LOAD_LOADED
	};

	enum LoadThreadMode {
		LOAD_THREAD_FROM_CURRENT,
		LOAD_THREAD_SPAWN_SINGLE,
		LOAD_THREAD_DISTRIBUTE,
	};

	struct LoadToken : public RefCounted {
		String local_path;
		String user_path;
		uint32_t user_rc = 0; // Having user RC implies regular RC incremented in one, until the user RC reaches zero.
		ThreadLoadTask *task_if_unregistered = nullptr;

		void clear();

		virtual ~LoadToken();
	};

	static const int BINARY_MUTEX_TAG = 1;

	static Ref<LoadToken> _load_start(const String &p_path, const String &p_type_hint, LoadThreadMode p_thread_mode, CacheMode p_cache_mode, bool p_for_user = false);
	static Ref<Resource> _load_complete(LoadToken &p_load_token, Error *r_error);

private:
	static LoadToken *_load_threaded_request_reuse_user_token(const String &p_path);
	static void _load_threaded_request_setup_user_token(LoadToken *p_token, const String &p_path);

	static Ref<Resource> _load_complete_inner(LoadToken &p_load_token, Error *r_error, MutexLock<SafeBinaryMutex<BINARY_MUTEX_TAG>> &p_thread_load_lock);

	// Registered formats, without a size limit. Every change builds a new snapshot under `loaders_mutex`.
	// Readers take the current snapshot from `_get_loaders()` and iterate it without a lock,
	// so other threads may add or remove formats meanwhile.
	static ResourceLoaderFormatSnapshot loaders;
	static Mutex loaders_mutex;
	static ResourceLoaderFormatSnapshot _get_loaders();
	static bool timestamp_on_load;

	static void *err_notify_ud;
	static ResourceLoadErrorNotify err_notify;
	static void *dep_err_notify_ud;
	static DependencyErrorNotify dep_err_notify;
	static bool abort_on_missing_resource;
	static bool create_missing_resources_if_class_unavailable;
	static HashMap<String, Vector<String>> translation_remaps;

	static String _path_remap(const String &p_path, bool *r_translation_remapped = nullptr);
	friend class Resource;

	static SelfList<Resource>::List remapped_list;

	friend class ResourceFormatImporter;

	static Ref<Resource> _load(const String &p_path, const String &p_original_path, const String &p_type_hint, CacheMode p_cache_mode, Error *r_error, bool p_use_sub_threads, float *r_progress);

	static ResourceLoadedCallback _loaded_callback;

	static Ref<ResourceFormatLoader> _find_custom_resource_format_loader(const String &p_path);

	struct ThreadLoadTask {
		WorkerThreadPool::TaskID task_id = 0; // Used if run on a worker thread from the pool.
		Thread::ID thread_id = 0; // Used if running on an user thread (e.g., simple non-threaded load).
		int thread_index = -1;
		ConditionVariable *cond_var = nullptr; // In not in the worker pool or already awaiting, this is used as a secondary awaiting mechanism.
		uint32_t awaiters_count = 0;
		LoadToken *load_token = nullptr;
		String local_path;
		String type_hint;
		float progress = 0.0f;
		float max_reported_progress = 0.0f;
		uint64_t last_progress_check_main_thread_frame = UINT64_MAX;
		ThreadLoadStatus status = THREAD_LOAD_IN_PROGRESS;
		CacheMode cache_mode = CACHE_MODE_REUSE;
		Error error = OK;
		String error_message; // Errors printed while this task loaded, set only if it failed.
		Ref<Resource> resource;
		LocalVector<Ref<Resource>> resource_dependencies; // We need to keep these alive for as long as the task is alive at least.
		ThreadLoadTask *parent_task = nullptr;
		HashSet<String> sub_tasks;

		bool awaited : 1; // If it's in the pool, this helps not awaiting from more than one dependent thread.
		bool need_wait : 1;
		bool in_progress_check : 1; // Measure against recursion cycles in progress reporting. Cycles are not expected, but can happen due to how it's currently implemented.
		bool use_sub_threads : 1;
		bool started_load : 1;
		bool finished_load : 1;
		bool connections_propagated : 1;

		struct ResourceChangedConnection {
			Resource *source = nullptr;
			Callable callable;
			uint32_t flags = 0;
		};
		LocalVector<ResourceChangedConnection> resource_changed_connections;

		ThreadLoadTask() :
				awaited(false),
				need_wait(true),
				in_progress_check(false),
				use_sub_threads(false),
				started_load(false),
				finished_load(false),
				connections_propagated(false) {}
	};
	static void _run_load_task(void *p_userdata);

	static thread_local bool import_thread;
	static thread_local int load_nesting;
	static thread_local HashMap<int, HashMap<String, Ref<Resource>>> res_ref_overrides; // Outermost key is nesting level.
	static thread_local ThreadLoadTask *curr_load_task;

	static SafeBinaryMutex<BINARY_MUTEX_TAG> thread_load_mutex;
	friend SafeBinaryMutex<BINARY_MUTEX_TAG> &_get_res_loader_mutex();

	static HashMap<String, ThreadLoadTask> thread_load_tasks;
	static HashMap<int, String> thread_waiting_on;
	static LocalVector<int> yielders;
	static bool cleaning_tasks;

	static HashMap<String, LoadToken *> user_load_tokens;

	static float _dependency_get_progress(const String &p_path);

	static bool _ensure_load_progress();

	static String _validate_local_path(const String &p_path);

public:
	static Error load_threaded_request(const String &p_path, const String &p_type_hint = "", bool p_use_sub_threads = false, CacheMode p_cache_mode = CACHE_MODE_REUSE);
	static ThreadLoadStatus load_threaded_get_status(const String &p_path, float *r_progress = nullptr);
	static Ref<Resource> load_threaded_get(const String &p_path, Error *r_error = nullptr);

	static bool is_within_load() { return load_nesting > 0; }

	// Errors printed by the last load finished on this thread (`load()`, `load_threaded_get()`),
	// empty if it succeeded. Read right after the call; any later load on the thread replaces it.
	static thread_local String last_load_error_message;

	static void resource_changed_connect(Resource *p_source, const Callable &p_callable, uint32_t p_flags);
	static void resource_changed_disconnect(Resource *p_source, const Callable &p_callable);
	static void resource_changed_emit(Resource *p_source);

	static Ref<Resource> load(const String &p_path, const String &p_type_hint = "", CacheMode p_cache_mode = CACHE_MODE_REUSE, Error *r_error = nullptr);
	static bool exists(const String &p_path, const String &p_type_hint = "");

	static void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions);
	static void add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front = false);
	// The registered loaders and their extension index, for tests and diagnostics.
	static ResourceLoaderFormatSnapshot get_format_snapshot() { return _get_loaders(); }
	// Reads the extension lists of all registered loaders again and rebuilds the index.
	static void rebuild_extension_index();
	static void remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader);
	static void get_classes_used(const String &p_path, HashSet<StringName> *r_classes);
	static String get_resource_type(const String &p_path);
	static String get_resource_script_class(const String &p_path);
	static ResourceUID::ID get_resource_uid(const String &p_path);
	static bool should_create_uid_file(const String &p_path);
	static void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false);
	static Error rename_dependencies(const String &p_path, const HashMap<String, String> &p_map);
	static bool is_import_valid(const String &p_path);
	static String get_import_group_file(const String &p_path);
	static bool is_imported(const String &p_path);

	static void set_is_import_thread(bool p_import_thread);

	static void set_timestamp_on_load(bool p_timestamp) { timestamp_on_load = p_timestamp; }
	static bool get_timestamp_on_load() { return timestamp_on_load; }

	// Loaders can safely use this regardless which thread they are running on.
	static void notify_load_error(const String &p_err);

	static void set_error_notify_func(ResourceLoadErrorNotify p_err_notify) {
		err_notify = p_err_notify;
	}

	// Loaders can safely use this regardless which thread they are running on.
	static void notify_dependency_error(const String &p_path, const String &p_dependency, const String &p_type);

	static void set_dependency_error_notify_func(DependencyErrorNotify p_err_notify) {
		dep_err_notify = p_err_notify;
	}

	static void set_abort_on_missing_resources(bool p_abort) { abort_on_missing_resource = p_abort; }
	static bool get_abort_on_missing_resources() { return abort_on_missing_resource; }

	static String path_remap(const String &p_path);
	// Asked before translation_remaps (ResourceLocalizationServer, #344). Returns true if it handles `p_path`; its
	// resources are not flagged as translation-remapped, it relocalizes them itself. A missing file falls back to
	// `p_path` like a translation remap.
	typedef bool (*PathLocalizer)(const String &p_path, String &r_path);
	static PathLocalizer path_localizer;
	static String import_remap(const String &p_path);

	static void reload_translation_remaps();
	static void load_translation_remaps();
	static void clear_translation_remaps();

	static void clear_thread_load_tasks();

	static void set_load_callback(ResourceLoadedCallback p_callback);
	static ResourceLoaderImport import;

	static bool add_custom_resource_format_loader(const String &p_script_path);
	static void add_custom_loaders();
	static void remove_custom_loaders();

	static void set_create_missing_resources_if_class_unavailable(bool p_enable);
	_FORCE_INLINE_ static bool is_creating_missing_resources_if_class_unavailable_enabled() { return create_missing_resources_if_class_unavailable; }

	static Ref<Resource> ensure_resource_ref_override_for_outer_load(const String &p_path, const String &p_res_type);
	static Ref<Resource> get_resource_ref_override(const String &p_path);

	static bool is_cleaning_tasks();

	static Vector<String> list_directory(const String &p_directory);

	static void initialize();
	static void finalize();
};
