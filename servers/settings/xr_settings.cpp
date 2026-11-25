/**************************************************************************/
/*  xr_settings.cpp                                                       */
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

#include "xr_settings.h"

#if defined(MODULE_OPENXR_ENABLED) && !defined(XR_DISABLED)

#include "core/object/class_db.h"

void XRSettings::set_foveation_level(FoveationLevel p_level) {
	foveation_level = p_level;
	emit_changed();
}

XRSettings::FoveationLevel XRSettings::get_foveation_level() const {
	return foveation_level;
}

void XRSettings::set_foveation_dynamic(bool p_enabled) {
	foveation_dynamic = p_enabled;
	emit_changed();
}

bool XRSettings::is_foveation_dynamic() const {
	return foveation_dynamic;
}

void XRSettings::set_foveation_with_subsampled_images(bool p_enabled) {
	foveation_with_subsampled_images = p_enabled;
	emit_changed();
}

bool XRSettings::is_foveation_with_subsampled_images() const {
	return foveation_with_subsampled_images;
}

void XRSettings::set_render_target_size_multiplier(float p_multiplier) {
	render_target_size_multiplier = p_multiplier;
	emit_changed();
}

float XRSettings::get_render_target_size_multiplier() const {
	return render_target_size_multiplier;
}

void XRSettings::set_display_refresh_rate(float p_rate) {
	display_refresh_rate = p_rate;
	emit_changed();
}

float XRSettings::get_display_refresh_rate() const {
	return display_refresh_rate;
}

void XRSettings::set_vrs_min_radius(float p_radius) {
	vrs_min_radius = p_radius;
	emit_changed();
}

float XRSettings::get_vrs_min_radius() const {
	return vrs_min_radius;
}

void XRSettings::set_vrs_strength(float p_strength) {
	vrs_strength = p_strength;
	emit_changed();
}

float XRSettings::get_vrs_strength() const {
	return vrs_strength;
}

void XRSettings::set_cpu_level(PerformanceLevel p_level) {
	cpu_level = p_level;
	emit_changed();
}

XRSettings::PerformanceLevel XRSettings::get_cpu_level() const {
	return cpu_level;
}

void XRSettings::set_gpu_level(PerformanceLevel p_level) {
	gpu_level = p_level;
	emit_changed();
}

XRSettings::PerformanceLevel XRSettings::get_gpu_level() const {
	return gpu_level;
}

void XRSettings::set_left_hand_motion_range(HandMotionRange p_range) {
	left_hand_motion_range = p_range;
	emit_changed();
}

XRSettings::HandMotionRange XRSettings::get_left_hand_motion_range() const {
	return left_hand_motion_range;
}

void XRSettings::set_right_hand_motion_range(HandMotionRange p_range) {
	right_hand_motion_range = p_range;
	emit_changed();
}

XRSettings::HandMotionRange XRSettings::get_right_hand_motion_range() const {
	return right_hand_motion_range;
}

void XRSettings::set_hand_tracking_enabled(bool p_enabled) {
	hand_tracking_enabled = p_enabled;
	emit_changed();
}

bool XRSettings::is_hand_tracking_enabled() const {
	return hand_tracking_enabled;
}

void XRSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_foveation_level", "level"), &XRSettings::set_foveation_level);
	ClassDB::bind_method(D_METHOD("get_foveation_level"), &XRSettings::get_foveation_level);
	ClassDB::bind_method(D_METHOD("set_foveation_dynamic", "enabled"), &XRSettings::set_foveation_dynamic);
	ClassDB::bind_method(D_METHOD("is_foveation_dynamic"), &XRSettings::is_foveation_dynamic);
	ClassDB::bind_method(D_METHOD("set_foveation_with_subsampled_images", "enabled"), &XRSettings::set_foveation_with_subsampled_images);
	ClassDB::bind_method(D_METHOD("is_foveation_with_subsampled_images"), &XRSettings::is_foveation_with_subsampled_images);
	ClassDB::bind_method(D_METHOD("set_render_target_size_multiplier", "multiplier"), &XRSettings::set_render_target_size_multiplier);
	ClassDB::bind_method(D_METHOD("get_render_target_size_multiplier"), &XRSettings::get_render_target_size_multiplier);
	ClassDB::bind_method(D_METHOD("set_display_refresh_rate", "rate"), &XRSettings::set_display_refresh_rate);
	ClassDB::bind_method(D_METHOD("get_display_refresh_rate"), &XRSettings::get_display_refresh_rate);
	ClassDB::bind_method(D_METHOD("set_vrs_min_radius", "radius"), &XRSettings::set_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("get_vrs_min_radius"), &XRSettings::get_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("set_vrs_strength", "strength"), &XRSettings::set_vrs_strength);
	ClassDB::bind_method(D_METHOD("get_vrs_strength"), &XRSettings::get_vrs_strength);
	ClassDB::bind_method(D_METHOD("set_cpu_level", "level"), &XRSettings::set_cpu_level);
	ClassDB::bind_method(D_METHOD("get_cpu_level"), &XRSettings::get_cpu_level);
	ClassDB::bind_method(D_METHOD("set_gpu_level", "level"), &XRSettings::set_gpu_level);
	ClassDB::bind_method(D_METHOD("get_gpu_level"), &XRSettings::get_gpu_level);
	ClassDB::bind_method(D_METHOD("set_left_hand_motion_range", "range"), &XRSettings::set_left_hand_motion_range);
	ClassDB::bind_method(D_METHOD("get_left_hand_motion_range"), &XRSettings::get_left_hand_motion_range);
	ClassDB::bind_method(D_METHOD("set_right_hand_motion_range", "range"), &XRSettings::set_right_hand_motion_range);
	ClassDB::bind_method(D_METHOD("get_right_hand_motion_range"), &XRSettings::get_right_hand_motion_range);
	ClassDB::bind_method(D_METHOD("set_hand_tracking_enabled", "enabled"), &XRSettings::set_hand_tracking_enabled);
	ClassDB::bind_method(D_METHOD("is_hand_tracking_enabled"), &XRSettings::is_hand_tracking_enabled);

	ADD_GROUP("Foveation", "foveation_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "foveation_level", PROPERTY_HINT_ENUM, "Off,Low,Medium,High"), "set_foveation_level", "get_foveation_level");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foveation_dynamic"), "set_foveation_dynamic", "is_foveation_dynamic");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foveation_with_subsampled_images"), "set_foveation_with_subsampled_images", "is_foveation_with_subsampled_images");
	ADD_GROUP("Display", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_target_size_multiplier", PROPERTY_HINT_RANGE, "0.1,3,0.05"), "set_render_target_size_multiplier", "get_render_target_size_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "display_refresh_rate", PROPERTY_HINT_RANGE, "0,500,0.01,suffix:Hz"), "set_display_refresh_rate", "get_display_refresh_rate");
	ADD_GROUP("Vulkan VRS", "vrs_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_min_radius", PROPERTY_HINT_RANGE, "1,100,1"), "set_vrs_min_radius", "get_vrs_min_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_strength", PROPERTY_HINT_RANGE, "0.1,10,0.1"), "set_vrs_strength", "get_vrs_strength");
	ADD_GROUP("Performance", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cpu_level", PROPERTY_HINT_ENUM, "Runtime Default,Power Savings,Sustained Low,Sustained High,Boost"), "set_cpu_level", "get_cpu_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gpu_level", PROPERTY_HINT_ENUM, "Runtime Default,Power Savings,Sustained Low,Sustained High,Boost"), "set_gpu_level", "get_gpu_level");
	ADD_GROUP("Hands", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "left_hand_motion_range", PROPERTY_HINT_ENUM, "Unobstructed,Conform To Controller"), "set_left_hand_motion_range", "get_left_hand_motion_range");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "right_hand_motion_range", PROPERTY_HINT_ENUM, "Unobstructed,Conform To Controller"), "set_right_hand_motion_range", "get_right_hand_motion_range");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hand_tracking_enabled"), "set_hand_tracking_enabled", "is_hand_tracking_enabled");

	BIND_ENUM_CONSTANT(FOVEATION_LEVEL_OFF);
	BIND_ENUM_CONSTANT(FOVEATION_LEVEL_LOW);
	BIND_ENUM_CONSTANT(FOVEATION_LEVEL_MEDIUM);
	BIND_ENUM_CONSTANT(FOVEATION_LEVEL_HIGH);

	BIND_ENUM_CONSTANT(PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	BIND_ENUM_CONSTANT(PERFORMANCE_LEVEL_POWER_SAVINGS);
	BIND_ENUM_CONSTANT(PERFORMANCE_LEVEL_SUSTAINED_LOW);
	BIND_ENUM_CONSTANT(PERFORMANCE_LEVEL_SUSTAINED_HIGH);
	BIND_ENUM_CONSTANT(PERFORMANCE_LEVEL_BOOST);

	BIND_ENUM_CONSTANT(HAND_MOTION_RANGE_UNOBSTRUCTED);
	BIND_ENUM_CONSTANT(HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
}

#endif // MODULE_OPENXR_ENABLED && !XR_DISABLED
