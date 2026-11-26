/**************************************************************************/
/*  render_effect_settings.cpp                                            */
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

#include "render_effect_settings.h"

#include "core/object/class_db.h"

void RenderEffectSettings::set_ssao_quality(RenderingServerEnums::EnvironmentSSAOQuality p_quality) {
	ssao_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::EnvironmentSSAOQuality RenderEffectSettings::get_ssao_quality() const {
	return ssao_quality;
}

void RenderEffectSettings::set_ssao_half_size(bool p_enabled) {
	ssao_half_size = p_enabled;
	emit_changed();
}

bool RenderEffectSettings::is_ssao_half_size_enabled() const {
	return ssao_half_size;
}

void RenderEffectSettings::set_ssao_adaptive_target(float p_target) {
	ssao_adaptive_target = p_target;
	emit_changed();
}

float RenderEffectSettings::get_ssao_adaptive_target() const {
	return ssao_adaptive_target;
}

void RenderEffectSettings::set_ssao_blur_passes(int p_passes) {
	ssao_blur_passes = p_passes;
	emit_changed();
}

int RenderEffectSettings::get_ssao_blur_passes() const {
	return ssao_blur_passes;
}

void RenderEffectSettings::set_ssao_fadeout_from(float p_distance) {
	ssao_fadeout_from = p_distance;
	emit_changed();
}

float RenderEffectSettings::get_ssao_fadeout_from() const {
	return ssao_fadeout_from;
}

void RenderEffectSettings::set_ssao_fadeout_to(float p_distance) {
	ssao_fadeout_to = p_distance;
	emit_changed();
}

float RenderEffectSettings::get_ssao_fadeout_to() const {
	return ssao_fadeout_to;
}

void RenderEffectSettings::set_ssil_quality(RenderingServerEnums::EnvironmentSSILQuality p_quality) {
	ssil_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::EnvironmentSSILQuality RenderEffectSettings::get_ssil_quality() const {
	return ssil_quality;
}

void RenderEffectSettings::set_ssil_half_size(bool p_enabled) {
	ssil_half_size = p_enabled;
	emit_changed();
}

bool RenderEffectSettings::is_ssil_half_size_enabled() const {
	return ssil_half_size;
}

void RenderEffectSettings::set_ssil_adaptive_target(float p_target) {
	ssil_adaptive_target = p_target;
	emit_changed();
}

float RenderEffectSettings::get_ssil_adaptive_target() const {
	return ssil_adaptive_target;
}

void RenderEffectSettings::set_ssil_blur_passes(int p_passes) {
	ssil_blur_passes = p_passes;
	emit_changed();
}

int RenderEffectSettings::get_ssil_blur_passes() const {
	return ssil_blur_passes;
}

void RenderEffectSettings::set_ssil_fadeout_from(float p_distance) {
	ssil_fadeout_from = p_distance;
	emit_changed();
}

float RenderEffectSettings::get_ssil_fadeout_from() const {
	return ssil_fadeout_from;
}

void RenderEffectSettings::set_ssil_fadeout_to(float p_distance) {
	ssil_fadeout_to = p_distance;
	emit_changed();
}

float RenderEffectSettings::get_ssil_fadeout_to() const {
	return ssil_fadeout_to;
}

void RenderEffectSettings::set_sdfgi_ray_count(RenderingServerEnums::EnvironmentSDFGIRayCount p_ray_count) {
	sdfgi_ray_count = p_ray_count;
	emit_changed();
}

RenderingServerEnums::EnvironmentSDFGIRayCount RenderEffectSettings::get_sdfgi_ray_count() const {
	return sdfgi_ray_count;
}

void RenderEffectSettings::set_sdfgi_frames_to_converge(RenderingServerEnums::EnvironmentSDFGIFramesToConverge p_frames) {
	sdfgi_frames_to_converge = p_frames;
	emit_changed();
}

RenderingServerEnums::EnvironmentSDFGIFramesToConverge RenderEffectSettings::get_sdfgi_frames_to_converge() const {
	return sdfgi_frames_to_converge;
}

void RenderEffectSettings::set_sdfgi_frames_to_update_light(RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight p_frames) {
	sdfgi_frames_to_update_light = p_frames;
	emit_changed();
}

RenderingServerEnums::EnvironmentSDFGIFramesToUpdateLight RenderEffectSettings::get_sdfgi_frames_to_update_light() const {
	return sdfgi_frames_to_update_light;
}

void RenderEffectSettings::set_voxel_gi_quality(RenderingServerEnums::VoxelGIQuality p_quality) {
	voxel_gi_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::VoxelGIQuality RenderEffectSettings::get_voxel_gi_quality() const {
	return voxel_gi_quality;
}

void RenderEffectSettings::set_volumetric_fog_volume_size(int p_size) {
	volumetric_fog_volume_size = p_size;
	emit_changed();
}

int RenderEffectSettings::get_volumetric_fog_volume_size() const {
	return volumetric_fog_volume_size;
}

void RenderEffectSettings::set_volumetric_fog_volume_depth(int p_depth) {
	volumetric_fog_volume_depth = p_depth;
	emit_changed();
}

int RenderEffectSettings::get_volumetric_fog_volume_depth() const {
	return volumetric_fog_volume_depth;
}

void RenderEffectSettings::set_volumetric_fog_use_filter(bool p_enabled) {
	volumetric_fog_use_filter = p_enabled;
	emit_changed();
}

bool RenderEffectSettings::is_volumetric_fog_using_filter() const {
	return volumetric_fog_use_filter;
}

void RenderEffectSettings::set_sss_quality(RenderingServerEnums::SubSurfaceScatteringQuality p_quality) {
	sss_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::SubSurfaceScatteringQuality RenderEffectSettings::get_sss_quality() const {
	return sss_quality;
}

void RenderEffectSettings::set_sss_scale(float p_scale) {
	sss_scale = p_scale;
	emit_changed();
}

float RenderEffectSettings::get_sss_scale() const {
	return sss_scale;
}

void RenderEffectSettings::set_sss_depth_scale(float p_scale) {
	sss_depth_scale = p_scale;
	emit_changed();
}

float RenderEffectSettings::get_sss_depth_scale() const {
	return sss_depth_scale;
}

void RenderEffectSettings::set_dof_bokeh_shape(RenderingServerEnums::DOFBokehShape p_shape) {
	dof_bokeh_shape = p_shape;
	emit_changed();
}

RenderingServerEnums::DOFBokehShape RenderEffectSettings::get_dof_bokeh_shape() const {
	return dof_bokeh_shape;
}

void RenderEffectSettings::set_dof_blur_quality(RenderingServerEnums::DOFBlurQuality p_quality) {
	dof_blur_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::DOFBlurQuality RenderEffectSettings::get_dof_blur_quality() const {
	return dof_blur_quality;
}

void RenderEffectSettings::set_dof_use_jitter(bool p_enabled) {
	dof_use_jitter = p_enabled;
	emit_changed();
}

bool RenderEffectSettings::is_dof_using_jitter() const {
	return dof_use_jitter;
}

void RenderEffectSettings::set_allow_glow(bool p_allowed) {
	allow_glow = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_glow_allowed() const {
	return allow_glow;
}

void RenderEffectSettings::set_allow_ssao(bool p_allowed) {
	allow_ssao = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_ssao_allowed() const {
	return allow_ssao;
}

void RenderEffectSettings::set_allow_ssil(bool p_allowed) {
	allow_ssil = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_ssil_allowed() const {
	return allow_ssil;
}

void RenderEffectSettings::set_allow_ssr(bool p_allowed) {
	allow_ssr = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_ssr_allowed() const {
	return allow_ssr;
}

void RenderEffectSettings::set_allow_fog(bool p_allowed) {
	allow_fog = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_fog_allowed() const {
	return allow_fog;
}

void RenderEffectSettings::set_allow_volumetric_fog(bool p_allowed) {
	allow_volumetric_fog = p_allowed;
	emit_changed();
}

bool RenderEffectSettings::is_volumetric_fog_allowed() const {
	return allow_volumetric_fog;
}

void RenderEffectSettings::set_brightness(float p_brightness) {
	brightness = p_brightness;
	emit_changed();
}

float RenderEffectSettings::get_brightness() const {
	return brightness;
}

void RenderEffectSettings::set_contrast(float p_contrast) {
	contrast = p_contrast;
	emit_changed();
}

float RenderEffectSettings::get_contrast() const {
	return contrast;
}

void RenderEffectSettings::set_saturation(float p_saturation) {
	saturation = p_saturation;
	emit_changed();
}

float RenderEffectSettings::get_saturation() const {
	return saturation;
}

void RenderEffectSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ssao_quality", "quality"), &RenderEffectSettings::set_ssao_quality);
	ClassDB::bind_method(D_METHOD("get_ssao_quality"), &RenderEffectSettings::get_ssao_quality);
	ClassDB::bind_method(D_METHOD("set_ssao_half_size", "enabled"), &RenderEffectSettings::set_ssao_half_size);
	ClassDB::bind_method(D_METHOD("is_ssao_half_size_enabled"), &RenderEffectSettings::is_ssao_half_size_enabled);
	ClassDB::bind_method(D_METHOD("set_ssao_adaptive_target", "target"), &RenderEffectSettings::set_ssao_adaptive_target);
	ClassDB::bind_method(D_METHOD("get_ssao_adaptive_target"), &RenderEffectSettings::get_ssao_adaptive_target);
	ClassDB::bind_method(D_METHOD("set_ssao_blur_passes", "passes"), &RenderEffectSettings::set_ssao_blur_passes);
	ClassDB::bind_method(D_METHOD("get_ssao_blur_passes"), &RenderEffectSettings::get_ssao_blur_passes);
	ClassDB::bind_method(D_METHOD("set_ssao_fadeout_from", "distance"), &RenderEffectSettings::set_ssao_fadeout_from);
	ClassDB::bind_method(D_METHOD("get_ssao_fadeout_from"), &RenderEffectSettings::get_ssao_fadeout_from);
	ClassDB::bind_method(D_METHOD("set_ssao_fadeout_to", "distance"), &RenderEffectSettings::set_ssao_fadeout_to);
	ClassDB::bind_method(D_METHOD("get_ssao_fadeout_to"), &RenderEffectSettings::get_ssao_fadeout_to);
	ClassDB::bind_method(D_METHOD("set_ssil_quality", "quality"), &RenderEffectSettings::set_ssil_quality);
	ClassDB::bind_method(D_METHOD("get_ssil_quality"), &RenderEffectSettings::get_ssil_quality);
	ClassDB::bind_method(D_METHOD("set_ssil_half_size", "enabled"), &RenderEffectSettings::set_ssil_half_size);
	ClassDB::bind_method(D_METHOD("is_ssil_half_size_enabled"), &RenderEffectSettings::is_ssil_half_size_enabled);
	ClassDB::bind_method(D_METHOD("set_ssil_adaptive_target", "target"), &RenderEffectSettings::set_ssil_adaptive_target);
	ClassDB::bind_method(D_METHOD("get_ssil_adaptive_target"), &RenderEffectSettings::get_ssil_adaptive_target);
	ClassDB::bind_method(D_METHOD("set_ssil_blur_passes", "passes"), &RenderEffectSettings::set_ssil_blur_passes);
	ClassDB::bind_method(D_METHOD("get_ssil_blur_passes"), &RenderEffectSettings::get_ssil_blur_passes);
	ClassDB::bind_method(D_METHOD("set_ssil_fadeout_from", "distance"), &RenderEffectSettings::set_ssil_fadeout_from);
	ClassDB::bind_method(D_METHOD("get_ssil_fadeout_from"), &RenderEffectSettings::get_ssil_fadeout_from);
	ClassDB::bind_method(D_METHOD("set_ssil_fadeout_to", "distance"), &RenderEffectSettings::set_ssil_fadeout_to);
	ClassDB::bind_method(D_METHOD("get_ssil_fadeout_to"), &RenderEffectSettings::get_ssil_fadeout_to);
	ClassDB::bind_method(D_METHOD("set_sdfgi_ray_count", "ray_count"), &RenderEffectSettings::set_sdfgi_ray_count);
	ClassDB::bind_method(D_METHOD("get_sdfgi_ray_count"), &RenderEffectSettings::get_sdfgi_ray_count);
	ClassDB::bind_method(D_METHOD("set_sdfgi_frames_to_converge", "frames"), &RenderEffectSettings::set_sdfgi_frames_to_converge);
	ClassDB::bind_method(D_METHOD("get_sdfgi_frames_to_converge"), &RenderEffectSettings::get_sdfgi_frames_to_converge);
	ClassDB::bind_method(D_METHOD("set_sdfgi_frames_to_update_light", "frames"), &RenderEffectSettings::set_sdfgi_frames_to_update_light);
	ClassDB::bind_method(D_METHOD("get_sdfgi_frames_to_update_light"), &RenderEffectSettings::get_sdfgi_frames_to_update_light);
	ClassDB::bind_method(D_METHOD("set_voxel_gi_quality", "quality"), &RenderEffectSettings::set_voxel_gi_quality);
	ClassDB::bind_method(D_METHOD("get_voxel_gi_quality"), &RenderEffectSettings::get_voxel_gi_quality);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_volume_size", "size"), &RenderEffectSettings::set_volumetric_fog_volume_size);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_volume_size"), &RenderEffectSettings::get_volumetric_fog_volume_size);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_volume_depth", "depth"), &RenderEffectSettings::set_volumetric_fog_volume_depth);
	ClassDB::bind_method(D_METHOD("get_volumetric_fog_volume_depth"), &RenderEffectSettings::get_volumetric_fog_volume_depth);
	ClassDB::bind_method(D_METHOD("set_volumetric_fog_use_filter", "enabled"), &RenderEffectSettings::set_volumetric_fog_use_filter);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_using_filter"), &RenderEffectSettings::is_volumetric_fog_using_filter);
	ClassDB::bind_method(D_METHOD("set_sss_quality", "quality"), &RenderEffectSettings::set_sss_quality);
	ClassDB::bind_method(D_METHOD("get_sss_quality"), &RenderEffectSettings::get_sss_quality);
	ClassDB::bind_method(D_METHOD("set_sss_scale", "scale"), &RenderEffectSettings::set_sss_scale);
	ClassDB::bind_method(D_METHOD("get_sss_scale"), &RenderEffectSettings::get_sss_scale);
	ClassDB::bind_method(D_METHOD("set_sss_depth_scale", "scale"), &RenderEffectSettings::set_sss_depth_scale);
	ClassDB::bind_method(D_METHOD("get_sss_depth_scale"), &RenderEffectSettings::get_sss_depth_scale);
	ClassDB::bind_method(D_METHOD("set_dof_bokeh_shape", "shape"), &RenderEffectSettings::set_dof_bokeh_shape);
	ClassDB::bind_method(D_METHOD("get_dof_bokeh_shape"), &RenderEffectSettings::get_dof_bokeh_shape);
	ClassDB::bind_method(D_METHOD("set_dof_blur_quality", "quality"), &RenderEffectSettings::set_dof_blur_quality);
	ClassDB::bind_method(D_METHOD("get_dof_blur_quality"), &RenderEffectSettings::get_dof_blur_quality);
	ClassDB::bind_method(D_METHOD("set_dof_use_jitter", "enabled"), &RenderEffectSettings::set_dof_use_jitter);
	ClassDB::bind_method(D_METHOD("is_dof_using_jitter"), &RenderEffectSettings::is_dof_using_jitter);
	ClassDB::bind_method(D_METHOD("set_allow_glow", "allowed"), &RenderEffectSettings::set_allow_glow);
	ClassDB::bind_method(D_METHOD("is_glow_allowed"), &RenderEffectSettings::is_glow_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssao", "allowed"), &RenderEffectSettings::set_allow_ssao);
	ClassDB::bind_method(D_METHOD("is_ssao_allowed"), &RenderEffectSettings::is_ssao_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssil", "allowed"), &RenderEffectSettings::set_allow_ssil);
	ClassDB::bind_method(D_METHOD("is_ssil_allowed"), &RenderEffectSettings::is_ssil_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_ssr", "allowed"), &RenderEffectSettings::set_allow_ssr);
	ClassDB::bind_method(D_METHOD("is_ssr_allowed"), &RenderEffectSettings::is_ssr_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_fog", "allowed"), &RenderEffectSettings::set_allow_fog);
	ClassDB::bind_method(D_METHOD("is_fog_allowed"), &RenderEffectSettings::is_fog_allowed);
	ClassDB::bind_method(D_METHOD("set_allow_volumetric_fog", "allowed"), &RenderEffectSettings::set_allow_volumetric_fog);
	ClassDB::bind_method(D_METHOD("is_volumetric_fog_allowed"), &RenderEffectSettings::is_volumetric_fog_allowed);
	ClassDB::bind_method(D_METHOD("set_brightness", "brightness"), &RenderEffectSettings::set_brightness);
	ClassDB::bind_method(D_METHOD("get_brightness"), &RenderEffectSettings::get_brightness);
	ClassDB::bind_method(D_METHOD("set_contrast", "contrast"), &RenderEffectSettings::set_contrast);
	ClassDB::bind_method(D_METHOD("get_contrast"), &RenderEffectSettings::get_contrast);
	ClassDB::bind_method(D_METHOD("set_saturation", "saturation"), &RenderEffectSettings::set_saturation);
	ClassDB::bind_method(D_METHOD("get_saturation"), &RenderEffectSettings::get_saturation);

	// `or_less`/`or_greater`: the Inspector shows the usual range, RenderEffectSettingsServer clamps with a warning.
	ADD_GROUP("SSAO", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ssao_quality", PROPERTY_HINT_ENUM, "Very Low,Low,Medium,High,Ultra"), "set_ssao_quality", "get_ssao_quality");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ssao_half_size"), "set_ssao_half_size", "is_ssao_half_size_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_adaptive_target", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssao_adaptive_target", "get_ssao_adaptive_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ssao_blur_passes", PROPERTY_HINT_RANGE, "0,6,1,or_greater,or_less"), "set_ssao_blur_passes", "get_ssao_blur_passes");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_fadeout_from", PROPERTY_HINT_RANGE, "0,512,0.1,or_greater,or_less,suffix:m"), "set_ssao_fadeout_from", "get_ssao_fadeout_from");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssao_fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater,or_less,suffix:m"), "set_ssao_fadeout_to", "get_ssao_fadeout_to");
	ADD_GROUP("SSIL", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ssil_quality", PROPERTY_HINT_ENUM, "Very Low,Low,Medium,High,Ultra"), "set_ssil_quality", "get_ssil_quality");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ssil_half_size"), "set_ssil_half_size", "is_ssil_half_size_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_adaptive_target", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ssil_adaptive_target", "get_ssil_adaptive_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ssil_blur_passes", PROPERTY_HINT_RANGE, "0,6,1,or_greater,or_less"), "set_ssil_blur_passes", "get_ssil_blur_passes");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_fadeout_from", PROPERTY_HINT_RANGE, "0,512,0.1,or_greater,or_less,suffix:m"), "set_ssil_fadeout_from", "get_ssil_fadeout_from");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ssil_fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater,or_less,suffix:m"), "set_ssil_fadeout_to", "get_ssil_fadeout_to");
	ADD_GROUP("SDFGI", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdfgi_ray_count", PROPERTY_HINT_ENUM, "4,8,16,32,64,96,128"), "set_sdfgi_ray_count", "get_sdfgi_ray_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdfgi_frames_to_converge", PROPERTY_HINT_ENUM, "5,10,15,20,25,30"), "set_sdfgi_frames_to_converge", "get_sdfgi_frames_to_converge");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdfgi_frames_to_update_light", PROPERTY_HINT_ENUM, "1,2,4,8,16"), "set_sdfgi_frames_to_update_light", "get_sdfgi_frames_to_update_light");
	ADD_GROUP("VoxelGI", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "voxel_gi_quality", PROPERTY_HINT_ENUM, "Low,High"), "set_voxel_gi_quality", "get_voxel_gi_quality");
	ADD_GROUP("Volumetric Fog", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "volumetric_fog_volume_size", PROPERTY_HINT_RANGE, "16,512,1,or_greater,or_less"), "set_volumetric_fog_volume_size", "get_volumetric_fog_volume_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "volumetric_fog_volume_depth", PROPERTY_HINT_RANGE, "16,512,1,or_greater,or_less"), "set_volumetric_fog_volume_depth", "get_volumetric_fog_volume_depth");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "volumetric_fog_use_filter"), "set_volumetric_fog_use_filter", "is_volumetric_fog_using_filter");
	ADD_GROUP("Subsurface Scattering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sss_quality", PROPERTY_HINT_ENUM, "Disabled,Low,Medium,High"), "set_sss_quality", "get_sss_quality");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sss_scale", PROPERTY_HINT_RANGE, "0.001,1,0.001,or_greater,or_less"), "set_sss_scale", "get_sss_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sss_depth_scale", PROPERTY_HINT_RANGE, "0.001,1,0.001,or_greater,or_less"), "set_sss_depth_scale", "get_sss_depth_scale");
	ADD_GROUP("Depth of Field", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dof_bokeh_shape", PROPERTY_HINT_ENUM, "Box,Hexagon,Circle"), "set_dof_bokeh_shape", "get_dof_bokeh_shape");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dof_blur_quality", PROPERTY_HINT_ENUM, "Very Low,Low,Medium,High"), "set_dof_blur_quality", "get_dof_blur_quality");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dof_use_jitter"), "set_dof_use_jitter", "is_dof_using_jitter");
	ADD_GROUP("Environment", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_glow"), "set_allow_glow", "is_glow_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_ssao"), "set_allow_ssao", "is_ssao_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_ssil"), "set_allow_ssil", "is_ssil_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_ssr"), "set_allow_ssr", "is_ssr_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_fog"), "set_allow_fog", "is_fog_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_volumetric_fog"), "set_allow_volumetric_fog", "is_volumetric_fog_allowed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brightness", PROPERTY_HINT_RANGE, "0.01,8,0.01,or_greater,or_less"), "set_brightness", "get_brightness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "contrast", PROPERTY_HINT_RANGE, "0.01,8,0.01,or_greater,or_less"), "set_contrast", "get_contrast");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "saturation", PROPERTY_HINT_RANGE, "0.01,8,0.01,or_greater,or_less"), "set_saturation", "get_saturation");
}
