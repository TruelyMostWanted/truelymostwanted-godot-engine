/**************************************************************************/
/*  video_preset_settings_server.cpp                                      */
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

#include "video_preset_settings_server.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_server.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_preset_settings.h"

static constexpr const char *SETTING_VIDEO_PRESET_PRESETS = "settings/video_preset/presets";
static constexpr const char *SETTING_VIDEO_PRESET_USE_BUILTIN = "settings/video_preset/use_builtin_presets";
static constexpr const char *VIDEO_PRESET_CUSTOM = "Custom";

// The built-in presets, from the lowest to the highest.
static const char *video_preset_builtin_names[] = { "Low", "Medium", "High", "Ultra" };
static constexpr int VIDEO_PRESET_BUILTIN_COUNT = 4;

// One value of the built-in presets: its value in Low, Medium, High and Ultra.
struct VideoPresetBuiltinValue {
	const char *domain;
	const char *name;
	Variant::Type type;
	double values[VIDEO_PRESET_BUILTIN_COUNT];
};

// Enum values are the values of the engine enums (see the hints of the domain resources).
static const VideoPresetBuiltinValue video_preset_builtin_values[] = {
	// Resolution scale (bilinear), mesh LOD threshold in pixels.
	{ "video_quality", "scaling_3d_scale", Variant::FLOAT, { 0.75, 1.0, 1.0, 1.0 } },
	{ "video_quality", "mesh_lod_threshold", Variant::FLOAT, { 4.0, 2.0, 1.0, 0.5 } },
	// MSAA 3D: Disabled, Disabled, 2x, 4x. Screen-space AA: FXAA, FXAA, Disabled, Disabled.
	{ "video_quality", "msaa_3d", Variant::INT, { 0, 0, 1, 2 } },
	{ "video_quality", "screen_space_aa", Variant::INT, { 1, 1, 0, 0 } },
	{ "video_quality", "use_taa", Variant::BOOL, { 0, 0, 0, 1 } },
	{ "video_quality", "use_debanding", Variant::BOOL, { 0, 0, 1, 1 } },
	// Anisotropic filtering: 2x, 4x, 8x, 16x. 2D SDF scale: 25%, 50%, 50%, 100%.
	{ "video_texture", "anisotropic_filtering_level", Variant::INT, { 1, 2, 3, 4 } },
	{ "video_texture", "sdf_scale", Variant::INT, { 2, 1, 1, 0 } },
	{ "video_texture", "canvas_shadow_atlas_size", Variant::INT, { 1024, 2048, 2048, 4096 } },
	// Shadow atlas sizes; filter quality: Soft Very Low, Soft Low, Soft Medium, Soft High.
	{ "video_shadow", "directional_shadow_atlas_size", Variant::INT, { 2048, 4096, 4096, 8192 } },
	{ "video_shadow", "directional_soft_shadow_filter_quality", Variant::INT, { 1, 2, 3, 4 } },
	{ "video_shadow", "positional_shadow_atlas_size", Variant::INT, { 2048, 4096, 4096, 8192 } },
	{ "video_shadow", "positional_soft_shadow_filter_quality", Variant::INT, { 1, 2, 3, 4 } },
	// SSAO and SSIL quality: Very Low, Low, Medium, High.
	{ "video_effect", "ssao_quality", Variant::INT, { 0, 1, 2, 3 } },
	{ "video_effect", "ssao_half_size", Variant::BOOL, { 1, 1, 0, 0 } },
	{ "video_effect", "ssil_quality", Variant::INT, { 0, 1, 2, 3 } },
	{ "video_effect", "ssil_half_size", Variant::BOOL, { 1, 1, 0, 0 } },
	// SDFGI rays: 4, 8, 16, 32. VoxelGI: Low, Low, High, High.
	{ "video_effect", "sdfgi_ray_count", Variant::INT, { 0, 1, 2, 3 } },
	{ "video_effect", "voxel_gi_quality", Variant::INT, { 0, 0, 1, 1 } },
	{ "video_effect", "volumetric_fog_volume_size", Variant::INT, { 48, 64, 64, 128 } },
	{ "video_effect", "volumetric_fog_volume_depth", Variant::INT, { 48, 64, 64, 128 } },
	{ "video_effect", "volumetric_fog_use_filter", Variant::BOOL, { 0, 1, 1, 1 } },
	// Subsurface scattering: Low, Low, Medium, High. Depth of field blur: Very Low, Low, Medium, High.
	{ "video_effect", "sss_quality", Variant::INT, { 1, 1, 2, 3 } },
	{ "video_effect", "dof_blur_quality", Variant::INT, { 0, 1, 2, 3 } },
	// Low turns the most expensive environment effects off.
	{ "video_effect", "allow_ssao", Variant::BOOL, { 0, 1, 1, 1 } },
	{ "video_effect", "allow_ssil", Variant::BOOL, { 0, 0, 1, 1 } },
	{ "video_effect", "allow_ssr", Variant::BOOL, { 0, 1, 1, 1 } },
	{ "video_effect", "allow_volumetric_fog", Variant::BOOL, { 0, 1, 1, 1 } },
};

