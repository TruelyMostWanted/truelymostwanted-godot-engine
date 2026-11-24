/**************************************************************************/
/*  image_texture_resource_format.h                                       */
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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

class Image;
class ImageSaveOptions;

// Loads image files that have no import (for example from `user://`, a mod folder or an absolute path) at runtime:
// as `ImageTexture`, or as `Image` with the type hint `Image`. Paths with a `.import` file whose importer is not
// `keep` or `skip` are left to `ResourceFormatImporter`.
// KTX and DDS files keep their own loaders (`ResourceFormatKTX`, `ResourceFormatDDS`); this loader only takes them
// for the type hint `Image`.
class ResourceFormatLoaderImageTexture : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderImageTexture, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	// Always the full list: the extension index of ResourceLoader is built from it.
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const override;
	// Narrows the extension list (importer precedence, type hint, decoder available) and remembers the type hint
	// for the `load()` call that follows on the same thread.
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

// Writes an `ImageTexture` or an `Image` to PNG, JPEG, WebP, OpenEXR or DDS, with the `ImageSaveOptions` attached
// to the resource (or the defaults of `Image.save_*()`).
class ResourceFormatSaverImageTexture : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverImageTexture, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override;
};

namespace ImageTextureResourceFormat {

// Lower-case extensions decoded through `ImageLoader` (png, jpg, jpeg, webp, bmp, tga, svg, exr, hdr).
bool is_decoded_extension(const String &p_extension);
// Lower-case extensions of texture containers with their own loaders (ktx, ktx2, dds).
bool is_container_extension(const String &p_extension);
// True if the decoder for this lower-case extension is part of the build.
bool is_decoder_available(const String &p_extension);
// Lower-case extensions the saver writes (png, jpg, jpeg, webp, exr, dds).
bool is_saved_extension(const String &p_extension);
// True if the encoder for this lower-case extension is part of the build.
bool is_encoder_available(const String &p_extension);
// True if the path has a `.import` file whose importer is not `keep` or `skip`.
bool is_handled_by_importer(const String &p_path);

// Reads the image file at `p_path` (any format above).
Ref<Image> load_image(const String &p_path, Error *r_error = nullptr);
// Encodes `p_image` by the extension of `p_path`. `p_options` may be null (defaults).
Error save_image(const Ref<Image> &p_image, const String &p_path, const Ref<ImageSaveOptions> &p_options);

} // namespace ImageTextureResourceFormat
