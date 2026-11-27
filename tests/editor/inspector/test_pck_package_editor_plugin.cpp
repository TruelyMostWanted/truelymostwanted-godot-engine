/**************************************************************************/
/*  test_pck_package_editor_plugin.cpp                                    */
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

TEST_FORCE_LINK(test_pck_package_editor_plugin)

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/pck_package.h"
#include "editor/inspector/pck_package_editor_plugin.h"
#include "tests/test_utils.h"

namespace TestPCKPackageEditorPlugin {

using Helper = PCKPackageEditorHelper;

static void write_file(const String &p_path, const String &p_content) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string(p_content);
}

static Dictionary file_entry(const String &p_path, const String &p_source, bool p_encrypt = false) {
	Dictionary entry;
	entry["path"] = p_path;
	entry["source"] = p_source;
	if (p_encrypt) {
		entry["encrypt"] = true;
	}
	return entry;
}

static String path_at(const Array &p_files, int p_index) {
	const Dictionary entry = p_files[p_index];
	return entry.get("path", String());
}

TEST_CASE("[PCKPackageEditorHelper] to_target_path") {
	CHECK(Helper::to_target_path("res://icon.png") == "icon.png");
	CHECK(Helper::to_target_path("res://levels/a/../b/level.tscn") == "levels/b/level.tscn");
	CHECK(Helper::to_target_path("levels\\level.tscn") == "levels/level.tscn");
	CHECK(Helper::to_target_path("/levels/level.tscn") == "levels/level.tscn");
	CHECK(Helper::to_target_path("C:/project/assets/x.txt", "C:/project") == "assets/x.txt");
	CHECK(Helper::to_target_path("C:/project/assets/x.txt", "C:/project/") == "assets/x.txt");
	// The root must match whole folders.
	CHECK(Helper::to_target_path("C:/project2/x.txt", "C:/project") == "");

	CHECK(Helper::to_target_path("") == "");
	CHECK(Helper::to_target_path("res://") == "");
	CHECK(Helper::to_target_path("res://levels/") == "");
	CHECK(Helper::to_target_path("../outside.txt") == "");
	CHECK(Helper::to_target_path("user://save.dat") == "");
	CHECK(Helper::to_target_path("D:/other/x.txt") == "");
}

TEST_CASE("[PCKPackageEditorHelper] is_packable") {
	CHECK(Helper::is_packable("icon.png"));
	CHECK(Helper::is_packable("levels/level.tscn"));
	CHECK_FALSE(Helper::is_packable(""));
	CHECK_FALSE(Helper::is_packable("icon.png.import"));
	CHECK_FALSE(Helper::is_packable("player.gd.uid"));
	CHECK_FALSE(Helper::is_packable(".godot/imported/icon.png-123.ctex"));
	CHECK_FALSE(Helper::is_packable("levels/.hidden/x.txt"));
	CHECK_FALSE(Helper::is_packable(".gitignore"));
}

TEST_CASE("[PCKPackageEditorHelper] add_sources adds files and folders with relative target paths") {
	const String root = TestUtils::get_temp_path("pck_package_editor_add");
	if (DirAccess::dir_exists_absolute(root)) {
		Ref<DirAccess> da = DirAccess::open(root);
		REQUIRE(da.is_valid());
		da->erase_contents_recursive();
	}
	write_file(root.path_join("single.txt"), "single");
	write_file(root.path_join("assets/b.txt"), "b");
	write_file(root.path_join("assets/a.txt"), "a");
	write_file(root.path_join("assets/a.txt.import"), "[remap]");
	write_file(root.path_join("assets/a.txt.uid"), "uid://abc");
	write_file(root.path_join("assets/sub/c.txt"), "c");
	write_file(root.path_join("assets/.hidden/d.txt"), "d");

	const PackedStringArray collected = Helper::collect_files(root.path_join("assets"));
	CHECK(collected.size() == 5);

	int added = 0;
	Array files = Helper::add_sources(Array(), PackedStringArray({ root.path_join("single.txt"), root.path_join("assets") + "/" }), root, false, &added);
	CHECK(added == 4);
	REQUIRE(files.size() == 4);
	CHECK(path_at(files, 0) == "single.txt");
	// The files of a folder come first, by name, then its subfolders.
	CHECK(path_at(files, 1) == "assets/a.txt");
	CHECK(path_at(files, 2) == "assets/b.txt");
	CHECK(path_at(files, 3) == "assets/sub/c.txt");
	const Dictionary first = files[0];
	CHECK(String(first["source"]) == root.path_join("single.txt"));
	CHECK_FALSE(first.has("encrypt"));

	// Adding a target again replaces the entry in place.
	files = Helper::add_sources(files, PackedStringArray({ root.path_join("assets/b.txt") }), root, true, &added);
	CHECK(added == 1);
	REQUIRE(files.size() == 4);
	const Dictionary replaced = files[2];
	CHECK(String(replaced["path"]) == "assets/b.txt");
	CHECK(bool(replaced["encrypt"]));

	// Only sidecars: nothing is added.
	Helper::add_sources(files, PackedStringArray({ root.path_join("assets/a.txt.import") }), root, false, &added);
	CHECK(added == 0);

	// The package accepts the result as its `files`.
	Ref<PCKPackage> package;
	package.instantiate();
	package->set("files", files);
	CHECK(package->get_file_count() == 4);
	CHECK(package->has_file("res://assets/sub/c.txt"));

	Ref<DirAccess> da = DirAccess::open(root);
	REQUIRE(da.is_valid());
	da->erase_contents_recursive();
}

