/**************************************************************************/
/*  screenshot_server.cpp                                                 */
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

#include "screenshot_server.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

ScreenshotServer *ScreenshotServer::singleton = nullptr;

static const char *SETTING_FOLDER = "display/screenshots/folder";
static const char *SETTING_FILE_NAME = "display/screenshots/file_name";
static const char *SETTING_FORMAT = "display/screenshots/format";
static const char *SETTING_QUALITY = "display/screenshots/quality";

ScreenshotServer *ScreenshotServer::get_singleton() {
	return singleton;
}

Ref<Image> ScreenshotServer::capture_viewport(Viewport *p_viewport) const {
	Viewport *viewport = p_viewport;
	if (viewport == nullptr) {
		SceneTree *tree = SceneTree::get_singleton();
		ERR_FAIL_NULL_V_MSG(tree, Ref<Image>(), "No viewport given and no SceneTree to take the main window from.");
		viewport = tree->get_root();
	}
	ERR_FAIL_NULL_V(viewport, Ref<Image>());
	Ref<ViewportTexture> texture = viewport->get_texture();
	ERR_FAIL_COND_V(texture.is_null(), Ref<Image>());
	Ref<Image> image = texture->get_image();
	ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), Ref<Image>(), "The viewport has no rendered image (the headless display server renders nothing).");
	return image;
}

Ref<Image> ScreenshotServer::capture_screen(int p_screen) const {
	DisplayServer *display_server = DisplayServer::get_singleton();
	ERR_FAIL_NULL_V(display_server, Ref<Image>());
	Ref<Image> image = display_server->screen_get_image(p_screen);
	ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), Ref<Image>(), "This platform can't capture screens (DisplayServer.screen_get_image() returned nothing).");
	return image;
}

String ScreenshotServer::format_file_name(const String &p_pattern, const Dictionary &p_datetime, int p_millisecond, int64_t p_unix_time, const String &p_project) {
	String name = p_pattern;
	name = name.replace("{year}", String::num_int64(int64_t(p_datetime.get("year", 0))).pad_zeros(4));
	name = name.replace("{month}", String::num_int64(int64_t(p_datetime.get("month", 0))).pad_zeros(2));
	name = name.replace("{day}", String::num_int64(int64_t(p_datetime.get("day", 0))).pad_zeros(2));
	name = name.replace("{hour}", String::num_int64(int64_t(p_datetime.get("hour", 0))).pad_zeros(2));
	name = name.replace("{minute}", String::num_int64(int64_t(p_datetime.get("minute", 0))).pad_zeros(2));
	name = name.replace("{second}", String::num_int64(int64_t(p_datetime.get("second", 0))).pad_zeros(2));
	name = name.replace("{millisecond}", String::num_int64(p_millisecond).pad_zeros(3));
	name = name.replace("{unix}", String::num_int64(p_unix_time));
	name = name.replace("{project}", p_project);
	// Characters that are not allowed in file names on some platforms.
	const char32_t invalid[] = { ':', '/', '\\', '?', '*', '"', '<', '>', '|' };
	for (const char32_t c : invalid) {
		name = name.replace(String::chr(c), "-");
	}
	name = name.strip_edges();
	return name.is_empty() ? String("Screenshot") : name;
}

String ScreenshotServer::make_unique_path(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return p_path;
	}
	const String base = p_path.get_basename();
	const String extension = p_path.get_extension();
	for (int i = 2; i < 100000; i++) {
		const String candidate = vformat("%s (%d).%s", base, i, extension);
		if (!FileAccess::exists(candidate)) {
			return candidate;
		}
	}
	return p_path;
}

String ScreenshotServer::make_screenshot_path() const {
	const String folder = String(GLOBAL_GET(SETTING_FOLDER)).strip_edges();
	const String pattern = GLOBAL_GET(SETTING_FILE_NAME);
	const String format = String(GLOBAL_GET(SETTING_FORMAT)).to_lower();
	Time *time = Time::get_singleton();
	const double unix_time = time->get_unix_time_from_system();
	const int millisecond = int(Math::fmod(unix_time, 1.0) * 1000.0);
	const String project = GLOBAL_GET("application/config/name");
	const String name = format_file_name(pattern, time->get_datetime_dict_from_system(), millisecond, int64_t(unix_time), project);
	return make_unique_path(folder.path_join(name + "." + format));
}

String ScreenshotServer::save_screenshot(const Ref<Image> &p_image, const String &p_path) {
	ERR_FAIL_COND_V_MSG(p_image.is_null() || p_image->is_empty(), String(), "No image to save.");
	const String path = p_path.is_empty() ? make_screenshot_path() : p_path;
	const String extension = path.get_extension().to_lower();
	const float quality = GLOBAL_GET(SETTING_QUALITY);

	const String directory = path.get_base_dir();
	if (!directory.is_empty() && !DirAccess::exists(directory)) {
		DirAccess::make_dir_recursive_absolute(directory);
	}

	Error err = ERR_FILE_UNRECOGNIZED;
	if (extension == "png") {
		err = p_image->save_png(path);
	} else if (extension == "jpg" || extension == "jpeg") {
		err = p_image->save_jpg(path, quality);
	} else if (extension == "webp") {
		err = p_image->save_webp(path, true, quality);
	}
	ERR_FAIL_COND_V_MSG(err != OK, String(), vformat("Cannot save the screenshot '%s' (%s). Use .png, .jpg or .webp.", path, error_names[err]));
	emit_signal(SNAME("screenshot_saved"), path, p_image);
	return path;
}

String ScreenshotServer::take_screenshot(Viewport *p_viewport) {
	const Ref<Image> image = capture_viewport(p_viewport);
	return image.is_valid() ? save_screenshot(image) : String();
}

String ScreenshotServer::take_screen_screenshot(int p_screen) {
	const Ref<Image> image = capture_screen(p_screen);
	return image.is_valid() ? save_screenshot(image) : String();
}

void ScreenshotServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("capture_viewport", "viewport"), &ScreenshotServer::capture_viewport, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("capture_screen", "screen"), &ScreenshotServer::capture_screen, DEFVAL(DisplayServerEnums::SCREEN_WITH_MOUSE_FOCUS));
	ClassDB::bind_method(D_METHOD("save_screenshot", "image", "path"), &ScreenshotServer::save_screenshot, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("take_screenshot", "viewport"), &ScreenshotServer::take_screenshot, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("take_screen_screenshot", "screen"), &ScreenshotServer::take_screen_screenshot, DEFVAL(DisplayServerEnums::SCREEN_WITH_MOUSE_FOCUS));
	ClassDB::bind_method(D_METHOD("make_screenshot_path"), &ScreenshotServer::make_screenshot_path);

	ADD_SIGNAL(MethodInfo("screenshot_saved", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image")));
}

ScreenshotServer::ScreenshotServer() {
	singleton = this;
	GLOBAL_DEF(PropertyInfo(Variant::STRING, SETTING_FOLDER, PROPERTY_HINT_DIR), "user://screenshots");
	GLOBAL_DEF(PropertyInfo(Variant::STRING, SETTING_FILE_NAME), "Screenshot {year}-{month}-{day} {hour}-{minute}-{second}");
	GLOBAL_DEF(PropertyInfo(Variant::STRING, SETTING_FORMAT, PROPERTY_HINT_ENUM, "png,jpg,webp"), "png");
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, SETTING_QUALITY, PROPERTY_HINT_RANGE, "0.01,1,0.01"), 0.9);
}

ScreenshotServer::~ScreenshotServer() {
	singleton = nullptr;
}
