/**************************************************************************/
/*  transition_screen.cpp                                                 */
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

#include "transition_screen.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/preload_list.h"
#include "core/io/resource_database.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/range.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

// TransitionScreen

void TransitionScreen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return; // Shown fully visible and idle, so it can be designed like any scene.
			}
			_set_fade(0.0f);
			set_process_internal(true);
			set_process_input(true);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			_process_transition(get_process_delta_time());
		} break;
	}
}

void TransitionScreen::input(const Ref<InputEvent> &p_event) {
	if (!skippable || state == STATE_IDLE || state >= STATE_FADING_OUT || finish_requested) {
		return;
	}
	if (p_event.is_null() || !p_event->is_pressed() || p_event->is_echo()) {
		return;
	}
	get_viewport()->set_input_as_handled();
	emit_signal(SNAME("skipped"));
	finish();
}

String TransitionScreen::_get_next_scene_path() const {
	return next_scene.is_empty() ? String() : ResourceUID::ensure_path(next_scene);
}

void TransitionScreen::_start() {
	state = STATE_FADING_IN;
	phase_time = 0.0;
	loading_path = _get_next_scene_path();
	if (!loading_path.is_empty() && loading_path == get_scene_file_path()) {
		loading_path = String(); // Never load the screen's own scene again (a splash set as main scene).
	}
	emit_signal(SNAME("started"));
	if (!free_current_scene_before_loading) {
		_request_loading();
	}
}

void TransitionScreen::_process_transition(double p_delta) {
	if (state == STATE_IDLE) {
		_start();
		p_delta = 0.0;
	}

	if (state == STATE_FADING_IN) {
		phase_time += p_delta;
		_set_fade(fade_in_duration > 0.0 ? float(MIN(phase_time / fade_in_duration, 1.0)) : 1.0f);
		if (fade < 1.0f) {
			return;
		}
		state = STATE_LOADING;
		phase_time = 0.0;
		p_delta = 0.0;
		if (free_current_scene_before_loading) {
			Node *current = get_tree()->get_current_scene();
			if (current && current != this) {
				// Removing clears `SceneTree.current_scene`; the node is freed at the end of the frame.
				current->get_parent()->remove_child(current);
				current->queue_free();
			}
			_request_loading();
		}
	}

	if (state == STATE_LOADING || state == STATE_LOADED) {
		phase_time += p_delta;
		if (state == STATE_LOADING) {
			_poll_loading();
		}
		if (state != STATE_LOADED) {
			return;
		}
		if (!finish_requested && !(auto_finish && phase_time >= min_duration)) {
			return;
		}
		_change_scene();
		state = STATE_FADING_OUT;
		phase_time = 0.0;
		p_delta = 0.0;
	}

	if (state == STATE_FADING_OUT) {
		phase_time += p_delta;
		_set_fade(fade_out_duration > 0.0 ? float(1.0 - MIN(phase_time / fade_out_duration, 1.0)) : 0.0f);
		if (fade > 0.0f) {
			return;
		}
		state = STATE_COMPLETED;
		set_process_internal(false);
		set_process_input(false);
		emit_signal(SNAME("completed"));
		queue_free();
	}
}

void TransitionScreen::_set_fade(float p_fade) {
	fade = p_fade;
	if (!GDVIRTUAL_CALL(_update_fade, fade)) {
		// The layer's own canvas: every child fades together, their own `modulate` stays untouched.
		RenderingServer::get_singleton()->canvas_set_modulate(get_canvas(), Color(1, 1, 1, fade));
	}
}

void TransitionScreen::_set_progress(float p_progress) {
	if (p_progress == progress) {
		return;
	}
	progress = p_progress;
	_progress_changed(progress);
	emit_signal(SNAME("load_progress"), progress);
}

void TransitionScreen::_request_loading() {
	if (preload_list.is_valid() && ResourceDatabase::get_singleton()) {
		ResourceDatabase *database = ResourceDatabase::get_singleton();
		const Callable on_finished = callable_mp(this, &TransitionScreen::_preload_finished);
		database->connect(SNAME("preload_finished"), on_finished);
		preload_pending = true;
		const Error preload_error = database->preload_from_list(preload_list);
		if (preload_error == ERR_INVALID_PARAMETER || preload_error == ERR_UNAVAILABLE) {
			// Rejected before starting: `preload_finished` will not be emitted.
			preload_pending = false;
			database->disconnect(SNAME("preload_finished"), on_finished);
		}
	}
	if (loading_path.is_empty()) {
		return;
	}
	const Error err = ResourceLoader::load_threaded_request(loading_path, "PackedScene");
	if (err != OK) {
		ERR_PRINT(vformat("TransitionScreen: Can't start loading the next scene '%s' (error %d).", loading_path, int(err)));
		loading_path = String();
	}
}

