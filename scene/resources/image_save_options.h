/**************************************************************************/
/*  image_save_options.h                                                  */
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

#include "core/io/resource.h"

// Encoder options for `ResourceSaver.save()` when an `ImageTexture` or `Image` is written to an image file.
// Attached to the saved resource as metadata, so that no global state is shared between threads.
class ImageSaveOptions : public Resource {
	GDCLASS(ImageSaveOptions, Resource);

	float jpg_quality = 0.75f;
	bool webp_lossy = false;
	float webp_quality = 0.75f;
	bool exr_grayscale = false;

protected:
	static void _bind_methods();

public:
	// Metadata key under which `attach_to()` stores the options on a resource.
	static const StringName &get_meta_name();

	void set_jpg_quality(float p_quality);
	float get_jpg_quality() const;

	void set_webp_lossy(bool p_lossy);
	bool is_webp_lossy() const;

	void set_webp_quality(float p_quality);
	float get_webp_quality() const;

	void set_exr_grayscale(bool p_grayscale);
	bool is_exr_grayscale() const;

	void attach_to(const Ref<Resource> &p_resource);
	static Ref<ImageSaveOptions> get_attached(const Ref<Resource> &p_resource);
	static void detach_from(const Ref<Resource> &p_resource);
};
