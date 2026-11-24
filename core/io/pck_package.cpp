/**************************************************************************/
/*  pck_package.cpp                                                       */
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

#include "pck_package.h"

#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_pack.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"

// Smallest directory entry: path length, offset, size, MD5 and flags.
static constexpr uint64_t PCK_MIN_ENTRY_SIZE = 4 + 8 + 8 + 16 + 4;
static constexpr uint64_t PCK_MAX_DETECTED_ALIGNMENT = 4096;

String PckPackage::_key_path(const String &p_path) {
	return p_path.replace_char('\\', '/').simplify_path().trim_prefix("res://").trim_prefix("/");
}

Error PckPackage::decode_key(const String &p_key, Vector<uint8_t> &r_key) {
	r_key.resize_initialized(32);
	if (p_key.is_empty()) {
		memcpy(r_key.ptrw(), script_encryption_key, 32);
		return OK;
	}
	if (p_key.length() != 64 || !p_key.is_valid_hex_number(false)) {
		return ERR_INVALID_PARAMETER;
	}
	const Vector<uint8_t> decoded = p_key.to_lower().hex_decode();
	if (decoded.size() != 32) {
		return ERR_INVALID_PARAMETER;
	}
	r_key = decoded;
	return OK;
}

Error PckPackage::set_key(const String &p_key) {
	Vector<uint8_t> new_key;
	ERR_FAIL_COND_V_MSG(decode_key(p_key, new_key) != OK, ERR_INVALID_PARAMETER, "Invalid pack key: use 64 hexadecimal characters, or an empty string for the key compiled into the engine.");
	key = new_key;
	base_cache.unref();
	return OK;
}

Error PckPackage::open_pack(const String &p_path, const String &p_key, uint64_t p_offset) {
	Vector<uint8_t> new_key;
	ERR_FAIL_COND_V_MSG(decode_key(p_key, new_key) != OK, ERR_INVALID_PARAMETER, "Invalid pack key: use 64 hexadecimal characters, or an empty string for the key compiled into the engine.");
	return _open_pack(p_path, new_key, p_offset);
}

