/**************************************************************************/
/*  window_settings.h                                                     */
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
#include "core/variant/dictionary.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"

// Data of the `window` settings domain (#20): the main window (mode, size, screen, position, flags,
// V-Sync, content scale, HDR) and the stored states of further windows, keyed by node path.
// WindowSettingsServer pushes the values to DisplayServer and the root window.
class WindowSettings : public Resource {
	GDCLASS(WindowSettings, Resource);

	DisplayServerEnums::WindowMode window_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
	Size2i window_size = Size2i(1152, 648);
	int screen = DisplayServerEnums::SCREEN_PRIMARY;
	bool window_centered = true;
	Point2i window_position = Point2i();
	bool borderless = false;
	bool always_on_top = false;
	bool unresizable = false;
	bool transparent = false;
	DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
	Window::ContentScaleMode content_scale_mode = Window::CONTENT_SCALE_MODE_DISABLED;
	Window::ContentScaleAspect content_scale_aspect = Window::CONTENT_SCALE_ASPECT_IGNORE;
	Window::ContentScaleStretch content_scale_stretch = Window::CONTENT_SCALE_STRETCH_FRACTIONAL;
	float content_scale_factor = 1.0;
	bool hdr_output_requested = false;
	float hdr_reference_luminance = -1.0;
	float hdr_max_luminance = -1.0;
	Dictionary window_states;

protected:
	static void _bind_methods();

public:
	void set_window_mode(DisplayServerEnums::WindowMode p_mode);
	DisplayServerEnums::WindowMode get_window_mode() const;
	void set_window_size(const Size2i &p_size);
	Size2i get_window_size() const;
	void set_screen(int p_screen);
	int get_screen() const;
	void set_window_centered(bool p_centered);
	bool is_window_centered() const;
	void set_window_position(const Point2i &p_position);
	Point2i get_window_position() const;
	void set_borderless(bool p_borderless);
	bool is_borderless() const;
	void set_always_on_top(bool p_always_on_top);
	bool is_always_on_top() const;
	void set_unresizable(bool p_unresizable);
	bool is_unresizable() const;
	void set_transparent(bool p_transparent);
	bool is_transparent() const;
	void set_vsync_mode(DisplayServerEnums::VSyncMode p_mode);
	DisplayServerEnums::VSyncMode get_vsync_mode() const;
	void set_content_scale_mode(Window::ContentScaleMode p_mode);
	Window::ContentScaleMode get_content_scale_mode() const;
	void set_content_scale_aspect(Window::ContentScaleAspect p_aspect);
	Window::ContentScaleAspect get_content_scale_aspect() const;
	void set_content_scale_stretch(Window::ContentScaleStretch p_stretch);
	Window::ContentScaleStretch get_content_scale_stretch() const;
	void set_content_scale_factor(float p_factor);
	float get_content_scale_factor() const;
	void set_hdr_output_requested(bool p_requested);
	bool is_hdr_output_requested() const;
	void set_hdr_reference_luminance(float p_luminance);
	float get_hdr_reference_luminance() const;
	void set_hdr_max_luminance(float p_luminance);
	float get_hdr_max_luminance() const;
	void set_window_states(const Dictionary &p_states);
	Dictionary get_window_states() const;
};
