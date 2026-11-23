/**************************************************************************/
/*  test_files_manifest.cpp                                               */
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

TEST_FORCE_LINK(test_files_manifest)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/files_manifest.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "tests/test_utils.h"

namespace TestFilesManifest {

static void remove_tree(const String &p_root) {
	if (!DirAccess::dir_exists_absolute(p_root)) {
		return;
	}
	Ref<DirAccess> da = DirAccess::open(p_root);
	REQUIRE(da.is_valid());
	da->erase_contents_recursive();
	DirAccess::remove_absolute(p_root);
}

// Builds this tree and returns its canonical root:
//   a.tres (Resource), b.res (Resource), m.tres (FilesManifest), note.txt (no resource)
//   sub/c.tres (Resource)
static String create_tree(const String &p_name) {
	const String root = TestUtils::get_temp_path(p_name);
	remove_tree(root);
	REQUIRE(DirAccess::make_dir_recursive_absolute(root.path_join("sub")) == OK);

	Ref<Resource> res;
	res.instantiate();
	REQUIRE(ResourceSaver::save(res, root.path_join("a.tres")) == OK);
	REQUIRE(ResourceSaver::save(res, root.path_join("b.res")) == OK);
	REQUIRE(ResourceSaver::save(res, root.path_join("sub/c.tres")) == OK);
	Ref<FilesManifest> manifest;
	manifest.instantiate();
	REQUIRE(ResourceSaver::save(manifest, root.path_join("m.tres")) == OK);
	{
		Ref<FileAccess> f = FileAccess::open(root.path_join("note.txt"), FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("not a resource");
	}
	return FilesManifest::normalize_path(root);
}

static PackedStringArray make_paths(const String &p_root, const Vector<String> &p_names) {
	PackedStringArray ret;
	for (const String &name : p_names) {
		ret.push_back(p_root.path_join(name));
	}
	return ret;
}

TEST_CASE("[FilesManifest] Extensions are normalized") {
	CHECK(FilesManifest::normalize_extension("png") == ".png");
	CHECK(FilesManifest::normalize_extension("*.PNG") == ".png");
	CHECK(FilesManifest::normalize_extension(" .Tres ") == ".tres");
	CHECK(FilesManifest::normalize_extension("tres.json") == ".tres.json");
	CHECK(FilesManifest::normalize_extension("").is_empty());
	CHECK(FilesManifest::normalize_extension("*").is_empty());

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	manifest->set_extensions({ "png", ".PNG", "", "*.webp" });
	CHECK(manifest->get_extensions() == PackedStringArray({ ".png", ".webp" }));

	CHECK(manifest->matches_extension("res://a/B.PNG"));
	CHECK(manifest->matches_extension("res://a/b.webp"));
	CHECK_FALSE(manifest->matches_extension("res://a/b.png.txt"));
	manifest->set_extensions(PackedStringArray());
	CHECK(manifest->matches_extension("res://a/b.anything"));
}

TEST_CASE("[FilesManifest] Paths are normalized") {
	CHECK(FilesManifest::normalize_path("res://a/../b/") == "res://b");
	CHECK(FilesManifest::normalize_path("data/items") == "res://data/items");
	CHECK(FilesManifest::normalize_path("USER://saves\\a.tres") == "user://saves/a.tres");
	CHECK(FilesManifest::normalize_path("res://") == "res://");

	Error err = OK;
	ERR_PRINT_OFF;
	CHECK(FilesManifest::normalize_path("unknownprefix://x", nullptr, &err).is_empty());
	ERR_PRINT_ON;
	CHECK(err == ERR_FILE_BAD_PATH);
	CHECK(FilesManifest::normalize_path("   ", nullptr, &err).is_empty());
	CHECK(err == ERR_INVALID_PARAMETER);
}

TEST_CASE("[FilesManifest] Folders are filtered by extension, recursion and type") {
	const String root = create_tree("files_manifest_filter");

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	manifest->set_folder_paths({ root });

	// No extensions: every recognized resource, `note.txt` is not one.
	Error err = FAILED;
	CHECK(manifest->get_paths(&err) == make_paths(root, { "a.tres", "b.res", "m.tres", "sub/c.tres" }));
	CHECK(err == OK);

	manifest->set_recursive(false);
	CHECK(manifest->get_paths() == make_paths(root, { "a.tres", "b.res", "m.tres" }));

	manifest->set_recursive(true);
	manifest->set_extensions({ "TRES" });
	CHECK(manifest->get_paths() == make_paths(root, { "a.tres", "m.tres", "sub/c.tres" }));

	manifest->set_type_hint("FilesManifest");
	CHECK(manifest->get_paths() == make_paths(root, { "m.tres" }));

	// Script class names cannot be checked without loading: no filter.
	manifest->set_type_hint("SomeScriptClass");
	CHECK(manifest->get_paths() == make_paths(root, { "a.tres", "m.tres", "sub/c.tres" }));

	remove_tree(root);
}

TEST_CASE("[FilesManifest] Single files are added once and missing ones are reported") {
	const String root = create_tree("files_manifest_files");

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	manifest->set_extensions({ "res" });
	manifest->set_folder_paths({ root });
	// `sub/c.tres` does not match the extension, but single files are always included.
	manifest->set_file_paths({ root.path_join("sub/c.tres"), root.path_join("b.res"), root.path_join("sub/../sub/c.tres") });

	Error err = FAILED;
	CHECK(manifest->get_paths(&err) == make_paths(root, { "b.res", "sub/c.tres" }));
	CHECK(err == OK);

	manifest->set_file_paths({ root.path_join("missing.tres"), root.path_join("a.tres") });
	ERR_PRINT_OFF;
	CHECK(manifest->get_paths(&err) == make_paths(root, { "b.res", "a.tres" }));
	ERR_PRINT_ON;
	CHECK(err == ERR_FILE_NOT_FOUND);

	remove_tree(root);
}

TEST_CASE("[FilesManifest] Missing folders report an error or are created") {
	const String root = create_tree("files_manifest_missing");
	const String missing = root.path_join("new/deeper");

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	manifest->set_folder_paths({ missing, root });
	manifest->set_recursive(false);

	Error err = OK;
	ERR_PRINT_OFF;
	CHECK(manifest->get_paths(&err) == make_paths(root, { "a.tres", "b.res", "m.tres" }));
	ERR_PRINT_ON;
	CHECK(err == ERR_FILE_NOT_FOUND);
	CHECK_FALSE(DirAccess::dir_exists_absolute(missing));

	manifest->set_create_non_existing_folders(true);
	CHECK(manifest->get_paths(&err) == make_paths(root, { "a.tres", "b.res", "m.tres" }));
	CHECK(err == OK);
	CHECK(DirAccess::dir_exists_absolute(missing));

	remove_tree(root);
}

TEST_CASE("[FilesManifest] All resources are loaded, blocking and threaded") {
	const String root = create_tree("files_manifest_load");

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	manifest->set_folder_paths({ root });
	manifest->set_extensions({ "tres", "res" });

	Error err = FAILED;
	const Dictionary loaded = manifest->load_resources(ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	CHECK(err == OK);
	CHECK(loaded.size() == 4);
	const PackedStringArray paths = manifest->get_paths();
	for (const String &path : paths) {
		CHECK(loaded.has(path));
		const Ref<Resource> res = loaded.get(path, Variant());
		CHECK(res.is_valid());
	}
	const Ref<FilesManifest> nested = loaded.get(root.path_join("m.tres"), Variant());
	CHECK(nested.is_valid());

	CHECK(manifest->load_threaded_request_resources(false, ResourceFormatLoader::CACHE_MODE_IGNORE) == OK);
	for (const String &path : paths) {
		const Ref<Resource> res = ResourceLoader::load_threaded_get(path);
		CHECK_MESSAGE(res.is_valid(), vformat("Threaded load of '%s'.", path));
	}

	remove_tree(root);
}

} // namespace TestFilesManifest
