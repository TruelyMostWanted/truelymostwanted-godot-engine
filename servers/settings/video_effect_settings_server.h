/**************************************************************************/
/*  video_effect_settings_server.h                                        */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "scene/resources/environment.h"
#include "servers/rendering/rendering_server.h"
#include "servers/settings/settings_server.h"

class Node;

// Settings domain `video_effect` (#24): the global quality of SSAO, SSIL, SDFGI, VoxelGI, volumetric fog,
// subsurface scattering and depth of field, and the environment overrides: effects the player turned off
// (`allow_*`) and brightness, contrast and saturation (VideoEffectSettings).
// Everything goes to RenderingServer and takes effect at once. The overrides are applied at render time
// (RenderingServer.environment_set_global_effect_disabled() and environment_set_global_adjustment()): the
// Environment resources keep their authored values, and environments of scenes loaded later are covered too.
// Environments of WorldEnvironment and Camera3D nodes in the group get_ignore_group(), and those passed to
// set_environment_ignored(), are excluded from the overrides.
// Applied in APPLY_PHASE_SERVERS, after the renderer has read the project settings.
// Values the current renderer ignores are stored, but not pushed (is_value_supported()).
// Defaults are the project settings; the overrides default to "allowed" and 1.0. Precedence: player's file > project settings.
class VideoEffectSettingsServer : public SettingsServer {
	GDCLASS(VideoEffectSettingsServer, SettingsServer);

	static VideoEffectSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed before RenderingServer existed: delivered later.
	HashSet<StringName> undelivered;
	// Settings whose skipped value was already reported, so the warning is printed once per value.
	HashSet<StringName> warned;
	// Environments excluded from the overrides (RenderingServer has no getter for the flag).
	HashSet<ObjectID> ignored_environments;
	bool defaults_captured = false;

	static Variant _get_project_value(const StringName &p_name);
	static String _get_rendering_method();

	Variant _get_current(const StringName &p_name) const;
	bool _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_undelivered();
	void _push_effect_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_effect_value(const StringName &p_name);
	void _connect_tree();
	void _ignore_node_environment(Node *p_node);

	void _on_node_added(Node *p_node);
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static VideoEffectSettingsServer *get_singleton();

	// The group of WorldEnvironment and Camera3D nodes whose environments are not affected by the overrides.
	static StringName get_ignore_group();
	// True if the current renderer uses the value (no check that the domain has it).
	static bool is_renderer_supported(const StringName &p_name);

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;
	PackedStringArray get_unsupported_setting_names() const;

	// SSAO.
	void set_ssao_quality(RenderingServerEnums::EnvironmentSSAOQuality p_quality);
	RenderingServerEnums::EnvironmentSSAOQuality get_ssao_quality() const;
	void set_ssao_half_size(bool p_enabled);
	bool is_ssao_half_size_enabled() const;
	void set_ssao_adaptive_target(float p_target);
	float get_ssao_adaptive_target() const;
	void set_ssao_blur_passes(int p_passes);
	int get_ssao_blur_passes() const;
	void set_ssao_fadeout_from(float p_distance);
	float get_ssao_fadeout_from() const;
	void set_ssao_fadeout_to(float p_distance);
	float get_ssao_fadeout_to() const;

	// SSIL.
	void set_ssil_quality(RenderingServerEnums::EnvironmentSSILQuality p_quality);
	RenderingServerEnums::EnvironmentSSILQuality get_ssil_quality() const;
	void set_ssil_half_size(bool p_enabled);
	bool is_ssil_half_size_enabled() const;
	void set_ssil_adaptive_target(float p_target);
	float get_ssil_adaptive_target() const;
	void set_ssil_blur_passes(int p_passes);
	int get_ssil_blur_passes() const;
	void set_ssil_fadeout_from(float p_distance);
	float get_ssil_fadeout_from() const;
	void set_ssil_fadeout_to(float p_distance);
	float get_ssil_fadeout_to() const;

	// SDFGI.
	void set_sdfgi_ray_count(RenderingServerEnums::EnvironmentSDFGIRayCount p_ray_count);
	RenderingServerEnums::EnvironmentSDFGIRayCount get_sdfgi_ray_count() const;
	void set_sdfgi_frames_to_converge(RenderingServerEnums::EnvironmentSDFGIFramesToConverge p_frames);
	RenderingServerEnums::EnvironmentSDFGIFramesToConverge get_sdfgi_frames_to_converge() const;
	void set_sdfgi_frames_to_update_light(RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight p_frames);
	RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight get_sdfgi_frames_to_update_light() const;

	// VoxelGI.
	void set_voxel_gi_quality(RenderingServerEnums::VoxelGIQuality p_quality);
	RenderingServerEnums::VoxelGIQuality get_voxel_gi_quality() const;

	// Volumetric Fog.
	void set_volumetric_fog_volume_size(int p_size);
	int get_volumetric_fog_volume_size() const;
	void set_volumetric_fog_volume_depth(int p_depth);
	int get_volumetric_fog_volume_depth() const;
	void set_volumetric_fog_use_filter(bool p_enabled);
	bool is_volumetric_fog_using_filter() const;

	// Subsurface Scattering.
	void set_sss_quality(RenderingServerEnums::SubSurfaceScatteringQuality p_quality);
	RenderingServerEnums::SubSurfaceScatteringQuality get_sss_quality() const;
	void set_sss_scale(float p_scale);
	float get_sss_scale() const;
	void set_sss_depth_scale(float p_scale);
	float get_sss_depth_scale() const;

	// Depth of Field.
	void set_dof_bokeh_shape(RenderingServerEnums::DOFBokehShape p_shape);
	RenderingServerEnums::DOFBokehShape get_dof_bokeh_shape() const;
	void set_dof_blur_quality(RenderingServerEnums::DOFBlurQuality p_quality);
	RenderingServerEnums::DOFBlurQuality get_dof_blur_quality() const;
	void set_dof_use_jitter(bool p_enabled);
	bool is_dof_using_jitter() const;

	// Environment.
	void set_allow_glow(bool p_allowed);
	bool is_glow_allowed() const;
	void set_allow_ssao(bool p_allowed);
	bool is_ssao_allowed() const;
	void set_allow_ssil(bool p_allowed);
	bool is_ssil_allowed() const;
	void set_allow_ssr(bool p_allowed);
	bool is_ssr_allowed() const;
	void set_allow_fog(bool p_allowed);
	bool is_fog_allowed() const;
	void set_allow_volumetric_fog(bool p_allowed);
	bool is_volumetric_fog_allowed() const;
	void set_brightness(float p_brightness);
	float get_brightness() const;
	void set_contrast(float p_contrast);
	float get_contrast() const;
	void set_saturation(float p_saturation);
	float get_saturation() const;

	// Exclusions from the environment overrides.
	void set_environment_ignored(const Ref<Environment> &p_environment, bool p_ignored);
	bool is_environment_ignored(const Ref<Environment> &p_environment) const;

	VideoEffectSettingsServer();
	~VideoEffectSettingsServer();
};
