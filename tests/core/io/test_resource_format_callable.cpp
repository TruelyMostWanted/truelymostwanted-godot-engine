/**************************************************************************/
/*  test_resource_format_callable.cpp                                     */
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

TEST_FORCE_LINK(test_resource_format_callable)

#include "core/core_bind.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_format_callable.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "tests/test_utils.h"

namespace TestResourceFormatCallable {

// A file of the test format holds one line of text, stored as the "text" meta of a plain Resource.
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

static Variant load_upper_text(const String &p_path) {
	Ref<Resource> resource = load_text(p_path);
	if (resource.is_valid()) {
		resource->set_meta("text", String(resource->get_meta("text")).to_upper());
	}
	return resource;
}

static Variant save_text(const Ref<Resource> &p_resource, const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return int(ERR_CANT_CREATE);
	}
	file->store_string(p_resource->get_meta("text", ""));
	return Variant(); // Like a GDScript function without a return value.
}

static Variant load_corrupt(const String &p_path) {
	return int(ERR_FILE_CORRUPT);
}

static Variant load_unrecognized(const String &p_path) {
	return int(ERR_FILE_UNRECOGNIZED);
}

static Variant load_not_a_resource(const String &p_path) {
	return "not a resource";
}

class TextFormatOwner : public Object {
public:
	Variant load(const String &p_path) { return load_text(p_path); }
};

static String write_test_file(const String &p_name, const String &p_text) {
	const String path = TestUtils::get_temp_path(p_name);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(p_text);
	}
	return path;
}

static Ref<Resource> load_uncached(const String &p_path, const String &p_type_hint = "") {
	return ResourceLoader::load(p_path, p_type_hint, ResourceFormatLoader::CACHE_MODE_IGNORE);
}

static bool recognized_for_type(const String &p_type, const String &p_extension) {
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type(p_type, &extensions);
	return extensions.find(p_extension) != nullptr;
}

TEST_CASE("[ResourceFormatCallable] A registered extension saves and loads through ResourceSaver and ResourceLoader") {
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbtext", callable_mp_static(&load_text), "Resource") == OK);
	REQUIRE(ResourceFormatSaverCallable::register_extension("cbtext", callable_mp_static(&save_text), "Resource") == OK);
	CHECK(ResourceFormatLoaderCallable::is_extension_registered("cbtext"));
	CHECK(ResourceFormatSaverCallable::is_extension_registered("cbtext"));

	Ref<Resource> resource;
	resource.instantiate();
	resource->set_meta("text", "Hello callable format");
	const String path = TestUtils::get_temp_path("callable_format.cbtext");
	CHECK(ResourceSaver::save(resource, path) == OK);
	CHECK(FileAccess::get_file_as_string(path) == "Hello callable format");

	CHECK(ResourceLoader::exists(path));
	CHECK(ResourceLoader::get_resource_type(path) == "Resource");
	const Ref<Resource> loaded = load_uncached(path);
	REQUIRE(loaded.is_valid());
	CHECK(String(loaded->get_meta("text")) == "Hello callable format");

	List<String> save_extensions;
	ResourceSaver::get_recognized_extensions(resource, &save_extensions);
	CHECK(save_extensions.find("cbtext") != nullptr);

	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbtext") == OK);
	CHECK(ResourceFormatSaverCallable::unregister_extension("cbtext") == OK);
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceFormatCallable] Unregistering removes the format") {
	const String path = write_test_file("callable_format_removed.cbgone", "gone");
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbgone", callable_mp_static(&load_text), "Resource") == OK);
	REQUIRE(ResourceFormatSaverCallable::register_extension("cbgone", callable_mp_static(&save_text), "Resource") == OK);
	CHECK(load_uncached(path).is_valid());

	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbgone") == OK);
	CHECK(ResourceFormatSaverCallable::unregister_extension("cbgone") == OK);
	CHECK_FALSE(ResourceFormatLoaderCallable::is_extension_registered("cbgone"));
	CHECK_FALSE(ResourceFormatSaverCallable::is_extension_registered("cbgone"));
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbgone") == ERR_DOES_NOT_EXIST);
	CHECK(ResourceFormatSaverCallable::unregister_extension("cbgone") == ERR_DOES_NOT_EXIST);

	ERR_PRINT_OFF;
	CHECK(load_uncached(path).is_null());
	Ref<Resource> resource;
	resource.instantiate();
	CHECK(ResourceSaver::save(resource, path) == ERR_FILE_UNRECOGNIZED);
	ERR_PRINT_ON;
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceFormatCallable] Extensions are case-insensitive, may start with a dot and may contain dots") {
	REQUIRE(ResourceFormatLoaderCallable::register_extension(".CbCase", callable_mp_static(&load_text), "Resource") == OK);
	CHECK(ResourceFormatLoaderCallable::is_extension_registered("cbcase"));
	CHECK(ResourceFormatLoaderCallable::is_extension_registered(".CBCASE"));
	const String path = write_test_file("callable_format_case.CBCASE", "case");
	CHECK(load_uncached(path).is_valid());
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbcase") == OK);
	DirAccess::remove_absolute(path);

	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbquiz.cbdata", callable_mp_static(&load_text), "Resource") == OK);
	CHECK(ResourceLoader::get_resource_type("res://questions.cbquiz.cbdata") == "Resource");
	CHECK(ResourceLoader::get_resource_type("res://questions.cbdata") == "");
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbquiz.cbdata") == OK);
}

