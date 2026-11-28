/**************************************************************************/
/*  test_transition_screen.cpp                                            */
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

TEST_FORCE_LINK(test_transition_screen)

#include "core/io/dir_access.h"
#include "core/io/preload_list.h"
#include "core/io/resource_database.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "scene/gui/label.h"
#include "scene/gui/progress_bar.h"
#include "scene/main/scene_tree.h"
#include "scene/main/transition_screen.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "tests/display_server_mock.h"
#include "tests/signal_watcher.h"

namespace TestTransitionScreen {

static String _scene_dir() {
	const String dir = OS::get_singleton()->get_cache_path().path_join("test_transition_screen");
	DirAccess::make_dir_recursive_absolute(dir);
	return dir;
}

// Saves a scene whose root is a plain node with the given name; returns its path.
static String _save_scene(const String &p_file, const String &p_root_name) {
	Node *root = memnew(Node);
	root->set_name(p_root_name);
	Ref<PackedScene> packed;
	packed.instantiate();
	packed->pack(root);
	memdelete(root);
	const String path = _scene_dir().path_join(p_file);
	CHECK(ResourceSaver::save(packed, path) == OK);
	return path;
}

// One emission without arguments, for SIGNAL_CHECK (a nested initializer list would be flattened).
static Array _one_emission() {
	Array emissions;
	emissions.push_back(Array());
	return emissions;
}

static bool _is_alive(ObjectID p_id) {
	return ObjectDB::get_instance(p_id) != nullptr;
}

// Runs frames of `p_delta` seconds until the object is freed (or a frame limit is hit); loading may run on threads.
static void _run_until_freed(ObjectID p_id, double p_delta = 0.05) {
	for (int frame = 0; frame < 2000 && _is_alive(p_id); frame++) {
		SceneTree::get_singleton()->process(p_delta);
		OS::get_singleton()->delay_usec(500);
	}
}

static void _run_until_state(TransitionScreen *p_screen, TransitionScreen::State p_state, double p_delta = 0.0) {
	for (int frame = 0; frame < 2000 && p_screen->get_state() < p_state; frame++) {
		SceneTree::get_singleton()->process(p_delta);
		OS::get_singleton()->delay_usec(500);
	}
}

TEST_CASE("[SceneTree][TransitionScreen] Defaults, overrides and clamped durations") {
	TransitionScreen *screen = memnew(TransitionScreen);
	CHECK(screen->get_layer() == 100);
	CHECK(screen->get_process_mode() == Node::PROCESS_MODE_ALWAYS);
	CHECK(screen->get_next_scene().is_empty());
	CHECK(screen->get_min_duration() == doctest::Approx(0.0));
	CHECK(screen->get_fade_in_duration() == doctest::Approx(0.3));
	CHECK(screen->get_fade_out_duration() == doctest::Approx(0.3));
	CHECK_FALSE(screen->is_skippable());
	CHECK(screen->is_auto_finish());
	CHECK_FALSE(screen->is_free_current_scene_before_loading());
	CHECK(screen->get_preload_list().is_null());
	CHECK(screen->get_state() == TransitionScreen::STATE_IDLE);
	CHECK(screen->get_progress() == doctest::Approx(0.0));

	screen->set_min_duration(-1.0);
	screen->set_fade_in_duration(-2.0);
	screen->set_fade_out_duration(-3.0);
	CHECK(screen->get_min_duration() == doctest::Approx(0.0));
	CHECK(screen->get_fade_in_duration() == doctest::Approx(0.0));
	CHECK(screen->get_fade_out_duration() == doctest::Approx(0.0));
	memdelete(screen);

	SplashScreen *splash = memnew(SplashScreen);
	CHECK(splash->get_min_duration() == doctest::Approx(2.0));
	CHECK(splash->get_fade_in_duration() == doctest::Approx(0.0));
	CHECK(splash->is_skippable());
	memdelete(splash);

	LoadingScreen *loading = memnew(LoadingScreen);
	CHECK(loading->get_progress_bar().is_empty());
	CHECK(loading->get_tip_label().is_empty());
	CHECK(loading->get_tips().is_empty());
	CHECK(loading->get_tip_interval() == doctest::Approx(5.0));
	CHECK(loading->get_current_tip().is_empty());
	loading->set_tip_interval(-1.0);
	CHECK(loading->get_tip_interval() == doctest::Approx(0.0));
	memdelete(loading);
}

TEST_CASE("[SceneTree][TransitionScreen] Without a next scene: fade in, wait, fade out, free") {
	TransitionScreen *screen = memnew(TransitionScreen);
	screen->set_fade_in_duration(0.4);
	screen->set_fade_out_duration(0.4);
	screen->set_min_duration(0.2);
	const ObjectID id = screen->get_instance_id();
	SIGNAL_WATCH(screen, "started");
	SIGNAL_WATCH(screen, "loaded");
	SIGNAL_WATCH(screen, "completed");

	SceneTree::get_singleton()->get_root()->add_child(screen);
	CHECK(screen->get_fade() == doctest::Approx(0.0));
	CHECK(screen->get_state() == TransitionScreen::STATE_IDLE);

	SceneTree::get_singleton()->process(0.1); // First frame starts; its delta is not counted.
	SIGNAL_CHECK("started", _one_emission());
	CHECK(screen->get_state() == TransitionScreen::STATE_FADING_IN);

	SceneTree::get_singleton()->process(0.2);
	CHECK(screen->get_fade() == doctest::Approx(0.5));

	SceneTree::get_singleton()->process(0.2);
	CHECK(screen->get_fade() == doctest::Approx(1.0));
	CHECK(screen->get_state() == TransitionScreen::STATE_LOADED);
	CHECK(screen->get_progress() == doctest::Approx(1.0));
	SIGNAL_CHECK("loaded", _one_emission());

	SceneTree::get_singleton()->process(0.1);
	CHECK(screen->get_state() == TransitionScreen::STATE_LOADED); // 0.1 of 0.2 s fully visible.

	SceneTree::get_singleton()->process(0.1);
	CHECK(screen->get_state() == TransitionScreen::STATE_FADING_OUT);
	CHECK(screen->get_fade() == doctest::Approx(1.0));

	SceneTree::get_singleton()->process(0.2);
	CHECK(screen->get_fade() == doctest::Approx(0.5));
	SIGNAL_CHECK_FALSE("completed");

	SceneTree::get_singleton()->process(0.2);
	SIGNAL_CHECK("completed", _one_emission());
	SceneTree::get_singleton()->process(0.0);
	CHECK_FALSE(_is_alive(id)); // Freed: the watcher's connections are gone with it.
}

TEST_CASE("[SceneTree][TransitionScreen] Changes the current scene while fully visible") {
	SceneTree *tree = SceneTree::get_singleton();
	const String path = _save_scene("next.tscn", "NextScene");
	Node *old_scene = memnew(Node);
	old_scene->set_name("OldScene");
	tree->add_current_scene(old_scene);
	const ObjectID old_id = old_scene->get_instance_id();

	TransitionScreen *screen = memnew(TransitionScreen);
	screen->set_next_scene(path);
	screen->set_fade_in_duration(0.0);
	screen->set_fade_out_duration(0.1);
	const ObjectID id = screen->get_instance_id();
	tree->get_root()->add_child(screen);

	_run_until_state(screen, TransitionScreen::STATE_FADING_OUT);
	REQUIRE(screen->get_state() == TransitionScreen::STATE_FADING_OUT);
	CHECK(screen->get_progress() == doctest::Approx(1.0));
	tree->process(0.0); // The new scene is added at the end of the frame of the change.
	REQUIRE(tree->get_current_scene() != nullptr);
	CHECK(tree->get_current_scene()->get_name() == StringName("NextScene"));
	CHECK_FALSE(_is_alive(old_id));
	CHECK(screen->is_inside_tree()); // Still there for the fade-out.

	_run_until_freed(id);
	CHECK_FALSE(_is_alive(id));
	tree->unload_current_scene();
}

TEST_CASE("[SceneTree][TransitionScreen] As the current scene it survives the change; auto_finish waits for finish()") {
	SceneTree *tree = SceneTree::get_singleton();
	const String path = _save_scene("next_manual.tscn", "ManualNext");
	TransitionScreen *screen = memnew(TransitionScreen);
	screen->set_next_scene(path);
	screen->set_fade_in_duration(0.0);
	screen->set_fade_out_duration(0.0);
	screen->set_auto_finish(false);
	const ObjectID id = screen->get_instance_id();
	tree->add_current_scene(screen);

	_run_until_state(screen, TransitionScreen::STATE_LOADED);
	REQUIRE(screen->get_state() == TransitionScreen::STATE_LOADED);
	tree->process(1.0);
	tree->process(1.0);
	CHECK(screen->get_state() == TransitionScreen::STATE_LOADED); // Waits for finish().
	CHECK(tree->get_current_scene() == screen);

	screen->finish();
	tree->process(0.0);
	tree->process(0.0);
	REQUIRE(tree->get_current_scene() != nullptr);
	CHECK(tree->get_current_scene()->get_name() == StringName("ManualNext"));
	CHECK_FALSE(_is_alive(id)); // Fade-out of 0 s: freed right after the change.
	tree->unload_current_scene();
}

TEST_CASE("[SceneTree][TransitionScreen] Skippable: a key press ends min_duration") {
	SceneTree *tree = SceneTree::get_singleton();
	TransitionScreen *screen = memnew(TransitionScreen);
	screen->set_fade_in_duration(0.0);
	screen->set_fade_out_duration(1.0);
	screen->set_min_duration(100.0);
	screen->set_skippable(true);
	tree->get_root()->add_child(screen);
	SIGNAL_WATCH(screen, "skipped");

	tree->process(0.0);
	CHECK(screen->get_state() == TransitionScreen::STATE_LOADED);
	SEND_GUI_KEY_EVENT(Key::A);
	SIGNAL_CHECK("skipped", _one_emission());
	tree->process(0.0);
	CHECK(screen->get_state() == TransitionScreen::STATE_FADING_OUT);

	SIGNAL_UNWATCH(screen, "skipped");
	memdelete(screen);
}

TEST_CASE("[SceneTree][TransitionScreen] Free the current scene before loading; never load its own scene") {
	SceneTree *tree = SceneTree::get_singleton();
	const String path = _save_scene("next_low_memory.tscn", "LowMemoryNext");
	Node *old_scene = memnew(Node);
	tree->add_current_scene(old_scene);
	const ObjectID old_id = old_scene->get_instance_id();

	TransitionScreen *screen = memnew(TransitionScreen);
	screen->set_next_scene(path);
	screen->set_fade_in_duration(0.0);
	screen->set_fade_out_duration(0.0);
	screen->set_free_current_scene_before_loading(true);
	screen->set_auto_finish(false);
	tree->get_root()->add_child(screen);
	tree->process(0.0);
	tree->process(0.0);
	CHECK(tree->get_current_scene() == nullptr);
	CHECK_FALSE(_is_alive(old_id));
	_run_until_state(screen, TransitionScreen::STATE_LOADED);
	const ObjectID id = screen->get_instance_id();
	screen->finish();
	_run_until_freed(id);
	REQUIRE(tree->get_current_scene() != nullptr);
	CHECK(tree->get_current_scene()->get_name() == StringName("LowMemoryNext"));
	tree->unload_current_scene();

	// A screen whose next scene is its own file does not change the scene.
	TransitionScreen *self_loop = memnew(TransitionScreen);
	self_loop->set_scene_file_path(path);
	self_loop->set_next_scene(path);
	self_loop->set_fade_in_duration(0.0);
	self_loop->set_fade_out_duration(0.0);
	const ObjectID loop_id = self_loop->get_instance_id();
	tree->get_root()->add_child(self_loop);
	_run_until_freed(loop_id);
	CHECK(tree->get_current_scene() == nullptr);
}

TEST_CASE("[SceneTree][TransitionScreen] SceneTree.change_scene_with_transition()") {
	SceneTree *tree = SceneTree::get_singleton();
	const String path = _save_scene("next_tree.tscn", "TreeNext");
	Node *old_scene = memnew(Node);
	tree->add_current_scene(old_scene);
	const int child_count = tree->get_root()->get_child_count();

	ERR_PRINT_OFF;
	CHECK(tree->change_scene_with_transition(String(), Ref<PackedScene>()) == ERR_INVALID_PARAMETER);
	Node *not_a_screen = memnew(Node);
	Ref<PackedScene> wrong;
	wrong.instantiate();
	wrong->pack(not_a_screen);
	memdelete(not_a_screen);
	CHECK(tree->change_scene_with_transition(path, wrong) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;

	// Null transition scene: built-in black fade.
	REQUIRE(tree->change_scene_with_transition(path, Ref<PackedScene>()) == OK);
	tree->process(0.0); // Added deferred.
	REQUIRE(tree->get_root()->get_child_count() == child_count + 1);
	TransitionScreen *screen = Object::cast_to<TransitionScreen>(tree->get_root()->get_child(child_count));
	REQUIRE(screen != nullptr);
	CHECK(screen->get_next_scene() == path);
	CHECK(screen->get_child_count() == 1);
	_run_until_freed(screen->get_instance_id());
	REQUIRE(tree->get_current_scene() != nullptr);
	CHECK(tree->get_current_scene()->get_name() == StringName("TreeNext"));
	tree->unload_current_scene();
}

TEST_CASE("[SceneTree][LoadingScreen] Progress bar, tips and the preload list") {
	SceneTree *tree = SceneTree::get_singleton();
	const String path = _save_scene("next_loading.tscn", "LoadingNext");
	const String asset_path = _save_scene("asset.tscn", "Asset");
	ResourceDatabase *database = ResourceDatabase::get_singleton();
	REQUIRE(database != nullptr);
	database->clear();

	LoadingScreen *screen = memnew(LoadingScreen);
	ProgressBar *bar = memnew(ProgressBar);
	bar->set_name("Bar");
	bar->set_max(200.0);
	screen->add_child(bar);
	Label *label = memnew(Label);
	label->set_name("Tip");
	screen->add_child(label);
	screen->set_progress_bar(NodePath("Bar"));
	screen->set_tip_label(NodePath("Tip"));
	screen->set_tips(PackedStringArray({ "first", "second" }));
	screen->set_tip_interval(1.0);
	screen->set_next_scene(path);
	screen->set_fade_in_duration(0.0);
	screen->set_fade_out_duration(10.0);
	screen->set_auto_finish(false);
	Ref<PreloadList> list;
	list.instantiate();
	list->set_paths(PackedStringArray({ asset_path }));
	screen->set_preload_list(list);
	tree->get_root()->add_child(screen);

	CHECK(label->get_text() == "first");
	CHECK(screen->get_current_tip() == "first");
	CHECK(bar->get_value() == doctest::Approx(0.0));
	tree->process(0.6);
	tree->process(0.6);
	CHECK(label->get_text() == "second");
	tree->process(1.0);
	CHECK(label->get_text() == "first");

	_run_until_state(screen, TransitionScreen::STATE_LOADED);
	REQUIRE(screen->get_state() == TransitionScreen::STATE_LOADED);
	CHECK(bar->get_value() == doctest::Approx(200.0));
	CHECK(database->has_resource(asset_path));

	const ObjectID id = screen->get_instance_id();
	screen->finish();
	tree->process(0.0);
	tree->process(0.0);
	REQUIRE(tree->get_current_scene() != nullptr);
	CHECK(tree->get_current_scene()->get_name() == StringName("LoadingNext"));
	memdelete(screen);
	CHECK_FALSE(_is_alive(id));
	tree->unload_current_scene();
	database->clear();
}

} // namespace TestTransitionScreen
