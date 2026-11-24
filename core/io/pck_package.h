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

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

// A `.pck` file as a resource. Opened from a `.pck` (`PckPackageServer::open()`, `load("x.pck")`), it holds
// the directory of that pack plus edits in memory. Created with `PckPackage.new()` or loaded from a
// `.tres`/`.res`, it is a pack description: target paths mapped to sources, built with `PckPackageServer::build()`.
// The key is a runtime member only and is never stored. Not thread-safe: use one package from one thread at a time.
class PckPackage : public Resource {
	GDCLASS(PckPackage, Resource);

public:
	enum SourceType {
		// The data is a file of a pack: of `pack_path` if the entry was read by `open_pack()`, else of `base_pack`.
		SOURCE_PACK,
		// The data is the file at `source_path`, read when the pack is built.
		SOURCE_FILE,
		// The data is held in memory.
		SOURCE_DATA,
	};

	struct Entry {
		// Path inside the pack, without `res://`.
		String path;
		SourceType source_type = SOURCE_PACK;
		// Absolute position of the data in the pack file at `pack_path`. Only valid if `pending` is false.
		uint64_t offset = 0;
		uint64_t size = 0;
		uint8_t md5[16] = {};
		bool encrypted = false;
		bool removal = false;
		bool delta = false;
		// Not (yet) in the pack file at `pack_path`: added in memory, or described in a `.tres`.
		bool pending = false;
		// SOURCE_FILE: the source file. SOURCE_PACK: the path inside the base pack, without `res://`.
		String source_path;
		// SOURCE_DATA: the data.
		Vector<uint8_t> data;
	};

private:
	// Empty, and 0 for the format, until a pack is read with `open_pack()`.
	String pack_path;
	String base_pack;
	int format_version = 0;
	String engine_version;
	int alignment = 32;
	bool encrypted_directory = false;
	bool sparse_bundle = false;
	// Runtime only: never a storage property, so it is never written into a `.tres`.
	Vector<uint8_t> key;
	// Insertion order is kept, so entries are listed and built in pack order.
	HashMap<String, Entry> entries;
	// The opened directory of `base_pack`, for SOURCE_PACK entries of a description.
	mutable Ref<PckPackage> base_cache;

	static String _key_path(const String &p_path);
	Error _open_pack(const String &p_path, const Vector<uint8_t> &p_key, uint64_t p_offset);
	void _copy_state(const PckPackage &p_from, bool p_copy_key);
	Array _get_files() const;
	void _set_files(const Array &p_files);
	PackedStringArray _get_removals() const;
	void _set_removals(const PackedStringArray &p_removals);

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual Ref<Resource> _duplicate(const DuplicateParams &p_params) const override;

public:
	// Decodes a key of 64 hexadecimal characters. An empty key is the script encryption key compiled into the engine.
	static Error decode_key(const String &p_key, Vector<uint8_t> &r_key);

	// Reads the header and directory of the pack at `p_path` (does not mount it). Replaces all entries,
	// and `base_pack` becomes `p_path`.
	Error open_pack(const String &p_path, const String &p_key = String(), uint64_t p_offset = 0);
	// Reads the data of one entry into `r_data`; encrypted entries are decrypted with the key of the package.
	Error read_entry(const String &p_path, Vector<uint8_t> &r_data) const;
	// Drops the cached directory of `base_pack`, for example after the base pack was rebuilt.
	void release_base_pack() const;

	const HashMap<String, Entry> &get_entry_map() const { return entries; }
	const Vector<uint8_t> &get_key() const { return key; }
	Error set_key(const String &p_key);
	bool is_sparse_bundle() const { return sparse_bundle; }
	bool is_description() const { return pack_path.is_empty(); }

	String get_pack_path() const { return pack_path; }
	void set_base_pack(const String &p_base_pack);
	String get_base_pack() const { return base_pack; }
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

	virtual Error copy_from(const Ref<Resource> &p_resource) override;
	virtual void reload_from_file() override;
};

VARIANT_ENUM_CAST(PckPackage::SourceType);
