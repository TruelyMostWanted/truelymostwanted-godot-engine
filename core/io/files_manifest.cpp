/**************************************************************************/
/*  files_manifest.cpp                                                    */
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

#include "files_manifest.h"

#include "core/io/dir_access.h"
#include "core/io/path_info.h"
#include "core/io/path_prefix_server.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

String FilesManifest::normalize_extension(const String &p_extension) {
	String ext = p_extension.strip_edges().to_lower();
	if (ext.begins_with("*")) {
		ext = ext.substr(1);
	}
	if (ext.is_empty() || ext == ".") {
		return String();
	}
	if (!ext.begins_with(".")) {
		ext = "." + ext;
	}
	return ext;
}

String FilesManifest::normalize_path(const String &p_path, bool *r_read_only, Error *r_error) {
	if (r_read_only) {
		*r_read_only = false;
	}
	if (r_error) {
		*r_error = OK;
	}

	const String path = p_path.strip_edges().replace_char('\\', '/');
	if (path.is_empty()) {
		if (r_error) {
			*r_error = ERR_INVALID_PARAMETER;
		}
		return String();
	}

	PathPrefixServer *server = PathPrefixServer::get_singleton();
	if (!server) {
		return path.is_relative_path() ? ("res://" + path).simplify_path() : path;
	}

	const Ref<PathInfo> info = server->resolve(path);
	if (!info->is_valid()) {
		if (r_error) {
			*r_error = info->get_error();
		}
		ERR_PRINT(vformat("FilesManifest: cannot resolve the path '%s': %s", p_path, info->get_error_message()));
		return String();
	}
	if (r_read_only) {
		*r_read_only = info->is_read_only();
	}

	String ret = info->get_local_path().is_empty() ? info->get_global_path() : info->get_local_path();
	if (ret.length() > 1 && ret.ends_with("/") && !ret.ends_with("://")) {
		ret = ret.substr(0, ret.length() - 1);
	}
	return ret;
}

Error FilesManifest::load_threaded_request_paths(const PackedStringArray &p_paths, const String &p_type_hint, bool p_use_sub_threads, ResourceLoaderConstants::CacheMode p_cache_mode) {
	Error first_error = OK;
	for (const String &path : p_paths) {
		const Error err = ResourceLoader::load_threaded_request(path, p_type_hint, p_use_sub_threads, p_cache_mode);
		if (err != OK && first_error == OK) {
			first_error = err;
		}
	}
	return first_error;
}

void FilesManifest::set_type_hint(const StringName &p_type_hint) {
	type_hint = p_type_hint;
	emit_changed();
}

StringName FilesManifest::get_type_hint() const {
	return type_hint;
}

void FilesManifest::set_extensions(const PackedStringArray &p_extensions) {
	extensions.clear();
	for (const String &extension : p_extensions) {
		const String ext = normalize_extension(extension);
		if (!ext.is_empty() && !extensions.has(ext)) {
			extensions.push_back(ext);
		}
	}
	emit_changed();
}

PackedStringArray FilesManifest::get_extensions() const {
	return extensions;
}

void FilesManifest::set_folder_paths(const PackedStringArray &p_folder_paths) {
	folder_paths = p_folder_paths;
	emit_changed();
}

PackedStringArray FilesManifest::get_folder_paths() const {
	return folder_paths;
}

void FilesManifest::set_file_paths(const PackedStringArray &p_file_paths) {
	file_paths = p_file_paths;
	emit_changed();
}

PackedStringArray FilesManifest::get_file_paths() const {
	return file_paths;
}

void FilesManifest::set_recursive(bool p_recursive) {
	recursive = p_recursive;
	emit_changed();
}

bool FilesManifest::is_recursive() const {
	return recursive;
}

void FilesManifest::set_create_non_existing_folders(bool p_create) {
	create_non_existing_folders = p_create;
	emit_changed();
}

bool FilesManifest::is_create_non_existing_folders() const {
	return create_non_existing_folders;
}

