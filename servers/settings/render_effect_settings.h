/**************************************************************************/
/*  render_effect_settings.h                                              */
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
#include "servers/rendering/rendering_server.h"

// Data of the `render_effect` settings domain (#24): the global quality of SSAO, SSIL, SDFGI, VoxelGI,
// volumetric fog, subsurface scattering and depth of field, and the player's environment overrides (effects
// turned off for every environment, brightness, contrast and saturation).
// RenderEffectSettingsServer pushes the values to RenderingServer.
class RenderEffectSettings : public Resource {
	GDCLASS(RenderEffectSettings, Resource);

	RenderingServerEnums::EnvironmentSSAOQuality ssao_quality = RenderingServerEnums::ENV_SSAO_QUALITY_MEDIUM;
	bool ssao_half_size = true;
	float ssao_adaptive_target = 0.5;
	int ssao_blur_passes = 2;
	float ssao_fadeout_from = 50.0;
	float ssao_fadeout_to = 300.0;
	RenderingServerEnums::EnvironmentSSILQuality ssil_quality = RenderingServerEnums::ENV_SSIL_QUALITY_MEDIUM;
	bool ssil_half_size = true;
	float ssil_adaptive_target = 0.5;
	int ssil_blur_passes = 4;
	float ssil_fadeout_from = 50.0;
	float ssil_fadeout_to = 300.0;
	RenderingServerEnums::EnvironmentSDFGIRayCount sdfgi_ray_count = RenderingServerEnums::ENV_SDFGI_RAY_COUNT_8;
	RenderingServerEnums::EnvironmentSDFGIFramesToConverge sdfgi_frames_to_converge = RenderingServerEnums::ENV_SDFGI_CONVERGE_IN_30_FRAMES;
	RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight sdfgi_frames_to_update_light = RenderingServerEnums::ENV_SDFGI_UPDATE_LIGHT_IN_4_FRAMES;
	RenderingServerEnums::VoxelGIQuality voxel_gi_quality = RenderingServerEnums::VOXEL_GI_QUALITY_LOW;
	int volumetric_fog_volume_size = 64;
	int volumetric_fog_volume_depth = 64;
	bool volumetric_fog_use_filter = true;
	RenderingServerEnums::SubSurfaceScatteringQuality sss_quality = RenderingServerEnums::SUB_SURFACE_SCATTERING_QUALITY_LOW;
	float sss_scale = 0.05;
	float sss_depth_scale = 0.01;
	RenderingServerEnums::DOFBokehShape dof_bokeh_shape = RenderingServerEnums::DOF_BOKEH_HEXAGON;
	RenderingServerEnums::DOFBlurQuality dof_blur_quality = RenderingServerEnums::DOF_BLUR_QUALITY_LOW;
	bool dof_use_jitter = false;
	bool allow_glow = true;
	bool allow_ssao = true;
	bool allow_ssil = true;
	bool allow_ssr = true;
	bool allow_fog = true;
	bool allow_volumetric_fog = true;
	float brightness = 1.0;
	float contrast = 1.0;
	float saturation = 1.0;

protected:
	static void _bind_methods();

public:
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
	void set_sdfgi_ray_count(RenderingServerEnums::EnvironmentSDFGIRayCount p_ray_count);
	RenderingServerEnums::EnvironmentSDFGIRayCount get_sdfgi_ray_count() const;
	void set_sdfgi_frames_to_converge(RenderingServerEnums::EnvironmentSDFGIFramesToConverge p_frames);
	RenderingServerEnums::EnvironmentSDFGIFramesToConverge get_sdfgi_frames_to_converge() const;
	void set_sdfgi_frames_to_update_light(RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight p_frames);
	RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight get_sdfgi_frames_to_update_light() const;
	void set_voxel_gi_quality(RenderingServerEnums::VoxelGIQuality p_quality);
	RenderingServerEnums::VoxelGIQuality get_voxel_gi_quality() const;
	void set_volumetric_fog_volume_size(int p_size);
	int get_volumetric_fog_volume_size() const;
	void set_volumetric_fog_volume_depth(int p_depth);
	int get_volumetric_fog_volume_depth() const;
	void set_volumetric_fog_use_filter(bool p_enabled);
	bool is_volumetric_fog_using_filter() const;
	void set_sss_quality(RenderingServerEnums::SubSurfaceScatteringQuality p_quality);
	RenderingServerEnums::SubSurfaceScatteringQuality get_sss_quality() const;
	void set_sss_scale(float p_scale);
	float get_sss_scale() const;
	void set_sss_depth_scale(float p_scale);
	float get_sss_depth_scale() const;
	void set_dof_bokeh_shape(RenderingServerEnums::DOFBokehShape p_shape);
	RenderingServerEnums::DOFBokehShape get_dof_bokeh_shape() const;
	void set_dof_blur_quality(RenderingServerEnums::DOFBlurQuality p_quality);
	RenderingServerEnums::DOFBlurQuality get_dof_blur_quality() const;
	void set_dof_use_jitter(bool p_enabled);
	bool is_dof_using_jitter() const;
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
};
