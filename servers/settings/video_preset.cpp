/**************************************************************************/
/*  video_preset.cpp                                                      */
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

#include "video_preset.h"

#include "core/object/class_db.h"

PackedStringArray VideoPreset::get_domains() {
	PackedStringArray domains;
	domains.push_back("render_quality");
	domains.push_back("texture");
	domains.push_back("shadow");
	domains.push_back("render_effect");
	return domains;
}

void VideoPreset::set_preset_name(const String &p_name) {
	preset_name = p_name;
	emit_changed();
}

String VideoPreset::get_preset_name() const {
	return preset_name;
}

void VideoPreset::set_render_quality(const Ref<RenderQualitySettings> &p_settings) {
	render_quality = p_settings;
	emit_changed();
}

Ref<RenderQualitySettings> VideoPreset::get_render_quality() const {
	return render_quality;
}

void VideoPreset::set_texture(const Ref<TextureSettings> &p_settings) {
	texture = p_settings;
	emit_changed();
}

Ref<TextureSettings> VideoPreset::get_texture() const {
	return texture;
}

void VideoPreset::set_shadow(const Ref<ShadowSettings> &p_settings) {
	shadow = p_settings;
	emit_changed();
}

Ref<ShadowSettings> VideoPreset::get_shadow() const {
	return shadow;
}

void VideoPreset::set_render_effect(const Ref<RenderEffectSettings> &p_settings) {
	render_effect = p_settings;
	emit_changed();
}

Ref<RenderEffectSettings> VideoPreset::get_render_effect() const {
	return render_effect;
}

void VideoPreset::set_domain_settings(const StringName &p_domain, const Ref<Resource> &p_settings) {
	if (p_domain == SNAME("render_quality")) {
		const Ref<RenderQualitySettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"render_quality\" must be a RenderQualitySettings resource.");
		set_render_quality(typed);
	} else if (p_domain == SNAME("texture")) {
		const Ref<TextureSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"texture\" must be a TextureSettings resource.");
		set_texture(typed);
	} else if (p_domain == SNAME("shadow")) {
		const Ref<ShadowSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"shadow\" must be a ShadowSettings resource.");
		set_shadow(typed);
	} else if (p_domain == SNAME("render_effect")) {
		const Ref<RenderEffectSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"render_effect\" must be a RenderEffectSettings resource.");
		set_render_effect(typed);
	} else {
		ERR_FAIL_MSG(vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	}
}

Ref<Resource> VideoPreset::get_domain_settings(const StringName &p_domain) const {
	if (p_domain == SNAME("render_quality")) {
		return render_quality;
	} else if (p_domain == SNAME("texture")) {
		return texture;
	} else if (p_domain == SNAME("shadow")) {
		return shadow;
	} else if (p_domain == SNAME("render_effect")) {
		return render_effect;
	}
	ERR_FAIL_V_MSG(Ref<Resource>(), vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
}

bool VideoPreset::has_domain_settings(const StringName &p_domain) const {
	if (!get_domains().has(p_domain)) {
		return false;
	}
	return get_domain_settings(p_domain).is_valid();
}

void VideoPreset::_bind_methods() {
	ClassDB::bind_static_method("VideoPreset", D_METHOD("get_domains"), &VideoPreset::get_domains);

	ClassDB::bind_method(D_METHOD("set_preset_name", "name"), &VideoPreset::set_preset_name);
	ClassDB::bind_method(D_METHOD("get_preset_name"), &VideoPreset::get_preset_name);
	ClassDB::bind_method(D_METHOD("set_render_quality", "settings"), &VideoPreset::set_render_quality);
	ClassDB::bind_method(D_METHOD("get_render_quality"), &VideoPreset::get_render_quality);
	ClassDB::bind_method(D_METHOD("set_texture", "settings"), &VideoPreset::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &VideoPreset::get_texture);
	ClassDB::bind_method(D_METHOD("set_shadow", "settings"), &VideoPreset::set_shadow);
	ClassDB::bind_method(D_METHOD("get_shadow"), &VideoPreset::get_shadow);
	ClassDB::bind_method(D_METHOD("set_render_effect", "settings"), &VideoPreset::set_render_effect);
	ClassDB::bind_method(D_METHOD("get_render_effect"), &VideoPreset::get_render_effect);

	ClassDB::bind_method(D_METHOD("set_domain_settings", "domain", "settings"), &VideoPreset::set_domain_settings);
	ClassDB::bind_method(D_METHOD("get_domain_settings", "domain"), &VideoPreset::get_domain_settings);
	ClassDB::bind_method(D_METHOD("has_domain_settings", "domain"), &VideoPreset::has_domain_settings);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preset_name"), "set_preset_name", "get_preset_name");
	ADD_GROUP("Domains", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "render_quality", PROPERTY_HINT_RESOURCE_TYPE, RenderQualitySettings::get_class_static()), "set_render_quality", "get_render_quality");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, TextureSettings::get_class_static()), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow", PROPERTY_HINT_RESOURCE_TYPE, ShadowSettings::get_class_static()), "set_shadow", "get_shadow");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "render_effect", PROPERTY_HINT_RESOURCE_TYPE, RenderEffectSettings::get_class_static()), "set_render_effect", "get_render_effect");
}
