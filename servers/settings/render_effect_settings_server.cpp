/**************************************************************************/
/*  render_effect_settings_server.cpp                                     */
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

#include "render_effect_settings_server.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "servers/settings/render_effect_settings.h"
#include "servers/settings/settings_server_manager.h"

#ifndef _3D_DISABLED
#include "scene/3d/camera_3d.h"
#include "scene/3d/world_environment.h"
#endif // _3D_DISABLED

// The values of the domain, in the order of RenderEffectSettings.
static const char *render_effect_setting_names[] = {
	"ssao_quality",
	"ssao_half_size",
	"ssao_adaptive_target",
	"ssao_blur_passes",
	"ssao_fadeout_from",
	"ssao_fadeout_to",
	"ssil_quality",
	"ssil_half_size",
	"ssil_adaptive_target",
	"ssil_blur_passes",
	"ssil_fadeout_from",
	"ssil_fadeout_to",
	"sdfgi_ray_count",
	"sdfgi_frames_to_converge",
	"sdfgi_frames_to_update_light",
	"voxel_gi_quality",
	"volumetric_fog_volume_size",
	"volumetric_fog_volume_depth",
	"volumetric_fog_use_filter",
	"sss_quality",
	"sss_scale",
	"sss_depth_scale",
	"dof_bokeh_shape",
	"dof_blur_quality",
	"dof_use_jitter",
	"allow_glow",
	"allow_ssao",
	"allow_ssil",
	"allow_ssr",
	"allow_fog",
	"allow_volumetric_fog",
	"brightness",
	"contrast",
	"saturation",
};

// Type, project setting and valid range of a value.
struct RenderEffectSettingInfo {
	const char *name = nullptr;
	const char *project_setting = nullptr; // nullptr: an environment override (no project setting).
	Variant::Type type = Variant::NIL;
	double default_value = 0.0; // Used if the project setting is missing.
	double min_value = 0.0;
	double max_value = 0.0;
	int enum_size = 0; // > 0: an enum with the values 0 to enum_size - 1 (min_value/max_value unused).
};