TEST_CASE("[PCKPackageEditorHelper] Edits do not change their input") {
	Array files;
	files.push_back(file_entry("a.txt", "res://a.txt"));
	const Array result = Helper::set_encrypt(files, "a.txt", true);
	const Dictionary original = files[0];
	CHECK_FALSE(original.has("encrypt"));
	const Dictionary changed = result[0];
	CHECK(bool(changed["encrypt"]));
}

TEST_CASE("[PCKPackageEditorHelper] remove, rename and encrypt") {
	Array files;
	files.push_back(file_entry("a.txt", "res://a.txt"));
	files.push_back(file_entry("b.txt", "res://b.txt", true));
	files.push_back(file_entry("c.txt", "res://c.txt"));

	CHECK(Helper::find_file(files, "b.txt") == 1);
	CHECK(Helper::find_file(files, "missing.txt") == -1);

	const Array removed = Helper::remove_files(files, PackedStringArray({ "res://a.txt", "c.txt" }));
	REQUIRE(removed.size() == 1);
	CHECK(path_at(removed, 0) == "b.txt");
	CHECK(files.size() == 3);

	Error err = FAILED;
	Array renamed = Helper::rename_file(files, "b.txt", "res://data/b2.txt", &err);
	CHECK(err == OK);
	CHECK(path_at(renamed, 1) == "data/b2.txt");
	CHECK(path_at(files, 1) == "b.txt");
	const Dictionary renamed_entry = renamed[1];
	CHECK(bool(renamed_entry["encrypt"]));

	Helper::rename_file(files, "b.txt", "c.txt", &err);
	CHECK(err == ERR_ALREADY_EXISTS);
	Helper::rename_file(files, "missing.txt", "x.txt", &err);
	CHECK(err == ERR_DOES_NOT_EXIST);
	Helper::rename_file(files, "b.txt", "folder/", &err);
	CHECK(err == ERR_INVALID_PARAMETER);
	renamed = Helper::rename_file(files, "b.txt", "b.txt", &err);
	CHECK(err == OK);
	CHECK(path_at(renamed, 1) == "b.txt");

	const Array decrypted = Helper::set_encrypt(files, "b.txt", false);
	const Dictionary decrypted_entry = decrypted[1];
	CHECK_FALSE(decrypted_entry.has("encrypt"));
}

TEST_CASE("[PCKPackageEditorHelper] move_sources follows moved files") {
	Array files;
	files.push_back(file_entry("a.txt", "res://a.txt"));
	files.push_back(file_entry("b.txt", "res://b.txt"));
	Dictionary moves;
	moves["res://a.txt"] = "res://moved/a.txt";
	moves["res://other.txt"] = "res://x.txt";
	int moved = 0;
	const Array result = Helper::move_sources(files, moves, &moved);
	CHECK(moved == 1);
	const Dictionary first = result[0];
	const Dictionary second = result[1];
	CHECK(String(first["source"]) == "res://moved/a.txt");
	CHECK(String(first["path"]) == "a.txt");
	CHECK(String(second["source"]) == "res://b.txt");
	const Dictionary original = files[0];
	CHECK(String(original["source"]) == "res://a.txt");
}

TEST_CASE("[PCKPackageEditorHelper] removals") {
	PackedStringArray removals = Helper::add_removals(PackedStringArray(), PackedStringArray({ "res://old/a.txt", "old/a.txt", "res://old/b.txt", "res://" }));
	REQUIRE(removals.size() == 2);
	CHECK(removals[0] == "old/a.txt");
	CHECK(removals[1] == "old/b.txt");

	removals = Helper::remove_removals(removals, PackedStringArray({ "res://old/a.txt" }));
	REQUIRE(removals.size() == 1);
	CHECK(removals[0] == "old/b.txt");

	Ref<PCKPackage> package;
	package.instantiate();
	package->set("removals", removals);
	const PackedStringArray stored = package->get("removals");
	CHECK(stored == removals);
}

} // namespace TestPCKPackageEditorPlugin

#endif // TOOLS_ENABLED
