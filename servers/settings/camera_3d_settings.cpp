/**************************************************************************/
/*  camera_3d_settings.cpp                                                */
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

#include "camera_3d_settings.h"

#ifndef _3D_DISABLED

#include "core/object/class_db.h"

void Camera3DSettings::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	emit_changed();
}

bool Camera3DSettings::is_enabled() const {
	return enabled;
}

void Camera3DSettings::set_field_of_view(float p_degrees) {
	field_of_view = p_degrees;
	emit_changed();
}

float Camera3DSettings::get_field_of_view() const {
	return field_of_view;
}

void Camera3DSettings::set_keep_aspect(Camera3D::KeepAspect p_keep_aspect) {
	keep_aspect = p_keep_aspect;
	emit_changed();
}

Camera3D::KeepAspect Camera3DSettings::get_keep_aspect() const {
	return keep_aspect;
}

void Camera3DSettings::set_near(float p_near) {
	near_distance = p_near;
	emit_changed();
}

float Camera3DSettings::get_near() const {
	return near_distance;
}

void Camera3DSettings::set_far(float p_far) {
	far_distance = p_far;
	emit_changed();
}

float Camera3DSettings::get_far() const {
	return far_distance;
}

void Camera3DSettings::set_shake_intensity(float p_intensity) {
	shake_intensity = p_intensity;
	emit_changed();
}

float Camera3DSettings::get_shake_intensity() const {
	return shake_intensity;
}

void Camera3DSettings::set_head_bob_intensity(float p_intensity) {
	head_bob_intensity = p_intensity;
	emit_changed();
}

float Camera3DSettings::get_head_bob_intensity() const {
	return head_bob_intensity;
}

void Camera3DSettings::set_motion_blur_intensity(float p_intensity) {
	motion_blur_intensity = p_intensity;
	emit_changed();
}

float Camera3DSettings::get_motion_blur_intensity() const {
	return motion_blur_intensity;
}

void Camera3DSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &Camera3DSettings::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Camera3DSettings::is_enabled);
	ClassDB::bind_method(D_METHOD("set_field_of_view", "degrees"), &Camera3DSettings::set_field_of_view);
	ClassDB::bind_method(D_METHOD("get_field_of_view"), &Camera3DSettings::get_field_of_view);
	ClassDB::bind_method(D_METHOD("set_keep_aspect", "keep_aspect"), &Camera3DSettings::set_keep_aspect);
	ClassDB::bind_method(D_METHOD("get_keep_aspect"), &Camera3DSettings::get_keep_aspect);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &Camera3DSettings::set_near);
	ClassDB::bind_method(D_METHOD("get_near"), &Camera3DSettings::get_near);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &Camera3DSettings::set_far);
	ClassDB::bind_method(D_METHOD("get_far"), &Camera3DSettings::get_far);
	ClassDB::bind_method(D_METHOD("set_shake_intensity", "intensity"), &Camera3DSettings::set_shake_intensity);
	ClassDB::bind_method(D_METHOD("get_shake_intensity"), &Camera3DSettings::get_shake_intensity);
	ClassDB::bind_method(D_METHOD("set_head_bob_intensity", "intensity"), &Camera3DSettings::set_head_bob_intensity);
	ClassDB::bind_method(D_METHOD("get_head_bob_intensity"), &Camera3DSettings::get_head_bob_intensity);
	ClassDB::bind_method(D_METHOD("set_motion_blur_intensity", "intensity"), &Camera3DSettings::set_motion_blur_intensity);
	ClassDB::bind_method(D_METHOD("get_motion_blur_intensity"), &Camera3DSettings::get_motion_blur_intensity);

	// `or_less`/`or_greater`: the Inspector shows the usual range, Camera3DSettingsServer clamps with a warning.
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "field_of_view", PROPERTY_HINT_RANGE, "1,179,0.1,or_greater,or_less,degrees"), "set_field_of_view", "get_field_of_view");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "keep_aspect", PROPERTY_HINT_ENUM, "Keep Width,Keep Height"), "set_keep_aspect", "get_keep_aspect");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "near", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,or_less,exp,suffix:m"), "set_near", "get_near");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "far", PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,or_less,exp,suffix:m"), "set_far", "get_far");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shake_intensity", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater,or_less"), "set_shake_intensity", "get_shake_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "head_bob_intensity", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater,or_less"), "set_head_bob_intensity", "get_head_bob_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "motion_blur_intensity", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater,or_less"), "set_motion_blur_intensity", "get_motion_blur_intensity");
}

#endif // _3D_DISABLED
