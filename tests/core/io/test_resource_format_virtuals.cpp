/**************************************************************************/
/*  test_resource_format_virtuals.cpp                                     */
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

TEST_FORCE_LINK(test_resource_format_virtuals)

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "tests/test_utils.h"

namespace TestResourceFormatVirtuals {

TEST_CASE("[ResourceLoader] resource_path and the cache per cache mode") {
	const String path = TestUtils::get_temp_path("cache_mode_path.tres");
	const String local_path = ProjectSettings::get_singleton()->localize_path(path);
	Ref<Resource> source;
	source.instantiate();
	source->set_name("first");
	REQUIRE(ResourceSaver::save(source, path) == OK);
	CHECK_MESSAGE(source->get_path().is_empty(), "Saving without FLAG_CHANGE_PATH does not set the path.");

	// REUSE: the loaded resource gets the path and is cached; the next load returns the same instance.
	const Ref<Resource> reused = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_REUSE);
	REQUIRE(reused.is_valid());
	CHECK(reused->get_path() == local_path);
	CHECK(ResourceCache::get_ref(local_path) == reused);
	CHECK(ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_REUSE) == reused);

	// IGNORE: a new instance that reports the path but is not cached; the cached instance stays.
	const Ref<Resource> ignored = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_IGNORE);
	REQUIRE(ignored.is_valid());
	CHECK(ignored != reused);
	CHECK(ignored->get_path() == local_path);
	CHECK(ResourceCache::get_ref(local_path) == reused);

	// REPLACE: the cached instance is updated in place from the file and returned.
	source->set_name("second");
	REQUIRE(ResourceSaver::save(source, path) == OK);
	const Ref<Resource> replaced = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_REPLACE);
	CHECK(replaced == reused);
	CHECK(reused->get_name() == "second");
	CHECK(reused->get_path() == local_path);
}

TEST_CASE("[ResourceSaver] FLAG_CHANGE_PATH and taking over paths") {
	const String path = TestUtils::get_temp_path("take_over_path.tres");
	const String local_path = ProjectSettings::get_singleton()->localize_path(path);
	const String other_local_path = ProjectSettings::get_singleton()->localize_path(TestUtils::get_temp_path("take_over_other.tres"));

	// FLAG_CHANGE_PATH uses the target path only while saving; after a successful save the previous path is back.
	Ref<Resource> saved;
	saved.instantiate();
	REQUIRE(ResourceSaver::save(saved, path, ResourceSaver::FLAG_CHANGE_PATH) == OK);
	CHECK(saved->get_path().is_empty());

	Ref<Resource> holder;
	holder.instantiate();
	holder->set_path(local_path);
	REQUIRE(ResourceCache::get_ref(local_path) == holder);

	// Saving another resource to a path that a loaded resource holds does not take the path over.
	ERR_PRINT_OFF;
	CHECK(ResourceSaver::save(saved, path, ResourceSaver::FLAG_CHANGE_PATH) == OK);
	ERR_PRINT_ON;
	CHECK(saved->get_path().is_empty());
	CHECK(holder->get_path() == local_path);

	// Without take over, setting a held path fails and leaves the resource without a path.
	Ref<Resource> other;
	other.instantiate();
	other->set_path(other_local_path);
	ERR_PRINT_OFF;
	other->set_path(local_path);
	ERR_PRINT_ON;
	CHECK(other->get_path().is_empty());
	CHECK(holder->get_path() == local_path);

	// With take over, the path moves to the new resource and the previous holder loses it.
	other->set_path(local_path, true);
	CHECK(other->get_path() == local_path);
	CHECK(holder->get_path().is_empty());
	CHECK(ResourceCache::get_ref(local_path) == other);
}

} // namespace TestResourceFormatVirtuals
