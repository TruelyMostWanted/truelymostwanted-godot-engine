/**************************************************************************/
/*  video_window_settings_server.cpp                                      */
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

#include "video_window_settings_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_window_settings.h"

static_assert(int(Window::MODE_EXCLUSIVE_FULLSCREEN) == int(DisplayServerEnums::WINDOW_MODE_EXCLUSIVE_FULLSCREEN), "Window::Mode mirrors DisplayServer::WindowMode.");
static_assert(int(Window::FLAG_RESIZE_DISABLED) == int(DisplayServerEnums::WINDOW_FLAG_RESIZE_DISABLED) && int(Window::FLAG_TRANSPARENT) == int(DisplayServerEnums::WINDOW_FLAG_TRANSPARENT), "Window::Flags mirrors DisplayServer::WindowFlags.");

VideoWindowSettingsServer *VideoWindowSettingsServer::singleton = nullptr;

VideoWindowSettingsServer *VideoWindowSettingsServer::get_singleton() {
	return singleton;
}

static Variant video_window_project_setting(const String &p_key, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_key)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_key);
}

// The window flag a setting controls, or -1.
static int video_window_flag(const StringName &p_name) {
	if (p_name == SNAME("borderless")) {
		return DisplayServerEnums::WINDOW_FLAG_BORDERLESS;
	} else if (p_name == SNAME("always_on_top")) {
		return DisplayServerEnums::WINDOW_FLAG_ALWAYS_ON_TOP;
	} else if (p_name == SNAME("unresizable")) {
		return DisplayServerEnums::WINDOW_FLAG_RESIZE_DISABLED;
	} else if (p_name == SNAME("transparent")) {
		return DisplayServerEnums::WINDOW_FLAG_TRANSPARENT;
	}
	return -1;
}

/////////////////////////////////////////////////////////////////////////////
// Targets.

VideoWindowSettingsServer::Target VideoWindowSettingsServer::_get_target(const StringName &p_name) {
	if (p_name == SNAME("screen") || p_name == SNAME("window_position") || p_name == SNAME("window_centered")) {
		return TARGET_PLACEMENT;
	}
	if (p_name == SNAME("window_states")) {
		return TARGET_NONE;
	}
	const String name = p_name;
	if (name.begins_with("content_scale_") || name.begins_with("hdr_")) {
		return TARGET_SCENE;
	}
	return TARGET_BOOT;
}

DisplayServer *VideoWindowSettingsServer::_get_display_server() {
	// No display server yet (APPLY_PHASE_PRE_DISPLAY), or one without a screen (headless): the values wait.
	DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr || display_server->get_screen_count() <= 0) {
		return nullptr;
	}
	return display_server;
}

Window *VideoWindowSettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

bool VideoWindowSettingsServer::_is_embedded_in_editor() {
	return get_command_line_options().has("embedded");
}

