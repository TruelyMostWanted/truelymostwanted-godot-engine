/**************************************************************************/
/*  test_video_texture_settings_server.cpp                                */
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

TEST_FORCE_LINK(test_video_texture_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_texture_settings.h"
#include "servers/settings/video_texture_settings_server.h"
#include "servers/settings/video_texture_streaming_settings.h"
#include "servers/settings/video_texture_streaming_settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

#ifdef MODULE_TEXTURE_STREAMING_ENABLED
#include "modules/texture_streaming/texture_streaming.h"
#endif

namespace TestVideoTextureSettingsServer {

// Autosave off; the project settings a test changes are restored afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
	}
};

// The values of the root viewport, restored at the end of a [SceneTree] test.
struct RootState {
	Viewport *root = nullptr;
	Viewport::AnisotropicFiltering anisotropic_filtering_level = Viewport::ANISOTROPY_4X;
	float texture_mipmap_bias = 0.0;
	Viewport::DefaultCanvasItemTextureFilter texture_filter = Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
	Viewport::DefaultCanvasItemTextureRepeat texture_repeat = Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
	bool snap_transforms = false;
	bool snap_vertices = false;
	Viewport::SDFOversize sdf_oversize = Viewport::SDF_OVERSIZE_120_PERCENT;
	Viewport::SDFScale sdf_scale = Viewport::SDF_SCALE_50_PERCENT;

