/**************************************************************************/
/*  test_video_quality_settings_server.cpp                                */
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

TEST_FORCE_LINK(test_video_quality_settings_server)

// The tested values are 3D viewport settings, which do not exist without 3D.
#ifndef _3D_DISABLED

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_quality_settings.h"
#include "servers/settings/video_quality_settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#ifdef RD_ENABLED
#include "servers/rendering/rendering_device.h"
#endif

namespace TestVideoQualitySettingsServer {

// Autosave off; the project settings a test changes are restored afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

// The values of the root viewport, restored at the end of a [SceneTree] test.
struct RootState {
	Viewport *root = nullptr;
	Viewport::Scaling3DMode scaling_3d_mode = Viewport::SCALING_3D_MODE_BILINEAR;
	float scaling_3d_scale = 1.0;
	float fsr_sharpness = 0.2;
	float mesh_lod_threshold = 1.0;
	Viewport::MSAA msaa_2d = Viewport::MSAA_DISABLED;
	Viewport::MSAA msaa_3d = Viewport::MSAA_DISABLED;
	Viewport::ScreenSpaceAA screen_space_aa = Viewport::SCREEN_SPACE_AA_DISABLED;
	bool use_taa = false;
	bool use_debanding = false;
	bool use_occlusion_culling = false;

