/**************************************************************************/
/*  test_resource_result.cpp                                              */
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

TEST_FORCE_LINK(test_resource_result)

#include "core/core_bind.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_error_capture.h"
#include "core/io/resource_format_callable.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_result.h"
#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "tests/test_utils.h"

namespace TestResourceResult {

// Formats of the tests: "lrtext" holds one line of text as the "text" meta of a plain Resource,
// "lrbroken" always fails like a corrupt file, "lrimage" claims to hold an Image but loads a
// plain Resource (a type mismatch for the type hint "Image").
static Variant load_text(const String &p_path) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		return int(err);
	}
	Ref<Resource> resource;
	resource.instantiate();
	resource->set_meta("text", text);
	return resource;
}

static Variant save_text(const Ref<Resource> &p_resource, const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return int(ERR_CANT_CREATE);
	}
	file->store_string(p_resource->get_meta("text", ""));
	return Variant();
}

static Variant load_broken(const String &p_path) {
	return int(ERR_FILE_CORRUPT);
}

static void register_formats() {
	ResourceFormatLoaderCallable::register_extension("lrtext", callable_mp_static(&load_text), "Resource");
	ResourceFormatSaverCallable::register_extension("lrtext", callable_mp_static(&save_text), "Resource");
	ResourceFormatLoaderCallable::register_extension("lrbroken", callable_mp_static(&load_broken), "Resource");
	ResourceFormatLoaderCallable::register_extension("lrimage", callable_mp_static(&load_text), "Image");
}

static void unregister_formats() {
	ResourceFormatLoaderCallable::unregister_extension("lrtext");
	ResourceFormatSaverCallable::unregister_extension("lrtext");
	ResourceFormatLoaderCallable::unregister_extension("lrbroken");
	ResourceFormatLoaderCallable::unregister_extension("lrimage");
}

static String write_test_file(const String &p_name, const String &p_text) {
	const String path = TestUtils::get_temp_path(p_name);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(p_text);
	}
	return path;
}

static CoreBind::ResourceLoader *loader() {
	return CoreBind::ResourceLoader::get_singleton();
}

static CoreBind::ResourceSaver *saver() {
	return CoreBind::ResourceSaver::get_singleton();
}

TEST_CASE("[ResourceErrorCapture] Captures the errors of its scope, and nested scopes pass them on") {
	ResourceErrorCapture outer;
	CHECK_FALSE(outer.has_errors());

	ERR_PRINT_OFF;
	{
		ResourceErrorCapture inner;
		ERR_PRINT("First error.");
		WARN_PRINT("A warning is not captured.");
		CHECK(inner.has_errors());
		CHECK(inner.get_message() == "First error.");
	}
	ERR_PRINT("Second error.");
	ERR_PRINT_ON;

	CHECK(outer.get_message() == "First error.\nSecond error.");
}

TEST_CASE("[ResourceErrorCapture] Keeps a bounded number of lines") {
	ResourceErrorCapture capture;
	ERR_PRINT_OFF;
	for (int i = 0; i < 20; i++) {
		ERR_PRINT("Repeated error.");
	}
	ERR_PRINT_ON;
	CHECK(capture.get_message().ends_with("(4 more errors)"));
}

TEST_CASE("[ResourceLoadResult] A missing file gives ERR_FILE_NOT_FOUND and names the path") {
	register_formats();
	const String path = TestUtils::get_temp_path("load_result_missing.lrtext");
	DirAccess::remove_absolute(path);

	ERR_PRINT_OFF;
	const Ref<ResourceLoadResult> result = loader()->load_result(path, "", CoreBind::ResourceLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;

	REQUIRE(result.is_valid());
	CHECK_FALSE(result->is_ok());
	CHECK(result->get_resource().is_null());
	CHECK(result->get_error() == ERR_FILE_NOT_FOUND);
	CHECK(result->get_error_message().contains("load_result_missing.lrtext"));
	CHECK(result->get_path().ends_with("load_result_missing.lrtext"));
	unregister_formats();
}

TEST_CASE("[ResourceLoadResult] A broken file gives the loader's message") {
	register_formats();
	const String path = write_test_file("load_result_broken.lrbroken", "broken");

	ERR_PRINT_OFF;
	const Ref<ResourceLoadResult> result = loader()->load_result(path, "", CoreBind::ResourceLoader::CACHE_MODE_IGNORE);
	ERR_PRINT_ON;

	CHECK_FALSE(result->is_ok());
	CHECK(result->get_error() == ERR_FILE_CORRUPT);
	CHECK(result->get_error_message().contains("Can't load"));
	CHECK(result->get_error_message().contains("lrbroken"));
	CHECK(::ResourceLoader::last_load_error_message.contains("Can't load"));
	unregister_formats();
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceLoadResult] A successful load returns the cached instance of load()") {
	register_formats();
	const String path = write_test_file("load_result_ok.lrtext", "Hello");

	const Ref<ResourceLoadResult> result = loader()->load_result(path);
	REQUIRE(result->is_ok());
	CHECK(result->get_error() == OK);
	CHECK(result->get_error_message().is_empty());
	CHECK(result->get_path().ends_with("load_result_ok.lrtext"));
	REQUIRE(result->get_resource().is_valid());
	CHECK(String(result->get_resource()->get_meta("text")) == "Hello");

	const Ref<Resource> again = ::ResourceLoader::load(path);
	CHECK(again == result->get_resource());
	CHECK(::ResourceLoader::last_load_error_message.is_empty());
	unregister_formats();
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceLoadResult] A resource of another type than the type hint gives ERR_INVALID_DATA") {
	register_formats();
	const String path = write_test_file("load_result_type.lrimage", "not an image");

	const Ref<ResourceLoadResult> result = loader()->load_result(path, "Image", CoreBind::ResourceLoader::CACHE_MODE_IGNORE);
	CHECK_FALSE(result->is_ok());
	CHECK(result->get_resource().is_null());
	CHECK(result->get_error() == ERR_INVALID_DATA);
	CHECK(result->get_error_message().contains("expected Image, got Resource"));
	unregister_formats();
	DirAccess::remove_absolute(path);
}

