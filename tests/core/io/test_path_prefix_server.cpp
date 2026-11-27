/**************************************************************************/
/*  test_path_prefix_server.cpp                                           */
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

TEST_FORCE_LINK(test_path_prefix_server)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/path_prefix_server.h"
#include "core/io/pck_packer.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "tests/test_utils.h"

namespace TestPathPrefixServer {

TEST_CASE("[PathPrefixServer] Engine prefixes are registered and locked") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	REQUIRE(server != nullptr);

	const char *engine_prefixes[] = { "res://", "user://", "uid://", "pipe://", "exec://" };
	for (const char *prefix : engine_prefixes) {
		CHECK(server->has_prefix(prefix));
		CHECK(server->is_prefix_locked(prefix));

		// `uid://a/b.txt` is not a valid UID: silence the errors of its resolution.
		ERR_PRINT_OFF;
		const String before = server->globalize(String(prefix) + "a/b.txt");
		CHECK(server->register_prefix_folder(prefix, "C:/somewhere") == ERR_LOCKED);
		CHECK(server->register_prefix_folder(prefix, String(prefix) + "x") == ERR_LOCKED);
		CHECK(server->unregister_prefix(prefix) == ERR_LOCKED);
		CHECK(server->globalize(String(prefix) + "a/b.txt") == before);
		ERR_PRINT_ON;
	}

	const PackedStringArray prefixes = server->get_prefixes();
	REQUIRE(prefixes.size() >= 5);
	CHECK(prefixes[0] == "res://");
	CHECK(prefixes[1] == "user://");
}

TEST_CASE("[PathPrefixServer] Prefix names are validated") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	CHECK(PathPrefixServer::is_valid_prefix_name("mods://"));
	CHECK(PathPrefixServer::is_valid_prefix_name("mods"));
	CHECK(PathPrefixServer::is_valid_prefix_name("save_data2://"));
	CHECK_FALSE(PathPrefixServer::is_valid_prefix_name("Mods://"));
	CHECK_FALSE(PathPrefixServer::is_valid_prefix_name("2mods://"));
	CHECK_FALSE(PathPrefixServer::is_valid_prefix_name("my-mods://"));
	CHECK_FALSE(PathPrefixServer::is_valid_prefix_name("http://"));
	CHECK_FALSE(PathPrefixServer::is_valid_prefix_name("file://"));

	ERR_PRINT_OFF;
	CHECK(server->register_prefix_folder("Bad://", "C:/x") == ERR_INVALID_PARAMETER);
	CHECK(server->register_prefix_folder("https://", "C:/x") == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK_FALSE(server->has_prefix("https://"));
}

TEST_CASE("[PathPrefixServer] resolve() normalizes user:// paths") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	const String user_dir = OS::get_singleton()->get_user_data_dir().replace_char('\\', '/');

	Ref<PathInfo> info = server->resolve("USER://saves\\a.json");
	REQUIRE(info.is_valid());
	CHECK(info->is_valid());
	CHECK(info->get_error() == OK);
	CHECK(info->get_local_path() == "user://saves/a.json");
	CHECK(info->get_prefix() == "user://");
	CHECK(info->get_relative_path() == "saves/a.json");
	CHECK(info->get_file_name() == "a.json");
	CHECK(info->get_extension() == "json");
	CHECK(info->get_base_dir() == "user://saves");
	CHECK(info->is_locked());
	CHECK_FALSE(info->is_read_only());
	if (!user_dir.is_empty()) {
		CHECK(info->get_global_path() == user_dir + "/saves/a.json");

		Ref<PathInfo> back = server->resolve(user_dir + "/saves/a.json");
		CHECK(back->is_valid());
		CHECK(back->get_local_path() == "user://saves/a.json");
		CHECK(server->localize(server->globalize("user://saves/a.json")) == "user://saves/a.json");
	}

	Ref<PathInfo> dots = server->resolve("user://saves/./x/../a.json");
	CHECK(dots->get_local_path() == "user://saves/a.json");
}

TEST_CASE("[PathPrefixServer] resolve() reports unknown prefixes") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	Ref<PathInfo> info = server->resolve("nope://x");
	CHECK_FALSE(info->is_valid());
	CHECK(info->get_error() == ERR_FILE_BAD_PATH);
	CHECK_FALSE(info->get_error_message().is_empty());
	CHECK(info->get_source_path() == "nope://x");

	Ref<PathInfo> outside = server->resolve("user://../x");
	CHECK_FALSE(outside->is_valid());
	CHECK(outside->get_error() == ERR_FILE_BAD_PATH);
}