static Variant video_preset_builtin_value(const VideoPresetBuiltinValue &p_value, int p_level) {
	const double number = p_value.values[p_level];
	switch (p_value.type) {
		case Variant::BOOL:
			return number != 0.0;
		case Variant::INT:
			return int64_t(number);
		default:
			return number;
	}
}

static bool video_preset_is_number(const Variant &p_value) {
	return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
}

// Like SettingsServer::values_equal(), but numbers of different types are compared by value and floats
// approximately (domain resources store `float`, presets `double`).
static bool video_preset_values_equal(const Variant &p_a, const Variant &p_b) {
	if (video_preset_is_number(p_a) && video_preset_is_number(p_b)) {
		if (p_a.get_type() == Variant::INT && p_b.get_type() == Variant::INT) {
			return int64_t(p_a) == int64_t(p_b);
		}
		return Math::is_equal_approx(float(double(p_a)), float(double(p_b)));
	}
	return SettingsServer::values_equal(p_a, p_b);
}

// A new settings resource of `p_domain` for a built-in preset: a copy of the domain's defaults (the project
// settings), so the values a built-in preset does not define keep the project's choice; the class defaults
// if the domain is not registered.
static Ref<Resource> video_preset_builtin_base(const String &p_domain) {
	Ref<Resource> base;
	if (p_domain == "video_quality") {
		base = Ref<Resource>(memnew(VideoQualitySettings));
	} else if (p_domain == "video_texture") {
		base = Ref<Resource>(memnew(VideoTextureSettings));
	} else if (p_domain == "video_shadow") {
		base = Ref<Resource>(memnew(VideoShadowSettings));
	} else if (p_domain == "video_effect") {
		base = Ref<Resource>(memnew(VideoEffectSettings));
	}
	ERR_FAIL_COND_V(base.is_null(), base);

	const SettingsServerManager *manager = SettingsServerManager::get_singleton();
	const SettingsServer *server = manager != nullptr ? manager->get_server(p_domain) : nullptr;
	if (server != nullptr) {
		const Ref<Resource> domain_defaults = server->get_default_settings();
		if (domain_defaults.is_valid() && domain_defaults->get_class_name() == base->get_class_name()) {
			return SettingsServer::copy_settings(domain_defaults);
		}
	}
	return base;
}

// Compares the stored values of a preset's domain resource with the values of the domain server. Values the
// domain doesn't have or support are not applied, so they don't count either.
static bool video_preset_domain_matches(const SettingsServer *p_server, const Ref<Resource> &p_settings) {
	LocalVector<PropertyInfo> infos;
	SettingsServer::get_value_infos(p_settings.ptr(), infos);
	for (const PropertyInfo &info : infos) {
		if (!p_server->has_setting(info.name) || !p_server->is_value_supported(info.name)) {
			continue;
		}
		if (!video_preset_values_equal(p_server->get_value(info.name), p_settings->get(info.name))) {
			return false;
		}
	}
	return true;
}

VideoPresetSettingsServer *VideoPresetSettingsServer::singleton = nullptr;

VideoPresetSettingsServer *VideoPresetSettingsServer::get_singleton() {
	return singleton;
}

void VideoPresetSettingsServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_VIDEO_PRESET_PRESETS, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::STRING, PROPERTY_HINT_FILE, "*.tres,*.res")), PackedStringArray());
	GLOBAL_DEF(SETTING_VIDEO_PRESET_USE_BUILTIN, true);
}

String VideoPresetSettingsServer::get_custom_preset_name() {
	return VIDEO_PRESET_CUSTOM;
}

