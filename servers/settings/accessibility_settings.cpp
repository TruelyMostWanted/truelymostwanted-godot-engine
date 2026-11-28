/**************************************************************************/
/*  accessibility_settings.cpp                                            */
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

#include "accessibility_settings.h"

#include "core/object/class_db.h"

void AccessibilitySettings::set_ui_scale(float p_scale) {
	ui_scale = p_scale;
	emit_changed();
}

float AccessibilitySettings::get_ui_scale() const {
	return ui_scale;
}

void AccessibilitySettings::set_font_size_factor(float p_factor) {
	font_size_factor = p_factor;
	emit_changed();
}

float AccessibilitySettings::get_font_size_factor() const {
	return font_size_factor;
}

void AccessibilitySettings::set_color_scheme(int p_scheme) {
	color_scheme = p_scheme;
	emit_changed();
}

int AccessibilitySettings::get_color_scheme() const {
	return color_scheme;
}

void AccessibilitySettings::set_custom_color_scheme_path(const String &p_path) {
	custom_color_scheme_path = p_path;
	emit_changed();
}

String AccessibilitySettings::get_custom_color_scheme_path() const {
	return custom_color_scheme_path;
}

void AccessibilitySettings::set_focus_outline_width(int p_width) {
	focus_outline_width = p_width;
	emit_changed();
}

int AccessibilitySettings::get_focus_outline_width() const {
	return focus_outline_width;
}

void AccessibilitySettings::set_speak_focused_controls(bool p_enabled) {
	speak_focused_controls = p_enabled;
	emit_changed();
}

bool AccessibilitySettings::is_speaking_focused_controls() const {
	return speak_focused_controls;
}

void AccessibilitySettings::set_speech_persona(const String &p_persona) {
	speech_persona = p_persona;
	emit_changed();
}

String AccessibilitySettings::get_speech_persona() const {
	return speech_persona;
}

void AccessibilitySettings::set_reduce_motion(bool p_enabled) {
	reduce_motion = p_enabled;
	emit_changed();
}

bool AccessibilitySettings::is_reducing_motion() const {
	return reduce_motion;
}

void AccessibilitySettings::set_reduce_transparency(bool p_enabled) {
	reduce_transparency = p_enabled;
	emit_changed();
}

bool AccessibilitySettings::is_reducing_transparency() const {
	return reduce_transparency;
}

void AccessibilitySettings::set_colorblind_mode(int p_mode) {
	colorblind_mode = p_mode;
	emit_changed();
}

int AccessibilitySettings::get_colorblind_mode() const {
	return colorblind_mode;
}

void AccessibilitySettings::set_subtitles_enabled(bool p_enabled) {
	subtitles_enabled = p_enabled;
	emit_changed();
}

bool AccessibilitySettings::is_subtitles_enabled() const {
	return subtitles_enabled;
}

void AccessibilitySettings::set_captions_enabled(bool p_enabled) {
	captions_enabled = p_enabled;
	emit_changed();
}

bool AccessibilitySettings::is_captions_enabled() const {
	return captions_enabled;
}

void AccessibilitySettings::set_subtitle_scale(float p_scale) {
	subtitle_scale = p_scale;
	emit_changed();
}

float AccessibilitySettings::get_subtitle_scale() const {
	return subtitle_scale;
}

void AccessibilitySettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ui_scale", "scale"), &AccessibilitySettings::set_ui_scale);
	ClassDB::bind_method(D_METHOD("get_ui_scale"), &AccessibilitySettings::get_ui_scale);
	ClassDB::bind_method(D_METHOD("set_font_size_factor", "factor"), &AccessibilitySettings::set_font_size_factor);
	ClassDB::bind_method(D_METHOD("get_font_size_factor"), &AccessibilitySettings::get_font_size_factor);
	ClassDB::bind_method(D_METHOD("set_color_scheme", "scheme"), &AccessibilitySettings::set_color_scheme);
	ClassDB::bind_method(D_METHOD("get_color_scheme"), &AccessibilitySettings::get_color_scheme);
	ClassDB::bind_method(D_METHOD("set_custom_color_scheme_path", "path"), &AccessibilitySettings::set_custom_color_scheme_path);
	ClassDB::bind_method(D_METHOD("get_custom_color_scheme_path"), &AccessibilitySettings::get_custom_color_scheme_path);
	ClassDB::bind_method(D_METHOD("set_focus_outline_width", "width"), &AccessibilitySettings::set_focus_outline_width);
	ClassDB::bind_method(D_METHOD("get_focus_outline_width"), &AccessibilitySettings::get_focus_outline_width);
	ClassDB::bind_method(D_METHOD("set_speak_focused_controls", "enabled"), &AccessibilitySettings::set_speak_focused_controls);
	ClassDB::bind_method(D_METHOD("is_speaking_focused_controls"), &AccessibilitySettings::is_speaking_focused_controls);
	ClassDB::bind_method(D_METHOD("set_speech_persona", "persona"), &AccessibilitySettings::set_speech_persona);
	ClassDB::bind_method(D_METHOD("get_speech_persona"), &AccessibilitySettings::get_speech_persona);
	ClassDB::bind_method(D_METHOD("set_reduce_motion", "enabled"), &AccessibilitySettings::set_reduce_motion);
	ClassDB::bind_method(D_METHOD("is_reducing_motion"), &AccessibilitySettings::is_reducing_motion);
	ClassDB::bind_method(D_METHOD("set_reduce_transparency", "enabled"), &AccessibilitySettings::set_reduce_transparency);
	ClassDB::bind_method(D_METHOD("is_reducing_transparency"), &AccessibilitySettings::is_reducing_transparency);
	ClassDB::bind_method(D_METHOD("set_colorblind_mode", "mode"), &AccessibilitySettings::set_colorblind_mode);
	ClassDB::bind_method(D_METHOD("get_colorblind_mode"), &AccessibilitySettings::get_colorblind_mode);
	ClassDB::bind_method(D_METHOD("set_subtitles_enabled", "enabled"), &AccessibilitySettings::set_subtitles_enabled);
	ClassDB::bind_method(D_METHOD("is_subtitles_enabled"), &AccessibilitySettings::is_subtitles_enabled);
	ClassDB::bind_method(D_METHOD("set_captions_enabled", "enabled"), &AccessibilitySettings::set_captions_enabled);
	ClassDB::bind_method(D_METHOD("is_captions_enabled"), &AccessibilitySettings::is_captions_enabled);
	ClassDB::bind_method(D_METHOD("set_subtitle_scale", "scale"), &AccessibilitySettings::set_subtitle_scale);
	ClassDB::bind_method(D_METHOD("get_subtitle_scale"), &AccessibilitySettings::get_subtitle_scale);

	ADD_GROUP("Interface", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ui_scale", PROPERTY_HINT_RANGE, "0.5,4,0.05"), "set_ui_scale", "get_ui_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "font_size_factor", PROPERTY_HINT_RANGE, "0.5,3,0.05"), "set_font_size_factor", "get_font_size_factor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_scheme", PROPERTY_HINT_ENUM, "Default,High Contrast Dark,High Contrast Light,Custom"), "set_color_scheme", "get_color_scheme");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "custom_color_scheme_path", PROPERTY_HINT_FILE, "*.tres,*.res"), "set_custom_color_scheme_path", "get_custom_color_scheme_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "focus_outline_width", PROPERTY_HINT_RANGE, "0,8,1,suffix:px"), "set_focus_outline_width", "get_focus_outline_width");
	ADD_GROUP("Speech", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "speak_focused_controls"), "set_speak_focused_controls", "is_speaking_focused_controls");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "speech_persona"), "set_speech_persona", "get_speech_persona");
	ADD_GROUP("Game", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reduce_motion"), "set_reduce_motion", "is_reducing_motion");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reduce_transparency"), "set_reduce_transparency", "is_reducing_transparency");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "colorblind_mode", PROPERTY_HINT_ENUM, "None,Protanopia,Deuteranopia,Tritanopia,Achromatopsia"), "set_colorblind_mode", "get_colorblind_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "subtitles_enabled"), "set_subtitles_enabled", "is_subtitles_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "captions_enabled"), "set_captions_enabled", "is_captions_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "subtitle_scale", PROPERTY_HINT_RANGE, "0.5,3,0.05"), "set_subtitle_scale", "get_subtitle_scale");
}
