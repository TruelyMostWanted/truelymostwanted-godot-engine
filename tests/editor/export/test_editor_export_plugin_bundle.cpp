/**************************************************************************/
/*  test_editor_export_plugin_bundle.cpp                                  */
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

TEST_FORCE_LINK(test_editor_export_plugin_bundle)

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/export/editor_export_plugin.h"
#include "tests/test_utils.h"

namespace TestEditorExportPluginBundle {

// Exposes the protected registration method of EditorExportPlugin.
class BundleTestPlugin : public EditorExportPlugin {
public:
	using EditorExportPlugin::add_bundle_resource;
};

static void write_file(const String &p_path, const String &p_content) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_content);
}

static String read_file(const String &p_path) {
	return FileAccess::get_file_as_string(p_path);
}

static String clean_temp_dir(const String &p_suffix) {
	const String root = TestUtils::get_temp_path(p_suffix);
	if (DirAccess::dir_exists_absolute(root)) {
		Ref<DirAccess> da = DirAccess::open(root);
		REQUIRE(da.is_valid());
		da->erase_contents_recursive();
	}
	DirAccess::make_dir_recursive_absolute(root);
	return root;
}

TEST_CASE("[EditorExportPlugin] is_valid_bundle_target") {
	CHECK(EditorExportPlugin::is_valid_bundle_target("data.pck"));
	CHECK(EditorExportPlugin::is_valid_bundle_target("MyGame_Data/StreamingAssets"));
	CHECK(EditorExportPlugin::is_valid_bundle_target("a/b/c.json"));
	CHECK(EditorExportPlugin::is_valid_bundle_target("name.with.dots"));

	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target(""));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("/abs"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("C:/abs"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("res://a"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("."));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target(".."));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("a/../b"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("./a"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("a//b"));
	CHECK_FALSE(EditorExportPlugin::is_valid_bundle_target("a\\b"));
}

TEST_CASE("[EditorExportPlugin] add_bundle_resource registers files and folders") {
	Ref<BundleTestPlugin> plugin;
	plugin.instantiate();
	CHECK(plugin->get_bundle_resources().is_empty());

	// An empty target keeps the file or folder name.
	plugin->add_bundle_resource("res://streaming_assets/", "");
	plugin->add_bundle_resource("res://build/game.streaming.pck");
	// A target renames the copy and may be nested.
	plugin->add_bundle_resource("res://streaming_assets", "MyGame_Data/StreamingAssets/");
	plugin->add_bundle_resource("C:\\data\\file.bin", "sub\\file.bin");

	const Vector<EditorExportPlugin::BundleResource> &resources = plugin->get_bundle_resources();
	REQUIRE(resources.size() == 4);
	CHECK(resources[0].path == "res://streaming_assets");
	CHECK(resources[0].target == "streaming_assets");
	CHECK(resources[1].path == "res://build/game.streaming.pck");
	CHECK(resources[1].target == "game.streaming.pck");
	CHECK(resources[2].path == "res://streaming_assets");
	CHECK(resources[2].target == "MyGame_Data/StreamingAssets");
	CHECK(resources[3].path == "C:/data/file.bin");
	CHECK(resources[3].target == "sub/file.bin");
}

TEST_CASE("[EditorExportPlugin] add_bundle_resource rejects invalid input") {
	Ref<BundleTestPlugin> plugin;
	plugin.instantiate();

	ERR_PRINT_OFF;
	plugin->add_bundle_resource("", "a");
	plugin->add_bundle_resource("   ", "");
	plugin->add_bundle_resource("res://a.txt", "../outside.txt");
	plugin->add_bundle_resource("res://a.txt", "/abs.txt");
	plugin->add_bundle_resource("res://a.txt", "C:/abs.txt");
	plugin->add_bundle_resource("res://", ""); // No file name to use as target.
	ERR_PRINT_ON;

	CHECK(plugin->get_bundle_resources().is_empty());
}

TEST_CASE("[EditorExportPlugin] copy_bundle_resources") {
	const String source = clean_temp_dir("bundle_resources_source");
	const String dest = clean_temp_dir("bundle_resources_dest");

	write_file(source.path_join("assets/a.json"), "a");
	write_file(source.path_join("assets/sub/b.txt"), "b");
	write_file(source.path_join("game.streaming.pck"), "pck");

	Ref<BundleTestPlugin> plugin;
	plugin.instantiate();
	plugin->add_bundle_resource(source.path_join("assets"), "MyGame_Data/StreamingAssets");
	plugin->add_bundle_resource(source.path_join("game.streaming.pck"));
	plugin->add_bundle_resource(source.path_join("assets/sub/b.txt"), "extra/renamed.txt");

	String failed_path;
	CHECK(plugin->copy_bundle_resources(dest, failed_path) == OK);
	CHECK(failed_path.is_empty());
	CHECK(read_file(dest.path_join("MyGame_Data/StreamingAssets/a.json")) == "a");
	CHECK(read_file(dest.path_join("MyGame_Data/StreamingAssets/sub/b.txt")) == "b");
	CHECK(read_file(dest.path_join("game.streaming.pck")) == "pck");
	CHECK(read_file(dest.path_join("extra/renamed.txt")) == "b");

	// Copying again overwrites the previous copy.
	CHECK(plugin->copy_bundle_resources(dest, failed_path) == OK);

	// A missing source stops the copy and names the source.
	Ref<BundleTestPlugin> missing;
	missing.instantiate();
	missing->add_bundle_resource(source.path_join("missing.bin"));
	ERR_PRINT_OFF;
	CHECK(missing->copy_bundle_resources(dest, failed_path) == ERR_FILE_NOT_FOUND);
	ERR_PRINT_ON;
	CHECK(failed_path == source.path_join("missing.bin"));
	CHECK_FALSE(FileAccess::exists(dest.path_join("missing.bin")));
}

} // namespace TestEditorExportPluginBundle

#endif // TOOLS_ENABLED
