/**************************************************************************/
/*  test_camera_settings_server.cpp                                       */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_camera_settings_server)

#ifndef _3D_DISABLED

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/camera_settings.h"
#include "servers/settings/camera_settings_server.h"
#include "servers/settings/settings_server_manager.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestCameraSettingsServer {

// Autosave off for the test.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("camera_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

// A camera with authored values that differ from the defaults of the domain.
static Camera3D *create_authored_camera() {
	Camera3D *camera = memnew(Camera3D);
	camera->set_fov(60.0);
	camera->set_keep_aspect_mode(Camera3D::KEEP_WIDTH);
	camera->set_near(0.2);
	camera->set_far(500.0);
	return camera;
}

static void check_authored(const Camera3D *p_camera) {
	CHECK(p_camera->get_fov() == doctest::Approx(60.0));
	CHECK(p_camera->get_keep_aspect_mode() == Camera3D::KEEP_WIDTH);
	CHECK(p_camera->get_near() == doctest::Approx(0.2));
	CHECK(p_camera->get_far() == doctest::Approx(500.0));
}

TEST_CASE("[CameraSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	CameraSettingsServer *server = CameraSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("CameraSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("camera") == server);
	CHECK(server->get_domain() == StringName("camera"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/camera.ini");
	CHECK(Object::cast_to<CameraSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(CameraSettingsServer::get_camera_group() == StringName("camera_settings"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 8);
	for (const char *name : { "enabled", "field_of_view", "keep_aspect", "near", "far" }) {
		CHECK_MESSAGE(names.has(name), name);
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	}
	for (const char *name : { "shake_intensity", "head_bob_intensity", "motion_blur_intensity" }) {
		CHECK_MESSAGE(names.has(name), name);
		CHECK(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
	}
}

TEST_CASE("[CameraSettingsServer] Defaults are the project settings") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_fov = project_settings->get_setting("settings/camera/field_of_view", 75.0);
	const Variant old_keep_aspect = project_settings->get_setting("settings/camera/keep_aspect", 1);
	const Variant old_far = project_settings->get_setting("settings/camera/far", 4000.0);
	project_settings->set_setting("settings/camera/field_of_view", 90.0);
	project_settings->set_setting("settings/camera/keep_aspect", int(Camera3D::KEEP_WIDTH));
	project_settings->set_setting("settings/camera/far", 1000.0);

	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	CHECK(server->is_enabled());
	CHECK(server->get_field_of_view() == doctest::Approx(90.0));
	CHECK(server->get_keep_aspect() == Camera3D::KEEP_WIDTH);
	CHECK(server->get_near() == doctest::Approx(0.05));
	CHECK(server->get_far() == doctest::Approx(1000.0));
	CHECK(server->get_shake_intensity() == doctest::Approx(1.0));
	CHECK(server->get_head_bob_intensity() == doctest::Approx(1.0));
	CHECK(server->get_motion_blur_intensity() == doctest::Approx(1.0));
	CHECK(double(server->get_default_value("field_of_view")) == doctest::Approx(90.0));
	memdelete(server);

	project_settings->set_setting("settings/camera/field_of_view", old_fov);
	project_settings->set_setting("settings/camera/keep_aspect", old_keep_aspect);
	project_settings->set_setting("settings/camera/far", old_far);
}

TEST_CASE("[CameraSettingsServer] Values out of range are clamped with a warning") {
	TestSetup setup;
	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	ERR_PRINT_OFF;
	server->set_field_of_view(200.0);
	CHECK(server->get_field_of_view() == doctest::Approx(179.0));
	server->set_field_of_view(0.0);
	CHECK(server->get_field_of_view() == doctest::Approx(1.0));
	server->set_near(0.0);
	CHECK(server->get_near() == doctest::Approx(0.001));
	server->set_far(-5.0);
	CHECK(server->get_far() == doctest::Approx(0.01));
	server->set_shake_intensity(2.0);
	CHECK(server->get_shake_intensity() == doctest::Approx(1.0));
	server->set_head_bob_intensity(-1.0);
	CHECK(server->get_head_bob_intensity() == doctest::Approx(0.0));
	server->set_value("keep_aspect", 5);
	CHECK(int(server->get_keep_aspect()) == int(server->get_default_value("keep_aspect")));
	ERR_PRINT_ON;

	server->set_field_of_view(100.0);
	CHECK(server->get_field_of_view() == doctest::Approx(100.0));
	server->set_motion_blur_intensity(0.25);
	CHECK(server->get_motion_blur_intensity() == doctest::Approx(0.25));
	memdelete(server);
}

TEST_CASE("[CameraSettingsServer] Values are saved and loaded") {
	TestSetup setup;
	const String path = temp_file("camera.ini");
	{
		CameraSettingsServer *server = memnew(CameraSettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_field_of_view(95.0);
		server->set_keep_aspect(Camera3D::KEEP_WIDTH);
		server->set_near(0.1);
		server->set_far(800.0);
		server->set_shake_intensity(0.5);
		server->set_head_bob_intensity(0.0);
		server->set_motion_blur_intensity(0.3);
		CHECK(server->apply() == OK);
		CHECK(server->save() == OK);
		memdelete(server);
	}

	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	CHECK(server->load() == OK);
	CHECK(server->get_field_of_view() == doctest::Approx(95.0));
	CHECK(server->get_keep_aspect() == Camera3D::KEEP_WIDTH);
	CHECK(server->get_near() == doctest::Approx(0.1));
	CHECK(server->get_far() == doctest::Approx(800.0));
	CHECK(server->get_shake_intensity() == doctest::Approx(0.5));
	CHECK(server->get_head_bob_intensity() == doctest::Approx(0.0));
	CHECK(server->get_motion_blur_intensity() == doctest::Approx(0.3));
	memdelete(server);
}

TEST_CASE("[SceneTree][CameraSettingsServer] Added cameras get the values and their authored values back") {
	TestSetup setup;
	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_file_path(temp_file("added.ini"));
	server->set_field_of_view(100.0);
	server->set_near(0.5);
	server->set_far(0.1); // Below near: the camera gets near + 0.01.

	Camera3D *camera = create_authored_camera();
	server->add_camera(camera);
	CHECK(server->has_camera(camera));
	CHECK(server->get_managed_cameras().size() == 1);
	CHECK(camera->get_fov() == doctest::Approx(100.0));
	CHECK(camera->get_keep_aspect_mode() == Camera3D::KEEP_HEIGHT);
	CHECK(camera->get_near() == doctest::Approx(0.5));
	CHECK(camera->get_far() == doctest::Approx(0.51));

	// Later changes reach it at once.
	server->set_far(300.0);
	server->set_keep_aspect(Camera3D::KEEP_WIDTH);
	CHECK(camera->get_far() == doctest::Approx(300.0));
	CHECK(camera->get_keep_aspect_mode() == Camera3D::KEEP_WIDTH);

	// Game-side values never touch the camera.
	server->set_shake_intensity(0.0);
	CHECK(camera->get_fov() == doctest::Approx(100.0));

	// Off: the authored values; on again: the settings.
	server->set_enabled(false);
	check_authored(camera);
	server->set_enabled(true);
	CHECK(camera->get_fov() == doctest::Approx(100.0));

	// Removed: the authored values, no more changes.
	server->remove_camera(camera);
	CHECK_FALSE(server->has_camera(camera));
	check_authored(camera);
	server->set_field_of_view(110.0);
	CHECK(camera->get_fov() == doctest::Approx(60.0));

	memdelete(camera);
	memdelete(server);
}

TEST_CASE("[SceneTree][CameraSettingsServer] Cameras in the group get the values, others keep theirs") {
	TestSetup setup;
	Window *root = SceneTree::get_singleton()->get_root().ptr();

	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_file_path(temp_file("group.ini"));
	server->set_field_of_view(90.0);
	server->apply_to_cameras(); // Connects to the SceneTree.

	// In the group: gets the values when it enters the tree (a camera of a new scene).
	Camera3D *player = create_authored_camera();
	player->add_to_group(CameraSettingsServer::get_camera_group());
	root->add_child(player);
	CHECK(server->has_camera(player));
	CHECK(player->get_fov() == doctest::Approx(90.0));
	server->set_field_of_view(80.0);
	CHECK(player->get_fov() == doctest::Approx(80.0));

	// Not in the group (a cutscene camera): untouched, also when it is current.
	Camera3D *cutscene = create_authored_camera();
	root->add_child(cutscene);
	cutscene->make_current();
	server->set_field_of_view(70.0);
	CHECK_FALSE(server->has_camera(cutscene));
	check_authored(cutscene);

	// A scene change: the old camera is freed, the new one gets the values.
	root->remove_child(player);
	memdelete(player);
	Camera3D *next_player = create_authored_camera();
	next_player->add_to_group(CameraSettingsServer::get_camera_group());
	root->add_child(next_player);
	CHECK(next_player->get_fov() == doctest::Approx(70.0));
	CHECK(server->get_managed_cameras().size() == 1);

	// Joined the group while in the tree: apply_to_cameras() picks it up.
	cutscene->add_to_group(CameraSettingsServer::get_camera_group());
	CHECK_FALSE(server->has_camera(cutscene));
	server->apply_to_cameras();
	CHECK(cutscene->get_fov() == doctest::Approx(70.0));

	root->remove_child(cutscene);
	root->remove_child(next_player);
	memdelete(cutscene);
	memdelete(next_player);
	memdelete(server);
}

TEST_CASE("[CameraSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	CameraSettingsServer *server = memnew(CameraSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "field_of_view_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("field_of_view", 90.0);
	SIGNAL_CHECK_FALSE("field_of_view_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 90.0 } };
	SIGNAL_CHECK("field_of_view_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("field_of_view_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("field_of_view");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("field_of_view_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("field_of_view_changed", default_args);

	SIGNAL_UNWATCH(server, "field_of_view_changed");
	memdelete(server);
}

} // namespace TestCameraSettingsServer

#endif // _3D_DISABLED