bool FilesManifest::matches_extension(const String &p_path) const {
	if (extensions.is_empty()) {
		return true;
	}
	const String file = p_path.get_file().to_lower();
	for (const String &ext : extensions) {
		if (file.ends_with(ext)) {
			return true;
		}
	}
	return false;
}

String FilesManifest::_get_load_type_hint() const {
	// Script class names are not known to the loaders: load those without a hint.
	if (type_hint != StringName() && ClassDB::class_exists(type_hint)) {
		return type_hint;
	}
	return String();
}

bool FilesManifest::_type_matches(const String &p_path) const {
	const String hint = _get_load_type_hint();
	if (hint.is_empty()) {
		return true;
	}
	// In exported builds, text resources may be converted to binary and reached through a `.remap` file.
	const String type = ResourceLoader::get_resource_type(ResourceLoader::path_remap(p_path));
	// Files whose type is only known after loading are kept.
	return type.is_empty() || ClassDB::is_parent_class(type, hint);
}

void FilesManifest::_collect_folder(const String &p_folder, int p_depth, HashSet<String> &r_seen, PackedStringArray &r_paths) const {
	// `list_directory()` hides `.import`, `.remap` and `.uid` files and returns the original names,
	// so this works the same in the editor and in exported builds (files inside the PCK).
	const Vector<String> entries = ResourceLoader::list_directory(p_folder);
	Vector<String> subfolders;
	for (const String &entry : entries) {
		if (entry.ends_with("/")) {
			if (recursive) {
				subfolders.push_back(p_folder.path_join(entry.substr(0, entry.length() - 1)));
			}
			continue;
		}
		const String path = p_folder.path_join(entry);
		if (r_seen.has(path) || !matches_extension(path) || !_type_matches(path)) {
			continue;
		}
		r_seen.insert(path);
		r_paths.push_back(path);
	}

	for (const String &subfolder : subfolders) {
		if (p_depth + 1 > MAX_FOLDER_DEPTH) {
			WARN_PRINT(vformat("FilesManifest: '%s' is deeper than %d folders and is skipped.", subfolder, MAX_FOLDER_DEPTH));
			continue;
		}
		_collect_folder(subfolder, p_depth + 1, r_seen, r_paths);
	}
}

PackedStringArray FilesManifest::get_paths(Error *r_error) const {
	Error first_error = OK;
	HashSet<String> seen;
	PackedStringArray paths;

	for (const String &folder_path : folder_paths) {
		if (folder_path.strip_edges().is_empty()) {
			continue;
		}
		bool read_only = false;
		Error err = OK;
		const String folder = normalize_path(folder_path, &read_only, &err);
		if (folder.is_empty()) {
			if (first_error == OK) {
				first_error = err != OK ? err : ERR_FILE_BAD_PATH;
			}
			continue;
		}

		if (!DirAccess::dir_exists_absolute(folder)) {
			if (!create_non_existing_folders) {
				ERR_PRINT(vformat("FilesManifest: the folder '%s' does not exist.", folder));
				if (first_error == OK) {
					first_error = ERR_FILE_NOT_FOUND;
				}
			} else if (read_only) {
				ERR_PRINT(vformat("FilesManifest: the folder '%s' does not exist and cannot be created, because its path is read-only.", folder));
				if (first_error == OK) {
					first_error = ERR_FILE_NO_PERMISSION;
				}
			} else if (DirAccess::make_dir_recursive_absolute(folder) != OK) {
				ERR_PRINT(vformat("FilesManifest: the folder '%s' does not exist and could not be created.", folder));
				if (first_error == OK) {
					first_error = ERR_CANT_CREATE;
				}
			}
			// A new folder is empty.
			continue;
		}

		_collect_folder(folder, 0, seen, paths);
	}

	for (const String &file_path : file_paths) {
		if (file_path.strip_edges().is_empty()) {
			continue;
		}
		Error err = OK;
		const String path = normalize_path(file_path, nullptr, &err);
		if (path.is_empty()) {
			if (first_error == OK) {
				first_error = err != OK ? err : ERR_FILE_BAD_PATH;
			}
			continue;
		}
		if (seen.has(path)) {
			continue;
		}
		if (!ResourceLoader::exists(path)) {
			ERR_PRINT(vformat("FilesManifest: the file '%s' does not exist or is not a resource.", path));
			if (first_error == OK) {
				first_error = ERR_FILE_NOT_FOUND;
			}
			continue;
		}
		seen.insert(path);
		paths.push_back(path);
	}

	if (r_error) {
		*r_error = first_error;
	}
	return paths;
}

