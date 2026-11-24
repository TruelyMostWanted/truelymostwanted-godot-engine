/**************************************************************************/
/*  image_save_options.cpp                                                */
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

#include "image_save_options.h"

#include "core/object/class_db.h"

const StringName &ImageSaveOptions::get_meta_name() {
	return SNAME("image_save_options");
}

void ImageSaveOptions::set_jpg_quality(float p_quality) {
	ERR_FAIL_COND_MSG(p_quality < 0.0f || p_quality > 1.0f, vformat("JPEG quality must be between 0.0 and 1.0, got %f.", p_quality));
	jpg_quality = p_quality;
	emit_changed();
}

float ImageSaveOptions::get_jpg_quality() const {
	return jpg_quality;
}

void ImageSaveOptions::set_webp_lossy(bool p_lossy) {
	webp_lossy = p_lossy;
	emit_changed();
}

bool ImageSaveOptions::is_webp_lossy() const {
	return webp_lossy;
}

void ImageSaveOptions::set_webp_quality(float p_quality) {
	ERR_FAIL_COND_MSG(p_quality < 0.0f || p_quality > 1.0f, vformat("WebP quality must be between 0.0 and 1.0, got %f.", p_quality));
	webp_quality = p_quality;
	emit_changed();
}

float ImageSaveOptions::get_webp_quality() const {
	return webp_quality;
}

void ImageSaveOptions::set_exr_grayscale(bool p_grayscale) {
	exr_grayscale = p_grayscale;
	emit_changed();
}

bool ImageSaveOptions::is_exr_grayscale() const {
	return exr_grayscale;
}

void ImageSaveOptions::attach_to(const Ref<Resource> &p_resource) {
	ERR_FAIL_COND_MSG(p_resource.is_null(), "Cannot attach image save options to a null resource.");
	p_resource->set_meta(get_meta_name(), Ref<ImageSaveOptions>(this));
}

Ref<ImageSaveOptions> ImageSaveOptions::get_attached(const Ref<Resource> &p_resource) {
	if (p_resource.is_null() || !p_resource->has_meta(get_meta_name())) {
		return Ref<ImageSaveOptions>();
	}
	return Ref<ImageSaveOptions>(p_resource->get_meta(get_meta_name()));
}

void ImageSaveOptions::detach_from(const Ref<Resource> &p_resource) {
	if (p_resource.is_valid() && p_resource->has_meta(get_meta_name())) {
		p_resource->remove_meta(get_meta_name());
	}
}

void ImageSaveOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_jpg_quality", "quality"), &ImageSaveOptions::set_jpg_quality);
	ClassDB::bind_method(D_METHOD("get_jpg_quality"), &ImageSaveOptions::get_jpg_quality);
	ClassDB::bind_method(D_METHOD("set_webp_lossy", "lossy"), &ImageSaveOptions::set_webp_lossy);
	ClassDB::bind_method(D_METHOD("is_webp_lossy"), &ImageSaveOptions::is_webp_lossy);
	ClassDB::bind_method(D_METHOD("set_webp_quality", "quality"), &ImageSaveOptions::set_webp_quality);
	ClassDB::bind_method(D_METHOD("get_webp_quality"), &ImageSaveOptions::get_webp_quality);
	ClassDB::bind_method(D_METHOD("set_exr_grayscale", "grayscale"), &ImageSaveOptions::set_exr_grayscale);
	ClassDB::bind_method(D_METHOD("is_exr_grayscale"), &ImageSaveOptions::is_exr_grayscale);

	ClassDB::bind_method(D_METHOD("attach_to", "resource"), &ImageSaveOptions::attach_to);
	ClassDB::bind_static_method("ImageSaveOptions", D_METHOD("get_attached", "resource"), &ImageSaveOptions::get_attached);
	ClassDB::bind_static_method("ImageSaveOptions", D_METHOD("detach_from", "resource"), &ImageSaveOptions::detach_from);

	ADD_GROUP("JPEG", "jpg_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jpg_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_jpg_quality", "get_jpg_quality");
	ADD_GROUP("WebP", "webp_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "webp_lossy"), "set_webp_lossy", "is_webp_lossy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "webp_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_webp_quality", "get_webp_quality");
	ADD_GROUP("EXR", "exr_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "exr_grayscale"), "set_exr_grayscale", "is_exr_grayscale");
}
