/**************************************************************************/
/*  path_info.cpp                                                         */
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

#include "path_info.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"

void PathInfo::_set_error(Error p_error, const String &p_message) {
	valid = false;
	error = p_error;
	error_message = p_message;
}

String PathInfo::get_file_name() const {
	if (relative_path.is_empty()) {
		return local_path.is_empty() ? global_path.get_file() : String();
	}
	return relative_path.get_file();
}

String PathInfo::get_extension() const {
	return get_file_name().get_extension();
}

String PathInfo::get_base_dir() const {
	const String &path = local_path.is_empty() ? global_path : local_path;
	if (!prefix.is_empty() && relative_path.find_char('/') == -1) {
		// Directly below the prefix root: `user://a.json` -> `user://`.
		return prefix;
	}
	return path.get_base_dir();
}

bool PathInfo::exists() const {
	if (!valid) {
		return false;
	}
	const String &path = local_path.is_empty() ? global_path : local_path;
	if (path.is_empty()) {
		return false;
	}
	return FileAccess::exists(path) || DirAccess::exists(path);
}

bool PathInfo::is_dir() const {
	if (!valid) {
		return false;
	}
	const String &path = local_path.is_empty() ? global_path : local_path;
	if (path.is_empty()) {
		return false;
	}
	return DirAccess::exists(path);
}

void PathInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &PathInfo::is_valid);
	ClassDB::bind_method(D_METHOD("get_error"), &PathInfo::get_error);
	ClassDB::bind_method(D_METHOD("get_error_message"), &PathInfo::get_error_message);
	ClassDB::bind_method(D_METHOD("get_source_path"), &PathInfo::get_source_path);
	ClassDB::bind_method(D_METHOD("get_local_path"), &PathInfo::get_local_path);
	ClassDB::bind_method(D_METHOD("get_global_path"), &PathInfo::get_global_path);
	ClassDB::bind_method(D_METHOD("get_prefix"), &PathInfo::get_prefix);
	ClassDB::bind_method(D_METHOD("get_relative_path"), &PathInfo::get_relative_path);
	ClassDB::bind_method(D_METHOD("get_file_name"), &PathInfo::get_file_name);
	ClassDB::bind_method(D_METHOD("get_extension"), &PathInfo::get_extension);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &PathInfo::get_base_dir);
	ClassDB::bind_method(D_METHOD("is_locked"), &PathInfo::is_locked);
	ClassDB::bind_method(D_METHOD("is_read_only"), &PathInfo::is_read_only);
	ClassDB::bind_method(D_METHOD("is_available"), &PathInfo::is_available);
	ClassDB::bind_method(D_METHOD("exists"), &PathInfo::exists);
	ClassDB::bind_method(D_METHOD("is_dir"), &PathInfo::is_dir);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid"), "", "is_valid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_message"), "", "get_error_message");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path"), "", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "local_path"), "", "get_local_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "global_path"), "", "get_global_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "prefix"), "", "get_prefix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "relative_path"), "", "get_relative_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_name"), "", "get_file_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "extension"), "", "get_extension");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir"), "", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "locked"), "", "is_locked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "read_only"), "", "is_read_only");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "available"), "", "is_available");
}
