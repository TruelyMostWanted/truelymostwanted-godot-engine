/**************************************************************************/
/*  progress_bar_server.cpp                                               */
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

#include "progress_bar_server.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/canvas_item.h"
#include "scene/resources/theme.h"
#include "scene/scene_string_names.h"
#include "scene/theme/theme_db.h"
#include "servers/rendering/rendering_server.h"

ProgressBarServer *ProgressBarServer::singleton = nullptr;

ProgressBarServer *ProgressBarServer::get_singleton() {
	return singleton;
}

// A missing StyleBox of a style falls back to the ProgressBar theme item (project theme first), so a new
// ProgressBarStyle looks like a ProgressBar.
static Ref<StyleBox> _get_style_box(const Ref<StyleBox> &p_style_box, const StringName &p_theme_name) {
	if (p_style_box.is_valid()) {
		return p_style_box;
	}
	const StringName type = SNAME("ProgressBar");
	Ref<Theme> theme = ThemeDB::get_singleton()->get_project_theme();
	if (theme.is_null() || !theme->has_stylebox(p_theme_name, type)) {
		theme = ThemeDB::get_singleton()->get_default_theme();
	}
	return theme.is_valid() ? theme->get_stylebox(p_theme_name, type) : Ref<StyleBox>();
}

Transform2D ProgressBarServer::get_upright_transform(const Transform2D &p_parent_global) {
	Transform2D basis = p_parent_global;
	basis.columns[2] = Vector2();
	if (Math::is_zero_approx(basis.determinant())) {
		return Transform2D();
	}
	return basis.affine_inverse();
}

void ProgressBarServer::_redraw(const RID &p_bar, const Bar &p_data) {
	RenderingServer *rs = RenderingServer::get_singleton();
	const ProgressBarStyle *style = p_data.style.ptr();
	const float ratio = p_data.ratio;
	const bool shown = p_data.visible && !(style->is_hide_when_full() && ratio >= 1.0f) && !(style->is_hide_when_empty() && ratio <= 0.0f);
	rs->canvas_item_set_visible(p_bar, shown);
	if (!shown) {
		return; // Redrawn when it is shown again.
	}
	rs->canvas_item_clear(p_bar);

	const Size2 size = style->get_size();
	const Rect2 rect(style->get_offset() + p_data.offset - size * 0.5f, size);
	Ref<StyleBox> background = _get_style_box(style->get_background(), SNAME("background"));
	if (background.is_valid()) {
		background->draw(p_bar, rect);
	}
	Ref<StyleBox> fill = _get_style_box(style->get_fill(), SNAME("fill"));
	if (fill.is_null() || ratio <= 0.0f) {
		return;
	}
	// Like ProgressBar: the fill keeps the minimum size of its StyleBox.
	const Size2 min_size = fill->get_minimum_size();
	Rect2 fill_rect = rect;
	switch (style->get_fill_mode()) {
		case ProgressBar::FILL_BEGIN_TO_END:
		case ProgressBar::FILL_END_TO_BEGIN: {
			fill_rect.size.width = min_size.width + ratio * (size.width - min_size.width);
			if (style->get_fill_mode() == ProgressBar::FILL_END_TO_BEGIN) {
				fill_rect.position.x += size.width - fill_rect.size.width;
			}
		} break;
		case ProgressBar::FILL_TOP_TO_BOTTOM:
		case ProgressBar::FILL_BOTTOM_TO_TOP: {
			fill_rect.size.height = min_size.height + ratio * (size.height - min_size.height);
			if (style->get_fill_mode() == ProgressBar::FILL_BOTTOM_TO_TOP) {
				fill_rect.position.y += size.height - fill_rect.size.height;
			}
		} break;
		case ProgressBar::FILL_MODE_MAX:
			break;
	}
	fill->draw(p_bar, fill_rect);
}

void ProgressBarServer::_apply_style(const RID &p_bar, Bar &r_data) {
	if (r_data.style->get_follow_mode() == ProgressBarStyle::FOLLOW_MODE_POSITION_ONLY) {
		position_only_bars.insert(p_bar);
	} else if (position_only_bars.erase(p_bar)) {
		r_data.transform = Transform2D();
		RenderingServer::get_singleton()->canvas_item_set_transform(p_bar, Transform2D());
	}
	_redraw(p_bar, r_data);
}

void ProgressBarServer::_add_style_user(const Ref<ProgressBarStyle> &p_style) {
	uint32_t &count = styles[p_style->get_instance_id()];
	if (count++ == 0) {
		p_style->connect_changed(callable_mp(this, &ProgressBarServer::_on_style_changed).bind(uint64_t(p_style->get_instance_id())));
	}
}

