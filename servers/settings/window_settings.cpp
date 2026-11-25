/**************************************************************************/
/*  window_settings.cpp                                                   */
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

#include "window_settings.h"

#include "core/object/class_db.h"

void WindowSettings::set_window_mode(DisplayServerEnums::WindowMode p_mode) {
	window_mode = p_mode;
	emit_changed();
}

DisplayServerEnums::WindowMode WindowSettings::get_window_mode() const {
	return window_mode;
}

void WindowSettings::set_window_size(const Size2i &p_size) {
	window_size = p_size;
	emit_changed();
}

Size2i WindowSettings::get_window_size() const {
	return window_size;
}

void WindowSettings::set_screen(int p_screen) {
	screen = p_screen;
	emit_changed();
}

int WindowSettings::get_screen() const {
	return screen;
}

void WindowSettings::set_window_centered(bool p_centered) {
	window_centered = p_centered;
	emit_changed();
}

bool WindowSettings::is_window_centered() const {
	return window_centered;
}

void WindowSettings::set_window_position(const Point2i &p_position) {
	window_position = p_position;
	emit_changed();
}

Point2i WindowSettings::get_window_position() const {
	return window_position;
}

void WindowSettings::set_borderless(bool p_borderless) {
	borderless = p_borderless;
	emit_changed();
}

bool WindowSettings::is_borderless() const {
	return borderless;
}

void WindowSettings::set_always_on_top(bool p_always_on_top) {
	always_on_top = p_always_on_top;
	emit_changed();
}

bool WindowSettings::is_always_on_top() const {
	return always_on_top;
}

void WindowSettings::set_unresizable(bool p_unresizable) {
	unresizable = p_unresizable;
	emit_changed();
}

bool WindowSettings::is_unresizable() const {
	return unresizable;
}

void WindowSettings::set_transparent(bool p_transparent) {
	transparent = p_transparent;
	emit_changed();
}

bool WindowSettings::is_transparent() const {
	return transparent;
}

void WindowSettings::set_vsync_mode(DisplayServerEnums::VSyncMode p_mode) {
	vsync_mode = p_mode;
	emit_changed();
}

DisplayServerEnums::VSyncMode WindowSettings::get_vsync_mode() const {
	return vsync_mode;
}

void WindowSettings::set_content_scale_mode(Window::ContentScaleMode p_mode) {
	content_scale_mode = p_mode;
	emit_changed();
}

Window::ContentScaleMode WindowSettings::get_content_scale_mode() const {
	return content_scale_mode;
}

void WindowSettings::set_content_scale_aspect(Window::ContentScaleAspect p_aspect) {
	content_scale_aspect = p_aspect;
	emit_changed();
}

Window::ContentScaleAspect WindowSettings::get_content_scale_aspect() const {
	return content_scale_aspect;
}

void WindowSettings::set_content_scale_stretch(Window::ContentScaleStretch p_stretch) {
	content_scale_stretch = p_stretch;
	emit_changed();
}

Window::ContentScaleStretch WindowSettings::get_content_scale_stretch() const {
	return content_scale_stretch;
}

void WindowSettings::set_content_scale_factor(float p_factor) {
	content_scale_factor = p_factor;
	emit_changed();
}

float WindowSettings::get_content_scale_factor() const {
	return content_scale_factor;
}

void WindowSettings::set_hdr_output_requested(bool p_requested) {
	hdr_output_requested = p_requested;
	emit_changed();
}

bool WindowSettings::is_hdr_output_requested() const {
	return hdr_output_requested;
}

void WindowSettings::set_hdr_reference_luminance(float p_luminance) {
	hdr_reference_luminance = p_luminance;
	emit_changed();
}

float WindowSettings::get_hdr_reference_luminance() const {
	return hdr_reference_luminance;
}

void WindowSettings::set_hdr_max_luminance(float p_luminance) {
	hdr_max_luminance = p_luminance;
	emit_changed();
}