PackedStringArray VideoPresetSettingsServer::get_builtin_preset_names() {
	PackedStringArray names;
	for (const char *builtin_name : video_preset_builtin_names) {
		names.push_back(builtin_name);
	}
	return names;
}

Ref<VideoPreset> VideoPresetSettingsServer::create_builtin_preset(const String &p_name) {
	int level = -1;
	for (int i = 0; i < VIDEO_PRESET_BUILTIN_COUNT; i++) {
		if (p_name == video_preset_builtin_names[i]) {
			level = i;
			break;
		}
	}
	ERR_FAIL_COND_V_MSG(level < 0, Ref<VideoPreset>(), vformat("VideoPresetSettingsServer: \"%s\" is not a built-in preset (%s).", p_name, String(", ").join(get_builtin_preset_names())));

	Ref<VideoPreset> preset;
	preset.instantiate();
	preset->set_preset_name(p_name);
	for (const String &domain : VideoPreset::get_domains()) {
		preset->set_domain_settings(domain, video_preset_builtin_base(domain));
	}
	for (const VideoPresetBuiltinValue &value : video_preset_builtin_values) {
		const Ref<Resource> domain_settings = preset->get_domain_settings(value.domain);
		ERR_CONTINUE(domain_settings.is_null());
		domain_settings->set(value.name, video_preset_builtin_value(value, level));
	}
	return preset;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName VideoPresetSettingsServer::get_domain() const {
	return SNAME("video_preset");
}

SettingsServer::ApplyPhase VideoPresetSettingsServer::get_apply_phase() const {
	// Together with the video domains; the name has no engine target.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> VideoPresetSettingsServer::_create_default_settings() const {
	Ref<VideoPresetSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void VideoPresetSettingsServer::_apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) {
	// The name of the preset has no engine target: the values are applied by apply_preset() on the video domains.
}

void VideoPresetSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	// Nothing to read: the engine has no preset.
}

Variant VideoPresetSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (p_name == SNAME("preset") && value.get_type() == Variant::STRING) {
		return String(value).strip_edges();
	}
	return value;
}

/////////////////////////////////////////////////////////////////////////////
// Domain servers.

SettingsServer *VideoPresetSettingsServer::_get_domain_server(const StringName &p_domain) const {
	const ObjectID *override_id = domain_overrides.getptr(p_domain);
	if (override_id != nullptr) {
		return ObjectDB::get_instance<SettingsServer>(*override_id);
	}
	const SettingsServerManager *manager = SettingsServerManager::get_singleton();
	return manager != nullptr ? manager->get_server(p_domain) : nullptr;
}

void VideoPresetSettingsServer::_connect_domain(SettingsServer *p_server) {
	if (p_server == nullptr || p_server == this) {
		return;
	}
	const Callable callback = callable_mp(this, &VideoPresetSettingsServer::_on_domain_value_changed);
	if (!p_server->is_connected(SNAME("value_changed"), callback)) {
		p_server->connect(SNAME("value_changed"), callback);
	}
}

void VideoPresetSettingsServer::_connect_domains() {
	for (const String &domain : VideoPreset::get_domains()) {
		_connect_domain(_get_domain_server(domain));
	}
}

void VideoPresetSettingsServer::set_domain_server(const StringName &p_domain, SettingsServer *p_server) {
	if (p_server == nullptr) {
		domain_overrides.erase(p_domain);
	} else {
		domain_overrides[p_domain] = p_server->get_instance_id();
	}
	_connect_domain(_get_domain_server(p_domain));
}

