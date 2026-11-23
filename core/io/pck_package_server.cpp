/**************************************************************************/
/*  pck_package_server.cpp                                                */
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

#include "pck_package_server.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/pck_packer.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"

PckPackageServer *PckPackageServer::singleton = nullptr;

PckPackageServer *PckPackageServer::get_singleton() {
	return singleton;
}

static bool _same_pack_path(const String &p_a, const String &p_b) {
	return p_a == p_b || p_a.simplify_path() == p_b.simplify_path();
}

void PckPackageServer::_emit_deferred(const StringName &p_signal, const String &p_path) {
	if (Thread::is_main_thread()) {
		emit_signal(p_signal, p_path);
	} else {
		call_deferred(SNAME("emit_signal"), p_signal, p_path);
	}
}

Ref<PckPackage> PckPackageServer::open(const String &p_path, const String &p_key, uint64_t p_offset) {
	Ref<PckPackage> package;
	package.instantiate();
	if (package->open_pack(p_path, p_key, p_offset) != OK) {
		return Ref<PckPackage>();
	}
	return package;
}

Vector<uint8_t> PckPackageServer::read_file(const Ref<PckPackage> &p_package, const String &p_path) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), Vector<uint8_t>(), "The package is null.");
	Vector<uint8_t> data;
	if (p_package->read_entry(p_path, data) != OK) {
		return Vector<uint8_t>();
	}
	return data;
}

Error PckPackageServer::verify(const Ref<PckPackage> &p_package) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), ERR_INVALID_PARAMETER, "The package is null.");
	ERR_FAIL_COND_V_MSG(p_package->is_sparse_bundle(), ERR_UNAVAILABLE, vformat("Pack \"%s\" is a sparse bundle: its files are stored next to the pack and can't be verified here.", p_package->get_path()));
	for (const KeyValue<String, PckPackage::Entry> &kv : p_package->get_entry_map()) {
		const PckPackage::Entry &entry = kv.value;
		// Removal entries have no data, delta entries store a patch, and pending entries are not in the pack yet.
		if (entry.removal || entry.delta || entry.pending) {
			continue;
		}
		Vector<uint8_t> data;
		const Error err = p_package->read_entry(kv.key, data);
		if (err != OK) {
			return err;
		}
		unsigned char hash[16];
		CryptoCore::md5(data.ptr(), data.size(), hash);
		ERR_FAIL_COND_V_MSG(memcmp(hash, entry.md5, 16) != 0, ERR_FILE_CORRUPT, vformat("The MD5 sum of \"res://%s\" in pack \"%s\" does not match: the pack is corrupt.", kv.key, p_package->get_path()));
	}
	return OK;
}

Error PckPackageServer::build(const Ref<PckPackage> &p_package, const String &p_target_path, const String &p_key) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), ERR_INVALID_PARAMETER, "The package is null.");
	ERR_FAIL_COND_V_MSG(p_target_path.is_empty(), ERR_INVALID_PARAMETER, "The target path of the pack is empty.");
	ERR_FAIL_COND_V_MSG(is_mounted(p_target_path), ERR_BUSY, vformat("Pack \"%s\" is mounted: unmount it before building over it.", p_target_path));

	Vector<uint8_t> key;
	if (p_key.is_empty() && !p_package->get_key().is_empty()) {
		key = p_package->get_key();
	} else {
		ERR_FAIL_COND_V_MSG(PckPackage::decode_key(p_key, key) != OK, ERR_INVALID_PARAMETER, "Invalid pack key: use 64 hexadecimal characters, or an empty string for the key of the package.");
	}
	const String key_hex = String::hex_encode_buffer(key.ptr(), key.size());

	for (const KeyValue<String, PckPackage::Entry> &kv : p_package->get_entry_map()) {
		ERR_FAIL_COND_V_MSG(kv.value.delta, ERR_UNAVAILABLE, vformat("\"res://%s\" is a delta patch: packs with delta patches can't be rebuilt.", kv.key));
	}

	// Writing over the source pack would destroy the data that is still being read: write a temporary file first.
	const bool same_file = !p_package->get_path().is_empty() && _same_pack_path(p_package->get_path(), p_target_path);
	const String write_path = same_file ? p_target_path + ".tmp" : p_target_path;

	Ref<PCKPacker> packer;
	packer.instantiate();
	Error err = packer->pck_start(write_path, p_package->get_alignment(), key_hex, p_package->is_encrypted_directory());
	if (err != OK) {
		return err;
	}
	for (const KeyValue<String, PckPackage::Entry> &kv : p_package->get_entry_map()) {
		if (kv.value.removal) {
			err = packer->add_file_removal(kv.key);
		} else {
			Vector<uint8_t> data;
			err = p_package->read_entry(kv.key, data);
			if (err == OK) {
				err = packer->add_file_from_buffer(kv.key, data, kv.value.encrypted);
			}
		}
		if (err != OK) {
			break;
		}
	}
	const Error flush_err = packer->flush();
	packer.unref();
	if (err == OK) {
		err = flush_err;
	}
	if (err != OK) {
		DirAccess::remove_absolute(write_path);
		return err;
	}

	if (same_file) {
		if (DirAccess::rename_absolute(write_path, p_target_path) != OK) {
			// Some platforms don't replace an existing file on rename.
			DirAccess::remove_absolute(p_target_path);
			err = DirAccess::rename_absolute(write_path, p_target_path);
			ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't replace pack \"%s\" with the new pack \"%s\".", p_target_path, write_path));
		}
		// The offsets of the package point into the old file: read the new one.
		err = p_package->open_pack(p_target_path, key_hex);
	}
	return err;
}

