/**************************************************************************/
/*  video_preset.h                                                        */
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
#include "servers/settings/video_effect_settings.h"
#include "servers/settings/video_quality_settings.h"
#include "servers/settings/video_shadow_settings.h"
#include "servers/settings/video_texture_settings.h"

// A graphics preset (#25): a name and one settings resource per video settings domain (`video_quality`,
// `video_texture`, `video_shadow`, `video_effect`), the same classes the domains store. A domain resource sets
// all values of its domain; a null resource leaves the domain as it is.
// VideoPresetSettingsServer applies presets; the four built-in presets are created in code, projects add or
// replace presets with `.tres` files (project setting `settings/video_preset/presets`).
class VideoPreset : public Resource {
	GDCLASS(VideoPreset, Resource);

	String preset_name;
	Ref<VideoQualitySettings> video_quality;
	Ref<VideoTextureSettings> video_texture;
	Ref<VideoShadowSettings> video_shadow;
	Ref<VideoEffectSettings> video_effect;

protected:
	static void _bind_methods();

public:
	// The settings domains a preset covers, in the order they are applied.
	static PackedStringArray get_domains();

	void set_preset_name(const String &p_name);
	String get_preset_name() const;

	void set_video_quality(const Ref<VideoQualitySettings> &p_settings);
	Ref<VideoQualitySettings> get_video_quality() const;
	void set_video_texture(const Ref<VideoTextureSettings> &p_settings);
	Ref<VideoTextureSettings> get_video_texture() const;
	void set_video_shadow(const Ref<VideoShadowSettings> &p_settings);
	Ref<VideoShadowSettings> get_video_shadow() const;
	void set_video_effect(const Ref<VideoEffectSettings> &p_settings);
	Ref<VideoEffectSettings> get_video_effect() const;

	// Generic access by domain name (one of get_domains()); `p_settings` must be the class of the domain or null.
	void set_domain_settings(const StringName &p_domain, const Ref<Resource> &p_settings);
	Ref<Resource> get_domain_settings(const StringName &p_domain) const;
	bool has_domain_settings(const StringName &p_domain) const;
};