struct ThreadLoad {
	String path;
	Ref<ResourceLoadResult> result;
};

static void thread_load(void *p_userdata) {
	ThreadLoad *data = static_cast<ThreadLoad *>(p_userdata);
	data->result = CoreBind::ResourceLoader::get_singleton()->load_result(data->path, "", CoreBind::ResourceLoader::CACHE_MODE_IGNORE);
}

TEST_CASE("[ResourceLoadResult] The message is captured on other threads and in threaded loads") {
	register_formats();
	const String path = write_test_file("load_result_thread.lrbroken", "broken");

	ERR_PRINT_OFF;
	ThreadLoad data;
	data.path = path;
	Thread thread;
	thread.start(&thread_load, &data);
	thread.wait_to_finish();

	REQUIRE(data.result.is_valid());
	CHECK_FALSE(data.result->is_ok());
	CHECK(data.result->get_error() == ERR_FILE_CORRUPT);
	CHECK(data.result->get_error_message().contains("Can't load"));

	REQUIRE(::ResourceLoader::load_threaded_request(path, "", false, ResourceFormatLoader::CACHE_MODE_IGNORE) == OK);
	const Ref<ResourceLoadResult> threaded = loader()->load_threaded_get_result(path);
	CHECK_FALSE(threaded->is_ok());
	CHECK(threaded->get_error() == ERR_FILE_CORRUPT);
	CHECK(threaded->get_error_message().contains("Can't load"));

	const Ref<ResourceLoadResult> not_requested = loader()->load_threaded_get_result(path);
	CHECK_FALSE(not_requested->is_ok());
	CHECK(not_requested->get_error() == ERR_INVALID_PARAMETER);
	CHECK(not_requested->get_error_message().contains("No threaded load"));
	ERR_PRINT_ON;

	unregister_formats();
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceSaveResult] A failed save gives the error and the saver's message") {
	register_formats();
	Ref<Resource> resource;
	resource.instantiate();
	resource->set_meta("text", "Hello");

	const String ok_path = TestUtils::get_temp_path("save_result_ok.lrtext");
	const Ref<ResourceSaveResult> saved = saver()->save_result(resource, ok_path, CoreBind::ResourceSaver::FLAG_NONE);
	CHECK(saved->is_ok());
	CHECK(saved->get_error() == OK);
	CHECK(saved->get_path() == ok_path);
	CHECK(saved->get_error_message().is_empty());

	const String bad_path = TestUtils::get_temp_path("no_such_folder_70/save_result.lrtext");
	ERR_PRINT_OFF;
	const Ref<ResourceSaveResult> failed = saver()->save_result(resource, bad_path, CoreBind::ResourceSaver::FLAG_NONE);
	const Ref<ResourceSaveResult> null_resource = saver()->save_result(Ref<Resource>(), ok_path, CoreBind::ResourceSaver::FLAG_NONE);
	ERR_PRINT_ON;

	CHECK_FALSE(failed->is_ok());
	CHECK(failed->get_error() == ERR_CANT_CREATE);
	CHECK(failed->get_error_message().contains("Can't save"));
	CHECK(failed->get_path() == bad_path);
	CHECK_FALSE(null_resource->is_ok());
	CHECK(null_resource->get_error() == ERR_INVALID_PARAMETER);

	unregister_formats();
	DirAccess::remove_absolute(ok_path);
}

} // namespace TestResourceResult
