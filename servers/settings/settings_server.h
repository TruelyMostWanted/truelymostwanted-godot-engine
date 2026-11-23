/**************************************************************************/
/*  settings_server.h                                                     */
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
#include "core/object/gdvirtual.gen.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

// Base of all settings servers (one engine singleton per domain: audio, language, video, ...).
// Keeps three copies of the domain's settings resource: the defaults, the applied state and the
// pending (edited) state. Loads and saves the player's file in `user://settings/`, pushes changed
// values to the engine and reads them back. Domains derive from it in C++, GDScript, C# or GDExtension
// and register with SettingsServerManager, which applies them at startup.
// Main-thread API: call it from the main thread only; signals are emitted on the main thread.
class SettingsServer : public Object {
	GDCLASS(SettingsServer, Object);

public:
	enum ApplyMode {
		APPLY_MODE_IMMEDIATE,
		APPLY_MODE_MANUAL,
	};

	// When SettingsServerManager applies the stored settings at startup (main/main.cpp).
	enum ApplyPhase {
		APPLY_PHASE_PRE_DISPLAY, // After the servers are registered, before the DisplayServer exists.
		APPLY_PHASE_SERVERS, // After all servers and script languages are initialized.
		APPLY_PHASE_SCENE, // After the SceneTree and the root window exist, before the main scene is loaded.
		APPLY_PHASE_MAX,
	};

	enum SettingFlags {
		SETTING_FLAG_NONE = 0,
		SETTING_FLAG_RESTART_REQUIRED = 1, // The engine reads the value only at startup.
		SETTING_FLAG_CONFIRM = 2, // Changes go through apply_with_confirmation().
		SETTING_FLAG_NO_ENGINE_TARGET = 4, // Stored for the game only; never pushed to the engine.
	};

private:
	enum BindingKind {
		BINDING_NONE,
		BINDING_OBJECT,
		BINDING_SINGLETON,
		BINDING_CALLBACKS,
	};

	struct Binding {
		BindingKind kind = BINDING_NONE;
		ObjectID target;
		StringName singleton;
		StringName property;
		Callable setter;
		Callable getter;
		BitField<SettingFlags> flags = SETTING_FLAG_NONE;
	};

	HashMap<StringName, Binding> bindings;
	HashMap<StringName, Variant> session_overrides;

	Ref<Resource> settings; // Pending: what set_value() changes.
	Ref<Resource> applied; // What the engine has now.
	Ref<Resource> defaults;

	ApplyMode apply_mode = APPLY_MODE_IMMEDIATE;
	String file_path;
	PackedStringArray restart_values;

	bool initialized = false;
	bool started = false;
	bool in_startup = false;
	bool save_queued = false;
	bool unsaved = false;
	bool format_warned = false;

	bool confirmation_active = false;
	Ref<Resource> confirmation_backup;
	uint64_t confirmation_deadline_msec = 0;

	void _ensure_initialized() const;
	void _initialize();
	Ref<Resource> _effective_settings() const;
	bool _object_has_value(const Ref<Resource> &p_settings, const StringName &p_name) const;
	void _push_values(const PackedStringArray &p_names);
	void _retrieve_into(const Ref<Resource> &p_settings);
	Variant _validate(const StringName &p_name, const Variant &p_value) const;
	bool _has_flag(const PackedStringArray &p_names, SettingFlags p_flag) const;
	PackedStringArray _get_changed_names(const Ref<Resource> &p_a, const Ref<Resource> &p_b) const;
	void _record_restart_values(const PackedStringArray &p_names);
	void _queue_autosave();
	void _set_confirmation_processing(bool p_enabled);
	void _on_process_frame();

protected:
	static void _bind_methods();

