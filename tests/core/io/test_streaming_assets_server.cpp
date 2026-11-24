/**************************************************************************/
/*  test_streaming_assets_server.cpp                                      */
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

TEST_FORCE_LINK(test_streaming_assets_server)

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/path_prefix_server.h"
#include "core/io/pck_package_server.h"
#include "core/io/pck_packer.h"
#include "core/io/streaming_assets_server.h"
#include "tests/test_utils.h"

namespace TestStreamingAssetsServer {

using Server = StreamingAssetsServer;

static void _write_text(const String &p_path, const String &p_content) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_content);
}

// A configuration for an exported game on a desktop platform below `p_exe_dir`.
static Server::Configuration _export_config(const String &p_exe_dir) {
	Server::Configuration config;
	config.enabled = true;
	config.target_folder = "MyGame_Data/StreamingAssets";
	config.pack_file = "MyGame.streaming.pck";
	config.external = true;
	config.executable_dir = p_exe_dir;
	config.main_pack_dir = p_exe_dir;
	return config;
}

// Puts the server back into the state of the test run (disabled).
static void _reset() {
	Server::get_singleton()->configure(Server::Configuration());
}

TEST_CASE("[StreamingAssetsServer] Settings are registered with their defaults") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps->has_setting(Server::SETTING_ENABLED));
	CHECK(ps->has_setting(Server::SETTING_SOURCE_FOLDER));
	CHECK(ps->has_setting(Server::SETTING_MODE));
	CHECK(ps->has_setting(Server::SETTING_TARGET_FOLDER));
	CHECK(ps->has_setting(Server::SETTING_PACK_FILE));
	CHECK(ps->has_setting(Server::SETTING_FALLBACK));

	CHECK(bool(ps->property_get_revert(Server::SETTING_ENABLED)) == false);
	CHECK(String(ps->property_get_revert(Server::SETTING_SOURCE_FOLDER)) == "res://streaming_assets");
	CHECK(int(ps->property_get_revert(Server::SETTING_MODE)) == Server::MODE_RAW);
	CHECK(String(ps->property_get_revert(Server::SETTING_TARGET_FOLDER)) == "{exe_name}_Data/StreamingAssets");
	CHECK(String(ps->property_get_revert(Server::SETTING_PACK_FILE)) == "{exe_name}.streaming.pck");
	CHECK(int(ps->property_get_revert(Server::SETTING_FALLBACK)) == Server::FALLBACK_EMBED);

	// The feature is off by default: nothing resolves.
	CHECK_FALSE(Server::get_singleton()->is_enabled());
	CHECK(Server::get_singleton()->get_location() == Server::LOCATION_NONE);
}

TEST_CASE("[StreamingAssetsServer] streaming:// is a locked prefix") {
	PathPrefixServer *prefixes = PathPrefixServer::get_singleton();
	CHECK(prefixes->has_prefix("streaming://"));
	CHECK(prefixes->is_prefix_locked("streaming://"));
	CHECK_FALSE(prefixes->is_prefix_available("streaming://"));

	ERR_PRINT_OFF;
	CHECK(prefixes->register_prefix_folder("streaming://", "user://elsewhere") == ERR_LOCKED);
	CHECK(prefixes->unregister_prefix("streaming://") == ERR_LOCKED);
	ERR_PRINT_ON;
	CHECK(prefixes->is_prefix_locked("streaming://"));
}

TEST_CASE("[StreamingAssetsServer] expand_template") {
	CHECK(Server::expand_template(Server::DEFAULT_TARGET_FOLDER, "MyGame") == "MyGame_Data/StreamingAssets");
	CHECK(Server::expand_template(Server::DEFAULT_PACK_FILE, "MyGame") == "MyGame.streaming.pck");
	CHECK(Server::expand_template("{exe_name}/{exe_name}", "a") == "a/a");
	CHECK(Server::expand_template("Assets", "MyGame") == "Assets");
	CHECK(Server::expand_template("{unknown}", "MyGame") == "{unknown}");

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const Variant old_name = ps->get_setting("application/config/name");
	const Variant old_version = ps->get_setting("application/config/version");
	ps->set_setting("application/config/name", "My Game");
	ps->set_setting("application/config/version", "1.2.0");
	CHECK(Server::expand_template("{app_name}/{version}", "x") == "My Game/1.2.0");
	ps->set_setting("application/config/name", "a:b");
	CHECK_FALSE(Server::expand_template("{app_name}", "x").contains_char(':'));
	ps->set_setting("application/config/name", "");
	CHECK(Server::expand_template("{app_name}", "x") == "x");
	ps->set_setting("application/config/name", old_name);
	ps->set_setting("application/config/version", old_version);

	CHECK_FALSE(Server::get_exe_name().is_empty());
}

