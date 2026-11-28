/**************************************************************************/
/*  path_prefix_server.cpp                                                */
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

#include "path_prefix_server.h"

#include "core/config/project_settings.h"
#include "core/io/file_access_pack.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/pair.h"

/////////////////////////////////////////////////////////////////////////////
// PathPrefix

String PathPrefix::globalize_path(const String &p_prefix, const String &p_path) const {
	String ret;
	GDVIRTUAL_CALL(_globalize, p_path, ret);
	return ret;
}

String PathPrefix::localize_path(const String &p_prefix, const String &p_global_path) const {
	String ret;
	GDVIRTUAL_CALL(_localize, p_global_path, ret);
	return ret;
}

bool PathPrefix::is_available() const {
	bool ret = true;
	GDVIRTUAL_CALL(_is_available, ret);
	return ret;
}

bool PathPrefix::is_read_only() const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_read_only, ret);
	return ret;
}

void PathPrefix::_bind_methods() {
	GDVIRTUAL_BIND(_globalize, "path");
	GDVIRTUAL_BIND(_localize, "global_path");
	GDVIRTUAL_BIND(_is_available);
	GDVIRTUAL_BIND(_is_read_only);
}

namespace {

// Maps `p_global_path` below `p_root` to `p_prefix` + rest. Case-insensitive where the file system is.
String _localize_below_root(const String &p_prefix, const String &p_global_path, const String &p_root) {
	String root = p_root.replace_char('\\', '/');
	if (root.is_empty() || root.contains("://")) {
		return String();
	}
	if (root.length() > 1 && root.ends_with("/") && !root.ends_with(":/")) {
		root = root.substr(0, root.length() - 1);
	}
	const String root_slash = root.ends_with("/") ? root : root + "/";
#if defined(WINDOWS_ENABLED) || defined(MACOS_ENABLED)
	const String path = p_global_path.to_lower();
	if (path == root.to_lower()) {
		return p_prefix;
	}
	const bool match = path.begins_with(root_slash.to_lower());
#else
	if (p_global_path == root) {
		return p_prefix;
	}
	const bool match = p_global_path.begins_with(root_slash);
#endif
	return match ? p_prefix + p_global_path.substr(root_slash.length()) : String();
}

// Wraps the callables given to `register_prefix_callable()`.
class PathPrefixCallable : public PathPrefix {
	GDSOFTCLASS(PathPrefixCallable, PathPrefix);

public:
	Callable globalize_callable;
	Callable localize_callable;
	Callable is_available_callable;

	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		return globalize_callable.call(p_path);
	}

	virtual String localize_path(const String &p_prefix, const String &p_global_path) const override {
		if (!localize_callable.is_valid()) {
			return String();
		}
		return localize_callable.call(p_global_path);
	}

	virtual bool is_available() const override {
		if (!is_available_callable.is_valid()) {
			return true;
		}
		return is_available_callable.call();
	}

	virtual bool is_folder() const override { return localize_callable.is_valid(); }
};

// Maps a prefix to one folder: a fixed folder (which may itself use a prefix), an OS path or a system folder.
class PathPrefixFolder : public PathPrefix {
	GDSOFTCLASS(PathPrefixFolder, PathPrefix);

public:
	enum Mode {
		MODE_FOLDER,
		MODE_OS_PATH,
		MODE_SYSTEM_DIR,
	};

	Mode mode = MODE_FOLDER;
	String folder;
	PathPrefixServer::OSPath os_path = PathPrefixServer::OS_PATH_DATA;
	::OS::SystemDir system_dir = ::OS::SYSTEM_DIR_DOCUMENTS;
	bool shared_storage = true;

