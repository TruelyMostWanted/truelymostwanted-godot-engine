/**************************************************************************/
/*  test_streaming_assets_export_plugin.cpp                               */
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

TEST_FORCE_LINK(test_streaming_assets_export_plugin)

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/streaming_assets_server.h"
#include "editor/export/streaming_assets_export_plugin.h"
#include "tests/test_utils.h"

namespace TestStreamingAssetsExportPlugin {

using Plugin = StreamingAssetsExportPlugin;

static void write_file(const String &p_path, const String &p_content) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_content);
}

TEST_CASE("[StreamingAssetsExportPlugin] expand_template") {
	CHECK(Plugin::expand_template(Plugin::DEFAULT_TARGET_FOLDER, "MyGame") == "MyGame_Data/StreamingAssets");
	CHECK(Plugin::expand_template("{exe_name}/{exe_name}.data", "a") == "a/a.data");
	CHECK(Plugin::expand_template("Assets", "MyGame") == "Assets");
	// Unknown placeholders stay unchanged.
	CHECK(Plugin::expand_template("{unknown}/x", "MyGame") == "{unknown}/x");
	// The plugin and the runtime share StreamingAssetsServer::expand_template().
	CHECK(Plugin::expand_template("{app_name}/{exe_name}", "MyGame") == StreamingAssetsServer::expand_template("{app_name}/{exe_name}", "MyGame"));
}

TEST_CASE("[StreamingAssetsExportPlugin] is_valid_target_folder") {
	CHECK(Plugin::is_valid_target_folder("MyGame_Data/StreamingAssets"));
	CHECK(Plugin::is_valid_target_folder("StreamingAssets"));
	CHECK(Plugin::is_valid_target_folder("StreamingAssets/"));
	CHECK(Plugin::is_valid_target_folder("a\\b"));

	CHECK_FALSE(Plugin::is_valid_target_folder(""));
	CHECK_FALSE(Plugin::is_valid_target_folder("."));
	CHECK_FALSE(Plugin::is_valid_target_folder("./a"));
	CHECK_FALSE(Plugin::is_valid_target_folder(".."));
	CHECK_FALSE(Plugin::is_valid_target_folder("a/../b"));
	CHECK_FALSE(Plugin::is_valid_target_folder("a//b"));
	CHECK_FALSE(Plugin::is_valid_target_folder("/abs"));
	CHECK_FALSE(Plugin::is_valid_target_folder("C:/abs"));
	CHECK_FALSE(Plugin::is_valid_target_folder("res://a"));
	CHECK_FALSE(Plugin::is_valid_target_folder("{app_name}/x"));
}

TEST_CASE("[StreamingAssetsExportPlugin] is_path_in_folder") {
	CHECK(Plugin::is_path_in_folder("res://streaming_assets/a.png", "res://streaming_assets"));
	CHECK(Plugin::is_path_in_folder("res://streaming_assets/sub/b.json", "res://streaming_assets/"));
	CHECK(Plugin::is_path_in_folder("res://streaming_assets", "res://streaming_assets"));

	CHECK_FALSE(Plugin::is_path_in_folder("res://streaming_assets_other/a.png", "res://streaming_assets"));
	CHECK_FALSE(Plugin::is_path_in_folder("res://other/streaming_assets/a.png", "res://streaming_assets"));
	CHECK_FALSE(Plugin::is_path_in_folder("res://a.png", "res://"));
	CHECK_FALSE(Plugin::is_path_in_folder("res://a.png", ""));
}

TEST_CASE("[StreamingAssetsExportPlugin] is_file_excluded") {
	CHECK(Plugin::is_file_excluded(".gdignore"));
	CHECK(Plugin::is_file_excluded(".DS_Store"));
	CHECK(Plugin::is_file_excluded("icon.png.import"));
	CHECK(Plugin::is_file_excluded("script.gd.uid"));
	CHECK(Plugin::is_file_excluded(""));

	CHECK_FALSE(Plugin::is_file_excluded("icon.png"));
	CHECK_FALSE(Plugin::is_file_excluded("level.json"));
	CHECK_FALSE(Plugin::is_file_excluded("importer.txt"));
}

TEST_CASE("[StreamingAssetsExportPlugin] collect_files") {
	const String root = TestUtils::get_temp_path("streaming_assets_export_plugin");
	if (DirAccess::dir_exists_absolute(root)) {
		Ref<DirAccess> da = DirAccess::open(root);
		REQUIRE(da.is_valid());
		da->erase_contents_recursive();
	}

	write_file(root.path_join("b.json"), "{}");
	write_file(root.path_join("a.png"), "png");
	write_file(root.path_join("a.png.import"), "[remap]");
	write_file(root.path_join("tool.gd.uid"), "uid://x");
	write_file(root.path_join(".gdignore"), "");
	write_file(root.path_join("sub/c.ogg"), "ogg");
	write_file(root.path_join("sub/deeper/d.bin"), "bin");
	write_file(root.path_join(".hidden/e.txt"), "hidden");

	const PackedStringArray files = Plugin::collect_files(root);
	CHECK(files == PackedStringArray({ "a.png", "b.json", "sub/c.ogg", "sub/deeper/d.bin" }));

	// A trailing slash does not change the result.
	CHECK(Plugin::collect_files(root + "/") == files);

	// A missing folder yields an empty list.
	CHECK(Plugin::collect_files(root.path_join("missing")).is_empty());
	CHECK(Plugin::collect_files("").is_empty());
}

TEST_CASE("[StreamingAssetsExportPlugin] resolve_placement") {
	const PackedStringArray tags_windows = { "pc", "windows", "x86_64" };
	const PackedStringArray tags_linux = { "pc", "linux", "linuxbsd", "x86_64" };
	const PackedStringArray tags_macos = { "pc", "macos", "arm64" };
	const PackedStringArray tags_android = { "mobile", "android", "arm64" };
	const PackedStringArray tags_web = { "web" };
	const PackedStringArray tags_ios = { "mobile", "ios", "arm64" };

	CHECK(Plugin::resolve_placement(tags_windows, "C:/build/Game.exe", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EXTERNAL);
	CHECK(Plugin::resolve_placement(tags_windows, "C:/build/Game.pck", Plugin::FALLBACK_NONE) == Plugin::PLACEMENT_EXTERNAL);
	CHECK(Plugin::resolve_placement(tags_linux, "/build/Game.x86_64", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EXTERNAL);

	// ZIP exports cannot hold an external folder.
	CHECK(Plugin::resolve_placement(tags_windows, "C:/build/Game.zip", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EMBED);
	CHECK(Plugin::resolve_placement(tags_linux, "/build/Game.ZIP", Plugin::FALLBACK_NONE) == Plugin::PLACEMENT_NONE);

	// Bundles (#90), Android and Web use the fallback.
	CHECK(Plugin::resolve_placement(tags_macos, "/build/Game.app", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EMBED);
	CHECK(Plugin::resolve_placement(tags_ios, "/build/Game.ipa", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EMBED);
	CHECK(Plugin::resolve_placement(tags_android, "/build/Game.apk", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EMBED);
	CHECK(Plugin::resolve_placement(tags_web, "/build/index.html", Plugin::FALLBACK_EMBED) == Plugin::PLACEMENT_EMBED);
	CHECK(Plugin::resolve_placement(tags_web, "/build/index.html", Plugin::FALLBACK_NONE) == Plugin::PLACEMENT_NONE);
}

} // namespace TestStreamingAssetsExportPlugin

#endif // TOOLS_ENABLED
