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
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "core/templates/hash_set.h"

PCKPackageServer *PCKPackageServer::singleton = nullptr;

PCKPackageServer *PCKPackageServer::get_singleton() {
	return singleton;
}

static bool _same_pack_path(const String &p_a, const String &p_b) {
	if (p_a == p_b || p_a.simplify_path() == p_b.simplify_path()) {
		return true;
	}
	// `user://a.pck` and the OS path of the same file.
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	return project_settings && project_settings->globalize_path(p_a).simplify_path() == project_settings->globalize_path(p_b).simplify_path();
}

void PCKPackageServer::_emit_deferred(const StringName &p_signal, const String &p_path) {
	if (Thread::is_main_thread()) {
		emit_signal(p_signal, p_path);
	} else {
		call_deferred(SNAME("emit_signal"), p_signal, p_path);
	}
}

Ref<PCKPackage> PCKPackageServer::open(const String &p_path, const String &p_key, uint64_t p_offset) {
	Ref<PCKPackage> package;
	package.instantiate();
	if (package->open_pack(p_path, p_key, p_offset) != OK) {
		return Ref<PCKPackage>();
	}
	return package;
}

Vector<uint8_t> PCKPackageServer::read_file(const Ref<PCKPackage> &p_package, const String &p_path) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), Vector<uint8_t>(), "The package is null.");
	Vector<uint8_t> data;
	if (p_package->read_entry(p_path, data) != OK) {
		return Vector<uint8_t>();
	}
	return data;
}

Error PCKPackageServer::verify(const Ref<PCKPackage> &p_package) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), ERR_INVALID_PARAMETER, "The package is null.");
	ERR_FAIL_COND_V_MSG(p_package->is_sparse_bundle(), ERR_UNAVAILABLE, vformat("Pack \"%s\" is a sparse bundle: its files are stored next to the pack and can't be verified here.", p_package->get_pack_path()));
	for (const KeyValue<String, PCKPackage::Entry> &kv : p_package->get_entry_map()) {
		const PCKPackage::Entry &entry = kv.value;
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
		ERR_FAIL_COND_V_MSG(memcmp(hash, entry.md5, 16) != 0, ERR_FILE_CORRUPT, vformat("The MD5 sum of \"res://%s\" in pack \"%s\" does not match: the pack is corrupt.", kv.key, p_package->get_pack_path()));
	}
	return OK;
}