TEST_CASE("[StreamingAssetsServer] Disabled") {
	Server *server = Server::get_singleton();
	Server::Configuration config = _export_config(TestUtils::get_temp_path("streaming_assets_disabled"));
	config.enabled = false;
	server->configure(config);
	CHECK_FALSE(server->is_enabled());
	CHECK(server->get_location() == Server::LOCATION_NONE);
	CHECK(server->get_root().is_empty());
	CHECK(server->get_os_path("streaming://a.txt").is_empty());
	CHECK_FALSE(PathPrefixServer::get_singleton()->is_prefix_available("streaming://"));
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Source folder in the editor") {
	Server *server = Server::get_singleton();
	Server::Configuration config;
	config.enabled = true;
	config.from_source = true;
	config.source_folder = "res://my_streaming/";
	server->configure(config);

	CHECK(server->is_enabled());
	CHECK(server->get_location() == Server::LOCATION_SOURCE_FOLDER);
	CHECK(server->get_source_folder() == "res://my_streaming");
	CHECK(server->get_root() == "res://my_streaming");
	CHECK(PathPrefixServer::get_singleton()->is_prefix_available("streaming://"));
	CHECK_FALSE(PathPrefixServer::get_singleton()->is_prefix_read_only("streaming://"));

	const String expected = ProjectSettings::get_singleton()->globalize_path("res://my_streaming/a/b.txt");
	CHECK(server->get_os_path("streaming://a/b.txt") == expected);
	CHECK(server->get_os_path("a/b.txt") == expected);
	CHECK(server->get_os_path("streaming://a/./c/../b.txt") == expected);
	CHECK(PathPrefixServer::get_singleton()->globalize("streaming://a/b.txt") == expected);
	CHECK(PathPrefixServer::get_singleton()->resolve_engine_path("streaming://a/b.txt") == "res://my_streaming/a/b.txt");

	// Paths that leave the folder or use another prefix have no OS path here.
	CHECK(server->get_os_path("streaming://../project.godot").is_empty());
	CHECK(server->get_os_path("user://a.txt").is_empty());
	_reset();
}

