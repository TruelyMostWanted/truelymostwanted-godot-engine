/**************************************************************************/
/*  camera_settings_server.h                                              */
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

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "scene/3d/camera_3d.h"
#include "servers/settings/settings_server.h"

// Settings domain `camera` (#28): field of view, horizontal or vertical FOV (Camera3D.keep_aspect), near and far
// clip plane, and the game-side motion values (camera shake, head bob, motion blur) (CameraSettings).
// The camera values go to every Camera3D in the group get_camera_group() when it enters the tree, and to the
// cameras passed to add_camera(). Other cameras (cutscenes, the editor) are never changed. The authored values of
// each camera are remembered the first time it gets the settings and given back by remove_camera() and by
// `enabled` = false. Applied in APPLY_PHASE_SCENE. Not built with `disable_3d`.
// Defaults are the project settings `settings/camera/*`. Precedence: player's file > project settings.
class CameraSettingsServer : public SettingsServer {
	GDCLASS(CameraSettingsServer, SettingsServer);

	static CameraSettingsServer *singleton;

	// The values a camera had before it got the settings.
	struct AuthoredValues {
		real_t fov = 75.0;
		Camera3D::KeepAspect keep_aspect = Camera3D::KEEP_HEIGHT;
		real_t near_distance = 0.05;
		real_t far_distance = 4000.0;
	};

	// The last value pushed per camera setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Every camera that got the settings (group or add_camera()), with its authored values.
	HashMap<ObjectID, AuthoredValues> authored_values;
	bool defaults_captured = false;

	static Variant _get_project_value(const StringName &p_name);

	Variant _get_current(const StringName &p_name) const;
	bool _is_applying() const;
	void _remember_camera(Camera3D *p_camera);
	void _apply_to_camera(Camera3D *p_camera);
	void _restore_camera(Camera3D *p_camera);
	void _deliver_all();
	void _push_camera_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_camera_value(const StringName &p_name);
	void _connect_tree();

	void _on_node_added(Node *p_node);
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	// Emits the typed `<value>_changed` signal of an applied value.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static CameraSettingsServer *get_singleton();
	static void register_settings();

	// The group of cameras that get the settings when they enter the tree.
	static StringName get_camera_group();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;

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

	// Game-side values: stored and signaled, never pushed to the engine.
	void set_shake_intensity(float p_intensity);
	float get_shake_intensity() const;
	void set_head_bob_intensity(float p_intensity);
	float get_head_bob_intensity() const;
	void set_motion_blur_intensity(float p_intensity);
	float get_motion_blur_intensity() const;

	// Cameras.
	void add_camera(Camera3D *p_camera);
	void remove_camera(Camera3D *p_camera);
	bool has_camera(Camera3D *p_camera) const;
	TypedArray<Camera3D> get_managed_cameras() const;
	// Gives the settings to every camera in the group now (for cameras that joined it while in the tree).
	void apply_to_cameras();

	CameraSettingsServer();
	~CameraSettingsServer();
};

#endif // _3D_DISABLED
