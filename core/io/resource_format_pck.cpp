/**************************************************************************/
/*  resource_format_pck.cpp                                               */
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

#include "resource_format_pck.h"

#include "core/io/pck_package.h"
#include "core/io/pck_package_server.h"
#include "core/object/class_db.h"

Ref<Resource> ResourceFormatLoaderPck::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<PCKPackage> package;
	package.instantiate();
	// Opens the directory only: the pack is not mounted.
	const Error err = package->open_pack(p_path);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Resource>();
	}
	return package;
}

void ResourceFormatLoaderPck::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("pck");
}

bool ResourceFormatLoaderPck::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("PCKPackage", p_type);
}

String ResourceFormatLoaderPck::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "pck" ? "PCKPackage" : "";
}

Error ResourceFormatSaverPck::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	const Ref<PCKPackage> package = p_resource;
	ERR_FAIL_COND_V(package.is_null(), ERR_INVALID_PARAMETER);
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	ERR_FAIL_NULL_V(server, ERR_UNAVAILABLE);
	// Encrypted files use the key of the package, or the key compiled into the engine.
	// Use `PCKPackageServer.build()` for any other key.
	return server->build(package, p_path);
}

void ResourceFormatSaverPck::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<PCKPackage>(*p_resource)) {
		p_extensions->push_back("pck");
	}
}

bool ResourceFormatSaverPck::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<PCKPackage>(*p_resource) != nullptr;
}
