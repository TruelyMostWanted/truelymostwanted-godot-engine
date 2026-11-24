/**************************************************************************/
/*  video_texture_settings.h                                              */
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

// Data of the `video_texture` settings domain (#22): texture filtering (anisotropic filtering, mipmap bias,
// light projector and decal filter) and Canvas2D (default canvas item texture filter and repeat, pixel snap,
// SDF oversize and scale, canvas shadow atlas size).
// VideoTextureSettingsServer pushes the values to the root Viewport (and further viewports) and RenderingServer.
class VideoTextureSettings : public Resource {
	GDCLASS(VideoTextureSettings, Resource);

	Viewport::AnisotropicFiltering anisotropic_filtering_level = Viewport::ANISOTROPY_4X;
	float texture_mipmap_bias = 0.0;
	RenderingServerEnums::LightProjectorFilter light_projector_filter = RenderingServerEnums::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS;
	RenderingServerEnums::DecalFilter decal_filter = RenderingServerEnums::DECAL_FILTER_LINEAR_MIPMAPS;
	Viewport::DefaultCanvasItemTextureFilter canvas_item_default_texture_filter = Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
	Viewport::DefaultCanvasItemTextureRepeat canvas_item_default_texture_repeat = Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
	bool snap_2d_transforms_to_pixel = false;
	bool snap_2d_vertices_to_pixel = false;
	Viewport::SDFOversize sdf_oversize = Viewport::SDF_OVERSIZE_120_PERCENT;
	Viewport::SDFScale sdf_scale = Viewport::SDF_SCALE_50_PERCENT;
	int canvas_shadow_atlas_size = 2048;

protected:
	static void _bind_methods();

public:
	void set_anisotropic_filtering_level(Viewport::AnisotropicFiltering p_level);
	Viewport::AnisotropicFiltering get_anisotropic_filtering_level() const;
	void set_texture_mipmap_bias(float p_bias);
	float get_texture_mipmap_bias() const;
	void set_light_projector_filter(RenderingServerEnums::LightProjectorFilter p_filter);
	RenderingServerEnums::LightProjectorFilter get_light_projector_filter() const;
	void set_decal_filter(RenderingServerEnums::DecalFilter p_filter);
	RenderingServerEnums::DecalFilter get_decal_filter() const;
	void set_canvas_item_default_texture_filter(Viewport::DefaultCanvasItemTextureFilter p_filter);
	Viewport::DefaultCanvasItemTextureFilter get_canvas_item_default_texture_filter() const;
	void set_canvas_item_default_texture_repeat(Viewport::DefaultCanvasItemTextureRepeat p_repeat);
	Viewport::DefaultCanvasItemTextureRepeat get_canvas_item_default_texture_repeat() const;
	void set_snap_2d_transforms_to_pixel(bool p_enabled);
	bool is_snap_2d_transforms_to_pixel_enabled() const;
	void set_snap_2d_vertices_to_pixel(bool p_enabled);
	bool is_snap_2d_vertices_to_pixel_enabled() const;
	void set_sdf_oversize(Viewport::SDFOversize p_oversize);
	Viewport::SDFOversize get_sdf_oversize() const;
	void set_sdf_scale(Viewport::SDFScale p_scale);
	Viewport::SDFScale get_sdf_scale() const;
	void set_canvas_shadow_atlas_size(int p_size);
	int get_canvas_shadow_atlas_size() const;
};
