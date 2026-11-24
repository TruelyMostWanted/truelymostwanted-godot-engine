/**************************************************************************/
/*  settings_server_manager.cpp                                           */
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

#include "settings_server_manager.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "servers/settings/list_settings_server.h"

static constexpr const char *SETTING_APPLY_ON_STARTUP = "settings/startup/apply_on_startup";
static constexpr const char *SETTING_CUSTOM_SERVERS = "settings/startup/custom_servers";

SettingsServerManager *SettingsServerManager::singleton = nullptr;

SettingsServerManager *SettingsServerManager::get_singleton() {
	return singleton;
}

void SettingsServerManager::register_settings() {
	GLOBAL_DEF(SETTING_APPLY_ON_STARTUP, true);
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_CUSTOM_SERVERS, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:*.gd,*.cs", Variant::STRING, PROPERTY_HINT_FILE)), PackedStringArray());
	SettingsServer::register_settings();
}

LocalVector<SettingsServer *> SettingsServerManager::_get_servers_snapshot() const {
	LocalVector<ObjectID> ids;
	{
		MutexLock lock(mutex);
		for (const Entry &entry : servers) {
			ids.push_back(entry.id);
		}
	}
	LocalVector<SettingsServer *> result;
	for (const ObjectID &id : ids) {
		SettingsServer *server = Object::cast_to<SettingsServer>(ObjectDB::get_instance(id));
		if (server != nullptr) {
			result.push_back(server);
		}
	}
	return result;
}

Error SettingsServerManager::register_server(SettingsServer *p_server) {
	ERR_FAIL_NULL_V(p_server, ERR_INVALID_PARAMETER);
	const StringName domain = p_server->get_domain();
	ERR_FAIL_COND_V_MSG(domain == StringName(), ERR_INVALID_PARAMETER, "A settings server needs a domain name: implement _get_domain() (C++: get_domain()).");
	const ObjectID id = p_server->get_instance_id();
	bool duplicate = false;
	SettingsServer::ApplyPhase phase = SettingsServer::APPLY_PHASE_NONE;
	bool enabled = false;
	{
		MutexLock lock(mutex);
		for (const Entry &entry : servers) {
			if (entry.id == id) {
				return OK;
			}
			if (entry.domain == domain && ObjectDB::get_instance(entry.id) != nullptr) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			Entry entry;
			entry.id = id;
			entry.domain = domain;
			servers.push_back(entry);
			phase = startup_phase;
			enabled = startup_enabled;
		}
	}
	ERR_FAIL_COND_V_MSG(duplicate, ERR_ALREADY_EXISTS, vformat("A settings server for the domain \"%s\" is already registered.", domain));
	// `settings/paths/<domain>_settings_path`: engine domains register in the editor too, so their path shows up in the project settings.
	SettingsServer::register_path_setting(domain, Object::cast_to<ListSettingsServer>(p_server) != nullptr);
	// Registered after its startup phase (for example by a module or a GDExtension at scene level).
	if (enabled && phase != SettingsServer::APPLY_PHASE_NONE && int(p_server->get_apply_phase()) <= int(phase)) {
		p_server->startup();
	}
	emit_signal(SNAME("server_registered"), domain);
	return OK;
}

void SettingsServerManager::unregister_server(const StringName &p_domain) {
	SettingsServer *server = get_server(p_domain);
	ERR_FAIL_NULL_MSG(server, vformat("No settings server is registered for the domain \"%s\".", p_domain));
	remove_server(server);
}

void SettingsServerManager::remove_server(SettingsServer *p_server) {
	ERR_FAIL_NULL(p_server);
	const ObjectID id = p_server->get_instance_id();
	StringName domain;
	bool removed = false;
	{
		MutexLock lock(mutex);
		for (uint32_t i = 0; i < servers.size(); i++) {
			if (servers[i].id == id) {
				domain = servers[i].domain;
				servers.remove_at(i);
				removed = true;
				break;
			}
		}
	}
	if (removed) {
		emit_signal(SNAME("server_unregistered"), domain);
	}
}

bool SettingsServerManager::has_server(const StringName &p_domain) const {
	return get_server(p_domain) != nullptr;
}

