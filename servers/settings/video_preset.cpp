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

Dictionary *VideoPreset::_get_domain_dictionary(const StringName &p_domain) {
	if (p_domain == SNAME("video_quality")) {
		return &video_quality;
	} else if (p_domain == SNAME("video_texture")) {
		return &video_texture;
	} else if (p_domain == SNAME("video_shadow")) {
		return &video_shadow;
	} else if (p_domain == SNAME("video_effect")) {
		return &video_effect;
	}
	return nullptr;
}

const Dictionary *VideoPreset::_get_domain_dictionary(const StringName &p_domain) const {
	return const_cast<VideoPreset *>(this)->_get_domain_dictionary(p_domain);
}

void VideoPreset::set_preset_name(const String &p_name) {
	preset_name = p_name;
	emit_changed();
}

String VideoPreset::get_preset_name() const {
	return preset_name;
}

void VideoPreset::set_video_quality(const Dictionary &p_values) {
	video_quality = p_values;
	emit_changed();
}

Dictionary VideoPreset::get_video_quality() const {
	return video_quality;
}

void VideoPreset::set_video_texture(const Dictionary &p_values) {
	video_texture = p_values;
	emit_changed();
}

Dictionary VideoPreset::get_video_texture() const {
	return video_texture;
}

void VideoPreset::set_video_shadow(const Dictionary &p_values) {
	video_shadow = p_values;
	emit_changed();
}

Dictionary VideoPreset::get_video_shadow() const {
	return video_shadow;
}

void VideoPreset::set_video_effect(const Dictionary &p_values) {
	video_effect = p_values;
	emit_changed();
}

Dictionary VideoPreset::get_video_effect() const {
	return video_effect;
}

void VideoPreset::set_domain_values(const StringName &p_domain, const Dictionary &p_values) {
	Dictionary *values = _get_domain_dictionary(p_domain);
	ERR_FAIL_NULL_MSG(values, vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	*values = p_values;
	emit_changed();
}

Dictionary VideoPreset::get_domain_values(const StringName &p_domain) const {
	const Dictionary *values = _get_domain_dictionary(p_domain);
	ERR_FAIL_NULL_V_MSG(values, Dictionary(), vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	return *values;
}

void VideoPreset::set_value(const StringName &p_domain, const StringName &p_name, const Variant &p_value) {
	Dictionary *values = _get_domain_dictionary(p_domain);
	ERR_FAIL_NULL_MSG(values, vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	// One key type (String), whatever the caller passes: keys from `.tres` files are Strings too.
	(*values)[String(p_name)] = p_value;
	emit_changed();
}

Variant VideoPreset::get_value(const StringName &p_domain, const StringName &p_name, const Variant &p_default) const {
	const Dictionary *values = _get_domain_dictionary(p_domain);
	ERR_FAIL_NULL_V_MSG(values, p_default, vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	const String key = p_name;
	if (values->has(key)) {
		return values->get(key, p_default);
	}
	return values->get(p_name, p_default);
}

bool VideoPreset::has_value(const StringName &p_domain, const StringName &p_name) const {
	const Dictionary *values = _get_domain_dictionary(p_domain);
	return values != nullptr && (values->has(String(p_name)) || values->has(p_name));
}

void VideoPreset::erase_value(const StringName &p_domain, const StringName &p_name) {
	Dictionary *values = _get_domain_dictionary(p_domain);
	ERR_FAIL_NULL_MSG(values, vformat("VideoPreset: \"%s\" is not a video settings domain of a preset (%s).", p_domain, String(", ").join(get_domains())));
	const bool erased_string = values->erase(String(p_name));
	const bool erased_name = values->erase(p_name);
	if (erased_string || erased_name) {
		emit_changed();
	}
}

void VideoPreset::clear() {
	video_quality.clear();
	video_texture.clear();
	video_shadow.clear();
	video_effect.clear();
	emit_changed();
}

void VideoPreset::_bind_methods() {
	ClassDB::bind_static_method("VideoPreset", D_METHOD("get_domains"), &VideoPreset::get_domains);

	ClassDB::bind_method(D_METHOD("set_preset_name", "name"), &VideoPreset::set_preset_name);
	ClassDB::bind_method(D_METHOD("get_preset_name"), &VideoPreset::get_preset_name);
	ClassDB::bind_method(D_METHOD("set_video_quality", "values"), &VideoPreset::set_video_quality);
	ClassDB::bind_method(D_METHOD("get_video_quality"), &VideoPreset::get_video_quality);
	ClassDB::bind_method(D_METHOD("set_video_texture", "values"), &VideoPreset::set_video_texture);
	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoPreset::get_video_texture);
	ClassDB::bind_method(D_METHOD("set_video_shadow", "values"), &VideoPreset::set_video_shadow);
	ClassDB::bind_method(D_METHOD("get_video_shadow"), &VideoPreset::get_video_shadow);
	ClassDB::bind_method(D_METHOD("set_video_effect", "values"), &VideoPreset::set_video_effect);
	ClassDB::bind_method(D_METHOD("get_video_effect"), &VideoPreset::get_video_effect);

	ClassDB::bind_method(D_METHOD("set_domain_values", "domain", "values"), &VideoPreset::set_domain_values);
	ClassDB::bind_method(D_METHOD("get_domain_values", "domain"), &VideoPreset::get_domain_values);
	ClassDB::bind_method(D_METHOD("set_value", "domain", "name", "value"), &VideoPreset::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "domain", "name", "default"), &VideoPreset::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "domain", "name"), &VideoPreset::has_value);
	ClassDB::bind_method(D_METHOD("erase_value", "domain", "name"), &VideoPreset::erase_value);
	ClassDB::bind_method(D_METHOD("clear"), &VideoPreset::clear);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preset_name"), "set_preset_name", "get_preset_name");
	ADD_GROUP("Domains", "");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "video_quality"), "set_video_quality", "get_video_quality");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "video_texture"), "set_video_texture", "get_video_texture");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "video_shadow"), "set_video_shadow", "get_video_shadow");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "video_effect"), "set_video_effect", "get_video_effect");
}
