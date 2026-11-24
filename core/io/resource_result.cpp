/**************************************************************************/
/*  resource_result.cpp                                                   */
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

#include "resource_result.h"

#include "core/error/error_list.h"
#include "core/object/class_db.h"

String resource_result_error_message(Error p_error, const String &p_path, const String &p_captured) {
	if (p_error == OK) {
		return String();
	}
	if (!p_captured.is_empty()) {
		return p_captured;
	}
	const String name = (p_error > OK && p_error < ERR_MAX) ? String(error_names[p_error]) : String("Unknown error");
	return name + ": '" + p_path + "'.";
}

Ref<ResourceLoadResult> ResourceLoadResult::create(const String &p_path, const Ref<Resource> &p_resource, Error p_error, const String &p_error_message) {
	Ref<ResourceLoadResult> result;
	result.instantiate();
	result->path = p_path;
	result->error = p_error;
	if (p_error == OK && p_resource.is_null()) {
		result->error = ERR_CANT_ACQUIRE_RESOURCE;
	}
	if (result->error == OK) {
		result->resource = p_resource;
	} else {
		result->error_message = resource_result_error_message(result->error, p_path, p_error_message);
	}
	return result;
}

void ResourceLoadResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ok"), &ResourceLoadResult::is_ok);
	ClassDB::bind_method(D_METHOD("get_resource"), &ResourceLoadResult::get_resource);
	ClassDB::bind_method(D_METHOD("get_error"), &ResourceLoadResult::get_error);
	ClassDB::bind_method(D_METHOD("get_error_message"), &ResourceLoadResult::get_error_message);
	ClassDB::bind_method(D_METHOD("get_path"), &ResourceLoadResult::get_path);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ok"), "", "is_ok");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), "", "get_resource");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_message"), "", "get_error_message");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "", "get_path");
}

Ref<ResourceSaveResult> ResourceSaveResult::create(const String &p_path, Error p_error, const String &p_error_message) {
	Ref<ResourceSaveResult> result;
	result.instantiate();
	result->path = p_path;
	result->error = p_error;
	result->error_message = resource_result_error_message(p_error, p_path, p_error_message);
	return result;
}

void ResourceSaveResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_ok"), &ResourceSaveResult::is_ok);
	ClassDB::bind_method(D_METHOD("get_error"), &ResourceSaveResult::get_error);
	ClassDB::bind_method(D_METHOD("get_error_message"), &ResourceSaveResult::get_error_message);
	ClassDB::bind_method(D_METHOD("get_path"), &ResourceSaveResult::get_path);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ok"), "", "is_ok");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_message"), "", "get_error_message");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "", "get_path");
}