	String get_root() const {
		String root;
		switch (mode) {
			case MODE_FOLDER: {
				// Folders that point at each other (`a://` -> `b://` -> `a://`) would recurse without end:
				// a folder logic that is already resolving its root on this thread has no root.
				static thread_local const PathPrefixFolder *resolving[8] = {};
				static thread_local int resolving_count = 0;
				for (int i = 0; i < resolving_count; i++) {
					if (resolving[i] == this) {
						return String();
					}
				}
				if (resolving_count >= 8) {
					return String();
				}
				root = folder;
				if (PathPrefixServer::get_singleton()) {
					resolving[resolving_count++] = this;
					root = PathPrefixServer::get_singleton()->globalize(folder);
					resolving_count--;
				}
			} break;
			case MODE_OS_PATH: {
				switch (os_path) {
					case PathPrefixServer::OS_PATH_DATA:
						root = ::OS::get_singleton()->get_data_path();
						break;
					case PathPrefixServer::OS_PATH_CONFIG:
						root = ::OS::get_singleton()->get_config_path();
						break;
					case PathPrefixServer::OS_PATH_CACHE:
						root = ::OS::get_singleton()->get_cache_path();
						break;
					case PathPrefixServer::OS_PATH_TEMP:
						root = ::OS::get_singleton()->get_temp_path();
						break;
				}
			} break;
			case MODE_SYSTEM_DIR: {
				root = ::OS::get_singleton()->get_system_dir(system_dir, shared_storage);
			} break;
		}
		root = root.replace_char('\\', '/');
		while (root.length() > 1 && root.ends_with("/") && !root.ends_with(":/")) {
			root = root.substr(0, root.length() - 1);
		}
		return root;
	}

	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		const String root = get_root();
		if (root.is_empty()) {
			return String();
		}
		const String rest = p_path.substr(p_prefix.length());
		return rest.is_empty() ? root : root.path_join(rest);
	}

	virtual String forward_path(const String &p_prefix, const String &p_path) const override {
		if (mode != MODE_FOLDER) {
			return globalize_path(p_prefix, p_path);
		}
		// The folder as written (`res://streaming_assets`), not globalized: the server follows the chain.
		String root = folder;
		while (root.length() > 1 && root.ends_with("/") && !root.ends_with("://") && !root.ends_with(":/")) {
			root = root.substr(0, root.length() - 1);
		}
		const String rest = p_path.substr(p_prefix.length());
		return rest.is_empty() ? root : root.path_join(rest);
	}

	virtual String localize_path(const String &p_prefix, const String &p_global_path) const override {
		return _localize_below_root(p_prefix, p_global_path, get_root());
	}

	virtual bool is_available() const override {
		return !get_root().is_empty();
	}
};

// The locked engine prefixes. Their resolution is the one of the engine before this server existed.
class PathPrefixBuiltin : public PathPrefix {
	GDSOFTCLASS(PathPrefixBuiltin, PathPrefix);

public:
	enum Kind {
		KIND_RES,
		KIND_USER,
		KIND_UID,
		KIND_PIPE,
		KIND_EXEC,
	};

	Kind kind = KIND_RES;

	String get_builtin_root() const {
		switch (kind) {
			case KIND_RES: {
				return ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->get_resource_path() : String();
			}
			case KIND_USER: {
				return ::OS::get_singleton()->get_user_data_dir();
			}
			case KIND_EXEC: {
#ifdef TOOLS_ENABLED
				// In the editor (and in games started from it) `exec://` is the project folder.
				return ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->get_resource_path() : String();
#else
				String dir = ::OS::get_singleton()->get_executable_path().replace_char('\\', '/').get_base_dir();
				if (dir.ends_with(".app/Contents/MacOS")) {
					// macOS: the folder that contains the app bundle, so that portable files live next to `Foo.app`.
					dir = dir.get_base_dir().get_base_dir().get_base_dir();
				}
				return dir;
#endif
			}
			default: {
				return String();
			}
		}
	}

	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		switch (kind) {
			case KIND_RES:
			case KIND_USER:
			case KIND_UID: {
				return ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(p_path) : String();
			}
			case KIND_PIPE: {
#ifdef WINDOWS_ENABLED
				return "\\\\.\\pipe\\LOCAL\\" + p_path.substr(p_prefix.length());
#else
				return "/tmp/" + p_path.substr(p_prefix.length());
#endif
			}
			case KIND_EXEC: {
				const String root = get_builtin_root();
				if (root.is_empty()) {
					return String();
				}
				const String rest = p_path.substr(p_prefix.length());
				return rest.is_empty() ? root : root.path_join(rest);
			}
		}
		return String();
	}

	virtual String localize_path(const String &p_prefix, const String &p_global_path) const override {
		if (!is_folder() || !is_available()) {
			return String();
		}
		return _localize_below_root(p_prefix, p_global_path, get_builtin_root());
	}

	virtual bool is_available() const override {
		switch (kind) {
			case KIND_PIPE: {
#ifdef WEB_ENABLED
				return false;
#else
				return true;
#endif
			}
			case KIND_EXEC: {
#if defined(WEB_ENABLED) || defined(ANDROID_ENABLED) || defined(APPLE_EMBEDDED_ENABLED) || defined(IOS_ENABLED)
				// No folder next to the executable: APK, app bundle or browser.
				return false;
#else
				return true;
#endif
			}
			default: {
				return true;
			}
		}
	}

	virtual bool is_read_only() const override {
#ifdef TOOLS_ENABLED
		return false;
#else
		// Exported builds read `res://` from the PCK or the app bundle.
		return kind == KIND_RES || kind == KIND_UID;
#endif
	}

	virtual bool is_folder() const override {
		return kind == KIND_RES || kind == KIND_USER || kind == KIND_EXEC;
	}

	virtual bool is_opaque() const override {
		return kind == KIND_UID;
	}
};

