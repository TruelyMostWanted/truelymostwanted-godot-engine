/**************************************************************************/
/*  camera_3d_settings.h                                                  */
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

#ifndef _3D_DISABLED

#include "core/io/resource.h"
#include "scene/3d/camera_3d.h"

// Data of the `camera` settings domain (#28): field of view (and whether it is vertical or horizontal), near and far
// clip plane, and the game-side motion values (camera shake, head bob, motion blur).
// Camera3DSettingsServer pushes the camera values to the Camera3D nodes in its group and to the cameras added to it.
class Camera3DSettings : public Resource {
	GDCLASS(Camera3DSettings, Resource);

	bool enabled = true;
	float field_of_view = 75.0;
	Camera3D::KeepAspect keep_aspect = Camera3D::KEEP_HEIGHT;
	float near_distance = 0.05;
	float far_distance = 4000.0;
	float shake_intensity = 1.0;
	float head_bob_intensity = 1.0;
	float motion_blur_intensity = 1.0;

protected:
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;
	void set_field_of_view(float p_degrees);
	float get_field_of_view() const;
	void set_keep_aspect(Camera3D::KeepAspect p_keep_aspect);
	Camera3D::KeepAspect get_keep_aspect() const;
	void set_near(float p_near);
	float get_near() const;
	void set_far(float p_far);
	float get_far() const;
	void set_shake_intensity(float p_intensity);
	float get_shake_intensity() const;
	void set_head_bob_intensity(float p_intensity);
	float get_head_bob_intensity() const;
	void set_motion_blur_intensity(float p_intensity);
	float get_motion_blur_intensity() const;
};

#endif // _3D_DISABLED