void TransitionScreen::_poll_loading() {
	if (!loading_path.is_empty() && loaded_scene.is_null()) {
#ifdef THREADS_ENABLED
		float load_progress = 0.0f;
		if (ResourceLoader::load_threaded_get_status(loading_path, &load_progress) == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
			_set_progress(MIN(load_progress, 0.99f)); // 1.0 only when everything is loaded.
			return;
		}
#endif
		// Without threads (default Web export) the load completes here, in one blocking step while the screen is
		// fully visible.
		loaded_scene = ResourceLoader::load_threaded_get(loading_path);
		if (loaded_scene.is_null()) {
			ERR_PRINT(vformat("TransitionScreen: Can't load the next scene '%s'. The current scene stays.", loading_path));
			loading_path = String();
		}
	}
	if (preload_pending) {
		return;
	}
	_set_progress(1.0f);
	state = STATE_LOADED;
	emit_signal(SNAME("loaded"));
}

void TransitionScreen::_preload_finished(const Ref<PreloadList> &p_list, int p_error) {
	if (p_list != preload_list) {
		return;
	}
	preload_pending = false;
	ResourceDatabase *database = ResourceDatabase::get_singleton();
	const Callable on_finished = callable_mp(this, &TransitionScreen::_preload_finished);
	if (database && database->is_connected(SNAME("preload_finished"), on_finished)) {
		database->disconnect(SNAME("preload_finished"), on_finished);
	}
}

void TransitionScreen::_change_scene() {
	if (loaded_scene.is_null()) {
		return;
	}
	SceneTree *tree = get_tree();
	if (tree->get_current_scene() == this) {
		// Opened as the current scene (boot splash, `change_scene_to_file()`): stay in the tree for the fade-out.
		tree->set_current_scene(nullptr);
	}
	tree->change_scene_to_packed(loaded_scene);
	loaded_scene.unref();
}

Error TransitionScreen::change_scene_with_transition(SceneTree *p_tree, const String &p_path, const Ref<PackedScene> &p_transition_scene) {
	ERR_FAIL_NULL_V(p_tree, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V_MSG(p_path.is_empty(), ERR_INVALID_PARAMETER, "The path of the next scene is empty.");

	TransitionScreen *screen = nullptr;
	if (p_transition_scene.is_valid()) {
		Node *node = p_transition_scene->instantiate();
		ERR_FAIL_NULL_V(node, ERR_CANT_CREATE);
		screen = Object::cast_to<TransitionScreen>(node);
		if (!screen) {
			memdelete(node);
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "The root node of the transition scene must be a TransitionScreen, LoadingScreen or SplashScreen.");
		}
	} else {
		// Built-in black fade.
		screen = memnew(TransitionScreen);
		ColorRect *background = memnew(ColorRect);
		background->set_color(Color(0, 0, 0));
		background->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		screen->add_child(background);
	}
	screen->set_next_scene(p_path);
	// Deferred: also works when called from `_ready()` while the root is busy adding children.
	p_tree->get_root()->call_deferred(SNAME("add_child"), screen);
	return OK;
}

void TransitionScreen::set_next_scene(const String &p_path) {
	next_scene = p_path;
}

String TransitionScreen::get_next_scene() const {
	return next_scene;
}

void TransitionScreen::set_min_duration(double p_seconds) {
	min_duration = MAX(p_seconds, 0.0);
}

double TransitionScreen::get_min_duration() const {
	return min_duration;
}

void TransitionScreen::set_fade_in_duration(double p_seconds) {
	fade_in_duration = MAX(p_seconds, 0.0);
}

double TransitionScreen::get_fade_in_duration() const {
	return fade_in_duration;
}

void TransitionScreen::set_fade_out_duration(double p_seconds) {
	fade_out_duration = MAX(p_seconds, 0.0);
}

double TransitionScreen::get_fade_out_duration() const {
	return fade_out_duration;
}

void TransitionScreen::set_skippable(bool p_skippable) {
	skippable = p_skippable;
}

bool TransitionScreen::is_skippable() const {
	return skippable;
}

