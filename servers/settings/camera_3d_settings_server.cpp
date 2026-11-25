/**************************************************************************/
/*  camera_3d_settings_server.cpp                                         */
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

#include "camera_3d_settings_server.h"

#ifndef _3D_DISABLED

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "servers/settings/camera_3d_settings.h"
#include "servers/settings/settings_server_manager.h"

static constexpr const char *SETTING_CAMERA_FIELD_OF_VIEW = "settings/camera_3d/field_of_view";
static constexpr const char *SETTING_CAMERA_KEEP_ASPECT = "settings/camera_3d/keep_aspect";
static constexpr const char *SETTING_CAMERA_NEAR = "settings/camera_3d/near";
static constexpr const char *SETTING_CAMERA_FAR = "settings/camera_3d/far";

static constexpr const char *CAMERA_SETTINGS_GROUP = "camera_3d_settings";

// The values pushed to the cameras, in the order of Camera3DSettings.
static const char *camera_setting_names[] = {
	"enabled",
	"field_of_view",
	"keep_aspect",
	"near",
	"far",
};

// Game-side values: stored, validated and signaled, never pushed to the engine.
static const char *camera_game_setting_names[] = {
	"shake_intensity",
	"head_bob_intensity",
	"motion_blur_intensity",
};

// The smallest distance between the near and the far plane that reaches a camera.
static constexpr double CAMERA_MIN_CLIP_DISTANCE = 0.01;

static Variant camera_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_name);
}

static bool camera_range(const StringName &p_name, double &r_min, double &r_max) {
	if (p_name == SNAME("field_of_view")) {
		// The range of Camera3D.fov.
		r_min = 1.0;
		r_max = 179.0;
	} else if (p_name == SNAME("near")) {
		r_min = 0.001;
		r_max = 10.0;
	} else if (p_name == SNAME("far")) {
		r_min = 0.01;
		r_max = 1000000.0;
	} else if (p_name == SNAME("shake_intensity") || p_name == SNAME("head_bob_intensity") || p_name == SNAME("motion_blur_intensity")) {
		r_min = 0.0;
		r_max = 1.0;
	} else {
		return false;
	}
	return true;
}

Camera3DSettingsServer *Camera3DSettingsServer::singleton = nullptr;

Camera3DSettingsServer *Camera3DSettingsServer::get_singleton() {
	return singleton;
}

void Camera3DSettingsServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, SETTING_CAMERA_FIELD_OF_VIEW, PROPERTY_HINT_RANGE, "1,179,0.1,degrees"), 75.0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, SETTING_CAMERA_KEEP_ASPECT, PROPERTY_HINT_ENUM, "Keep Width,Keep Height"), int(Camera3D::KEEP_HEIGHT));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, SETTING_CAMERA_NEAR, PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater,exp,suffix:m"), 0.05);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, SETTING_CAMERA_FAR, PROPERTY_HINT_RANGE, "0.01,4000,0.01,or_greater,exp,suffix:m"), 4000.0);
}