Error PckPackageServer::mount(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data || packed_data->is_disabled()) {
		return ERR_UNAVAILABLE;
	}
	// `res://` as a directory source is reserved for the engine.
	ERR_FAIL_COND_V_MSG(p_path.is_empty() || p_path == "res://", ERR_INVALID_PARAMETER, vformat("Invalid pack path \"%s\".", p_path));

	// Like `ProjectSettings.load_resource_pack()`, a missing or unknown file is reported by the return value only,
	// so that games can probe for optional packs.
	bool ok = false;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings) {
		ok = project_settings->_load_resource_pack(p_path, p_replace_files, int(p_offset), false);
	} else {
		ok = packed_data->add_pack(p_path, p_replace_files, p_offset) == OK;
	}
	if (!ok) {
		return FileAccess::exists(p_path) ? ERR_FILE_UNRECOGNIZED : ERR_FILE_NOT_FOUND;
	}
	_emit_deferred(SNAME("pack_mounted"), p_path);
	return OK;
}

Error PckPackageServer::unmount(const String &p_path) {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_UNAVAILABLE, "PckPackageServer.unmount() must be called on the main thread.");
	PackedData *packed_data = PackedData::get_singleton();
	ERR_FAIL_NULL_V(packed_data, ERR_UNAVAILABLE);

	Vector<String> recorded;
	bool main_pack = false;
	for (const PackedData::PackInfo &info : packed_data->get_packs()) {
		if (info.path != "res://" && _same_pack_path(info.path, p_path)) {
			if (!recorded.has(info.path)) {
				recorded.push_back(info.path);
			}
			main_pack = main_pack || info.main_pack;
		}
	}
	ERR_FAIL_COND_V_MSG(recorded.is_empty(), ERR_DOES_NOT_EXIST, vformat("Pack \"%s\" is not mounted.", p_path));
	ERR_FAIL_COND_V_MSG(main_pack, ERR_UNAVAILABLE, vformat("Pack \"%s\" is the main pack of the project and can't be unmounted.", p_path));

	Error replay_err = OK;
	for (const String &path : recorded) {
		const Error err = packed_data->remove_pack(path);
		if (err != OK) {
			replay_err = err;
		}
	}
	if (replay_err != OK) {
		WARN_PRINT(vformat("Pack \"%s\" was unmounted, but at least one of the remaining packs could not be read again (error %d). Its files are no longer available.", p_path, replay_err));
	}

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings && project_settings->is_project_loaded()) {
		// The pack may have declared global classes.
		project_settings->refresh_global_class_list();
	}
	_emit_deferred(SNAME("pack_unmounted"), recorded[0]);
	return OK;
}

bool PckPackageServer::is_mounted(const String &p_path) const {
	const PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data) {
		return false;
	}
	for (const PackedData::PackInfo &info : packed_data->get_packs()) {
		if (info.path != "res://" && _same_pack_path(info.path, p_path)) {
			return true;
		}
	}
	return false;
}

bool PckPackageServer::is_main_pack(const String &p_path) const {
	const PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data) {
		return false;
	}
	for (const PackedData::PackInfo &info : packed_data->get_packs()) {
		if (info.main_pack && _same_pack_path(info.path, p_path)) {
			return true;
		}
	}
	return false;
}

PackedStringArray PckPackageServer::get_mounted() const {
	PackedStringArray result;
	const PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data) {
		return result;
	}
	for (const PackedData::PackInfo &info : packed_data->get_packs()) {
		if (info.path != "res://" && !result.has(info.path)) {
			result.push_back(info.path);
		}
	}
	return result;
}

String PckPackageServer::get_pack_of(const String &p_res_path) const {
	PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data || packed_data->is_disabled()) {
		return String();
	}
	const String pack = packed_data->get_pack_of(p_res_path);
	// Files of the project folder (editor, Android without a pack) don't come from a pack.
	return pack == "res://" ? String() : pack;
}

void PckPackageServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "key", "offset"), &PckPackageServer::open, DEFVAL(String()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("read_file", "package", "path"), &PckPackageServer::read_file);
	ClassDB::bind_method(D_METHOD("verify", "package"), &PckPackageServer::verify);
	ClassDB::bind_method(D_METHOD("build", "package", "target_path", "key"), &PckPackageServer::build, DEFVAL(String()));

	ClassDB::bind_method(D_METHOD("mount", "path", "replace_files", "offset"), &PckPackageServer::mount, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("unmount", "path"), &PckPackageServer::unmount);
	ClassDB::bind_method(D_METHOD("is_mounted", "path"), &PckPackageServer::is_mounted);
	ClassDB::bind_method(D_METHOD("is_main_pack", "path"), &PckPackageServer::is_main_pack);
	ClassDB::bind_method(D_METHOD("get_mounted"), &PckPackageServer::get_mounted);
	ClassDB::bind_method(D_METHOD("get_pack_of", "res_path"), &PckPackageServer::get_pack_of);

	ADD_SIGNAL(MethodInfo("pack_mounted", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("pack_unmounted", PropertyInfo(Variant::STRING, "path")));
}

PckPackageServer::PckPackageServer() {
	singleton = this;
}

PckPackageServer::~PckPackageServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
