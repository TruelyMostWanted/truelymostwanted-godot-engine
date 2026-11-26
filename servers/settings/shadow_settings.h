/**************************************************************************/
/*  shadow_settings.h                                                     */
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

// Data of the `shadow` settings domain (#23): the directional shadow atlas (size, 16-bit depth), the
// positional shadow atlas (size, 16-bit depth, subdivision of the four quadrants) and the soft shadow filter
// quality of directional and positional lights.
// ShadowSettingsServer pushes the values to the root Viewport (and further viewports) and RenderingServer.
class ShadowSettings : public Resource {
	GDCLASS(ShadowSettings, Resource);

	int directional_shadow_atlas_size = 4096;
	bool directional_shadow_16_bits = true;
	RenderingServerEnums::ShadowQuality directional_soft_shadow_filter_quality = RenderingServerEnums::SHADOW_QUALITY_SOFT_LOW;
	int positional_shadow_atlas_size = 4096;
	bool positional_shadow_atlas_16_bits = true;
	Viewport::PositionalShadowAtlasQuadrantSubdiv positional_shadow_atlas_quadrant_subdiv[4] = {
		Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4,
		Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4,
		Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_16,
		Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_64,
	};
	RenderingServerEnums::ShadowQuality positional_soft_shadow_filter_quality = RenderingServerEnums::SHADOW_QUALITY_SOFT_LOW;

protected:
	static void _bind_methods();

public:
	void set_directional_shadow_atlas_size(int p_size);
	int get_directional_shadow_atlas_size() const;
	void set_directional_shadow_16_bits(bool p_enabled);
	bool is_directional_shadow_16_bits_enabled() const;
	void set_directional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality);
	RenderingServerEnums::ShadowQuality get_directional_soft_shadow_filter_quality() const;
	void set_positional_shadow_atlas_size(int p_size);
	int get_positional_shadow_atlas_size() const;
	void set_positional_shadow_atlas_16_bits(bool p_enabled);
	bool is_positional_shadow_atlas_16_bits_enabled() const;
	void set_positional_shadow_atlas_quadrant_subdiv(int p_quadrant, Viewport::PositionalShadowAtlasQuadrantSubdiv p_subdiv);
	Viewport::PositionalShadowAtlasQuadrantSubdiv get_positional_shadow_atlas_quadrant_subdiv(int p_quadrant) const;
	void set_positional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality);
	RenderingServerEnums::ShadowQuality get_positional_soft_shadow_filter_quality() const;
};