void VideoPresetSettingsServer::_on_server_registered(const StringName &p_domain) {
	if (VideoPreset::get_domains().has(p_domain)) {
		_connect_domain(_get_domain_server(p_domain));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Presets.

void VideoPresetSettingsServer::_ensure_presets() const {
	if (!presets_loaded) {
		const_cast<VideoPresetSettingsServer *>(this)->_load_presets();
	}
}

void VideoPresetSettingsServer::_load_presets() {
	presets_loaded = true;
	presets.clear();

	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	bool use_builtin = true;
	PackedStringArray paths;
	if (project_settings != nullptr) {
		if (project_settings->has_setting(SETTING_VIDEO_PRESET_USE_BUILTIN)) {
			use_builtin = project_settings->get_setting(SETTING_VIDEO_PRESET_USE_BUILTIN);
		}
		if (project_settings->has_setting(SETTING_VIDEO_PRESET_PRESETS)) {
			paths = project_settings->get_setting(SETTING_VIDEO_PRESET_PRESETS);
		}
	}

	if (use_builtin) {
		for (const char *builtin_name : video_preset_builtin_names) {
			presets.push_back(create_builtin_preset(builtin_name));
		}
	}

	for (const String &path : paths) {
		if (path.is_empty()) {
			continue;
		}
		const Ref<VideoPreset> preset = ResourceLoader::load(path);
		if (preset.is_null()) {
			ERR_PRINT(vformat("VideoPresetSettingsServer: \"%s\" (project setting \"%s\") is not a VideoPreset resource; it is ignored.", path, SETTING_VIDEO_PRESET_PRESETS));
			continue;
		}
		const String preset_name = preset->get_preset_name();
		if (preset_name.is_empty() || preset_name == VIDEO_PRESET_CUSTOM) {
			ERR_PRINT(vformat("VideoPresetSettingsServer: the preset \"%s\" needs a name other than \"\" and \"%s\"; it is ignored.", path, VIDEO_PRESET_CUSTOM));
			continue;
		}
		const int index = _find_preset(preset_name);
		if (index >= 0) {
			presets[index] = preset; // Replaces the built-in preset (or an earlier file) of the same name.
		} else {
			presets.push_back(preset);
		}
	}
}

int VideoPresetSettingsServer::_find_preset(const String &p_name) const {
	for (uint32_t i = 0; i < presets.size(); i++) {
		if (presets[i]->get_preset_name() == p_name) {
			return int(i);
		}
	}
	return -1;
}

TypedArray<VideoPreset> VideoPresetSettingsServer::get_presets() const {
	_ensure_presets();
	TypedArray<VideoPreset> result;
	for (const Ref<VideoPreset> &preset : presets) {
		result.push_back(preset);
	}
	return result;
}

PackedStringArray VideoPresetSettingsServer::get_preset_names() const {
	_ensure_presets();
	PackedStringArray names;
	for (const Ref<VideoPreset> &preset : presets) {
		names.push_back(preset->get_preset_name());
	}
	return names;
}

Ref<VideoPreset> VideoPresetSettingsServer::get_preset(const String &p_name) const {
	_ensure_presets();
	const int index = _find_preset(p_name);
	return index >= 0 ? presets[index] : Ref<VideoPreset>();
}

bool VideoPresetSettingsServer::has_preset(const String &p_name) const {
	_ensure_presets();
	return _find_preset(p_name) >= 0;
}

void VideoPresetSettingsServer::add_preset(const Ref<VideoPreset> &p_preset) {
	ERR_FAIL_COND(p_preset.is_null());
	const String preset_name = p_preset->get_preset_name();
	ERR_FAIL_COND_MSG(preset_name.is_empty() || preset_name == VIDEO_PRESET_CUSTOM, vformat("VideoPresetSettingsServer: a preset needs a name other than \"\" and \"%s\".", VIDEO_PRESET_CUSTOM));
	_ensure_presets();
	const int index = _find_preset(preset_name);
	if (index >= 0) {
		presets[index] = p_preset;
	} else {
		presets.push_back(p_preset);
	}
	_update_current();
}

void VideoPresetSettingsServer::remove_preset(const String &p_name) {
	_ensure_presets();
	const int index = _find_preset(p_name);
	ERR_FAIL_COND_MSG(index < 0, vformat("VideoPresetSettingsServer: there is no preset \"%s\".", p_name));
	presets.remove_at(index);
	_update_current();
}

void VideoPresetSettingsServer::reload_presets() {
	_load_presets();
	_update_current();
}

/////////////////////////////////////////////////////////////////////////////
// Current preset.

bool VideoPresetSettingsServer::_matches(const Ref<VideoPreset> &p_preset) const {
	for (const String &domain : VideoPreset::get_domains()) {
		// A domain the preset doesn't touch (null resource) doesn't count.
		const Ref<Resource> domain_settings = p_preset->get_domain_settings(domain);
		if (domain_settings.is_null()) {
			continue;
		}
		const SettingsServer *server = _get_domain_server(domain);
		if (server == nullptr) {
			continue;
		}
		if (!video_preset_domain_matches(server, domain_settings)) {
			return false;
		}
	}
	return true;
}

String VideoPresetSettingsServer::_detect(const String &p_preferred) const {
	_ensure_presets();
	if (!p_preferred.is_empty()) {
		const int index = _find_preset(p_preferred);
		if (index >= 0 && _matches(presets[index])) {
			return p_preferred;
		}
	}
	for (const Ref<VideoPreset> &preset : presets) {
		if (_matches(preset)) {
			return preset->get_preset_name();
		}
	}
	return VIDEO_PRESET_CUSTOM;
}

String VideoPresetSettingsServer::detect_preset() const {
	return _detect(String());
}

bool VideoPresetSettingsServer::matches_preset(const String &p_name) const {
	const Ref<VideoPreset> preset = get_preset(p_name);
	return preset.is_valid() && _matches(preset);
}

String VideoPresetSettingsServer::get_current_preset() const {
	const String stored = get_value(SNAME("preset"));
	if (stored == VIDEO_PRESET_CUSTOM || (!stored.is_empty() && has_preset(stored))) {
		return stored;
	}
	// No preset picked yet, or the stored one no longer exists: the values decide.
	return _detect(String());
}

bool VideoPresetSettingsServer::is_custom() const {
	return get_current_preset() == VIDEO_PRESET_CUSTOM;
}

void VideoPresetSettingsServer::_set_stored_preset(const String &p_name) {
	set_value(SNAME("preset"), p_name);
	_update_current();
}

void VideoPresetSettingsServer::_update_current() {
	const String current = get_current_preset();
	if (last_current_known && current == last_current) {
		return;
	}
	last_current = current;
	last_current_known = true;
	emit_signal(SNAME("preset_changed"), current);
}

Error VideoPresetSettingsServer::apply_preset(const String &p_name) {
	ERR_FAIL_COND_V_MSG(p_name == VIDEO_PRESET_CUSTOM, ERR_INVALID_PARAMETER, vformat("VideoPresetSettingsServer: \"%s\" is not a preset; it is the state after a single change.", VIDEO_PRESET_CUSTOM));
	const Ref<VideoPreset> preset = get_preset(p_name);
	ERR_FAIL_COND_V_MSG(preset.is_null(), ERR_DOES_NOT_EXIST, vformat("VideoPresetSettingsServer: there is no preset \"%s\" (%s).", p_name, String(", ").join(get_preset_names())));

	Error result = OK;
	PackedStringArray missing_domains;
	applying_preset = true;
	for (const String &domain : VideoPreset::get_domains()) {
		// A null resource: the preset doesn't touch the domain.
		const Ref<Resource> domain_settings = preset->get_domain_settings(domain);
		if (domain_settings.is_null()) {
			continue;
		}
		SettingsServer *server = _get_domain_server(domain);
		if (server == nullptr) {
			missing_domains.push_back(domain);
			continue;
		}
		// All values of the domain first, then one apply(): one `applied` signal, one autosave, one confirmation.
		const ApplyMode mode = server->get_apply_mode();
		server->set_apply_mode(APPLY_MODE_MANUAL);
		LocalVector<PropertyInfo> infos;
		SettingsServer::get_value_infos(domain_settings.ptr(), infos);
		for (const PropertyInfo &info : infos) {
			if (!server->has_setting(info.name) || !server->is_value_supported(info.name)) {
				continue; // Not used by the current renderer: the domain keeps its value.
			}
			server->set_value(info.name, domain_settings->get(info.name));
		}
		server->set_apply_mode(mode);
		if (mode == APPLY_MODE_IMMEDIATE) {
			const Error err = server->apply();
			if (err != OK && result == OK) {
				result = err;
			}
		}
	}
	applying_preset = false;
	_set_stored_preset(p_name);

	if (!missing_domains.is_empty()) {
		WARN_PRINT(vformat("VideoPresetSettingsServer: the settings domains %s are not registered; the values of preset \"%s\" for them are skipped.", String(", ").join(missing_domains), p_name));
	}
	return result;
}

String VideoPresetSettingsServer::suggest_preset() const {
	// 0 = Low, 1 = Medium, 2 = High. Ultra is never suggested: that needs a benchmark (out of scope).
	int level = 1;
	const String method = OS::get_singleton() != nullptr ? OS::get_singleton()->get_current_rendering_method() : String();
	if (method == "gl_compatibility") {
		level = 0;
	} else if (method == "mobile") {
		level = 1;
	} else {
#ifdef RD_ENABLED
		const RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server != nullptr) {
			switch (rendering_server->get_video_adapter_type()) {
				case RenderingDeviceEnums::DEVICE_TYPE_DISCRETE_GPU:
					level = 2;
					break;
				case RenderingDeviceEnums::DEVICE_TYPE_CPU:
					level = 0;
					break;
				default:
					level = 1;
					break;
			}
		}
#endif // RD_ENABLED
	}
	for (int i = level; i >= 0; i--) {
		if (has_preset(video_preset_builtin_names[i])) {
			return video_preset_builtin_names[i];
		}
	}
	return String();
}

/////////////////////////////////////////////////////////////////////////////
// Signals.

void VideoPresetSettingsServer::_on_domain_value_changed(const StringName &p_name, const Variant &p_value) {
	if (applying_preset) {
		return;
	}
	const String stored = get_value(SNAME("preset"));
	if (stored.is_empty()) {
		// No preset picked yet: get_current_preset() detects it from the values.
		_update_current();
		return;
	}
	// A single change: still the chosen preset, another preset, or "Custom".
	const String detected = _detect(stored == VIDEO_PRESET_CUSTOM ? String() : stored);
	if (detected != stored) {
		_set_stored_preset(detected);
	} else {
		_update_current();
	}
}

void VideoPresetSettingsServer::_on_own_value_changed(const StringName &p_name, const Variant &p_value) {
	_update_current();
}

void VideoPresetSettingsServer::_on_own_changed() {
	_update_current();
}

void VideoPresetSettingsServer::_bind_methods() {
	ClassDB::bind_static_method("VideoPresetSettingsServer", D_METHOD("get_custom_preset_name"), &VideoPresetSettingsServer::get_custom_preset_name);
	ClassDB::bind_static_method("VideoPresetSettingsServer", D_METHOD("get_builtin_preset_names"), &VideoPresetSettingsServer::get_builtin_preset_names);
	ClassDB::bind_static_method("VideoPresetSettingsServer", D_METHOD("create_builtin_preset", "name"), &VideoPresetSettingsServer::create_builtin_preset);

	ClassDB::bind_method(D_METHOD("get_presets"), &VideoPresetSettingsServer::get_presets);
	ClassDB::bind_method(D_METHOD("get_preset_names"), &VideoPresetSettingsServer::get_preset_names);
	ClassDB::bind_method(D_METHOD("get_preset", "name"), &VideoPresetSettingsServer::get_preset);
	ClassDB::bind_method(D_METHOD("has_preset", "name"), &VideoPresetSettingsServer::has_preset);
	ClassDB::bind_method(D_METHOD("add_preset", "preset"), &VideoPresetSettingsServer::add_preset);
	ClassDB::bind_method(D_METHOD("remove_preset", "name"), &VideoPresetSettingsServer::remove_preset);
	ClassDB::bind_method(D_METHOD("reload_presets"), &VideoPresetSettingsServer::reload_presets);

	ClassDB::bind_method(D_METHOD("apply_preset", "name"), &VideoPresetSettingsServer::apply_preset);
	ClassDB::bind_method(D_METHOD("get_current_preset"), &VideoPresetSettingsServer::get_current_preset);
	ClassDB::bind_method(D_METHOD("is_custom"), &VideoPresetSettingsServer::is_custom);
	ClassDB::bind_method(D_METHOD("detect_preset"), &VideoPresetSettingsServer::detect_preset);
	ClassDB::bind_method(D_METHOD("matches_preset", "name"), &VideoPresetSettingsServer::matches_preset);
	ClassDB::bind_method(D_METHOD("suggest_preset"), &VideoPresetSettingsServer::suggest_preset);

	ADD_SIGNAL(MethodInfo("preset_changed", PropertyInfo(Variant::STRING, "name")));
}

VideoPresetSettingsServer::VideoPresetSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();

	const Callable own_value_changed = callable_mp(this, &VideoPresetSettingsServer::_on_own_value_changed);
	connect(SNAME("value_changed"), own_value_changed);
	connect(SNAME("loaded"), callable_mp(this, &VideoPresetSettingsServer::_on_own_changed));
	connect(SNAME("reset"), callable_mp(this, &VideoPresetSettingsServer::_on_own_changed));

	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("server_registered"), callable_mp(this, &VideoPresetSettingsServer::_on_server_registered));
	}
	_connect_domains();
}

VideoPresetSettingsServer::~VideoPresetSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