TEST_CASE("[PathPrefixServer] resolve_safe() rejects unsafe paths") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();

	CHECK(server->resolve_safe("user://saves/a.json", PathPrefixServer::ACCESS_WRITE)->is_valid());
	CHECK(server->resolve_safe("user://saves/../../x", PathPrefixServer::ACCESS_READ)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("user://saves/../a.json", PathPrefixServer::ACCESS_READ)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("/etc/passwd", PathPrefixServer::ACCESS_READ)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("C:/Windows/win.ini", PathPrefixServer::ACCESS_READ)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("nope://a", PathPrefixServer::ACCESS_READ)->get_error() == ERR_FILE_BAD_PATH);

	PackedStringArray roots;
	roots.push_back("user://saves/");
	CHECK(server->resolve_safe("user://saves/a.json", PathPrefixServer::ACCESS_WRITE, roots)->is_valid());
	CHECK(server->resolve_safe("user://saves", PathPrefixServer::ACCESS_WRITE, roots)->is_valid());
	CHECK(server->resolve_safe("user://other/a.json", PathPrefixServer::ACCESS_WRITE, roots)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("user://savesx/a.json", PathPrefixServer::ACCESS_WRITE, roots)->get_error() == ERR_FILE_BAD_PATH);
}

TEST_CASE("[PathPrefixServer] Registered folder prefixes") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	const String user_dir = OS::get_singleton()->get_user_data_dir().replace_char('\\', '/');
	REQUIRE_FALSE(user_dir.is_empty());

	CHECK(server->register_prefix_folder("unittest://", "user://unittest") == OK);
	CHECK(server->has_prefix("unittest://"));
	CHECK(server->is_prefix_available("unittest://"));
	CHECK_FALSE(server->is_prefix_locked("unittest://"));
	CHECK(server->handles_path("unittest://a.txt"));
	CHECK_FALSE(server->handles_path("res://a.txt"));

	CHECK(server->globalize("unittest://a/b.txt") == user_dir + "/unittest/a/b.txt");
	CHECK(server->localize(user_dir + "/unittest/a/b.txt") == "unittest://a/b.txt");

	Ref<PathInfo> info = server->resolve("unittest://a/b.txt");
	CHECK(info->is_valid());
	CHECK_FALSE(info->is_locked());
	CHECK(info->get_global_path() == user_dir + "/unittest/a/b.txt");

	// A registration for a platform that does not match makes the prefix unavailable here.
	CHECK(server->unregister_prefix("unittest://") == OK);
	PackedStringArray platforms;
	platforms.push_back("unittest_platform_that_does_not_exist");
	CHECK(server->register_prefix_folder("unittest://", "user://unittest", platforms) == OK);
	CHECK(server->has_prefix("unittest://"));
	CHECK_FALSE(server->is_prefix_available("unittest://"));
	CHECK(server->resolve("unittest://a")->get_error() == ERR_UNAVAILABLE);
	CHECK(server->globalize("unittest://a") == "unittest://a");

	// A registration for all platforms is used where no named platform matches.
	CHECK(server->register_prefix_folder("unittest://", "user://unittest2") == OK);
	CHECK(server->is_prefix_available("unittest://"));
	CHECK(server->globalize("unittest://a") == user_dir + "/unittest2/a");

	CHECK(server->unregister_prefix("unittest://") == OK);
	CHECK_FALSE(server->has_prefix("unittest://"));
}

// A script-like logic whose `_globalize` returns a `res://` path.
class TestResPathPrefix : public PathPrefix {
	GDSOFTCLASS(TestResPathPrefix, PathPrefix);

public:
	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		return "res://unittest_pck_87/" + p_path.substr(p_prefix.length());
	}
};

