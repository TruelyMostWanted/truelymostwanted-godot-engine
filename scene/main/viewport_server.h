/**************************************************************************/
/*  viewport_server.h                                                     */
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

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/rid.h"
#include "core/variant/typed_array.h"

class Node;
class Viewport;
class Window;

// Registry of every Viewport and Window in the scene tree. Follows SceneTree.node_added/node_removed,
// without changes to Viewport or Window; viewports outside the tree are not listed.
class ViewportServer : public Object {
	GDCLASS(ViewportServer, Object);

	static ViewportServer *singleton;

	// The SceneTree the registry follows; the registry is rebuilt when SceneTree::get_singleton() changes.
	ObjectID tree_id;
	// The registered viewports in the order they entered the tree, with their RID.
	HashMap<ObjectID, RID> viewports;
	HashMap<RID, ObjectID> viewports_by_rid;

	void _ensure_tree();
	void _clear();
	void _scan(Node *p_node);
	bool _register(Viewport *p_viewport);
	bool _unregister(Viewport *p_viewport);
	void _on_node_added(Node *p_node);
	void _on_node_removed(Node *p_node);

protected:
	static void _bind_methods();

public:
	static ViewportServer *get_singleton();

	TypedArray<Viewport> get_viewports();
	TypedArray<Window> get_windows();
	int get_viewport_count();
	bool has_viewport(Viewport *p_viewport);
	Viewport *get_viewport_by_rid(const RID &p_rid);
	Window *get_window_by_id(int p_window_id);

	ViewportServer();
	~ViewportServer();
};