	RootState() {
		root = SceneTree::get_singleton()->get_root().ptr();
		scaling_3d_mode = root->get_scaling_3d_mode();
		scaling_3d_scale = root->get_scaling_3d_scale();
		fsr_sharpness = root->get_fsr_sharpness();
		mesh_lod_threshold = root->get_mesh_lod_threshold();
		msaa_2d = root->get_msaa_2d();
		msaa_3d = root->get_msaa_3d();
		screen_space_aa = root->get_screen_space_aa();
		use_taa = root->is_using_taa();
		use_debanding = root->is_using_debanding();
		use_occlusion_culling = root->is_using_occlusion_culling();
	}
	~RootState() {
		root->set_scaling_3d_mode(scaling_3d_mode);
		root->set_scaling_3d_scale(scaling_3d_scale);
		root->set_fsr_sharpness(fsr_sharpness);
		root->set_mesh_lod_threshold(mesh_lod_threshold);
		root->set_msaa_2d(msaa_2d);
		root->set_msaa_3d(msaa_3d);
		root->set_screen_space_aa(screen_space_aa);
		root->set_use_taa(use_taa);
		root->set_use_debanding(use_debanding);
		root->set_use_occlusion_culling(use_occlusion_culling);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("video_quality_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[VideoQualitySettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	VideoQualitySettingsServer *server = VideoQualitySettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoQualitySettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_quality") == server);
	CHECK(server->get_domain() == StringName("video_quality"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/video_quality.ini");
	CHECK(Object::cast_to<VideoQualitySettings>(server->get_settings().ptr()) != nullptr);
	CHECK(VideoQualitySettingsServer::get_viewport_group() == StringName("video_quality_settings"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 13);
	for (const char *name : { "scaling_3d_mode", "scaling_3d_scale", "fsr_sharpness", "mesh_lod_threshold", "msaa_2d", "msaa_3d", "screen_space_aa", "use_taa", "use_debanding", "vrs_mode", "use_occlusion_culling", "occlusion_rays_per_thread", "occlusion_culling_build_quality" }) {
		CHECK_MESSAGE(names.has(name), name);
		// Everything takes effect at once: no confirmation, no restart.
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	}
	CHECK_FALSE(names.has("window_mode")); // #20.
}

TEST_CASE("[VideoQualitySettingsServer] Defaults are the project settings") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_msaa_3d = project_settings->get_setting("rendering/anti_aliasing/quality/msaa_3d", 0);
	const Variant old_scale = project_settings->get_setting("rendering/scaling_3d/scale", 1.0);
	const Variant old_rays = project_settings->get_setting("rendering/occlusion_culling/occlusion_rays_per_thread", 512);
	project_settings->set_setting("rendering/anti_aliasing/quality/msaa_3d", int(Viewport::MSAA_4X));
	project_settings->set_setting("rendering/scaling_3d/scale", 0.75);
	project_settings->set_setting("rendering/occlusion_culling/occlusion_rays_per_thread", 256);

	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	CHECK(server->get_msaa_3d() == Viewport::MSAA_4X);
	CHECK(server->get_scaling_3d_scale() == doctest::Approx(0.75));
	CHECK(server->get_occlusion_rays_per_thread() == 256);
	CHECK(int(server->get_default_value("msaa_3d")) == int(Viewport::MSAA_4X));
	memdelete(server);

	project_settings->set_setting("rendering/anti_aliasing/quality/msaa_3d", old_msaa_3d);
	project_settings->set_setting("rendering/scaling_3d/scale", old_scale);
	project_settings->set_setting("rendering/occlusion_culling/occlusion_rays_per_thread", old_rays);
}

TEST_CASE("[VideoQualitySettingsServer] Values out of range are clamped with a warning") {
	TestSetup setup;
	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	ERR_PRINT_OFF;
	server->set_scaling_3d_scale(5.0);
	CHECK(server->get_scaling_3d_scale() == doctest::Approx(2.0));
	server->set_scaling_3d_scale(0.0);
	CHECK(server->get_scaling_3d_scale() == doctest::Approx(0.1));
	server->set_fsr_sharpness(-1.0);
	CHECK(server->get_fsr_sharpness() == doctest::Approx(0.0));
	server->set_mesh_lod_threshold(5000.0);
	CHECK(server->get_mesh_lod_threshold() == doctest::Approx(1024.0));
	server->set_occlusion_rays_per_thread(0);
	CHECK(server->get_occlusion_rays_per_thread() == 1);

	// Enum values out of range: the default.
	const Viewport::MSAA default_msaa = Viewport::MSAA(int(server->get_default_value("msaa_2d")));
	server->set_value("msaa_2d", 9);
	CHECK(server->get_msaa_2d() == default_msaa);
	server->set_value("scaling_3d_mode", -1);
	CHECK(int(server->get_scaling_3d_mode()) == int(server->get_default_value("scaling_3d_mode")));
	server->set_value("occlusion_culling_build_quality", 3);
	CHECK(int(server->get_occlusion_culling_build_quality()) == int(server->get_default_value("occlusion_culling_build_quality")));
	ERR_PRINT_ON;

	// Valid values pass unchanged.
	server->set_msaa_2d(Viewport::MSAA_8X);
	CHECK(server->get_msaa_2d() == Viewport::MSAA_8X);
	server->set_screen_space_aa(Viewport::SCREEN_SPACE_AA_SMAA);
	CHECK(server->get_screen_space_aa() == Viewport::SCREEN_SPACE_AA_SMAA);
	server->set_occlusion_culling_build_quality(RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_LOW);
	CHECK(server->get_occlusion_culling_build_quality() == RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_LOW);
	memdelete(server);
}

TEST_CASE("[VideoQualitySettingsServer] Values are saved and loaded") {
	TestSetup setup;
	const String path = temp_file("video_quality.ini");
	{
		VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_scaling_3d_mode(Viewport::SCALING_3D_MODE_FSR2);
		server->set_scaling_3d_scale(0.67);
		server->set_fsr_sharpness(0.5);
		server->set_mesh_lod_threshold(4.0);
		server->set_msaa_3d(Viewport::MSAA_2X);
		server->set_use_taa(true);
		server->set_use_debanding(true);
		server->set_vrs_mode(Viewport::VRS_TEXTURE);
		server->set_use_occlusion_culling(true);
		server->set_occlusion_culling_build_quality(RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_MEDIUM);
		ERR_PRINT_OFF; // Values the renderer of the test run ignores are skipped with a warning.
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(server->save() == OK);
		memdelete(server);
	}

	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	ERR_PRINT_OFF;
	CHECK(server->load() == OK);
	ERR_PRINT_ON;
	// Stored as they are, also where the current renderer ignores them: the file works with every renderer.
	CHECK(server->get_scaling_3d_mode() == Viewport::SCALING_3D_MODE_FSR2);
	CHECK(server->get_scaling_3d_scale() == doctest::Approx(0.67));
	CHECK(server->get_fsr_sharpness() == doctest::Approx(0.5));
	CHECK(server->get_mesh_lod_threshold() == doctest::Approx(4.0));
	CHECK(server->get_msaa_3d() == Viewport::MSAA_2X);
	CHECK(server->is_using_taa());
	CHECK(server->is_using_debanding());
	CHECK(server->get_vrs_mode() == Viewport::VRS_TEXTURE);
	CHECK(server->is_using_occlusion_culling());
	CHECK(server->get_occlusion_culling_build_quality() == RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_MEDIUM);
	memdelete(server);
}

TEST_CASE("[VideoQualitySettingsServer] Supported values follow the renderer") {
	TestSetup setup;
	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(temp_file("supported.ini"));

	const PackedStringArray unsupported = server->get_unsupported_setting_names();
	for (const String &name : server->get_setting_names()) {
		CHECK_MESSAGE(server->is_value_supported(name) == VideoQualitySettingsServer::is_renderer_supported(name), name);
		CHECK_MESSAGE(unsupported.has(name) == !server->is_value_supported(name), name);
	}
	CHECK_FALSE(server->is_value_supported("missing"));

	// Every renderer can scale, set the LOD threshold and use MSAA.
	for (const char *name : { "scaling_3d_mode", "scaling_3d_scale", "mesh_lod_threshold", "msaa_2d", "msaa_3d" }) {
		CHECK_MESSAGE(server->is_value_supported(name), name);
	}
	CHECK(VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::SCALING_3D_MODE_BILINEAR));
	CHECK(VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::SCALING_3D_MODE_NEAREST));
	CHECK_FALSE(VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::SCALING_3D_MODE_MAX));

