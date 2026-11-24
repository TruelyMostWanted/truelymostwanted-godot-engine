/**************************************************************************/
/*  test_video_effect_settings_server.cpp                                 */
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

TEST_FORCE_LINK(test_video_effect_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/environment.h"
#include "servers/rendering/rendering_method.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_effect_settings.h"
#include "servers/settings/video_effect_settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#ifndef _3D_DISABLED
#include "scene/3d/world_environment.h"
#endif // _3D_DISABLED

namespace TestVideoEffectSettingsServer {

// Autosave off; the project settings a test changes are restored afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
	}
};

// Turns the global environment overrides off again at the end of a [SceneTree] test.
struct OverrideReset {
	~OverrideReset() {
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server == nullptr) {
			return;
		}
		for (int i = 0; i < RenderingServerEnums::ENV_EFFECT_MAX; i++) {
			rendering_server->environment_set_global_effect_disabled(RenderingServerEnums::EnvironmentEffect(i), false);
		}
		rendering_server->environment_set_global_adjustment(1.0, 1.0, 1.0);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("video_effect_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[VideoEffectSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	VideoEffectSettingsServer *server = VideoEffectSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoEffectSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_effect") == server);
	CHECK(server->get_domain() == StringName("video_effect"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SERVERS);
	CHECK(server->get_file_path() == "user://settings/video_effect.ini");
	CHECK(Object::cast_to<VideoEffectSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(VideoEffectSettingsServer::get_ignore_group() == StringName("ignore_quality_settings"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 34);
	for (const char *name : { "ssao_quality", "ssao_half_size", "ssao_adaptive_target", "ssao_blur_passes", "ssao_fadeout_from", "ssao_fadeout_to",
				 "ssil_quality", "ssil_half_size", "ssil_adaptive_target", "ssil_blur_passes", "ssil_fadeout_from", "ssil_fadeout_to",
				 "sdfgi_ray_count", "sdfgi_frames_to_converge", "sdfgi_frames_to_update_light", "voxel_gi_quality",
				 "volumetric_fog_volume_size", "volumetric_fog_volume_depth", "volumetric_fog_use_filter",
				 "sss_quality", "sss_scale", "sss_depth_scale", "dof_bokeh_shape", "dof_blur_quality", "dof_use_jitter",
				 "allow_glow", "allow_ssao", "allow_ssil", "allow_ssr", "allow_fog", "allow_volumetric_fog",
				 "brightness", "contrast", "saturation" }) {
		CHECK_MESSAGE(names.has(name), name);
		// Everything takes effect at once: no confirmation, no restart.
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	}
}

TEST_CASE("[VideoEffectSettingsServer] Defaults are the project settings") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_quality = project_settings->get_setting("rendering/environment/ssao/quality", 2);
	const Variant old_size = project_settings->get_setting("rendering/environment/volumetric_fog/volume_size", 64);
	const Variant old_filter = project_settings->get_setting("rendering/environment/volumetric_fog/use_filter", 1);
	project_settings->set_setting("rendering/environment/ssao/quality", int(RenderingServerEnums::ENV_SSAO_QUALITY_ULTRA));
	project_settings->set_setting("rendering/environment/volumetric_fog/volume_size", 128);
	project_settings->set_setting("rendering/environment/volumetric_fog/use_filter", 0);

	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	CHECK(server->get_ssao_quality() == RenderingServerEnums::ENV_SSAO_QUALITY_ULTRA);
	CHECK(server->get_volumetric_fog_volume_size() == 128);
	CHECK_FALSE(server->is_volumetric_fog_using_filter());
	CHECK(int(server->get_default_value("ssao_quality")) == int(RenderingServerEnums::ENV_SSAO_QUALITY_ULTRA));
	// The overrides have no project setting: everything allowed, no adjustment.
	CHECK(server->is_glow_allowed());
	CHECK(server->is_ssr_allowed());
	CHECK(server->is_volumetric_fog_allowed());
	CHECK(server->get_brightness() == doctest::Approx(1.0));
	CHECK(server->get_contrast() == doctest::Approx(1.0));
	CHECK(server->get_saturation() == doctest::Approx(1.0));
	memdelete(server);

	project_settings->set_setting("rendering/environment/ssao/quality", old_quality);
	project_settings->set_setting("rendering/environment/volumetric_fog/volume_size", old_size);
	project_settings->set_setting("rendering/environment/volumetric_fog/use_filter", old_filter);
}

TEST_CASE("[VideoEffectSettingsServer] Values out of range are clamped with a warning") {
	TestSetup setup;
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	ERR_PRINT_OFF;
	server->set_ssao_adaptive_target(2.0);
	CHECK(server->get_ssao_adaptive_target() == doctest::Approx(1.0));
	server->set_ssil_blur_passes(9);
	CHECK(server->get_ssil_blur_passes() == 6);
	server->set_volumetric_fog_volume_depth(8);
	CHECK(server->get_volumetric_fog_volume_depth() == 16);
	server->set_sss_scale(0.0);
	CHECK(server->get_sss_scale() == doctest::Approx(0.001));
	server->set_brightness(0.0);
	CHECK(server->get_brightness() == doctest::Approx(0.01));
	server->set_saturation(20.0);
	CHECK(server->get_saturation() == doctest::Approx(8.0));

	// Enum values out of range: the default.
	server->set_value("ssao_quality", 7);
	CHECK(int(server->get_ssao_quality()) == int(server->get_default_value("ssao_quality")));
	server->set_value("sdfgi_ray_count", int(RenderingServerEnums::ENV_SDFGI_RAY_COUNT_MAX));
	CHECK(int(server->get_sdfgi_ray_count()) == int(server->get_default_value("sdfgi_ray_count")));
	server->set_value("dof_bokeh_shape", -1);
	CHECK(int(server->get_dof_bokeh_shape()) == int(server->get_default_value("dof_bokeh_shape")));
	ERR_PRINT_ON;

	// Valid values pass unchanged.
	server->set_sdfgi_ray_count(RenderingServerEnums::ENV_SDFGI_RAY_COUNT_128);
	CHECK(server->get_sdfgi_ray_count() == RenderingServerEnums::ENV_SDFGI_RAY_COUNT_128);
	server->set_voxel_gi_quality(RenderingServerEnums::VOXEL_GI_QUALITY_HIGH);
	CHECK(server->get_voxel_gi_quality() == RenderingServerEnums::VOXEL_GI_QUALITY_HIGH);
	server->set_contrast(1.25);
	CHECK(server->get_contrast() == doctest::Approx(1.25));
	memdelete(server);
}

TEST_CASE("[VideoEffectSettingsServer] Values are saved and loaded") {
	TestSetup setup;
	const String path = temp_file("video_effect.ini");
	{
		VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_ssao_quality(RenderingServerEnums::ENV_SSAO_QUALITY_HIGH);
		server->set_ssao_half_size(false);
		server->set_ssil_fadeout_to(500.0);
		server->set_sdfgi_frames_to_converge(RenderingServerEnums::ENV_SDFGI_CONVERGE_IN_10_FRAMES);
		server->set_volumetric_fog_volume_size(96);
		server->set_sss_quality(RenderingServerEnums::SUB_SURFACE_SCATTERING_QUALITY_HIGH);
		server->set_dof_blur_quality(RenderingServerEnums::DOF_BLUR_QUALITY_MEDIUM);
		server->set_dof_use_jitter(true);
		server->set_allow_ssr(false);
		server->set_allow_volumetric_fog(false);
		server->set_brightness(1.2);
		// save() writes the applied values.
		ERR_PRINT_OFF;
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(server->save() == OK);
		memdelete(server);
	}

	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	ERR_PRINT_OFF;
	CHECK(server->load() == OK);
	ERR_PRINT_ON;
	// Stored as they are, also where the current renderer ignores them: the file works with every renderer.
	CHECK(server->get_ssao_quality() == RenderingServerEnums::ENV_SSAO_QUALITY_HIGH);
	CHECK_FALSE(server->is_ssao_half_size_enabled());
	CHECK(server->get_ssil_fadeout_to() == doctest::Approx(500.0));
	CHECK(server->get_sdfgi_frames_to_converge() == RenderingServerEnums::ENV_SDFGI_CONVERGE_IN_10_FRAMES);
	CHECK(server->get_volumetric_fog_volume_size() == 96);
	CHECK(server->get_sss_quality() == RenderingServerEnums::SUB_SURFACE_SCATTERING_QUALITY_HIGH);
	CHECK(server->get_dof_blur_quality() == RenderingServerEnums::DOF_BLUR_QUALITY_MEDIUM);
	CHECK(server->is_dof_using_jitter());
	CHECK_FALSE(server->is_ssr_allowed());
	CHECK_FALSE(server->is_volumetric_fog_allowed());
	CHECK(server->is_glow_allowed());
	CHECK(server->get_brightness() == doctest::Approx(1.2));
	memdelete(server);
}

TEST_CASE("[VideoEffectSettingsServer] Supported values follow the renderer") {
	TestSetup setup;
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("supported.ini"));

	const PackedStringArray unsupported = server->get_unsupported_setting_names();
	for (const String &name : server->get_setting_names()) {
		CHECK_MESSAGE(server->is_value_supported(name) == VideoEffectSettingsServer::is_renderer_supported(name), name);
		CHECK_MESSAGE(unsupported.has(name) == !server->is_value_supported(name), name);
	}
	CHECK_FALSE(server->is_value_supported("missing"));

	// Every renderer has glow, fog and the adjustments.
	for (const char *name : { "allow_glow", "allow_fog", "brightness", "contrast", "saturation" }) {
		CHECK_MESSAGE(server->is_value_supported(name), name);
	}
	const String method = OS::get_singleton()->get_current_rendering_method();
	if (method == "gl_compatibility") {
		for (const char *name : { "ssao_quality", "ssil_quality", "sdfgi_ray_count", "voxel_gi_quality", "volumetric_fog_volume_size", "sss_quality", "dof_blur_quality", "allow_ssao", "allow_ssr", "allow_volumetric_fog" }) {
			CHECK_FALSE_MESSAGE(server->is_value_supported(name), name);
		}
	} else if (method == "mobile") {
		for (const char *name : { "ssao_quality", "ssil_quality", "sdfgi_ray_count", "volumetric_fog_volume_size", "sss_quality", "allow_ssao", "allow_ssr" }) {
			CHECK_FALSE_MESSAGE(server->is_value_supported(name), name);
		}
		CHECK(server->is_value_supported("voxel_gi_quality"));
		CHECK(server->is_value_supported("dof_blur_quality"));
	}
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoEffectSettingsServer] Environment overrides apply at render time and keep the authored values") {
	TestSetup setup;
	OverrideReset reset;
	REQUIRE(RSG::scene != nullptr);
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("overrides.ini"));

	Ref<Environment> authored;
	authored.instantiate();
	authored->set_glow_enabled(true);
	authored->set_fog_enabled(true);
	CHECK(RSG::scene->environment_get_glow_enabled(authored->get_rid()));

	// "Glow off" reaches the environment at once; the resource keeps the author's value.
	server->set_allow_glow(false);
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(authored->get_rid()));
	CHECK(authored->is_glow_enabled());
	CHECK(RSG::scene->environment_get_fog_enabled(authored->get_rid()));

	// An environment created later (a scene loaded after the change) is covered too.
	Ref<Environment> later;
	later.instantiate();
	later->set_glow_enabled(true);
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(later->get_rid()));

	// Turning it on again restores the authored values; an environment without glow stays without.
	Ref<Environment> without_glow;
	without_glow.instantiate();
	server->set_allow_glow(true);
	CHECK(RSG::scene->environment_get_glow_enabled(authored->get_rid()));
	CHECK(RSG::scene->environment_get_glow_enabled(later->get_rid()));
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(without_glow->get_rid()));

	server->set_allow_fog(false);
	CHECK_FALSE(RSG::scene->environment_get_fog_enabled(authored->get_rid()));
	server->set_allow_fog(true);
	CHECK(RSG::scene->environment_get_fog_enabled(authored->get_rid()));

	// Excluded environments keep their values.
	server->set_environment_ignored(later, true);
	CHECK(server->is_environment_ignored(later));
	CHECK_FALSE(server->is_environment_ignored(authored));
	server->set_allow_glow(false);
	CHECK(RSG::scene->environment_get_glow_enabled(later->get_rid()));
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(authored->get_rid()));
	server->set_environment_ignored(later, false);
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(later->get_rid()));
	server->set_allow_glow(true);
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoEffectSettingsServer] Brightness, contrast and saturation multiply the authored adjustment") {
	TestSetup setup;
	OverrideReset reset;
	REQUIRE(RSG::scene != nullptr);
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("adjustment.ini"));

	Ref<Environment> plain; // The author did not enable adjustments.
	plain.instantiate();
	Ref<Environment> adjusted;
	adjusted.instantiate();
	adjusted->set_adjustment_enabled(true);
	adjusted->set_adjustment_brightness(2.0);
	CHECK_FALSE(RSG::scene->environment_get_adjustments_enabled(plain->get_rid()));
	// The renderer keeps brightness after the sRGB transfer; the player's factor goes through the same transfer.
	const auto to_linear = [](float p_value) { return p_value < 0.04045f ? p_value * (1.0f / 12.92f) : Math::pow(float((p_value + 0.055f) * (1.0f / 1.055f)), 2.4f); };
	const float authored = RSG::scene->environment_get_adjustments_brightness(adjusted->get_rid());
	CHECK(authored == doctest::Approx(to_linear(2.0f)));

	server->set_brightness(1.5);
	server->set_saturation(0.5);
	// The pass is turned on for the plain environment, with 1.0 as its own values.
	CHECK(RSG::scene->environment_get_adjustments_enabled(plain->get_rid()));
	CHECK(RSG::scene->environment_get_adjustments_brightness(plain->get_rid()) == doctest::Approx(to_linear(1.5f)));
	CHECK(RSG::scene->environment_get_adjustments_saturation(plain->get_rid()) == doctest::Approx(0.5));
	CHECK(RSG::scene->environment_get_adjustments_contrast(plain->get_rid()) == doctest::Approx(1.0));
	CHECK(RSG::scene->environment_get_adjustments_brightness(adjusted->get_rid()) == doctest::Approx(authored * to_linear(1.5f)));
	CHECK(adjusted->get_adjustment_brightness() == doctest::Approx(2.0));

	// Back to 1.0: no extra pass, the authored values again.
	server->set_brightness(1.0);
	server->set_saturation(1.0);
	CHECK_FALSE(RSG::scene->environment_get_adjustments_enabled(plain->get_rid()));
	CHECK(RSG::scene->environment_get_adjustments_enabled(adjusted->get_rid()));
	CHECK(RSG::scene->environment_get_adjustments_brightness(adjusted->get_rid()) == doctest::Approx(authored));
	memdelete(server);
}

