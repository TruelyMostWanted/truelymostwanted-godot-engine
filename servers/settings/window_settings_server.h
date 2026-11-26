/**************************************************************************/
/*  window_settings_server.h                                              */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"
#include "servers/settings/settings_server.h"

// Settings domain `window` (#20): mode, size, screen, position, flags and V-Sync of the main window,
// its content scale and HDR output (WindowSettings), plus the stored states of further windows.
// Applied in APPLY_PHASE_PRE_DISPLAY: Main::setup2() creates the main window with the stored mode, size,
// V-Sync mode and flags (get_boot_window()) and places it on the stored screen before it is shown
// (on_display_server_created()). Content scale and HDR wait for the root window (APPLY_PHASE_SCENE).
// Precedence: command line (session overrides) > player's file > project settings.
class WindowSettingsServer : public SettingsServer {
	GDCLASS(WindowSettingsServer, SettingsServer);

public:
	// The main window values DisplayServer::create() takes. Main::setup2() fills them from the project
	// settings, get_boot_window() replaces those the player changed.
	struct BootWindow {
		DisplayServerEnums::WindowMode mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
		DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
		uint32_t flags = 0;
		Size2i size;
	};

private:
	enum Target {
		TARGET_BOOT, // DisplayServer::create(), later the main window.
		TARGET_PLACEMENT, // Screen and position of the main window, after DisplayServer::create().
		TARGET_SCENE, // Root window (content scale, HDR), from APPLY_PHASE_SCENE on.
		TARGET_NONE, // Stored only (window states).
	};

	static WindowSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed while the target did not exist (before the display server, headless, no root window).
	HashSet<StringName> undelivered;
	bool startup_overrides_done = false;
	bool placing = false;

	static Target _get_target(const StringName &p_name);
	static DisplayServer *_get_display_server();
	static Window *_get_root();
	static bool _is_embedded_in_editor();
	static Variant _get_project_value(const StringName &p_name);
	static int _resolve_screen(int p_screen);
	static Rect2i _fit_into_screen(const Rect2i &p_rect, int p_screen);

	Variant _get_current(const StringName &p_name) const;
	bool _can_deliver(const StringName &p_name) const;
	void _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_undelivered(Target p_target);
	void _push_window_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_window_value(const StringName &p_name);
	void _place_main_window();
	void _connect_tree();

	Dictionary _capture_window_state(Window *p_window) const;
	void _apply_window_state(Window *p_window, const Dictionary &p_state) const;
	String _get_window_key(Window *p_window, const String &p_key) const;

	void _on_node_added(Node *p_node);
	void _on_loaded();
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	// Emits the typed `<value>_changed` signal of an applied value.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;
	// `video_window`: the domain name before #243.
	virtual String _get_legacy_file_name() const override;

public:
	static WindowSettingsServer *get_singleton();

	// The window options passed on the command line (Main::get_command_line_window_options()).
	static Dictionary get_command_line_options();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;

	// Startup (Main::setup2()). Not bound.
	void get_boot_window(BootWindow &r_window);
	void on_display_server_created();

	// Main window.
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

	// Content scale of the root window.
	void set_content_scale_mode(Window::ContentScaleMode p_mode);
	Window::ContentScaleMode get_content_scale_mode() const;
	void set_content_scale_aspect(Window::ContentScaleAspect p_aspect);
	Window::ContentScaleAspect get_content_scale_aspect() const;
	void set_content_scale_stretch(Window::ContentScaleStretch p_stretch);
	Window::ContentScaleStretch get_content_scale_stretch() const;
	void set_content_scale_factor(float p_factor);
	float get_content_scale_factor() const;

	// HDR output of the main window.
	void set_hdr_output_requested(bool p_requested);
	bool is_hdr_output_requested() const;
	void set_hdr_reference_luminance(float p_luminance);
	float get_hdr_reference_luminance() const;
	void set_hdr_max_luminance(float p_luminance);
	float get_hdr_max_luminance() const;

	// Places the main window on the stored screen (or the primary screen if it is missing) and keeps it visible.
	void place_main_window();

	// Further windows, keyed by node path (or a key of the game).
	void save_window_state(Window *p_window, const String &p_key = String());
	bool restore_window_state(Window *p_window, const String &p_key = String());
	bool has_window_state(const String &p_key) const;
	void clear_window_state(const String &p_key);

	WindowSettingsServer();
	~WindowSettingsServer();
};
