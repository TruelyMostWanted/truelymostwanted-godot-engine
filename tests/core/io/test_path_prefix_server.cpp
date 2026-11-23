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

#include "core/io/path_prefix_server.h"
#include "core/os/os.h"

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

	CHECK(server->resolve_safe("user://saves/a.json", true)->is_valid());
	CHECK(server->resolve_safe("user://saves/../../x", false)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("user://saves/../a.json", false)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("/etc/passwd", false)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("C:/Windows/win.ini", false)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("nope://a", false)->get_error() == ERR_FILE_BAD_PATH);

	PackedStringArray roots;
	roots.push_back("user://saves/");
	CHECK(server->resolve_safe("user://saves/a.json", true, roots)->is_valid());
	CHECK(server->resolve_safe("user://saves", true, roots)->is_valid());
	CHECK(server->resolve_safe("user://other/a.json", true, roots)->get_error() == ERR_FILE_BAD_PATH);
	CHECK(server->resolve_safe("user://savesx/a.json", true, roots)->get_error() == ERR_FILE_BAD_PATH);
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

} // namespace TestPathPrefixServer
