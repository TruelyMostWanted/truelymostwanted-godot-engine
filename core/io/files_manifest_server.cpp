/**************************************************************************/
/*  files_manifest_server.cpp                                             */
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

#include "files_manifest_server.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "core/os/time.h"

FilesManifestServer *FilesManifestServer::singleton = nullptr;

FilesManifestServer *FilesManifestServer::get_singleton() {
	return singleton;
}

void FilesManifestServer::_ensure_autoload() {
	if (autoload_done) {
		return;
	}
	autoload_done = true;
	// `filesystem/manifests/autoload`: a Dictionary of name -> manifest path, registered on first use
	// (at that time all resource formats are registered).
	const Dictionary autoload = GLOBAL_GET("filesystem/manifests/autoload");
	for (const KeyValue<Variant, Variant> &kv : autoload) {
		register_manifest_file(kv.key, kv.value);
	}
}

FilesManifestServer::Entry *FilesManifestServer::_get_entry(const String &p_name) {
	_ensure_autoload();
	Entry *entry = manifests.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(entry, nullptr, vformat("No file manifest is registered as \"%s\".", p_name));
	return entry;
}

void FilesManifestServer::_changed(const String &p_name, Entry &r_entry) {
	r_entry.cached = false;
	if (r_entry.watching) {
		// The new file list becomes the base for watching; changes of the manifest itself are not file events.
		_scan(p_name, r_entry, false);
	}
	emit_signal(SNAME("manifest_changed"), p_name);
}

void FilesManifestServer::_scan(const String &p_name, Entry &r_entry, bool p_report) {
	Error err = OK;
	const PackedStringArray paths = r_entry.manifest->get_paths(&err);
	HashSet<String> path_set;
	for (const String &path : paths) {
		path_set.insert(path);
	}
	const bool report = p_report && r_entry.cached;
	HashSet<String> old_set = std::move(r_entry.path_set);
	r_entry.paths = paths;
	r_entry.path_set = std::move(path_set);
	r_entry.cached = true;
	if (!report) {
		return;
	}
	for (const String &path : paths) {
		if (!old_set.has(path)) {
			emit_signal(SNAME("file_added"), p_name, path);
		}
	}
	for (const String &path : old_set) {
		if (!r_entry.path_set.has(path)) {
			emit_signal(SNAME("file_removed"), p_name, path);
		}
	}
}

Error FilesManifestServer::register_manifest(const String &p_name, const Ref<FilesManifest> &p_manifest) {
	ERR_FAIL_COND_V_MSG(p_name.is_empty(), ERR_INVALID_PARAMETER, "The name of a file manifest can't be empty.");
	ERR_FAIL_COND_V_MSG(p_manifest.is_null(), ERR_INVALID_PARAMETER, "The file manifest is null.");
	Entry entry;
	entry.manifest = p_manifest;
	entry.save_path = p_manifest->get_path();
	if (Entry *old = manifests.getptr(p_name)) {
		entry.watching = old->watching;
	}
	manifests[p_name] = entry;
	_changed(p_name, manifests[p_name]);
	return OK;
}

Error FilesManifestServer::register_manifest_file(const String &p_name, const String &p_path) {
	const Ref<FilesManifest> manifest = ResourceLoader::load(p_path, "FilesManifest");
	ERR_FAIL_COND_V_MSG(manifest.is_null(), ERR_FILE_CANT_OPEN, vformat("Can't load the file manifest \"%s\" from \"%s\".", p_name, p_path));
	return register_manifest(p_name, manifest);
}

Error FilesManifestServer::unregister_manifest(const String &p_name) {
	ERR_FAIL_NULL_V(_get_entry(p_name), ERR_DOES_NOT_EXIST);
	manifests.erase(p_name);
	return OK;
}

bool FilesManifestServer::has_manifest(const String &p_name) {
	_ensure_autoload();
	return manifests.has(p_name);
}

Ref<FilesManifest> FilesManifestServer::get_manifest(const String &p_name) {
	Entry *entry = _get_entry(p_name);
	return entry ? entry->manifest : Ref<FilesManifest>();
}

PackedStringArray FilesManifestServer::get_manifest_names() {
	_ensure_autoload();
	PackedStringArray names;
	for (const KeyValue<String, Entry> &kv : manifests) {
		names.push_back(kv.key);
	}
	names.sort();
	return names;
}

Error FilesManifestServer::_edit_list(const String &p_name, bool p_folders, bool p_add, const String &p_path) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	Error err = OK;
	const String path = FilesManifest::normalize_path(p_path, nullptr, &err);
	ERR_FAIL_COND_V_MSG(err != OK || path.is_empty(), ERR_FILE_BAD_PATH, vformat("Invalid path \"%s\".", p_path));
	PackedStringArray list = p_folders ? entry->manifest->get_folder_paths() : entry->manifest->get_file_paths();
	int index = -1;
	for (int i = 0; i < list.size(); i++) {
		if (FilesManifest::normalize_path(list[i]) == path) {
			index = i;
			break;
		}
	}
	if (p_add == (index >= 0)) {
		return p_add ? ERR_ALREADY_EXISTS : ERR_DOES_NOT_EXIST;
	}
	if (p_add) {
		list.push_back(path);
	} else {
		list.remove_at(index);
	}
	if (p_folders) {
		entry->manifest->set_folder_paths(list);
	} else {
		entry->manifest->set_file_paths(list);
	}
	_changed(p_name, *entry);
	return OK;
}

