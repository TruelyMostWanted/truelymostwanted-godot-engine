/**************************************************************************/
/*  progress_bar_server.h                                                 */
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
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/variant/typed_array.h"
#include "scene/resources/progress_bar_style.h"

class CanvasItem;

// World-space progress bars (#285), for example health bars over many units. Each bar is one canvas item
// parented to the canvas item of its unit, so it inherits the unit's transform without per-frame work. A bar is
// redrawn only when its ratio, visibility, offset or style changes. Only bars with
// ProgressBarStyle.FOLLOW_MODE_POSITION_ONLY and parents outside the tree are looked at once per frame.
// The bar RID is the RID of its canvas item. Use from the main thread.
class ProgressBarServer : public Object {
	GDCLASS(ProgressBarServer, Object);

	static ProgressBarServer *singleton;

	struct Bar {
		ObjectID parent;
		Ref<ProgressBarStyle> style;
		Vector2 offset;
		Transform2D transform; // Last transform sent to the RenderingServer (position-only bars).
		float ratio = 1.0f;
		bool visible = true;
	};

	HashMap<RID, Bar> bars;
	// The bars of each parent; one `tree_exited` connection per parent.
	HashMap<ObjectID, LocalVector<RID>> parents;
	// Parents outside the scene tree: checked once per frame, their bars are freed when they are deleted.
	HashSet<ObjectID> detached_parents;
	// Number of bars per style; one `changed` connection per style.
	HashMap<ObjectID, uint32_t> styles;
	HashSet<RID> position_only_bars;
	LocalVector<ObjectID> scratch; // Reused every frame, no allocation.
	bool upright_connected = false; // Connected to RenderingServer.frame_pre_draw.
	ObjectID detached_tree; // The SceneTree whose process_frame is connected.

	void _redraw(const RID &p_bar, const Bar &p_data);
	void _apply_style(const RID &p_bar, Bar &r_data);
	void _add_style_user(const Ref<ProgressBarStyle> &p_style);
	void _remove_style_user(const Ref<ProgressBarStyle> &p_style);
	void _free_bar(RID p_bar); // By value: callers pass references into containers it changes.
	void _update_processing();
	void _free_detached();
	void _update_upright();
	void _on_parent_exited(uint64_t p_parent);
	void _on_style_changed(uint64_t p_style);

protected:
	static void _bind_methods();

public:
	static ProgressBarServer *get_singleton();

	// Local transform of a position-only bar: undoes the parent's rotation, scale and skew, keeps its position.
	static Transform2D get_upright_transform(const Transform2D &p_parent_global);

	RID bar_create(CanvasItem *p_parent, const Ref<ProgressBarStyle> &p_style);
	void bar_free(const RID &p_bar);
	void bar_set_ratio(const RID &p_bar, float p_ratio);
	float bar_get_ratio(const RID &p_bar) const;
	void bar_set_style(const RID &p_bar, const Ref<ProgressBarStyle> &p_style);
	void bar_set_visible(const RID &p_bar, bool p_visible);
	void bar_set_offset(const RID &p_bar, const Vector2 &p_offset);
	void bar_set_modulate(const RID &p_bar, const Color &p_color);
	void bars_set_ratios(const TypedArray<RID> &p_bars, const PackedFloat32Array &p_ratios);
	int get_bar_count() const;

	// What runs once per frame while needed (SceneTree.process_frame, RenderingServer.frame_pre_draw); for the unit tests.
	void process();

	ProgressBarServer();
	~ProgressBarServer();
};