// Splits the part after the prefix into segments and resolves `.` and `..`.
// Returns false if `..` leaves the root of the prefix.
bool _normalize_rest(const String &p_rest, String &r_out) {
	Vector<String> parts = p_rest.split("/", false);
	Vector<String> out;
	for (const String &part : parts) {
		if (part == ".") {
			continue;
		}
		if (part == "..") {
			if (out.is_empty()) {
				return false;
			}
			out.resize(out.size() - 1);
			continue;
		}
		out.push_back(part);
	}
	r_out = String("/").join(out);
	return true;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// PathPrefixServer

PathPrefixServer *PathPrefixServer::singleton = nullptr;

PathPrefixServer *PathPrefixServer::get_singleton() {
	return singleton;
}

bool PathPrefixServer::split_prefix(const String &p_path, String &r_prefix, String &r_rest) {
	const int p = p_path.find("://");
	if (p <= 0) {
		return false;
	}
	for (int i = 0; i < p; i++) {
		const char32_t c = p_path[i];
		if (!is_ascii_alphanumeric_char(c) && c != '_') {
			return false;
		}
	}
	r_prefix = p_path.substr(0, p).to_lower() + "://";
	r_rest = p_path.substr(p + 3);
	return true;
}

String PathPrefixServer::normalize_prefix(const String &p_prefix) {
	String prefix = p_prefix.strip_edges();
	if (!prefix.ends_with("://")) {
		prefix += "://";
	}
	return prefix;
}

bool PathPrefixServer::is_valid_prefix_name(const String &p_prefix) {
	const String prefix = normalize_prefix(p_prefix);
	const String name = prefix.substr(0, prefix.length() - 3);
	if (name.is_empty() || !is_ascii_lower_case(name[0])) {
		return false;
	}
	for (int i = 0; i < name.length(); i++) {
		const char32_t c = name[i];
		if (!is_ascii_lower_case(c) && !is_digit(c) && c != '_') {
			return false;
		}
	}
	// URL schemes the engine uses elsewhere.
	static const char *reserved[] = { "http", "https", "file", "ws", "wss", "ftp", "data", nullptr };
	for (int i = 0; reserved[i]; i++) {
		if (name == reserved[i]) {
			return false;
		}
	}
	return true;
}

bool PathPrefixServer::is_engine_fast_path(const String &p_path) {
	return p_path.begins_with("res://") || p_path.begins_with("user://") || p_path.begins_with("uid://") || p_path.begins_with("pipe://");
}

bool PathPrefixServer::_platforms_match(const PackedStringArray &p_platforms) {
	if (p_platforms.is_empty()) {
		return true;
	}
	for (const String &tag : p_platforms) {
		if (::OS::get_singleton()->has_feature(tag)) {
			return true;
		}
	}
	return false;
}

bool PathPrefixServer::_same_platforms(const PackedStringArray &p_a, const PackedStringArray &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	for (const String &tag : p_a) {
		if (!p_b.has(tag)) {
			return false;
		}
	}
	return true;
}

void PathPrefixServer::_update_active(Entry &r_entry) {
	// A registration for named platforms beats one for all platforms; among equals, the last one wins.
	Ref<PathPrefix> generic;
	Ref<PathPrefix> specific;
	for (const Registration &reg : r_entry.registrations) {
		if (!_platforms_match(reg.platforms)) {
			continue;
		}
		if (reg.platforms.is_empty()) {
			generic = reg.logic;
		} else {
			specific = reg.logic;
		}
	}
	r_entry.active = specific.is_valid() ? specific : generic;
}

Ref<PathPrefix> PathPrefixServer::_get_active(const String &p_prefix, bool *r_locked) const {
	RWLockRead read_lock(lock);
	const Entry *entry = prefixes.getptr(p_prefix);
	if (!entry) {
		return Ref<PathPrefix>();
	}
	if (r_locked) {
		*r_locked = entry->locked;
	}
	return entry->active;
}

void PathPrefixServer::_emit_deferred(const StringName &p_signal, const String &p_prefix) {
	if (Thread::is_main_thread()) {
		emit_signal(p_signal, p_prefix);
	} else {
		call_deferred(SNAME("emit_signal"), p_signal, p_prefix);
	}
}

Error PathPrefixServer::_register(const String &p_prefix, const Ref<PathPrefix> &p_logic, const PackedStringArray &p_platforms, bool p_locked) {
	ERR_FAIL_COND_V_MSG(p_logic.is_null(), ERR_INVALID_PARAMETER, "The logic of a path prefix must not be null.");
	const String prefix = normalize_prefix(p_prefix);
	ERR_FAIL_COND_V_MSG(!p_locked && !is_valid_prefix_name(prefix), ERR_INVALID_PARAMETER, vformat("Invalid path prefix '%s': use lowercase letters, digits and '_', followed by '://'. URL schemes such as 'http://' are reserved.", p_prefix));
	// Errors are printed after the lock is released: printing takes the global lock,
	// which other threads may hold while resolving paths through this server.
	bool was_locked = false;
	{
		RWLockWrite write_lock(lock);
		Entry *entry = prefixes.getptr(prefix);
		if (entry && entry->locked) {
			was_locked = true;
		} else {
			if (!entry) {
				Entry new_entry;
				new_entry.locked = p_locked;
				new_entry.order = next_order++;
				entry = &prefixes.insert(prefix, new_entry)->value;
			}
			bool replaced = false;
			for (Registration &reg : entry->registrations) {
				if (_same_platforms(reg.platforms, p_platforms)) {
					reg.logic = p_logic;
					replaced = true;
					break;
				}
			}
			if (!replaced) {
				Registration reg;
				reg.platforms = p_platforms;
				reg.logic = p_logic;
				entry->registrations.push_back(reg);
			}
			_update_active(*entry);
		}
	}
	ERR_FAIL_COND_V_MSG(was_locked, ERR_LOCKED, vformat("The path prefix '%s' is an engine prefix and cannot be changed.", prefix));
	if (!p_locked) {
		_emit_deferred(SNAME("prefix_registered"), prefix);
	}
	return OK;
}

Error PathPrefixServer::register_prefix(const String &p_prefix, const Ref<PathPrefix> &p_path_prefix, const PackedStringArray &p_platforms) {
	return _register(p_prefix, p_path_prefix, p_platforms, false);
}

Error PathPrefixServer::register_prefix_callable(const String &p_prefix, const Callable &p_globalize, const PackedStringArray &p_platforms, const Callable &p_localize, const Callable &p_is_available) {
	ERR_FAIL_COND_V_MSG(!p_globalize.is_valid(), ERR_INVALID_PARAMETER, "The globalize callable of a path prefix must be valid.");
	Ref<PathPrefixCallable> logic;
	logic.instantiate();
	logic->globalize_callable = p_globalize;
	logic->localize_callable = p_localize;
	logic->is_available_callable = p_is_available;
	return _register(p_prefix, logic, p_platforms, false);
}

Error PathPrefixServer::register_prefix_folder(const String &p_prefix, const String &p_folder, const PackedStringArray &p_platforms) {
	ERR_FAIL_COND_V_MSG(p_folder.is_empty(), ERR_INVALID_PARAMETER, "The folder of a path prefix must not be empty.");
	const String prefix = normalize_prefix(p_prefix);
	ERR_FAIL_COND_V_MSG(is_prefix_locked(prefix), ERR_LOCKED, vformat("The path prefix '%s' is an engine prefix and cannot be changed.", prefix));
	ERR_FAIL_COND_V_MSG(p_folder.to_lower().begins_with(prefix), ERR_INVALID_PARAMETER, vformat("The folder of the path prefix '%s' must not use the same prefix.", prefix));
	return _register(prefix, create_folder_prefix(p_folder), p_platforms, false);
}

Error PathPrefixServer::unregister_prefix(const String &p_prefix, const PackedStringArray &p_platforms) {
	const String prefix = normalize_prefix(p_prefix);
	// Errors are printed after the lock is released (see _register()).
	enum UnregisterResult {
		UNREGISTER_OK,
		UNREGISTER_MISSING,
		UNREGISTER_LOCKED,
		UNREGISTER_NO_PLATFORMS,
	};
	UnregisterResult result = UNREGISTER_OK;
	{
		RWLockWrite write_lock(lock);
		Entry *entry = prefixes.getptr(prefix);
		if (!entry) {
			result = UNREGISTER_MISSING;
		} else if (entry->locked) {
			result = UNREGISTER_LOCKED;
		} else if (p_platforms.is_empty()) {
			prefixes.erase(prefix);
		} else {
			bool found = false;
			for (int i = 0; i < entry->registrations.size(); i++) {
				if (_same_platforms(entry->registrations[i].platforms, p_platforms)) {
					entry->registrations.remove_at(i);
					found = true;
					break;
				}
			}
			if (!found) {
				result = UNREGISTER_NO_PLATFORMS;
			} else if (entry->registrations.is_empty()) {
				prefixes.erase(prefix);
			} else {
				_update_active(*entry);
			}
		}
	}
	ERR_FAIL_COND_V_MSG(result == UNREGISTER_MISSING, ERR_DOES_NOT_EXIST, vformat("The path prefix '%s' is not registered.", prefix));
	ERR_FAIL_COND_V_MSG(result == UNREGISTER_LOCKED, ERR_LOCKED, vformat("The path prefix '%s' is an engine prefix and cannot be unregistered.", prefix));
	ERR_FAIL_COND_V_MSG(result == UNREGISTER_NO_PLATFORMS, ERR_DOES_NOT_EXIST, vformat("The path prefix '%s' has no registration for these platforms.", prefix));
	_emit_deferred(SNAME("prefix_unregistered"), prefix);
	return OK;
}

bool PathPrefixServer::has_prefix(const String &p_prefix) const {
	RWLockRead read_lock(lock);
	return prefixes.has(normalize_prefix(p_prefix).to_lower());
}

bool PathPrefixServer::is_prefix_available(const String &p_prefix) const {
	Ref<PathPrefix> logic = _get_active(normalize_prefix(p_prefix).to_lower());
	return logic.is_valid() && logic->is_available();
}

bool PathPrefixServer::is_prefix_locked(const String &p_prefix) const {
	RWLockRead read_lock(lock);
	const Entry *entry = prefixes.getptr(normalize_prefix(p_prefix).to_lower());
	return entry && entry->locked;
}

bool PathPrefixServer::is_prefix_read_only(const String &p_prefix) const {
	Ref<PathPrefix> logic = _get_active(normalize_prefix(p_prefix).to_lower());
	return logic.is_valid() && logic->is_read_only();
}

PackedStringArray PathPrefixServer::get_prefixes() const {
	RWLockRead read_lock(lock);
	// Sorted by registration order: the engine prefixes first.
	Vector<Pair<uint32_t, String>> sorted;
	for (const KeyValue<String, Entry> &kv : prefixes) {
		sorted.push_back(Pair<uint32_t, String>(kv.value.order, kv.key));
	}
	sorted.sort_custom<PairSort<uint32_t, String>>();
	PackedStringArray ret;
	for (const Pair<uint32_t, String> &item : sorted) {
		ret.push_back(item.second);
	}
	return ret;
}

bool PathPrefixServer::handles_path(const String &p_path) const {
	if (is_engine_fast_path(p_path)) {
		return false;
	}
	String prefix;
	String rest;
	if (!split_prefix(p_path, prefix, rest)) {
		return false;
	}
	return _get_active(prefix).is_valid();
}

String PathPrefixServer::globalize(const String &p_path) const {
	String prefix;
	String rest;
	if (!split_prefix(p_path, prefix, rest)) {
		return p_path;
	}
	Ref<PathPrefix> logic = _get_active(prefix);
	if (logic.is_null() || !logic->is_available()) {
		return p_path;
	}
	// The logic is called without holding the lock, so that it may call back into this server.
	String ret = logic->globalize_path(prefix, prefix + rest.replace_char('\\', '/'));
	// A logic may return a path with another prefix (`user://mods/a.txt`): resolve that one too.
	for (int depth = 0; depth < 8; depth++) {
		String next_prefix;
		String next_rest;
		if (!split_prefix(ret, next_prefix, next_rest) || next_prefix == prefix) {
			break;
		}
		Ref<PathPrefix> next = _get_active(next_prefix);
		if (next.is_null() || !next->is_available()) {
			break;
		}
		prefix = next_prefix;
		ret = next->globalize_path(prefix, prefix + next_rest);
	}
	return ret;
}

String PathPrefixServer::resolve_engine_path(const String &p_path) const {
	if (is_engine_fast_path(p_path)) {
		return p_path;
	}
	String prefix;
	String rest;
	if (!split_prefix(p_path, prefix, rest)) {
		return p_path;
	}
	String path = prefix + rest.replace_char('\\', '/');
	// The same limits as in `globalize()`: at most 8 steps, and a step must change the prefix.
	// Nothing is printed and no lock is held while a logic runs.
	for (int depth = 0; depth < 8; depth++) {
		Ref<PathPrefix> logic = _get_active(prefix);
		if (logic.is_null() || !logic->is_available()) {
			return p_path;
		}
		const String next = logic->forward_path(prefix, path);
		if (next.begins_with("res://") || next.begins_with("user://")) {
			return next.simplify_path();
		}
		if (next.begins_with("uid://")) {
			return next;
		}
		String next_prefix;
		String next_rest;
		if (!split_prefix(next, next_prefix, next_rest) || next_prefix == prefix || is_engine_fast_path(next)) {
			// An OS path, the same prefix again or `pipe://`: not an engine path.
			return p_path;
		}
		prefix = next_prefix;
		path = next_prefix + next_rest;
	}
	return p_path;
}

Error PathPrefixServer::check_writable(const String &p_path) const {
	String prefix;
	String rest;
	if (is_engine_fast_path(p_path) || !split_prefix(p_path, prefix, rest)) {
		return OK;
	}
	const Ref<PathPrefix> logic = _get_active(prefix);
	ERR_FAIL_COND_V_MSG(logic.is_valid() && logic->is_read_only(), ERR_FILE_NO_PERMISSION, vformat("Cannot write to '%s': the prefix '%s' is read-only.", p_path, prefix));
	return OK;
}

String PathPrefixServer::localize(const String &p_global_path) const {
	const String path = p_global_path.replace_char('\\', '/').simplify_path();

	struct Candidate {
		uint32_t order = 0;
		String prefix;
		Ref<PathPrefix> logic;
	};
	Vector<Candidate> candidates;
	{
		RWLockRead read_lock(lock);
		for (const KeyValue<String, Entry> &kv : prefixes) {
			if (kv.value.active.is_valid()) {
				Candidate c;
				c.order = kv.value.order;
				c.prefix = kv.key;
				c.logic = kv.value.active;
				candidates.push_back(c);
			}
		}
	}

	// The prefix with the deepest root (the shortest rest) wins; on a tie the earlier registration wins,
	// so `res://` beats `exec://` in the editor where both are the project folder.
	String best;
	int best_rest = INT_MAX;
	uint32_t best_order = UINT32_MAX;
	for (const Candidate &c : candidates) {
		if (!c.logic->is_folder() || !c.logic->is_available()) {
			continue;
		}
		const String local = c.logic->localize_path(c.prefix, path);
		if (local.is_empty() || !local.begins_with(c.prefix)) {
			continue;
		}
		const int rest = local.length() - c.prefix.length();
		if (rest < best_rest || (rest == best_rest && c.order < best_order)) {
			best = local;
			best_rest = rest;
			best_order = c.order;
		}
	}
	return best;
}

Ref<PathInfo> PathPrefixServer::resolve(const String &p_path) const {
	Ref<PathInfo> info;
	info.instantiate();
	info->source_path = p_path;
	if (p_path.is_empty()) {
		info->_set_error(ERR_INVALID_PARAMETER, "The path is empty.");
		return info;
	}

	String path = p_path.replace_char('\\', '/');
	String prefix;
	String rest;
	String global_input;
	if (!split_prefix(path, prefix, rest)) {
		if (path.is_absolute_path()) {
			// A global path: find the prefix it belongs to.
			global_input = path.simplify_path();
			const String local = localize(global_input);
			if (local.is_empty()) {
				// Not below any prefix: still a valid OS path, but without a local form.
				info->valid = true;
				info->error = OK;
				info->global_path = global_input;
				info->available = true;
				return info;
			}
			split_prefix(local, prefix, rest);
		} else {
			// A relative path is relative to `res://`, like in `ProjectSettings.localize_path()`.
			prefix = "res://";
			rest = path;
		}
	}

	bool locked = false;
	Ref<PathPrefix> logic = _get_active(prefix, &locked);
	info->prefix = prefix;
	info->locked = locked;
	if (logic.is_null()) {
		if (has_prefix(prefix)) {
			info->_set_error(ERR_UNAVAILABLE, vformat("The path prefix '%s' is not available on this platform.", prefix));
		} else {
			info->_set_error(ERR_FILE_BAD_PATH, vformat("Unknown path prefix '%s'.", prefix));
		}
		return info;
	}
	if (!logic->is_available()) {
		info->_set_error(ERR_UNAVAILABLE, vformat("The path prefix '%s' is not available on this platform.", prefix));
		return info;
	}
	info->available = true;
	info->read_only = logic->is_read_only();

	String relative = rest;
	if (!logic->is_opaque()) {
		if (!_normalize_rest(rest, relative)) {
			info->_set_error(ERR_FILE_BAD_PATH, vformat("The path '%s' leaves the root of the prefix '%s'.", p_path, prefix));
			return info;
		}
	}
	info->relative_path = relative;
	info->local_path = prefix + relative;

	String global = global_input.is_empty() ? globalize(info->local_path) : global_input;
	if (prefix == "res://" || prefix == "uid://") {
		// No OS path for files that come from a PCK.
		const String res_path = prefix == "uid://" ? ResourceUID::uid_to_path(info->local_path) : info->local_path;
		if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && PackedData::get_singleton()->has_path(res_path)) {
			global = String();
		}
	}
	info->global_path = global.replace_char('\\', '/');
	info->valid = true;
	info->error = OK;
	return info;
}

