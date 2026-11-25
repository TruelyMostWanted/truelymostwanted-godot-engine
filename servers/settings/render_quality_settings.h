/**************************************************************************/
/*  render_quality_settings.h                                             */
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
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_server.h"

// Data of the `render_quality` settings domain (#21): 3D scaling (mode, scale, FSR sharpness), mesh LOD,
// anti-aliasing (MSAA 2D/3D, FXAA/SMAA, TAA, debanding), variable rate shading and occlusion culling.
// RenderQualitySettingsServer pushes the values to the root Viewport (and further viewports) and RenderingServer.
class RenderQualitySettings : public Resource {
	GDCLASS(RenderQualitySettings, Resource);

	Viewport::Scaling3DMode scaling_3d_mode = Viewport::SCALING_3D_MODE_BILINEAR;
	float scaling_3d_scale = 1.0;
	float fsr_sharpness = 0.2;
	float mesh_lod_threshold = 1.0;
	Viewport::MSAA msaa_2d = Viewport::MSAA_DISABLED;
	Viewport::MSAA msaa_3d = Viewport::MSAA_DISABLED;
	Viewport::ScreenSpaceAA screen_space_aa = Viewport::SCREEN_SPACE_AA_DISABLED;
	bool use_taa = false;
	bool use_debanding = false;
	Viewport::VRSMode vrs_mode = Viewport::VRS_DISABLED;
	bool use_occlusion_culling = false;
	int occlusion_rays_per_thread = 512;
	RenderingServerEnums::ViewportOcclusionCullingBuildQuality occlusion_culling_build_quality = RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_HIGH;

protected:
	static void _bind_methods();

public:
	void set_scaling_3d_mode(Viewport::Scaling3DMode p_mode);
	Viewport::Scaling3DMode get_scaling_3d_mode() const;
	void set_scaling_3d_scale(float p_scale);
	float get_scaling_3d_scale() const;
	void set_fsr_sharpness(float p_sharpness);
	float get_fsr_sharpness() const;
	void set_mesh_lod_threshold(float p_pixels);
	float get_mesh_lod_threshold() const;
	void set_msaa_2d(Viewport::MSAA p_msaa);
	Viewport::MSAA get_msaa_2d() const;
	void set_msaa_3d(Viewport::MSAA p_msaa);
	Viewport::MSAA get_msaa_3d() const;
	void set_screen_space_aa(Viewport::ScreenSpaceAA p_screen_space_aa);
	Viewport::ScreenSpaceAA get_screen_space_aa() const;
	void set_use_taa(bool p_use_taa);
	bool is_using_taa() const;
	void set_use_debanding(bool p_use_debanding);
	bool is_using_debanding() const;
	void set_vrs_mode(Viewport::VRSMode p_vrs_mode);
	Viewport::VRSMode get_vrs_mode() const;
	void set_use_occlusion_culling(bool p_use_occlusion_culling);
	bool is_using_occlusion_culling() const;
	void set_occlusion_rays_per_thread(int p_rays_per_thread);
	int get_occlusion_rays_per_thread() const;
	void set_occlusion_culling_build_quality(RenderingServerEnums::ViewportOcclusionCullingBuildQuality p_quality);
	RenderingServerEnums::ViewportOcclusionCullingBuildQuality get_occlusion_culling_build_quality() const;
};
