/**************************************************************************/
/*  test_shadow_settings_server.cpp                                       */
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

TEST_FORCE_LINK(test_shadow_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/shadow_settings.h"
#include "servers/settings/shadow_settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestShadowSettingsServer {

// Autosave off; the project settings a test changes are restored afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

// The positional shadow atlas of the root viewport, restored at the end of a [SceneTree] test.
struct RootState {
	Viewport *root = nullptr;
	int atlas_size = 0;
	bool atlas_16_bits = true;
	Viewport::PositionalShadowAtlasQuadrantSubdiv quadrants[4] = {};

	RootState() {
		root = SceneTree::get_singleton()->get_root().ptr();
		atlas_size = root->get_positional_shadow_atlas_size();
		atlas_16_bits = root->get_positional_shadow_atlas_16_bits();
		for (int i = 0; i < 4; i++) {
			quadrants[i] = root->get_positional_shadow_atlas_quadrant_subdiv(i);
		}
	}
	~RootState() {
		root->set_positional_shadow_atlas_size(atlas_size);
		root->set_positional_shadow_atlas_16_bits(atlas_16_bits);
		for (int i = 0; i < 4; i++) {
			root->set_positional_shadow_atlas_quadrant_subdiv(i, quadrants[i]);
		}
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("shadow_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[ShadowSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	ShadowSettingsServer *server = ShadowSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("ShadowSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("shadow") == server);
	CHECK(server->get_domain() == StringName("shadow"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/shadow.ini");
	CHECK(Object::cast_to<ShadowSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(ShadowSettingsServer::get_viewport_group() == StringName("shadow_settings"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 10);
	for (const char *name : { "directional_shadow_atlas_size", "directional_shadow_16_bits", "directional_soft_shadow_filter_quality", "positional_shadow_atlas_size", "positional_shadow_atlas_16_bits", "positional_shadow_atlas_quad_0", "positional_shadow_atlas_quad_1", "positional_shadow_atlas_quad_2", "positional_shadow_atlas_quad_3", "positional_soft_shadow_filter_quality" }) {
		CHECK_MESSAGE(names.has(name), name);
		// Everything takes effect at once: no confirmation, no restart.
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
		// Every renderer uses every value.
		CHECK_MESSAGE(server->is_value_supported(name), name);
	}
	CHECK(server->get_unsupported_setting_names().is_empty());
	CHECK_FALSE(server->is_value_supported("missing"));
}

TEST_CASE("[ShadowSettingsServer] Defaults are the project settings") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_directional = project_settings->get_setting("rendering/lights_and_shadows/directional_shadow/size", 4096);
	const Variant old_quality = project_settings->get_setting("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", 2);
	const Variant old_quadrant = project_settings->get_setting("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_3_subdiv", 4);
	project_settings->set_setting("rendering/lights_and_shadows/directional_shadow/size", 2048);
	project_settings->set_setting("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", int(RenderingServerEnums::SHADOW_QUALITY_SOFT_ULTRA));
	project_settings->set_setting("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_3_subdiv", int(Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1));

	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	CHECK(server->get_directional_shadow_atlas_size() == 2048);
	CHECK(server->get_positional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_SOFT_ULTRA);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(3) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1);
	CHECK(int(server->get_default_value("directional_shadow_atlas_size")) == 2048);
	memdelete(server);

	project_settings->set_setting("rendering/lights_and_shadows/directional_shadow/size", old_directional);
	project_settings->set_setting("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", old_quality);
	project_settings->set_setting("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_3_subdiv", old_quadrant);
}

TEST_CASE("[ShadowSettingsServer] Atlas sizes become powers of two in range, invalid enums the default") {
	TestSetup setup;
	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	// Rounded up to a power of two, like the renderers do (no warning).
	server->set_directional_shadow_atlas_size(3000);
	CHECK(server->get_directional_shadow_atlas_size() == 4096);
	server->set_positional_shadow_atlas_size(1500);
	CHECK(server->get_positional_shadow_atlas_size() == 2048);
	server->set_directional_shadow_atlas_size(8192);
	CHECK(server->get_directional_shadow_atlas_size() == 8192);
	// 0 disables positional shadows.
	server->set_positional_shadow_atlas_size(0);
	CHECK(server->get_positional_shadow_atlas_size() == 0);

	ERR_PRINT_OFF;
	server->set_directional_shadow_atlas_size(16);
	CHECK(server->get_directional_shadow_atlas_size() == 256);
	server->set_directional_shadow_atlas_size(0);
	CHECK(server->get_directional_shadow_atlas_size() == 256);
	server->set_directional_shadow_atlas_size(100000);
	CHECK(server->get_directional_shadow_atlas_size() == 16384);
	server->set_positional_shadow_atlas_size(100);
	CHECK(server->get_positional_shadow_atlas_size() == 256);
	server->set_positional_shadow_atlas_size(-5);
	CHECK(server->get_positional_shadow_atlas_size() == 0);

	// Enum values out of range: the default.
	server->set_value("directional_soft_shadow_filter_quality", 6.0);
	CHECK(int(server->get_directional_soft_shadow_filter_quality()) == int(server->get_default_value("directional_soft_shadow_filter_quality")));
	server->set_value("positional_shadow_atlas_quad_1", -1.0);
	CHECK(int(server->get_positional_shadow_atlas_quadrant_subdiv(1)) == int(server->get_default_value("positional_shadow_atlas_quad_1")));
	server->set_value("positional_shadow_atlas_quad_2", 7);
	CHECK(int(server->get_positional_shadow_atlas_quadrant_subdiv(2)) == int(server->get_default_value("positional_shadow_atlas_quad_2")));
	// A quadrant index out of range is an error, nothing changes.
	server->set_positional_shadow_atlas_quadrant_subdiv(4, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(4) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	ERR_PRINT_ON;

	// Valid values pass unchanged.
	server->set_positional_soft_shadow_filter_quality(RenderingServerEnums::SHADOW_QUALITY_HARD);
	CHECK(server->get_positional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_HARD);
	server->set_positional_shadow_atlas_quadrant_subdiv(0, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1024);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(0) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1024);
	memdelete(server);
}

TEST_CASE("[ShadowSettingsServer] Values are saved and loaded") {
	TestSetup setup;
	const String path = temp_file("shadow.ini");
	{
		ShadowSettingsServer *server = memnew(ShadowSettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_directional_shadow_atlas_size(2048);
		server->set_directional_shadow_16_bits(false);
		server->set_directional_soft_shadow_filter_quality(RenderingServerEnums::SHADOW_QUALITY_SOFT_HIGH);
		server->set_positional_shadow_atlas_size(1024);
		server->set_positional_shadow_atlas_16_bits(false);
		server->set_positional_shadow_atlas_quadrant_subdiv(0, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1);
		server->set_positional_shadow_atlas_quadrant_subdiv(1, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4);
		server->set_positional_shadow_atlas_quadrant_subdiv(2, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
		server->set_positional_shadow_atlas_quadrant_subdiv(3, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_256);
		server->set_positional_soft_shadow_filter_quality(RenderingServerEnums::SHADOW_QUALITY_SOFT_VERY_LOW);
		// save() writes the applied values.
		ERR_PRINT_OFF;
		CHECK(server->apply() == OK);
		ERR_PRINT_ON;
		CHECK(server->save() == OK);
		memdelete(server);
	}

	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	CHECK(server->load() == OK);
	CHECK(server->get_directional_shadow_atlas_size() == 2048);
	CHECK_FALSE(server->is_directional_shadow_16_bits_enabled());
	CHECK(server->get_directional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_SOFT_HIGH);
	CHECK(server->get_positional_shadow_atlas_size() == 1024);
	CHECK_FALSE(server->is_positional_shadow_atlas_16_bits_enabled());
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(0) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_1);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(1) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(2) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(3) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_256);
	CHECK(server->get_positional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_SOFT_VERY_LOW);
	memdelete(server);
}

TEST_CASE("[SceneTree][ShadowSettingsServer] Each value reaches the engine at once") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_file_path(temp_file("root.ini"));
	server->set_positional_shadow_atlas_size(1024);
	CHECK(root->get_positional_shadow_atlas_size() == 1024);
	server->set_positional_shadow_atlas_16_bits(false);
	CHECK_FALSE(root->get_positional_shadow_atlas_16_bits());
	for (int i = 0; i < 4; i++) {
		server->set_positional_shadow_atlas_quadrant_subdiv(i, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_64);
		CHECK(root->get_positional_shadow_atlas_quadrant_subdiv(i) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_64);
	}

	// The global values have no getter in RenderingServer: the domain keeps them.
	server->set_directional_shadow_atlas_size(2048);
	server->set_directional_shadow_16_bits(false);
	server->set_directional_soft_shadow_filter_quality(RenderingServerEnums::SHADOW_QUALITY_HARD);
	server->set_positional_soft_shadow_filter_quality(RenderingServerEnums::SHADOW_QUALITY_SOFT_MEDIUM);
	CHECK(server->get_directional_shadow_atlas_size() == 2048);
	CHECK_FALSE(server->is_directional_shadow_16_bits_enabled());
	CHECK(server->get_directional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_HARD);
	CHECK(server->get_positional_soft_shadow_filter_quality() == RenderingServerEnums::SHADOW_QUALITY_SOFT_MEDIUM);

	// Reading the engine back picks up a change made on the root.
	root->set_positional_shadow_atlas_size(512);
	root->set_positional_shadow_atlas_quadrant_subdiv(2, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	server->read_from_engine();
	CHECK(server->get_positional_shadow_atlas_size() == 512);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(2) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	CHECK(server->get_positional_shadow_atlas_quadrant_subdiv(1) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_64);
	CHECK(server->get_directional_shadow_atlas_size() == 2048);

	server->reset_to_defaults();
	memdelete(server);
}

TEST_CASE("[SceneTree][ShadowSettingsServer] Added viewports and viewports in the group get the positional atlas") {
	TestSetup setup;
	RootState state;
	Viewport *root = state.root;

	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_file_path(temp_file("viewports.ini"));
	server->set_positional_shadow_atlas_size(1024);

	// Added by hand: gets the current values at once and every later change.
	SubViewport *added = memnew(SubViewport);
	server->add_viewport(added);
	CHECK(server->has_viewport(added));
	CHECK(added->get_positional_shadow_atlas_size() == 1024);
	server->set_positional_shadow_atlas_quadrant_subdiv(0, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	CHECK(added->get_positional_shadow_atlas_quadrant_subdiv(0) == Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	server->remove_viewport(added);
	CHECK_FALSE(server->has_viewport(added));
	server->set_positional_shadow_atlas_size(512);
	CHECK(added->get_positional_shadow_atlas_size() == 1024);

	// In the group: gets the values when it enters the tree, and later changes while it is in it.
	SubViewport *grouped = memnew(SubViewport);
	grouped->add_to_group(ShadowSettingsServer::get_viewport_group());
	root->add_child(grouped);
	CHECK(grouped->get_positional_shadow_atlas_size() == 512);
	server->set_positional_shadow_atlas_16_bits(false);
	CHECK_FALSE(grouped->get_positional_shadow_atlas_16_bits());

	// Not in the group: untouched (the Viewport default).
	SubViewport *other = memnew(SubViewport);
	root->add_child(other);
	CHECK(other->get_positional_shadow_atlas_size() == 2048);

	root->remove_child(other);
	root->remove_child(grouped);
	memdelete(other);
	memdelete(grouped);
	memdelete(added);
	server->reset_to_defaults();
	memdelete(server);
}

TEST_CASE("[ShadowSettingsServer] Typed signals fire once per applied change") {
	TestSetup setup;
	ShadowSettingsServer *server = memnew(ShadowSettingsServer);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "directional_shadow_atlas_size_changed");

	// A pending change does not reach the engine: no signal.
	server->set_value("directional_shadow_atlas_size", 2048);
	SIGNAL_CHECK_FALSE("directional_shadow_atlas_size_changed");

	CHECK(server->apply() == OK);
	const Array args = { { 2048 } };
	SIGNAL_CHECK("directional_shadow_atlas_size_changed", args);

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("directional_shadow_atlas_size_changed");

	// Back to the default: the signal fires again with the default value.
	const Variant default_value = server->get_default_value("directional_shadow_atlas_size");
	server->reset_to_defaults();
	SIGNAL_CHECK_FALSE("directional_shadow_atlas_size_changed");
	CHECK(server->apply() == OK);
	// Signal arguments: one call with one argument. Built explicitly, `{ { variant } }` collapses to one level.
	Array default_call;
	default_call.push_back(default_value);
	Array default_args;
	default_args.push_back(default_call);
	SIGNAL_CHECK("directional_shadow_atlas_size_changed", default_args);

	SIGNAL_UNWATCH(server, "directional_shadow_atlas_size_changed");
	memdelete(server);
}

} // namespace TestShadowSettingsServer
