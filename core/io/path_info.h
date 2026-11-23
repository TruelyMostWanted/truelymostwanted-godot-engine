/**************************************************************************/
/*  path_info.h                                                           */
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

#include "core/object/ref_counted.h"

// The result of `PathPrefixServer::resolve()` and `resolve_safe()`.
// All forms and facts of one path at once. Read-only after creation.
class PathInfo : public RefCounted {
	GDCLASS(PathInfo, RefCounted);

	friend class PathPrefixServer;

	bool valid = false;
	Error error = ERR_UNCONFIGURED;
	String error_message;

	String source_path;
	String local_path;
	String global_path;
	String prefix;
	String relative_path;

	bool locked = false;
	bool read_only = false;
	bool available = false;

	void _set_error(Error p_error, const String &p_message);

protected:
	static void _bind_methods();

public:
	bool is_valid() const { return valid; }
	Error get_error() const { return error; }
	String get_error_message() const { return error_message; }

	String get_source_path() const { return source_path; }
	String get_local_path() const { return local_path; }
	String get_global_path() const { return global_path; }
	String get_prefix() const { return prefix; }
	String get_relative_path() const { return relative_path; }
	String get_file_name() const;
	String get_extension() const;
	String get_base_dir() const;

	bool is_locked() const { return locked; }
	bool is_read_only() const { return read_only; }
	bool is_available() const { return available; }

	bool exists() const;
	bool is_dir() const;
};
