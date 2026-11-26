/**************************************************************************/
/*  tile_map_path_graph.h                                                 */
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

#include "tile_set.h"

#include "core/math/a_star.h"
#include "core/math/a_star_grid_2d.h"
#include "core/object/gdvirtual.gen.h"
#include "core/variant/binder_common.h"

class TileMapLayer;

// Settings of a TileMapPathGraph: neighbors, corner cutting and where costs and blocked cells come from.
class TileMapPathGraphOptions : public Resource {
	GDCLASS(TileMapPathGraphOptions, Resource);

public:
	enum NeighborMode {
		NEIGHBOR_MODE_SIDES,
		NEIGHBOR_MODE_SIDES_AND_CORNERS,
		NEIGHBOR_MODE_MAX,
	};

private:
	NeighborMode neighbor_mode = NEIGHBOR_MODE_SIDES;
	AStarGrid2D::DiagonalMode diagonal_mode = AStarGrid2D::DIAGONAL_MODE_ONLY_IF_NO_OBSTACLES;
	String cost_data_layer;
	String blocked_data_layer;
	real_t default_cost = 1.0;
	bool empty_cells_blocked = true;

protected:
	static void _bind_methods();

public:
	void set_neighbor_mode(NeighborMode p_mode);
	NeighborMode get_neighbor_mode() const;
	void set_diagonal_mode(AStarGrid2D::DiagonalMode p_mode);
	AStarGrid2D::DiagonalMode get_diagonal_mode() const;
	void set_cost_data_layer(const String &p_layer);
	String get_cost_data_layer() const;
	void set_blocked_data_layer(const String &p_layer);
	String get_blocked_data_layer() const;
	void set_default_cost(real_t p_cost);
	real_t get_default_cost() const;
	void set_empty_cells_blocked(bool p_blocked);
	bool is_empty_cells_blocked() const;
};

// Pathfinding graph built from a TileMapLayer. Square and diamond-isometric maps use AStarGrid2D,
// every other shape and layout uses AStar2D with neighbors from TileSet::get_neighbor_cell().
// Not thread-safe, like AStar2D: build and query on one thread.
class TileMapPathGraph : public RefCounted {
	GDCLASS(TileMapPathGraph, RefCounted);

public:
	enum Backend {
		BACKEND_GRID,
		BACKEND_GRAPH,
		BACKEND_MAX,
	};

private:
	Ref<TileMapPathGraphOptions> options;
	ObjectID layer_id;
	Ref<TileSet> tile_set; // Tile set of the last build, for positions.
	Backend backend = BACKEND_GRID;
	Rect2i region; // Cells covered by the graph: the used rect of the layer at the last build.
	int cost_layer_index = -1;
	int blocked_layer_index = -1;
	Ref<AStarGrid2D> astar_grid;
	Ref<AStar2D> astar;

	static Backend _get_backend_for(const Ref<TileSet> &p_tile_set);
	TileMapLayer *_get_layer_or_null() const;
	void _compute_cell(const TileMapLayer *p_layer, const Vector2i &p_cell, bool &r_blocked, real_t &r_cost) const;
	void _apply_cell(const Vector2i &p_cell, bool p_blocked, real_t p_cost);

protected:
	static void _bind_methods();

	GDVIRTUAL2RC(real_t, _get_cell_cost, Vector2i, TileData *)
	GDVIRTUAL2RC(bool, _is_cell_blocked, Vector2i, TileData *)

public:
	static Ref<TileMapPathGraph> create(TileMapLayer *p_layer, const Ref<TileMapPathGraphOptions> &p_options = Ref<TileMapPathGraphOptions>());
	static int64_t cell_to_point_id(const Vector2i &p_cell);
	static Vector2i point_id_to_cell(int64_t p_id);

	void set_options(const Ref<TileMapPathGraphOptions> &p_options);
	Ref<TileMapPathGraphOptions> get_options() const;
	void set_layer(TileMapLayer *p_layer);
	TileMapLayer *get_layer() const;

	Error update_all();
	void update_cells(const TypedArray<Vector2i> &p_cells);

	TypedArray<Vector2i> get_cell_path(const Vector2i &p_from, const Vector2i &p_to, bool p_allow_partial_path = false);
	PackedVector2Array get_point_path(const Vector2i &p_from, const Vector2i &p_to, bool p_allow_partial_path = false);
	bool is_cell_blocked(const Vector2i &p_cell) const;
	real_t get_cell_cost(const Vector2i &p_cell) const;
	Rect2i get_region() const;

	void draw_debug(RID p_canvas_item, const Rect2i &p_cells = Rect2i(), const Color &p_blocked_color = Color(1, 0.2, 0.1, 0.5), const Color &p_cost_color = Color(1, 0.8, 0, 0.5)) const;

	Backend get_backend() const;
	Ref<AStarGrid2D> get_astar_grid() const;
	Ref<AStar2D> get_astar() const;

	TileMapPathGraph();
};

VARIANT_ENUM_CAST(TileMapPathGraphOptions::NeighborMode);
VARIANT_ENUM_CAST(TileMapPathGraph::Backend);
