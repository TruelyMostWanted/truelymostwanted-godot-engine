/**************************************************************************/
/*  video_window_settings.cpp                                             */
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

#include "video_window_settings.h"

#include "core/object/class_db.h"

void VideoWindowSettings::set_window_mode(DisplayServerEnums::WindowMode p_mode) {
	window_mode = p_mode;
	emit_changed();
}

DisplayServerEnums::WindowMode VideoWindowSettings::get_window_mode() const {
	return window_mode;
}

void VideoWindowSettings::set_window_size(const Size2i &p_size) {
	window_size = p_size;
	emit_changed();
}

Size2i VideoWindowSettings::get_window_size() const {
	return window_size;
}

void VideoWindowSettings::set_screen(int p_screen) {
	screen = p_screen;
	emit_changed();
}

int VideoWindowSettings::get_screen() const {
	return screen;
}

void VideoWindowSettings::set_window_centered(bool p_centered) {
	window_centered = p_centered;
	emit_changed();
}

bool VideoWindowSettings::is_window_centered() const {
	return window_centered;
}

void VideoWindowSettings::set_window_position(const Point2i &p_position) {
	window_position = p_position;
	emit_changed();
}

Point2i VideoWindowSettings::get_window_position() const {
	return window_position;
}

void VideoWindowSettings::set_borderless(bool p_borderless) {
	borderless = p_borderless;
	emit_changed();
}

bool VideoWindowSettings::is_borderless() const {
	return borderless;
}

void VideoWindowSettings::set_always_on_top(bool p_always_on_top) {
	always_on_top = p_always_on_top;
	emit_changed();
}

bool VideoWindowSettings::is_always_on_top() const {
	return always_on_top;
}

void VideoWindowSettings::set_unresizable(bool p_unresizable) {
	unresizable = p_unresizable;
	emit_changed();
}

bool VideoWindowSettings::is_unresizable() const {
	return unresizable;
}

void VideoWindowSettings::set_transparent(bool p_transparent) {
	transparent = p_transparent;
	emit_changed();
}

bool VideoWindowSettings::is_transparent() const {
	return transparent;
}

void VideoWindowSettings::set_vsync_mode(DisplayServerEnums::VSyncMode p_mode) {
	vsync_mode = p_mode;
	emit_changed();
}

DisplayServerEnums::VSyncMode VideoWindowSettings::get_vsync_mode() const {
	return vsync_mode;
}

void VideoWindowSettings::set_content_scale_mode(Window::ContentScaleMode p_mode) {
	content_scale_mode = p_mode;
	emit_changed();
}

Window::ContentScaleMode VideoWindowSettings::get_content_scale_mode() const {
	return content_scale_mode;
}

void VideoWindowSettings::set_content_scale_aspect(Window::ContentScaleAspect p_aspect) {
	content_scale_aspect = p_aspect;
	emit_changed();
}

Window::ContentScaleAspect VideoWindowSettings::get_content_scale_aspect() const {
	return content_scale_aspect;
}

void VideoWindowSettings::set_content_scale_stretch(Window::ContentScaleStretch p_stretch) {
	content_scale_stretch = p_stretch;
	emit_changed();
}

Window::ContentScaleStretch VideoWindowSettings::get_content_scale_stretch() const {
	return content_scale_stretch;
}

void VideoWindowSettings::set_content_scale_factor(float p_factor) {
	content_scale_factor = p_factor;
	emit_changed();
}

float VideoWindowSettings::get_content_scale_factor() const {
	return content_scale_factor;
}

void VideoWindowSettings::set_hdr_output_requested(bool p_requested) {
	hdr_output_requested = p_requested;
	emit_changed();
}

bool VideoWindowSettings::is_hdr_output_requested() const {
	return hdr_output_requested;
}

void VideoWindowSettings::set_hdr_reference_luminance(float p_luminance) {
	hdr_reference_luminance = p_luminance;
	emit_changed();
}

float VideoWindowSettings::get_hdr_reference_luminance() const {
	return hdr_reference_luminance;
}

void VideoWindowSettings::set_hdr_max_luminance(float p_luminance) {
	hdr_max_luminance = p_luminance;
	emit_changed();
}

float VideoWindowSettings::get_hdr_max_luminance() const {
	return hdr_max_luminance;
}

void VideoWindowSettings::set_window_states(const Dictionary &p_states) {
	window_states = p_states;
	emit_changed();
}

Dictionary VideoWindowSettings::get_window_states() const {
	return window_states;
}

void VideoWindowSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_window_mode", "mode"), &VideoWindowSettings::set_window_mode);
	ClassDB::bind_method(D_METHOD("get_window_mode"), &VideoWindowSettings::get_window_mode);
	ClassDB::bind_method(D_METHOD("set_window_size", "size"), &VideoWindowSettings::set_window_size);
	ClassDB::bind_method(D_METHOD("get_window_size"), &VideoWindowSettings::get_window_size);
	ClassDB::bind_method(D_METHOD("set_screen", "screen"), &VideoWindowSettings::set_screen);
	ClassDB::bind_method(D_METHOD("get_screen"), &VideoWindowSettings::get_screen);
	ClassDB::bind_method(D_METHOD("set_window_centered", "centered"), &VideoWindowSettings::set_window_centered);
	ClassDB::bind_method(D_METHOD("is_window_centered"), &VideoWindowSettings::is_window_centered);
	ClassDB::bind_method(D_METHOD("set_window_position", "position"), &VideoWindowSettings::set_window_position);
	ClassDB::bind_method(D_METHOD("get_window_position"), &VideoWindowSettings::get_window_position);
	ClassDB::bind_method(D_METHOD("set_borderless", "borderless"), &VideoWindowSettings::set_borderless);
	ClassDB::bind_method(D_METHOD("is_borderless"), &VideoWindowSettings::is_borderless);
	ClassDB::bind_method(D_METHOD("set_always_on_top", "always_on_top"), &VideoWindowSettings::set_always_on_top);
	ClassDB::bind_method(D_METHOD("is_always_on_top"), &VideoWindowSettings::is_always_on_top);
	ClassDB::bind_method(D_METHOD("set_unresizable", "unresizable"), &VideoWindowSettings::set_unresizable);
	ClassDB::bind_method(D_METHOD("is_unresizable"), &VideoWindowSettings::is_unresizable);
	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &VideoWindowSettings::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &VideoWindowSettings::is_transparent);
	ClassDB::bind_method(D_METHOD("set_vsync_mode", "mode"), &VideoWindowSettings::set_vsync_mode);
	ClassDB::bind_method(D_METHOD("get_vsync_mode"), &VideoWindowSettings::get_vsync_mode);
	ClassDB::bind_method(D_METHOD("set_content_scale_mode", "mode"), &VideoWindowSettings::set_content_scale_mode);
	ClassDB::bind_method(D_METHOD("get_content_scale_mode"), &VideoWindowSettings::get_content_scale_mode);
	ClassDB::bind_method(D_METHOD("set_content_scale_aspect", "aspect"), &VideoWindowSettings::set_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("get_content_scale_aspect"), &VideoWindowSettings::get_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("set_content_scale_stretch", "stretch"), &VideoWindowSettings::set_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("get_content_scale_stretch"), &VideoWindowSettings::get_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("set_content_scale_factor", "factor"), &VideoWindowSettings::set_content_scale_factor);
	ClassDB::bind_method(D_METHOD("get_content_scale_factor"), &VideoWindowSettings::get_content_scale_factor);
	ClassDB::bind_method(D_METHOD("set_hdr_output_requested", "requested"), &VideoWindowSettings::set_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("is_hdr_output_requested"), &VideoWindowSettings::is_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("set_hdr_reference_luminance", "luminance"), &VideoWindowSettings::set_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_reference_luminance"), &VideoWindowSettings::get_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("set_hdr_max_luminance", "luminance"), &VideoWindowSettings::set_hdr_max_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_max_luminance"), &VideoWindowSettings::get_hdr_max_luminance);
	ClassDB::bind_method(D_METHOD("set_window_states", "states"), &VideoWindowSettings::set_window_states);
	ClassDB::bind_method(D_METHOD("get_window_states"), &VideoWindowSettings::get_window_states);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "window_mode", PROPERTY_HINT_ENUM, "Windowed,Minimized,Maximized,Fullscreen,Exclusive Fullscreen"), "set_window_mode", "get_window_mode");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "window_size", PROPERTY_HINT_NONE, "suffix:px"), "set_window_size", "get_window_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "screen", PROPERTY_HINT_RANGE, "-2,16,1,or_greater"), "set_screen", "get_screen");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "window_centered"), "set_window_centered", "is_window_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "window_position", PROPERTY_HINT_NONE, "suffix:px"), "set_window_position", "get_window_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "borderless"), "set_borderless", "is_borderless");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "always_on_top"), "set_always_on_top", "is_always_on_top");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unresizable"), "set_unresizable", "is_unresizable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent"), "set_transparent", "is_transparent");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vsync_mode", PROPERTY_HINT_ENUM, "Disabled,Enabled,Adaptive,Mailbox"), "set_vsync_mode", "get_vsync_mode");
	ADD_GROUP("Content Scale", "content_scale_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_mode", PROPERTY_HINT_ENUM, "Disabled,Canvas Items,Viewport"), "set_content_scale_mode", "get_content_scale_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_aspect", PROPERTY_HINT_ENUM, "Ignore,Keep,Keep Width,Keep Height,Expand"), "set_content_scale_aspect", "get_content_scale_aspect");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "content_scale_stretch", PROPERTY_HINT_ENUM, "Fractional,Integer"), "set_content_scale_stretch", "get_content_scale_stretch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "content_scale_factor", PROPERTY_HINT_RANGE, "0.1,8,0.01,or_greater"), "set_content_scale_factor", "get_content_scale_factor");
	ADD_GROUP("HDR", "hdr_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hdr_output_requested"), "set_hdr_output_requested", "is_hdr_output_requested");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hdr_reference_luminance", PROPERTY_HINT_RANGE, "-1,10000,0.1,suffix:nits"), "set_hdr_reference_luminance", "get_hdr_reference_luminance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hdr_max_luminance", PROPERTY_HINT_RANGE, "-1,10000,0.1,suffix:nits"), "set_hdr_max_luminance", "get_hdr_max_luminance");
	ADD_GROUP("", "");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "window_states"), "set_window_states", "get_window_states");
}