Ref<PathInfo> PathPrefixServer::resolve_safe(const String &p_path, AccessMode p_access, const PackedStringArray &p_allowed_roots) const {
	const String path = p_path.replace_char('\\', '/');
	String prefix;
	String rest;
	if (!split_prefix(path, prefix, rest) || path.split("/").has("..")) {
		Ref<PathInfo> info;
		info.instantiate();
		info->source_path = p_path;
		if (path.split("/").has("..")) {
			info->_set_error(ERR_FILE_BAD_PATH, vformat("The path '%s' contains a '..' segment.", p_path));
		} else {
			info->_set_error(ERR_FILE_BAD_PATH, vformat("The path '%s' has no prefix. Untrusted paths must start with a registered prefix such as 'user://'.", p_path));
		}
		return info;
	}

	Ref<PathInfo> info = resolve(path);
	info->source_path = p_path;
	if (!info->valid) {
		info->error = ERR_FILE_BAD_PATH;
		return info;
	}

	if (!p_allowed_roots.is_empty()) {
		bool allowed = false;
		for (const String &root : p_allowed_roots) {
			String root_prefix;
			String root_rest;
			String root_relative;
			if (!split_prefix(root.replace_char('\\', '/'), root_prefix, root_rest) || !_normalize_rest(root_rest, root_relative)) {
				continue;
			}
			if (root_prefix != info->prefix) {
				continue;
			}
			if (root_relative.is_empty() || info->relative_path == root_relative || info->relative_path.begins_with(root_relative + "/")) {
				allowed = true;
				break;
			}
		}
		if (!allowed) {
			info->_set_error(ERR_FILE_BAD_PATH, vformat("The path '%s' is outside the allowed roots.", p_path));
			return info;
		}
	}

	if (p_access == ACCESS_WRITE && info->read_only) {
		info->_set_error(ERR_FILE_NO_PERMISSION, vformat("The path '%s' is read-only.", p_path));
		return info;
	}
	return info;
}

