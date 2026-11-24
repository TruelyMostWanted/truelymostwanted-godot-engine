/**************************************************************************/
/*  files_manifest_validation.h                                           */
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

// The result of `FilesManifestServer::validate()`: what is missing or unknown in one manifest.
// Read-only after creation.
class FilesManifestValidation : public RefCounted {
	GDCLASS(FilesManifestValidation, RefCounted);

	friend class FilesManifestServer;

	Error error = ERR_UNCONFIGURED;
	PackedStringArray missing_folders;
	PackedStringArray missing_files;
	PackedStringArray unknown_extensions;

protected:
	static void _bind_methods();

public:
	bool is_valid() const { return error == OK; }
	Error get_error() const { return error; }
	PackedStringArray get_missing_folders() const { return missing_folders; }
	PackedStringArray get_missing_files() const { return missing_files; }
	PackedStringArray get_unknown_extensions() const { return unknown_extensions; }
};
