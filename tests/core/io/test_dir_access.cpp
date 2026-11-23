/**************************************************************************/
/*  test_dir_access.cpp                                                   */
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

TEST_FORCE_LINK(test_dir_access)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "tests/test_utils.h"

namespace TestDirAccess {

static void write_file(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string("x");
}

static PackedStringArray make_paths(const String &p_base, const Vector<String> &p_names) {
	PackedStringArray ret;
	for (const String &name : p_names) {
		ret.push_back(p_base.path_join(name));
	}
	return ret;
}

static void remove_test_tree(const String &p_root) {
	if (!DirAccess::dir_exists_absolute(p_root)) {
		return;
	}
	Ref<DirAccess> da = DirAccess::open(p_root);
	REQUIRE(da.is_valid());
	if (da->is_link(p_root.path_join("link"))) {
		da->remove("link");
	}
	da->erase_contents_recursive();
	DirAccess::remove_absolute(p_root);
}

// Builds this tree (3 levels, mixed extensions in mixed case, a hidden file, an empty folder):
//   a.PNG, b.txt, .hidden.png (hidden)
//   sub1/c.png, sub1/d.WebP, sub1/noext
//   sub1/sub2/e.Tres, sub1/sub2/f.json
//   sub3/ (empty)
// Returns whether a symbolic link `link` -> `sub1` could be created as well.
static bool create_test_tree(const String &p_root) {
	remove_test_tree(p_root);
	REQUIRE(DirAccess::make_dir_recursive_absolute(p_root.path_join("sub1/sub2")) == OK);
	REQUIRE(DirAccess::make_dir_recursive_absolute(p_root.path_join("sub3")) == OK);
	write_file(p_root.path_join("a.PNG"));
	write_file(p_root.path_join("b.txt"));
	write_file(p_root.path_join(".hidden.png"));
	// Hidden by name on Unix, by attribute on Windows.
	FileAccess::set_hidden_attribute(p_root.path_join(".hidden.png"), true);
	write_file(p_root.path_join("sub1/c.png"));
	write_file(p_root.path_join("sub1/d.WebP"));
	write_file(p_root.path_join("sub1/noext"));
	write_file(p_root.path_join("sub1/sub2/e.Tres"));
	write_file(p_root.path_join("sub1/sub2/f.json"));

	Ref<DirAccess> da = DirAccess::open(p_root);
	REQUIRE(da.is_valid());
	ERR_PRINT_OFF;
	// Creating symbolic links can need extra privileges (e.g. on Windows without developer mode).
	bool has_link = da->create_link(p_root.path_join("sub1"), p_root.path_join("link")) == OK;
	ERR_PRINT_ON;
#ifdef WINDOWS_ENABLED
	if (!has_link) {
		// A junction is a folder link that needs no extra privileges.
		List<String> args = { "/c", "mklink", "/J", p_root.path_join("link").replace_char('/', '\\'), p_root.path_join("sub1").replace_char('/', '\\') };
		String output;
		int exit_code = -1;
		has_link = OS::get_singleton()->execute("cmd.exe", args, &output, &exit_code, true) == OK && exit_code == 0;
	}
#endif
	return has_link && da->is_link(p_root.path_join("link"));
}

static PackedStringArray expected_directories(const String &p_base, bool p_recursive, bool p_has_link) {
	Vector<String> names;
	if (p_has_link) {
		names.push_back("link");
	}
	names.push_back("sub1");
	if (p_recursive) {
		names.push_back("sub1/sub2");
	}
	names.push_back("sub3");
	return make_paths(p_base, names);
}

TEST_CASE("[DirAccess] File paths, directory paths and extensions") {
	const String root = TestUtils::get_temp_path("dir_access_paths");
	const bool has_link = create_test_tree(root);
	if (!has_link) {
		MESSAGE("Symbolic links can't be created here; the link cases are skipped.");
	}

	SUBCASE("get_file_paths_at") {
		CHECK(DirAccess::get_file_paths_at(root) == make_paths(root, { "a.PNG", "b.txt" }));
		CHECK(DirAccess::get_file_paths_at(root, true) == make_paths(root, { "a.PNG", "b.txt", "sub1/c.png", "sub1/d.WebP", "sub1/noext", "sub1/sub2/e.Tres", "sub1/sub2/f.json" }));
		CHECK(DirAccess::get_file_paths_at(root, true, { "png", "webp" }) == make_paths(root, { "a.PNG", "sub1/c.png", "sub1/d.WebP" }));
		CHECK(DirAccess::get_file_paths_at(root, true, { ".PNG" }) == make_paths(root, { "a.PNG", "sub1/c.png" }));
		CHECK(DirAccess::get_file_paths_at(root, false, { "json" }).is_empty());
		CHECK(DirAccess::get_file_paths_at(root + "/", true, { "json" }) == make_paths(root, { "sub1/sub2/f.json" }));
	}

	SUBCASE("get_directory_paths_at") {
		CHECK(DirAccess::get_directory_paths_at(root) == expected_directories(root, false, has_link));
		// The link is listed, but not followed.
		CHECK(DirAccess::get_directory_paths_at(root, true) == expected_directories(root, true, has_link));
		CHECK(DirAccess::get_directory_paths_at(root.path_join("sub3"), true).is_empty());
	}

	SUBCASE("get_file_extensions_at") {
		CHECK(DirAccess::get_file_extensions_at(root) == PackedStringArray({ "png", "txt" }));
		CHECK(DirAccess::get_file_extensions_at(root, true) == PackedStringArray({ "json", "png", "tres", "txt", "webp" }));
	}

	SUBCASE("Instance methods respect include_hidden") {
		Ref<DirAccess> da = DirAccess::open(root);
		REQUIRE(da.is_valid());
		const String base = da->get_current_dir();
		CHECK(da->get_file_paths(false, { "png" }) == make_paths(base, { "a.PNG" }));
		da->set_include_hidden(true);
		CHECK(da->get_file_paths(false, { "png" }) == make_paths(base, { ".hidden.png", "a.PNG" }));
		CHECK(da->get_file_paths(true, { "png" }) == make_paths(base, { ".hidden.png", "a.PNG", "sub1/c.png" }));
		CHECK(da->get_file_extensions(true) == PackedStringArray({ "json", "png", "tres", "txt", "webp" }));

		// Navigational entries are never returned or followed.
		da->set_include_navigational(true);
		CHECK(da->get_directory_paths(true) == expected_directories(base, true, has_link));
	}

	SUBCASE("Missing folder") {
		ERR_PRINT_OFF;
		CHECK(DirAccess::get_file_paths_at(root.path_join("missing"), true).is_empty());
		CHECK(DirAccess::get_directory_paths_at(root.path_join("missing"), true).is_empty());
		CHECK(DirAccess::get_file_extensions_at(root.path_join("missing"), true).is_empty());
		ERR_PRINT_ON;
	}

	remove_test_tree(root);
}

} // namespace TestDirAccess