Error FilesManifestServer::_edit_extension(const String &p_name, bool p_add, const String &p_extension) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	const String extension = FilesManifest::normalize_extension(p_extension);
	ERR_FAIL_COND_V_MSG(extension.is_empty(), ERR_INVALID_PARAMETER, "The extension is empty.");
	PackedStringArray list = entry->manifest->get_extensions();
	const int index = list.find(extension);
	if (p_add == (index >= 0)) {
		return p_add ? ERR_ALREADY_EXISTS : ERR_DOES_NOT_EXIST;
	}
	if (p_add) {
		list.push_back(extension);
	} else {
		list.remove_at(index);
	}
	entry->manifest->set_extensions(list);
	_changed(p_name, *entry);
	return OK;
}

Error FilesManifestServer::add_folder(const String &p_name, const String &p_path) {
	return _edit_list(p_name, true, true, p_path);
}

Error FilesManifestServer::remove_folder(const String &p_name, const String &p_path) {
	return _edit_list(p_name, true, false, p_path);
}

Error FilesManifestServer::add_file(const String &p_name, const String &p_path) {
	return _edit_list(p_name, false, true, p_path);
}

Error FilesManifestServer::remove_file(const String &p_name, const String &p_path) {
	return _edit_list(p_name, false, false, p_path);
}

Error FilesManifestServer::add_extension(const String &p_name, const String &p_extension) {
	return _edit_extension(p_name, true, p_extension);
}

Error FilesManifestServer::remove_extension(const String &p_name, const String &p_extension) {
	return _edit_extension(p_name, false, p_extension);
}

PackedStringArray FilesManifestServer::get_paths(const String &p_name) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, PackedStringArray());
	if (!entry->cached) {
		_scan(p_name, *entry, false);
	}
	return entry->paths;
}

Error FilesManifestServer::refresh(const String &p_name) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	_scan(p_name, *entry, entry->watching);
	return OK;
}

Dictionary FilesManifestServer::validate(const String &p_name) {
	Dictionary result;
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, result);
	PackedStringArray missing_folders;
	for (const String &folder : entry->manifest->get_folder_paths()) {
		if (!DirAccess::dir_exists_absolute(FilesManifest::normalize_path(folder))) {
			missing_folders.push_back(folder);
		}
	}
	PackedStringArray missing_files;
	for (const String &file : entry->manifest->get_file_paths()) {
		if (!FileAccess::exists(FilesManifest::normalize_path(file))) {
			missing_files.push_back(file);
		}
	}
	List<String> known;
	ResourceLoader::get_recognized_extensions_for_type("", &known);
	PackedStringArray unknown_extensions;
	for (const String &extension : entry->manifest->get_extensions()) {
		// Multi-part extensions (".tres.json") are known if their last part is.
		if (!known.find(extension.get_extension().to_lower())) {
			unknown_extensions.push_back(extension);
		}
	}
	result["missing_folders"] = missing_folders;
	result["missing_files"] = missing_files;
	result["unknown_extensions"] = unknown_extensions;
	result["valid"] = missing_folders.is_empty() && missing_files.is_empty() && unknown_extensions.is_empty();
	return result;
}

Error FilesManifestServer::create_missing_folders(const String &p_name) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	Error result = OK;
	for (const String &folder : entry->manifest->get_folder_paths()) {
		bool read_only = false;
		const String path = FilesManifest::normalize_path(folder, &read_only);
		if (DirAccess::dir_exists_absolute(path)) {
			continue;
		}
		if (read_only) {
			ERR_PRINT(vformat("Can't create \"%s\": the path is read-only.", folder));
			result = result == OK ? ERR_FILE_NO_PERMISSION : result;
			continue;
		}
		const Error err = DirAccess::make_dir_recursive_absolute(path);
		if (err != OK && result == OK) {
			result = err;
		}
	}
	_changed(p_name, *entry);
	return result;
}

Error FilesManifestServer::save_manifest(const String &p_name, const String &p_path) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	const String path = p_path.is_empty() ? entry->save_path : p_path;
	ERR_FAIL_COND_V_MSG(path.is_empty(), ERR_FILE_BAD_PATH, vformat("The file manifest \"%s\" has no path yet: pass one to save_manifest().", p_name));
	const Error err = ResourceSaver::save(entry->manifest, path);
	if (err == OK) {
		entry->save_path = path;
	}
	return err;
}

