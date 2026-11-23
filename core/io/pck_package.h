/**************************************************************************/
/*  pck_package.h                                                         */
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
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

// The directory of one `.pck` file plus edits in memory. Created by `PckPackageServer::open()`,
// or empty with `PckPackage.new()`. Not thread-safe: use one package from one thread at a time.
class PckPackage : public RefCounted {
	GDCLASS(PckPackage, RefCounted);

public:
	struct Entry {
		// Path inside the pack, without `res://`.
		String path;
		// Absolute position of the data in the pack file at `PckPackage::path`.
		uint64_t offset = 0;
		uint64_t size = 0;
		uint8_t md5[16] = {};
		bool encrypted = false;
		bool removal = false;
		bool delta = false;
		// Added in memory: the data comes from `source_path` or `data`, not from the pack file.
		bool pending = false;
		String source_path;
		Vector<uint8_t> data;
	};

private:
	// Empty, and 0 for the format, until a pack is read with `open_pack()`.
	String path;
	int format_version = 0;
	String engine_version;
	int alignment = 32;
	bool encrypted_directory = false;
	bool sparse_bundle = false;
	Vector<uint8_t> key;
	// Insertion order is kept, so entries are listed and built in pack order.
	HashMap<String, Entry> entries;

	static String _key_path(const String &p_path);

protected:
	static void _bind_methods();

public:
	// Decodes a key of 64 hexadecimal characters. An empty key is the script encryption key compiled into the engine.
	static Error decode_key(const String &p_key, Vector<uint8_t> &r_key);

	// Reads the header and directory of the pack at `p_path` (does not mount it). Replaces all entries.
	Error open_pack(const String &p_path, const String &p_key = String(), uint64_t p_offset = 0);
	// Reads the data of one entry into `r_data`; encrypted entries are decrypted with the key of the package.
	Error read_entry(const String &p_path, Vector<uint8_t> &r_data) const;

	const HashMap<String, Entry> &get_entry_map() const { return entries; }
	const Vector<uint8_t> &get_key() const { return key; }
	bool is_sparse_bundle() const { return sparse_bundle; }

	String get_path() const { return path; }
	int get_format_version() const { return format_version; }
	String get_engine_version() const { return engine_version; }
	void set_alignment(int p_alignment);
	int get_alignment() const { return alignment; }
	void set_encrypted_directory(bool p_encrypted);
	bool is_encrypted_directory() const { return encrypted_directory; }

	TypedArray<Dictionary> get_entries() const;
	PackedStringArray get_paths() const;
	int get_file_count() const;
	bool has_file(const String &p_path) const;

	Error add_file(const String &p_target_path, const String &p_source_path, bool p_encrypt = false);
	Error add_file_from_buffer(const String &p_target_path, const Vector<uint8_t> &p_data, bool p_encrypt = false);
	Error remove_file(const String &p_path);
	Error rename_file(const String &p_from, const String &p_to);
	Error mark_removal(const String &p_path);
};