Error PCKPackageServer::build(const Ref<PCKPackage> &p_package, const String &p_target_path, const String &p_key) {
	ERR_FAIL_COND_V_MSG(p_package.is_null(), ERR_INVALID_PARAMETER, "The package is null.");
	ERR_FAIL_COND_V_MSG(p_target_path.is_empty(), ERR_INVALID_PARAMETER, "The target path of the pack is empty.");
	ERR_FAIL_COND_V_MSG(is_mounted(p_target_path), ERR_BUSY, vformat("Pack \"%s\" is mounted: unmount it before building over it.", p_target_path));

	Vector<uint8_t> key;
	if (p_key.is_empty() && !p_package->get_key().is_empty()) {
		key = p_package->get_key();
	} else {
		ERR_FAIL_COND_V_MSG(PCKPackage::decode_key(p_key, key) != OK, ERR_INVALID_PARAMETER, "Invalid pack key: use 64 hexadecimal characters, or an empty string for the key of the package.");
	}
	const String key_hex = String::hex_encode_buffer(key.ptr(), key.size());

	PackedStringArray missing_sources;
	for (const KeyValue<String, PCKPackage::Entry> &kv : p_package->get_entry_map()) {
		ERR_FAIL_COND_V_MSG(kv.value.delta, ERR_UNAVAILABLE, vformat("\"res://%s\" is a delta patch: packs with delta patches can't be rebuilt.", kv.key));
		if (!kv.value.removal && kv.value.source_type == PCKPackage::SOURCE_FILE && !FileAccess::exists(kv.value.source_path)) {
			missing_sources.push_back(kv.value.source_path);
		}
	}
	// A description may name files that don't exist yet: report all missing sources before anything is written.
	ERR_FAIL_COND_V_MSG(!missing_sources.is_empty(), ERR_FILE_NOT_FOUND, vformat("Can't build pack \"%s\": %d source files are missing: %s.", p_target_path, missing_sources.size(), String(", ").join(missing_sources)));

	// Writing over the source pack would destroy the data that is still being read: write a temporary file first.
	const bool same_file = !p_package->get_pack_path().is_empty() && _same_pack_path(p_package->get_pack_path(), p_target_path);
	const bool same_base = !p_package->get_base_pack().is_empty() && _same_pack_path(p_package->get_base_pack(), p_target_path);
	const String write_path = (same_file || same_base) ? p_target_path + ".tmp" : p_target_path;

	Ref<PCKPacker> packer;
	packer.instantiate();
	Error err = packer->pck_start(write_path, p_package->get_alignment(), key_hex, p_package->is_encrypted_directory());
	if (err != OK) {
		return err;
	}
	const int total = p_package->get_entry_map().size();
	int done = 0;
	for (const KeyValue<String, PCKPackage::Entry> &kv : p_package->get_entry_map()) {
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
		_emit_progress(SNAME("build_progress"), "res://" + kv.key, ++done, total);
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

	if (same_file || same_base) {
		// The cached directory of the base pack points into the old file.
		p_package->release_base_pack();
		if (DirAccess::rename_absolute(write_path, p_target_path) != OK) {
			// Some platforms don't replace an existing file on rename.
			DirAccess::remove_absolute(p_target_path);
			err = DirAccess::rename_absolute(write_path, p_target_path);
			ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't replace pack \"%s\" with the new pack \"%s\".", p_target_path, write_path));
		}
		if (same_file) {
			// The offsets of the package point into the old file: read the new one.
			err = p_package->open_pack(p_target_path, key_hex);
		}
	}
	return err;
}

// Paths in a pack are stored without `res://`; filters may use either form.
static String _extract_key(const String &p_path) {
	return p_path.trim_prefix("res://").trim_prefix("/");
}

static bool _extract_filter_matches(const String &p_key, const Vector<String> &p_filters) {
	if (p_filters.is_empty()) {
		return true;
	}
	for (const String &filter : p_filters) {
		if (filter.is_empty()) {
			return true;
		}
		if (filter.contains_char('*') || filter.contains_char('?')) {
			if (p_key.match(filter)) {
				return true;
			}
		} else if (filter.ends_with("/")) {
			if (p_key.begins_with(filter)) {
				return true;
			}
		} else if (p_key == filter || p_key.begins_with(filter + "/")) {
			return true;
		}
	}
	return false;
}

// `x.png.import` and `x.gd.uid` belong to `x.png` and `x.gd`.
static String _extract_sidecar_source(const String &p_key) {
	if (p_key.ends_with(".import")) {
		return p_key.trim_suffix(".import");
	}
	if (p_key.ends_with(".uid")) {
		return p_key.trim_suffix(".uid");
	}
	return String();
}

// A free name next to `p_path`: `name_2.ext`, `name_3.ext`, … Sidecars of the new name must be free too.
static String _extract_free_path(const String &p_path, const HashSet<String> &p_planned) {
	const String dir = p_path.get_base_dir();
	const String file = p_path.get_file();
	const String extension = file.get_extension();
	const String stem = extension.is_empty() ? file : file.get_basename();
	for (int i = 2; i < 100000; i++) {
		const String candidate = dir.path_join(stem + "_" + itos(i) + (extension.is_empty() ? String() : "." + extension));
		if (!p_planned.has(candidate) && !FileAccess::exists(candidate) && !FileAccess::exists(candidate + ".import") && !FileAccess::exists(candidate + ".uid")) {
			return candidate;
		}
	}
	return String();
}

// Finds the `uid="uid://…"` attribute in the header line of a text resource, or the `uid="…"` line of an
// `.import` file. Returns the UID text and the range to remove, or an empty string.
static String _extract_find_uid(const String &p_text, bool p_import_file, int &r_from, int &r_to) {
	if (p_import_file) {
		const int pos = p_text.find("\nuid=\"");
		if (pos < 0) {
			return String();
		}
		const int end = p_text.find_char('"', pos + 6);
		if (end < 0) {
			return String();
		}
		int line_end = p_text.find_char('\n', end);
		line_end = line_end < 0 ? p_text.length() : line_end;
		// Remove the leading newline and the line; the newline after the line stays.
		r_from = pos;
		r_to = line_end;
		return p_text.substr(pos + 6, end - pos - 6);
	}
	if (!p_text.begins_with("[gd_")) {
		return String();
	}
	int header_end = p_text.find_char('\n');
	header_end = header_end < 0 ? p_text.length() : header_end;
	const int pos = p_text.find(" uid=\"");
	if (pos < 0 || pos > header_end) {
		return String();
	}
	const int end = p_text.find_char('"', pos + 6);
	if (end < 0 || end > header_end) {
		return String();
	}
	r_from = pos;
	r_to = end + 1;
	return p_text.substr(pos + 6, end - pos - 6);
}

void PCKPackageServer::_emit_progress(const StringName &p_signal, const String &p_path, int p_done, int p_total) {
	if (Thread::is_main_thread()) {
		emit_signal(p_signal, p_path, p_done, p_total);
	} else {
		call_deferred(SNAME("emit_signal"), p_signal, p_path, p_done, p_total);
	}
}

Ref<PCKExtractResult> PCKPackageServer::extract(const Ref<PCKPackage> &p_package, const String &p_target_dir, const PackedStringArray &p_paths, ExtractPolicy p_policy, bool p_skip_engine_files) {
	Ref<PCKExtractResult> result;
	result.instantiate();
	if (p_package.is_null()) {
		result->_add_error(ERR_INVALID_PARAMETER, "The package is null.");
		ERR_FAIL_V_MSG(result, "The package is null.");
	}
	if (p_target_dir.is_empty()) {
		result->_add_error(ERR_INVALID_PARAMETER, "The target folder is empty.");
		ERR_FAIL_V_MSG(result, "The target folder is empty.");
	}
	if (p_policy < EXTRACT_SKIP || p_policy > EXTRACT_RENAME) {
		result->_add_error(ERR_INVALID_PARAMETER, vformat("Invalid extract policy %d.", int(p_policy)));
		ERR_FAIL_V_MSG(result, vformat("Invalid extract policy %d.", int(p_policy)));
	}
	if (p_package->is_sparse_bundle()) {
		const String message = vformat("Pack \"%s\" is a sparse bundle: its files are stored next to the pack and can't be extracted here.", p_package->get_pack_path());
		result->_add_error(ERR_UNAVAILABLE, message);
		ERR_FAIL_V_MSG(result, message);
	}

	Vector<String> filters;
	for (const String &path : p_paths) {
		filters.push_back(_extract_key(path));
	}

	const HashMap<String, PCKPackage::Entry> &entries = p_package->get_entry_map();

	// 1. Select and classify. Main files first decide where they go; sidecars follow them in step 2.
	HashSet<String> matched;
	for (const KeyValue<String, PCKPackage::Entry> &kv : entries) {
		if (_extract_filter_matches(kv.key, filters)) {
			matched.insert(kv.key);
		}
	}
	// A selected file brings its `.import` and `.uid` files along.
	Vector<String> selected;
	HashSet<String> selected_set;
	for (const KeyValue<String, PCKPackage::Entry> &kv : entries) {
		if (matched.has(kv.key) || matched.has(_extract_sidecar_source(kv.key))) {
			selected.push_back(kv.key);
			selected_set.insert(kv.key);
		}
	}
	const int total = selected.size();

	enum Action {
		ACTION_WRITE,
		ACTION_SKIP,
		ACTION_NOT_EDITABLE,
	};
	struct Plan {
		Action action = ACTION_SKIP;
		String target;
		String source_key; // For sidecars: the key of their source file.
		String listed_path; // For ACTION_NOT_EDITABLE: the path to list.
	};
	HashMap<String, Plan> plans;
	HashSet<String> planned_targets;
	const String own_pack = p_package->get_pack_path();

	// A sidecar is a sidecar only if its source is selected and is a file of its own.
	auto is_sidecar = [&](const String &p_key) -> String {
		const String source = _extract_sidecar_source(p_key);
		if (!source.is_empty() && selected_set.has(source) && _extract_sidecar_source(source).is_empty()) {
			return source;
		}
		return String();
	};

	for (const String &key : selected) {
		const PCKPackage::Entry &entry = entries[key];
		Plan plan;
		if (entry.removal) {
			plan.action = ACTION_SKIP;
		} else if (entry.delta) {
			plan.action = ACTION_NOT_EDITABLE;
			plan.listed_path = "res://" + key;
		} else if (p_skip_engine_files && key.begins_with(".godot/")) {
			plan.action = ACTION_SKIP;
		} else if (p_skip_engine_files && key == "project.binary") {
			plan.action = ACTION_NOT_EDITABLE;
			plan.listed_path = "res://" + key;
		} else if (p_skip_engine_files && key.ends_with(".remap")) {
			// The converted file lives below `.godot/`: list the original resource.
			plan.action = ACTION_NOT_EDITABLE;
			plan.listed_path = "res://" + key.trim_suffix(".remap");
		} else if (!is_sidecar(key).is_empty()) {
			plan.source_key = is_sidecar(key);
		} else if (p_skip_engine_files && !_extract_sidecar_source(key).is_empty()) {
			// A `.import` or `.uid` file without its source can't be used.
			plan.action = ACTION_SKIP;
		} else {
			String target = p_target_dir.path_join(key);
			if (!own_pack.is_empty() && _same_pack_path(target, own_pack)) {
				// Writing over the pack would destroy the data that is still being read.
				result->_add_error(ERR_BUSY, vformat("\"res://%s\" would overwrite the pack \"%s\" itself.", key, own_pack));
				plan.action = ACTION_SKIP;
			} else if (FileAccess::exists(target) || planned_targets.has(target)) {
				if (p_policy == EXTRACT_OVERWRITE && !planned_targets.has(target)) {
					plan.action = ACTION_WRITE;
				} else if (p_policy == EXTRACT_RENAME) {
					target = _extract_free_path(target, planned_targets);
					plan.action = target.is_empty() ? ACTION_SKIP : ACTION_WRITE;
				} else {
					plan.action = ACTION_SKIP;
				}
			} else {
				plan.action = ACTION_WRITE;
			}
			if (plan.action == ACTION_WRITE) {
				plan.target = target;
				planned_targets.insert(target);
			}
		}
		plans.insert(key, plan);
	}

	// 2. Sidecars take over the decision of their source.
	for (KeyValue<String, Plan> &kv : plans) {
		if (kv.value.source_key.is_empty()) {
			continue;
		}
		const Plan *source_plan = plans.getptr(kv.value.source_key);
		if (source_plan && source_plan->action == ACTION_WRITE) {
			kv.value.action = ACTION_WRITE;
			kv.value.target = source_plan->target + kv.key.substr(kv.value.source_key.length());
		} else {
			kv.value.action = ACTION_SKIP;
		}
	}

	// 3. Write in pack order.
	ResourceUID *uids = ResourceUID::get_singleton();
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	int done = 0;
	for (const String &key : selected) {
		const Plan &plan = *plans.getptr(key);
		const String pack_path = "res://" + key;
		if (plan.action == ACTION_SKIP) {
			result->skipped.push_back(pack_path);
		} else if (plan.action == ACTION_NOT_EDITABLE) {
			result->not_editable.push_back(plan.listed_path);
		} else {
			Vector<uint8_t> data;
			Error err = p_package->read_entry(key, data);
			if (err != OK) {
				result->_add_error(err, vformat("Can't read \"%s\" from the pack.", pack_path));
				done++;
				_emit_progress(SNAME("extract_progress"), pack_path, done, total);
				continue;
			}

			// The file the UID belongs to: the source for sidecars, else the file itself.
			const String uid_owner = plan.source_key.is_empty() ? plan.target : plans.getptr(plan.source_key)->target;
			const String uid_owner_local = project_settings ? project_settings->localize_path(uid_owner) : uid_owner;
			ResourceUID::ID kept_uid = ResourceUID::INVALID_ID;
			bool drop_file = false;
			bool binary_collision = false;
			if (uids) {
				const bool uid_file = key.ends_with(".uid") && !plan.source_key.is_empty();
				const bool import_file = key.ends_with(".import") && !plan.source_key.is_empty();
				const bool text_resource = key.ends_with(".tscn") || key.ends_with(".tres");
				const bool binary_resource = key.ends_with(".scn") || key.ends_with(".res");
				if (uid_file || import_file || text_resource) {
					String text = String::utf8((const char *)data.ptr(), data.size());
					String uid_text;
					int from = 0;
					int to = 0;
					if (uid_file) {
						uid_text = text.strip_edges();
					} else {
						uid_text = _extract_find_uid(text, import_file, from, to);
					}
					const ResourceUID::ID id = uid_text.is_empty() ? ResourceUID::INVALID_ID : uids->text_to_id(uid_text);
					if (id != ResourceUID::INVALID_ID) {
						if (uids->has_id(id) && uids->get_id_path(id) != uid_owner_local) {
							result->dropped_uids.push_back(uid_text);
							if (uid_file) {
								drop_file = true;
							} else {
								// The editor assigns a new UID on the next scan.
								text = text.substr(0, from) + text.substr(to);
								data = text.to_utf8_buffer();
							}
						} else {
							kept_uid = id;
						}
					}
				} else if (binary_resource) {
					binary_collision = true; // Checked after writing: the binary header is read by the loader.
				}
			}

			if (drop_file) {
				result->skipped.push_back(pack_path);
			} else {
				err = DirAccess::make_dir_recursive_absolute(plan.target.get_base_dir());
				if (err != OK && err != ERR_ALREADY_EXISTS) {
					result->_add_error(err, vformat("Can't create the folder \"%s\".", plan.target.get_base_dir()));
				} else {
					Ref<FileAccess> f = FileAccess::open(plan.target, FileAccess::WRITE, &err);
					if (f.is_null()) {
						result->_add_error(err == OK ? ERR_FILE_CANT_WRITE : err, vformat("Can't write \"%s\".", plan.target));
					} else {
						const bool stored = f->store_buffer(data);
						f.unref();
						if (!stored) {
							result->_add_error(ERR_FILE_CANT_WRITE, vformat("Can't write \"%s\".", plan.target));
						} else {
							result->written.push_back(plan.target);
							if (plan.target != p_target_dir.path_join(key)) {
								result->renamed[pack_path] = plan.target;
							}
							if (binary_collision) {
								const ResourceUID::ID id = ResourceLoader::get_resource_uid(plan.target);
								if (id != ResourceUID::INVALID_ID) {
									if (uids->has_id(id) && uids->get_id_path(id) != uid_owner_local) {
										result->dropped_uids.push_back(uids->id_to_text(id));
										if (ResourceSaver::set_uid(plan.target, uids->create_id()) != OK) {
											result->_add_error(ERR_FILE_CANT_WRITE, vformat("Can't give \"%s\" a new UID.", plan.target));
										}
									} else {
										kept_uid = id;
									}
								}
							}
							// Later files and `uid://` paths see the UID at once, before the next editor scan.
							if (kept_uid != ResourceUID::INVALID_ID && !uids->has_id(kept_uid)) {
								uids->add_id(kept_uid, uid_owner_local);
							}
						}
					}
				}
			}
		}
		done++;
		_emit_progress(SNAME("extract_progress"), pack_path, done, total);
	}
	return result;
}

Error PCKPackageServer::mount(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data || packed_data->is_disabled()) {
		return ERR_UNAVAILABLE;
	}
	// Like `ProjectSettings.load_resource_pack()`, errors are reported by the return value only,
	// so that games can probe for optional packs. `res://` as a directory source is reserved for the engine.
	if (p_path.is_empty() || p_path == "res://") {
		return ERR_INVALID_PARAMETER;
	}
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (p_replace_files && project_settings) {
		// A pack inside `res://` that lists its own path would replace itself and recurse on every open.
		const String local_path = project_settings->localize_path(p_path);
		if (local_path.begins_with("res://") && FileAccess::exists(p_path)) {
			Ref<PCKPackage> probe;
			probe.instantiate();
			if (probe->open_pack(p_path, String(), p_offset) == OK && probe->has_file(local_path)) {
				ERR_FAIL_V_MSG(ERR_CYCLIC_LINK, vformat("Pack \"%s\" contains its own path: mounting it with replace_files would make it replace itself.", p_path));
			}
		}
	}
	bool ok = false;
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

Error PCKPackageServer::unmount(const String &p_path) {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_UNAVAILABLE, "PCKPackageServer.unmount() must be called on the main thread.");
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
		WARN_PRINT(vformat("Pack \"%s\" was unmounted, but at least one of the remaining packs could not be read again (%s). Its files are no longer available.", p_path, error_names[replay_err]));
	}

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings && project_settings->is_project_loaded()) {
		// The pack may have declared global classes.
		project_settings->refresh_global_class_list();
	}
	_emit_deferred(SNAME("pack_unmounted"), recorded[0]);
	return OK;
}

