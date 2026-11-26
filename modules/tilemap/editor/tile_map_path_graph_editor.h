/**************************************************************************/
/*  tile_map_path_graph_editor.h                                          */
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

#include "../tile_map_path_graph.h"

#include "scene/gui/box_container.h"

class Button;
class EditorResourcePicker;
class InputEvent;
class TileMapLayer;

// truely-godot-engine: TileMap editor toolbar toggle that overlays a TileMapPathGraph on the edited layer:
// blocked cells, cost shading and a path preview between two Alt+clicked cells.
class TileMapPathGraphEditor : public HBoxContainer {
	GDCLASS(TileMapPathGraphEditor, HBoxContainer);

	Button *toggle_button = nullptr;
	EditorResourcePicker *options_picker = nullptr;
	Ref<TileMapPathGraphOptions> options;
	Ref<TileMapPathGraph> graph;
	bool graph_dirty = true;
	Vector2i path_cells[2];
	int path_cell_count = 0;

	void _toggled(bool p_pressed);
	void _options_picked(const Ref<Resource> &p_options);

protected:
	void _notification(int p_what);

public:
	void queue_rebuild();
	bool forward_canvas_gui_input(const TileMapLayer *p_layer, const Ref<InputEvent> &p_event);
	void draw(Control *p_overlay, TileMapLayer *p_layer, const Transform2D &p_xform);

	TileMapPathGraphEditor();
};