Error PckPackage::_open_pack(const String &p_path, const Vector<uint8_t> &p_key, uint64_t p_offset) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err == OK ? ERR_FILE_CANT_OPEN : err, vformat("Can't open pack \"%s\".", p_path));
	const uint64_t length = f->get_length();

	bool found = false;
	if (length >= p_offset + 4) {
		f->seek(p_offset);
		found = f->get_32() == PACK_HEADER_MAGIC;
	}
	if (!found && p_offset == 0 && length >= 16) {
		// Pack appended to the end of a file (self-contained executable layout).
		f->seek(length - 4);
		if (f->get_32() == PACK_HEADER_MAGIC) {
			f->seek(length - 12);
			const uint64_t directory_size = f->get_64();
			if (directory_size + 16 <= length) {
				f->seek(length - 12 - directory_size);
				found = f->get_32() == PACK_HEADER_MAGIC;
			}
		}
	}
	ERR_FAIL_COND_V_MSG(!found, ERR_FILE_UNRECOGNIZED, vformat("\"%s\" is not a pack: no pack header at offset %d or at the end of the file.", p_path, p_offset));

	const uint64_t pck_start = f->get_position() - 4;
	const uint32_t version = f->get_32();
	const uint32_t ver_major = f->get_32();
	const uint32_t ver_minor = f->get_32();
	const uint32_t ver_patch = f->get_32();
	ERR_FAIL_COND_V_MSG(version != PACK_FORMAT_VERSION_V2 && version != PACK_FORMAT_VERSION_V3 && version != PACK_FORMAT_VERSION_V4, ERR_FILE_UNRECOGNIZED, vformat("Pack \"%s\" uses the unsupported format version %d (supported: 2 to %d).", p_path, version, PACK_FORMAT_VERSION));

	const uint32_t pack_flags = f->get_32();
	const bool enc_directory = pack_flags & PACK_DIR_ENCRYPTED;
	const bool rel_filebase = pack_flags & PACK_REL_FILEBASE;
	const bool sparse = pack_flags & PACK_SPARSE_BUNDLE;

	uint64_t file_base = f->get_64();
	if (version == PACK_FORMAT_VERSION_V4 || version == PACK_FORMAT_VERSION_V3 || rel_filebase) {
		file_base += pck_start;
	}
	if (version == PACK_FORMAT_VERSION_V3 || version == PACK_FORMAT_VERSION_V4) {
		const uint64_t dir_offset = f->get_64() + pck_start;
		ERR_FAIL_COND_V_MSG(dir_offset + 4 > length, ERR_FILE_CORRUPT, vformat("Pack \"%s\" is corrupt: the directory lies outside the file.", p_path));
		f->seek(dir_offset);
	} else {
		for (int i = 0; i < 16; i++) {
			f->get_32(); // Reserved.
		}
	}

	const uint32_t file_count = f->get_32();
	if (enc_directory) {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_READ, false);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't read the encrypted directory of pack \"%s\": wrong key or corrupt data.", p_path));
		f = fae;
	}
	ERR_FAIL_COND_V_MSG(uint64_t(file_count) * PCK_MIN_ENTRY_SIZE > f->get_length(), ERR_FILE_CORRUPT, vformat("Pack \"%s\" is corrupt: %d directory entries don't fit into the file.", p_path, file_count));

	HashMap<String, Entry> new_entries;
	uint64_t detected_alignment = PCK_MAX_DETECTED_ALIGNMENT;
	bool has_data = false;
	for (uint32_t i = 0; i < file_count; i++) {
		const uint32_t string_length = f->get_32();
		ERR_FAIL_COND_V_MSG(f->get_position() + string_length > f->get_length(), ERR_FILE_CORRUPT, vformat("Pack \"%s\" is corrupt: directory entry %d is too long.", p_path, i));
		CharString cs;
		cs.resize_uninitialized(string_length + 1);
		f->get_buffer((uint8_t *)cs.ptrw(), string_length);
		cs[string_length] = 0;

		Entry entry;
		entry.path = _key_path(String::utf8(cs.ptr()));
		entry.source_type = SOURCE_PACK;
		entry.source_path = entry.path;
		entry.offset = file_base + f->get_64();
		entry.size = f->get_64();
		f->get_buffer(entry.md5, 16);
		const uint32_t flags = f->get_32();
		entry.encrypted = flags & PACK_FILE_ENCRYPTED;
		entry.removal = flags & PACK_FILE_REMOVAL;
		entry.delta = flags & PACK_FILE_DELTA;

		if (!entry.removal && !sparse && entry.size > 0) {
			has_data = true;
			const uint64_t relative = entry.offset - pck_start;
			while (detected_alignment > 1 && relative % detected_alignment != 0) {
				detected_alignment >>= 1;
			}
		}
		const String entry_path = entry.path;
		new_entries.insert(entry_path, entry);
	}

	pack_path = p_path;
	base_pack = p_path;
	format_version = version;
	engine_version = vformat("%d.%d.%d", ver_major, ver_minor, ver_patch);
	alignment = has_data ? int(detected_alignment) : 32;
	encrypted_directory = enc_directory;
	sparse_bundle = sparse;
	key = p_key;
	entries = new_entries;
	base_cache.unref();
	return OK;
}