TEST_CASE("[ResourceFormatCallable] Invalid registrations are rejected") {
	ERR_PRINT_OFF;
	CHECK(ResourceFormatLoaderCallable::register_extension("", callable_mp_static(&load_text), "Resource") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatLoaderCallable::register_extension(".", callable_mp_static(&load_text), "Resource") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatLoaderCallable::register_extension("cb/bad", callable_mp_static(&load_text), "Resource") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatLoaderCallable::register_extension("cbbad", Callable(), "Resource") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatLoaderCallable::register_extension("cbbad", callable_mp_static(&load_text), "Object") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatLoaderCallable::register_extension("cbbad", callable_mp_static(&load_text), "NoSuchClass") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatSaverCallable::register_extension("cbbad", Callable(), "Resource") == ERR_INVALID_PARAMETER);
	CHECK(ResourceFormatSaverCallable::register_extension("cbbad", callable_mp_static(&save_text), "Object") == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK_FALSE(ResourceFormatLoaderCallable::is_extension_registered("cbbad"));
	CHECK_FALSE(ResourceFormatSaverCallable::is_extension_registered("cbbad"));
}

TEST_CASE("[ResourceFormatCallable] Registering again replaces the callable") {
	const String path = write_test_file("callable_format_replace.cbrepl", "quiet");
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbrepl", callable_mp_static(&load_text), "Resource") == OK);
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbrepl", callable_mp_static(&load_upper_text), "Resource") == OK);
	const Ref<Resource> loaded = load_uncached(path);
	REQUIRE(loaded.is_valid());
	CHECK(String(loaded->get_meta("text")) == "QUIET");

	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbrepl") == OK);
	CHECK_FALSE(ResourceFormatLoaderCallable::is_extension_registered("cbrepl"));
	ERR_PRINT_OFF;
	CHECK(load_uncached(path).is_null()); // The replaced loader was removed as well.
	ERR_PRINT_ON;
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceFormatCallable] The type limits loading with a type hint and saving") {
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbjson", callable_mp_static(&load_text), "JSON") == OK);
	CHECK(recognized_for_type("Resource", "cbjson"));
	CHECK(recognized_for_type("JSON", "cbjson"));
	CHECK(ResourceLoader::get_resource_type("res://data.cbjson") == "JSON");
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbjson") == OK);

	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbany", callable_mp_static(&load_text), "Resource") == OK);
	CHECK(recognized_for_type("Resource", "cbany"));
	CHECK_FALSE(recognized_for_type("JSON", "cbany"));
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbany") == OK);

	REQUIRE(ResourceFormatSaverCallable::register_extension("cbjson", callable_mp_static(&save_text), "JSON") == OK);
	Ref<Resource> plain;
	plain.instantiate();
	Ref<JSON> json;
	json.instantiate();
	const String path = TestUtils::get_temp_path("callable_format_type.cbjson");
	CHECK(ResourceSaver::save(plain, path) == ERR_FILE_UNRECOGNIZED);
	CHECK(ResourceSaver::save(Ref<Resource>(json), path) == OK);
	CHECK(ResourceFormatSaverCallable::unregister_extension("cbjson") == OK);
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceFormatCallable] Errors returned by the callable") {
	const String path = write_test_file("callable_format_errors.cberr", "x");
	Error err = OK;

	REQUIRE(ResourceFormatLoaderCallable::register_extension("cberr", callable_mp_static(&load_corrupt), "Resource") == OK);
	ERR_PRINT_OFF;
	CHECK(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err).is_null());
	ERR_PRINT_ON;
	CHECK(err != OK);

	REQUIRE(ResourceFormatLoaderCallable::register_extension("cberr", callable_mp_static(&load_not_a_resource), "Resource") == OK);
	ERR_PRINT_OFF;
	CHECK(load_uncached(path).is_null());
	ERR_PRINT_ON;

	// ERR_FILE_UNRECOGNIZED hands the file to the next loader: in front of the text loader, the text loader takes it.
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cberr", callable_mp_static(&load_unrecognized), "Resource", true) == OK);
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cberr.cberr", callable_mp_static(&load_text), "Resource") == OK);
	const String double_path = write_test_file("callable_format_errors.cberr.cberr", "second");
	const Ref<Resource> second = load_uncached(double_path);
	REQUIRE(second.is_valid());
	CHECK(String(second->get_meta("text")) == "second");

	CHECK(ResourceFormatLoaderCallable::unregister_extension("cberr") == OK);
	CHECK(ResourceFormatLoaderCallable::unregister_extension("cberr.cberr") == OK);
	DirAccess::remove_absolute(path);
	DirAccess::remove_absolute(double_path);
}