Ref<PathPrefix> PathPrefixServer::create_folder_prefix(const String &p_folder) const {
	Ref<PathPrefixFolder> logic;
	logic.instantiate();
	logic->mode = PathPrefixFolder::MODE_FOLDER;
	logic->folder = p_folder.replace_char('\\', '/');
	return logic;
}

Ref<PathPrefix> PathPrefixServer::create_system_dir_prefix(CoreBind::OS::SystemDir p_dir, bool p_shared_storage) const {
	Ref<PathPrefixFolder> logic;
	logic.instantiate();
	logic->mode = PathPrefixFolder::MODE_SYSTEM_DIR;
	logic->system_dir = ::OS::SystemDir(p_dir);
	logic->shared_storage = p_shared_storage;
	return logic;
}

Ref<PathPrefix> PathPrefixServer::create_os_path_prefix(OSPath p_path) const {
	Ref<PathPrefixFolder> logic;
	logic.instantiate();
	logic->mode = PathPrefixFolder::MODE_OS_PATH;
	logic->os_path = p_path;
	return logic;
}

void PathPrefixServer::_register_builtins() {
	const PathPrefixBuiltin::Kind kinds[] = { PathPrefixBuiltin::KIND_RES, PathPrefixBuiltin::KIND_USER, PathPrefixBuiltin::KIND_UID, PathPrefixBuiltin::KIND_PIPE, PathPrefixBuiltin::KIND_EXEC };
	const char *names[] = { "res://", "user://", "uid://", "pipe://", "exec://" };
	for (int i = 0; i < 5; i++) {
		Ref<PathPrefixBuiltin> logic;
		logic.instantiate();
		logic->kind = kinds[i];
		_register(names[i], logic, PackedStringArray(), true);
	}
}

