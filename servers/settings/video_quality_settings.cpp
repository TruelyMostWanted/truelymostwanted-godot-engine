/**************************************************************************/
/*  video_quality_settings.cpp                                            */
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

#include "video_quality_settings.h"

#include "core/object/class_db.h"

void VideoQualitySettings::set_scaling_3d_mode(Viewport::Scaling3DMode p_mode) {
	scaling_3d_mode = p_mode;
	emit_changed();
}

Viewport::Scaling3DMode VideoQualitySettings::get_scaling_3d_mode() const {
	return scaling_3d_mode;
}

void VideoQualitySettings::set_scaling_3d_scale(float p_scale) {
	scaling_3d_scale = p_scale;
	emit_changed();
}

float VideoQualitySettings::get_scaling_3d_scale() const {
	return scaling_3d_scale;
}

void VideoQualitySettings::set_fsr_sharpness(float p_sharpness) {
	fsr_sharpness = p_sharpness;
	emit_changed();
}

float VideoQualitySettings::get_fsr_sharpness() const {
	return fsr_sharpness;
}

void VideoQualitySettings::set_mesh_lod_threshold(float p_pixels) {
	mesh_lod_threshold = p_pixels;
	emit_changed();
}

float VideoQualitySettings::get_mesh_lod_threshold() const {
	return mesh_lod_threshold;
}

void VideoQualitySettings::set_msaa_2d(Viewport::MSAA p_msaa) {
	msaa_2d = p_msaa;
	emit_changed();
}

Viewport::MSAA VideoQualitySettings::get_msaa_2d() const {
	return msaa_2d;
}

void VideoQualitySettings::set_msaa_3d(Viewport::MSAA p_msaa) {
	msaa_3d = p_msaa;
	emit_changed();
}

Viewport::MSAA VideoQualitySettings::get_msaa_3d() const {
	return msaa_3d;
}

void VideoQualitySettings::set_screen_space_aa(Viewport::ScreenSpaceAA p_screen_space_aa) {
	screen_space_aa = p_screen_space_aa;
	emit_changed();
}

Viewport::ScreenSpaceAA VideoQualitySettings::get_screen_space_aa() const {
	return screen_space_aa;
}

void VideoQualitySettings::set_use_taa(bool p_use_taa) {
	use_taa = p_use_taa;
	emit_changed();
}

bool VideoQualitySettings::is_using_taa() const {
	return use_taa;
}

void VideoQualitySettings::set_use_debanding(bool p_use_debanding) {
	use_debanding = p_use_debanding;
	emit_changed();
}

bool VideoQualitySettings::is_using_debanding() const {
	return use_debanding;
}

void VideoQualitySettings::set_vrs_mode(Viewport::VRSMode p_vrs_mode) {
	vrs_mode = p_vrs_mode;
	emit_changed();
}

Viewport::VRSMode VideoQualitySettings::get_vrs_mode() const {
	return vrs_mode;
}

void VideoQualitySettings::set_use_occlusion_culling(bool p_use_occlusion_culling) {
	use_occlusion_culling = p_use_occlusion_culling;
	emit_changed();
}

bool VideoQualitySettings::is_using_occlusion_culling() const {
	return use_occlusion_culling;
}

void VideoQualitySettings::set_occlusion_rays_per_thread(int p_rays_per_thread) {
	occlusion_rays_per_thread = p_rays_per_thread;
	emit_changed();
}

int VideoQualitySettings::get_occlusion_rays_per_thread() const {
	return occlusion_rays_per_thread;
}

