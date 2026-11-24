/**************************************************************************/
/*  files_manifest_validation.cpp                                         */
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

#include "files_manifest_validation.h"

#include "core/object/class_db.h"

void FilesManifestValidation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &FilesManifestValidation::is_valid);
	ClassDB::bind_method(D_METHOD("get_error"), &FilesManifestValidation::get_error);
	ClassDB::bind_method(D_METHOD("get_missing_folders"), &FilesManifestValidation::get_missing_folders);
	ClassDB::bind_method(D_METHOD("get_missing_files"), &FilesManifestValidation::get_missing_files);
	ClassDB::bind_method(D_METHOD("get_unknown_extensions"), &FilesManifestValidation::get_unknown_extensions);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid"), "", "is_valid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "missing_folders"), "", "get_missing_folders");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "missing_files"), "", "get_missing_files");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "unknown_extensions"), "", "get_unknown_extensions");
}
