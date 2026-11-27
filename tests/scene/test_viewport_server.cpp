/**************************************************************************/
/*  test_viewport_server.cpp                                              */
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

TEST_FORCE_LINK(test_viewport_server)

#include "core/config/engine.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/viewport_server.h"
#include "scene/main/window.h"
#include "servers/display/display_server_enums.h"
#include "tests/signal_watcher.h"

namespace TestViewportServer {

TEST_CASE("[SceneTree][ViewportServer] Singleton") {
	CHECK(ViewportServer::get_singleton() != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("ViewportServer"));
}

TEST_CASE("[SceneTree][ViewportServer] Initial scan and lookups") {
	ViewportServer *server = ViewportServer::get_singleton();
	Window *root = SceneTree::get_singleton()->get_root();

	// Added before the server looked at this SceneTree (a new one per test case): found by the initial scan.
	Node *holder = memnew(Node);
	SubViewport *early = memnew(SubViewport);
	holder->add_child(early);
	root->add_child(holder);

	CHECK(server->has_viewport(root));
	CHECK(server->has_viewport(early));
	CHECK_FALSE(server->has_viewport(nullptr));

	const TypedArray<Viewport> viewports = server->get_viewports();
	REQUIRE(viewports.size() >= 2);
	CHECK(viewports[0] == Variant(root));
	CHECK(viewports.has(early));
	CHECK(server->get_viewport_count() == viewports.size());

	const TypedArray<Window> windows = server->get_windows();
	CHECK(windows.has(root));
	bool early_in_windows = false; // `has()` on a TypedArray<Window> rejects a SubViewport with an error.
	for (const Variant &window : windows) {
		early_in_windows = early_in_windows || window == Variant(early);
	}
	CHECK_FALSE(early_in_windows);

	REQUIRE(early->get_viewport_rid().is_valid());
	CHECK(server->get_viewport_by_rid(early->get_viewport_rid()) == early);
	CHECK(server->get_viewport_by_rid(root->get_viewport_rid()) == root);
	CHECK(server->get_viewport_by_rid(RID()) == nullptr);

	CHECK(server->get_window_by_id(DisplayServerEnums::MAIN_WINDOW_ID) == root);
	CHECK(server->get_window_by_id(DisplayServerEnums::INVALID_WINDOW_ID) == nullptr);
	CHECK(server->get_window_by_id(123456) == nullptr);

	const int count = server->get_viewport_count();
	const RID early_rid = early->get_viewport_rid();
	memdelete(holder);
	CHECK(server->get_viewport_count() == count - 1);
	CHECK(server->get_viewport_by_rid(early_rid) == nullptr);
}

TEST_CASE("[SceneTree][ViewportServer] Signals on add and remove") {
	ViewportServer *server = ViewportServer::get_singleton();
	Window *root = SceneTree::get_singleton()->get_root();
	const int count = server->get_viewport_count(); // Starts following this SceneTree.

	SIGNAL_WATCH(server, "viewport_added");
	SIGNAL_WATCH(server, "viewport_removed");

	SubViewport *sub = memnew(SubViewport);
	root->add_child(sub);
	{
		const Array args = { { sub } };
		SIGNAL_CHECK("viewport_added", args);
	}
	CHECK(server->has_viewport(sub));
	CHECK(server->get_viewport_count() == count + 1);

	// A viewport inside an added subtree.
	Node *holder = memnew(Node);
	SubViewport *nested = memnew(SubViewport);
	holder->add_child(nested);
	root->add_child(holder);
	{
		const Array args = { { nested } };
		SIGNAL_CHECK("viewport_added", args);
	}
	CHECK(server->get_viewport_count() == count + 2);

	// Not a viewport: no signal.
	Node *plain = memnew(Node);
	root->add_child(plain);
	SIGNAL_CHECK_FALSE("viewport_added");
	memdelete(plain);
	SIGNAL_CHECK_FALSE("viewport_removed");

	root->remove_child(sub);
	{
		const Array args = { { sub } };
		SIGNAL_CHECK("viewport_removed", args);
	}
	CHECK_FALSE(server->has_viewport(sub));

	// Back into the tree: registered again.
	root->add_child(sub);
	SIGNAL_DISCARD("viewport_added");
	CHECK(server->has_viewport(sub));

	{
		const Array args = { { nested } }; // Built before `nested` is freed.
		memdelete(holder);
		SIGNAL_CHECK("viewport_removed", args);
	}
	memdelete(sub);
	SIGNAL_DISCARD("viewport_removed");
	CHECK(server->get_viewport_count() == count);

	SIGNAL_UNWATCH(server, "viewport_added");
	SIGNAL_UNWATCH(server, "viewport_removed");
}

TEST_CASE("[SceneTree][ViewportServer] Windows") {
	ViewportServer *server = ViewportServer::get_singleton();
	Window *root = SceneTree::get_singleton()->get_root();

	const int window_count = server->get_windows().size();

	Window *w = memnew(Window);
	root->add_child(w);
	CHECK(server->get_windows().size() == window_count + 1);
	CHECK(server->has_viewport(w));
	CHECK(server->get_windows().has(w));
	CHECK(server->get_viewports().has(w));
	CHECK(server->get_viewport_by_rid(w->get_viewport_rid()) == w);
	if (w->is_embedded()) {
		// An embedded window reports its embedder's window ID: the ID belongs to the root window.
		CHECK(server->get_window_by_id(w->get_window_id()) == root);
	} else if (w->get_window_id() != DisplayServerEnums::INVALID_WINDOW_ID) {
		CHECK(server->get_window_by_id(w->get_window_id()) == w);
	}

	memdelete(w);
	CHECK(server->get_windows().size() == window_count);
}

} // namespace TestViewportServer