Error PckPackage::read_entry(const String &p_path, Vector<uint8_t> &r_data) const {
	const Entry *entry = entries.getptr(_key_path(p_path));
	ERR_FAIL_NULL_V_MSG(entry, ERR_FILE_NOT_FOUND, vformat("The pack has no file \"%s\".", p_path));
	ERR_FAIL_COND_V_MSG(entry->removal, ERR_FILE_NOT_FOUND, vformat("\"%s\" is a removal entry and has no data.", p_path));

	if (entry->pending) {
		if (entry->source_type == SOURCE_DATA) {
			r_data = entry->data;
			return OK;
		}
		if (entry->source_type == SOURCE_FILE) {
			ERR_FAIL_COND_V_MSG(!FileAccess::exists(entry->source_path), ERR_FILE_NOT_FOUND, vformat("The source \"%s\" of \"%s\" does not exist.", entry->source_path, p_path));
			Error err = OK;
			r_data = FileAccess::get_file_as_bytes(entry->source_path, &err);
			ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't read \"%s\", the source of \"%s\".", entry->source_path, p_path));
			return OK;
		}
		// A file of the base pack, described in a `.tres`: find it in the directory of the base pack.
		ERR_FAIL_COND_V_MSG(base_pack.is_empty(), ERR_FILE_NOT_FOUND, vformat("\"%s\" comes from a pack, but the package has no base pack.", p_path));
		if (base_cache.is_null()) {
			Vector<uint8_t> base_key = key;
			if (base_key.is_empty()) {
				decode_key(String(), base_key);
			}
			Ref<PckPackage> base;
			base.instantiate();
			const Error err = base->_open_pack(base_pack, base_key, 0);
			if (err != OK) {
				return err;
			}
			base_cache = base;
		}
		const Entry *source = base_cache->entries.getptr(entry->source_path);
		ERR_FAIL_NULL_V_MSG(source, ERR_FILE_NOT_FOUND, vformat("The base pack \"%s\" has no file \"res://%s\" (the source of \"%s\").", base_pack, entry->source_path, p_path));
		ERR_FAIL_COND_V_MSG(source->delta, ERR_UNAVAILABLE, vformat("\"res://%s\" in the base pack \"%s\" is a delta patch and can't be read as a file.", entry->source_path, base_pack));
		return base_cache->read_entry(entry->source_path, r_data);
	}

	ERR_FAIL_COND_V_MSG(sparse_bundle, ERR_UNAVAILABLE, vformat("Pack \"%s\" is a sparse bundle: its files are stored next to the pack, not in it.", pack_path));
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(pack_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err == OK ? ERR_FILE_CANT_OPEN : err, vformat("Can't open pack \"%s\".", pack_path));
	ERR_FAIL_COND_V_MSG(entry->offset > f->get_length(), ERR_FILE_CORRUPT, vformat("Pack \"%s\" is corrupt: \"%s\" lies outside the file.", pack_path, p_path));
	f->seek(entry->offset);
	if (entry->encrypted) {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't decrypt \"%s\" in pack \"%s\": wrong key or corrupt data.", p_path, pack_path));
		f = fae;
	}
	ERR_FAIL_COND_V_MSG(f->get_position() + entry->size > f->get_length(), ERR_FILE_CORRUPT, vformat("Pack \"%s\" is corrupt: \"%s\" is cut off.", pack_path, p_path));
	ERR_FAIL_COND_V(r_data.resize_uninitialized(entry->size) != OK, ERR_OUT_OF_MEMORY);
	if (entry->size > 0 && f->get_buffer(r_data.ptrw(), entry->size) != entry->size) {
		r_data.clear();
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, vformat("Can't read \"%s\" from pack \"%s\".", p_path, pack_path));
	}
	return OK;
}

void PckPackage::release_base_pack() const {
	base_cache.unref();
}

void PckPackage::set_base_pack(const String &p_base_pack) {
	if (base_pack == p_base_pack) {
		return;
	}
	base_pack = p_base_pack;
	base_cache.unref();
	// Entries read from `pack_path` now come from the new base pack.
	for (KeyValue<String, Entry> &kv : entries) {
		if (kv.value.source_type == SOURCE_PACK && !kv.value.removal) {
			kv.value.pending = true;
		}
	}
	emit_changed();
}

void PckPackage::set_alignment(int p_alignment) {
	ERR_FAIL_COND_MSG(p_alignment <= 0, "The pack alignment must be greater than 0.");
	alignment = p_alignment;
	emit_changed();
}

void PckPackage::set_encrypted_directory(bool p_encrypted) {
	encrypted_directory = p_encrypted;
	emit_changed();
}

TypedArray<Dictionary> PckPackage::get_entries() const {
	TypedArray<Dictionary> result;
	for (const KeyValue<String, Entry> &kv : entries) {
		const Entry &entry = kv.value;
		Dictionary d;
		d["path"] = "res://" + entry.path;
		d["size"] = entry.size;
		d["offset"] = entry.pending ? 0 : entry.offset;
		d["md5"] = String::md5(entry.md5);
		d["encrypted"] = entry.encrypted;
		d["removal"] = entry.removal;
		d["delta"] = entry.delta;
		d["pending"] = entry.pending;
		d["source_type"] = int(entry.source_type);
		if (entry.removal || entry.source_type == SOURCE_DATA) {
			d["source"] = String();
		} else if (entry.source_type == SOURCE_FILE) {
			d["source"] = entry.source_path;
		} else {
			d["source"] = "res://" + entry.source_path;
		}
		result.push_back(d);
	}
	return result;
}

