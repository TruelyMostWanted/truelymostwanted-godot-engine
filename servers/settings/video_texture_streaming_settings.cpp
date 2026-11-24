/**************************************************************************/
/*  video_texture_streaming_settings.cpp                                  */
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

#include "video_texture_streaming_settings.h"

#ifdef MODULE_TEXTURE_STREAMING_ENABLED

#include "core/object/class_db.h"

void VideoTextureStreamingSettings::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	emit_changed();
}

bool VideoTextureStreamingSettings::is_enabled() const {
	return enabled;
}

void VideoTextureStreamingSettings::set_memory_budget_enabled(bool p_enabled) {
	memory_budget_enabled = p_enabled;
	emit_changed();
}

bool VideoTextureStreamingSettings::is_memory_budget_enabled() const {
	return memory_budget_enabled;
}

void VideoTextureStreamingSettings::set_memory_budget_mb(int p_mb) {
	memory_budget_mb = p_mb;
	emit_changed();
}

int VideoTextureStreamingSettings::get_memory_budget_mb() const {
	return memory_budget_mb;
}

void VideoTextureStreamingSettings::set_min_lod(int p_lod) {
	min_lod = p_lod;
	emit_changed();
}

int VideoTextureStreamingSettings::get_min_lod() const {
	return min_lod;
}

void VideoTextureStreamingSettings::set_max_lod(int p_lod) {
	max_lod = p_lod;
	emit_changed();
}

int VideoTextureStreamingSettings::get_max_lod() const {
	return max_lod;
}

void VideoTextureStreamingSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &VideoTextureStreamingSettings::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &VideoTextureStreamingSettings::is_enabled);
	ClassDB::bind_method(D_METHOD("set_memory_budget_enabled", "enabled"), &VideoTextureStreamingSettings::set_memory_budget_enabled);
	ClassDB::bind_method(D_METHOD("is_memory_budget_enabled"), &VideoTextureStreamingSettings::is_memory_budget_enabled);
	ClassDB::bind_method(D_METHOD("set_memory_budget_mb", "mb"), &VideoTextureStreamingSettings::set_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("get_memory_budget_mb"), &VideoTextureStreamingSettings::get_memory_budget_mb);
	ClassDB::bind_method(D_METHOD("set_min_lod", "lod"), &VideoTextureStreamingSettings::set_min_lod);
	ClassDB::bind_method(D_METHOD("get_min_lod"), &VideoTextureStreamingSettings::get_min_lod);
	ClassDB::bind_method(D_METHOD("set_max_lod", "lod"), &VideoTextureStreamingSettings::set_max_lod);
	ClassDB::bind_method(D_METHOD("get_max_lod"), &VideoTextureStreamingSettings::get_max_lod);

	// `or_less`/`or_greater`: the Inspector shows the usual range, VideoTextureStreamingSettingsServer clamps with a warning.
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "memory_budget_enabled"), "set_memory_budget_enabled", "is_memory_budget_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "memory_budget_mb", PROPERTY_HINT_RANGE, "0,65535,1,or_greater,or_less,suffix:MiB"), "set_memory_budget_mb", "get_memory_budget_mb");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "min_lod", PROPERTY_HINT_RANGE, "0,13,1,or_greater,or_less"), "set_min_lod", "get_min_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_lod", PROPERTY_HINT_RANGE, "0,13,1,or_greater,or_less"), "set_max_lod", "get_max_lod");
}

#endif // MODULE_TEXTURE_STREAMING_ENABLED