StringName Camera3DSettingsServer::get_camera_group() {
	return StringName(CAMERA_SETTINGS_GROUP);
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

Variant Camera3DSettingsServer::_get_project_value(const StringName &p_name) {
	if (p_name == SNAME("enabled")) {
		return true;
	} else if (p_name == SNAME("field_of_view")) {
		return double(camera_project_setting(SETTING_CAMERA_FIELD_OF_VIEW, 75.0));
	} else if (p_name == SNAME("keep_aspect")) {
		return int(camera_project_setting(SETTING_CAMERA_KEEP_ASPECT, int(Camera3D::KEEP_HEIGHT)));
	} else if (p_name == SNAME("near")) {
		return double(camera_project_setting(SETTING_CAMERA_NEAR, 0.05));
	} else if (p_name == SNAME("far")) {
		return double(camera_project_setting(SETTING_CAMERA_FAR, 4000.0));
	}
	return Variant();
}

Variant Camera3DSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

bool Camera3DSettingsServer::_is_applying() const {
	return bool(_get_current(SNAME("enabled")));
}

void Camera3DSettingsServer::_remember_camera(Camera3D *p_camera) {
	const ObjectID id = p_camera->get_instance_id();
	if (authored_values.has(id)) {
		return;
	}
	AuthoredValues values;
	values.fov = p_camera->get_fov();
	values.keep_aspect = p_camera->get_keep_aspect_mode();
	values.near_distance = p_camera->get_near();
	values.far_distance = p_camera->get_far();
	authored_values.insert(id, values);
}

void Camera3DSettingsServer::_apply_to_camera(Camera3D *p_camera) {
	_remember_camera(p_camera);
	if (!_is_applying()) {
		_restore_camera(p_camera);
		return;
	}
	const double near_distance = _get_current(SNAME("near"));
	const double far_distance = MAX(double(_get_current(SNAME("far"))), near_distance + CAMERA_MIN_CLIP_DISTANCE);
	p_camera->set_fov(real_t(double(_get_current(SNAME("field_of_view")))));
	p_camera->set_keep_aspect_mode(Camera3D::KeepAspect(int(_get_current(SNAME("keep_aspect")))));
	p_camera->set_near(real_t(near_distance));
	p_camera->set_far(real_t(far_distance));
}

void Camera3DSettingsServer::_restore_camera(Camera3D *p_camera) {
	const AuthoredValues *values = authored_values.getptr(p_camera->get_instance_id());
	if (values == nullptr) {
		return;
	}
	p_camera->set_fov(values->fov);
	p_camera->set_keep_aspect_mode(values->keep_aspect);
	p_camera->set_near(values->near_distance);
	p_camera->set_far(values->far_distance);
}

void Camera3DSettingsServer::_deliver_all() {
	// Cameras that got the settings before; freed ones are forgotten.
	LocalVector<ObjectID> freed;
	for (const KeyValue<ObjectID, AuthoredValues> &kv : authored_values) {
		Camera3D *camera = ObjectDB::get_instance<Camera3D>(kv.key);
		if (camera == nullptr) {
			freed.push_back(kv.key);
			continue;
		}
		_apply_to_camera(camera);
	}
	for (const ObjectID &id : freed) {
		authored_values.erase(id);
	}

	// Cameras in the group that have not got the settings yet.
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || (Engine::get_singleton() != nullptr && Engine::get_singleton()->is_editor_hint())) {
		return;
	}
	for (Node *node : tree->get_nodes_in_group(get_camera_group())) {
		Camera3D *camera = Object::cast_to<Camera3D>(node);
		if (camera != nullptr && !authored_values.has(camera->get_instance_id())) {
			_apply_to_camera(camera);
		}
	}
}

void Camera3DSettingsServer::_push_camera_value(const Variant &p_value, const StringName &p_name) {
	pushed_values[p_name] = p_value;
	_deliver_all();
	_connect_tree();
}

Variant Camera3DSettingsServer::_pull_camera_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the cameras (which have their own authored values).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	// Every managed camera has what was pushed last (unless the game changed it since).
	return _get_current(p_name);
}

void Camera3DSettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || (Engine::get_singleton() != nullptr && Engine::get_singleton()->is_editor_hint())) {
		// Never change the cameras of a scene open in the editor (it would save the values into the scene).
		return;
	}
	const Callable callback = callable_mp(this, &Camera3DSettingsServer::_on_node_added);
	if (!tree->is_connected(SNAME("node_added"), callback)) {
		tree->connect(SNAME("node_added"), callback);
	}
}

void Camera3DSettingsServer::_on_node_added(Node *p_node) {
	Camera3D *camera = Object::cast_to<Camera3D>(p_node);
	if (camera == nullptr || !camera->is_in_group(get_camera_group())) {
		return;
	}
	if (!authored_values.has(camera->get_instance_id())) {
		// A new camera (for example after a scene change): forget the freed ones first.
		LocalVector<ObjectID> freed;
		for (const KeyValue<ObjectID, AuthoredValues> &kv : authored_values) {
			if (ObjectDB::get_instance(kv.key) == nullptr) {
				freed.push_back(kv.key);
			}
		}
		for (const ObjectID &id : freed) {
			authored_values.erase(id);
		}
	}
	_apply_to_camera(camera);
}

void Camera3DSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// The SceneTree exists; the main scene is loaded after this phase and its cameras get the values.
		_connect_tree();
		_deliver_all();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName Camera3DSettingsServer::get_domain() const {
	return SNAME("camera_3d");
}

