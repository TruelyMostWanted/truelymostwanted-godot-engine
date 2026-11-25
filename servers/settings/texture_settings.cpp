/**************************************************************************/
/*  texture_settings.cpp                                                  */
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

#include "texture_settings.h"

#include "core/object/class_db.h"

void TextureSettings::set_anisotropic_filtering_level(Viewport::AnisotropicFiltering p_level) {
	anisotropic_filtering_level = p_level;
	emit_changed();
}

Viewport::AnisotropicFiltering TextureSettings::get_anisotropic_filtering_level() const {
	return anisotropic_filtering_level;
}

void TextureSettings::set_texture_mipmap_bias(float p_bias) {
	texture_mipmap_bias = p_bias;
	emit_changed();
}

float TextureSettings::get_texture_mipmap_bias() const {
	return texture_mipmap_bias;
}

void TextureSettings::set_light_projector_filter(RenderingServerEnums::LightProjectorFilter p_filter) {
	light_projector_filter = p_filter;
	emit_changed();
}

RenderingServerEnums::LightProjectorFilter TextureSettings::get_light_projector_filter() const {
	return light_projector_filter;
}

void TextureSettings::set_decal_filter(RenderingServerEnums::DecalFilter p_filter) {
	decal_filter = p_filter;
	emit_changed();
}

RenderingServerEnums::DecalFilter TextureSettings::get_decal_filter() const {
	return decal_filter;
}

void TextureSettings::set_canvas_item_default_texture_filter(Viewport::DefaultCanvasItemTextureFilter p_filter) {
	canvas_item_default_texture_filter = p_filter;
	emit_changed();
}

Viewport::DefaultCanvasItemTextureFilter TextureSettings::get_canvas_item_default_texture_filter() const {
	return canvas_item_default_texture_filter;
}

void TextureSettings::set_canvas_item_default_texture_repeat(Viewport::DefaultCanvasItemTextureRepeat p_repeat) {
	canvas_item_default_texture_repeat = p_repeat;
	emit_changed();
}

Viewport::DefaultCanvasItemTextureRepeat TextureSettings::get_canvas_item_default_texture_repeat() const {
	return canvas_item_default_texture_repeat;
}

void TextureSettings::set_snap_2d_transforms_to_pixel(bool p_enabled) {
	snap_2d_transforms_to_pixel = p_enabled;
	emit_changed();
}

bool TextureSettings::is_snap_2d_transforms_to_pixel_enabled() const {
	return snap_2d_transforms_to_pixel;
}

void TextureSettings::set_snap_2d_vertices_to_pixel(bool p_enabled) {
	snap_2d_vertices_to_pixel = p_enabled;
	emit_changed();
}

bool TextureSettings::is_snap_2d_vertices_to_pixel_enabled() const {
	return snap_2d_vertices_to_pixel;
}

void TextureSettings::set_sdf_oversize(Viewport::SDFOversize p_oversize) {
	sdf_oversize = p_oversize;
	emit_changed();
}

Viewport::SDFOversize TextureSettings::get_sdf_oversize() const {
	return sdf_oversize;
}

void TextureSettings::set_sdf_scale(Viewport::SDFScale p_scale) {
	sdf_scale = p_scale;
	emit_changed();
}

Viewport::SDFScale TextureSettings::get_sdf_scale() const {
	return sdf_scale;
}

void TextureSettings::set_canvas_shadow_atlas_size(int p_size) {
	canvas_shadow_atlas_size = p_size;
	emit_changed();
}

int TextureSettings::get_canvas_shadow_atlas_size() const {
	return canvas_shadow_atlas_size;
}

void TextureSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_anisotropic_filtering_level", "level"), &TextureSettings::set_anisotropic_filtering_level);
	ClassDB::bind_method(D_METHOD("get_anisotropic_filtering_level"), &TextureSettings::get_anisotropic_filtering_level);
	ClassDB::bind_method(D_METHOD("set_texture_mipmap_bias", "bias"), &TextureSettings::set_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("get_texture_mipmap_bias"), &TextureSettings::get_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("set_light_projector_filter", "filter"), &TextureSettings::set_light_projector_filter);
	ClassDB::bind_method(D_METHOD("get_light_projector_filter"), &TextureSettings::get_light_projector_filter);
	ClassDB::bind_method(D_METHOD("set_decal_filter", "filter"), &TextureSettings::set_decal_filter);
	ClassDB::bind_method(D_METHOD("get_decal_filter"), &TextureSettings::get_decal_filter);
	ClassDB::bind_method(D_METHOD("set_canvas_item_default_texture_filter", "filter"), &TextureSettings::set_canvas_item_default_texture_filter);
	ClassDB::bind_method(D_METHOD("get_canvas_item_default_texture_filter"), &TextureSettings::get_canvas_item_default_texture_filter);
	ClassDB::bind_method(D_METHOD("set_canvas_item_default_texture_repeat", "repeat"), &TextureSettings::set_canvas_item_default_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_canvas_item_default_texture_repeat"), &TextureSettings::get_canvas_item_default_texture_repeat);
	ClassDB::bind_method(D_METHOD("set_snap_2d_transforms_to_pixel", "enabled"), &TextureSettings::set_snap_2d_transforms_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_transforms_to_pixel_enabled"), &TextureSettings::is_snap_2d_transforms_to_pixel_enabled);
	ClassDB::bind_method(D_METHOD("set_snap_2d_vertices_to_pixel", "enabled"), &TextureSettings::set_snap_2d_vertices_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_vertices_to_pixel_enabled"), &TextureSettings::is_snap_2d_vertices_to_pixel_enabled);
	ClassDB::bind_method(D_METHOD("set_sdf_oversize", "oversize"), &TextureSettings::set_sdf_oversize);
	ClassDB::bind_method(D_METHOD("get_sdf_oversize"), &TextureSettings::get_sdf_oversize);
	ClassDB::bind_method(D_METHOD("set_sdf_scale", "scale"), &TextureSettings::set_sdf_scale);
	ClassDB::bind_method(D_METHOD("get_sdf_scale"), &TextureSettings::get_sdf_scale);
	ClassDB::bind_method(D_METHOD("set_canvas_shadow_atlas_size", "size"), &TextureSettings::set_canvas_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_canvas_shadow_atlas_size"), &TextureSettings::get_canvas_shadow_atlas_size);

	// `or_less`/`or_greater`: the Inspector shows the usual range, TextureSettingsServer clamps with a warning.
	ADD_GROUP("Texture Filtering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "anisotropic_filtering_level", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x,16x"), "set_anisotropic_filtering_level", "get_anisotropic_filtering_level");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_mipmap_bias", PROPERTY_HINT_RANGE, "-2,2,0.001,or_greater,or_less"), "set_texture_mipmap_bias", "get_texture_mipmap_bias");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_projector_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_light_projector_filter", "get_light_projector_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "decal_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_decal_filter", "get_decal_filter");
	ADD_GROUP("Canvas 2D", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_item_default_texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Linear Mipmap,Nearest Mipmap,Parent Node"), "set_canvas_item_default_texture_filter", "get_canvas_item_default_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_item_default_texture_repeat", PROPERTY_HINT_ENUM, "Disabled,Enabled,Mirror,Parent Node"), "set_canvas_item_default_texture_repeat", "get_canvas_item_default_texture_repeat");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "snap_2d_transforms_to_pixel"), "set_snap_2d_transforms_to_pixel", "is_snap_2d_transforms_to_pixel_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "snap_2d_vertices_to_pixel"), "set_snap_2d_vertices_to_pixel", "is_snap_2d_vertices_to_pixel_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_oversize", PROPERTY_HINT_ENUM, "100%,120%,150%,200%"), "set_sdf_oversize", "get_sdf_oversize");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_scale", PROPERTY_HINT_ENUM, "100%,50%,25%"), "set_sdf_scale", "get_sdf_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_shadow_atlas_size", PROPERTY_HINT_RANGE, "128,16384,1,or_greater,or_less,suffix:px"), "set_canvas_shadow_atlas_size", "get_canvas_shadow_atlas_size");
}
