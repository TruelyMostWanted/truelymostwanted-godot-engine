/**************************************************************************/
/*  test_screenshot_server.cpp                                            */
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

TEST_FORCE_LINK(test_screenshot_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/main/screenshot_server.h"
#include "tests/test_utils.h"

namespace TestScreenshotServer {

TEST_CASE("[ScreenshotServer] Singleton and settings") {
	CHECK(ScreenshotServer::get_singleton() != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("ScreenshotServer"));
	CHECK(String(GLOBAL_GET("display/screenshots/folder")) == "user://screenshots");
	CHECK(String(GLOBAL_GET("display/screenshots/format")) == "png");
}

TEST_CASE("[ScreenshotServer] File name pattern") {
	Dictionary datetime;
	datetime["year"] = 2026;
	datetime["month"] = 11;
	datetime["day"] = 5;
	datetime["hour"] = 7;
	datetime["minute"] = 3;
	datetime["second"] = 9;
	CHECK(ScreenshotServer::format_file_name("Screenshot {year}-{month}-{day} {hour}-{minute}-{second}", datetime, 42, 1790000000, "Game") == "Screenshot 2026-11-05 07-03-09");
	CHECK(ScreenshotServer::format_file_name("{project}_{unix}_{millisecond}", datetime, 42, 1790000000, "Game") == "Game_1790000000_042");
	// Characters that are invalid in file names become '-'.
	CHECK(ScreenshotServer::format_file_name("{hour}:{minute}:{second} a/b?", datetime, 0, 0, "") == "07-03-09 a-b-");
	CHECK(ScreenshotServer::format_file_name("   ", datetime, 0, 0, "") == "Screenshot");
}

TEST_CASE("[ScreenshotServer] Saving in every format, never overwriting") {
	ScreenshotServer *server = ScreenshotServer::get_singleton();
	Ref<Image> image = Image::create_empty(16, 8, false, Image::FORMAT_RGBA8);
	image->fill(Color(0.2, 0.6, 1.0));

	const String folder = TestUtils::get_temp_path("screenshots_test");
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	for (const char *extension : { "png", "jpg", "webp" }) {
		const String path = folder.path_join(String("shot.") + extension);
		if (FileAccess::exists(path)) {
			dir->remove(path);
		}
		CHECK(server->save_screenshot(image, path) == path);
		CHECK(FileAccess::exists(path));
		Ref<Image> loaded = Image::load_from_file(path);
		REQUIRE(loaded.is_valid());
		CHECK(loaded->get_size() == Vector2i(16, 8));
	}

	ERR_PRINT_OFF;
	CHECK(server->save_screenshot(image, folder.path_join("shot.bmp2")).is_empty());
	CHECK(server->save_screenshot(Ref<Image>(), folder.path_join("shot.png")).is_empty());
	ERR_PRINT_ON;

	// Generated paths come from the settings and are unique.
	const Variant old_folder = GLOBAL_GET("display/screenshots/folder");
	ProjectSettings::get_singleton()->set_setting("display/screenshots/folder", folder.path_join("auto"));
	ProjectSettings::get_singleton()->set_setting("display/screenshots/file_name", "same name");
	const String first = server->save_screenshot(image);
	const String second = server->save_screenshot(image);
	CHECK(first.ends_with("same name.png"));
	CHECK(second.ends_with("same name (2).png"));
	CHECK(FileAccess::exists(first));
	CHECK(FileAccess::exists(second));
	CHECK(ScreenshotServer::make_unique_path(first) == folder.path_join("auto").path_join("same name (3).png"));
	dir->remove(first);
	dir->remove(second);
	ProjectSettings::get_singleton()->set_setting("display/screenshots/folder", old_folder);
	ProjectSettings::get_singleton()->set_setting("display/screenshots/file_name", "Screenshot {year}-{month}-{day} {hour}-{minute}-{second}");
}

} // namespace TestScreenshotServer