SettingsServer::ApplyPhase Camera3DSettingsServer::get_apply_phase() const {
	// The SceneTree exists; the main scene is not loaded yet.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> Camera3DSettingsServer::_create_default_settings() const {
	Ref<Camera3DSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void Camera3DSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant Camera3DSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}
	double min_value = 0.0;
	double max_value = 0.0;
	if (!camera_range(p_name, min_value, max_value)) {
		return value;
	}
	const double number = value;
	if (Math::is_nan(number)) {
		WARN_PRINT(vformat("Camera settings: \"%s\" is not a number; the default is used.", p_name));
		return get_default_value(p_name);
	}
	const double clamped = CLAMP(number, min_value, max_value);
	if (clamped != number) {
		WARN_PRINT(vformat("Camera settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(min_value), Variant(max_value), Variant(clamped)));
	}
	return clamped;
}

/////////////////////////////////////////////////////////////////////////////
// Cameras.

void Camera3DSettingsServer::add_camera(Camera3D *p_camera) {
	ERR_FAIL_NULL(p_camera);
	_apply_to_camera(p_camera);
}

void Camera3DSettingsServer::remove_camera(Camera3D *p_camera) {
	ERR_FAIL_NULL(p_camera);
	_restore_camera(p_camera);
	authored_values.erase(p_camera->get_instance_id());
}

bool Camera3DSettingsServer::has_camera(Camera3D *p_camera) const {
	return p_camera != nullptr && authored_values.has(p_camera->get_instance_id());
}

TypedArray<Camera3D> Camera3DSettingsServer::get_managed_cameras() const {
	TypedArray<Camera3D> cameras;
	for (const KeyValue<ObjectID, AuthoredValues> &kv : authored_values) {
		Camera3D *camera = ObjectDB::get_instance<Camera3D>(kv.key);
		if (camera != nullptr) {
			cameras.push_back(camera);
		}
	}
	return cameras;
}

void Camera3DSettingsServer::apply_to_cameras() {
	_deliver_all();
	_connect_tree();
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void Camera3DSettingsServer::set_enabled(bool p_enabled) {
	set_value(SNAME("enabled"), p_enabled);
}

bool Camera3DSettingsServer::is_enabled() const {
	return get_value(SNAME("enabled"));
}

void Camera3DSettingsServer::set_field_of_view(float p_degrees) {
	set_value(SNAME("field_of_view"), p_degrees);
}

float Camera3DSettingsServer::get_field_of_view() const {
	return get_value(SNAME("field_of_view"));
}

void Camera3DSettingsServer::set_keep_aspect(Camera3D::KeepAspect p_keep_aspect) {
	set_value(SNAME("keep_aspect"), int(p_keep_aspect));
}

Camera3D::KeepAspect Camera3DSettingsServer::get_keep_aspect() const {
	return Camera3D::KeepAspect(int(get_value(SNAME("keep_aspect"))));
}

void Camera3DSettingsServer::set_near(float p_near) {
	set_value(SNAME("near"), p_near);
}

float Camera3DSettingsServer::get_near() const {
	return get_value(SNAME("near"));
}

void Camera3DSettingsServer::set_far(float p_far) {
	set_value(SNAME("far"), p_far);
}

float Camera3DSettingsServer::get_far() const {
	return get_value(SNAME("far"));
}

void Camera3DSettingsServer::set_shake_intensity(float p_intensity) {
	set_value(SNAME("shake_intensity"), p_intensity);
}

float Camera3DSettingsServer::get_shake_intensity() const {
	return get_value(SNAME("shake_intensity"));
}

void Camera3DSettingsServer::set_head_bob_intensity(float p_intensity) {
	set_value(SNAME("head_bob_intensity"), p_intensity);
}

float Camera3DSettingsServer::get_head_bob_intensity() const {
	return get_value(SNAME("head_bob_intensity"));
}

void Camera3DSettingsServer::set_motion_blur_intensity(float p_intensity) {
	set_value(SNAME("motion_blur_intensity"), p_intensity);
}

float Camera3DSettingsServer::get_motion_blur_intensity() const {
	return get_value(SNAME("motion_blur_intensity"));
}

void Camera3DSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"field_of_view",
		"keep_aspect",
		"near",
		"far",
		"shake_intensity",
		"head_bob_intensity",
		"motion_blur_intensity",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void Camera3DSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("field_of_view_changed", PropertyInfo(Variant::FLOAT, "fov")));
	ADD_SIGNAL(MethodInfo("keep_aspect_changed", PropertyInfo(Variant::INT, "keep_aspect", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Camera3D.KeepAspect")));
	ADD_SIGNAL(MethodInfo("near_changed", PropertyInfo(Variant::FLOAT, "near")));
	ADD_SIGNAL(MethodInfo("far_changed", PropertyInfo(Variant::FLOAT, "far")));
	ADD_SIGNAL(MethodInfo("shake_intensity_changed", PropertyInfo(Variant::FLOAT, "intensity")));
	ADD_SIGNAL(MethodInfo("head_bob_intensity_changed", PropertyInfo(Variant::FLOAT, "intensity")));
	ADD_SIGNAL(MethodInfo("motion_blur_intensity_changed", PropertyInfo(Variant::FLOAT, "intensity")));

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &Camera3DSettingsServer::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Camera3DSettingsServer::is_enabled);
	ClassDB::bind_method(D_METHOD("set_field_of_view", "degrees"), &Camera3DSettingsServer::set_field_of_view);
	ClassDB::bind_method(D_METHOD("get_field_of_view"), &Camera3DSettingsServer::get_field_of_view);
	ClassDB::bind_method(D_METHOD("set_keep_aspect", "keep_aspect"), &Camera3DSettingsServer::set_keep_aspect);
	ClassDB::bind_method(D_METHOD("get_keep_aspect"), &Camera3DSettingsServer::get_keep_aspect);
	ClassDB::bind_method(D_METHOD("set_near", "near"), &Camera3DSettingsServer::set_near);
	ClassDB::bind_method(D_METHOD("get_near"), &Camera3DSettingsServer::get_near);
	ClassDB::bind_method(D_METHOD("set_far", "far"), &Camera3DSettingsServer::set_far);
	ClassDB::bind_method(D_METHOD("get_far"), &Camera3DSettingsServer::get_far);
	ClassDB::bind_method(D_METHOD("set_shake_intensity", "intensity"), &Camera3DSettingsServer::set_shake_intensity);
	ClassDB::bind_method(D_METHOD("get_shake_intensity"), &Camera3DSettingsServer::get_shake_intensity);
	ClassDB::bind_method(D_METHOD("set_head_bob_intensity", "intensity"), &Camera3DSettingsServer::set_head_bob_intensity);
	ClassDB::bind_method(D_METHOD("get_head_bob_intensity"), &Camera3DSettingsServer::get_head_bob_intensity);
	ClassDB::bind_method(D_METHOD("set_motion_blur_intensity", "intensity"), &Camera3DSettingsServer::set_motion_blur_intensity);
	ClassDB::bind_method(D_METHOD("get_motion_blur_intensity"), &Camera3DSettingsServer::get_motion_blur_intensity);

	ClassDB::bind_method(D_METHOD("add_camera", "camera"), &Camera3DSettingsServer::add_camera);
	ClassDB::bind_method(D_METHOD("remove_camera", "camera"), &Camera3DSettingsServer::remove_camera);
	ClassDB::bind_method(D_METHOD("has_camera", "camera"), &Camera3DSettingsServer::has_camera);
	ClassDB::bind_method(D_METHOD("get_managed_cameras"), &Camera3DSettingsServer::get_managed_cameras);
	ClassDB::bind_method(D_METHOD("apply_to_cameras"), &Camera3DSettingsServer::apply_to_cameras);

	ClassDB::bind_static_method("Camera3DSettingsServer", D_METHOD("get_camera_group"), &Camera3DSettingsServer::get_camera_group);
}

Camera3DSettingsServer::Camera3DSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	for (const char *setting_name : camera_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &Camera3DSettingsServer::_push_camera_value).bind(bound_name), callable_mp(this, &Camera3DSettingsServer::_pull_camera_value).bind(bound_name));
	}
	for (const char *setting_name : camera_game_setting_names) {
		set_setting_flags(StringName(setting_name), SETTING_FLAG_NO_ENGINE_TARGET);
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &Camera3DSettingsServer::_on_startup_applied));
	}
}

Camera3DSettingsServer::~Camera3DSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif // _3D_DISABLED