Variant VideoWindowSettingsServer::_get_project_value(const StringName &p_name) {
	if (p_name == SNAME("window_mode")) {
		return int(video_window_project_setting("display/window/size/mode", int(DisplayServerEnums::WINDOW_MODE_WINDOWED)));
	} else if (p_name == SNAME("window_size")) {
		// What Main::setup() passes to DisplayServer::create().
		Vector2i size(int(video_window_project_setting("display/window/size/viewport_width", 1152)), int(video_window_project_setting("display/window/size/viewport_height", 648)));
		const int width_override = video_window_project_setting("display/window/size/window_width_override", 0);
		const int height_override = video_window_project_setting("display/window/size/window_height_override", 0);
		if (width_override > 0) {
			size.x = width_override;
		}
		if (height_override > 0) {
			size.y = height_override;
		}
		return size;
	} else if (p_name == SNAME("screen")) {
		return int(DisplayServerEnums::SCREEN_PRIMARY);
	} else if (p_name == SNAME("window_centered")) {
		return true;
	} else if (p_name == SNAME("window_position")) {
		return Vector2i();
	} else if (p_name == SNAME("borderless")) {
		return bool(video_window_project_setting("display/window/size/borderless", false));
	} else if (p_name == SNAME("always_on_top")) {
		return bool(video_window_project_setting("display/window/size/always_on_top", false));
	} else if (p_name == SNAME("unresizable")) {
		return !bool(video_window_project_setting("display/window/size/resizable", true));
	} else if (p_name == SNAME("transparent")) {
		return bool(video_window_project_setting("display/window/size/transparent", false));
	} else if (p_name == SNAME("vsync_mode")) {
		return int(video_window_project_setting("display/window/vsync/vsync_mode", int(DisplayServerEnums::VSYNC_ENABLED)));
	} else if (p_name == SNAME("content_scale_mode")) {
		// The same mapping as Main::start().
		const String mode = video_window_project_setting("display/window/stretch/mode", "disabled");
		if (mode == "canvas_items") {
			return int(Window::CONTENT_SCALE_MODE_CANVAS_ITEMS);
		} else if (mode == "viewport") {
			return int(Window::CONTENT_SCALE_MODE_VIEWPORT);
		}
		return int(Window::CONTENT_SCALE_MODE_DISABLED);
	} else if (p_name == SNAME("content_scale_aspect")) {
		const String aspect = video_window_project_setting("display/window/stretch/aspect", "keep");
		if (aspect == "keep") {
			return int(Window::CONTENT_SCALE_ASPECT_KEEP);
		} else if (aspect == "keep_width") {
			return int(Window::CONTENT_SCALE_ASPECT_KEEP_WIDTH);
		} else if (aspect == "keep_height") {
			return int(Window::CONTENT_SCALE_ASPECT_KEEP_HEIGHT);
		} else if (aspect == "expand") {
			return int(Window::CONTENT_SCALE_ASPECT_EXPAND);
		}
		return int(Window::CONTENT_SCALE_ASPECT_IGNORE);
	} else if (p_name == SNAME("content_scale_stretch")) {
		const String stretch = video_window_project_setting("display/window/stretch/scale_mode", "fractional");
		return int(stretch == "integer" ? Window::CONTENT_SCALE_STRETCH_INTEGER : Window::CONTENT_SCALE_STRETCH_FRACTIONAL);
	} else if (p_name == SNAME("content_scale_factor")) {
		return float(video_window_project_setting("display/window/stretch/scale", 1.0));
	} else if (p_name == SNAME("hdr_output_requested")) {
		return bool(video_window_project_setting("display/window/hdr/request_hdr_output", false));
	} else if (p_name == SNAME("hdr_reference_luminance") || p_name == SNAME("hdr_max_luminance")) {
		return -1.0; // Automatic: the value of the screen.
	}
	return Dictionary(); // window_states
}

int VideoWindowSettingsServer::_resolve_screen(int p_screen) {
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr) {
		return p_screen;
	}
	if (p_screen == DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) {
		return display_server->window_get_current_screen();
	}
	const int screen_count = display_server->get_screen_count();
	if (p_screen >= 0 && p_screen < screen_count) {
		return p_screen;
	}
	if (p_screen >= screen_count) {
		// A monitor that was unplugged: the normal case, no error.
		print_verbose(vformat("Video window settings: screen %d does not exist (%d screens); the primary screen is used.", p_screen, screen_count));
	}
	return display_server->get_primary_screen();
}

Rect2i VideoWindowSettingsServer::_fit_into_screen(const Rect2i &p_rect, int p_screen) {
	const DisplayServer *display_server = DisplayServer::get_singleton();
	const Rect2i usable = display_server->screen_get_usable_rect(p_screen);
	if (usable.size.x <= 0 || usable.size.y <= 0) {
		return p_rect;
	}
	Rect2i rect = p_rect;
	rect.size = rect.size.min(usable.size).max(Vector2i(1, 1));
	rect.position.x = CLAMP(rect.position.x, usable.position.x, usable.position.x + usable.size.x - rect.size.x);
	rect.position.y = CLAMP(rect.position.y, usable.position.y, usable.position.y + usable.size.y - rect.size.y);
	return rect;
}

/////////////////////////////////////////////////////////////////////////////
// Push and pull.

Variant VideoWindowSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

bool VideoWindowSettingsServer::_can_deliver(const StringName &p_name) const {
	switch (_get_target(p_name)) {
		case TARGET_BOOT: {
			if (p_name == SNAME("vsync_mode")) {
				return _get_display_server() != nullptr;
			}
			return _get_display_server() != nullptr && !_is_embedded_in_editor();
		}
		case TARGET_PLACEMENT: {
			return _get_display_server() != nullptr && !_is_embedded_in_editor();
		}
		case TARGET_SCENE: {
			if (String(p_name).begins_with("hdr_")) {
				return _get_root() != nullptr && _get_display_server() != nullptr;
			}
			return _get_root() != nullptr;
		}
		default: {
			return false;
		}
	}
}

void VideoWindowSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	DisplayServer *display_server = _get_display_server();
	Window *root = _get_root();
	const DisplayServerEnums::WindowID main_window = DisplayServerEnums::MAIN_WINDOW_ID;

	if (p_name == SNAME("window_mode")) {
		const DisplayServerEnums::WindowMode mode = DisplayServerEnums::WindowMode(int(p_value));
		if (root != nullptr) {
			root->set_mode(Window::Mode(mode));
		} else {
			display_server->window_set_mode(mode, main_window);
		}
		if (mode == DisplayServerEnums::WINDOW_MODE_WINDOWED) {
			// Back in windowed mode: the stored size and position.
			_deliver(SNAME("window_size"), _get_current(SNAME("window_size")));
		}
	} else if (p_name == SNAME("window_size")) {
		if (display_server->window_get_mode(main_window) != DisplayServerEnums::WINDOW_MODE_WINDOWED) {
			return; // Used when the window is windowed again.
		}
		const Size2i size = Vector2i(p_value).max(Vector2i(1, 1));
		if (root != nullptr) {
			root->set_size(size);
		} else {
			display_server->window_set_size(size, main_window);
		}
		_place_main_window();
	} else if (_get_target(p_name) == TARGET_PLACEMENT) {
		_place_main_window();
	} else if (video_window_flag(p_name) >= 0) {
		const int flag = video_window_flag(p_name);
		if (flag == DisplayServerEnums::WINDOW_FLAG_TRANSPARENT && !OS::get_singleton()->is_layered_allowed()) {
			return; // Needs `display/window/per_pixel_transparency/allowed` at startup.
		}
		if (root != nullptr) {
			root->set_flag(Window::Flags(flag), bool(p_value));
		} else {
			display_server->window_set_flag(DisplayServerEnums::WindowFlags(flag), bool(p_value), main_window);
		}
	} else if (p_name == SNAME("vsync_mode")) {
		display_server->window_set_vsync_mode(DisplayServerEnums::VSyncMode(int(p_value)), main_window);
	} else if (p_name == SNAME("content_scale_mode")) {
		root->set_content_scale_mode(Window::ContentScaleMode(int(p_value)));
	} else if (p_name == SNAME("content_scale_aspect")) {
		root->set_content_scale_aspect(Window::ContentScaleAspect(int(p_value)));
	} else if (p_name == SNAME("content_scale_stretch")) {
		root->set_content_scale_stretch(Window::ContentScaleStretch(int(p_value)));
	} else if (p_name == SNAME("content_scale_factor")) {
		root->set_content_scale_factor(MAX(real_t(p_value), real_t(0.01)));
	} else if (p_name == SNAME("hdr_output_requested")) {
		root->set_hdr_output_requested(bool(p_value));
	} else if (p_name == SNAME("hdr_reference_luminance")) {
		display_server->window_set_hdr_output_reference_luminance(float(p_value), main_window);
	} else if (p_name == SNAME("hdr_max_luminance")) {
		display_server->window_set_hdr_output_max_luminance(float(p_value), main_window);
	}
}

void VideoWindowSettingsServer::_deliver_undelivered(Target p_target) {
	LocalVector<StringName> names;
	for (const StringName &name : undelivered) {
		if (_get_target(name) == p_target && _can_deliver(name)) {
			names.push_back(name);
		}
	}
	for (const StringName &name : names) {
		undelivered.erase(name);
		_deliver(name, _get_current(name));
	}
}

void VideoWindowSettingsServer::_push_window_value(const Variant &p_value, const StringName &p_name) {
	pushed_values[p_name] = p_value;
	if (!_can_deliver(p_name)) {
		undelivered.insert(p_name);
		return;
	}
	undelivered.erase(p_name);
	_deliver(p_name, p_value);
}

Variant VideoWindowSettingsServer::_pull_window_value(const StringName &p_name) {
	const Target target = _get_target(p_name);
	if (undelivered.has(p_name) || target == TARGET_NONE) {
		return _get_current(p_name);
	}
	DisplayServer *display_server = _get_display_server();
	const DisplayServerEnums::WindowID main_window = DisplayServerEnums::MAIN_WINDOW_ID;

	if (target == TARGET_SCENE) {
		const Window *root = _get_root();
		if (p_name == SNAME("content_scale_mode") && root != nullptr) {
			return int(root->get_content_scale_mode());
		} else if (p_name == SNAME("content_scale_aspect") && root != nullptr) {
			return int(root->get_content_scale_aspect());
		} else if (p_name == SNAME("content_scale_stretch") && root != nullptr) {
			return int(root->get_content_scale_stretch());
		} else if (p_name == SNAME("content_scale_factor") && root != nullptr) {
			return float(root->get_content_scale_factor());
		} else if (p_name == SNAME("hdr_output_requested") && root != nullptr && display_server != nullptr) {
			return root->is_hdr_output_requested();
		} else if (p_name == SNAME("hdr_reference_luminance") && display_server != nullptr) {
			return display_server->window_get_hdr_output_reference_luminance(main_window);
		} else if (p_name == SNAME("hdr_max_luminance") && display_server != nullptr) {
			return display_server->window_get_hdr_output_max_luminance(main_window);
		}
		return _get_current(p_name);
	}

	if (display_server == nullptr || _is_embedded_in_editor()) {
		return _get_current(p_name);
	}
	if (p_name == SNAME("vsync_mode")) {
		return int(display_server->window_get_vsync_mode(main_window));
	}
	const int flag = video_window_flag(p_name);
	if (flag >= 0) {
		return display_server->window_get_flag(DisplayServerEnums::WindowFlags(flag), main_window);
	}
	const DisplayServerEnums::WindowMode mode = display_server->window_get_mode(main_window);
	if (p_name == SNAME("window_mode")) {
		return int(mode);
	}
	const int screen = display_server->window_get_current_screen(main_window);
	if (p_name == SNAME("screen")) {
		return screen;
	}
	// Size and position are those of the windowed window; in the other modes the stored ones stay.
	if (mode != DisplayServerEnums::WINDOW_MODE_WINDOWED) {
		return _get_current(p_name);
	}
	const Size2i size = display_server->window_get_size(main_window);
	if (p_name == SNAME("window_size")) {
		return Vector2i(size);
	}
	const Point2i screen_position = display_server->screen_get_position(screen);
	const Point2i position = display_server->window_get_position(main_window) - screen_position;
	if (p_name == SNAME("window_position")) {
		return Vector2i(position);
	}
	// window_centered: the window is where place_main_window() puts a centered window (within a pixel).
	const Rect2i usable = display_server->screen_get_usable_rect(screen);
	const Point2i centered = usable.position - screen_position + (usable.size - size) / 2;
	const Vector2i offset = (position - centered).abs();
	return offset.x <= 1 && offset.y <= 1;
}