static const RenderEffectSettingInfo render_effect_setting_infos[] = {
	{ "ssao_quality", "rendering/environment/ssao/quality", Variant::INT, 2, 0, 0, RenderingServerEnums::ENV_SSAO_QUALITY_ULTRA + 1 },
	{ "ssao_half_size", "rendering/environment/ssao/half_size", Variant::BOOL, 1, 0, 0, 0 },
	{ "ssao_adaptive_target", "rendering/environment/ssao/adaptive_target", Variant::FLOAT, 0.5, 0.0, 1.0, 0 },
	{ "ssao_blur_passes", "rendering/environment/ssao/blur_passes", Variant::INT, 2, 0, 6, 0 },
	{ "ssao_fadeout_from", "rendering/environment/ssao/fadeout_from", Variant::FLOAT, 50.0, 0.0, 65536.0, 0 },
	{ "ssao_fadeout_to", "rendering/environment/ssao/fadeout_to", Variant::FLOAT, 300.0, 0.0, 65536.0, 0 },
	{ "ssil_quality", "rendering/environment/ssil/quality", Variant::INT, 2, 0, 0, RenderingServerEnums::ENV_SSIL_QUALITY_ULTRA + 1 },
	{ "ssil_half_size", "rendering/environment/ssil/half_size", Variant::BOOL, 1, 0, 0, 0 },
	{ "ssil_adaptive_target", "rendering/environment/ssil/adaptive_target", Variant::FLOAT, 0.5, 0.0, 1.0, 0 },
	{ "ssil_blur_passes", "rendering/environment/ssil/blur_passes", Variant::INT, 4, 0, 6, 0 },
	{ "ssil_fadeout_from", "rendering/environment/ssil/fadeout_from", Variant::FLOAT, 50.0, 0.0, 65536.0, 0 },
	{ "ssil_fadeout_to", "rendering/environment/ssil/fadeout_to", Variant::FLOAT, 300.0, 0.0, 65536.0, 0 },
	{ "sdfgi_ray_count", "rendering/global_illumination/sdfgi/probe_ray_count", Variant::INT, 1, 0, 0, RenderingServerEnums::ENV_SDFGI_RAY_COUNT_MAX },
	{ "sdfgi_frames_to_converge", "rendering/global_illumination/sdfgi/frames_to_converge", Variant::INT, 5, 0, 0, RenderingServerEnums::ENV_SDFGI_CONVERGE_MAX },
	{ "sdfgi_frames_to_update_light", "rendering/global_illumination/sdfgi/frames_to_update_lights", Variant::INT, 2, 0, 0, RenderingServerEnums::ENV_SDFGI_UPDATE_LIGHT_MAX },
	{ "voxel_gi_quality", "rendering/global_illumination/voxel_gi/quality", Variant::INT, 0, 0, 0, RenderingServerEnums::VOXEL_GI_QUALITY_HIGH + 1 },
	{ "volumetric_fog_volume_size", "rendering/environment/volumetric_fog/volume_size", Variant::INT, 64, 16, 512, 0 },
	{ "volumetric_fog_volume_depth", "rendering/environment/volumetric_fog/volume_depth", Variant::INT, 64, 16, 512, 0 },
	{ "volumetric_fog_use_filter", "rendering/environment/volumetric_fog/use_filter", Variant::BOOL, 1, 0, 0, 0 },
	{ "sss_quality", "rendering/environment/subsurface_scattering/subsurface_scattering_quality", Variant::INT, 1, 0, 0, RenderingServerEnums::SUB_SURFACE_SCATTERING_QUALITY_HIGH + 1 },
	{ "sss_scale", "rendering/environment/subsurface_scattering/subsurface_scattering_scale", Variant::FLOAT, 0.05, 0.001, 1.0, 0 },
	{ "sss_depth_scale", "rendering/environment/subsurface_scattering/subsurface_scattering_depth_scale", Variant::FLOAT, 0.01, 0.001, 1.0, 0 },
	{ "dof_bokeh_shape", "rendering/camera/depth_of_field/depth_of_field_bokeh_shape", Variant::INT, 1, 0, 0, RenderingServerEnums::DOF_BOKEH_CIRCLE + 1 },
	{ "dof_blur_quality", "rendering/camera/depth_of_field/depth_of_field_bokeh_quality", Variant::INT, 1, 0, 0, RenderingServerEnums::DOF_BLUR_QUALITY_HIGH + 1 },
	{ "dof_use_jitter", "rendering/camera/depth_of_field/depth_of_field_use_jitter", Variant::BOOL, 0, 0, 0, 0 },
	{ "allow_glow", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	{ "allow_ssao", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	{ "allow_ssil", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	{ "allow_ssr", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	{ "allow_fog", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	{ "allow_volumetric_fog", nullptr, Variant::BOOL, 1, 0, 0, 0 },
	// The range of Environment.adjustment_*.
	{ "brightness", nullptr, Variant::FLOAT, 1.0, 0.01, 8.0, 0 },
	{ "contrast", nullptr, Variant::FLOAT, 1.0, 0.01, 8.0, 0 },
	{ "saturation", nullptr, Variant::FLOAT, 1.0, 0.01, 8.0, 0 },
};

static constexpr const char *RENDER_EFFECT_IGNORE_GROUP = "ignore_quality_settings";

static const RenderEffectSettingInfo *render_effect_find_info(const StringName &p_name) {
	for (const RenderEffectSettingInfo &info : render_effect_setting_infos) {
		if (p_name == info.name) {
			return &info;
		}
	}
	return nullptr;
}

static Variant render_effect_typed(Variant::Type p_type, const Variant &p_value) {
	switch (p_type) {
		case Variant::BOOL:
			return bool(p_value);
		case Variant::INT:
			return int64_t(p_value);
		case Variant::FLOAT:
			return double(p_value);
		default:
			return p_value;
	}
}

// The effect that an `allow_*` value turns off, or ENV_EFFECT_MAX.
static RenderingServerEnums::EnvironmentEffect render_effect_override_effect(const StringName &p_name) {
	if (p_name == SNAME("allow_glow")) {
		return RenderingServerEnums::ENV_EFFECT_GLOW;
	} else if (p_name == SNAME("allow_ssao")) {
		return RenderingServerEnums::ENV_EFFECT_SSAO;
	} else if (p_name == SNAME("allow_ssil")) {
		return RenderingServerEnums::ENV_EFFECT_SSIL;
	} else if (p_name == SNAME("allow_ssr")) {
		return RenderingServerEnums::ENV_EFFECT_SSR;
	} else if (p_name == SNAME("allow_fog")) {
		return RenderingServerEnums::ENV_EFFECT_FOG;
	} else if (p_name == SNAME("allow_volumetric_fog")) {
		return RenderingServerEnums::ENV_EFFECT_VOLUMETRIC_FOG;
	}
	return RenderingServerEnums::ENV_EFFECT_MAX;
}

RenderEffectSettingsServer *RenderEffectSettingsServer::singleton = nullptr;

RenderEffectSettingsServer *RenderEffectSettingsServer::get_singleton() {
	return singleton;
}

StringName RenderEffectSettingsServer::get_ignore_group() {
	return RENDER_EFFECT_IGNORE_GROUP;
}

/////////////////////////////////////////////////////////////////////////////
// Renderer support.

String RenderEffectSettingsServer::_get_rendering_method() {
	const OS *os = OS::get_singleton();
	return os != nullptr ? os->get_current_rendering_method() : String();
}

bool RenderEffectSettingsServer::is_renderer_supported(const StringName &p_name) {
	const String method = _get_rendering_method();
	const bool compatibility = method == "gl_compatibility";
	const bool mobile = method == "mobile";
	const String name_string = p_name;
	if (name_string.begins_with("ssao_") || name_string.begins_with("ssil_") || name_string.begins_with("sdfgi_") || name_string.begins_with("volumetric_fog_") || name_string.begins_with("sss_") ||
			p_name == SNAME("allow_ssao") || p_name == SNAME("allow_ssil") || p_name == SNAME("allow_ssr") || p_name == SNAME("allow_volumetric_fog")) {
		// Effects of the Forward+ renderer only.
		return !compatibility && !mobile;
	} else if (p_name == SNAME("voxel_gi_quality") || name_string.begins_with("dof_")) {
		// Forward+ and Mobile; the Compatibility renderer has neither VoxelGI nor depth of field.
		return !compatibility;
	}
	// Glow, fog and the adjustments: every renderer.
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

Variant RenderEffectSettingsServer::_get_project_value(const StringName &p_name) {
	// What the renderers read from the project settings at startup.
	const RenderEffectSettingInfo *info = render_effect_find_info(p_name);
	if (info == nullptr) {
		return Variant();
	}
	Variant value = info->default_value;
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (info->project_setting != nullptr && project_settings != nullptr && project_settings->has_setting(info->project_setting)) {
		// With the feature overrides (`.mobile`, `.web`, ...), like the renderers read them.
		value = project_settings->get_setting_with_override(info->project_setting);
	}
	return render_effect_typed(info->type, value);
}

Variant RenderEffectSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

bool RenderEffectSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	// Returns false if RenderingServer does not exist yet.
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server == nullptr) {
		return false;
	}
	if (!is_renderer_supported(p_name)) {
		if (!warned.has(p_name) && !values_equal(p_value, _get_project_value(p_name))) {
			// The player's (or the game's) choice, not the project value: worth a warning, once.
			warned.insert(p_name);
			WARN_PRINT(vformat("Render effect settings: \"%s\" = %s is not supported by the current renderer (\"%s\"); the value is not applied. See RenderEffectSettingsServer.is_value_supported().", p_name, p_value, _get_rendering_method()));
		}
		return true;
	}
	warned.erase(p_name);

	const String name_string = p_name;
	if (name_string.begins_with("ssao_")) {
		rendering_server->environment_set_ssao_quality(RenderingServerEnums::EnvironmentSSAOQuality(int(_get_current(SNAME("ssao_quality")))), _get_current(SNAME("ssao_half_size")), _get_current(SNAME("ssao_adaptive_target")), _get_current(SNAME("ssao_blur_passes")), _get_current(SNAME("ssao_fadeout_from")), _get_current(SNAME("ssao_fadeout_to")));
	} else if (name_string.begins_with("ssil_")) {
		rendering_server->environment_set_ssil_quality(RenderingServerEnums::EnvironmentSSILQuality(int(_get_current(SNAME("ssil_quality")))), _get_current(SNAME("ssil_half_size")), _get_current(SNAME("ssil_adaptive_target")), _get_current(SNAME("ssil_blur_passes")), _get_current(SNAME("ssil_fadeout_from")), _get_current(SNAME("ssil_fadeout_to")));
	} else if (p_name == SNAME("sdfgi_ray_count")) {
		rendering_server->environment_set_sdfgi_ray_count(RenderingServerEnums::EnvironmentSDFGIRayCount(int(p_value)));
	} else if (p_name == SNAME("sdfgi_frames_to_converge")) {
		rendering_server->environment_set_sdfgi_frames_to_converge(RenderingServerEnums::EnvironmentSDFGIFramesToConverge(int(p_value)));
	} else if (p_name == SNAME("sdfgi_frames_to_update_light")) {
		rendering_server->environment_set_sdfgi_frames_to_update_light(RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight(int(p_value)));
	} else if (p_name == SNAME("voxel_gi_quality")) {
		rendering_server->voxel_gi_set_quality(RenderingServerEnums::VoxelGIQuality(int(p_value)));
	} else if (p_name == SNAME("volumetric_fog_volume_size") || p_name == SNAME("volumetric_fog_volume_depth")) {
		rendering_server->environment_set_volumetric_fog_volume_size(_get_current(SNAME("volumetric_fog_volume_size")), _get_current(SNAME("volumetric_fog_volume_depth")));
	} else if (p_name == SNAME("volumetric_fog_use_filter")) {
		rendering_server->environment_set_volumetric_fog_filter_active(p_value);
	} else if (p_name == SNAME("sss_quality")) {
		rendering_server->sub_surface_scattering_set_quality(RenderingServerEnums::SubSurfaceScatteringQuality(int(p_value)));
	} else if (p_name == SNAME("sss_scale") || p_name == SNAME("sss_depth_scale")) {
		rendering_server->sub_surface_scattering_set_scale(_get_current(SNAME("sss_scale")), _get_current(SNAME("sss_depth_scale")));
	} else if (p_name == SNAME("dof_bokeh_shape")) {
		rendering_server->camera_attributes_set_dof_blur_bokeh_shape(RenderingServerEnums::DOFBokehShape(int(p_value)));
	} else if (p_name == SNAME("dof_blur_quality") || p_name == SNAME("dof_use_jitter")) {
		rendering_server->camera_attributes_set_dof_blur_quality(RenderingServerEnums::DOFBlurQuality(int(_get_current(SNAME("dof_blur_quality")))), _get_current(SNAME("dof_use_jitter")));
	} else if (p_name == SNAME("brightness") || p_name == SNAME("contrast") || p_name == SNAME("saturation")) {
		rendering_server->environment_set_global_adjustment(_get_current(SNAME("brightness")), _get_current(SNAME("contrast")), _get_current(SNAME("saturation")));
	} else {
		const RenderingServerEnums::EnvironmentEffect effect = render_effect_override_effect(p_name);
		if (effect != RenderingServerEnums::ENV_EFFECT_MAX) {
			rendering_server->environment_set_global_effect_disabled(effect, !bool(p_value));
		}
	}
	return true;
}

void RenderEffectSettingsServer::_deliver_undelivered() {
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

void RenderEffectSettingsServer::_push_effect_value(const Variant &p_value, const StringName &p_name) {
	pushed_values[p_name] = p_value;
	if (_deliver(p_name, p_value)) {
		undelivered.erase(p_name);
	} else {
		// Before RenderingServer exists: delivered in APPLY_PHASE_SERVERS.
		undelivered.insert(p_name);
	}
	_connect_tree();
}

Variant RenderEffectSettingsServer::_pull_effect_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the engine state (which a game may have changed already).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	// RenderingServer has no getters for these global values: what was pushed is what the engine has.
	return _get_current(p_name);
}

void RenderEffectSettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &RenderEffectSettingsServer::_on_node_added);
	if (tree->is_connected(SNAME("node_added"), callback)) {
		return;
	}
	tree->connect(SNAME("node_added"), callback);
	// The nodes that are in the tree already.
	for (Node *node : tree->get_nodes_in_group(get_ignore_group())) {
		_ignore_node_environment(node);
	}
}

void RenderEffectSettingsServer::_ignore_node_environment(Node *p_node) {
#ifndef _3D_DISABLED
	Ref<Environment> environment;
	if (const WorldEnvironment *world_environment = Object::cast_to<WorldEnvironment>(p_node)) {
		environment = world_environment->get_environment();
	} else if (const Camera3D *camera = Object::cast_to<Camera3D>(p_node)) {
		environment = camera->get_environment();
	}
	if (environment.is_valid()) {
		set_environment_ignored(environment, true);
	}
#endif // _3D_DISABLED
}

void RenderEffectSettingsServer::_on_node_added(Node *p_node) {
	if (p_node != nullptr && p_node->is_in_group(get_ignore_group())) {
		_ignore_node_environment(p_node);
	}
}

void RenderEffectSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SERVERS) {
		_deliver_undelivered();
	}
	if (p_phase >= APPLY_PHASE_SCENE) {
		// The SceneTree exists: watch for environments to exclude.
		_connect_tree();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName RenderEffectSettingsServer::get_domain() const {
	return SNAME("render_effect");
}

String RenderEffectSettingsServer::_get_legacy_file_name() const {
	return "video_effect";
}

SettingsServer::ApplyPhase RenderEffectSettingsServer::get_apply_phase() const {
	// RenderingServer exists and the renderer has read the project settings.
	return APPLY_PHASE_SERVERS;
}

Ref<Resource> RenderEffectSettingsServer::_create_default_settings() const {
	Ref<RenderEffectSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void RenderEffectSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant RenderEffectSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}
	const RenderEffectSettingInfo *info = render_effect_find_info(p_name);
	if (info == nullptr || info->type == Variant::BOOL) {
		return value;
	}

	const double number = value;
	if (info->enum_size > 0) {
		if (Math::is_nan(number) || number < 0 || number >= info->enum_size) {
			const Variant default_value = get_default_value(p_name);
			WARN_PRINT(vformat("Render effect settings: %s is not a valid value for \"%s\"; %s is used.", value, p_name, default_value));
			return default_value;
		}
		return int64_t(number);
	}
	if (Math::is_nan(number)) {
		WARN_PRINT(vformat("Render effect settings: \"%s\" is not a number; the default is used.", p_name));
		return get_default_value(p_name);
	}
	const double clamped = CLAMP(number, info->min_value, info->max_value);
	if (clamped != number) {
		WARN_PRINT(vformat("Render effect settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(info->min_value), Variant(info->max_value), Variant(clamped)));
	}
	return info->type == Variant::INT ? Variant(int64_t(Math::round(clamped))) : Variant(clamped);
}

bool RenderEffectSettingsServer::is_value_supported(const StringName &p_name) const {
	return SettingsServer::is_value_supported(p_name) && is_renderer_supported(p_name);
}

PackedStringArray RenderEffectSettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : render_effect_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Exclusions.

void RenderEffectSettingsServer::set_environment_ignored(const Ref<Environment> &p_environment, bool p_ignored) {
	ERR_FAIL_COND(p_environment.is_null());
	if (p_ignored) {
		ignored_environments.insert(p_environment->get_instance_id());
	} else {
		ignored_environments.erase(p_environment->get_instance_id());
	}
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server != nullptr) {
		rendering_server->environment_set_ignore_global_overrides(p_environment->get_rid(), p_ignored);
	}
}

bool RenderEffectSettingsServer::is_environment_ignored(const Ref<Environment> &p_environment) const {
	return p_environment.is_valid() && ignored_environments.has(p_environment->get_instance_id());
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void RenderEffectSettingsServer::set_ssao_quality(RenderingServerEnums::EnvironmentSSAOQuality p_quality) {
	set_value(SNAME("ssao_quality"), int(p_quality));
}

RenderingServerEnums::EnvironmentSSAOQuality RenderEffectSettingsServer::get_ssao_quality() const {
	return RenderingServerEnums::EnvironmentSSAOQuality(int(get_value(SNAME("ssao_quality"))));
}

void RenderEffectSettingsServer::set_ssao_half_size(bool p_enabled) {
	set_value(SNAME("ssao_half_size"), p_enabled);
}

bool RenderEffectSettingsServer::is_ssao_half_size_enabled() const {
	return get_value(SNAME("ssao_half_size"));
}

void RenderEffectSettingsServer::set_ssao_adaptive_target(float p_target) {
	set_value(SNAME("ssao_adaptive_target"), p_target);
}

float RenderEffectSettingsServer::get_ssao_adaptive_target() const {
	return get_value(SNAME("ssao_adaptive_target"));
}

void RenderEffectSettingsServer::set_ssao_blur_passes(int p_passes) {
	set_value(SNAME("ssao_blur_passes"), p_passes);
}

int RenderEffectSettingsServer::get_ssao_blur_passes() const {
	return get_value(SNAME("ssao_blur_passes"));
}

void RenderEffectSettingsServer::set_ssao_fadeout_from(float p_distance) {
	set_value(SNAME("ssao_fadeout_from"), p_distance);
}

float RenderEffectSettingsServer::get_ssao_fadeout_from() const {
	return get_value(SNAME("ssao_fadeout_from"));
}

void RenderEffectSettingsServer::set_ssao_fadeout_to(float p_distance) {
	set_value(SNAME("ssao_fadeout_to"), p_distance);
}

float RenderEffectSettingsServer::get_ssao_fadeout_to() const {
	return get_value(SNAME("ssao_fadeout_to"));
}

void RenderEffectSettingsServer::set_ssil_quality(RenderingServerEnums::EnvironmentSSILQuality p_quality) {
	set_value(SNAME("ssil_quality"), int(p_quality));
}

RenderingServerEnums::EnvironmentSSILQuality RenderEffectSettingsServer::get_ssil_quality() const {
	return RenderingServerEnums::EnvironmentSSILQuality(int(get_value(SNAME("ssil_quality"))));
}

void RenderEffectSettingsServer::set_ssil_half_size(bool p_enabled) {
	set_value(SNAME("ssil_half_size"), p_enabled);
}

bool RenderEffectSettingsServer::is_ssil_half_size_enabled() const {
	return get_value(SNAME("ssil_half_size"));
}

void RenderEffectSettingsServer::set_ssil_adaptive_target(float p_target) {
	set_value(SNAME("ssil_adaptive_target"), p_target);
}

float RenderEffectSettingsServer::get_ssil_adaptive_target() const {
	return get_value(SNAME("ssil_adaptive_target"));
}

void RenderEffectSettingsServer::set_ssil_blur_passes(int p_passes) {
	set_value(SNAME("ssil_blur_passes"), p_passes);
}

int RenderEffectSettingsServer::get_ssil_blur_passes() const {
	return get_value(SNAME("ssil_blur_passes"));
}

void RenderEffectSettingsServer::set_ssil_fadeout_from(float p_distance) {
	set_value(SNAME("ssil_fadeout_from"), p_distance);
}

float RenderEffectSettingsServer::get_ssil_fadeout_from() const {
	return get_value(SNAME("ssil_fadeout_from"));
}

void RenderEffectSettingsServer::set_ssil_fadeout_to(float p_distance) {
	set_value(SNAME("ssil_fadeout_to"), p_distance);
}

float RenderEffectSettingsServer::get_ssil_fadeout_to() const {
	return get_value(SNAME("ssil_fadeout_to"));
}

void RenderEffectSettingsServer::set_sdfgi_ray_count(RenderingServerEnums::EnvironmentSDFGIRayCount p_ray_count) {
	set_value(SNAME("sdfgi_ray_count"), int(p_ray_count));
}

RenderingServerEnums::EnvironmentSDFGIRayCount RenderEffectSettingsServer::get_sdfgi_ray_count() const {
	return RenderingServerEnums::EnvironmentSDFGIRayCount(int(get_value(SNAME("sdfgi_ray_count"))));
}

void RenderEffectSettingsServer::set_sdfgi_frames_to_converge(RenderingServerEnums::EnvironmentSDFGIFramesToConverge p_frames) {
	set_value(SNAME("sdfgi_frames_to_converge"), int(p_frames));
}

RenderingServerEnums::EnvironmentSDFGIFramesToConverge RenderEffectSettingsServer::get_sdfgi_frames_to_converge() const {
	return RenderingServerEnums::EnvironmentSDFGIFramesToConverge(int(get_value(SNAME("sdfgi_frames_to_converge"))));
}

void RenderEffectSettingsServer::set_sdfgi_frames_to_update_light(RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight p_frames) {
	set_value(SNAME("sdfgi_frames_to_update_light"), int(p_frames));
}

RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight RenderEffectSettingsServer::get_sdfgi_frames_to_update_light() const {
	return RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight(int(get_value(SNAME("sdfgi_frames_to_update_light"))));
}

void RenderEffectSettingsServer::set_voxel_gi_quality(RenderingServerEnums::VoxelGIQuality p_quality) {
	set_value(SNAME("voxel_gi_quality"), int(p_quality));
}

RenderingServerEnums::VoxelGIQuality RenderEffectSettingsServer::get_voxel_gi_quality() const {
	return RenderingServerEnums::VoxelGIQuality(int(get_value(SNAME("voxel_gi_quality"))));
}

void RenderEffectSettingsServer::set_volumetric_fog_volume_size(int p_size) {
	set_value(SNAME("volumetric_fog_volume_size"), p_size);
}

int RenderEffectSettingsServer::get_volumetric_fog_volume_size() const {
	return get_value(SNAME("volumetric_fog_volume_size"));
}

void RenderEffectSettingsServer::set_volumetric_fog_volume_depth(int p_depth) {
	set_value(SNAME("volumetric_fog_volume_depth"), p_depth);
}

int RenderEffectSettingsServer::get_volumetric_fog_volume_depth() const {
	return get_value(SNAME("volumetric_fog_volume_depth"));
}

void RenderEffectSettingsServer::set_volumetric_fog_use_filter(bool p_enabled) {
	set_value(SNAME("volumetric_fog_use_filter"), p_enabled);
}

bool RenderEffectSettingsServer::is_volumetric_fog_using_filter() const {
	return get_value(SNAME("volumetric_fog_use_filter"));
}

void RenderEffectSettingsServer::set_sss_quality(RenderingServerEnums::SubSurfaceScatteringQuality p_quality) {
	set_value(SNAME("sss_quality"), int(p_quality));
}

RenderingServerEnums::SubSurfaceScatteringQuality RenderEffectSettingsServer::get_sss_quality() const {
	return RenderingServerEnums::SubSurfaceScatteringQuality(int(get_value(SNAME("sss_quality"))));
}

void RenderEffectSettingsServer::set_sss_scale(float p_scale) {
	set_value(SNAME("sss_scale"), p_scale);
}

float RenderEffectSettingsServer::get_sss_scale() const {
	return get_value(SNAME("sss_scale"));
}

void RenderEffectSettingsServer::set_sss_depth_scale(float p_scale) {
	set_value(SNAME("sss_depth_scale"), p_scale);
}

float RenderEffectSettingsServer::get_sss_depth_scale() const {
	return get_value(SNAME("sss_depth_scale"));
}

void RenderEffectSettingsServer::set_dof_bokeh_shape(RenderingServerEnums::DOFBokehShape p_shape) {
	set_value(SNAME("dof_bokeh_shape"), int(p_shape));
}

RenderingServerEnums::DOFBokehShape RenderEffectSettingsServer::get_dof_bokeh_shape() const {
	return RenderingServerEnums::DOFBokehShape(int(get_value(SNAME("dof_bokeh_shape"))));
}

void RenderEffectSettingsServer::set_dof_blur_quality(RenderingServerEnums::DOFBlurQuality p_quality) {
	set_value(SNAME("dof_blur_quality"), int(p_quality));
}

RenderingServerEnums::DOFBlurQuality RenderEffectSettingsServer::get_dof_blur_quality() const {
	return RenderingServerEnums::DOFBlurQuality(int(get_value(SNAME("dof_blur_quality"))));
}

void RenderEffectSettingsServer::set_dof_use_jitter(bool p_enabled) {
	set_value(SNAME("dof_use_jitter"), p_enabled);
}

bool RenderEffectSettingsServer::is_dof_using_jitter() const {
	return get_value(SNAME("dof_use_jitter"));
}

void RenderEffectSettingsServer::set_allow_glow(bool p_allowed) {
	set_value(SNAME("allow_glow"), p_allowed);
}

bool RenderEffectSettingsServer::is_glow_allowed() const {
	return get_value(SNAME("allow_glow"));
}

void RenderEffectSettingsServer::set_allow_ssao(bool p_allowed) {
	set_value(SNAME("allow_ssao"), p_allowed);
}

bool RenderEffectSettingsServer::is_ssao_allowed() const {
	return get_value(SNAME("allow_ssao"));
}

void RenderEffectSettingsServer::set_allow_ssil(bool p_allowed) {
	set_value(SNAME("allow_ssil"), p_allowed);
}

bool RenderEffectSettingsServer::is_ssil_allowed() const {
	return get_value(SNAME("allow_ssil"));
}

void RenderEffectSettingsServer::set_allow_ssr(bool p_allowed) {
	set_value(SNAME("allow_ssr"), p_allowed);
}

bool RenderEffectSettingsServer::is_ssr_allowed() const {
	return get_value(SNAME("allow_ssr"));
}

void RenderEffectSettingsServer::set_allow_fog(bool p_allowed) {
	set_value(SNAME("allow_fog"), p_allowed);
}

bool RenderEffectSettingsServer::is_fog_allowed() const {
	return get_value(SNAME("allow_fog"));
}

void RenderEffectSettingsServer::set_allow_volumetric_fog(bool p_allowed) {
	set_value(SNAME("allow_volumetric_fog"), p_allowed);
}

bool RenderEffectSettingsServer::is_volumetric_fog_allowed() const {
	return get_value(SNAME("allow_volumetric_fog"));
}

void RenderEffectSettingsServer::set_brightness(float p_brightness) {
	set_value(SNAME("brightness"), p_brightness);
}

float RenderEffectSettingsServer::get_brightness() const {
	return get_value(SNAME("brightness"));
}

void RenderEffectSettingsServer::set_contrast(float p_contrast) {
	set_value(SNAME("contrast"), p_contrast);
}

float RenderEffectSettingsServer::get_contrast() const {
	return get_value(SNAME("contrast"));
}

void RenderEffectSettingsServer::set_saturation(float p_saturation) {
	set_value(SNAME("saturation"), p_saturation);
}

float RenderEffectSettingsServer::get_saturation() const {
	return get_value(SNAME("saturation"));
}

void RenderEffectSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"allow_glow",
		"allow_ssao",
		"allow_ssil",
		"allow_ssr",
		"allow_fog",
		"allow_volumetric_fog",
		"brightness",
		"contrast",
		"saturation",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void RenderEffectSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("allow_glow_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("allow_ssao_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("allow_ssil_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("allow_ssr_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("allow_fog_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("allow_volumetric_fog_changed", PropertyInfo(Variant::BOOL, "allowed")));
	ADD_SIGNAL(MethodInfo("brightness_changed", PropertyInfo(Variant::FLOAT, "brightness")));
	ADD_SIGNAL(MethodInfo("contrast_changed", PropertyInfo(Variant::FLOAT, "contrast")));
	ADD_SIGNAL(MethodInfo("saturation_changed", PropertyInfo(Variant::FLOAT, "saturation")));

	ClassDB::bind_method(D_METHOD("set_ssao_quality", "quality"), &RenderEffectSettingsServer::set_ssao_quality);
	ClassDB::bind_method(D_METHOD("get_ssao_quality"), &RenderEffectSettingsServer::get_ssao_quality);
	ClassDB::bind_method(D_METHOD("set_ssao_half_size", "enabled"), &RenderEffectSettingsServer::set_ssao_half_size);
	ClassDB::bind_method(D_METHOD("is_ssao_half_size_enabled"), &RenderEffectSettingsServer::is_ssao_half_size_enabled);
	ClassDB::bind_method(D_METHOD("set_ssao_adaptive_target", "target"), &RenderEffectSettingsServer::set_ssao_adaptive_target);
	ClassDB::bind_method(D_METHOD("get_ssao_adaptive_target"), &RenderEffectSettingsServer::get_ssao_adaptive_target);
	ClassDB::bind_method(D_METHOD("set_ssao_blur_passes", "passes"), &RenderEffectSettingsServer::set_ssao_blur_passes);
	ClassDB::bind_method(D_METHOD("get_ssao_blur_passes"), &RenderEffectSettingsServer::get_ssao_blur_passes);
	ClassDB::bind_method(D_METHOD("set_ssao_fadeout_from", "distance"), &RenderEffectSettingsServer::set_ssao_fadeout_from);
	ClassDB::bind_method(D_METHOD("get_ssao_fadeout_from"), &RenderEffectSettingsServer::get_ssao_fadeout_from);
	ClassDB::bind_method(D_METHOD("set_ssao_fadeout_to", "distance"), &RenderEffectSettingsServer::set_ssao_fadeout_to);
	ClassDB::bind_method(D_METHOD("get_ssao_fadeout_to"), &RenderEffectSettingsServer::get_ssao_fadeout_to);
	ClassDB::bind_method(D_METHOD("set_ssil_quality", "quality"), &RenderEffectSettingsServer::set_ssil_quality);
	ClassDB::bind_method(D_METHOD("get_ssil_quality"), &RenderEffectSettingsServer::get_ssil_quality);
	ClassDB::bind_method(D_METHOD("set_ssil_half_size", "enabled"), &RenderEffectSettingsServer::set_ssil_half_size);
	ClassDB::bind_method(D_METHOD("is_ssil_half_size_enabled"), &RenderEffectSettingsServer::is_ssil_half_size_enabled);
	ClassDB::bind_method(D_METHOD("set_ssil_adaptive_target", "target"), &RenderEffectSettingsServer::set_ssil_adaptive_target);
	ClassDB::bind_method(D_METHOD("get_ssil_adaptive_target"), &RenderEffectSettingsServer::get_ssil_adaptive_target);
	ClassDB::bind_method(D_METHOD("set_ssil_blur_passes", "passes"), &RenderEffectSettingsServer::set_ssil_blur_passes);
	ClassDB::bind_method(D_METHOD("get_ssil_blur_passes"), &RenderEffectSettingsServer::get_ssil_blur_passes);
	ClassDB::bind_method(D_METHOD("set_ssil_fadeout_from", "distance"), &RenderEffectSettingsServer::set_ssil_fadeout_from);
	ClassDB::bind_method(D_METHOD("get_ssil_fadeout_from"), &RenderEffectSettingsServer::get_ssil_fadeout_from);
	ClassDB::bind_method(D_METHOD("set_ssil_fadeout_to", "distance"), &RenderEffectSettingsServer::set_ssil_fadeout_to);
	ClassDB::bind_method(D_METHOD("get_ssil_fadeout_to"), &RenderEffectSettingsServer::get_ssil_fadeout_to);
	ClassDB::bind_method(D_METHOD("set_sdfgi_ray_count", "ray_count"), &RenderEffectSettingsServer::set_sdfgi_ray_count);
	ClassDB::bind_method(D_METHOD("get_sdfgi_ray_count"), &RenderEffectSettingsServer::get_sdfgi_ray_count);
	ClassDB::bind_method(D_METHOD("set_sdfgi_frames_to_converge", "frames"), &RenderEffectSettingsServer::set_sdfgi_frames_to_converge);
	ClassDB::bind_method(D_METHOD("get_sdfgi_frames_to_converge"), &RenderEffectSettingsServer::get_sdfgi_frames_to_converge);
	ClassDB::bind_method(D_METHOD("set_sdfgi_frames_to_update_light", "frames"), &RenderEffectSettingsServer::set_sdfgi_frames_to_update_light);
	ClassDB::bind_method(D_METHOD("get_sdfgi_frames_to_update_light"), &RenderEffectSettingsServer::get_sdfgi_frames_to_update_light);
	ClassDB::bind_method(D_METHOD("set_voxel_gi_quality", "quality"), &RenderEffectSettingsServer::set_voxel_gi_quality);
	ClassDB::bind_method(D_METHOD("get_voxel_gi_quality"), &RenderEffectSettingsServer::get_voxel_gi_quality);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_volume_size", "size"), &RenderEffectSettingsServer::set_volumetric_fog_volume_size);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_volume_size"), &RenderEffectSettingsServer::get_volumetric_fog_volume_size);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_volume_depth", "depth"), &RenderEffectSettingsServer::set_volumetric_fog_volume_depth);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_volume_depth"), &RenderEffectSettingsServer::get_volumetric_fog_volume_depth);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_use_filter", "enabled"), &RenderEffectSettingsServer::set_volumetric_fog_use_filter);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_using_filter"), &RenderEffectSettingsServer::is_volumetric_fog_using_filter);
	ClassDB::bind_method(D_METHOD("set_sss_quality", "quality"), &RenderEffectSettingsServer::set_sss_quality);
	ClassDB::bind_method(D_METHOD("get_sss_quality"), &RenderEffectSettingsServer::get_sss_quality);
	ClassDB::bind_method(D_METHOD("set_sss_scale", "scale"), &RenderEffectSettingsServer::set_sss_scale);
	ClassDB::bind_method(D_METHOD("get_sss_scale"), &RenderEffectSettingsServer::get_sss_scale);
	ClassDB::bind_method(D_METHOD("set_sss_depth_scale", "scale"), &RenderEffectSettingsServer::set_sss_depth_scale);
	ClassDB::bind_method(D_METHOD("get_sss_depth_scale"), &RenderEffectSettingsServer::get_sss_depth_scale);
	ClassDB::bind_method(D_METHOD("set_dof_bokeh_shape", "shape"), &RenderEffectSettingsServer::set_dof_bokeh_shape);
	ClassDB::bind_method(D_METHOD("get_dof_bokeh_shape"), &RenderEffectSettingsServer::get_dof_bokeh_shape);
	ClassDB::bind_method(D_METHOD("set_dof_blur_quality", "quality"), &RenderEffectSettingsServer::set_dof_blur_quality);
	ClassDB::bind_method(D_METHOD("get_dof_blur_quality"), &RenderEffectSettingsServer::get_dof_blur_quality);
	ClassDB::bind_method(D_METHOD("set_dof_use_jitter", "enabled"), &RenderEffectSettingsServer::set_dof_use_jitter);
	ClassDB::bind_method(D_METHOD("is_dof_using_jitter"), &RenderEffectSettingsServer::is_dof_using_jitter);
	ClassDB::bind_method(D_METHOD("set_allow_glow", "allowed"), &RenderEffectSettingsServer::set_allow_glow);
	ClassDB::bind_method(D_METHOD("is_glow_allowed"), &RenderEffectSettingsServer::is_glow_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssao", "allowed"), &RenderEffectSettingsServer::set_allow_ssao);
	ClassDB::bind_method(D_METHOD("is_ssao_allowed"), &RenderEffectSettingsServer::is_ssao_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssil", "allowed"), &RenderEffectSettingsServer::set_allow_ssil);
	ClassDB::bind_method(D_METHOD("is_ssil_allowed"), &RenderEffectSettingsServer::is_ssil_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssr", "allowed"), &RenderEffectSettingsServer::set_allow_ssr);
	ClassDB::bind_method(D_METHOD("is_ssr_allowed"), &RenderEffectSettingsServer::is_ssr_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_fog", "allowed"), &RenderEffectSettingsServer::set_allow_fog);
	ClassDB::bind_method(D_METHOD("is_fog_allowed"), &RenderEffectSettingsServer::is_fog_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_volumetric_fog", "allowed"), &RenderEffectSettingsServer::set_allow_volumetric_fog);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_allowed"), &RenderEffectSettingsServer::is_volumetric_fog_allowed);
	ClassDB::bind_method(D_METHOD("set_brightness", "brightness"), &RenderEffectSettingsServer::set_brightness);
	ClassDB::bind_method(D_METHOD("get_brightness"), &RenderEffectSettingsServer::get_brightness);
	ClassDB::bind_method(D_METHOD("set_contrast", "contrast"), &RenderEffectSettingsServer::set_contrast);
	ClassDB::bind_method(D_METHOD("get_contrast"), &RenderEffectSettingsServer::get_contrast);
	ClassDB::bind_method(D_METHOD("set_saturation", "saturation"), &RenderEffectSettingsServer::set_saturation);
	ClassDB::bind_method(D_METHOD("get_saturation"), &RenderEffectSettingsServer::get_saturation);

	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &RenderEffectSettingsServer::get_unsupported_setting_names);
	ClassDB::bind_method(D_METHOD("set_environment_ignored", "environment", "ignored"), &RenderEffectSettingsServer::set_environment_ignored);
	ClassDB::bind_method(D_METHOD("is_environment_ignored", "environment"), &RenderEffectSettingsServer::is_environment_ignored);

	ClassDB::bind_static_method("RenderEffectSettingsServer", D_METHOD("get_ignore_group"), &RenderEffectSettingsServer::get_ignore_group);
	ClassDB::bind_static_method("RenderEffectSettingsServer", D_METHOD("is_renderer_supported", "name"), &RenderEffectSettingsServer::is_renderer_supported);
}

RenderEffectSettingsServer::RenderEffectSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : render_effect_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &RenderEffectSettingsServer::_push_effect_value).bind(bound_name), callable_mp(this, &RenderEffectSettingsServer::_pull_effect_value).bind(bound_name));
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &RenderEffectSettingsServer::_on_startup_applied));
	}
}

RenderEffectSettingsServer::~RenderEffectSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