void VideoQualitySettings::set_occlusion_culling_build_quality(RenderingServerEnums::ViewportOcclusionCullingBuildQuality p_quality) {
	occlusion_culling_build_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::ViewportOcclusionCullingBuildQuality VideoQualitySettings::get_occlusion_culling_build_quality() const {
	return occlusion_culling_build_quality;
}

void VideoQualitySettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scaling_3d_mode", "mode"), &VideoQualitySettings::set_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_mode"), &VideoQualitySettings::get_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("set_scaling_3d_scale", "scale"), &VideoQualitySettings::set_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_scale"), &VideoQualitySettings::get_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("set_fsr_sharpness", "sharpness"), &VideoQualitySettings::set_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("get_fsr_sharpness"), &VideoQualitySettings::get_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("set_mesh_lod_threshold", "pixels"), &VideoQualitySettings::set_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_threshold"), &VideoQualitySettings::get_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("set_msaa_2d", "msaa"), &VideoQualitySettings::set_msaa_2d);
	ClassDB::bind_method(D_METHOD("get_msaa_2d"), &VideoQualitySettings::get_msaa_2d);
	ClassDB::bind_method(D_METHOD("set_msaa_3d", "msaa"), &VideoQualitySettings::set_msaa_3d);
	ClassDB::bind_method(D_METHOD("get_msaa_3d"), &VideoQualitySettings::get_msaa_3d);
	ClassDB::bind_method(D_METHOD("set_screen_space_aa", "screen_space_aa"), &VideoQualitySettings::set_screen_space_aa);
	ClassDB::bind_method(D_METHOD("get_screen_space_aa"), &VideoQualitySettings::get_screen_space_aa);
	ClassDB::bind_method(D_METHOD("set_use_taa", "enable"), &VideoQualitySettings::set_use_taa);
	ClassDB::bind_method(D_METHOD("is_using_taa"), &VideoQualitySettings::is_using_taa);
	ClassDB::bind_method(D_METHOD("set_use_debanding", "enable"), &VideoQualitySettings::set_use_debanding);
	ClassDB::bind_method(D_METHOD("is_using_debanding"), &VideoQualitySettings::is_using_debanding);
	ClassDB::bind_method(D_METHOD("set_vrs_mode", "mode"), &VideoQualitySettings::set_vrs_mode);
	ClassDB::bind_method(D_METHOD("get_vrs_mode"), &VideoQualitySettings::get_vrs_mode);
	ClassDB::bind_method(D_METHOD("set_use_occlusion_culling", "enable"), &VideoQualitySettings::set_use_occlusion_culling);
	ClassDB::bind_method(D_METHOD("is_using_occlusion_culling"), &VideoQualitySettings::is_using_occlusion_culling);
	ClassDB::bind_method(D_METHOD("set_occlusion_rays_per_thread", "rays_per_thread"), &VideoQualitySettings::set_occlusion_rays_per_thread);
	ClassDB::bind_method(D_METHOD("get_occlusion_rays_per_thread"), &VideoQualitySettings::get_occlusion_rays_per_thread);
	ClassDB::bind_method(D_METHOD("set_occlusion_culling_build_quality", "quality"), &VideoQualitySettings::set_occlusion_culling_build_quality);
	ClassDB::bind_method(D_METHOD("get_occlusion_culling_build_quality"), &VideoQualitySettings::get_occlusion_culling_build_quality);

	// `or_less`/`or_greater`: the Inspector shows the usual range, VideoQualitySettingsServer clamps with a warning.
	ADD_GROUP("Scaling 3D", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scaling_3d_mode", PROPERTY_HINT_ENUM, "Bilinear,FSR 1.0,FSR 2.2,MetalFX (Spatial),MetalFX (Temporal),Nearest"), "set_scaling_3d_mode", "get_scaling_3d_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scaling_3d_scale", PROPERTY_HINT_RANGE, "0.1,2,0.01,or_greater,or_less"), "set_scaling_3d_scale", "get_scaling_3d_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fsr_sharpness", PROPERTY_HINT_RANGE, "0,2,0.01,or_greater,or_less"), "set_fsr_sharpness", "get_fsr_sharpness");
	ADD_GROUP("Mesh LOD", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_lod_threshold", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater,or_less,suffix:px"), "set_mesh_lod_threshold", "get_mesh_lod_threshold");
	ADD_GROUP("Anti-Aliasing", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa_2d", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x"), "set_msaa_2d", "get_msaa_2d");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa_3d", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x"), "set_msaa_3d", "get_msaa_3d");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "screen_space_aa", PROPERTY_HINT_ENUM, "Disabled,FXAA,SMAA"), "set_screen_space_aa", "get_screen_space_aa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_taa"), "set_use_taa", "is_using_taa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_debanding"), "set_use_debanding", "is_using_debanding");
	ADD_GROUP("Variable Rate Shading", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vrs_mode", PROPERTY_HINT_ENUM, "Disabled,Texture,XR"), "set_vrs_mode", "get_vrs_mode");
	ADD_GROUP("Occlusion Culling", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_occlusion_culling"), "set_use_occlusion_culling", "is_using_occlusion_culling");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "occlusion_rays_per_thread", PROPERTY_HINT_RANGE, "1,2048,1,or_greater,or_less"), "set_occlusion_rays_per_thread", "get_occlusion_rays_per_thread");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "occlusion_culling_build_quality", PROPERTY_HINT_ENUM, "Low,Medium,High"), "set_occlusion_culling_build_quality", "get_occlusion_culling_build_quality");
}
