/**************************************************************************/
/*  pck_package_server.h                                                  */
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

#include "core/io/pck_extract_result.h"
#include "core/io/pck_package.h"
#include "core/object/object.h"

// One place for `.pck` files: open and inspect without mounting, read single files, verify,
// build a new pack, extract a pack into a folder, mount and unmount packs, and find the pack that provides a `res://` file.
// The mounted packs live in `PackedData`; the server keeps no state of its own.
class PCKPackageServer : public Object {
	GDCLASS(PCKPackageServer, Object);

public:
	enum ExtractPolicy {
		EXTRACT_SKIP,
		EXTRACT_OVERWRITE,
		EXTRACT_RENAME,
	};

private:
	static PCKPackageServer *singleton;

	void _emit_deferred(const StringName &p_signal, const String &p_path);
	void _emit_extract_progress(const String &p_path, int p_done, int p_total);

protected:
	static void _bind_methods();

public:
	static PCKPackageServer *get_singleton();

	Ref<PCKPackage> open(const String &p_path, const String &p_key = String(), uint64_t p_offset = 0);
	Vector<uint8_t> read_file(const Ref<PCKPackage> &p_package, const String &p_path);
	Error verify(const Ref<PCKPackage> &p_package);
	Error build(const Ref<PCKPackage> &p_package, const String &p_target_path, const String &p_key = String());
	// Writes the files of `p_package` below `p_target_dir`. `p_paths` filters (files, folders or `*`/`?` patterns;
	// empty: all files). Sidecars (`.import`, `.uid`) are selected with their source file and follow its decision.
	Ref<PCKExtractResult> extract(const Ref<PCKPackage> &p_package, const String &p_target_dir, const PackedStringArray &p_paths = PackedStringArray(), ExtractPolicy p_policy = EXTRACT_SKIP, bool p_skip_engine_files = true);

	Error mount(const String &p_path, bool p_replace_files = true, uint64_t p_offset = 0);
	Error unmount(const String &p_path);
	bool is_mounted(const String &p_path) const;
	bool is_main_pack(const String &p_path) const;
	PackedStringArray get_mounted_packs() const;
	String get_providing_pack(const String &p_res_path) const;

	PCKPackageServer();
	~PCKPackageServer();
};

VARIANT_ENUM_CAST(PCKPackageServer::ExtractPolicy);
