/**************************************************************************/
/*  test_resource_format_gltf.h                                           */
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

#pragma once

#include "../resource_format_gltf.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestResourceFormatGLTF {

// A Node3D "Ship" with one MeshInstance3D "Hull" (BoxMesh), packed.
static Ref<PackedScene> make_ship_scene() {
	Node3D *ship = memnew(Node3D);
	ship->set_name("Ship");

	Ref<BoxMesh> box;
	box.instantiate();
	MeshInstance3D *hull = memnew(MeshInstance3D);
	hull->set_name("Hull");
	hull->set_mesh(box);
	hull->set_position(Vector3(0, 1, 0));
	ship->add_child(hull);
	hull->set_owner(ship);

	Ref<PackedScene> scene;
	scene.instantiate();
	const Error err = scene->pack(ship);
	memdelete(ship);
	CHECK(err == OK);
	return scene;
}

static MeshInstance3D *find_hull(Node *p_root) {
	if (p_root->get_name() == StringName("Hull")) {
		return Object::cast_to<MeshInstance3D>(p_root);
	}
	return Object::cast_to<MeshInstance3D>(p_root->find_child("Hull", true, false));
}

static void check_ship(const Ref<PackedScene> &p_scene) {
	REQUIRE(p_scene.is_valid());
	Node *root = p_scene->instantiate();
	REQUIRE(root != nullptr);
	MeshInstance3D *hull = find_hull(root);
	REQUIRE_MESSAGE(hull != nullptr, "The loaded scene contains the MeshInstance3D \"Hull\".");
	REQUIRE(hull->get_mesh().is_valid());
	CHECK(hull->get_mesh()->get_surface_count() == 1);
	CHECK(hull->get_position().is_equal_approx(Vector3(0, 1, 0)));
	memdelete(root);
}

static void write_import_file(const String &p_path, const String &p_importer) {
	Ref<FileAccess> file = FileAccess::open(p_path + ".import", FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(vformat("[remap]\n\nimporter=\"%s\"\ntype=\"PackedScene\"\n", p_importer));
}

TEST_CASE("[SceneTree][ResourceFormatGLTF] Save and load .glb and .gltf through ResourceSaver and ResourceLoader") {
	const String folder = TestUtils::get_temp_path("resource_format_gltf");
	DirAccess::make_dir_recursive_absolute(folder);
	Ref<PackedScene> ship = make_ship_scene();

	SUBCASE("Binary .glb") {
		const String path = folder.path_join("ship.glb");
		CHECK(ResourceSaver::save(ship, path) == OK);
		CHECK(FileAccess::exists(path));
		CHECK(ResourceLoader::exists(path, "PackedScene"));
		CHECK(ResourceLoader::get_resource_type(path) == "PackedScene");

		Error err = FAILED;
		Ref<PackedScene> loaded = ResourceLoader::load(path, "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
		CHECK(err == OK);
		check_ship(loaded);
	}

	SUBCASE("Text .gltf with external buffer") {
		const String path = folder.path_join("ship.gltf");
		CHECK(ResourceSaver::save(ship, path) == OK);
		CHECK(FileAccess::exists(path));
		CHECK_MESSAGE(FileAccess::exists(folder.path_join("ship0.bin")), "The .gltf file has an external .bin buffer (GLTFDocument names it <name>0.bin).");

		Ref<PackedScene> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		check_ship(loaded);
	}

	SUBCASE("Threaded load") {
		const String path = folder.path_join("ship_threaded.glb");
		CHECK(ResourceSaver::save(ship, path) == OK);
		CHECK(ResourceLoader::load_threaded_request(path, "PackedScene", true, ResourceFormatLoader::CACHE_MODE_IGNORE) == OK);
		Ref<PackedScene> loaded = ResourceLoader::load_threaded_get(path);
		check_ship(loaded);
	}
}

TEST_CASE("[SceneTree][ResourceFormatGLTF] Imported files stay with the scene importer") {
	const String folder = TestUtils::get_temp_path("resource_format_gltf_import");
	DirAccess::make_dir_recursive_absolute(folder);
	Ref<ResourceFormatLoaderGLTF> loader;
	loader.instantiate();

	const String path = folder.path_join("ship.glb");
	REQUIRE(ResourceSaver::save(make_ship_scene(), path) == OK);
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	dir->remove(path + ".import");

	CHECK_MESSAGE(loader->recognize_path(path), "Without an .import file the loader takes the path.");
	CHECK(loader->recognize_path(path, "PackedScene"));
	CHECK(loader->recognize_path(path, "Resource"));
	CHECK_FALSE(loader->recognize_path(path, "Texture2D"));
	CHECK_FALSE(loader->recognize_path(folder.path_join("ship.tscn")));
	CHECK(loader->recognize_path(folder.path_join("SHIP.GLB")));
	CHECK(loader->get_resource_type(path) == "PackedScene");

	write_import_file(path, "scene");
	CHECK(ResourceFormatLoaderGLTF::is_handled_by_importer(path));
	CHECK_FALSE_MESSAGE(loader->recognize_path(path), "A file imported as a scene is left to ResourceFormatImporter.");

	write_import_file(path, "keep");
	CHECK_FALSE(ResourceFormatLoaderGLTF::is_handled_by_importer(path));
	CHECK_MESSAGE(loader->recognize_path(path), "A file kept as-is (\"keep\") loads through the glTF loader.");

	write_import_file(path, "skip");
	CHECK(loader->recognize_path(path));

	dir->remove(path + ".import");
}

TEST_CASE("[SceneTree][ResourceFormatGLTF] Saver rejects other resources and extensions") {
	Ref<ResourceFormatSaverGLTF> saver;
	saver.instantiate();
	Ref<PackedScene> ship = make_ship_scene();
	Ref<BoxMesh> box;
	box.instantiate();

	CHECK(saver->recognize(ship));
	CHECK_FALSE(saver->recognize(box));
	CHECK(saver->recognize_path(ship, "user://ship.glb"));
	CHECK(saver->recognize_path(ship, "user://ship.GLTF"));
	CHECK_FALSE(saver->recognize_path(ship, "user://ship.fbx"));

	List<String> extensions;
	saver->get_recognized_extensions(ship, &extensions);
	CHECK(extensions.find("glb") != nullptr);
	CHECK(extensions.find("gltf") != nullptr);
}

TEST_CASE("[ResourceFormatGLTF] Project settings") {
	CHECK(ProjectSettings::get_singleton()->has_setting("filesystem/scene_formats/embedded_image_handling"));
	CHECK(int(GLOBAL_GET("filesystem/scene_formats/embedded_image_handling")) == int(GLTFState::HANDLE_BINARY_IMAGE_MODE_EMBED_AS_UNCOMPRESSED));
	CHECK(bool(GLOBAL_GET("filesystem/scene_formats/import_animations")));
	CHECK(double(GLOBAL_GET("filesystem/scene_formats/animation_fps")) == doctest::Approx(30.0));
	CHECK_FALSE(bool(GLOBAL_GET("filesystem/scene_formats/animation_trimming")));
	CHECK(bool(GLOBAL_GET("filesystem/scene_formats/remove_immutable_tracks")));
}

} // namespace TestResourceFormatGLTF
