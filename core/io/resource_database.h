/**************************************************************************/
/*  resource_database.h                                                   */
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

#include "core/io/files_manifest.h"
#include "core/io/preload_list.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"
#include "core/os/rw_lock.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/pair.h"
#include "core/variant/typed_array.h"

// Keeps resources in memory with strong references and finds them again by path, key, type
// or pattern. Loaded resources are stored under their resource path, so they also stay in
// `ResourceCache` and `ResourceLoader.load()` of that path returns the stored instance.
// Thread-safe: every method can be called from any thread. Signals are emitted on the main thread.
class ResourceDatabase : public Object {
	GDCLASS(ResourceDatabase, Object);

	static ResourceDatabase *singleton;

	struct Entry {
		Ref<Resource> resource;
		// Other keys that lead to this entry (list keys, requested paths that differ from the resource path).
		Vector<String> aliases;
	};

	struct ManifestSource {
		Ref<FilesManifest> manifest;
		// Primary keys of the entries that were loaded from this manifest.
		HashSet<String> keys;
	};

	// One preload of paths, synchronous or on a worker thread.
	struct Job {
		uint64_t id = 0;
		Ref<PreloadList> list;
		// Requested path and type hint, in load order.
		Vector<Pair<String, String>> paths;
		// Manifest instance ID per path index, 0 for list paths.
		Vector<uint64_t> path_manifests;
		// Alias -> target (path or key), applied after all paths are loaded.
		Vector<Pair<String, String>> keys;
		Error error = OK;
		bool threaded = false;
		WorkerThreadPool::TaskID task_id = WorkerThreadPool::INVALID_TASK_ID;
	};

	mutable RWLock lock;
	HashMap<String, Entry> entries;
	HashMap<String, String> aliases;
	HashMap<uint64_t, ManifestSource> manifest_sources;

	Mutex jobs_mutex;
	HashMap<uint64_t, Job *> jobs;
	uint64_t next_job_id = 1;
	bool finished = false;

	static String _normalize_key(const String &p_key);
	static String _normalize_path(const String &p_path, Error *r_error = nullptr);
	static bool _matches_pattern(const String &p_key, const String &p_pattern_lower);
	static bool _matches_type(const Ref<Resource> &p_resource, const StringName &p_type);
	static uint64_t _estimate_variant_size(const Variant &p_value, HashSet<ObjectID> &r_visited, int p_depth);

	// Callers hold the lock.
	const String *_resolve(const String &p_key) const;
	bool _entry_matches(const String &p_key, const Entry &p_entry, const String &p_pattern_lower, const StringName &p_type) const;
	void _add_alias(const String &p_alias, const String &p_primary);
	void _erase_entry(const String &p_primary);
	void _store(const String &p_key, const Ref<Resource> &p_resource);

	// Stores a loaded resource under its resource path; `p_requested` becomes an alias if it differs.
	String _store_loaded(const String &p_requested, const Ref<Resource> &p_resource, uint64_t p_manifest_id);
	void _apply_keys(const Vector<Pair<String, String>> &p_keys);
	void _collect_manifest(const Ref<FilesManifest> &p_manifest, Job *r_job);

	void _run_job(Job *p_job);
	void _run_job_task(Job *p_job);
	void _job_done(uint64_t p_job_id);
	void _emit_preload_finished(const Ref<PreloadList> &p_list, Error p_error);

protected:
	static void _bind_methods();

public:
	static ResourceDatabase *get_singleton();

	// Loads everything a list names. With `p_use_threads`, the paths load on worker threads and the
	// call returns at once; without, it returns when all paths are loaded. `preload_finished` follows
	// on the main thread in both cases. Returns the first error found while starting (or loading).
	Error preload_list(const Ref<PreloadList> &p_list, bool p_use_threads = true);
	// Loads one path now and stores it; `p_key` becomes an extra key for it.
	Error preload_path(const String &p_path, const String &p_key = String());
	// Brings the entries of a manifest up to date: loads paths it lists now, removes entries it no
	// longer lists. For servers that watch manifests (call from any thread, runs synchronously).
	Error refresh_manifest(const Ref<FilesManifest> &p_manifest);
	// Removes all entries loaded from a manifest and forgets the manifest. Returns the number removed.
	int remove_manifest(const Ref<FilesManifest> &p_manifest);

	Error insert(const String &p_key, const Ref<Resource> &p_resource);
	bool insert_if_absent(const String &p_key, const Ref<Resource> &p_resource);
	int remove(const String &p_pattern);
	void clear();

	Ref<Resource> get_resource(const String &p_key_or_path) const;
	bool has_resource(const String &p_key_or_path) const;
	int count() const;
	TypedArray<Resource> select(const String &p_pattern = String(), const StringName &p_type = StringName()) const;
	PackedStringArray select_keys(const String &p_pattern = String(), const StringName &p_type = StringName()) const;

	// Estimated CPU-side bytes of the stored resources (stored properties, each resource counted once).
	int64_t get_memory_usage() const;
	bool is_preloading() const;

	// Preloads the lists of `filesystem/resources/preload_lists` synchronously. Called once at startup
	// (not in the editor).
	void preload_project_lists();
	// Waits for running preloads and drops every entry. Called before the servers shut down, so that
	// stored resources are freed while their servers still exist.
	void finish();

	ResourceDatabase();
	~ResourceDatabase();
};
