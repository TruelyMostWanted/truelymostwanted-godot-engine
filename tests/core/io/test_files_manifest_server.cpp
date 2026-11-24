/**************************************************************************/
/*  test_files_manifest_server.cpp                                        */
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

TEST_FORCE_LINK(test_files_manifest_server)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/files_manifest_server.h"
#include "core/os/os.h"
#include "tests/signal_watcher.h"

namespace TestFilesManifestServer {

static void _write(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	f->store_string("[gd_resource type=\"Resource\" format=3]\n\n[resource]\n");
}

TEST_CASE("[FilesManifestServer] Register, edit, validate, watch") {
	FilesManifestServer *server = FilesManifestServer::get_singleton();
	REQUIRE(server);
	const String root = OS::get_singleton()->get_cache_path().path_join("test_files_manifest_server");
	DirAccess::make_dir_recursive_absolute(root.path_join("a"));
	_write(root.path_join("a/one.tres"));

	Ref<FilesManifest> manifest;
	manifest.instantiate();
	CHECK(server->register_manifest("fms_test", manifest) == OK);
	CHECK(server->has_manifest("fms_test"));
	CHECK(server->add_folder("fms_test", root.path_join("a")) == OK);
	CHECK(server->add_folder("fms_test", root.path_join("a")) == ERR_ALREADY_EXISTS);
	CHECK(server->add_extension("fms_test", "TRES") == OK);
	CHECK(manifest->get_extensions() == PackedStringArray({ ".tres" }));
	CHECK(server->get_paths("fms_test").size() == 1);

	CHECK(server->add_folder("fms_test", root.path_join("missing")) == OK);
	CHECK(server->add_extension("fms_test", "nosuchext") == OK);
	Dictionary result = server->validate("fms_test");
	CHECK(PackedStringArray(result["missing_folders"]).size() == 1);
	CHECK(PackedStringArray(result["unknown_extensions"]) == PackedStringArray({ ".nosuchext" }));
	CHECK_FALSE(bool(result["valid"]));
	CHECK(server->create_missing_folders("fms_test") == OK);
	CHECK(DirAccess::dir_exists_absolute(root.path_join("missing")));

	SIGNAL_WATCH(server, "file_added");
	SIGNAL_WATCH(server, "file_removed");
	CHECK(server->set_watching("fms_test", true) == OK);
	_write(root.path_join("a/two.tres"));
	server->poll();
	CHECK_FALSE(SignalWatcher::get_singleton()->check_false("file_added"));
	DirAccess::remove_absolute(root.path_join("a/two.tres"));
	server->poll();
	CHECK_FALSE(SignalWatcher::get_singleton()->check_false("file_removed"));
	SIGNAL_UNWATCH(server, "file_added");
	SIGNAL_UNWATCH(server, "file_removed");

	CHECK(server->unregister_manifest("fms_test") == OK);
	CHECK_FALSE(server->has_manifest("fms_test"));
	DirAccess::remove_absolute(root.path_join("a/one.tres"));
	DirAccess::remove_absolute(root.path_join("a"));
	DirAccess::remove_absolute(root.path_join("missing"));
	DirAccess::remove_absolute(root);
}

} // namespace TestFilesManifestServer
