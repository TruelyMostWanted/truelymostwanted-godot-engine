/**************************************************************************/
/*  preloads.cpp                                                          */
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

#include "preloads.h"

#include "core/object/class_db.h"

void Preloads::set_resources(const TypedArray<Resource> &p_resources) {
	resources = p_resources;
	emit_changed();
}

TypedArray<Resource> Preloads::get_resources() const {
	return resources;
}

void Preloads::set_paths(const PackedStringArray &p_paths) {
	paths = p_paths;
	emit_changed();
}

PackedStringArray Preloads::get_paths() const {
	return paths;
}

void Preloads::set_manifests(const TypedArray<FilesManifest> &p_manifests) {
	manifests = p_manifests;
	emit_changed();
}

TypedArray<FilesManifest> Preloads::get_manifests() const {
	return manifests;
}

void Preloads::set_keys(const TypedDictionary<String, String> &p_keys) {
	keys = p_keys;
	emit_changed();
}

TypedDictionary<String, String> Preloads::get_keys() const {
	return keys;
}

bool Preloads::is_empty() const {
	return resources.is_empty() && paths.is_empty() && manifests.is_empty();
}

void Preloads::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_resources", "resources"), &Preloads::set_resources);
	ClassDB::bind_method(D_METHOD("get_resources"), &Preloads::get_resources);
	ClassDB::bind_method(D_METHOD("set_paths", "paths"), &Preloads::set_paths);
	ClassDB::bind_method(D_METHOD("get_paths"), &Preloads::get_paths);
	ClassDB::bind_method(D_METHOD("set_manifests", "manifests"), &Preloads::set_manifests);
	ClassDB::bind_method(D_METHOD("get_manifests"), &Preloads::get_manifests);
	ClassDB::bind_method(D_METHOD("set_keys", "keys"), &Preloads::set_keys);
	ClassDB::bind_method(D_METHOD("get_keys"), &Preloads::get_keys);
	ClassDB::bind_method(D_METHOD("is_empty"), &Preloads::is_empty);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "resources", PROPERTY_HINT_ARRAY_TYPE, "Resource"), "set_resources", "get_resources");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:", Variant::STRING, PROPERTY_HINT_FILE)), "set_paths", "get_paths");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "manifests", PROPERTY_HINT_ARRAY_TYPE, "FilesManifest"), "set_manifests", "get_manifests");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "keys", PROPERTY_HINT_DICTIONARY_TYPE, "String;String"), "set_keys", "get_keys");
}