TEST_CASE("[PathPrefixServer] resolve_engine_path() follows the chain to an engine prefix") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	const String user_dir = OS::get_singleton()->get_user_data_dir().replace_char('\\', '/');
	REQUIRE_FALSE(user_dir.is_empty());

	// Engine paths and OS paths are returned as they are.
	CHECK(server->resolve_engine_path("res://a/b.txt") == "res://a/b.txt");
	CHECK(server->resolve_engine_path("user://a.txt") == "user://a.txt");
	CHECK(server->resolve_engine_path("C:/a/b.txt") == "C:/a/b.txt");
	CHECK(server->resolve_engine_path("/a/b.txt") == "/a/b.txt");
	CHECK(server->resolve_engine_path("nope://a.txt") == "nope://a.txt");

	REQUIRE(server->register_prefix_folder("unittestres://", "res://unittest_pck_87") == OK);
	REQUIRE(server->register_prefix_folder("unittestchain://", "unittestres://sub") == OK);
	REQUIRE(server->register_prefix_folder("unittestuser://", "user://unittest_87") == OK);
	REQUIRE(server->register_prefix_folder("unittestos://", user_dir + "/unittest_87_os") == OK);
	REQUIRE(server->register_prefix("unittestlogic://", Ref<PathPrefix>(memnew(TestResPathPrefix))) == OK);

	CHECK(server->resolve_engine_path("unittestres://data.txt") == "res://unittest_pck_87/data.txt");
	CHECK(server->resolve_engine_path("unittestres://") == "res://unittest_pck_87");
	CHECK(server->resolve_engine_path("unittestres://x/../data.txt") == "res://unittest_pck_87/data.txt");
	CHECK(server->resolve_engine_path("unittestchain://b.txt") == "res://unittest_pck_87/sub/b.txt");
	CHECK(server->resolve_engine_path("unittestuser://saves/a.json") == "user://unittest_87/saves/a.json");
	CHECK(server->resolve_engine_path("unittestlogic://data.txt") == "res://unittest_pck_87/data.txt");
	// A prefix that ends in an OS path is not an engine path.
	CHECK(server->resolve_engine_path("unittestos://a.txt") == "unittestos://a.txt");

	// Folders that point at each other end after the depth limit, without an error.
	REQUIRE(server->register_prefix_folder("unittestcyclea://", "unittestcycleb://") == OK);
	REQUIRE(server->register_prefix_folder("unittestcycleb://", "unittestcyclea://") == OK);
	CHECK(server->resolve_engine_path("unittestcyclea://a.txt") == "unittestcyclea://a.txt");
	CHECK_FALSE(server->globalize("unittestcyclea://a.txt").is_empty()); // Must return.

	// Unavailable prefixes are not followed.
	PackedStringArray platforms;
	platforms.push_back("unittest_platform_that_does_not_exist");
	REQUIRE(server->register_prefix_folder("unittestgone://", "res://x", platforms) == OK);
	CHECK(server->resolve_engine_path("unittestgone://a.txt") == "unittestgone://a.txt");

	const char *registered[] = { "unittestres://", "unittestchain://", "unittestuser://", "unittestos://", "unittestlogic://", "unittestcyclea://", "unittestcycleb://", "unittestgone://" };
	for (const char *prefix : registered) {
		CHECK(server->unregister_prefix(prefix) == OK);
	}
}

TEST_CASE("[PathPrefixServer] Prefixes into res:// read from a mounted pack") {
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	PackedData *packed_data = PackedData::get_singleton();
	REQUIRE(packed_data);

	// The files exist only in the pack, as in an exported game.
	const String pack_path = TestUtils::get_temp_path("path_prefix_87.pck");
	{
		PCKPacker packer;
		REQUIRE(packer.pck_start(pack_path, 32, "0000000000000000000000000000000000000000000000000000000000000000", false) == OK);
		REQUIRE(packer.add_file_from_buffer("unittest_pck_87/data.txt", String("Packed").to_utf8_buffer(), false) == OK);
		REQUIRE(packer.add_file_from_buffer("unittest_pck_87/sub/b.txt", String("Nested").to_utf8_buffer(), false) == OK);
		REQUIRE(packer.add_file_from_buffer("unittest_pck_87/res.tres", String("[gd_resource type=\"Resource\" format=3]\n\n[resource]\n").to_utf8_buffer(), false) == OK);
		REQUIRE(packer.flush() == OK);
	}
	// Mounted through PackedData directly (see test_pck_package_server.cpp).
	REQUIRE(packed_data->add_pack(pack_path, true, 0) == OK);
	REQUIRE(server->register_prefix_folder("unittestres://", "res://unittest_pck_87") == OK);
	REQUIRE(server->register_prefix_folder("unittestchain://", "unittestres://sub") == OK);

	CHECK(FileAccess::exists("unittestres://data.txt"));
	CHECK(FileAccess::exists("unittestchain://b.txt"));
	CHECK_FALSE(FileAccess::exists("unittestres://missing.txt"));
	CHECK(FileAccess::get_file_as_string("unittestres://data.txt") == "Packed");
	CHECK(FileAccess::get_file_as_string("unittestchain://b.txt") == "Nested");

	Ref<FileAccess> file = FileAccess::open("unittestres://data.txt", FileAccess::READ);
	REQUIRE(file.is_valid());
	// The opened file reports the engine path.
	CHECK(file->get_path() == "res://unittest_pck_87/data.txt");
	file.unref();

	// ResourceLoader: the alias and the res:// path share the cache entry.
	CHECK(ResourceLoader::exists("unittestres://res.tres"));
	Ref<Resource> by_alias = ResourceLoader::load("unittestres://res.tres");
	REQUIRE(by_alias.is_valid());
	CHECK(by_alias->get_path() == "res://unittest_pck_87/res.tres");
	Ref<Resource> by_res = ResourceLoader::load("res://unittest_pck_87/res.tres");
	CHECK(by_res == by_alias);
	by_alias.unref();
	by_res.unref();

	CHECK(server->unregister_prefix("unittestchain://") == OK);
	CHECK(server->unregister_prefix("unittestres://") == OK);
	CHECK(packed_data->remove_pack(pack_path) == OK);
	CHECK_FALSE(FileAccess::exists("res://unittest_pck_87/data.txt"));
}

