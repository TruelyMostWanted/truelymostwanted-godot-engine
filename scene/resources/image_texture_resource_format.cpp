/**************************************************************************/
/*  image_texture_resource_format.cpp                                     */
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

#include "image_texture_resource_format.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "core/object/class_db.h"
#include "scene/resources/image_save_options.h"
#include "scene/resources/image_texture.h"

namespace ImageTextureResourceFormat {

static const char *const decoded_extensions[] = { "png", "jpg", "jpeg", "webp", "bmp", "tga", "svg", "exr", "hdr" };
static const char *const container_extensions[] = { "ktx", "ktx2", "dds" };
static const char *const saved_extensions[] = { "png", "jpg", "jpeg", "webp", "exr", "dds" };

template <size_t N>
static bool _is_in(const char *const (&p_list)[N], const String &p_extension) {
	for (const char *E : p_list) {
		if (p_extension == E) {
			return true;
		}
	}
	return false;
}

bool is_decoded_extension(const String &p_extension) {
	return _is_in(decoded_extensions, p_extension);
}

bool is_container_extension(const String &p_extension) {
	return _is_in(container_extensions, p_extension);
}

bool is_decoder_available(const String &p_extension) {
	if (p_extension == "ktx" || p_extension == "ktx2") {
		return Image::_ktx_mem_loader_func != nullptr;
	}
	if (p_extension == "dds") {
		return Image::_dds_mem_loader_func != nullptr;
	}
	return is_decoded_extension(p_extension) && ImageLoader::recognize(p_extension).is_valid();
}

bool is_saved_extension(const String &p_extension) {
	return _is_in(saved_extensions, p_extension);
}

bool is_encoder_available(const String &p_extension) {
	if (p_extension == "png") {
		return Image::save_png_func != nullptr;
	}
	if (p_extension == "jpg" || p_extension == "jpeg") {
		return Image::save_jpg_func != nullptr;
	}
	if (p_extension == "webp") {
		return Image::save_webp_func != nullptr;
	}
	if (p_extension == "exr") {
		return Image::save_exr_func != nullptr;
	}
	if (p_extension == "dds") {
		return Image::save_dds_func != nullptr;
	}
	return false;
}

bool is_handled_by_importer(const String &p_path) {
	const String import_path = p_path + ".import";
	if (!FileAccess::exists(import_path)) {
		return false;
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

Ref<Image> load_image(const String &p_path, Error *r_error) {
	const String extension = p_path.get_extension().to_lower();
	Ref<Image> image;
	image.instantiate();

	Error err = OK;
	if (is_container_extension(extension)) {
		const Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path, &err);
		if (err == OK) {
			err = extension == "dds" ? image->load_dds_from_buffer(data) : image->load_ktx_from_buffer(data);
		}
	} else {
		err = ImageLoader::load_image(p_path, image);
	}
	if (err == OK && image->is_empty()) {
		err = ERR_FILE_CORRUPT;
	}

	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Image>();
	}
	return image;
}

Error save_image(const Ref<Image> &p_image, const String &p_path, const Ref<ImageSaveOptions> &p_options) {
	ERR_FAIL_COND_V(p_image.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_image->is_empty(), ERR_INVALID_DATA, vformat("Cannot save an empty image to '%s'.", p_path));

	const String extension = p_path.get_extension().to_lower();
	ERR_FAIL_COND_V_MSG(!is_saved_extension(extension), ERR_FILE_UNRECOGNIZED, vformat("Cannot save an image as '.%s' (supported: png, jpg, jpeg, webp, exr, dds): '%s'.", extension, p_path));

	Ref<Image> image = p_image;
	if (image->is_compressed() && extension != "dds") {
		// The encoders except DDS need uncompressed pixels. Decompress a copy; the resource stays unchanged.
		image = Image::create_from_data(p_image->get_width(), p_image->get_height(), p_image->has_mipmaps(), p_image->get_format(), p_image->get_data());
		ERR_FAIL_COND_V(image.is_null(), ERR_INVALID_DATA);
		const Error err = image->decompress();
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot decompress the image to save it to '%s'.", p_path));
	}

	Ref<ImageSaveOptions> options = p_options;
	if (options.is_null()) {
		options.instantiate();
	}

	if (extension == "png") {
		return image->save_png(p_path);
	}
	if (extension == "jpg" || extension == "jpeg") {
		return image->save_jpg(p_path, options->get_jpg_quality());
	}
	if (extension == "webp") {
		return image->save_webp(p_path, options->is_webp_lossy(), options->get_webp_quality());
	}
	if (extension == "exr") {
		// The EXR encoder accepts grayscale only for single-channel images.
		const Image::Format format = image->get_format();
		const bool single_channel = format == Image::FORMAT_R8 || format == Image::FORMAT_RH || format == Image::FORMAT_RF;
		return image->save_exr(p_path, options->is_exr_grayscale() && single_channel);
	}
	return image->save_dds(p_path);
}

// The type hint is only passed to `recognize_path()`. `ResourceLoader` calls `load()` right after it on the same
// thread with the same path, so the hint is handed over per thread.
struct PendingLoad {
	String path;
	bool as_image = false;
};

static thread_local PendingLoad pending_load;

} // namespace ImageTextureResourceFormat