SettingsServer *SettingsServerManager::get_server(const StringName &p_domain) const {
	ObjectID id;
	{
		MutexLock lock(mutex);
		for (const Entry &entry : servers) {
			if (entry.domain == p_domain) {
				id = entry.id;
				break;
			}
		}
	}
	return Object::cast_to<SettingsServer>(ObjectDB::get_instance(id));
}

TypedArray<SettingsServer> SettingsServerManager::get_servers() const {
	TypedArray<SettingsServer> result;
	for (SettingsServer *server : _get_servers_snapshot()) {
		result.push_back(server);
	}
	return result;
}

PackedStringArray SettingsServerManager::get_domains() const {
	PackedStringArray result;
	MutexLock lock(mutex);
	for (const Entry &entry : servers) {
		result.push_back(entry.domain);
	}
	return result;
}

Error SettingsServerManager::load_all() {
	Error result = OK;
	for (SettingsServer *server : _get_servers_snapshot()) {
		const Error err = server->load();
		if (err != OK && err != ERR_FILE_NOT_FOUND) {
			result = err;
		}
	}
	return result;
}

Error SettingsServerManager::save_all() {
	Error result = OK;
	for (SettingsServer *server : _get_servers_snapshot()) {
		const Error err = server->save();
		if (err != OK) {
			result = err;
		}
	}
	return result;
}

Error SettingsServerManager::apply_all() {
	Error result = OK;
	for (SettingsServer *server : _get_servers_snapshot()) {
		const Error err = server->apply();
		if (err != OK) {
			result = err;
		}
	}
	return result;
}

void SettingsServerManager::revert_all() {
	for (SettingsServer *server : _get_servers_snapshot()) {
		server->revert();
	}
}

void SettingsServerManager::reset_all_to_defaults() {
	for (SettingsServer *server : _get_servers_snapshot()) {
		server->reset_to_defaults();
	}
}

bool SettingsServerManager::has_dirty_servers() const {
	for (SettingsServer *server : _get_servers_snapshot()) {
		if (server->is_dirty()) {
			return true;
		}
	}
	return false;
}

String SettingsServerManager::get_storage_folder() const {
	return SettingsServer::get_storage_folder();
}

ResourceTextSerializer::Format SettingsServerManager::get_storage_format() const {
	return SettingsServer::get_storage_format();
}

SettingsServer::ApplyPhase SettingsServerManager::get_startup_phase() const {
	MutexLock lock(mutex);
	return startup_phase;
}

void SettingsServerManager::_create_custom_servers() {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(SETTING_CUSTOM_SERVERS)) {
		return;
	}
	const PackedStringArray entries = project_settings->get_setting_with_override(SETTING_CUSTOM_SERVERS);
	for (const String &entry : entries) {
		String path = entry.strip_edges();
		if (path.is_empty()) {
			continue;
		}
		if (!path.contains("://") && ScriptServer::is_global_class(path)) {
			path = ScriptServer::get_global_class_path(path);
		}
		const Ref<Script> script = ResourceLoader::load(path, "Script");
		if (script.is_null()) {
			WARN_PRINT(vformat("Settings: can't load the settings server script \"%s\" (project setting \"%s\").", entry, SETTING_CUSTOM_SERVERS));
			continue;
		}
		const StringName base_type = script->get_instance_base_type();
		if (!ClassDB::is_parent_class(base_type, SettingsServer::get_class_static())) {
			WARN_PRINT(vformat("Settings: \"%s\" does not extend SettingsServer.", entry));
			continue;
		}
		Object *object = ClassDB::instantiate(base_type);
		if (object == nullptr) {
			WARN_PRINT(vformat("Settings: can't create \"%s\".", entry));
			continue;
		}
		object->set_script(script);
		SettingsServer *server = Object::cast_to<SettingsServer>(object);
		if (server == nullptr || object->get_script_instance() == nullptr) {
			memdelete(object);
			WARN_PRINT(vformat("Settings: can't create an instance of \"%s\".", entry));
			continue;
		}
		if (int(server->get_apply_phase()) < int(SettingsServer::APPLY_PHASE_SCENE)) {
			// Scripts can't run before the DisplayServer and the script languages exist.
			print_verbose(vformat("Settings: the script domain \"%s\" asks for an earlier apply phase; script domains from \"%s\" start in APPLY_PHASE_SCENE.", server->get_domain(), SETTING_CUSTOM_SERVERS));
		}
		{
			MutexLock lock(mutex);
			owned.push_back(object->get_instance_id());
		}
		if (register_server(server) != OK) {
			{
				MutexLock lock(mutex);
				owned.erase(object->get_instance_id());
			}
			memdelete(object);
		}
	}
}