Dictionary FilesManifest::load_resources(ResourceLoaderConstants::CacheMode p_cache_mode, Error *r_error) const {
	Error first_error = OK;
	const PackedStringArray paths = get_paths(&first_error);
	const String hint = _get_load_type_hint();

	Dictionary resources;
	for (const String &path : paths) {
		Error err = OK;
		const Ref<Resource> res = ResourceLoader::load(path, hint, p_cache_mode, &err);
		if (res.is_valid()) {
			resources[path] = res;
		} else if (first_error == OK) {
			first_error = err != OK ? err : ERR_CANT_OPEN;
		}
	}

	if (r_error) {
		*r_error = first_error;
	}
	return resources;
}

Error FilesManifest::load_threaded_request_resources(bool p_use_sub_threads, ResourceLoaderConstants::CacheMode p_cache_mode) const {
	Error paths_error = OK;
	const PackedStringArray paths = get_paths(&paths_error);
	const Error request_error = load_threaded_request_paths(paths, _get_load_type_hint(), p_use_sub_threads, p_cache_mode);
	return paths_error != OK ? paths_error : request_error;
}

void FilesManifest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_type_hint", "type_hint"), &FilesManifest::set_type_hint);
	ClassDB::bind_method(D_METHOD("get_type_hint"), &FilesManifest::get_type_hint);
	ClassDB::bind_method(D_METHOD("set_extensions", "extensions"), &FilesManifest::set_extensions);
	ClassDB::bind_method(D_METHOD("get_extensions"), &FilesManifest::get_extensions);
	ClassDB::bind_method(D_METHOD("set_folder_paths", "folder_paths"), &FilesManifest::set_folder_paths);
	ClassDB::bind_method(D_METHOD("get_folder_paths"), &FilesManifest::get_folder_paths);
	ClassDB::bind_method(D_METHOD("set_file_paths", "file_paths"), &FilesManifest::set_file_paths);
	ClassDB::bind_method(D_METHOD("get_file_paths"), &FilesManifest::get_file_paths);
	ClassDB::bind_method(D_METHOD("set_recursive", "recursive"), &FilesManifest::set_recursive);
	ClassDB::bind_method(D_METHOD("is_recursive"), &FilesManifest::is_recursive);
	ClassDB::bind_method(D_METHOD("set_create_non_existing_folders", "create"), &FilesManifest::set_create_non_existing_folders);
	ClassDB::bind_method(D_METHOD("is_create_non_existing_folders"), &FilesManifest::is_create_non_existing_folders);
	ClassDB::bind_method(D_METHOD("matches_extension", "path"), &FilesManifest::matches_extension);
	ClassDB::bind_static_method("FilesManifest", D_METHOD("normalize_extension", "extension"), &FilesManifest::normalize_extension);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "type_hint"), "set_type_hint", "get_type_hint");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "extensions"), "set_extensions", "get_extensions");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "folder_paths", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:", Variant::STRING, PROPERTY_HINT_DIR)), "set_folder_paths", "get_folder_paths");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "file_paths", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:", Variant::STRING, PROPERTY_HINT_FILE)), "set_file_paths", "get_file_paths");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "recursive"), "set_recursive", "is_recursive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "create_non_existing_folders"), "set_create_non_existing_folders", "is_create_non_existing_folders");
}