using namespace ImageTextureResourceFormat;

// ResourceFormatLoaderImageTexture

Ref<Resource> ResourceFormatLoaderImageTexture::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	const bool as_image = pending_load.path == p_path && pending_load.as_image;
	pending_load.path = String();
	pending_load.as_image = false;

	Error err = OK;
	Ref<Image> image = load_image(p_path, &err);
	if (r_error) {
		*r_error = err;
	}
	ERR_FAIL_COND_V_MSG(image.is_null(), Ref<Resource>(), vformat("Cannot load image file '%s': %s.", p_path, error_names[err]));

	if (r_progress) {
		*r_progress = 1.0f;
	}
	if (as_image) {
		return image;
	}
	return ImageTexture::create_from_image(image);
}

void ResourceFormatLoaderImageTexture::get_recognized_extensions(List<String> *p_extensions) const {
	for (const char *E : decoded_extensions) {
		p_extensions->push_back(E);
	}
	for (const char *E : container_extensions) {
		p_extensions->push_back(E);
	}
}

void ResourceFormatLoaderImageTexture::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (!p_type.is_empty() && Engine::get_singleton()->is_editor_hint()) {
		// The texture and image importers already list these extensions in editor file dialogs.
		return;
	}
	if (!p_type.is_empty() && !handles_type(p_type)) {
		return;
	}
	for (const char *E : decoded_extensions) {
		p_extensions->push_back(E);
	}
	if (p_type == "Image") {
		for (const char *E : container_extensions) {
			p_extensions->push_back(E);
		}
	}
}

bool ResourceFormatLoaderImageTexture::recognize_path(const String &p_path, const String &p_for_type) const {
	const String extension = p_path.get_extension().to_lower();
	const bool as_image = p_for_type == "Image";
	if (!p_for_type.is_empty() && !handles_type(p_for_type)) {
		return false;
	}
	if (is_container_extension(extension)) {
		if (!as_image) {
			return false; // ResourceFormatKTX and ResourceFormatDDS load these as textures.
		}
	} else if (!is_decoded_extension(extension)) {
		return false;
	}
	if (!is_decoder_available(extension) || is_handled_by_importer(p_path)) {
		return false;
	}
	pending_load.path = p_path;
	pending_load.as_image = as_image;
	return true;
}

bool ResourceFormatLoaderImageTexture::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("ImageTexture", p_type) || ClassDB::is_parent_class("Image", p_type);
}

String ResourceFormatLoaderImageTexture::get_resource_type(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (!is_decoded_extension(extension) || !is_decoder_available(extension) || is_handled_by_importer(p_path)) {
		return String();
	}
	return "ImageTexture";
}

// ResourceFormatSaverImageTexture

Error ResourceFormatSaverImageTexture::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<Image> image = p_resource;
	if (image.is_null()) {
		const Ref<ImageTexture> texture = p_resource;
		ERR_FAIL_COND_V_MSG(texture.is_null(), ERR_INVALID_PARAMETER, vformat("Only an ImageTexture or an Image can be saved as an image file: '%s'.", p_path));
		ERR_FAIL_COND_V_MSG(texture->get_width() == 0, ERR_INVALID_DATA, vformat("Cannot save an empty ImageTexture to '%s'.", p_path));
		image = texture->get_image();
		ERR_FAIL_COND_V_MSG(image.is_null(), ERR_INVALID_DATA, vformat("Cannot read the image of the ImageTexture to save it to '%s'.", p_path));
	}
	return save_image(image, p_path, ImageSaveOptions::get_attached(p_resource));
}

void ResourceFormatSaverImageTexture::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (!recognize(p_resource)) {
		return;
	}
	for (const char *E : saved_extensions) {
		if (is_encoder_available(E)) {
			p_extensions->push_back(E);
		}
	}
}

bool ResourceFormatSaverImageTexture::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<ImageTexture>(*p_resource) != nullptr || Object::cast_to<Image>(*p_resource) != nullptr;
}

bool ResourceFormatSaverImageTexture::recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	return is_saved_extension(extension) && is_encoder_available(extension);
}
