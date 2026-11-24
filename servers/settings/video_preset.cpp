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
	domains.push_back("video_quality");
	domains.push_back("video_texture");
	domains.push_back("video_shadow");
	domains.push_back("video_effect");
	return domains;
}

void VideoPreset::set_preset_name(const String &p_name) {
	preset_name = p_name;
	emit_changed();
}

String VideoPreset::get_preset_name() const {
	return preset_name;
}

void VideoPreset::set_video_quality(const Ref<VideoQualitySettings> &p_settings) {
	video_quality = p_settings;
	emit_changed();
}

Ref<VideoQualitySettings> VideoPreset::get_video_quality() const {
	return video_quality;
}

void VideoPreset::set_video_texture(const Ref<VideoTextureSettings> &p_settings) {
	video_texture = p_settings;
	emit_changed();
}

Ref<VideoTextureSettings> VideoPreset::get_video_texture() const {
	return video_texture;
}

void VideoPreset::set_video_shadow(const Ref<VideoShadowSettings> &p_settings) {
	video_shadow = p_settings;
	emit_changed();
}

Ref<VideoShadowSettings> VideoPreset::get_video_shadow() const {
	return video_shadow;
}

void VideoPreset::set_video_effect(const Ref<VideoEffectSettings> &p_settings) {
	video_effect = p_settings;
	emit_changed();
}

Ref<VideoEffectSettings> VideoPreset::get_video_effect() const {
	return video_effect;
}

void VideoPreset::set_domain_settings(const StringName &p_domain, const Ref<Resource> &p_settings) {
	if (p_domain == SNAME("video_quality")) {
		const Ref<VideoQualitySettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"video_quality\" must be a VideoQualitySettings resource.");
		set_video_quality(typed);
	} else if (p_domain == SNAME("video_texture")) {
		const Ref<VideoTextureSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"video_texture\" must be a VideoTextureSettings resource.");
		set_video_texture(typed);
	} else if (p_domain == SNAME("video_shadow")) {
		const Ref<VideoShadowSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"video_shadow\" must be a VideoShadowSettings resource.");
		set_video_shadow(typed);
	} else if (p_domain == SNAME("video_effect")) {
		const Ref<VideoEffectSettings> typed = p_settings;
		ERR_FAIL_COND_MSG(p_settings.is_valid() && typed.is_null(), "VideoPreset: the settings of \"video_effect\" must be a VideoEffectSettings resource.");
		set_video_effect(typed);
	} else {
		ERR_FAIL_MSG(vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	}
}

Ref<Resource> VideoPreset::get_domain_settings(const StringName &p_domain) const {
	if (p_domain == SNAME("video_quality")) {
		return video_quality;
	} else if (p_domain == SNAME("video_texture")) {
		return video_texture;
	} else if (p_domain == SNAME("video_shadow")) {
		return video_shadow;
	} else if (p_domain == SNAME("video_effect")) {
		return video_effect;
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
	ClassDB::bind_method(D_METHOD("set_video_quality", "settings"), &VideoPreset::set_video_quality);
	ClassDB::bind_method(D_METHOD("get_video_quality"), &VideoPreset::get_video_quality);
	ClassDB::bind_method(D_METHOD("set_video_texture", "settings"), &VideoPreset::set_video_texture);
	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoPreset::get_video_texture);
	ClassDB::bind_method(D_METHOD("set_video_shadow", "settings"), &VideoPreset::set_video_shadow);
	ClassDB::bind_method(D_METHOD("get_video_shadow"), &VideoPreset::get_video_shadow);
	ClassDB::bind_method(D_METHOD("set_video_effect", "settings"), &VideoPreset::set_video_effect);
	ClassDB::bind_method(D_METHOD("get_video_effect"), &VideoPreset::get_video_effect);

	ClassDB::bind_method(D_METHOD("set_domain_settings", "domain", "settings"), &VideoPreset::set_domain_settings);
	ClassDB::bind_method(D_METHOD("get_domain_settings", "domain"), &VideoPreset::get_domain_settings);
	ClassDB::bind_method(D_METHOD("has_domain_settings", "domain"), &VideoPreset::has_domain_settings);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preset_name"), "set_preset_name", "get_preset_name");
	ADD_GROUP("Domains", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "video_quality", PROPERTY_HINT_RESOURCE_TYPE, VideoQualitySettings::get_class_static()), "set_video_quality", "get_video_quality");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "video_texture", PROPERTY_HINT_RESOURCE_TYPE, VideoTextureSettings::get_class_static()), "set_video_texture", "get_video_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "video_shadow", PROPERTY_HINT_RESOURCE_TYPE, VideoShadowSettings::get_class_static()), "set_video_shadow", "get_video_shadow");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "video_effect", PROPERTY_HINT_RESOURCE_TYPE, VideoEffectSettings::get_class_static()), "set_video_effect", "get_video_effect");
}