	RootState() {
		root = SceneTree::get_singleton()->get_root().ptr();
		anisotropic_filtering_level = root->get_anisotropic_filtering_level();
		texture_mipmap_bias = root->get_texture_mipmap_bias();
		texture_filter = root->get_default_canvas_item_texture_filter();
		texture_repeat = root->get_default_canvas_item_texture_repeat();
		snap_transforms = root->is_snap_2d_transforms_to_pixel_enabled();
		snap_vertices = root->is_snap_2d_vertices_to_pixel_enabled();
		sdf_oversize = root->get_sdf_oversize();
		sdf_scale = root->get_sdf_scale();
	}
	~RootState() {
		root->set_anisotropic_filtering_level(anisotropic_filtering_level);
		root->set_texture_mipmap_bias(texture_mipmap_bias);
		root->set_default_canvas_item_texture_filter(texture_filter);
		root->set_default_canvas_item_texture_repeat(texture_repeat);
		root->set_snap_2d_transforms_to_pixel(snap_transforms);
		root->set_snap_2d_vertices_to_pixel(snap_vertices);
		root->set_sdf_oversize(sdf_oversize);
		root->set_sdf_scale(sdf_scale);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("video_texture_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[VideoTextureSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	VideoTextureSettingsServer *server = VideoTextureSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoTextureSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_texture") == server);
	CHECK(server->get_domain() == StringName("video_texture"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/video_texture.ini");
	CHECK(Object::cast_to<VideoTextureSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(VideoTextureSettingsServer::get_viewport_group() == StringName("video_texture_settings"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 11);
	for (const char *name : { "anisotropic_filtering_level", "texture_mipmap_bias", "light_projector_filter", "decal_filter", "canvas_item_default_texture_filter", "canvas_item_default_texture_repeat", "snap_2d_transforms_to_pixel", "snap_2d_vertices_to_pixel", "sdf_oversize", "sdf_scale", "canvas_shadow_atlas_size" }) {
		CHECK_MESSAGE(names.has(name), name);
		// Everything takes effect at once: no confirmation, no restart.
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	}
	CHECK_FALSE(names.has("msaa_3d")); // #21.
}

TEST_CASE("[VideoTextureSettingsServer] Defaults are the project settings") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_anisotropy = project_settings->get_setting("rendering/textures/default_filters/anisotropic_filtering_level", 2);
	const Variant old_filter = project_settings->get_setting("rendering/textures/canvas_textures/default_texture_filter", 1);
	const Variant old_atlas = project_settings->get_setting("rendering/2d/shadow_atlas/size", 2048);
	project_settings->set_setting("rendering/textures/default_filters/anisotropic_filtering_level", int(Viewport::ANISOTROPY_16X));
	project_settings->set_setting("rendering/textures/canvas_textures/default_texture_filter", int(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST));
	project_settings->set_setting("rendering/2d/shadow_atlas/size", 1024);

	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	CHECK(server->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_16X);
	CHECK(server->get_canvas_item_default_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	CHECK(server->get_canvas_shadow_atlas_size() == 1024);
	CHECK(int(server->get_default_value("anisotropic_filtering_level")) == int(Viewport::ANISOTROPY_16X));
	memdelete(server);

	project_settings->set_setting("rendering/textures/default_filters/anisotropic_filtering_level", old_anisotropy);
	project_settings->set_setting("rendering/textures/canvas_textures/default_texture_filter", old_filter);
	project_settings->set_setting("rendering/2d/shadow_atlas/size", old_atlas);
}

TEST_CASE("[VideoTextureSettingsServer] Values out of range are clamped with a warning") {
	TestSetup setup;
	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	ERR_PRINT_OFF;
	server->set_texture_mipmap_bias(5.0);
	CHECK(server->get_texture_mipmap_bias() == doctest::Approx(2.0));
	server->set_texture_mipmap_bias(-3.0);
	CHECK(server->get_texture_mipmap_bias() == doctest::Approx(-2.0));
	server->set_canvas_shadow_atlas_size(16);
	CHECK(server->get_canvas_shadow_atlas_size() == 128);
	server->set_canvas_shadow_atlas_size(100000);
	CHECK(server->get_canvas_shadow_atlas_size() == 16384);

	// Enum values out of range: the default.
	server->set_value("anisotropic_filtering_level", 9);
	CHECK(int(server->get_anisotropic_filtering_level()) == int(server->get_default_value("anisotropic_filtering_level")));
	server->set_value("decal_filter", 6);
	CHECK(int(server->get_decal_filter()) == int(server->get_default_value("decal_filter")));
	server->set_value("sdf_scale", -1);
	CHECK(int(server->get_sdf_scale()) == int(server->get_default_value("sdf_scale")));
	ERR_PRINT_ON;

	// Valid values pass unchanged.
	server->set_anisotropic_filtering_level(Viewport::ANISOTROPY_8X);
	CHECK(server->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_8X);
	server->set_light_projector_filter(RenderingServerEnums::LIGHT_PROJECTOR_FILTER_NEAREST);
	CHECK(server->get_light_projector_filter() == RenderingServerEnums::LIGHT_PROJECTOR_FILTER_NEAREST);
	server->set_canvas_item_default_texture_repeat(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	CHECK(server->get_canvas_item_default_texture_repeat() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	memdelete(server);
}

TEST_CASE("[VideoTextureSettingsServer] Values are saved and loaded") {
	TestSetup setup;
	const String path = temp_file("video_texture.ini");
	{
		VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_anisotropic_filtering_level(Viewport::ANISOTROPY_16X);
		server->set_texture_mipmap_bias(-0.5);
		server->set_light_projector_filter(RenderingServerEnums::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC);
		server->set_decal_filter(RenderingServerEnums::DECAL_FILTER_NEAREST);
		server->set_canvas_item_default_texture_filter(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
		server->set_canvas_item_default_texture_repeat(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
		server->set_snap_2d_transforms_to_pixel(true);
		server->set_snap_2d_vertices_to_pixel(true);
		server->set_sdf_oversize(Viewport::SDF_OVERSIZE_200_PERCENT);
		server->set_sdf_scale(Viewport::SDF_SCALE_100_PERCENT);
		server->set_canvas_shadow_atlas_size(4096);
		ERR_PRINT_OFF; // Values the renderer of the test run ignores are skipped with a warning.
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(server->save() == OK);
		memdelete(server);
	}

	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	ERR_PRINT_OFF;
	CHECK(server->load() == OK);
	ERR_PRINT_ON;
	// Stored as they are, also where the current renderer ignores them: the file works with every renderer.
	CHECK(server->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_16X);
	CHECK(server->get_texture_mipmap_bias() == doctest::Approx(-0.5));
	CHECK(server->get_light_projector_filter() == RenderingServerEnums::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC);
	CHECK(server->get_decal_filter() == RenderingServerEnums::DECAL_FILTER_NEAREST);
	CHECK(server->get_canvas_item_default_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	CHECK(server->get_canvas_item_default_texture_repeat() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
	CHECK(server->is_snap_2d_transforms_to_pixel_enabled());
	CHECK(server->is_snap_2d_vertices_to_pixel_enabled());
	CHECK(server->get_sdf_oversize() == Viewport::SDF_OVERSIZE_200_PERCENT);
	CHECK(server->get_sdf_scale() == Viewport::SDF_SCALE_100_PERCENT);
	CHECK(server->get_canvas_shadow_atlas_size() == 4096);
	memdelete(server);
}

TEST_CASE("[VideoTextureSettingsServer] Supported values follow the renderer") {
	TestSetup setup;
	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(temp_file("supported.ini"));

	const PackedStringArray unsupported = server->get_unsupported_setting_names();
	for (const String &name : server->get_setting_names()) {
		CHECK_MESSAGE(server->is_value_supported(name) == VideoTextureSettingsServer::is_renderer_supported(name), name);
		CHECK_MESSAGE(unsupported.has(name) == !server->is_value_supported(name), name);
	}
	CHECK_FALSE(server->is_value_supported("missing"));

	// Every renderer has the viewport and Canvas2D values.
	for (const char *name : { "anisotropic_filtering_level", "texture_mipmap_bias", "canvas_item_default_texture_filter", "canvas_item_default_texture_repeat", "snap_2d_transforms_to_pixel", "snap_2d_vertices_to_pixel", "sdf_oversize", "sdf_scale", "canvas_shadow_atlas_size" }) {
		CHECK_MESSAGE(server->is_value_supported(name), name);
	}
	const bool compatibility = OS::get_singleton()->get_current_rendering_method() == "gl_compatibility";
	CHECK(server->is_value_supported("light_projector_filter") == !compatibility);
	CHECK(server->is_value_supported("decal_filter") == !compatibility);
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoTextureSettingsServer] Each value reaches the root viewport at once") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(temp_file("root.ini"));
	server->set_anisotropic_filtering_level(Viewport::ANISOTROPY_16X);
	CHECK(root->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_16X);
	server->set_texture_mipmap_bias(-1.0);
	CHECK(root->get_texture_mipmap_bias() == doctest::Approx(-1.0));
	server->set_canvas_item_default_texture_filter(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	CHECK(root->get_default_canvas_item_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	server->set_canvas_item_default_texture_repeat(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	CHECK(root->get_default_canvas_item_texture_repeat() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	server->set_snap_2d_transforms_to_pixel(true);
	CHECK(root->is_snap_2d_transforms_to_pixel_enabled());
	server->set_snap_2d_vertices_to_pixel(true);
	CHECK(root->is_snap_2d_vertices_to_pixel_enabled());
	server->set_sdf_oversize(Viewport::SDF_OVERSIZE_150_PERCENT);
	CHECK(root->get_sdf_oversize() == Viewport::SDF_OVERSIZE_150_PERCENT);
	server->set_sdf_scale(Viewport::SDF_SCALE_25_PERCENT);
	CHECK(root->get_sdf_scale() == Viewport::SDF_SCALE_25_PERCENT);

	// The global values have no getter: the domain keeps them.
	ERR_PRINT_OFF;
	server->set_light_projector_filter(RenderingServerEnums::LIGHT_PROJECTOR_FILTER_NEAREST);
	server->set_decal_filter(RenderingServerEnums::DECAL_FILTER_LINEAR);
	ERR_PRINT_ON;
	server->set_canvas_shadow_atlas_size(1024);
	CHECK(server->get_light_projector_filter() == RenderingServerEnums::LIGHT_PROJECTOR_FILTER_NEAREST);
	CHECK(server->get_decal_filter() == RenderingServerEnums::DECAL_FILTER_LINEAR);
	CHECK(server->get_canvas_shadow_atlas_size() == 1024);

	// Reading the engine back picks up a change made on the root.
	root->set_sdf_scale(Viewport::SDF_SCALE_100_PERCENT);
	server->read_from_engine();
	CHECK(server->get_sdf_scale() == Viewport::SDF_SCALE_100_PERCENT);
	CHECK(server->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_16X);
	CHECK(server->get_canvas_shadow_atlas_size() == 1024);

	server->reset_to_defaults();
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoTextureSettingsServer] Added viewports and viewports in the group get the values") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_file_path(temp_file("viewports.ini"));
	server->set_canvas_item_default_texture_filter(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);

	// Added by hand: gets the current values at once and every later change.
	SubViewport *added = memnew(SubViewport);
	server->add_viewport(added);
	CHECK(server->has_viewport(added));
	CHECK(added->get_default_canvas_item_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	server->set_snap_2d_transforms_to_pixel(true);
	CHECK(added->is_snap_2d_transforms_to_pixel_enabled());
	server->remove_viewport(added);
	CHECK_FALSE(server->has_viewport(added));
	server->set_snap_2d_transforms_to_pixel(false);
	CHECK(added->is_snap_2d_transforms_to_pixel_enabled());

	// In the group: gets the values when it enters the tree, and later changes while it is in it.
	SubViewport *grouped = memnew(SubViewport);
	grouped->add_to_group(VideoTextureSettingsServer::get_viewport_group());
	root->add_child(grouped);
	CHECK(grouped->get_default_canvas_item_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	server->set_anisotropic_filtering_level(Viewport::ANISOTROPY_2X);
	CHECK(grouped->get_anisotropic_filtering_level() == Viewport::ANISOTROPY_2X);

	// Not in the group: untouched.
	SubViewport *other = memnew(SubViewport);
	root->add_child(other);
	CHECK(other->get_default_canvas_item_texture_filter() == Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR);

	root->remove_child(other);
	root->remove_child(grouped);
	memdelete(other);
	memdelete(grouped);
	memdelete(added);
	memdelete(server);
}

#ifdef MODULE_TEXTURE_STREAMING_ENABLED

// The project values of texture streaming, restored at the end of a test.
struct StreamingProjectState {
	Variant enabled;
	Variant memory_budget_enabled;

	StreamingProjectState() {
		ProjectSettings *project_settings = ProjectSettings::get_singleton();
		enabled = project_settings->get_setting("rendering/textures/streaming/enabled", false);
		memory_budget_enabled = project_settings->get_setting("rendering/textures/streaming/memory_budget_enabled", false);
	}
	~StreamingProjectState() {
		ProjectSettings *project_settings = ProjectSettings::get_singleton();
		project_settings->set_setting("rendering/textures/streaming/enabled", enabled);
		project_settings->set_setting("rendering/textures/streaming/memory_budget_enabled", memory_budget_enabled);
		TextureStreaming *texture_streaming = TextureStreaming::get_singleton();
		if (texture_streaming != nullptr) {
			texture_streaming->set_memory_budget_mb_override(UINT32_MAX);
			texture_streaming->set_min_lod_override(-1);
			texture_streaming->set_max_lod_override(-1);
		}
	}
};

TEST_CASE("[VideoTextureStreamingSettingsServer] The domain exists with the texture_streaming module") {
	TestSetup setup;
	VideoTextureStreamingSettingsServer *server = VideoTextureStreamingSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoTextureStreamingSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_texture_streaming") == server);
	CHECK(server->get_domain() == StringName("video_texture_streaming"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_PRE_DISPLAY);
	CHECK(server->get_file_path() == "user://settings/video_texture_streaming.ini");
	CHECK(Object::cast_to<VideoTextureStreamingSettings>(server->get_settings().ptr()) != nullptr);

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 5);
	for (const char *name : { "enabled", "memory_budget_enabled", "memory_budget_mb", "min_lod", "max_lod" }) {
		CHECK_MESSAGE(names.has(name), name);
		CHECK_MESSAGE(server->is_value_supported(name) == VideoTextureStreamingSettingsServer::is_renderer_supported(), name);
	}
	// Only `enabled` needs a restart: the renderers compile the streaming shader path at startup.
	CHECK(server->get_setting_flags("enabled").has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	CHECK_FALSE(server->get_setting_flags("memory_budget_mb").has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	CHECK_FALSE(server->get_setting_flags("memory_budget_enabled").has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
}

TEST_CASE("[VideoTextureStreamingSettingsServer] Defaults, ranges, storage and restart") {
	TestSetup setup;
	StreamingProjectState project_state;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_budget = project_settings->get_setting("rendering/textures/streaming/memory_budget_mb", 512);
	project_settings->set_setting("rendering/textures/streaming/memory_budget_mb", 256);

	const String path = temp_file("video_texture_streaming.ini");
	{
		VideoTextureStreamingSettingsServer *server = memnew(VideoTextureStreamingSettingsServer);
		server->set_file_path(path);
		CHECK(server->get_memory_budget_mb() == 256);
		CHECK(server->get_min_lod() == int(project_settings->get_setting("rendering/textures/streaming/min_lod", 0)));

		ERR_PRINT_OFF;
		server->set_memory_budget_mb(-5);
		CHECK(server->get_memory_budget_mb() == 0);
		server->set_max_lod(20);
		CHECK(server->get_max_lod() == 13);
		ERR_PRINT_ON;

		server->set_memory_budget_mb(128);
		server->set_min_lod(1);
		server->set_max_lod(5);
		server->set_memory_budget_enabled(true);
		ERR_PRINT_OFF;
		server->set_enabled(!bool(server->get_default_value("enabled")));
		ERR_PRINT_ON;
		// `enabled` takes effect at the next start.
		CHECK(server->get_restart_required_names().has("enabled"));
		CHECK_FALSE(server->get_restart_required_names().has("memory_budget_mb"));

		if (VideoTextureStreamingSettingsServer::is_renderer_supported() && TextureStreaming::get_singleton() != nullptr) {
			TextureStreaming *texture_streaming = TextureStreaming::get_singleton();
			CHECK(texture_streaming->get_memory_budget_mb_override() == 128);
			CHECK(texture_streaming->get_min_lod_override() == 1);
			CHECK(texture_streaming->get_max_lod_override() == 5);
			CHECK(bool(project_settings->get_setting("rendering/textures/streaming/memory_budget_enabled")));
		}
		CHECK(server->save() == OK);
		memdelete(server);
	}

	VideoTextureStreamingSettingsServer *server = memnew(VideoTextureStreamingSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	ERR_PRINT_OFF;
	CHECK(server->load() == OK);
	ERR_PRINT_ON;
	CHECK(server->get_memory_budget_mb() == 128);
	CHECK(server->get_min_lod() == 1);
	CHECK(server->get_max_lod() == 5);
	CHECK(server->is_memory_budget_enabled());
	memdelete(server);

	project_settings->set_setting("rendering/textures/streaming/memory_budget_mb", old_budget);
}

#endif // MODULE_TEXTURE_STREAMING_ENABLED

TEST_CASE("[VideoTextureSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	VideoTextureSettingsServer *server = memnew(VideoTextureSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "texture_mipmap_bias_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("texture_mipmap_bias", 0.5);
	SIGNAL_CHECK_FALSE("texture_mipmap_bias_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 0.5 } };
	SIGNAL_CHECK("texture_mipmap_bias_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("texture_mipmap_bias_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("texture_mipmap_bias");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("texture_mipmap_bias_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("texture_mipmap_bias_changed", default_args);

	SIGNAL_UNWATCH(server, "texture_mipmap_bias_changed");
	memdelete(server);
}

} // namespace TestVideoTextureSettingsServer