	const String method = OS::get_singleton()->get_current_rendering_method();
	if (method == "gl_compatibility") {
		for (const char *name : { "fsr_sharpness", "screen_space_aa", "use_taa", "use_debanding", "vrs_mode" }) {
			CHECK_FALSE_MESSAGE(server->is_value_supported(name), name);
		}
		CHECK_FALSE(VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::SCALING_3D_MODE_FSR));
	} else if (method == "mobile") {
		CHECK_FALSE(server->is_value_supported("use_taa"));
		CHECK_FALSE(VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::SCALING_3D_MODE_FSR2));
	}
	// Variable rate shading needs a RenderingDevice with VRS attachments.
#ifdef RD_ENABLED
	const bool has_rendering_device = RenderingDevice::get_singleton() != nullptr;
#else
	const bool has_rendering_device = false;
#endif
	if (!has_rendering_device) {
		CHECK_FALSE(server->is_value_supported("vrs_mode"));
	}
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoQualitySettingsServer] Each value reaches the root viewport at once") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(temp_file("root.ini"));
	server->set_scaling_3d_mode(Viewport::SCALING_3D_MODE_NEAREST);
	CHECK(root->get_scaling_3d_mode() == Viewport::SCALING_3D_MODE_NEAREST);
	server->set_scaling_3d_scale(0.5);
	CHECK(root->get_scaling_3d_scale() == doctest::Approx(0.5));
	server->set_mesh_lod_threshold(8.0);
	CHECK(root->get_mesh_lod_threshold() == doctest::Approx(8.0));
	server->set_msaa_2d(Viewport::MSAA_4X);
	CHECK(root->get_msaa_2d() == Viewport::MSAA_4X);
	server->set_msaa_3d(Viewport::MSAA_2X);
	CHECK(root->get_msaa_3d() == Viewport::MSAA_2X);

	// Values the renderer supports reach the root; the others are stored and skipped.
	ERR_PRINT_OFF;
	server->set_fsr_sharpness(1.0);
	server->set_screen_space_aa(Viewport::SCREEN_SPACE_AA_FXAA);
	server->set_use_taa(true);
	server->set_use_debanding(true);
	server->set_use_occlusion_culling(true);
	ERR_PRINT_ON;
	CHECK(server->get_screen_space_aa() == Viewport::SCREEN_SPACE_AA_FXAA);
	CHECK(server->is_using_taa());
	if (server->is_value_supported("fsr_sharpness")) {
		CHECK(root->get_fsr_sharpness() == doctest::Approx(1.0));
	}
	CHECK((root->get_screen_space_aa() == Viewport::SCREEN_SPACE_AA_FXAA) == server->is_value_supported("screen_space_aa"));
	CHECK(root->is_using_taa() == server->is_value_supported("use_taa"));
	CHECK(root->is_using_debanding() == server->is_value_supported("use_debanding"));
	CHECK(root->is_using_occlusion_culling() == server->is_value_supported("use_occlusion_culling"));

	// The global occlusion values have no getter: the domain keeps them.
	server->set_occlusion_rays_per_thread(128);
	CHECK(server->get_occlusion_rays_per_thread() == 128);

	// Reading the engine back picks up a change made on the root.
	root->set_msaa_2d(Viewport::MSAA_8X);
	server->read_from_engine();
	CHECK(server->get_msaa_2d() == Viewport::MSAA_8X);
	CHECK(server->get_mesh_lod_threshold() == doctest::Approx(8.0));
	CHECK(server->is_using_taa()); // Skipped values stay as stored.

	server->set_occlusion_rays_per_thread(512);
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoQualitySettingsServer] Added viewports and viewports in the group get the values") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_file_path(temp_file("viewports.ini"));
	server->set_msaa_3d(Viewport::MSAA_4X);

	// Added by hand: gets the current values at once and every later change.
	SubViewport *added = memnew(SubViewport);
	server->add_viewport(added);
	CHECK(server->has_viewport(added));
	CHECK(added->get_msaa_3d() == Viewport::MSAA_4X);
	server->set_mesh_lod_threshold(16.0);
	CHECK(added->get_mesh_lod_threshold() == doctest::Approx(16.0));
	server->remove_viewport(added);
	CHECK_FALSE(server->has_viewport(added));
	server->set_mesh_lod_threshold(2.0);
	CHECK(added->get_mesh_lod_threshold() == doctest::Approx(16.0));

	// In the group: gets the values when it enters the tree, and later changes while it is in it.
	SubViewport *grouped = memnew(SubViewport);
	grouped->add_to_group(VideoQualitySettingsServer::get_viewport_group());
	root->add_child(grouped);
	CHECK(grouped->get_msaa_3d() == Viewport::MSAA_4X);
	CHECK(grouped->get_mesh_lod_threshold() == doctest::Approx(2.0));
	server->set_msaa_3d(Viewport::MSAA_8X);
	CHECK(grouped->get_msaa_3d() == Viewport::MSAA_8X);

	// Not in the group: untouched.
	SubViewport *other = memnew(SubViewport);
	root->add_child(other);
	CHECK(other->get_msaa_3d() == Viewport::MSAA_DISABLED);

	root->remove_child(other);
	root->remove_child(grouped);
	memdelete(other);
	memdelete(grouped);
	memdelete(added);
	memdelete(server);
}

TEST_CASE("[VideoQualitySettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	VideoQualitySettingsServer *server = memnew(VideoQualitySettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "scaling_3d_scale_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("scaling_3d_scale", 0.5);
	SIGNAL_CHECK_FALSE("scaling_3d_scale_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 0.5 } };
	SIGNAL_CHECK("scaling_3d_scale_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("scaling_3d_scale_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("scaling_3d_scale");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("scaling_3d_scale_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("scaling_3d_scale_changed", default_args);

	SIGNAL_UNWATCH(server, "scaling_3d_scale_changed");
	memdelete(server);
}

} // namespace TestVideoQualitySettingsServer

#endif // _3D_DISABLED
