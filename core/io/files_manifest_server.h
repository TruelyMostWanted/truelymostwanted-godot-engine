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
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

// Keeps file manifests under names: registers, edits, validates and saves them, caches their
// file lists and reports files that appear in or disappear from watched folders.
// Watching polls the folders on the main thread (no threads, no OS watchers), so it works the
// same on every platform including the Web export. Use the server from the main thread.
class FilesManifestServer : public Object {
	GDCLASS(FilesManifestServer, Object);

	static FilesManifestServer *singleton;

	struct Entry {
		Ref<FilesManifest> manifest;
		PackedStringArray paths; // Cached result of `FilesManifest::get_paths()`.
		HashSet<String> path_set;
		bool cached = false; // False until the first scan and after every change.
		bool watching = false;
		String save_path; // Last explicit target of `save_manifest()`.
	};

	HashMap<String, Entry> manifests;
	double watch_interval = 0.5;
	uint64_t last_poll_msec = 0;
	bool autoload_done = false;

	void _ensure_autoload();
	void _ensure_process_connection();
	void _on_process_frame();
	Entry *_get_entry(const String &p_name);
	void _changed(const String &p_name, Entry &r_entry);
	void _scan(const String &p_name, Entry &r_entry, bool p_report);
	Error _edit_list(const String &p_name, bool p_folders, bool p_add, const String &p_path);
	Error _edit_extension(const String &p_name, bool p_add, const String &p_extension);

protected:
	static void _bind_methods();

public:
	static FilesManifestServer *get_singleton();

	Error register_manifest(const String &p_name, const Ref<FilesManifest> &p_manifest);
	Error register_manifest_file(const String &p_name, const String &p_path);
	Error unregister_manifest(const String &p_name);
	bool has_manifest(const String &p_name);
	Ref<FilesManifest> get_manifest(const String &p_name);
	PackedStringArray get_manifest_names();

	Error add_folder(const String &p_name, const String &p_path);
	Error remove_folder(const String &p_name, const String &p_path);
	Error add_file(const String &p_name, const String &p_path);
	Error remove_file(const String &p_name, const String &p_path);
	Error add_extension(const String &p_name, const String &p_extension);
	Error remove_extension(const String &p_name, const String &p_extension);

	PackedStringArray get_paths(const String &p_name);
	Error refresh(const String &p_name);

	Dictionary validate(const String &p_name);
	Error create_missing_folders(const String &p_name);
	Error save_manifest(const String &p_name, const String &p_path = String());

	Error set_watching(const String &p_name, bool p_enabled);
	bool is_watching(const String &p_name);
	void set_watch_interval(double p_seconds);
	double get_watch_interval() const;
	void poll();

	FilesManifestServer();
	~FilesManifestServer();
};
