/**************************************************************************/
/*  texture_streaming_settings_server.cpp                                 */
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

#include "texture_streaming_settings_server.h"

#ifdef MODULE_TEXTURE_STREAMING_ENABLED

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/texture_streaming_settings.h"

#include "modules/texture_streaming/texture_streaming.h"

// The values of the domain, in the order of TextureStreamingSettings.
static const char *texture_streaming_setting_names[] = {
	"enabled",
	"memory_budget_enabled",
	"memory_budget_mb",
	"min_lod",
	"max_lod",
};

static Variant texture_streaming_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_name);
}

TextureStreamingSettingsServer *TextureStreamingSettingsServer::singleton = nullptr;

TextureStreamingSettingsServer *TextureStreamingSettingsServer::get_singleton() {
	return singleton;
}

/////////////////////////////////////////////////////////////////////////////
// Renderer support.

String TextureStreamingSettingsServer::_get_rendering_method() {
	const OS *os = OS::get_singleton();
	return os != nullptr ? os->get_current_rendering_method() : String();
}

bool TextureStreamingSettingsServer::is_renderer_supported() {
	// TextureStreaming does not start with the Compatibility renderer.
	return _get_rendering_method() != "gl_compatibility";
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

String TextureStreamingSettingsServer::_get_project_setting_path(const StringName &p_name) {
	if (p_name == SNAME("enabled")) {
		return "rendering/textures/streaming/enabled";
	} else if (p_name == SNAME("memory_budget_enabled")) {
		return "rendering/textures/streaming/memory_budget_enabled";
	} else if (p_name == SNAME("memory_budget_mb")) {
		return "rendering/textures/streaming/memory_budget_mb";
	} else if (p_name == SNAME("min_lod")) {
		return "rendering/textures/streaming/min_lod";
	} else if (p_name == SNAME("max_lod")) {
		return "rendering/textures/streaming/max_lod";
	}
	return String();
}

bool TextureStreamingSettingsServer::_is_project_setting_value(const StringName &p_name) {
	// Written to the project settings; the others go to the overrides of TextureStreaming.
	return p_name == SNAME("enabled") || p_name == SNAME("memory_budget_enabled");
}

Variant TextureStreamingSettingsServer::_get_project_value(const StringName &p_name) const {
	const Variant *original = original_project_values.getptr(p_name);
	if (original != nullptr) {
		return *original;
	}
	if (p_name == SNAME("enabled") || p_name == SNAME("memory_budget_enabled")) {
		return bool(texture_streaming_project_setting(_get_project_setting_path(p_name), false));
	} else if (p_name == SNAME("memory_budget_mb")) {
		return int(texture_streaming_project_setting(_get_project_setting_path(p_name), 512));
	} else if (p_name == SNAME("min_lod")) {
		return int(texture_streaming_project_setting(_get_project_setting_path(p_name), 0));
	} else if (p_name == SNAME("max_lod")) {
		return int(texture_streaming_project_setting(_get_project_setting_path(p_name), 3));
	}
	return Variant();
}

Variant TextureStreamingSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

bool TextureStreamingSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	// Returns false if the target does not exist yet.
	if (!is_renderer_supported()) {
		if (!warned.has(p_name) && !values_equal(p_value, _get_project_value(p_name))) {
			warned.insert(p_name);
			WARN_PRINT(vformat("Texture streaming settings: \"%s\" = %s is not supported by the current renderer (\"%s\"); the value is not applied. See TextureStreamingSettingsServer.is_value_supported().", p_name, p_value, _get_rendering_method()));
		}
		return true;
	}
	warned.erase(p_name);

	if (_is_project_setting_value(p_name)) {
		ProjectSettings *project_settings = ProjectSettings::get_singleton();
		if (project_settings == nullptr) {
			return false;
		}
		if (Engine::get_singleton() != nullptr && Engine::get_singleton()->is_editor_hint()) {
			return true; // Never change the project of the editor (it may save project.godot).
		}
		const String path = _get_project_setting_path(p_name);
		if (!original_project_values.has(p_name)) {
			original_project_values[p_name] = _get_project_value(p_name);
		}
		// Not saved to project.godot. TextureStreaming reads `memory_budget_enabled` again on `settings_changed`.
		project_settings->set_setting(path, bool(p_value));
		return true;
	}

	TextureStreaming *texture_streaming = TextureStreaming::get_singleton();
	if (texture_streaming == nullptr) {
		return false;
	}
	if (p_name == SNAME("memory_budget_mb")) {
		texture_streaming->set_memory_budget_mb_override(uint32_t(MAX(int(p_value), 0)));
	} else if (p_name == SNAME("min_lod")) {
		texture_streaming->set_min_lod_override(int(p_value));
	} else if (p_name == SNAME("max_lod")) {
		texture_streaming->set_max_lod_override(int(p_value));
	}
	return true;
}