TEST_CASE("[ResourceFormatCallable] A callable bound to a freed object fails cleanly") {
	const String path = write_test_file("callable_format_freed.cbfreed", "freed");
	TextFormatOwner *owner = memnew(TextFormatOwner);
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cbfreed", callable_mp(owner, &TextFormatOwner::load), "Resource") == OK);
	CHECK(load_uncached(path).is_valid());

	memdelete(owner);
	Error err = OK;
	ERR_PRINT_OFF;
	CHECK(ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err).is_null());
	ERR_PRINT_ON;
	CHECK(err != OK);

	CHECK(ResourceFormatLoaderCallable::unregister_extension("cbfreed") == OK);
	DirAccess::remove_absolute(path);
}

TEST_CASE("[ResourceFormatCallable] unregister_all() removes every registration") {
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cball1", callable_mp_static(&load_text), "Resource") == OK);
	REQUIRE(ResourceFormatLoaderCallable::register_extension("cball2", callable_mp_static(&load_text), "Resource") == OK);
	REQUIRE(ResourceFormatSaverCallable::register_extension("cball1", callable_mp_static(&save_text), "Resource") == OK);
	ResourceFormatLoaderCallable::unregister_all();
	ResourceFormatSaverCallable::unregister_all();
	CHECK_FALSE(ResourceFormatLoaderCallable::is_extension_registered("cball1"));
	CHECK_FALSE(ResourceFormatLoaderCallable::is_extension_registered("cball2"));
	CHECK_FALSE(ResourceFormatSaverCallable::is_extension_registered("cball1"));
	CHECK(ResourceLoader::get_resource_type("res://a.cball1") == "");
}

TEST_CASE("[ResourceFormatCallable] Scripting API on the ResourceLoader and ResourceSaver singletons") {
	CoreBind::ResourceLoader *loader = CoreBind::ResourceLoader::get_singleton();
	CoreBind::ResourceSaver *saver = CoreBind::ResourceSaver::get_singleton();
	REQUIRE(loader != nullptr);
	REQUIRE(saver != nullptr);

	CHECK(loader->register_extension_loader("cbbind", callable_mp_static(&load_text)) == OK);
	CHECK(saver->register_extension_saver("cbbind", callable_mp_static(&save_text)) == OK);
	CHECK(loader->has_extension_loader("cbbind"));
	CHECK(saver->has_extension_saver("cbbind"));

	Ref<Resource> resource;
	resource.instantiate();
	resource->set_meta("text", "bound");
	const String path = TestUtils::get_temp_path("callable_format_bind.cbbind");
	CHECK(saver->save(resource, path, CoreBind::ResourceSaver::FLAG_NONE) == OK);
	const Ref<Resource> loaded = loader->load(path, "", CoreBind::ResourceLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(String(loaded->get_meta("text")) == "bound");

	CHECK(loader->unregister_extension_loader("cbbind") == OK);
	CHECK(saver->unregister_extension_saver("cbbind") == OK);
	CHECK_FALSE(loader->has_extension_loader("cbbind"));
	CHECK_FALSE(saver->has_extension_saver("cbbind"));
	DirAccess::remove_absolute(path);
}

} // namespace TestResourceFormatCallable
