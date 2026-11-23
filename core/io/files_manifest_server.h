/**************************************************************************/
/*  files_manifest_server.h                                               */
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
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

// Keeps the file manifests of a game under names: registers, edits, validates and saves them,
// caches their file lists and reports files that appear in or disappear from their folders.
// Watching polls the folders on the main thread (no threads, no OS watchers), so it works the
// same on every platform including the Web export. Polling runs automatically on the
// `process_frame` signal of the main loop (a `SceneTree`), or explicitly through `poll()`.
class FilesManifestServer : public Object {
	GDCLASS(FilesManifestServer, Object);

	static FilesManifestServer *singleton;

	struct Entry {
		Ref<FilesManifest> manifest;
		// Cached result of `FilesManifest::get_paths()`.
		PackedStringArray paths;
		HashSet<String> path_set;
		Error paths_error = OK;
		// False until the first scan and after every change of the manifest.
		bool cached = false;
		// Incremented on every change, so a scan that raced with a change is not stored.
		uint64_t version = 0;
		bool watching = false;
		// Target of `save_manifest()` without a path: the last explicit save path.
		String save_path;
	};

	mutable Mutex mutex;
	HashMap<String, Entry> manifests;
	int watching_count = 0;
	double watch_interval = 0.5;
	uint64_t last_poll_msec = 0;
	mutable bool autoload_done = false;
	mutable bool autoload_running = false;

	// Registers the manifests of `filesystem/manifests/autoload` on first use (the resource formats exist by then).
	void _ensure_autoload() const;
	void _ensure_process_connection();
	void _on_process_frame();
	void _on_manifest_changed(const String &p_name);
	Ref<FilesManifest> _get_manifest(const String &p_name) const;
	// Scans the manifest and stores the result. Emits `file_added` and `file_removed` if `p_report` is
	// set and the previous result was still valid. `p_quiet_repeated_errors` hides the error output of
	// a scan whose previous scan failed with the same error (polling a missing folder).
	Error _scan(const String &p_name, bool p_report, bool p_quiet_repeated_errors = false);
	void _emit(const StringName &p_signal, const String &p_name);
	void _emit(const StringName &p_signal, const String &p_name, const String &p_path);

	static Error _add_path(PackedStringArray &r_list, const String &p_path);
	static Error _remove_path(PackedStringArray &r_list, const String &p_path);
	static bool _is_known_extension(const String &p_extension, const List<String> &p_known_extensions);

protected:
	static void _bind_methods();

public:
	static FilesManifestServer *get_singleton();

	// Registration.
	Error register_manifest(const String &p_name, const Ref<FilesManifest> &p_manifest);
	Error register_manifest_file(const String &p_name, const String &p_path);
	Error unregister_manifest(const String &p_name);
	bool has_manifest(const String &p_name) const;
	Ref<FilesManifest> get_manifest(const String &p_name) const;
	PackedStringArray get_manifest_names() const;

	// Edits. Paths are stored in their canonical form (`FilesManifest::normalize_path()`).
	Error add_folder(const String &p_name, const String &p_path);
	Error remove_folder(const String &p_name, const String &p_path);
	Error add_file(const String &p_name, const String &p_path);
	Error remove_file(const String &p_name, const String &p_path);
	Error add_extension(const String &p_name, const String &p_extension);
	Error remove_extension(const String &p_name, const String &p_extension);

	// File lists.
	PackedStringArray get_paths(const String &p_name);
	Error refresh(const String &p_name);

	// Checks and maintenance.
	Dictionary validate(const String &p_name) const;
	Error create_missing_folders(const String &p_name);
	Error save_manifest(const String &p_name, const String &p_path = String());

	// Watching.
	Error set_watching(const String &p_name, bool p_enabled);
	bool is_watching(const String &p_name) const;
	void set_watch_interval(double p_seconds);
	double get_watch_interval() const;
	void poll();

	FilesManifestServer();
	~FilesManifestServer();
};
