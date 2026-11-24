/**************************************************************************/
/*  path_prefix_server.h                                                  */
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

#include "core/core_bind.h"
#include "core/io/path_info.h"
#include "core/object/gdvirtual.gen.h"
#include "core/object/ref_counted.h"
#include "core/os/rw_lock.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

// The logic of one path prefix: turns a prefixed path into a real path and back.
// Scripts and GDExtensions override the virtual methods. The engine uses C++
// subclasses for the built-in prefixes and the ready-made resolvers.
class PathPrefix : public RefCounted {
	GDCLASS(PathPrefix, RefCounted);

protected:
	static void _bind_methods();

	GDVIRTUAL1RC(String, _globalize, String)
	GDVIRTUAL1RC(String, _localize, String)
	GDVIRTUAL0RC(bool, _is_available)
	GDVIRTUAL0RC(bool, _is_read_only)

public:
	// `p_prefix` is the canonical prefix (`mods://`), `p_path` the canonical prefixed path (`mods://a/b.txt`).
	virtual String globalize_path(const String &p_prefix, const String &p_path) const;
	// Returns the canonical prefixed path, or an empty string if `p_global_path` does not belong to this prefix.
	virtual String localize_path(const String &p_prefix, const String &p_global_path) const;
	virtual bool is_available() const;
	virtual bool is_read_only() const;
	// True if the prefix maps to a folder, so that `globalize()` and `localize()` are inverse.
	virtual bool is_folder() const { return true; }
	// True if paths below this prefix are opaque IDs (`uid://`): no `.`/`..` normalization.
	virtual bool is_opaque() const { return false; }
	// One step of the prefix chain, used by `PathPrefixServer::resolve_engine_path()`. May return a path
	// with another prefix. Logics that resolve their target themselves (the folder logic) override it to
	// return the unresolved target, so that the chain can stop at `res://` or `user://`. Not bound.
	virtual String forward_path(const String &p_prefix, const String &p_path) const { return globalize_path(p_prefix, p_path); }
};

class PathPrefixServer : public Object {
	GDCLASS(PathPrefixServer, Object);

	// Registers the locked `streaming://` prefix.
	friend class StreamingAssetsServer;

public:
	enum OSPath {
		OS_PATH_DATA,
		OS_PATH_CONFIG,
		OS_PATH_CACHE,
		OS_PATH_TEMP,
	};

private:
	static PathPrefixServer *singleton;

	struct Registration {
		PackedStringArray platforms;
		Ref<PathPrefix> logic;
	};

	struct Entry {
		bool locked = false;
		uint32_t order = 0;
		Vector<Registration> registrations;
		// The registration that matches this platform, or null.
		Ref<PathPrefix> active;
	};

	mutable RWLock lock;
	HashMap<String, Entry> prefixes;
	uint32_t next_order = 0;
	bool defaults_registered = false;

	static bool _platforms_match(const PackedStringArray &p_platforms);
	static bool _same_platforms(const PackedStringArray &p_a, const PackedStringArray &p_b);
	static void _update_active(Entry &r_entry);

	Ref<PathPrefix> _get_active(const String &p_prefix, bool *r_locked = nullptr) const;
	Error _register(const String &p_prefix, const Ref<PathPrefix> &p_logic, const PackedStringArray &p_platforms, bool p_locked);
	void _emit_deferred(const StringName &p_signal, const String &p_prefix);
	void _register_builtins();

protected:
	static void _bind_methods();

public:
	static PathPrefixServer *get_singleton();

	// Splits `p_path` into a canonical prefix (`user://`) and the rest. Returns false if there is no `xxx://` prefix.
	static bool split_prefix(const String &p_path, String &r_prefix, String &r_rest);
	// Prefix syntax: lowercase letters, digits and `_`, then `://`. A bare name (`mods`) is accepted and completed.
	static String normalize_prefix(const String &p_prefix);
	static bool is_valid_prefix_name(const String &p_prefix);
	// True for the four engine prefixes that keep their hard-coded fast path in FileAccess, DirAccess and ProjectSettings.
	static bool is_engine_fast_path(const String &p_path);

	Error register_prefix(const String &p_prefix, const Ref<PathPrefix> &p_logic, const PackedStringArray &p_platforms = PackedStringArray());
	Error register_prefix_callable(const String &p_prefix, const Callable &p_globalize, const PackedStringArray &p_platforms = PackedStringArray(), const Callable &p_localize = Callable(), const Callable &p_is_available = Callable());
	Error register_prefix_folder(const String &p_prefix, const String &p_folder, const PackedStringArray &p_platforms = PackedStringArray());
	Error unregister_prefix(const String &p_prefix, const PackedStringArray &p_platforms = PackedStringArray());

	bool has_prefix(const String &p_prefix) const;
	bool is_prefix_available(const String &p_prefix) const;
	bool is_prefix_locked(const String &p_prefix) const;
	bool is_prefix_read_only(const String &p_prefix) const;
	PackedStringArray get_prefixes() const;

	// True if `p_path` starts with a registered prefix that is not one of the engine fast-path prefixes.
	bool handles_path(const String &p_path) const;

	String globalize(const String &p_path) const;
	String localize(const String &p_global_path) const;
	// Follows the prefix chain of `p_path` until it reaches `res://`, `user://` or `uid://`, so that
	// FileAccess, DirAccess and ResourceLoader can use their engine access (including the PCK).
	// Returns `p_path` unchanged if the chain does not reach one of them.
	String resolve_engine_path(const String &p_path) const;

	Ref<PathInfo> resolve(const String &p_path) const;
	Ref<PathInfo> resolve_safe(const String &p_path, bool p_for_write, const PackedStringArray &p_allowed_roots = PackedStringArray()) const;

	Ref<PathPrefix> create_folder_logic(const String &p_folder) const;
	Ref<PathPrefix> create_system_dir_logic(CoreBind::OS::SystemDir p_dir, bool p_shared_storage = true) const;
	Ref<PathPrefix> create_os_path_logic(OSPath p_path) const;

	// Registers `appdata://`, `config://`, `cache://` and `temp://` if `filesystem/path_prefixes/defaults` is on.
	// Called once after the project settings are loaded.
	void register_default_prefixes();

	PathPrefixServer();
	~PathPrefixServer();
};

VARIANT_ENUM_CAST(PathPrefixServer::OSPath);