void ProgressBarServer::_remove_style_user(const Ref<ProgressBarStyle> &p_style) {
	HashMap<ObjectID, uint32_t>::Iterator it = styles.find(p_style->get_instance_id());
	if (it && --it->value == 0) {
		styles.remove(it);
		p_style->disconnect_changed(callable_mp(this, &ProgressBarServer::_on_style_changed));
	}
}

void ProgressBarServer::_free_bar(RID p_bar) {
	HashMap<RID, Bar>::Iterator it = bars.find(p_bar);
	if (!it) {
		return;
	}
	_remove_style_user(it->value.style);
	const ObjectID parent_id = it->value.parent;
	HashMap<ObjectID, LocalVector<RID>>::Iterator parent_it = parents.find(parent_id);
	if (parent_it) {
		parent_it->value.erase(p_bar);
		if (parent_it->value.is_empty()) {
			parents.remove(parent_it);
			detached_parents.erase(parent_id);
			Node *parent = ObjectDB::get_instance<Node>(parent_id);
			if (parent != nullptr) {
				parent->disconnect(SceneStringName(tree_exited), callable_mp(this, &ProgressBarServer::_on_parent_exited));
			}
		}
	}
	position_only_bars.erase(p_bar);
	bars.remove(it);
	RenderingServer::get_singleton()->free_rid(p_bar);
}

void ProgressBarServer::_update_processing() {
	RenderingServer *rs = RenderingServer::get_singleton();
	const bool needed = !position_only_bars.is_empty() || !detached_parents.is_empty();
	if (needed == processing || rs == nullptr) {
		return;
	}
	processing = needed;
	if (needed) {
		rs->connect(SNAME("frame_pre_draw"), callable_mp(this, &ProgressBarServer::process));
	} else {
		rs->disconnect(SNAME("frame_pre_draw"), callable_mp(this, &ProgressBarServer::process));
	}
}

void ProgressBarServer::_on_parent_exited(uint64_t p_parent) {
	detached_parents.insert(ObjectID(p_parent));
	_update_processing();
}

void ProgressBarServer::_on_style_changed(uint64_t p_style) {
	const ObjectID style_id(p_style);
	for (KeyValue<RID, Bar> &kv : bars) {
		if (kv.value.style->get_instance_id() == style_id) {
			_apply_style(kv.key, kv.value);
		}
	}
	_update_processing();
}