	// Overridable in C++. The default implementations call the script and GDExtension virtuals below.
	virtual Ref<Resource> _create_default_settings() const;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names);
	virtual void _retrieve_settings(const Ref<Resource> &p_settings);
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const;
	virtual Dictionary _migrate_settings(int p_from_version, const Dictionary &p_data);
	// File extension used when no file path is set: `ini` or `json` (ListSettingsServer: also `csv`).
	virtual String _get_default_extension();

	GDVIRTUAL0RC(StringName, _get_domain)
	GDVIRTUAL0RC(int, _get_format_version)
	GDVIRTUAL0RC(int, _get_apply_phase)
	GDVIRTUAL0RC(Ref<Resource>, _create_settings)
	GDVIRTUAL2(_apply, Ref<Resource>, PackedStringArray)
	GDVIRTUAL1(_retrieve, Ref<Resource>)
	GDVIRTUAL2RC(Variant, _validate_value, StringName, Variant)
	GDVIRTUAL2R(Dictionary, _migrate, int, Dictionary)
	GDVIRTUAL1RC(bool, _is_value_supported, StringName)

	// For ListSettingsServer: the pending value was changed in place; emits `value_changed` and applies it in APPLY_MODE_IMMEDIATE.
	void notify_value_changed(const StringName &p_name);
	// Pending settings without starting the lazy initialization again.
	Ref<Resource> get_pending_settings() const;

public:
	// Stored values of a settings resource (no `resource_*`, `script` or `metadata/*`).
	static void get_value_infos(const Object *p_object, LocalVector<PropertyInfo> &r_infos);
	static Variant copy_value(const Variant &p_value);
	static bool values_equal(const Variant &p_a, const Variant &p_b);
	static Ref<Resource> copy_settings(const Ref<Resource> &p_settings);

	static void register_settings();
	static String get_storage_folder();
	static String get_storage_format();

	// Identity. C++ domains override these; scripts implement _get_domain() and the others.
	virtual StringName get_domain() const;
	virtual int get_format_version() const;
	virtual ApplyPhase get_apply_phase() const;

	// Values.
	Ref<Resource> get_settings() const;
	Ref<Resource> get_applied_settings() const;
	Ref<Resource> get_default_settings() const;
	PackedStringArray get_setting_names() const;
	bool has_setting(const StringName &p_name) const;
	void set_value(const StringName &p_name, const Variant &p_value);
	Variant get_value(const StringName &p_name) const;
	Variant get_applied_value(const StringName &p_name) const;
	Variant get_default_value(const StringName &p_name) const;
	virtual bool is_value_supported(const StringName &p_name) const;

	// Bindings: setting -> engine target.
	void bind_setting_property(const StringName &p_name, Object *p_target, const StringName &p_property, BitField<SettingFlags> p_flags = SETTING_FLAG_NONE);
	void bind_setting_singleton_property(const StringName &p_name, const StringName &p_singleton, const StringName &p_property, BitField<SettingFlags> p_flags = SETTING_FLAG_NONE);
	void bind_setting_callbacks(const StringName &p_name, const Callable &p_setter, const Callable &p_getter, BitField<SettingFlags> p_flags = SETTING_FLAG_NONE);
	void unbind_setting(const StringName &p_name);
	bool is_setting_bound(const StringName &p_name) const;
	void set_setting_flags(const StringName &p_name, BitField<SettingFlags> p_flags);
	BitField<SettingFlags> get_setting_flags(const StringName &p_name) const;

	// State.
	void set_apply_mode(ApplyMode p_mode);
	ApplyMode get_apply_mode() const;
	bool is_dirty() const;
	PackedStringArray get_dirty_values() const;
	PackedStringArray get_restart_values() const;
	void revert();
	void reset_value(const StringName &p_name);
	void reset_to_defaults();

	// Session overrides (for example from the command line): pushed to the engine, never saved.
	void set_session_override(const StringName &p_name, const Variant &p_value);
	void clear_session_override(const StringName &p_name);
	bool has_session_override(const StringName &p_name) const;

	// Engine.
	Error apply();
	Error apply_with_confirmation(double p_timeout_sec = -1.0);
	void confirm_pending();
	void cancel_pending();
	bool is_confirmation_pending() const;
	double get_confirmation_time_left() const;
	void retrieve();

	// Storage.
	Error load();
	Error save();
	void set_file_path(const String &p_path);
	String get_file_path() const;
	bool has_unsaved_changes() const;

	// Called by SettingsServerManager at startup: captures the defaults, loads the file and applies it.
	void startup();
	bool is_started() const;
	// Runs a save queued by autosave now (SettingsServerManager calls it on exit).
	void flush_autosave();

	SettingsServer();
	~SettingsServer();
};

VARIANT_ENUM_CAST(SettingsServer::ApplyMode);
VARIANT_ENUM_CAST(SettingsServer::ApplyPhase);
VARIANT_BITFIELD_CAST(SettingsServer::SettingFlags);