#ifndef _3D_DISABLED
TEST_CASE("[SceneTree][VideoEffectSettingsServer] WorldEnvironment nodes in the ignore group are excluded") {
	TestSetup setup;
	OverrideReset reset;
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_file_path(temp_file("ignore_group.ini"));
	server->set_allow_ssr(false); // Pushes a value: the server watches the scene tree from now on.

	Ref<Environment> menu_environment;
	menu_environment.instantiate();
	menu_environment->set_glow_enabled(true);
	WorldEnvironment *world_environment = memnew(WorldEnvironment);
	world_environment->set_environment(menu_environment);
	world_environment->add_to_group(VideoEffectSettingsServer::get_ignore_group());
	SceneTree::get_singleton()->get_root()->add_child(world_environment);
	CHECK(server->is_environment_ignored(menu_environment));

	server->set_allow_glow(false);
	CHECK(RSG::scene->environment_get_glow_enabled(menu_environment->get_rid()));
	memdelete(world_environment); // One WorldEnvironment per world.

	// Not in the group: affected.
	Ref<Environment> level_environment;
	level_environment.instantiate();
	level_environment->set_glow_enabled(true);
	WorldEnvironment *level_world_environment = memnew(WorldEnvironment);
	level_world_environment->set_environment(level_environment);
	SceneTree::get_singleton()->get_root()->add_child(level_world_environment);
	CHECK_FALSE(server->is_environment_ignored(level_environment));
	CHECK_FALSE(RSG::scene->environment_get_glow_enabled(level_environment->get_rid()));

	server->set_allow_glow(true);
	memdelete(level_world_environment);
	memdelete(server);
}
#endif // _3D_DISABLED

TEST_CASE("[VideoEffectSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	VideoEffectSettingsServer *server = memnew(VideoEffectSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "brightness_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("brightness", 1.5);
	SIGNAL_CHECK_FALSE("brightness_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 1.5 } };
	SIGNAL_CHECK("brightness_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("brightness_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("brightness");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("brightness_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("brightness_changed", default_args);

	SIGNAL_UNWATCH(server, "brightness_changed");
	memdelete(server);
}

} // namespace TestVideoEffectSettingsServer