void TransitionScreen::set_auto_finish(bool p_auto_finish) {
	auto_finish = p_auto_finish;
}

bool TransitionScreen::is_auto_finish() const {
	return auto_finish;
}

void TransitionScreen::set_free_current_scene_before_loading(bool p_free) {
	free_current_scene_before_loading = p_free;
}

bool TransitionScreen::is_free_current_scene_before_loading() const {
	return free_current_scene_before_loading;
}

void TransitionScreen::set_preload_list(const Ref<PreloadList> &p_list) {
	preload_list = p_list;
}

Ref<PreloadList> TransitionScreen::get_preload_list() const {
	return preload_list;
}

void TransitionScreen::finish() {
	finish_requested = true;
}

TransitionScreen::State TransitionScreen::get_state() const {
	return state;
}

float TransitionScreen::get_progress() const {
	return progress;
}

float TransitionScreen::get_fade() const {
	return fade;
}

void TransitionScreen::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_next_scene", "path"), &TransitionScreen::set_next_scene);
	ClassDB::bind_method(D_METHOD("get_next_scene"), &TransitionScreen::get_next_scene);
	ClassDB::bind_method(D_METHOD("set_min_duration", "seconds"), &TransitionScreen::set_min_duration);
	ClassDB::bind_method(D_METHOD("get_min_duration"), &TransitionScreen::get_min_duration);
	ClassDB::bind_method(D_METHOD("set_fade_in_duration", "seconds"), &TransitionScreen::set_fade_in_duration);
	ClassDB::bind_method(D_METHOD("get_fade_in_duration"), &TransitionScreen::get_fade_in_duration);
	ClassDB::bind_method(D_METHOD("set_fade_out_duration", "seconds"), &TransitionScreen::set_fade_out_duration);
	ClassDB::bind_method(D_METHOD("get_fade_out_duration"), &TransitionScreen::get_fade_out_duration);
	ClassDB::bind_method(D_METHOD("set_skippable", "skippable"), &TransitionScreen::set_skippable);
	ClassDB::bind_method(D_METHOD("is_skippable"), &TransitionScreen::is_skippable);
	ClassDB::bind_method(D_METHOD("set_auto_finish", "auto_finish"), &TransitionScreen::set_auto_finish);
	ClassDB::bind_method(D_METHOD("is_auto_finish"), &TransitionScreen::is_auto_finish);
	ClassDB::bind_method(D_METHOD("set_free_current_scene_before_loading", "enabled"), &TransitionScreen::set_free_current_scene_before_loading);
	ClassDB::bind_method(D_METHOD("is_free_current_scene_before_loading"), &TransitionScreen::is_free_current_scene_before_loading);
	ClassDB::bind_method(D_METHOD("set_preload_list", "list"), &TransitionScreen::set_preload_list);
	ClassDB::bind_method(D_METHOD("get_preload_list"), &TransitionScreen::get_preload_list);

	ClassDB::bind_method(D_METHOD("finish"), &TransitionScreen::finish);
	ClassDB::bind_method(D_METHOD("get_state"), &TransitionScreen::get_state);
	ClassDB::bind_method(D_METHOD("get_progress"), &TransitionScreen::get_progress);
	ClassDB::bind_method(D_METHOD("get_fade"), &TransitionScreen::get_fade);

	GDVIRTUAL_BIND(_update_fade, "fade");

	ADD_GROUP("Transition", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "next_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), "set_next_scene", "get_next_scene");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_duration", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater,suffix:s"), "set_min_duration", "get_min_duration");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_in_duration", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater,suffix:s"), "set_fade_in_duration", "get_fade_in_duration");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_out_duration", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater,suffix:s"), "set_fade_out_duration", "get_fade_out_duration");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "skippable"), "set_skippable", "is_skippable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_finish"), "set_auto_finish", "is_auto_finish");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "free_current_scene_before_loading"), "set_free_current_scene_before_loading", "is_free_current_scene_before_loading");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "preload_list", PROPERTY_HINT_RESOURCE_TYPE, "PreloadList"), "set_preload_list", "get_preload_list");

	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("load_progress", PropertyInfo(Variant::FLOAT, "progress")));
	ADD_SIGNAL(MethodInfo("loaded"));
	ADD_SIGNAL(MethodInfo("skipped"));
	ADD_SIGNAL(MethodInfo("completed"));

	BIND_ENUM_CONSTANT(STATE_IDLE);
	BIND_ENUM_CONSTANT(STATE_FADING_IN);
	BIND_ENUM_CONSTANT(STATE_LOADING);
	BIND_ENUM_CONSTANT(STATE_LOADED);
	BIND_ENUM_CONSTANT(STATE_FADING_OUT);
	BIND_ENUM_CONSTANT(STATE_COMPLETED);
}

