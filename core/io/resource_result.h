/**************************************************************************/
/*  resource_result.h                                                     */
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
#include "core/object/ref_counted.h"

// The result of `ResourceLoader.load_result()` and `load_threaded_get_result()`: the resource,
// or the error code and the message the loader printed. Read-only after creation.
//
// A class and not a struct: the binding layer (GDScript, C#, GDExtension) cannot expose C++
// structs, and a `RefCounted` is freed automatically like a value.
class ResourceLoadResult : public RefCounted {
	GDCLASS(ResourceLoadResult, RefCounted);

	Ref<Resource> resource;
	Error error = ERR_UNCONFIGURED;
	String error_message;
	String path;

protected:
	static void _bind_methods();

public:
	// `p_error == OK` with a null resource is turned into `ERR_CANT_ACQUIRE_RESOURCE`. The resource
	// is dropped when `p_error != OK`.
	static Ref<ResourceLoadResult> create(const String &p_path, const Ref<Resource> &p_resource, Error p_error, const String &p_error_message);

	bool is_ok() const { return error == OK; }
	Ref<Resource> get_resource() const { return resource; }
	Error get_error() const { return error; }
	String get_error_message() const { return error_message; }
	String get_path() const { return path; }
};

// The result of `ResourceSaver.save_result()`: the error code and the message the saver
// printed. Read-only after creation.
class ResourceSaveResult : public RefCounted {
	GDCLASS(ResourceSaveResult, RefCounted);

	Error error = ERR_UNCONFIGURED;
	String error_message;
	String path;

protected:
	static void _bind_methods();

public:
	static Ref<ResourceSaveResult> create(const String &p_path, Error p_error, const String &p_error_message);

	bool is_ok() const { return error == OK; }
	Error get_error() const { return error; }
	String get_error_message() const { return error_message; }
	String get_path() const { return path; }
};

// Builds the `error_message` of a failed result: the captured loader or saver message, or the
// readable name of the error code with the path when nothing was printed.
String resource_result_error_message(Error p_error, const String &p_path, const String &p_captured);