PackedStringArray PckPackage::get_paths() const {
	PackedStringArray result;
	for (const KeyValue<String, Entry> &kv : entries) {
		if (!kv.value.removal) {
			result.push_back("res://" + kv.key);
		}
	}
	return result;
}

int PckPackage::get_file_count() const {
	return entries.size();
}

bool PckPackage::has_file(const String &p_path) const {
	return entries.has(_key_path(p_path));
}

Error PckPackage::add_file(const String &p_target_path, const String &p_source_path, bool p_encrypt) {
	const String target = _key_path(p_target_path);
	ERR_FAIL_COND_V_MSG(target.is_empty() || target.ends_with("/"), ERR_INVALID_PARAMETER, vformat("Invalid target path \"%s\".", p_target_path));
	ERR_FAIL_COND_V_MSG(p_source_path.is_empty(), ERR_INVALID_PARAMETER, vformat("The source path of \"%s\" is empty.", p_target_path));

	// Only the source is recorded: a description may name a file that is created later. `build()` reports missing sources.
	Entry entry;
	entry.path = target;
	entry.source_type = SOURCE_FILE;
	entry.encrypted = p_encrypt;
	entry.pending = true;
	entry.source_path = p_source_path;
	entries.insert(target, entry);
	emit_changed();
	return OK;
}

Error PckPackage::add_file_from_buffer(const String &p_target_path, const Vector<uint8_t> &p_data, bool p_encrypt) {
	const String target = _key_path(p_target_path);
	ERR_FAIL_COND_V_MSG(target.is_empty() || target.ends_with("/"), ERR_INVALID_PARAMETER, vformat("Invalid target path \"%s\".", p_target_path));

	Entry entry;
	entry.path = target;
	entry.source_type = SOURCE_DATA;
	entry.size = p_data.size();
	entry.encrypted = p_encrypt;
	entry.pending = true;
	entry.data = p_data;
	CryptoCore::md5(p_data.ptr(), p_data.size(), entry.md5);
	entries.insert(target, entry);
	emit_changed();
	return OK;
}

Error PckPackage::remove_file(const String &p_path) {
	ERR_FAIL_COND_V_MSG(!entries.erase(_key_path(p_path)), ERR_DOES_NOT_EXIST, vformat("The pack has no file \"%s\".", p_path));
	emit_changed();
	return OK;
}

Error PckPackage::rename_file(const String &p_from, const String &p_to) {
	const String from = _key_path(p_from);
	const String to = _key_path(p_to);
	ERR_FAIL_COND_V_MSG(to.is_empty() || to.ends_with("/"), ERR_INVALID_PARAMETER, vformat("Invalid target path \"%s\".", p_to));
	const Entry *source = entries.getptr(from);
	ERR_FAIL_NULL_V_MSG(source, ERR_DOES_NOT_EXIST, vformat("The pack has no file \"%s\".", p_from));
	if (from == to) {
		return OK;
	}
	ERR_FAIL_COND_V_MSG(entries.has(to), ERR_ALREADY_EXISTS, vformat("The pack already has a file \"%s\".", p_to));
	Entry entry = *source;
	entry.path = to;
	entries.erase(from);
	entries.insert(to, entry);
	emit_changed();
	return OK;
}

Error PckPackage::mark_removal(const String &p_path) {
	const String target = _key_path(p_path);
	ERR_FAIL_COND_V_MSG(target.is_empty() || target.ends_with("/"), ERR_INVALID_PARAMETER, vformat("Invalid path \"%s\".", p_path));
	Entry entry;
	entry.path = target;
	entry.removal = true;
	entry.pending = true;
	entries.insert(target, entry);
	emit_changed();
	return OK;
}

Array PckPackage::_get_files() const {
	Array result;
	for (const KeyValue<String, Entry> &kv : entries) {
		const Entry &entry = kv.value;
		if (entry.removal) {
			continue;
		}
		Dictionary d;
		d["path"] = entry.path;
		switch (entry.source_type) {
			case SOURCE_FILE: {
				d["source"] = entry.source_path;
			} break;
			case SOURCE_DATA: {
				d["data"] = entry.data;
			} break;
			case SOURCE_PACK: {
				d["from_pack"] = entry.source_path;
			} break;
		}
		if (entry.encrypted) {
			d["encrypt"] = true;
		}
		result.push_back(d);
	}
	return result;
}