void ProgressBarServer::process() {
	// Parents that left the tree: free the bars of deleted ones, forget the ones that came back.
	if (!detached_parents.is_empty()) {
		scratch.clear();
		for (const ObjectID &id : detached_parents) {
			Node *parent = ObjectDB::get_instance<Node>(id);
			if (parent == nullptr || parent->is_inside_tree()) {
				scratch.push_back(id);
			}
		}
		for (const ObjectID &id : scratch) {
			detached_parents.erase(id);
			if (ObjectDB::get_instance(id) != nullptr) {
				continue;
			}
			// Each _free_bar() removes the bar from the list; the last one removes the parent.
			while (parents.has(id)) {
				_free_bar(parents[id][0]);
			}
		}
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	for (const RID &rid : position_only_bars) {
		Bar &bar = bars[rid];
		const CanvasItem *parent = ObjectDB::get_instance<CanvasItem>(bar.parent);
		if (parent == nullptr) {
			continue;
		}
		const Transform2D transform = get_upright_transform(parent->get_global_transform());
		if (transform != bar.transform) {
			bar.transform = transform;
			rs->canvas_item_set_transform(rid, transform);
		}
	}
	_update_processing();
}

RID ProgressBarServer::bar_create(CanvasItem *p_parent, const Ref<ProgressBarStyle> &p_style) {
	ERR_FAIL_NULL_V(p_parent, RID());
	ERR_FAIL_COND_V(p_style.is_null(), RID());
	RenderingServer *rs = RenderingServer::get_singleton();
	const RID rid = rs->canvas_item_create();
	rs->canvas_item_set_parent(rid, p_parent->get_canvas_item());

	const ObjectID parent_id = p_parent->get_instance_id();
	HashMap<ObjectID, LocalVector<RID>>::Iterator parent_it = parents.find(parent_id);
	if (!parent_it) {
		parent_it = parents.insert(parent_id, LocalVector<RID>());
		p_parent->connect(SceneStringName(tree_exited), callable_mp(this, &ProgressBarServer::_on_parent_exited).bind(uint64_t(parent_id)));
		if (!p_parent->is_inside_tree()) {
			detached_parents.insert(parent_id);
		}
	}
	parent_it->value.push_back(rid);

	Bar &bar = bars[rid];
	bar.parent = parent_id;
	bar.style = p_style;
	_add_style_user(p_style);
	_apply_style(rid, bar);
	_update_processing();
	return rid;
}

void ProgressBarServer::bar_free(const RID &p_bar) {
	ERR_FAIL_COND_MSG(!bars.has(p_bar), "Invalid progress bar RID.");
	_free_bar(p_bar);
	_update_processing();
}

void ProgressBarServer::bar_set_ratio(const RID &p_bar, float p_ratio) {
	Bar *bar = bars.getptr(p_bar);
	ERR_FAIL_NULL_MSG(bar, "Invalid progress bar RID.");
	p_ratio = CLAMP(p_ratio, 0.0f, 1.0f);
	if (bar->ratio != p_ratio) {
		bar->ratio = p_ratio;
		_redraw(p_bar, *bar);
	}
}

float ProgressBarServer::bar_get_ratio(const RID &p_bar) const {
	const Bar *bar = bars.getptr(p_bar);
	ERR_FAIL_NULL_V_MSG(bar, 0.0f, "Invalid progress bar RID.");
	return bar->ratio;
}

void ProgressBarServer::bar_set_style(const RID &p_bar, const Ref<ProgressBarStyle> &p_style) {
	Bar *bar = bars.getptr(p_bar);
	ERR_FAIL_NULL_MSG(bar, "Invalid progress bar RID.");
	ERR_FAIL_COND(p_style.is_null());
	if (bar->style == p_style) {
		return;
	}
	_add_style_user(p_style);
	_remove_style_user(bar->style);
	bar->style = p_style;
	_apply_style(p_bar, *bar);
	_update_processing();
}

void ProgressBarServer::bar_set_visible(const RID &p_bar, bool p_visible) {
	Bar *bar = bars.getptr(p_bar);
	ERR_FAIL_NULL_MSG(bar, "Invalid progress bar RID.");
	if (bar->visible != p_visible) {
		bar->visible = p_visible;
		_redraw(p_bar, *bar);
	}
}

void ProgressBarServer::bar_set_offset(const RID &p_bar, const Vector2 &p_offset) {
	Bar *bar = bars.getptr(p_bar);
	ERR_FAIL_NULL_MSG(bar, "Invalid progress bar RID.");
	if (bar->offset != p_offset) {
		bar->offset = p_offset;
		_redraw(p_bar, *bar);
	}
}

void ProgressBarServer::bar_set_modulate(const RID &p_bar, const Color &p_color) {
	ERR_FAIL_COND_MSG(!bars.has(p_bar), "Invalid progress bar RID.");
	RenderingServer::get_singleton()->canvas_item_set_modulate(p_bar, p_color);
}

void ProgressBarServer::bars_set_ratios(const TypedArray<RID> &p_bars, const PackedFloat32Array &p_ratios) {
	ERR_FAIL_COND_MSG(p_bars.size() != p_ratios.size(), "The bar and ratio arrays must have the same size.");
	const float *ratios = p_ratios.ptr();
	for (int i = 0; i < p_bars.size(); i++) {
		bar_set_ratio(p_bars[i], ratios[i]);
	}
}

int ProgressBarServer::get_bar_count() const {
	return bars.size();
}

void ProgressBarServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bar_create", "parent", "style"), &ProgressBarServer::bar_create);
	ClassDB::bind_method(D_METHOD("bar_free", "bar"), &ProgressBarServer::bar_free);
	ClassDB::bind_method(D_METHOD("bar_set_ratio", "bar", "ratio"), &ProgressBarServer::bar_set_ratio);
	ClassDB::bind_method(D_METHOD("bar_get_ratio", "bar"), &ProgressBarServer::bar_get_ratio);
	ClassDB::bind_method(D_METHOD("bar_set_style", "bar", "style"), &ProgressBarServer::bar_set_style);
	ClassDB::bind_method(D_METHOD("bar_set_visible", "bar", "visible"), &ProgressBarServer::bar_set_visible);
	ClassDB::bind_method(D_METHOD("bar_set_offset", "bar", "offset"), &ProgressBarServer::bar_set_offset);
	ClassDB::bind_method(D_METHOD("bar_set_modulate", "bar", "color"), &ProgressBarServer::bar_set_modulate);
	ClassDB::bind_method(D_METHOD("bars_set_ratios", "bars", "ratios"), &ProgressBarServer::bars_set_ratios);
	ClassDB::bind_method(D_METHOD("get_bar_count"), &ProgressBarServer::get_bar_count);
}

ProgressBarServer::ProgressBarServer() {
	singleton = this;
}

ProgressBarServer::~ProgressBarServer() {
	// Bars still alive at shutdown: free their canvas items and the connections to styles and parents.
	if (RenderingServer::get_singleton() != nullptr) {
		while (!bars.is_empty()) {
			_free_bar(bars.begin()->key);
		}
		_update_processing();
	}
	singleton = nullptr;
}
