/**************************************************************************/
/*  test_progress_bar_server.cpp                                          */
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

TEST_FORCE_LINK(test_progress_bar_server)

#include "core/config/engine.h"
#include "core/core_string_names.h"
#include "scene/2d/node_2d.h"
#include "scene/main/progress_bar_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "tests/signal_watcher.h"

namespace TestProgressBarServer {

TEST_CASE("[SceneTree][ProgressBarServer] Create, set ratios and free bars") {
	ProgressBarServer *server = ProgressBarServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("ProgressBarServer"));
	const int base_count = server->get_bar_count();

	Node2D *unit = memnew(Node2D);
	SceneTree::get_singleton()->get_root()->add_child(unit);
	Ref<ProgressBarStyle> style;
	style.instantiate();
	style->set_fill(memnew(StyleBoxFlat));

	const RID bar = server->bar_create(unit, style);
	REQUIRE(bar.is_valid());
	CHECK(server->get_bar_count() == base_count + 1);
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(1.0));

	server->bar_set_ratio(bar, 0.25f);
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(0.25));
	server->bar_set_ratio(bar, 3.0f);
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(1.0));
	server->bar_set_ratio(bar, -1.0f);
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(0.0));

	// Bulk update.
	const RID second = server->bar_create(unit, style);
	TypedArray<RID> both;
	both.push_back(bar);
	both.push_back(second);
	PackedFloat32Array ratios;
	ratios.push_back(0.5f);
	ratios.push_back(0.75f);
	server->bars_set_ratios(both, ratios);
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(0.5));
	CHECK(server->bar_get_ratio(second) == doctest::Approx(0.75));

	ERR_PRINT_OFF;
	ratios.push_back(0.1f);
	server->bars_set_ratios(both, ratios); // Size mismatch: nothing changes.
	CHECK(server->bar_create(nullptr, style).is_null());
	CHECK(server->bar_create(unit, Ref<ProgressBarStyle>()).is_null());
	server->bar_free(RID());
	ERR_PRINT_ON;
	CHECK(server->bar_get_ratio(bar) == doctest::Approx(0.5));

	// Style changes and setters keep the bar valid; a new style without StyleBoxes falls back to the theme.
	style->set_fill_mode(ProgressBar::FILL_BOTTOM_TO_TOP);
	style->set_hide_when_full(true);
	Ref<ProgressBarStyle> other;
	other.instantiate();
	server->bar_set_style(second, other);
	server->bar_set_visible(bar, false);
	server->bar_set_offset(bar, Vector2(0, -8));
	server->bar_set_modulate(bar, Color(1, 0, 0));
	CHECK(server->get_bar_count() == base_count + 2);

	server->bar_free(bar);
	server->bar_free(second);
	CHECK(server->get_bar_count() == base_count);
	memdelete(unit);
}

TEST_CASE("[SceneTree][ProgressBarServer] Freeing the parent frees its bars") {
	ProgressBarServer *server = ProgressBarServer::get_singleton();
	const int base_count = server->get_bar_count();
	Ref<ProgressBarStyle> style;
	style.instantiate();

	Node2D *unit = memnew(Node2D);
	SceneTree::get_singleton()->get_root()->add_child(unit);
	server->bar_create(unit, style);
	server->bar_create(unit, style);
	CHECK(server->get_bar_count() == base_count + 2);

	// Removed from the tree but alive: the bars stay.
	SceneTree::get_singleton()->get_root()->remove_child(unit);
	server->process();
	CHECK(server->get_bar_count() == base_count + 2);

	// Deleted outside the tree: freed on the next frame.
	memdelete(unit);
	server->process();
	CHECK(server->get_bar_count() == base_count);

	// Deleted inside the tree, and a parent that never was in the tree.
	Node2D *in_tree = memnew(Node2D);
	SceneTree::get_singleton()->get_root()->add_child(in_tree);
	Node2D *outside = memnew(Node2D);
	server->bar_create(in_tree, style);
	server->bar_create(outside, style);
	memdelete(in_tree);
	memdelete(outside);
	server->process();
	CHECK(server->get_bar_count() == base_count);
}

TEST_CASE("[ProgressBarServer] Upright transform of position-only bars") {
	CHECK(ProgressBarServer::get_upright_transform(Transform2D()).is_equal_approx(Transform2D()));
	// The bar keeps the parent's position (zero local origin) but undoes rotation and scale.
	const Transform2D parent(Math::PI / 3.0, Size2(2, 3), 0.0, Vector2(100, 50));
	const Transform2D local = ProgressBarServer::get_upright_transform(parent);
	const Transform2D global = parent * local;
	CHECK(global.get_origin().is_equal_approx(Vector2(100, 50)));
	CHECK(global.basis_xform(Vector2(1, 0)).is_equal_approx(Vector2(1, 0)));
	CHECK(global.basis_xform(Vector2(0, 1)).is_equal_approx(Vector2(0, 1)));
	// A parent scaled to zero must not produce NaN.
	CHECK(ProgressBarServer::get_upright_transform(Transform2D(0, Size2(0, 0), 0, Vector2(4, 4))).is_equal_approx(Transform2D()));
}

TEST_CASE("[ProgressBarStyle] Defaults and changed signal") {
	Ref<ProgressBarStyle> style;
	style.instantiate();
	CHECK(style->get_size() == Size2(32, 4));
	CHECK(style->get_offset() == Vector2(0, -20));
	CHECK(style->get_fill_mode() == ProgressBar::FILL_BEGIN_TO_END);
	CHECK(style->get_follow_mode() == ProgressBarStyle::FOLLOW_MODE_INHERIT);
	CHECK_FALSE(style->is_hide_when_full());
	CHECK_FALSE(style->is_hide_when_empty());

	SIGNAL_WATCH(*style, CoreStringName(changed));
	const Array empty_args = { {} };
	Ref<StyleBoxFlat> fill;
	fill.instantiate();
	style->set_fill(fill);
	CHECK(style->get_fill() == fill);
	SIGNAL_CHECK("changed", empty_args);
	// Changes inside a StyleBox are forwarded.
	fill->set_bg_color(Color(1, 0, 0));
	SIGNAL_CHECK("changed", empty_args);
	style->set_fill(Ref<StyleBox>());
	SIGNAL_DISCARD("changed");
	fill->set_bg_color(Color(0, 1, 0));
	SIGNAL_CHECK_FALSE("changed");
	SIGNAL_UNWATCH(*style, CoreStringName(changed));

	style->set_follow_mode(ProgressBarStyle::FOLLOW_MODE_POSITION_ONLY);
	CHECK(style->get_follow_mode() == ProgressBarStyle::FOLLOW_MODE_POSITION_ONLY);
	ERR_PRINT_OFF;
	style->set_follow_mode(ProgressBarStyle::FOLLOW_MODE_MAX);
	ERR_PRINT_ON;
	CHECK(style->get_follow_mode() == ProgressBarStyle::FOLLOW_MODE_POSITION_ONLY);
}

} // namespace TestProgressBarServer