void VideoWindowSettingsServer::_place_main_window() {
	DisplayServer *display_server = _get_display_server();
	if (display_server == nullptr || placing || _is_embedded_in_editor() || !display_server->has_feature(DisplayServerEnums::FEATURE_SUBWINDOWS)) {
		return;
	}
	placing = true;
	const DisplayServerEnums::WindowID main_window = DisplayServerEnums::MAIN_WINDOW_ID;
	const int screen = _resolve_screen(_get_current(SNAME("screen")));
	const DisplayServerEnums::WindowMode mode = display_server->window_get_mode(main_window);
	if (mode != DisplayServerEnums::WINDOW_MODE_WINDOWED) {
		if (mode != DisplayServerEnums::WINDOW_MODE_MINIMIZED && display_server->window_get_current_screen(main_window) != screen) {
			display_server->window_set_current_screen(screen, main_window);
		}
	} else {
		const Size2i size = display_server->window_get_size(main_window);
		Point2i position;
		if (bool(_get_current(SNAME("window_centered")))) {
			const Rect2i usable = display_server->screen_get_usable_rect(screen);
			position = usable.position + (usable.size - size) / 2;
		} else {
			position = display_server->screen_get_position(screen) + Vector2i(_get_current(SNAME("window_position")));
		}
		// After a monitor change the window is moved back into the visible area of the screen.
		const Rect2i rect = _fit_into_screen(Rect2i(position, size), screen);
		if (rect.size != size) {
			display_server->window_set_size(rect.size, main_window);
		}
		display_server->window_set_position(rect.position, main_window);
	}
	placing = false;
}

void VideoWindowSettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &VideoWindowSettingsServer::_on_node_added);
	if (!tree->is_connected(SNAME("node_added"), callback)) {
		tree->connect(SNAME("node_added"), callback);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Further windows.

String VideoWindowSettingsServer::_get_window_key(Window *p_window, const String &p_key) const {
	if (!p_key.is_empty()) {
		return p_key;
	}
	return p_window->is_inside_tree() ? String(p_window->get_path()) : String();
}

Dictionary VideoWindowSettingsServer::_capture_window_state(Window *p_window) const {
	Dictionary state;
	state["mode"] = int(p_window->get_mode());
	state["size"] = Vector2i(p_window->get_size());
	const DisplayServer *display_server = _get_display_server();
	if (display_server == nullptr || !p_window->is_inside_tree() || p_window->is_embedded()) {
		// Embedded windows are placed inside their embedder: no screen.
		state["position"] = Vector2i(p_window->get_position());
		return state;
	}
	const int screen = p_window->get_current_screen();
	state["screen"] = screen;
	state["position"] = Vector2i(p_window->get_position() - display_server->screen_get_position(screen));
	return state;
}

void VideoWindowSettingsServer::_apply_window_state(Window *p_window, const Dictionary &p_state) const {
	const Window::Mode mode = Window::Mode(int(p_state.get("mode", int(Window::MODE_WINDOWED))));
	Size2i size = p_state.get("size", Vector2i(p_window->get_size()));
	Point2i position = p_state.get("position", Vector2i(p_window->get_position()));
	const DisplayServer *display_server = _get_display_server();
	if (p_state.has("screen") && display_server != nullptr && !p_window->is_embedded()) {
		// A missing screen falls back to the primary screen; the window stays visible.
		const int screen = _resolve_screen(p_state["screen"]);
		const Rect2i rect = _fit_into_screen(Rect2i(display_server->screen_get_position(screen) + position, size), screen);
		position = rect.position;
		size = rect.size;
	}
	p_window->set_initial_position(Window::WINDOW_INITIAL_POSITION_ABSOLUTE);
	p_window->set_size(size.max(Vector2i(1, 1)));
	p_window->set_position(position);
	p_window->set_mode(mode);
}

void VideoWindowSettingsServer::_on_node_added(Node *p_node) {
	Window *window = Object::cast_to<Window>(p_node);
	if (window == nullptr || window == _get_root()) {
		return;
	}
	const Dictionary states = get_value(SNAME("window_states"));
	if (states.is_empty()) {
		return;
	}
	const Variant *state = states.getptr(String(window->get_path()));
	if (state != nullptr && state->get_type() == Variant::DICTIONARY) {
		_apply_window_state(window, *state);
	}
}

void VideoWindowSettingsServer::save_window_state(Window *p_window, const String &p_key) {
	ERR_FAIL_NULL(p_window);
	const String key = _get_window_key(p_window, p_key);
	ERR_FAIL_COND_MSG(key.is_empty(), "The window is not in the scene tree: pass a key to save its state.");
	Dictionary states = Dictionary(get_value(SNAME("window_states"))).duplicate();
	states[key] = _capture_window_state(p_window);
	set_value(SNAME("window_states"), states);
	_connect_tree();
}

bool VideoWindowSettingsServer::restore_window_state(Window *p_window, const String &p_key) {
	ERR_FAIL_NULL_V(p_window, false);
	const String key = _get_window_key(p_window, p_key);
	if (key.is_empty()) {
		return false;
	}
	const Dictionary states = get_value(SNAME("window_states"));
	const Variant *state = states.getptr(key);
	if (state == nullptr || state->get_type() != Variant::DICTIONARY) {
		return false;
	}
	_apply_window_state(p_window, *state);
	return true;
}

bool VideoWindowSettingsServer::has_window_state(const String &p_key) const {
	const Dictionary states = get_value(SNAME("window_states"));
	return states.has(p_key);
}

void VideoWindowSettingsServer::clear_window_state(const String &p_key) {
	Dictionary states = Dictionary(get_value(SNAME("window_states"))).duplicate();
	if (states.erase(p_key)) {
		set_value(SNAME("window_states"), states);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Startup.

Dictionary VideoWindowSettingsServer::get_command_line_options() {
	// Main::setup() consumes these options, so they are not in OS::get_cmdline_args().
	return Main::get_command_line_window_options();
}

void VideoWindowSettingsServer::get_boot_window(BootWindow &r_window) {
	// All boot values pushed so far: the player's file and the command line (`--windowed` included).
	static const char *boot_names[] = { "window_mode", "window_size", "vsync_mode", "borderless", "always_on_top", "unresizable", "transparent" };
	for (const char *boot_name : boot_names) {
		const StringName setting_name = boot_name;
		if (!undelivered.erase(setting_name)) {
			continue;
		}
		const Variant value = _get_current(setting_name);
		if (setting_name == SNAME("window_mode")) {
			r_window.mode = DisplayServerEnums::WindowMode(int(value));
		} else if (setting_name == SNAME("window_size")) {
			r_window.size = Vector2i(value).max(Vector2i(1, 1));
		} else if (setting_name == SNAME("vsync_mode")) {
			r_window.vsync_mode = DisplayServerEnums::VSyncMode(int(value));
		} else {
			const uint32_t bit = 1u << uint32_t(video_window_flag(setting_name));
			if (bool(value)) {
				r_window.flags |= bit;
			} else {
				r_window.flags &= ~bit;
			}
		}
	}
}

void VideoWindowSettingsServer::on_display_server_created() {
	// The stored screen and position; `--screen` and `--position` were already passed to DisplayServer::create().
	bool place = false;
	static const char *placement_names[] = { "screen", "window_position", "window_centered" };
	for (const char *placement_name : placement_names) {
		const StringName setting_name = placement_name;
		if (undelivered.erase(setting_name) && !has_session_override(setting_name)) {
			place = true;
		}
	}
	_deliver_undelivered(TARGET_BOOT);
	if (place) {
		_place_main_window();
	}
}

void VideoWindowSettingsServer::_on_loaded() {
	// Only the load of startup(), after the player's file is read and before it is applied.
	if (!is_started() || startup_overrides_done) {
		return;
	}
	startup_overrides_done = true;
	const Dictionary options = get_command_line_options();
	static const char *option_names[] = { "window_mode", "window_size", "screen", "window_position", "always_on_top", "vsync_mode" };
	for (const char *option_name : option_names) {
		if (options.has(option_name)) {
			// Pushed now, never saved; a later set_value() replaces it.
			set_session_override(option_name, options[option_name]);
		}
	}
	if (options.has("window_position")) {
		set_session_override(SNAME("window_centered"), false);
	}
}

void VideoWindowSettingsServer::_on_startup_applied(int p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// After Main::start() and SceneTree have set the project values of the root window.
		_deliver_undelivered(TARGET_SCENE);
		_connect_tree();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName VideoWindowSettingsServer::get_domain() const {
	return SNAME("video_window");
}

SettingsServer::ApplyPhase VideoWindowSettingsServer::get_apply_phase() const {
	// Before DisplayServer::create(): the main window opens with the stored values.
	return APPLY_PHASE_PRE_DISPLAY;
}

bool VideoWindowSettingsServer::is_value_supported(const StringName &p_name) const {
	if (!SettingsServer::is_value_supported(p_name)) {
		return false;
	}
	const DisplayServer *display_server = DisplayServer::get_singleton();
	if (display_server == nullptr) {
		return true; // Not known before the display server exists.
	}
	const bool has_screen = display_server->get_screen_count() > 0;
	if (p_name == SNAME("window_mode")) {
		return has_screen;
	} else if (p_name == SNAME("vsync_mode")) {
		return has_screen && !OS::get_singleton()->has_feature("web");
	} else if (p_name == SNAME("transparent")) {
		return display_server->has_feature(DisplayServerEnums::FEATURE_SUBWINDOWS) && display_server->has_feature(DisplayServerEnums::FEATURE_WINDOW_TRANSPARENCY) && OS::get_singleton()->is_layered_allowed();
	} else if (String(p_name).begins_with("hdr_")) {
		return display_server->has_feature(DisplayServerEnums::FEATURE_HDR_OUTPUT);
	}
	const Target target = _get_target(p_name);
	if (target == TARGET_BOOT || target == TARGET_PLACEMENT) {
		// Size, position, screen and flags need free windows (not Android, iOS, Web or headless).
		return display_server->has_feature(DisplayServerEnums::FEATURE_SUBWINDOWS);
	}
	return true;
}

Ref<Resource> VideoWindowSettingsServer::_create_default_settings() const {
	Ref<VideoWindowSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

Variant VideoWindowSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (p_name == SNAME("window_size")) {
		return Vector2i(value).max(Vector2i(1, 1));
	} else if (p_name == SNAME("hdr_reference_luminance") || p_name == SNAME("hdr_max_luminance")) {
		const float luminance = value;
		return luminance < 0.0f ? -1.0f : luminance; // Negative: automatic.
	} else if (p_name == SNAME("window_states") && value.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return value;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void VideoWindowSettingsServer::set_window_mode(DisplayServerEnums::WindowMode p_mode) {
	set_value(SNAME("window_mode"), int(p_mode));
}

DisplayServerEnums::WindowMode VideoWindowSettingsServer::get_window_mode() const {
	return DisplayServerEnums::WindowMode(int(get_value(SNAME("window_mode"))));
}

void VideoWindowSettingsServer::set_window_size(const Size2i &p_size) {
	set_value(SNAME("window_size"), Vector2i(p_size));
}

Size2i VideoWindowSettingsServer::get_window_size() const {
	return Vector2i(get_value(SNAME("window_size")));
}

void VideoWindowSettingsServer::set_screen(int p_screen) {
	set_value(SNAME("screen"), p_screen);
}

int VideoWindowSettingsServer::get_screen() const {
	return get_value(SNAME("screen"));
}

void VideoWindowSettingsServer::set_window_centered(bool p_centered) {
	set_value(SNAME("window_centered"), p_centered);
}

bool VideoWindowSettingsServer::is_window_centered() const {
	return get_value(SNAME("window_centered"));
}

void VideoWindowSettingsServer::set_window_position(const Point2i &p_position) {
	set_value(SNAME("window_position"), Vector2i(p_position));
}

Point2i VideoWindowSettingsServer::get_window_position() const {
	return Vector2i(get_value(SNAME("window_position")));
}

void VideoWindowSettingsServer::set_borderless(bool p_borderless) {
	set_value(SNAME("borderless"), p_borderless);
}

bool VideoWindowSettingsServer::is_borderless() const {
	return get_value(SNAME("borderless"));
}

void VideoWindowSettingsServer::set_always_on_top(bool p_always_on_top) {
	set_value(SNAME("always_on_top"), p_always_on_top);
}

bool VideoWindowSettingsServer::is_always_on_top() const {
	return get_value(SNAME("always_on_top"));
}

void VideoWindowSettingsServer::set_unresizable(bool p_unresizable) {
	set_value(SNAME("unresizable"), p_unresizable);
}

bool VideoWindowSettingsServer::is_unresizable() const {
	return get_value(SNAME("unresizable"));
}

void VideoWindowSettingsServer::set_transparent(bool p_transparent) {
	set_value(SNAME("transparent"), p_transparent);
}

bool VideoWindowSettingsServer::is_transparent() const {
	return get_value(SNAME("transparent"));
}

void VideoWindowSettingsServer::set_vsync_mode(DisplayServerEnums::VSyncMode p_mode) {
	set_value(SNAME("vsync_mode"), int(p_mode));
}

DisplayServerEnums::VSyncMode VideoWindowSettingsServer::get_vsync_mode() const {
	return DisplayServerEnums::VSyncMode(int(get_value(SNAME("vsync_mode"))));
}

void VideoWindowSettingsServer::set_content_scale_mode(Window::ContentScaleMode p_mode) {
	set_value(SNAME("content_scale_mode"), int(p_mode));
}

Window::ContentScaleMode VideoWindowSettingsServer::get_content_scale_mode() const {
	return Window::ContentScaleMode(int(get_value(SNAME("content_scale_mode"))));
}

void VideoWindowSettingsServer::set_content_scale_aspect(Window::ContentScaleAspect p_aspect) {
	set_value(SNAME("content_scale_aspect"), int(p_aspect));
}

Window::ContentScaleAspect VideoWindowSettingsServer::get_content_scale_aspect() const {
	return Window::ContentScaleAspect(int(get_value(SNAME("content_scale_aspect"))));
}

void VideoWindowSettingsServer::set_content_scale_stretch(Window::ContentScaleStretch p_stretch) {
	set_value(SNAME("content_scale_stretch"), int(p_stretch));
}

Window::ContentScaleStretch VideoWindowSettingsServer::get_content_scale_stretch() const {
	return Window::ContentScaleStretch(int(get_value(SNAME("content_scale_stretch"))));
}

void VideoWindowSettingsServer::set_content_scale_factor(float p_factor) {
	set_value(SNAME("content_scale_factor"), p_factor);
}

float VideoWindowSettingsServer::get_content_scale_factor() const {
	return get_value(SNAME("content_scale_factor"));
}

void VideoWindowSettingsServer::set_hdr_output_requested(bool p_requested) {
	set_value(SNAME("hdr_output_requested"), p_requested);
}

bool VideoWindowSettingsServer::is_hdr_output_requested() const {
	return get_value(SNAME("hdr_output_requested"));
}

void VideoWindowSettingsServer::set_hdr_reference_luminance(float p_luminance) {
	set_value(SNAME("hdr_reference_luminance"), p_luminance);
}

float VideoWindowSettingsServer::get_hdr_reference_luminance() const {
	return get_value(SNAME("hdr_reference_luminance"));
}

void VideoWindowSettingsServer::set_hdr_max_luminance(float p_luminance) {
	set_value(SNAME("hdr_max_luminance"), p_luminance);
}

float VideoWindowSettingsServer::get_hdr_max_luminance() const {
	return get_value(SNAME("hdr_max_luminance"));
}

void VideoWindowSettingsServer::place_main_window() {
	_place_main_window();
}

void VideoWindowSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_window_mode", "mode"), &VideoWindowSettingsServer::set_window_mode);
	ClassDB::bind_method(D_METHOD("get_window_mode"), &VideoWindowSettingsServer::get_window_mode);
	ClassDB::bind_method(D_METHOD("set_window_size", "size"), &VideoWindowSettingsServer::set_window_size);
	ClassDB::bind_method(D_METHOD("get_window_size"), &VideoWindowSettingsServer::get_window_size);
	ClassDB::bind_method(D_METHOD("set_screen", "screen"), &VideoWindowSettingsServer::set_screen);
	ClassDB::bind_method(D_METHOD("get_screen"), &VideoWindowSettingsServer::get_screen);
	ClassDB::bind_method(D_METHOD("set_window_centered", "centered"), &VideoWindowSettingsServer::set_window_centered);
	ClassDB::bind_method(D_METHOD("is_window_centered"), &VideoWindowSettingsServer::is_window_centered);
	ClassDB::bind_method(D_METHOD("set_window_position", "position"), &VideoWindowSettingsServer::set_window_position);
	ClassDB::bind_method(D_METHOD("get_window_position"), &VideoWindowSettingsServer::get_window_position);
	ClassDB::bind_method(D_METHOD("set_borderless", "borderless"), &VideoWindowSettingsServer::set_borderless);
	ClassDB::bind_method(D_METHOD("is_borderless"), &VideoWindowSettingsServer::is_borderless);
	ClassDB::bind_method(D_METHOD("set_always_on_top", "always_on_top"), &VideoWindowSettingsServer::set_always_on_top);
	ClassDB::bind_method(D_METHOD("is_always_on_top"), &VideoWindowSettingsServer::is_always_on_top);
	ClassDB::bind_method(D_METHOD("set_unresizable", "unresizable"), &VideoWindowSettingsServer::set_unresizable);
	ClassDB::bind_method(D_METHOD("is_unresizable"), &VideoWindowSettingsServer::is_unresizable);
	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &VideoWindowSettingsServer::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &VideoWindowSettingsServer::is_transparent);
	ClassDB::bind_method(D_METHOD("set_vsync_mode", "mode"), &VideoWindowSettingsServer::set_vsync_mode);
	ClassDB::bind_method(D_METHOD("get_vsync_mode"), &VideoWindowSettingsServer::get_vsync_mode);

	ClassDB::bind_method(D_METHOD("set_content_scale_mode", "mode"), &VideoWindowSettingsServer::set_content_scale_mode);
	ClassDB::bind_method(D_METHOD("get_content_scale_mode"), &VideoWindowSettingsServer::get_content_scale_mode);
	ClassDB::bind_method(D_METHOD("set_content_scale_aspect", "aspect"), &VideoWindowSettingsServer::set_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("get_content_scale_aspect"), &VideoWindowSettingsServer::get_content_scale_aspect);
	ClassDB::bind_method(D_METHOD("set_content_scale_stretch", "stretch"), &VideoWindowSettingsServer::set_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("get_content_scale_stretch"), &VideoWindowSettingsServer::get_content_scale_stretch);
	ClassDB::bind_method(D_METHOD("set_content_scale_factor", "factor"), &VideoWindowSettingsServer::set_content_scale_factor);
	ClassDB::bind_method(D_METHOD("get_content_scale_factor"), &VideoWindowSettingsServer::get_content_scale_factor);

	ClassDB::bind_method(D_METHOD("set_hdr_output_requested", "requested"), &VideoWindowSettingsServer::set_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("is_hdr_output_requested"), &VideoWindowSettingsServer::is_hdr_output_requested);
	ClassDB::bind_method(D_METHOD("set_hdr_reference_luminance", "luminance"), &VideoWindowSettingsServer::set_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_reference_luminance"), &VideoWindowSettingsServer::get_hdr_reference_luminance);
	ClassDB::bind_method(D_METHOD("set_hdr_max_luminance", "luminance"), &VideoWindowSettingsServer::set_hdr_max_luminance);
	ClassDB::bind_method(D_METHOD("get_hdr_max_luminance"), &VideoWindowSettingsServer::get_hdr_max_luminance);

	ClassDB::bind_method(D_METHOD("place_main_window"), &VideoWindowSettingsServer::place_main_window);
	ClassDB::bind_method(D_METHOD("save_window_state", "window", "key"), &VideoWindowSettingsServer::save_window_state, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("restore_window_state", "window", "key"), &VideoWindowSettingsServer::restore_window_state, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("has_window_state", "key"), &VideoWindowSettingsServer::has_window_state);
	ClassDB::bind_method(D_METHOD("clear_window_state", "key"), &VideoWindowSettingsServer::clear_window_state);

	ClassDB::bind_static_method("VideoWindowSettingsServer", D_METHOD("get_command_line_options"), &VideoWindowSettingsServer::get_command_line_options);
}

VideoWindowSettingsServer::VideoWindowSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	static const char *bound_names[] = {
		"window_mode",
		"window_size",
		"screen",
		"window_centered",
		"window_position",
		"borderless",
		"always_on_top",
		"unresizable",
		"transparent",
		"vsync_mode",
		"content_scale_mode",
		"content_scale_aspect",
		"content_scale_stretch",
		"content_scale_factor",
		"hdr_output_requested",
		"hdr_reference_luminance",
		"hdr_max_luminance",
	};
	for (const char *bound_name : bound_names) {
		const StringName setting_name = bound_name;
		// Mode, size and screen can leave the player without a visible window: they wait for confirm_pending().
		const bool confirm = setting_name == SNAME("window_mode") || setting_name == SNAME("window_size") || setting_name == SNAME("screen");
		bind_setting_callbacks(setting_name, callable_mp(this, &VideoWindowSettingsServer::_push_window_value).bind(setting_name), callable_mp(this, &VideoWindowSettingsServer::_pull_window_value).bind(setting_name), confirm ? SETTING_FLAG_CONFIRM : SETTING_FLAG_NONE);
	}

	connect(SNAME("loaded"), callable_mp(this, &VideoWindowSettingsServer::_on_loaded));
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &VideoWindowSettingsServer::_on_startup_applied));
	}
}

VideoWindowSettingsServer::~VideoWindowSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