TEST_CASE("[StreamingAssetsServer] External folder next to the executable") {
	Server *server = Server::get_singleton();
	const String exe_dir = TestUtils::get_temp_path("streaming_assets_external");
	const String folder = exe_dir.path_join("MyGame_Data/StreamingAssets");
	_write_text(folder.path_join("config/levels.json"), "[1, 2, 3]");

	server->configure(_export_config(exe_dir));
	CHECK(server->get_location() == Server::LOCATION_EXTERNAL_FOLDER);
	CHECK(server->get_root() == folder);
	CHECK(server->get_os_path("streaming://config/levels.json") == folder.path_join("config/levels.json"));
	CHECK(server->get_os_path("streaming://") == folder);
	CHECK(PathPrefixServer::get_singleton()->is_prefix_read_only("streaming://"));

	PathPrefixServer *prefixes = PathPrefixServer::get_singleton();
	CHECK(prefixes->globalize("streaming://config/levels.json") == folder.path_join("config/levels.json"));
	CHECK(prefixes->localize(folder.path_join("config/levels.json")) == "streaming://config/levels.json");

	Ref<FileAccess> f = FileAccess::open("streaming://config/levels.json", FileAccess::READ);
	REQUIRE(f.is_valid());
	CHECK(f->get_as_text() == "[1, 2, 3]");
	f.unref();
	CHECK(DirAccess::exists("streaming://config"));
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Bundle resources win over the executable folder") {
	Server *server = Server::get_singleton();
	const String exe_dir = TestUtils::get_temp_path("streaming_assets_bundle/MacOS");
	const String resources = TestUtils::get_temp_path("streaming_assets_bundle/Resources");
	DirAccess::make_dir_recursive_absolute(exe_dir.path_join("MyGame_Data/StreamingAssets"));
	DirAccess::make_dir_recursive_absolute(resources.path_join("MyGame_Data/StreamingAssets"));

	Server::Configuration config = _export_config(exe_dir);
	config.bundle = true;
	config.bundle_resource_dir = resources;
	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_BUNDLE);
	CHECK(server->get_root() == resources.path_join("MyGame_Data/StreamingAssets"));
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Fallback without an external folder") {
	Server *server = Server::get_singleton();
	const String exe_dir = TestUtils::get_temp_path("streaming_assets_fallback");
	DirAccess::make_dir_recursive_absolute(exe_dir);

	Server::Configuration config = _export_config(exe_dir);
	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);
	CHECK(server->get_root() == "res://streaming_assets");
	CHECK(server->get_os_path("streaming://a.txt").is_empty());
	CHECK(PathPrefixServer::get_singleton()->resolve_engine_path("streaming://a.txt") == "res://streaming_assets/a.txt");

	// Android and Web never look for an external folder.
	const String folder = exe_dir.path_join("MyGame_Data/StreamingAssets");
	DirAccess::make_dir_recursive_absolute(folder);
	config.external = false;
	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);

	config.fallback = Server::FALLBACK_NONE;
	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_NONE);
	CHECK_FALSE(PathPrefixServer::get_singleton()->is_prefix_available("streaming://"));
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Invalid settings") {
	Server *server = Server::get_singleton();
	const String exe_dir = TestUtils::get_temp_path("streaming_assets_invalid");
	DirAccess::make_dir_recursive_absolute(exe_dir);

	Server::Configuration config = _export_config(exe_dir);
	config.target_folder = "../outside";
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);

	config.target_folder = "{unknown}/x";
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);

	// A source folder outside `res://` falls back to the default.
	config.source_folder = "user://somewhere";
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_source_folder() == "res://streaming_assets");
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Command line override") {
	Server *server = Server::get_singleton();
	const String exe_dir = TestUtils::get_temp_path("streaming_assets_override_exe");
	const String override_dir = TestUtils::get_temp_path("streaming_assets_override");
	DirAccess::make_dir_recursive_absolute(override_dir);

	Server::Configuration config = _export_config(exe_dir);
	config.from_source = true;
	config.override_folder = override_dir;
	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_EXTERNAL_FOLDER);
	CHECK(server->get_root() == override_dir);

	config.override_folder = override_dir.path_join("missing");
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);
	_reset();
}

TEST_CASE("[StreamingAssetsServer] Pack mode") {
	Server *server = Server::get_singleton();
	PckPackageServer *pck_server = PckPackageServer::get_singleton();
	PackedData *packed_data = PackedData::get_singleton();
	REQUIRE(packed_data != nullptr);

	const String exe_dir = TestUtils::get_temp_path("streaming_assets_pack");
	DirAccess::make_dir_recursive_absolute(exe_dir);
	Server::Configuration config = _export_config(exe_dir);
	config.mode = Server::MODE_PACK;

	// No pack: the fallback decides.
	server->configure(config);
	CHECK(server->get_mode() == Server::MODE_PACK);
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);
	config.fallback = Server::FALLBACK_NONE;
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_location() == Server::LOCATION_NONE);

	// A pack next to the main pack. It is mounted through PackedData here: `mount()` goes through
	// ProjectSettings, which switches the default `res://` directory access for the whole test run.
	const String pack_path = exe_dir.path_join("MyGame.streaming.pck");
	PCKPacker packer;
	REQUIRE(packer.pck_start(pack_path) == OK);
	REQUIRE(packer.add_file_from_buffer("streaming_assets/pack_mode.txt", String("Streamed").to_utf8_buffer()) == OK);
	REQUIRE(packer.flush() == OK);
	REQUIRE(packed_data->add_pack(pack_path, true, 0) == OK);

	server->configure(config);
	CHECK(server->get_location() == Server::LOCATION_PACK);
	CHECK(server->get_root() == "res://streaming_assets");
	CHECK(server->get_os_path("streaming://pack_mode.txt").is_empty());
	CHECK(pck_server->is_mounted(pack_path));

	// An invalid pack file name is reported and falls back.
	config.pack_file = "../MyGame.streaming.pck";
	config.fallback = Server::FALLBACK_EMBED;
	ERR_PRINT_OFF;
	server->configure(config);
	ERR_PRINT_ON;
	CHECK(server->get_location() == Server::LOCATION_EMBEDDED);

	CHECK(pck_server->unmount(pack_path) == OK);
	_reset();
}

} // namespace TestStreamingAssetsServer
