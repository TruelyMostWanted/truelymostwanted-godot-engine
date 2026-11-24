/**************************************************************************/
/*  test_video_window_settings_server.cpp                                 */
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

TEST_FORCE_LINK(test_video_window_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_window_settings.h"
#include "servers/settings/video_window_settings_server.h"
#include "tests/test_utils.h"

namespace TestVideoWindowSettingsServer {

// Autosave off; the test display server has no screen, so the window values are kept by the domain.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("video_window_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

TEST_CASE("[VideoWindowSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	VideoWindowSettingsServer *server = VideoWindowSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoWindowSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_window") == server);
	CHECK(server->get_domain() == StringName("video_window"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_PRE_DISPLAY);
	CHECK(server->get_file_path() == "user://settings/video_window.ini");

	const PackedStringArray names = server->get_setting_names();
	for (const char *name : { "window_mode", "window_size", "screen", "window_centered", "window_position", "borderless", "always_on_top", "unresizable", "transparent", "vsync_mode", "content_scale_mode", "content_scale_aspect", "content_scale_stretch", "content_scale_factor", "hdr_output_requested", "hdr_reference_luminance", "hdr_max_luminance", "window_states" }) {
		CHECK_MESSAGE(names.has(name), name);
	}
	CHECK_FALSE(names.has("max_fps")); // #26.

	// Mode, size and screen go through the confirmation; nothing needs a restart.
	CHECK(server->get_setting_flags("window_mode").has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
	CHECK(server->get_setting_flags("window_size").has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
	CHECK(server->get_setting_flags("screen").has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
	CHECK_FALSE(server->get_setting_flags("vsync_mode").has_flag(SettingsServer::SETTING_FLAG_CONFIRM));
	CHECK_FALSE(server->get_setting_flags("window_mode").has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));

	// No window options on the test command line.
	CHECK(VideoWindowSettingsServer::get_command_line_options().is_empty());
}

TEST_CASE("[VideoWindowSettingsServer] Defaults come from the project settings") {
	TestSetup setup;
	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(temp_file("defaults.ini"));
	const int width = GLOBAL_GET("display/window/size/viewport_width");
	const int height = GLOBAL_GET("display/window/size/viewport_height");
	CHECK(server->get_window_mode() == DisplayServerEnums::WindowMode(int(GLOBAL_GET("display/window/size/mode"))));
	CHECK(server->get_window_size() == Size2i(width, height));
	CHECK(server->get_screen() == DisplayServerEnums::SCREEN_PRIMARY);
	CHECK(server->is_window_centered());
	// `display/window/vsync/vsync_mode` is defined in Main::setup(), which the tests don't run.
	const bool has_vsync_setting = ProjectSettings::get_singleton()->has_setting("display/window/vsync/vsync_mode");
	CHECK(server->get_vsync_mode() == (has_vsync_setting ? DisplayServerEnums::VSyncMode(int(GLOBAL_GET("display/window/vsync/vsync_mode"))) : DisplayServerEnums::VSYNC_ENABLED));
	CHECK(server->is_unresizable() == !bool(GLOBAL_GET("display/window/size/resizable")));
	CHECK(server->get_hdr_reference_luminance() == doctest::Approx(-1.0));
	CHECK(server->get_hdr_max_luminance() == doctest::Approx(-1.0));
	CHECK(Dictionary(server->get_value("window_states")).is_empty());
	memdelete(server);
}

TEST_CASE("[VideoWindowSettingsServer] Values are validated") {
	TestSetup setup;
	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(temp_file("validate.ini"));
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	server->set_window_size(Size2i(0, -5));
	CHECK(server->get_window_size() == Size2i(1, 1));
	server->set_hdr_reference_luminance(-20.0);
	CHECK(server->get_hdr_reference_luminance() == doctest::Approx(-1.0));
	server->set_hdr_max_luminance(1000.0);
	CHECK(server->get_hdr_max_luminance() == doctest::Approx(1000.0));
	server->set_screen(-9);
	CHECK(server->get_screen() == DisplayServerEnums::SCREEN_PRIMARY);
	server->set_content_scale_factor(0.0);
	CHECK(server->get_content_scale_factor() == doctest::Approx(0.1));
	server->set_value("window_states", 5);
	CHECK(server->get_value("window_states").get_type() == Variant::DICTIONARY);
	memdelete(server);
}

TEST_CASE("[VideoWindowSettingsServer] Mode and size wait for the confirmation, V-Sync is applied at once") {
	TestSetup setup;
	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(temp_file("confirm.ini"));

	server->set_vsync_mode(DisplayServerEnums::VSYNC_DISABLED);
	CHECK(server->get_applied_value("vsync_mode") == Variant(int(DisplayServerEnums::VSYNC_DISABLED)));
	CHECK_FALSE(server->is_confirmation_pending());

	server->set_window_mode(DisplayServerEnums::WINDOW_MODE_FULLSCREEN);
	CHECK(server->is_confirmation_pending());
	server->confirm_changes();
	CHECK_FALSE(server->is_confirmation_pending());
	CHECK(server->get_applied_value("window_mode") == Variant(int(DisplayServerEnums::WINDOW_MODE_FULLSCREEN)));

	server->set_window_size(Size2i(1920, 1080));
	CHECK(server->is_confirmation_pending());
	server->cancel_confirmation();
	CHECK(server->get_window_size() != Size2i(1920, 1080));

	// Without a screen the domain keeps the pushed values: read_from_engine() gives them back.
	server->read_from_engine();
	CHECK(server->get_window_mode() == DisplayServerEnums::WINDOW_MODE_FULLSCREEN);
	CHECK(server->get_vsync_mode() == DisplayServerEnums::VSYNC_DISABLED);
	memdelete(server);
}

TEST_CASE("[VideoWindowSettingsServer] Fullscreen and 1920x1080 are saved and given to the main window on startup") {
	TestSetup setup;
	const String path = temp_file("video_window.ini");
	{
		VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
		server->set_file_path(path);
		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_window_mode(DisplayServerEnums::WINDOW_MODE_FULLSCREEN);
		server->set_window_size(Size2i(1920, 1080));
		server->set_borderless(true);
		server->set_vsync_mode(DisplayServerEnums::VSYNC_ADAPTIVE);
		server->set_screen(1);
		CHECK(server->apply() == OK);
		// Mode, size and screen: the player keeps them in the confirmation dialog.
		CHECK(server->is_confirmation_pending());
		server->confirm_changes();
		CHECK(server->save() == OK);
		memdelete(server);
	}

	// "Restart": phase 1, before the display server exists.
	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(path);
	server->startup();
	CHECK(server->get_window_mode() == DisplayServerEnums::WINDOW_MODE_FULLSCREEN);
	CHECK(server->get_window_size() == Size2i(1920, 1080));
	CHECK(server->get_screen() == 1);

	// What Main::setup2() passes to DisplayServer::create().
	VideoWindowSettingsServer::BootWindow boot_window;
	boot_window.mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
	boot_window.vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
	boot_window.flags = DisplayServerEnums::WINDOW_FLAG_ALWAYS_ON_TOP_BIT;
	boot_window.size = Size2i(1152, 648);
	server->get_boot_window(boot_window);
	CHECK(boot_window.mode == DisplayServerEnums::WINDOW_MODE_FULLSCREEN);
	CHECK(boot_window.size == Size2i(1920, 1080));
	CHECK(boot_window.vsync_mode == DisplayServerEnums::VSYNC_ADAPTIVE);
	CHECK((boot_window.flags & DisplayServerEnums::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	// Values the player did not change keep the project value.
	CHECK((boot_window.flags & DisplayServerEnums::WINDOW_FLAG_ALWAYS_ON_TOP_BIT) != 0);

	// The values are handed over once.
	VideoWindowSettingsServer::BootWindow second;
	second.size = Size2i(10, 10);
	server->get_boot_window(second);
	CHECK(second.size == Size2i(10, 10));
	CHECK(second.mode == DisplayServerEnums::WINDOW_MODE_WINDOWED);

	// Without a screen the placement is skipped without errors.
	server->on_display_server_created();
	server->place_main_window();
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoWindowSettingsServer] Window states are saved by key and restored") {
	TestSetup setup;
	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(temp_file("states.ini"));

	Window *inventory = memnew(Window);
	inventory->set_size(Size2i(640, 480));
	inventory->set_position(Point2i(120, 80));
	ERR_PRINT_OFF;
	server->save_window_state(inventory); // Not in the tree and no key: refused.
	ERR_PRINT_ON;
	CHECK_FALSE(server->has_window_state(""));

	server->save_window_state(inventory, "inventory");
	CHECK(server->has_window_state("inventory"));
	const Dictionary state = Dictionary(server->get_value("window_states"))["inventory"];
	CHECK(state["size"] == Variant(Vector2i(640, 480)));
	CHECK(state["position"] == Variant(Vector2i(120, 80)));
	CHECK(state["mode"] == Variant(int(Window::MODE_WINDOWED)));

	Window *restored = memnew(Window);
	CHECK(server->restore_window_state(restored, "inventory"));
	CHECK(restored->get_size() == Size2i(640, 480));
	CHECK(restored->get_position() == Point2i(120, 80));
	CHECK(restored->get_initial_position() == Window::WINDOW_INITIAL_POSITION_ABSOLUTE);
	CHECK_FALSE(server->restore_window_state(restored, "missing"));

	// The file keeps the states as an untyped dictionary: after loading, the vectors are typed again.
	CHECK(server->save() == OK);
	CHECK(server->load() == OK);
	const Dictionary loaded = Dictionary(server->get_value("window_states"))["inventory"];
	CHECK(loaded["size"].get_type() == Variant::VECTOR2I);
	CHECK(loaded["size"] == Variant(Vector2i(640, 480)));
	CHECK(loaded["position"] == Variant(Vector2i(120, 80)));
	CHECK(int(loaded["mode"]) == int(Window::MODE_WINDOWED));
	restored->set_size(Size2i(10, 10));
	CHECK(server->restore_window_state(restored, "inventory"));
	CHECK(restored->get_size() == Size2i(640, 480));

	server->clear_window_state("inventory");
	CHECK_FALSE(server->has_window_state("inventory"));

	memdelete(restored);
	memdelete(inventory);
	memdelete(server);
}

TEST_CASE("[SceneTree][VideoWindowSettingsServer] Content scale reaches the root window, windows get their state when they enter the tree") {
	TestSetup setup;
	Window *root = SceneTree::get_singleton()->get_root().ptr();
	const Window::ContentScaleMode original_mode = root->get_content_scale_mode();
	const real_t original_factor = root->get_content_scale_factor();

	VideoWindowSettingsServer *server = memnew(VideoWindowSettingsServer);
	server->set_file_path(temp_file("scene.ini"));
	server->set_content_scale_mode(Window::CONTENT_SCALE_MODE_CANVAS_ITEMS);
	server->set_content_scale_factor(1.5);
	CHECK(root->get_content_scale_mode() == Window::CONTENT_SCALE_MODE_CANVAS_ITEMS);
	CHECK(root->get_content_scale_factor() == doctest::Approx(1.5));

	// A window that is added later gets its stored state.
	Window *popup = memnew(Window);
	popup->set_name("SettingsPopup");
	popup->set_visible(false);
	root->add_child(popup);
	popup->set_size(Size2i(300, 200));
	popup->set_position(Point2i(40, 30));
	server->save_window_state(popup);
	const String key = String(popup->get_path());
	CHECK(server->has_window_state(key));
	root->remove_child(popup);
	popup->set_size(Size2i(100, 100));
	popup->set_position(Point2i());
	root->add_child(popup);
	CHECK(popup->get_size() == Size2i(300, 200));
	CHECK(popup->get_position() == Point2i(40, 30));

	root->remove_child(popup);
	memdelete(popup);
	memdelete(server);
	root->set_content_scale_mode(original_mode);
	root->set_content_scale_factor(original_factor);
}

} // namespace TestVideoWindowSettingsServer