TransitionScreen::TransitionScreen() {
	set_layer(100); // Above the HUD layers of the scenes it covers.
	set_process_mode(PROCESS_MODE_ALWAYS); // Also while the tree is paused.
}

// LoadingScreen

void LoadingScreen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			_progress_changed(0.0f);
			_show_tip(0);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (tips.size() < 2 || tip_interval <= 0.0) {
				return;
			}
			tip_time += get_process_delta_time();
			if (tip_time >= tip_interval) {
				_show_tip((tip_index + 1) % tips.size());
			}
		} break;
	}
}

void LoadingScreen::_progress_changed(float p_progress) {
	Range *bar = Object::cast_to<Range>(get_node_or_null(progress_bar));
	if (bar) {
		bar->set_as_ratio(p_progress);
	}
}

void LoadingScreen::_show_tip(int p_index) {
	tip_time = 0.0;
	if (tips.is_empty()) {
		return;
	}
	tip_index = p_index;
	Node *label = get_node_or_null(tip_label);
	if (label) {
		label->set(SNAME("text"), tips[tip_index]);
	}
}

void LoadingScreen::set_progress_bar(const NodePath &p_path) {
	progress_bar = p_path;
}

NodePath LoadingScreen::get_progress_bar() const {
	return progress_bar;
}

void LoadingScreen::set_tip_label(const NodePath &p_path) {
	tip_label = p_path;
}

NodePath LoadingScreen::get_tip_label() const {
	return tip_label;
}

void LoadingScreen::set_tips(const PackedStringArray &p_tips) {
	tips = p_tips;
	tip_index = -1;
	if (is_ready() && !Engine::get_singleton()->is_editor_hint()) {
		_show_tip(0);
	}
}

PackedStringArray LoadingScreen::get_tips() const {
	return tips;
}

void LoadingScreen::set_tip_interval(double p_seconds) {
	tip_interval = MAX(p_seconds, 0.0);
}

double LoadingScreen::get_tip_interval() const {
	return tip_interval;
}

String LoadingScreen::get_current_tip() const {
	return tip_index >= 0 && tip_index < tips.size() ? tips[tip_index] : String();
}

void LoadingScreen::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_progress_bar", "path"), &LoadingScreen::set_progress_bar);
	ClassDB::bind_method(D_METHOD("get_progress_bar"), &LoadingScreen::get_progress_bar);
	ClassDB::bind_method(D_METHOD("set_tip_label", "path"), &LoadingScreen::set_tip_label);
	ClassDB::bind_method(D_METHOD("get_tip_label"), &LoadingScreen::get_tip_label);
	ClassDB::bind_method(D_METHOD("set_tips", "tips"), &LoadingScreen::set_tips);
	ClassDB::bind_method(D_METHOD("get_tips"), &LoadingScreen::get_tips);
	ClassDB::bind_method(D_METHOD("set_tip_interval", "seconds"), &LoadingScreen::set_tip_interval);
	ClassDB::bind_method(D_METHOD("get_tip_interval"), &LoadingScreen::get_tip_interval);
	ClassDB::bind_method(D_METHOD("get_current_tip"), &LoadingScreen::get_current_tip);

	ADD_GROUP("Loading", "");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "progress_bar", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Range"), "set_progress_bar", "get_progress_bar");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "tip_label"), "set_tip_label", "get_tip_label");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "tips"), "set_tips", "get_tips");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tip_interval", PROPERTY_HINT_RANGE, "0,30,0.1,or_greater,suffix:s"), "set_tip_interval", "get_tip_interval");
}

// SplashScreen

String SplashScreen::_get_next_scene_path() const {
	const String path = TransitionScreen::_get_next_scene_path();
	if (!path.is_empty()) {
		return path;
	}
	const String main_scene = GLOBAL_GET("application/run/main_scene");
	return main_scene.is_empty() ? String() : ResourceUID::ensure_path(main_scene);
}

SplashScreen::SplashScreen() {
	set_min_duration(2.0);
	set_fade_in_duration(0.0); // Follows the boot image without a black frame.
	set_skippable(true);
}
