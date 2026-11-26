/**************************************************************************/
/*  viewport_server.cpp                                                   */
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

#include "viewport_server.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display/display_server_enums.h"

ViewportServer *ViewportServer::singleton = nullptr;

ViewportServer *ViewportServer::get_singleton() {
	return singleton;
}

void ViewportServer::_ensure_tree() {
	// The server exists before the SceneTree (scene singletons are created first), and a new SceneTree
	// (for example one per unit test) replaces the old one: follow whichever tree is current.
	SceneTree *tree = SceneTree::get_singleton();
	const ObjectID current_id = tree != nullptr ? tree->get_instance_id() : ObjectID();
	if (current_id == tree_id) {
		return;
	}
	SceneTree *old_tree = ObjectDB::get_instance<SceneTree>(tree_id);
	if (old_tree != nullptr) {
		old_tree->disconnect(SNAME("node_added"), callable_mp(this, &ViewportServer::_on_node_added));
		old_tree->disconnect(SNAME("node_removed"), callable_mp(this, &ViewportServer::_on_node_removed));
	}
	_clear();
	tree_id = current_id;
	if (tree == nullptr) {
		return;
	}
	tree->connect(SNAME("node_added"), callable_mp(this, &ViewportServer::_on_node_added));
	tree->connect(SNAME("node_removed"), callable_mp(this, &ViewportServer::_on_node_removed));
	// The viewports that are in the tree already (the root window first).
	if (tree->get_root() != nullptr) {
		_scan(tree->get_root());
	}
}

void ViewportServer::_clear() {
	viewports.clear();
	viewports_by_rid.clear();
}

void ViewportServer::_scan(Node *p_node) {
	if (Viewport *viewport = Object::cast_to<Viewport>(p_node)) {
		_register(viewport);
	}
	const int child_count = p_node->get_child_count(true);
	for (int i = 0; i < child_count; i++) {
		_scan(p_node->get_child(i, true));
	}
}

bool ViewportServer::_register(Viewport *p_viewport) {
	const ObjectID id = p_viewport->get_instance_id();
	if (viewports.has(id)) {
		return false;
	}
	const RID rid = p_viewport->get_viewport_rid();
	viewports.insert(id, rid);
	if (rid.is_valid()) {
		viewports_by_rid.insert(rid, id);
	}
	return true;
}

bool ViewportServer::_unregister(Viewport *p_viewport) {
	HashMap<ObjectID, RID>::Iterator it = viewports.find(p_viewport->get_instance_id());
	if (!it) {
		return false;
	}
	if (it->value.is_valid()) {
		viewports_by_rid.erase(it->value);
	}
	viewports.remove(it);
	return true;
}

void ViewportServer::_on_node_added(Node *p_node) {
	Viewport *viewport = Object::cast_to<Viewport>(p_node);
	if (viewport != nullptr && _register(viewport)) {
		emit_signal(SNAME("viewport_added"), viewport);
	}
}

void ViewportServer::_on_node_removed(Node *p_node) {
	Viewport *viewport = Object::cast_to<Viewport>(p_node);
	if (viewport != nullptr && _unregister(viewport)) {
		emit_signal(SNAME("viewport_removed"), viewport);
	}
}

TypedArray<Viewport> ViewportServer::get_viewports() {
	_ensure_tree();
	TypedArray<Viewport> result;
	for (const KeyValue<ObjectID, RID> &kv : viewports) {
		Viewport *viewport = ObjectDB::get_instance<Viewport>(kv.key);
		if (viewport != nullptr) {
			result.push_back(viewport);
		}
	}
	return result;
}

TypedArray<Window> ViewportServer::get_windows() {
	_ensure_tree();
	TypedArray<Window> result;
	for (const KeyValue<ObjectID, RID> &kv : viewports) {
		Window *window = ObjectDB::get_instance<Window>(kv.key);
		if (window != nullptr) {
			result.push_back(window);
		}
	}
	return result;
}

int ViewportServer::get_viewport_count() {
	_ensure_tree();
	int count = 0;
	for (const KeyValue<ObjectID, RID> &kv : viewports) {
		if (ObjectDB::get_instance(kv.key) != nullptr) {
			count++;
		}
	}
	return count;
}

bool ViewportServer::has_viewport(Viewport *p_viewport) {
	_ensure_tree();
	return p_viewport != nullptr && viewports.has(p_viewport->get_instance_id());
}

Viewport *ViewportServer::get_viewport_by_rid(const RID &p_rid) {
	_ensure_tree();
	const ObjectID *id = viewports_by_rid.getptr(p_rid);
	return id != nullptr ? ObjectDB::get_instance<Viewport>(*id) : nullptr;
}

Window *ViewportServer::get_window_by_id(int p_window_id) {
	_ensure_tree();
	if (p_window_id == DisplayServerEnums::INVALID_WINDOW_ID) {
		return nullptr;
	}
	for (const KeyValue<ObjectID, RID> &kv : viewports) {
		Window *window = ObjectDB::get_instance<Window>(kv.key);
		// An embedded window reports the ID of its embedder's window: only the window that owns the ID matches.
		if (window != nullptr && !window->is_embedded() && window->get_window_id() == p_window_id) {
			return window;
		}
	}
	return nullptr;
}

void ViewportServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_viewports"), &ViewportServer::get_viewports);
	ClassDB::bind_method(D_METHOD("get_windows"), &ViewportServer::get_windows);
	ClassDB::bind_method(D_METHOD("get_viewport_count"), &ViewportServer::get_viewport_count);
	ClassDB::bind_method(D_METHOD("has_viewport", "viewport"), &ViewportServer::has_viewport);
	ClassDB::bind_method(D_METHOD("get_viewport_by_rid", "rid"), &ViewportServer::get_viewport_by_rid);
	ClassDB::bind_method(D_METHOD("get_window_by_id", "window_id"), &ViewportServer::get_window_by_id);

	ADD_SIGNAL(MethodInfo("viewport_added", PropertyInfo(Variant::OBJECT, "viewport", PROPERTY_HINT_RESOURCE_TYPE, Viewport::get_class_static())));
	ADD_SIGNAL(MethodInfo("viewport_removed", PropertyInfo(Variant::OBJECT, "viewport", PROPERTY_HINT_RESOURCE_TYPE, Viewport::get_class_static())));
}

ViewportServer::ViewportServer() {
	singleton = this;
	// The SceneTree does not exist yet; the first message queue flush happens in the first frame, when it does.
	callable_mp(this, &ViewportServer::_ensure_tree).call_deferred();
}

ViewportServer::~ViewportServer() {
	singleton = nullptr;
}
