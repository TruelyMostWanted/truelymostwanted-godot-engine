/**************************************************************************/
/*  resource_format_gltf.cpp                                              */
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

#include "resource_format_gltf.h"

#include "extensions/gltf_document_extension_convert_importer_mesh.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

// Loader.

void ResourceFormatLoaderGLTF::define_project_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, "filesystem/scene_formats/embedded_image_handling", PROPERTY_HINT_ENUM, "Discard All Textures,Extract Textures,Embed as Basis Universal,Embed as Uncompressed"), int(GLTFState::HANDLE_BINARY_IMAGE_MODE_EMBED_AS_UNCOMPRESSED));
	GLOBAL_DEF("filesystem/scene_formats/import_animations", true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "filesystem/scene_formats/animation_fps", PROPERTY_HINT_RANGE, "1,120,1"), 30.0);
	GLOBAL_DEF("filesystem/scene_formats/animation_trimming", false);
	GLOBAL_DEF("filesystem/scene_formats/remove_immutable_tracks", true);
}

bool ResourceFormatLoaderGLTF::is_handled_by_importer(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		// In the editor, a `res://` file without `.import` is about to be imported (or is read by
		// another importer as a dependency): leave it to the import system.
		return p_path.begins_with("res://") && Engine::get_singleton()->is_editor_hint();
	}
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(import_path) != OK) {
		return true; // Leave broken `.import` files to ResourceFormatImporter, which reports them.
	}
	// "keep" files are exported as-is without their `.import` file; "skip" files are not imported at all.
	const String importer = config->get_value("remap", "importer", String());
	return importer != "keep" && importer != "skip";
}

void ResourceFormatLoaderGLTF::_get_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gltf");
	p_extensions->push_back("glb");
}

Ref<GLTFDocument> ResourceFormatLoaderGLTF::_create_document() const {
	Ref<GLTFDocument> document;
	document.instantiate();
	return document;
}

Ref<GLTFState> ResourceFormatLoaderGLTF::_create_state() const {
	Ref<GLTFState> state;
	state.instantiate();
	return state;
}

bool ResourceFormatLoaderGLTF::_has_extension(const String &p_path) const {
	const String extension = p_path.get_extension();
	List<String> extensions;
	_get_extensions(&extensions);
	for (const String &E : extensions) {
		if (extension.nocasecmp_to(E) == 0) {
			return true;
		}
	}
	return false;
}

Ref<Resource> ResourceFormatLoaderGLTF::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Ref<GLTFDocument> document = _create_document();
	Ref<GLTFState> state = _create_state();
	ERR_FAIL_COND_V(document.is_null() || state.is_null(), Ref<Resource>());

	state->set_handle_binary_image_mode(GLTFState::HandleBinaryImageMode(int(GLOBAL_GET("filesystem/scene_formats/embedded_image_handling"))));
	state->set_create_animations(GLOBAL_GET("filesystem/scene_formats/import_animations"));
	const float bake_fps = GLOBAL_GET("filesystem/scene_formats/animation_fps");
	const bool trimming = GLOBAL_GET("filesystem/scene_formats/animation_trimming");
	const bool remove_immutable_tracks = GLOBAL_GET("filesystem/scene_formats/remove_immutable_tracks");

	Error err = document->append_from_file(p_path, state, _get_import_flags(), p_path.get_base_dir());
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot read scene file '%s': %s.", p_path, error_names[err]));
	}
	if (r_progress) {
		*r_progress = 0.5f;
	}

	Node *root = document->generate_scene(state, bake_fps, trimming, remove_immutable_tracks);
	if (root == nullptr) {
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot generate a scene from '%s'.", p_path));
	}
	if (Engine::get_singleton()->is_editor_hint()) {
		// GLTFDocumentExtensionConvertImporterMesh only runs in games; give `@tool` scripts the same MeshInstance3D nodes.
		root = GLTFDocumentExtensionConvertImporterMesh::convert_importer_meshes_recursively(root);
	}

	Ref<PackedScene> scene;
	scene.instantiate();
	err = scene->pack(root);
	memdelete(root);
	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot pack the scene generated from '%s': %s.", p_path, error_names[err]));
	}

	if (r_progress) {
		*r_progress = 1.0f;
	}
	if (r_error) {
		*r_error = OK;
	}
	return scene;
}

void ResourceFormatLoaderGLTF::get_recognized_extensions(List<String> *p_extensions) const {
	// Always the full list: the extension index of ResourceLoader is built from it.
	_get_extensions(p_extensions);
}

void ResourceFormatLoaderGLTF::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (!p_type.is_empty() && Engine::get_singleton()->is_editor_hint()) {
		// The scene importer already lists these extensions in editor file dialogs.
		return;
	}
	if (p_type.is_empty() || handles_type(p_type)) {
		_get_extensions(p_extensions);
	}
}

bool ResourceFormatLoaderGLTF::recognize_path(const String &p_path, const String &p_for_type) const {
	if (!_has_extension(p_path)) {
		return false;
	}
	if (!p_for_type.is_empty() && !handles_type(p_for_type)) {
		return false;
	}
	return !is_handled_by_importer(p_path);
}

bool ResourceFormatLoaderGLTF::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("PackedScene", p_type);
}

String ResourceFormatLoaderGLTF::get_resource_type(const String &p_path) const {
	return _has_extension(p_path) ? String("PackedScene") : String();
}

// Saver.

Error ResourceFormatSaverGLTF::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<PackedScene> scene = p_resource;
	ERR_FAIL_COND_V_MSG(scene.is_null(), ERR_INVALID_PARAMETER, vformat("Only a PackedScene can be saved as glTF: '%s'.", p_path));

	Node *root = scene->instantiate();
	ERR_FAIL_NULL_V_MSG(root, ERR_CANT_CREATE, vformat("Cannot instantiate the scene to save as '%s'.", p_path));

	Ref<GLTFDocument> document;
	document.instantiate();
	Ref<GLTFState> state;
	state.instantiate();
	Error err = document->append_from_scene(root, state);
	if (err == OK) {
		err = document->write_to_filesystem(state, p_path);
	}
	memdelete(root);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot save the scene as '%s': %s.", p_path, error_names[err]));
	return OK;
}

bool ResourceFormatSaverGLTF::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<PackedScene>(p_resource.ptr()) != nullptr;
}

void ResourceFormatSaverGLTF::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Engine::get_singleton()->is_editor_hint()) {
		// Keep the editor's "Save Scene As" dialog at .tscn/.scn; use Scene > Export As > glTF there.
		return;
	}
	if (recognize(p_resource)) {
		p_extensions->push_back("gltf");
		p_extensions->push_back("glb");
	}
}

bool ResourceFormatSaverGLTF::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	const String extension = p_path.get_extension();
	return extension.nocasecmp_to("gltf") == 0 || extension.nocasecmp_to("glb") == 0;
}
