/**************************************************************************/
/*  tile_map_path_graph_editor.cpp                                        */
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

#include "tile_map_path_graph_editor.h"

#include "../tile_map_layer.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/main/scene_tree.h"

void TileMapPathGraphEditor::_toggled(bool p_pressed) {
	options_picker->set_visible(p_pressed);
	path_cell_count = 0;
	graph_dirty = true;
	CanvasItemEditor::get_singleton()->update_viewport();
}

void TileMapPathGraphEditor::_options_picked(const Ref<Resource> &p_options) {
	const Callable rebuild = callable_mp(this, &TileMapPathGraphEditor::queue_rebuild);
	if (options.is_valid()) {
		options->disconnect_changed(rebuild);
	}
	options = p_options;
	if (options.is_valid()) {
		options->connect_changed(rebuild);
	}
	queue_rebuild();
}

void TileMapPathGraphEditor::queue_rebuild() {
	graph_dirty = true;
	if (toggle_button->is_pressed()) {
		CanvasItemEditor::get_singleton()->update_viewport();
	}
}

bool TileMapPathGraphEditor::forward_canvas_gui_input(const TileMapLayer *p_layer, const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (!toggle_button->is_pressed() || !p_layer || p_layer->get_tile_set().is_null() || mb.is_null() || !mb->is_pressed() || !mb->is_alt_pressed() || mb->get_button_index() != MouseButton::LEFT) {
		return false;
	}
	// Alt+click: the first click sets the start cell, the second the end cell, a third starts a new path.
	const Transform2D xform = CanvasItemEditor::get_singleton()->get_canvas_transform() * p_layer->get_global_transform_with_canvas();
	path_cell_count = path_cell_count % 2 + 1;
	path_cells[path_cell_count - 1] = p_layer->get_tile_set()->local_to_map(xform.affine_inverse().xform(mb->get_position()));
	CanvasItemEditor::get_singleton()->update_viewport();
	return true;
}

void TileMapPathGraphEditor::draw(Control *p_overlay, TileMapLayer *p_layer, const Transform2D &p_xform) {
	if (!toggle_button->is_pressed()) {
		return;
	}
	if (graph.is_null() || graph->get_layer() != p_layer) {
		graph = TileMapPathGraph::create(p_layer, options);
		path_cell_count = 0;
	} else if (graph_dirty) {
		graph->set_options(options);
		graph->update_all();
	}
	graph_dirty = false;

	// Only the cells on screen.
	const Ref<TileSet> tile_set = p_layer->get_tile_set();
	const Transform2D xform_inv = p_xform.affine_inverse();
	const Size2 screen_size = p_overlay->get_size();
	Rect2i screen_rect(tile_set->local_to_map(xform_inv.xform(Vector2())), Size2i());
	screen_rect.expand_to(tile_set->local_to_map(xform_inv.xform(Vector2(0, screen_size.height))));
	screen_rect.expand_to(tile_set->local_to_map(xform_inv.xform(Vector2(screen_size.width, 0))));
	screen_rect.expand_to(tile_set->local_to_map(xform_inv.xform(screen_size)));

	p_overlay->draw_set_transform_matrix(p_xform);
	graph->draw_debug(p_overlay->get_canvas_item(), screen_rect.grow(1));
	p_overlay->draw_set_transform_matrix(Transform2D());

	// The path preview, with the same query as at runtime.
	const Color path_color = SceneTree::get_singleton()->get_debug_paths_color();
	const real_t path_width = SceneTree::get_singleton()->get_debug_paths_width() * EDSCALE;
	for (int i = 0; i < path_cell_count; i++) {
		p_overlay->draw_circle(p_xform.xform(tile_set->map_to_local(path_cells[i])), 3 * path_width, path_color);
	}
	if (path_cell_count == 2) {
		PackedVector2Array points = graph->get_point_path(path_cells[0], path_cells[1]);
		for (int i = 0; i < points.size(); i++) {
			points.set(i, p_xform.xform(points[i]));
		}
		if (points.size() > 1) {
			p_overlay->draw_polyline(points, path_color, path_width);
		}
	}
}

void TileMapPathGraphEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Painted cells and TileSet data edits go through undo/redo; TileMapLayer has no signal for cell changes.
			EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &TileMapPathGraphEditor::queue_rebuild));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			EditorUndoRedoManager::get_singleton()->disconnect("version_changed", callable_mp(this, &TileMapPathGraphEditor::queue_rebuild));
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			toggle_button->set_button_icon(get_editor_theme_icon(SNAME("NavigationAgent2D")));
		} break;
	}
}

TileMapPathGraphEditor::TileMapPathGraphEditor() {
	toggle_button = memnew(Button);
	toggle_button->set_theme_type_variation(SceneStringName(FlatButton));
	toggle_button->set_toggle_mode(true);
	toggle_button->set_tooltip_text(TTRC("Show the TileMapPathGraph of the layer: blocked cells and cost shading. Alt+click two cells to preview the path between them."));
	toggle_button->connect(SceneStringName(toggled), callable_mp(this, &TileMapPathGraphEditor::_toggled));
	add_child(toggle_button);

	options_picker = memnew(EditorResourcePicker);
	options_picker->set_base_type("TileMapPathGraphOptions");
	options_picker->set_custom_minimum_size(Size2(120 * EDSCALE, 0));
	options_picker->set_tooltip_text(TTRC("Options of the previewed TileMapPathGraph. Empty uses the default options."));
	options_picker->connect("resource_changed", callable_mp(this, &TileMapPathGraphEditor::_options_picked));
	options_picker->hide();
	add_child(options_picker);
}