void PckPackage::_set_files(const Array &p_files) {
	HashMap<String, Entry> new_entries;
	int invalid = 0;
	for (int i = 0; i < p_files.size(); i++) {
		if (p_files[i].get_type() != Variant::DICTIONARY) {
			invalid++;
			continue;
		}
		const Dictionary d = p_files[i];
		Entry entry;
		entry.path = _key_path(d.get("path", String()));
		entry.encrypted = d.get("encrypt", false);
		entry.pending = true;
		if (entry.path.is_empty() || entry.path.ends_with("/")) {
			invalid++;
			continue;
		}
		if (d.has("source")) {
			entry.source_type = SOURCE_FILE;
			entry.source_path = d["source"];
		} else if (d.has("data")) {
			entry.source_type = SOURCE_DATA;
			const PackedByteArray data = d["data"];
			entry.data = data;
			entry.size = entry.data.size();
			CryptoCore::md5(entry.data.ptr(), entry.data.size(), entry.md5);
		} else if (d.has("from_pack")) {
			entry.source_type = SOURCE_PACK;
			entry.source_path = _key_path(d["from_pack"]);
			// An unchanged file of the opened pack keeps its place in `pack_path`.
			const Entry *old = entries.getptr(entry.source_path);
			if (!pack_path.is_empty() && base_pack == pack_path && old && !old->pending && !old->removal && old->source_type == SOURCE_PACK && old->encrypted == entry.encrypted) {
				entry.offset = old->offset;
				entry.size = old->size;
				memcpy(entry.md5, old->md5, 16);
				entry.delta = old->delta;
				entry.pending = false;
			}
		} else {
			invalid++;
			continue;
		}
		const String entry_path = entry.path;
		new_entries.insert(entry_path, entry);
	}
	// Removal entries are stored in `removals`.
	for (const KeyValue<String, Entry> &kv : entries) {
		if (kv.value.removal && !new_entries.has(kv.key)) {
			new_entries.insert(kv.key, kv.value);
		}
	}
	entries = new_entries;
	if (invalid > 0) {
		ERR_PRINT(vformat("%d entries of \"files\" were skipped: each needs a \"path\" and one of \"source\", \"data\" or \"from_pack\".", invalid));
	}
}

PackedStringArray PckPackage::_get_removals() const {
	PackedStringArray result;
	for (const KeyValue<String, Entry> &kv : entries) {
		if (kv.value.removal) {
			result.push_back(kv.key);
		}
	}
	return result;
}

void PckPackage::_set_removals(const PackedStringArray &p_removals) {
	HashMap<String, Entry> new_entries;
	for (const KeyValue<String, Entry> &kv : entries) {
		if (!kv.value.removal) {
			new_entries.insert(kv.key, kv.value);
		}
	}
	for (const String &removal : p_removals) {
		const String target = _key_path(removal);
		if (target.is_empty() || target.ends_with("/")) {
			continue;
		}
		const Entry *old = entries.getptr(target);
		if (old && old->removal) {
			new_entries.insert(target, *old);
			continue;
		}
		Entry entry;
		entry.path = target;
		entry.removal = true;
		entry.pending = true;
		new_entries.insert(target, entry);
	}
	entries = new_entries;
}

bool PckPackage::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("files")) {
		_set_files(p_value);
		emit_changed();
		return true;
	}
	if (p_name == SNAME("removals")) {
		_set_removals(p_value);
		emit_changed();
		return true;
	}
	return false;
}

bool PckPackage::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == SNAME("files")) {
		r_ret = _get_files();
		return true;
	}
	if (p_name == SNAME("removals")) {
		r_ret = _get_removals();
		return true;
	}
	return false;
}