float WindowSettings::get_hdr_max_luminance() const {
	return hdr_max_luminance;
}

void WindowSettings::set_window_states(const Dictionary &p_states) {
	window_states = p_states;
	emit_changed();
}

Dictionary WindowSettings::get_window_states() const {
	return window_states;
}

void WindowSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_window_mode", "mode"), &WindowSettings::set_window_mode);
	ClassDB::bind_method(D_METHOD("get_window_mode"), &WindowSettings::get_window_mode);
	ClassDB::bind_method(D_METHOD("set_window_size", "size"), &WindowSettings::set_window_size);
	ClassDB::bind_method(D_METHOD("get_window_size"), &WindowSettings::get_window_size);
	ClassDB::bind_method(D_METHOD("set_screen", "screen"), &WindowSettings::set_screen);
	ClassDB::bind_method(D_METHOD("get_screen"), &WindowSettings::get_screen);
	ClassDB::bind_method(D_METHOD("set_window_centered", "centered"), &WindowSettings::set_window_centered);
	ClassDB::bind_method(D_METHOD("is_window_centered"), &WindowSettings::is_window_centered);
	ClassDB::bind_method(D_METHOD("set_window_position", "position"), &WindowSettings::set_window_position);
	ClassDB::bind_method(D_METHOD("get_window_position"), &WindowSettings::get_window_position);
	ClassDB::bind_method(D_METHOD("set_borderless", "borderless"), &WindowSettings::set_borderless);
	ClassDB::bind_method(D_METHOD("is_borderless"), &WindowSettings::is_borderless);
	ClassDB::bind_method(D_METHOD("set_always_on_top", "always_on_top"), &WindowSettings::set_always_on_top);
	ClassDB::bind_method(D_METHOD("is_always_on_top"), &WindowSettings::is_always_on_top);
	ClassDB::bind_method(D_METHOD("set_unresizable", "unresizable"), &WindowSettings::set_unresizable);
	ClassDB::bind_method(D_METHOD("is_unresizable"), &WindowSettings::is_unresizable);
	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &WindowSettings::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &WindowSettings::is_transparent);
	ClassDB::bind_method(D_METHOD("set_vsync_mode", "mode"), &WindowSettings::set_vsync_mode);
	ClassDB::bind_method(D_METHOD("get_vsync_mode"), &WindowSettings::get_vsync_mode);
	ClassDB::bind_method(D_METHOD("set_content_scale_mode", "mode"), &WindowSettings::set_content_scale_mode);
	ClassDB::bind_method(D_METHOD("get_content_scale_mode"), &WindowSettings::get_content_scale_mode);
	ClassDB::bind_method(D_METHOD("set_content_scale_aspect", "aspect"), &WindowSettings::set_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("get_content_scale_aspect"), &WindowSettings::get_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("set_content_scale_stretch", "stretch"), &WindowSettings::set_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("get_content_scale_stretch"), &WindowSettings::get_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("set_content_scale_factor", "factor"), &WindowSettings::set_content_scale_factor);
	ClassDB::bind_method(D_METHOD("get_content_scale_factor"), &WindowSettings::get_content_scale_factor);
	ClassDB::bind_method(D_METHOD("set_hdr_output_requested", "requested"), &WindowSettings::set_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("is_hdr_output_requested"), &WindowSettings::is_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("set_hdr_reference_luminance", "luminance"), &WindowSettings::set_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_reference_luminance"), &WindowSettings::get_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("set_hdr_max_luminance", "luminance"), &WindowSettings::set_hdr_max_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_max_luminance"), &WindowSettings::get_hdr_max_luminance);
	ClassDB::bind_method(D_METHOD("set_window_states", "states"), &WindowSettings::set_window_states);
	ClassDB::bind_method(D_METHOD("get_window_states"), &WindowSettings::get_window_states);

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
