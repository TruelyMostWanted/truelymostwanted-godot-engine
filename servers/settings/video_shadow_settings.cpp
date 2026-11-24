/**************************************************************************/
/*  video_shadow_settings.cpp                                             */
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

#include "video_shadow_settings.h"

#include "core/object/class_db.h"

void VideoShadowSettings::set_directional_shadow_atlas_size(int p_size) {
	directional_shadow_atlas_size = p_size;
	emit_changed();
}

int VideoShadowSettings::get_directional_shadow_atlas_size() const {
	return directional_shadow_atlas_size;
}

void VideoShadowSettings::set_directional_shadow_16_bits(bool p_enabled) {
	directional_shadow_16_bits = p_enabled;
	emit_changed();
}

bool VideoShadowSettings::is_directional_shadow_16_bits_enabled() const {
	return directional_shadow_16_bits;
}

void VideoShadowSettings::set_directional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality) {
	directional_soft_shadow_filter_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::ShadowQuality VideoShadowSettings::get_directional_soft_shadow_filter_quality() const {
	return directional_soft_shadow_filter_quality;
}

void VideoShadowSettings::set_positional_shadow_atlas_size(int p_size) {
	positional_shadow_atlas_size = p_size;
	emit_changed();
}

int VideoShadowSettings::get_positional_shadow_atlas_size() const {
	return positional_shadow_atlas_size;
}

void VideoShadowSettings::set_positional_shadow_atlas_16_bits(bool p_enabled) {
	positional_shadow_atlas_16_bits = p_enabled;
	emit_changed();
}

bool VideoShadowSettings::is_positional_shadow_atlas_16_bits_enabled() const {
	return positional_shadow_atlas_16_bits;
}

void VideoShadowSettings::set_positional_shadow_atlas_quadrant_subdiv(int p_quadrant, Viewport::PositionalShadowAtlasQuadrantSubdiv p_subdiv) {
	ERR_FAIL_INDEX(p_quadrant, 4);
	positional_shadow_atlas_quadrant_subdiv[p_quadrant] = p_subdiv;
	emit_changed();
}

Viewport::PositionalShadowAtlasQuadrantSubdiv VideoShadowSettings::get_positional_shadow_atlas_quadrant_subdiv(int p_quadrant) const {
	ERR_FAIL_INDEX_V(p_quadrant, 4, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	return positional_shadow_atlas_quadrant_subdiv[p_quadrant];
}

void VideoShadowSettings::set_positional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality) {
	positional_soft_shadow_filter_quality = p_quality;
	emit_changed();
}

RenderingServerEnums::ShadowQuality VideoShadowSettings::get_positional_soft_shadow_filter_quality() const {
	return positional_soft_shadow_filter_quality;
}

void VideoShadowSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_directional_shadow_atlas_size", "size"), &VideoShadowSettings::set_directional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_directional_shadow_atlas_size"), &VideoShadowSettings::get_directional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("set_directional_shadow_16_bits", "enabled"), &VideoShadowSettings::set_directional_shadow_16_bits);
	ClassDB::bind_method(D_METHOD("is_directional_shadow_16_bits_enabled"), &VideoShadowSettings::is_directional_shadow_16_bits_enabled);
	ClassDB::bind_method(D_METHOD("set_directional_soft_shadow_filter_quality", "quality"), &VideoShadowSettings::set_directional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("get_directional_soft_shadow_filter_quality"), &VideoShadowSettings::get_directional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_size", "size"), &VideoShadowSettings::set_positional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_size"), &VideoShadowSettings::get_positional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_16_bits", "enabled"), &VideoShadowSettings::set_positional_shadow_atlas_16_bits);
	ClassDB::bind_method(D_METHOD("is_positional_shadow_atlas_16_bits_enabled"), &VideoShadowSettings::is_positional_shadow_atlas_16_bits_enabled);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_quadrant_subdiv", "quadrant", "subdiv"), &VideoShadowSettings::set_positional_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_quadrant_subdiv", "quadrant"), &VideoShadowSettings::get_positional_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("set_positional_soft_shadow_filter_quality", "quality"), &VideoShadowSettings::set_positional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("get_positional_soft_shadow_filter_quality"), &VideoShadowSettings::get_positional_soft_shadow_filter_quality);

	// `or_less`/`or_greater`: the Inspector shows the usual range, VideoShadowSettingsServer clamps with a warning.
	ADD_GROUP("Directional Shadow", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "directional_shadow_atlas_size", PROPERTY_HINT_RANGE, "256,16384,1,or_greater,or_less,suffix:px"), "set_directional_shadow_atlas_size", "get_directional_shadow_atlas_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "directional_shadow_16_bits"), "set_directional_shadow_16_bits", "is_directional_shadow_16_bits_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "directional_soft_shadow_filter_quality", PROPERTY_HINT_ENUM, "Hard,Soft Very Low,Soft Low,Soft Medium,Soft High,Soft Ultra"), "set_directional_soft_shadow_filter_quality", "get_directional_soft_shadow_filter_quality");
	ADD_GROUP("Positional Shadow", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "positional_shadow_atlas_size", PROPERTY_HINT_RANGE, "0,16384,1,or_greater,or_less,suffix:px"), "set_positional_shadow_atlas_size", "get_positional_shadow_atlas_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "positional_shadow_atlas_16_bits"), "set_positional_shadow_atlas_16_bits", "is_positional_shadow_atlas_16_bits_enabled");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_0", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 0);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_1", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 1);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_2", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 2);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_3", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 3);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "positional_soft_shadow_filter_quality", PROPERTY_HINT_ENUM, "Hard,Soft Very Low,Soft Low,Soft Medium,Soft High,Soft Ultra"), "set_positional_soft_shadow_filter_quality", "get_positional_soft_shadow_filter_quality");
}