bool PCKPackageServer::is_mounted(const String &p_path) const {
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

bool PCKPackageServer::is_main_pack(const String &p_path) const {
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

PackedStringArray PCKPackageServer::get_mounted_packs() const {
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

String PCKPackageServer::get_providing_pack(const String &p_res_path) const {
	PackedData *packed_data = PackedData::get_singleton();
	if (!packed_data || packed_data->is_disabled()) {
		return String();
	}
	const String pack = packed_data->get_pack_of(p_res_path);
	// Files of the project folder (editor, Android without a pack) don't come from a pack.
	return pack == "res://" ? String() : pack;
}

void PCKPackageServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "key", "offset"), &PCKPackageServer::open, DEFVAL(String()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("read_file", "package", "path"), &PCKPackageServer::read_file);
	ClassDB::bind_method(D_METHOD("verify", "package"), &PCKPackageServer::verify);
	ClassDB::bind_method(D_METHOD("build", "package", "target_path", "key"), &PCKPackageServer::build, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("extract", "package", "target_dir", "paths", "policy", "skip_engine_files"), &PCKPackageServer::extract, DEFVAL(PackedStringArray()), DEFVAL(EXTRACT_SKIP), DEFVAL(true));

	ClassDB::bind_method(D_METHOD("mount", "path", "replace_files", "offset"), &PCKPackageServer::mount, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("unmount", "path"), &PCKPackageServer::unmount);
	ClassDB::bind_method(D_METHOD("is_mounted", "path"), &PCKPackageServer::is_mounted);
	ClassDB::bind_method(D_METHOD("is_main_pack", "path"), &PCKPackageServer::is_main_pack);
	ClassDB::bind_method(D_METHOD("get_mounted_packs"), &PCKPackageServer::get_mounted_packs);
	ClassDB::bind_method(D_METHOD("get_providing_pack", "res_path"), &PCKPackageServer::get_providing_pack);

	ADD_SIGNAL(MethodInfo("pack_mounted", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("pack_unmounted", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("build_progress", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "done"), PropertyInfo(Variant::INT, "total")));
	ADD_SIGNAL(MethodInfo("extract_progress", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "done"), PropertyInfo(Variant::INT, "total")));

	BIND_ENUM_CONSTANT(EXTRACT_SKIP);
	BIND_ENUM_CONSTANT(EXTRACT_OVERWRITE);
	BIND_ENUM_CONSTANT(EXTRACT_RENAME);
}

PCKPackageServer::PCKPackageServer() {
	singleton = this;
}

PCKPackageServer::~PCKPackageServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