void PathPrefixServer::register_default_prefixes() {
	if (defaults_registered) {
		return;
	}
	defaults_registered = true;
	if (!bool(GLOBAL_GET("filesystem/path_prefixes/defaults"))) {
		return;
	}
#ifndef WEB_ENABLED
	// On the Web these folders live in memory and are lost on reload; only `user://` persists there.
	register_prefix("appdata://", create_os_path_prefix(OS_PATH_DATA));
	register_prefix("config://", create_os_path_prefix(OS_PATH_CONFIG));
	register_prefix("cache://", create_os_path_prefix(OS_PATH_CACHE));
#endif
	register_prefix("temp://", create_os_path_prefix(OS_PATH_TEMP));
}

void PathPrefixServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_prefix", "prefix", "path_prefix", "platforms"), &PathPrefixServer::register_prefix, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("register_prefix_callable", "prefix", "globalize", "platforms", "localize", "is_available"), &PathPrefixServer::register_prefix_callable, DEFVAL(PackedStringArray()), DEFVAL(Callable()), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("register_prefix_folder", "prefix", "folder", "platforms"), &PathPrefixServer::register_prefix_folder, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("unregister_prefix", "prefix", "platforms"), &PathPrefixServer::unregister_prefix, DEFVAL(PackedStringArray()));

	ClassDB::bind_method(D_METHOD("has_prefix", "prefix"), &PathPrefixServer::has_prefix);
	ClassDB::bind_method(D_METHOD("is_prefix_available", "prefix"), &PathPrefixServer::is_prefix_available);
	ClassDB::bind_method(D_METHOD("is_prefix_locked", "prefix"), &PathPrefixServer::is_prefix_locked);
	ClassDB::bind_method(D_METHOD("is_prefix_read_only", "prefix"), &PathPrefixServer::is_prefix_read_only);
	ClassDB::bind_method(D_METHOD("get_prefixes"), &PathPrefixServer::get_prefixes);

	ClassDB::bind_method(D_METHOD("globalize", "path"), &PathPrefixServer::globalize);
	ClassDB::bind_method(D_METHOD("localize", "global_path"), &PathPrefixServer::localize);
	ClassDB::bind_method(D_METHOD("resolve_engine_path", "path"), &PathPrefixServer::resolve_engine_path);
	ClassDB::bind_method(D_METHOD("resolve", "path"), &PathPrefixServer::resolve);
	ClassDB::bind_method(D_METHOD("resolve_safe", "path", "access", "allowed_roots"), &PathPrefixServer::resolve_safe, DEFVAL(PackedStringArray()));

	ClassDB::bind_method(D_METHOD("create_folder_prefix", "folder"), &PathPrefixServer::create_folder_prefix);
	ClassDB::bind_method(D_METHOD("create_system_dir_prefix", "dir", "shared_storage"), &PathPrefixServer::create_system_dir_prefix, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("create_os_path_prefix", "path"), &PathPrefixServer::create_os_path_prefix);

	ADD_SIGNAL(MethodInfo("prefix_registered", PropertyInfo(Variant::STRING, "prefix")));
	ADD_SIGNAL(MethodInfo("prefix_unregistered", PropertyInfo(Variant::STRING, "prefix")));

	BIND_ENUM_CONSTANT(OS_PATH_DATA);
	BIND_ENUM_CONSTANT(OS_PATH_CONFIG);
	BIND_ENUM_CONSTANT(OS_PATH_CACHE);
	BIND_ENUM_CONSTANT(OS_PATH_TEMP);

	BIND_ENUM_CONSTANT(ACCESS_READ);
	BIND_ENUM_CONSTANT(ACCESS_WRITE);
}

PathPrefixServer::PathPrefixServer() {
	singleton = this;
	_register_builtins();
}

PathPrefixServer::~PathPrefixServer() {
	singleton = nullptr;
}
