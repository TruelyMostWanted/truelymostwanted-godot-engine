/**************************************************************************/
/*  screenshot_server.h                                                   */
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

#include "core/io/image.h"
#include "core/object/object.h"

class Viewport;

// Screenshots of viewports and screens, saved to the folder and with the file name pattern of the project
// settings `display/screenshots/*`.
class ScreenshotServer : public Object {
	GDCLASS(ScreenshotServer, Object);

	static ScreenshotServer *singleton;

protected:
	static void _bind_methods();

public:
	static ScreenshotServer *get_singleton();

	Ref<Image> capture_viewport(Viewport *p_viewport = nullptr) const;
	Ref<Image> capture_screen(int p_screen) const;
	String save_screenshot(const Ref<Image> &p_image, const String &p_path = String());
	String take_screenshot(Viewport *p_viewport = nullptr);
	String take_screen_screenshot(int p_screen);
	String make_screenshot_path() const;

	// The file name pattern with the placeholders replaced by `p_datetime` (keys of
	// Time.get_datetime_dict_from_system()) and `p_millisecond`; characters that are invalid in file names become `-`.
	static String format_file_name(const String &p_pattern, const Dictionary &p_datetime, int p_millisecond, int64_t p_unix_time, const String &p_project);
	// `p_path`, or `p_path` with " (2)", " (3)", … before the extension when the file exists.
	static String make_unique_path(const String &p_path);

	ScreenshotServer();
	~ScreenshotServer();
};