void FilesManifestServer::_ensure_process_connection() {
	MainLoop *main_loop = OS::get_singleton()->get_main_loop();
	const Callable callable = callable_mp(this, &FilesManifestServer::_on_process_frame);
	// The SceneTree emits `process_frame`; other main loops call poll() themselves.
	if (main_loop && main_loop->has_signal(SNAME("process_frame")) && !main_loop->is_connected(SNAME("process_frame"), callable)) {
		main_loop->connect(SNAME("process_frame"), callable);
	}
}

void FilesManifestServer::_on_process_frame() {
	const uint64_t now = Time::get_singleton()->get_ticks_msec();
	if (now - last_poll_msec >= uint64_t(watch_interval * 1000.0)) {
		poll();
	}
}

Error FilesManifestServer::set_watching(const String &p_name, bool p_enabled) {
	Entry *entry = _get_entry(p_name);
	ERR_FAIL_NULL_V(entry, ERR_DOES_NOT_EXIST);
	entry->watching = p_enabled;
	if (p_enabled) {
		if (!entry->cached) {
			_scan(p_name, *entry, false);
		}
		_ensure_process_connection();
	}
	return OK;
}

bool FilesManifestServer::is_watching(const String &p_name) {
	Entry *entry = _get_entry(p_name);
	return entry && entry->watching;
}

void FilesManifestServer::set_watch_interval(double p_seconds) {
	watch_interval = MAX(p_seconds, 0.0);
}

double FilesManifestServer::get_watch_interval() const {
	return watch_interval;
}

void FilesManifestServer::poll() {
	last_poll_msec = Time::get_singleton()->get_ticks_msec();
	// Signal handlers may edit the server: scan a copy of the names.
	LocalVector<String> names;
	for (const KeyValue<String, Entry> &kv : manifests) {
		if (kv.value.watching) {
			names.push_back(kv.key);
		}
	}
	for (const String &name : names) {
		if (Entry *entry = manifests.getptr(name)) {
			_scan(name, *entry, true);
		}
	}
}

void FilesManifestServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_manifest", "name", "manifest"), &FilesManifestServer::register_manifest);
	ClassDB::bind_method(D_METHOD("register_manifest_file", "name", "path"), &FilesManifestServer::register_manifest_file);
	ClassDB::bind_method(D_METHOD("unregister_manifest", "name"), &FilesManifestServer::unregister_manifest);
	ClassDB::bind_method(D_METHOD("has_manifest", "name"), &FilesManifestServer::has_manifest);
	ClassDB::bind_method(D_METHOD("get_manifest", "name"), &FilesManifestServer::get_manifest);
	ClassDB::bind_method(D_METHOD("get_manifest_names"), &FilesManifestServer::get_manifest_names);
	ClassDB::bind_method(D_METHOD("add_folder", "name", "path"), &FilesManifestServer::add_folder);
	ClassDB::bind_method(D_METHOD("remove_folder", "name", "path"), &FilesManifestServer::remove_folder);
	ClassDB::bind_method(D_METHOD("add_file", "name", "path"), &FilesManifestServer::add_file);
	ClassDB::bind_method(D_METHOD("remove_file", "name", "path"), &FilesManifestServer::remove_file);
	ClassDB::bind_method(D_METHOD("add_extension", "name", "extension"), &FilesManifestServer::add_extension);
	ClassDB::bind_method(D_METHOD("remove_extension", "name", "extension"), &FilesManifestServer::remove_extension);
	ClassDB::bind_method(D_METHOD("get_paths", "name"), &FilesManifestServer::get_paths);
	ClassDB::bind_method(D_METHOD("refresh", "name"), &FilesManifestServer::refresh);
	ClassDB::bind_method(D_METHOD("validate", "name"), &FilesManifestServer::validate);
	ClassDB::bind_method(D_METHOD("create_missing_folders", "name"), &FilesManifestServer::create_missing_folders);
	ClassDB::bind_method(D_METHOD("save_manifest", "name", "path"), &FilesManifestServer::save_manifest, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("set_watching", "name", "enabled"), &FilesManifestServer::set_watching);
	ClassDB::bind_method(D_METHOD("is_watching", "name"), &FilesManifestServer::is_watching);
	ClassDB::bind_method(D_METHOD("set_watch_interval", "seconds"), &FilesManifestServer::set_watch_interval);
	ClassDB::bind_method(D_METHOD("get_watch_interval"), &FilesManifestServer::get_watch_interval);
	ClassDB::bind_method(D_METHOD("poll"), &FilesManifestServer::poll);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "watch_interval", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater,suffix:s"), "set_watch_interval", "get_watch_interval");

	ADD_SIGNAL(MethodInfo("file_added", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("file_removed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("manifest_changed", PropertyInfo(Variant::STRING, "name")));
}

FilesManifestServer::FilesManifestServer() {
	singleton = this;
}

FilesManifestServer::~FilesManifestServer() {
	singleton = nullptr;
}