void SettingsServerManager::startup(SettingsServer::ApplyPhase p_phase) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const bool enabled = project_settings == nullptr || !project_settings->has_setting(SETTING_APPLY_ON_STARTUP) || bool(project_settings->get_setting_with_override(SETTING_APPLY_ON_STARTUP));
	{
		MutexLock lock(mutex);
		startup_enabled = enabled;
	}
	{
		MutexLock lock(mutex);
		startup_phase = SettingsServer::ApplyPhase(MAX(int(startup_phase), int(p_phase)));
	}
	if (p_phase == SettingsServer::APPLY_PHASE_SCENE) {
		// Script domains: their classes are loaded now. The phase is already APPLY_PHASE_SCENE, so register_server()
		// starts each one at once, whatever phase its _get_apply_phase() returns (the latest phase is the only one left).
		_create_custom_servers();
	}
	if (!enabled) {
		return;
	}
	for (SettingsServer *server : _get_servers_snapshot()) {
		if (!server->is_started() && int(server->get_apply_phase()) <= int(p_phase)) {
			server->startup();
		}
	}
	emit_signal(SNAME("startup_applied"), p_phase);
}

void SettingsServerManager::finish() {
	for (SettingsServer *server : _get_servers_snapshot()) {
		server->flush_autosave();
	}
	LocalVector<ObjectID> to_free;
	{
		MutexLock lock(mutex);
		to_free = LocalVector<ObjectID>(owned);
		owned.clear();
	}
	for (const ObjectID &id : to_free) {
		Object *object = ObjectDB::get_instance(id);
		if (object != nullptr) {
			memdelete(object);
		}
	}
}

void SettingsServerManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_server", "server"), &SettingsServerManager::register_server);
	ClassDB::bind_method(D_METHOD("unregister_server", "domain"), &SettingsServerManager::unregister_server);
	ClassDB::bind_method(D_METHOD("has_server", "domain"), &SettingsServerManager::has_server);
	ClassDB::bind_method(D_METHOD("get_server", "domain"), &SettingsServerManager::get_server);
	ClassDB::bind_method(D_METHOD("get_servers"), &SettingsServerManager::get_servers);
	ClassDB::bind_method(D_METHOD("get_domains"), &SettingsServerManager::get_domains);
	ClassDB::bind_method(D_METHOD("load_all"), &SettingsServerManager::load_all);
	ClassDB::bind_method(D_METHOD("save_all"), &SettingsServerManager::save_all);
	ClassDB::bind_method(D_METHOD("apply_all"), &SettingsServerManager::apply_all);
	ClassDB::bind_method(D_METHOD("revert_all"), &SettingsServerManager::revert_all);
	ClassDB::bind_method(D_METHOD("reset_all_to_defaults"), &SettingsServerManager::reset_all_to_defaults);
	ClassDB::bind_method(D_METHOD("has_dirty_servers"), &SettingsServerManager::has_dirty_servers);
	ClassDB::bind_method(D_METHOD("get_storage_folder"), &SettingsServerManager::get_storage_folder);
	ClassDB::bind_method(D_METHOD("get_storage_format"), &SettingsServerManager::get_storage_format);
	ClassDB::bind_method(D_METHOD("get_startup_phase"), &SettingsServerManager::get_startup_phase);

	ADD_SIGNAL(MethodInfo("server_registered", PropertyInfo(Variant::STRING_NAME, "domain")));
	ADD_SIGNAL(MethodInfo("server_unregistered", PropertyInfo(Variant::STRING_NAME, "domain")));
	ADD_SIGNAL(MethodInfo("startup_applied", PropertyInfo(Variant::INT, "phase", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "SettingsServer.ApplyPhase")));
}

SettingsServerManager::SettingsServerManager() {
	singleton = this;
	register_settings();
}

SettingsServerManager::~SettingsServerManager() {
	singleton = nullptr;
}