void PckPackage::_get_property_list(List<PropertyInfo> *p_list) const {
	// Storage only: the entries are edited through the methods (and the Inspector plugin), not as raw arrays.
	p_list->push_back(PropertyInfo(Variant::ARRAY, "files", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_STRING_ARRAY, "removals", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
}

void PckPackage::_copy_state(const PckPackage &p_from, bool p_copy_key) {
	pack_path = p_from.pack_path;
	base_pack = p_from.base_pack;
	format_version = p_from.format_version;
	engine_version = p_from.engine_version;
	alignment = p_from.alignment;
	encrypted_directory = p_from.encrypted_directory;
	sparse_bundle = p_from.sparse_bundle;
	if (p_copy_key) {
		key = p_from.key;
	}
	entries = p_from.entries;
	base_cache.unref();
}

Ref<Resource> PckPackage::_duplicate(const DuplicateParams &p_params) const {
	Ref<PckPackage> copy = Resource::_duplicate(p_params);
	ERR_FAIL_COND_V(copy.is_null(), Ref<Resource>());
	// The storage properties only describe the files: take over the opened pack and the runtime key as well.
	copy->_copy_state(*this, true);
	return copy;
}

Error PckPackage::copy_from(const Ref<Resource> &p_resource) {
	const Ref<PckPackage> other = p_resource;
	ERR_FAIL_COND_V(other.is_null(), ERR_INVALID_PARAMETER);
	// Used by `CACHE_MODE_REPLACE`, whose new object was opened with the compiled-in key: a key set on this package is kept.
	_copy_state(**other, key.is_empty());
	emit_changed();
	return OK;
}

void PckPackage::reload_from_file() {
	const String resource_path = get_path();
	if (resource_path.get_extension().to_lower() != "pck") {
		// A description in `.tres` or `.res`: `copy_from()` takes over the stored properties.
		Resource::reload_from_file();
		return;
	}
	Vector<uint8_t> reload_key = key;
	if (reload_key.is_empty()) {
		decode_key(String(), reload_key);
	}
	if (_open_pack(resource_path, reload_key, 0) == OK) {
		emit_changed();
	}
}

void PckPackage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pack_path"), &PckPackage::get_pack_path);
	ClassDB::bind_method(D_METHOD("set_base_pack", "base_pack"), &PckPackage::set_base_pack);
	ClassDB::bind_method(D_METHOD("get_base_pack"), &PckPackage::get_base_pack);
	ClassDB::bind_method(D_METHOD("is_description"), &PckPackage::is_description);
	ClassDB::bind_method(D_METHOD("set_key", "key"), &PckPackage::set_key);
	ClassDB::bind_method(D_METHOD("get_format_version"), &PckPackage::get_format_version);
	ClassDB::bind_method(D_METHOD("get_engine_version"), &PckPackage::get_engine_version);
	ClassDB::bind_method(D_METHOD("set_alignment", "alignment"), &PckPackage::set_alignment);
	ClassDB::bind_method(D_METHOD("get_alignment"), &PckPackage::get_alignment);
	ClassDB::bind_method(D_METHOD("set_encrypted_directory", "encrypted"), &PckPackage::set_encrypted_directory);
	ClassDB::bind_method(D_METHOD("is_encrypted_directory"), &PckPackage::is_encrypted_directory);

	ClassDB::bind_method(D_METHOD("get_entries"), &PckPackage::get_entries);
	ClassDB::bind_method(D_METHOD("get_paths"), &PckPackage::get_paths);
	ClassDB::bind_method(D_METHOD("get_file_count"), &PckPackage::get_file_count);
	ClassDB::bind_method(D_METHOD("has_file", "path"), &PckPackage::has_file);

	ClassDB::bind_method(D_METHOD("add_file", "target_path", "source_path", "encrypt"), &PckPackage::add_file, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_file_from_buffer", "target_path", "data", "encrypt"), &PckPackage::add_file_from_buffer, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_file", "path"), &PckPackage::remove_file);
	ClassDB::bind_method(D_METHOD("rename_file", "from", "to"), &PckPackage::rename_file);
	ClassDB::bind_method(D_METHOD("mark_removal", "path"), &PckPackage::mark_removal);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pack_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_pack_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_pack", PROPERTY_HINT_FILE, "*.pck"), "set_base_pack", "get_base_pack");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "format_version", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_format_version");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "engine_version", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_engine_version");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), "set_alignment", "get_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "encrypted_directory"), "set_encrypted_directory", "is_encrypted_directory");

	BIND_ENUM_CONSTANT(SOURCE_PACK);
	BIND_ENUM_CONSTANT(SOURCE_FILE);
	BIND_ENUM_CONSTANT(SOURCE_DATA);
}
