/**************************************************************************/
/*  settings_server_manager.h                                             */
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

#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "servers/settings/settings_server.h"

// Registry of all settings servers (one per domain) and entry point for the startup phases in
// main/main.cpp, "load all", "save all" and "reset all".
class SettingsServerManager : public Object {
	GDCLASS(SettingsServerManager, Object);

	static SettingsServerManager *singleton;

	struct Entry {
		ObjectID id;
		StringName domain; // Read once at registration.
	};

	mutable Mutex mutex;
	LocalVector<Entry> servers;
	LocalVector<ObjectID> owned; // Created from `settings/startup/custom_servers`.
	SettingsServer::ApplyPhase startup_phase = SettingsServer::APPLY_PHASE_NONE; // Last phase passed to startup().
	bool startup_enabled = false;

	LocalVector<SettingsServer *> _get_servers_snapshot() const;
	void _create_custom_servers();

protected:
	static void _bind_methods();

public:
	static SettingsServerManager *get_singleton();
	static void register_settings();

	Error register_server(SettingsServer *p_server);
	void unregister_server(const StringName &p_domain);
	// Called by the SettingsServer destructor.
	void remove_server(SettingsServer *p_server);
	bool has_server(const StringName &p_domain) const;
	SettingsServer *get_server(const StringName &p_domain) const;
	TypedArray<SettingsServer> get_servers() const;
	PackedStringArray get_domains() const;

	Error load_all();
	Error save_all();
	Error apply_all();
	void revert_all();
	void reset_all_to_defaults();
	bool has_dirty_servers() const;

	String get_storage_folder() const;
	ResourceTextSerializer::Format get_storage_format() const;
	SettingsServer::ApplyPhase get_startup_phase() const;

	// Called from main/main.cpp for games (not in the editor or the project manager).
	void startup(SettingsServer::ApplyPhase p_phase);
	// Called from Main::cleanup(): runs queued autosaves and frees the servers created from the project settings.
	void finish();

	SettingsServerManager();
	~SettingsServerManager();
};