void TextureStreamingSettingsServer::_deliver_undelivered() {
	LocalVector<StringName> names;
	for (const StringName &undelivered_name : undelivered) {
		names.push_back(undelivered_name);
	}
	for (const StringName &undelivered_name : names) {
		if (_deliver(undelivered_name, _get_current(undelivered_name))) {
			undelivered.erase(undelivered_name);
		}
	}
}

void TextureStreamingSettingsServer::_push_streaming_value(const Variant &p_value, const StringName &p_name) {
	pushed_values[p_name] = p_value;
	if (_deliver(p_name, p_value)) {
		undelivered.erase(p_name);
	} else {
		// Before the TextureStreaming singleton exists: given to it after APPLY_PHASE_SERVERS.
		undelivered.insert(p_name);
	}
}

Variant TextureStreamingSettingsServer::_pull_streaming_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the engine state (which a game may have changed already).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	const TextureStreaming *texture_streaming = TextureStreaming::get_singleton();
	if (_is_project_setting_value(p_name) || texture_streaming == nullptr || undelivered.has(p_name) || !is_renderer_supported()) {
		// The project setting `enabled` describes the next start, not this one: keep what is stored.
		return _get_current(p_name);
	}
	if (p_name == SNAME("memory_budget_mb")) {
		const uint32_t mb = texture_streaming->get_memory_budget_mb_override();
		return mb != UINT32_MAX ? Variant(int64_t(mb)) : _get_current(p_name);
	} else if (p_name == SNAME("min_lod")) {
		const int32_t lod = texture_streaming->get_min_lod_override();
		return lod >= 0 ? Variant(lod) : _get_current(p_name);
	} else if (p_name == SNAME("max_lod")) {
		const int32_t lod = texture_streaming->get_max_lod_override();
		return lod >= 0 ? Variant(lod) : _get_current(p_name);
	}
	return _get_current(p_name);
}

void TextureStreamingSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SERVERS) {
		// The scene-level modules are initialized: TextureStreaming exists.
		_deliver_undelivered();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName TextureStreamingSettingsServer::get_domain() const {
	return SNAME("texture_streaming");
}

String TextureStreamingSettingsServer::_get_legacy_file_name() const {
	return "video_texture_streaming";
}

SettingsServer::ApplyPhase TextureStreamingSettingsServer::get_apply_phase() const {
	// Before the renderers and TextureStreaming read `rendering/textures/streaming/enabled`.
	return APPLY_PHASE_PRE_DISPLAY;
}

Ref<Resource> TextureStreamingSettingsServer::_create_default_settings() const {
	Ref<TextureStreamingSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void TextureStreamingSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant TextureStreamingSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}

	double min_value = 0.0;
	double max_value = 0.0;
	if (p_name == SNAME("memory_budget_mb")) {
		// The range of TextureStreaming.memory_budget_mb_override.
		max_value = 65535;
	} else if (p_name == SNAME("min_lod") || p_name == SNAME("max_lod")) {
		max_value = TextureStreaming::MAX_LOD_LEVEL;
	} else {
		return value;
	}
	const double number = value;
	if (Math::is_nan(number)) {
		WARN_PRINT(vformat("Texture streaming settings: \"%s\" is not a number; the default is used.", p_name));
		return get_default_value(p_name);
	}
	const double clamped = CLAMP(number, min_value, max_value);
	if (clamped != number) {
		WARN_PRINT(vformat("Texture streaming settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(min_value), Variant(max_value), Variant(clamped)));
	}
	return int64_t(Math::round(clamped));
}

bool TextureStreamingSettingsServer::is_value_supported(const StringName &p_name) const {
	return SettingsServer::is_value_supported(p_name) && is_renderer_supported();
}

PackedStringArray TextureStreamingSettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : texture_streaming_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void TextureStreamingSettingsServer::set_enabled(bool p_enabled) {
	set_value(SNAME("enabled"), p_enabled);
}

bool TextureStreamingSettingsServer::is_enabled() const {
	return get_value(SNAME("enabled"));
}

void TextureStreamingSettingsServer::set_memory_budget_enabled(bool p_enabled) {
	set_value(SNAME("memory_budget_enabled"), p_enabled);
}

bool TextureStreamingSettingsServer::is_memory_budget_enabled() const {
	return get_value(SNAME("memory_budget_enabled"));
}

void TextureStreamingSettingsServer::set_memory_budget_mb(int p_mb) {
	set_value(SNAME("memory_budget_mb"), p_mb);
}

int TextureStreamingSettingsServer::get_memory_budget_mb() const {
	return get_value(SNAME("memory_budget_mb"));
}

void TextureStreamingSettingsServer::set_min_lod(int p_lod) {
	set_value(SNAME("min_lod"), p_lod);
}

int TextureStreamingSettingsServer::get_min_lod() const {
	return get_value(SNAME("min_lod"));
}

void TextureStreamingSettingsServer::set_max_lod(int p_lod) {
	set_value(SNAME("max_lod"), p_lod);
}

int TextureStreamingSettingsServer::get_max_lod() const {
	return get_value(SNAME("max_lod"));
}

int64_t TextureStreamingSettingsServer::get_memory_budget_bytes_used() const {
	TextureStreaming *texture_streaming = TextureStreaming::get_singleton();
	return texture_streaming != nullptr ? int64_t(texture_streaming->get_memory_budget_bytes_used()) : 0;
}

void TextureStreamingSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &TextureStreamingSettingsServer::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &TextureStreamingSettingsServer::is_enabled);
	ClassDB::bind_method(D_METHOD("set_memory_budget_enabled", "enabled"), &TextureStreamingSettingsServer::set_memory_budget_enabled);
	ClassDB::bind_method(D_METHOD("is_memory_budget_enabled"), &TextureStreamingSettingsServer::is_memory_budget_enabled);
	ClassDB::bind_method(D_METHOD("set_memory_budget_mb", "mb"), &TextureStreamingSettingsServer::set_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("get_memory_budget_mb"), &TextureStreamingSettingsServer::get_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("set_min_lod", "lod"), &TextureStreamingSettingsServer::set_min_lod);
	ClassDB::bind_method(D_METHOD("get_min_lod"), &TextureStreamingSettingsServer::get_min_lod);
	ClassDB::bind_method(D_METHOD("set_max_lod", "lod"), &TextureStreamingSettingsServer::set_max_lod);
	ClassDB::bind_method(D_METHOD("get_max_lod"), &TextureStreamingSettingsServer::get_max_lod);
	ClassDB::bind_method(D_METHOD("get_memory_budget_bytes_used"), &TextureStreamingSettingsServer::get_memory_budget_bytes_used);
	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &TextureStreamingSettingsServer::get_unsupported_setting_names);

	ClassDB::bind_static_method("TextureStreamingSettingsServer", D_METHOD("is_renderer_supported"), &TextureStreamingSettingsServer::is_renderer_supported);
}

TextureStreamingSettingsServer::TextureStreamingSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : texture_streaming_setting_names) {
		const StringName bound_name = setting_name;
		// The renderers compile the streaming shader path and TextureStreaming starts its threads at startup only.
		const BitField<SettingFlags> flags = bound_name == SNAME("enabled") ? BitField<SettingFlags>(SETTING_FLAG_RESTART_REQUIRED) : BitField<SettingFlags>(SETTING_FLAG_NONE);
		bind_setting_callbacks(bound_name, callable_mp(this, &TextureStreamingSettingsServer::_push_streaming_value).bind(bound_name), callable_mp(this, &TextureStreamingSettingsServer::_pull_streaming_value).bind(bound_name), flags);
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &TextureStreamingSettingsServer::_on_startup_applied));
	}
}

TextureStreamingSettingsServer::~TextureStreamingSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif // MODULE_TEXTURE_STREAMING_ENABLED