TEST_CASE("[PathPrefixServer] Prefixes into user:// use the user:// access") {
	// CI runners start without a user data folder; the editor and games create it on startup.
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir());
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	REQUIRE(server->register_prefix_folder("unittestuser://", "user://unittest_87") == OK);

	CHECK(DirAccess::make_dir_recursive_absolute("unittestuser://saves") == OK);
	CHECK(DirAccess::dir_exists_absolute("unittestuser://saves"));
	CHECK(DirAccess::dir_exists_absolute("user://unittest_87/saves"));

	{
		Ref<FileAccess> file = FileAccess::open("unittestuser://saves/a.txt", FileAccess::WRITE);
		REQUIRE(file.is_valid());
		CHECK(file->get_path() == "user://unittest_87/saves/a.txt");
		file->store_string("Saved");
	}
	CHECK(FileAccess::exists("unittestuser://saves/a.txt"));
	CHECK(FileAccess::get_file_as_string("user://unittest_87/saves/a.txt") == "Saved");

	Ref<DirAccess> dir = DirAccess::open("unittestuser://saves");
	REQUIRE(dir.is_valid());
	CHECK(dir->get_current_dir().begins_with("user://"));
	CHECK(dir->file_exists("a.txt"));
	dir.unref();

	CHECK(DirAccess::remove_absolute("unittestuser://saves/a.txt") == OK);
	CHECK(DirAccess::remove_absolute("unittestuser://saves") == OK);
	CHECK_FALSE(DirAccess::dir_exists_absolute("unittestuser://saves"));
	CHECK(DirAccess::remove_absolute("unittestuser://") == OK);

	CHECK(server->unregister_prefix("unittestuser://") == OK);
}

// A read-only logic into user://, like `streaming://` in exports.
class TestReadOnlyPathPrefix : public PathPrefix {
	GDSOFTCLASS(TestReadOnlyPathPrefix, PathPrefix);

public:
	virtual String globalize_path(const String &p_prefix, const String &p_path) const override {
		return "user://unittest_88_ro/" + p_path.substr(p_prefix.length());
	}
	virtual bool is_read_only() const override { return true; }
};

TEST_CASE("[PathPrefixServer] Read-only prefixes refuse writes") {
	DirAccess::make_dir_recursive_absolute("user://unittest_88_ro/sub");
	{
		Ref<FileAccess> file = FileAccess::open("user://unittest_88_ro/a.txt", FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("Shipped");
	}
	PathPrefixServer *server = PathPrefixServer::get_singleton();
	REQUIRE(server->register_prefix("unittestro://", Ref<PathPrefix>(memnew(TestReadOnlyPathPrefix))) == OK);

	// Reads still work.
	CHECK(FileAccess::get_file_as_string("unittestro://a.txt") == "Shipped");
	CHECK(DirAccess::dir_exists_absolute("unittestro://sub"));
	CHECK(server->check_writable("user://a.txt") == OK);
	CHECK(server->check_writable("C:/a.txt") == OK);

	ERR_PRINT_OFF;
	Error err = OK;
	CHECK(FileAccess::open("unittestro://a.txt", FileAccess::WRITE, &err).is_null());
	CHECK(err == ERR_FILE_NO_PERMISSION);
	CHECK(FileAccess::open("unittestro://a.txt", FileAccess::READ_WRITE, &err).is_null());
	CHECK(err == ERR_FILE_NO_PERMISSION);
	CHECK(DirAccess::make_dir_absolute("unittestro://new") == ERR_FILE_NO_PERMISSION);
	CHECK(DirAccess::make_dir_recursive_absolute("unittestro://new/deep") == ERR_FILE_NO_PERMISSION);
	CHECK(DirAccess::remove_absolute("unittestro://a.txt") == ERR_FILE_NO_PERMISSION);
	CHECK(DirAccess::rename_absolute("unittestro://a.txt", "user://unittest_88_b.txt") == ERR_FILE_NO_PERMISSION);
	CHECK(DirAccess::copy_absolute("user://unittest_88_ro/a.txt", "unittestro://b.txt") == ERR_FILE_NO_PERMISSION);
	ERR_PRINT_ON;
	CHECK(FileAccess::get_file_as_string("unittestro://a.txt") == "Shipped");
	CHECK_FALSE(DirAccess::dir_exists_absolute("user://unittest_88_ro/new"));

	CHECK(server->unregister_prefix("unittestro://") == OK);
	DirAccess::remove_absolute("user://unittest_88_ro/a.txt");
	DirAccess::remove_absolute("user://unittest_88_ro/sub");
	DirAccess::remove_absolute("user://unittest_88_ro");
}

} // namespace TestPathPrefixServer
