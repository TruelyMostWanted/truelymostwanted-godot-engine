/**************************************************************************/
/*  tile_map_path_graph.cpp                                               */
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

#include "tile_map_path_graph.h"

#include "tile_map_layer.h"

#include "core/object/class_db.h"

// TileMapPathGraphOptions.

void TileMapPathGraphOptions::set_neighbor_mode(NeighborMode p_mode) {
	ERR_FAIL_INDEX(p_mode, NEIGHBOR_MODE_MAX);
	neighbor_mode = p_mode;
	emit_changed();
}

TileMapPathGraphOptions::NeighborMode TileMapPathGraphOptions::get_neighbor_mode() const {
	return neighbor_mode;
}

void TileMapPathGraphOptions::set_diagonal_mode(AStarGrid2D::DiagonalMode p_mode) {
	ERR_FAIL_INDEX(p_mode, AStarGrid2D::DIAGONAL_MODE_MAX);
	diagonal_mode = p_mode;
	emit_changed();
}

AStarGrid2D::DiagonalMode TileMapPathGraphOptions::get_diagonal_mode() const {
	return diagonal_mode;
}

void TileMapPathGraphOptions::set_cost_data_layer(const String &p_layer) {
	cost_data_layer = p_layer;
	emit_changed();
}

String TileMapPathGraphOptions::get_cost_data_layer() const {
	return cost_data_layer;
}

void TileMapPathGraphOptions::set_blocked_data_layer(const String &p_layer) {
	blocked_data_layer = p_layer;
	emit_changed();
}

String TileMapPathGraphOptions::get_blocked_data_layer() const {
	return blocked_data_layer;
}

void TileMapPathGraphOptions::set_default_cost(real_t p_cost) {
	ERR_FAIL_COND_MSG(p_cost < 0, "The default cost can't be negative.");
	default_cost = p_cost;
	emit_changed();
}

real_t TileMapPathGraphOptions::get_default_cost() const {
	return default_cost;
}

void TileMapPathGraphOptions::set_empty_cells_blocked(bool p_blocked) {
	empty_cells_blocked = p_blocked;
	emit_changed();
}

bool TileMapPathGraphOptions::is_empty_cells_blocked() const {
	return empty_cells_blocked;
}

void TileMapPathGraphOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_neighbor_mode", "mode"), &TileMapPathGraphOptions::set_neighbor_mode);
	ClassDB::bind_method(D_METHOD("get_neighbor_mode"), &TileMapPathGraphOptions::get_neighbor_mode);
	ClassDB::bind_method(D_METHOD("set_diagonal_mode", "mode"), &TileMapPathGraphOptions::set_diagonal_mode);
	ClassDB::bind_method(D_METHOD("get_diagonal_mode"), &TileMapPathGraphOptions::get_diagonal_mode);
	ClassDB::bind_method(D_METHOD("set_cost_data_layer", "layer"), &TileMapPathGraphOptions::set_cost_data_layer);
	ClassDB::bind_method(D_METHOD("get_cost_data_layer"), &TileMapPathGraphOptions::get_cost_data_layer);
	ClassDB::bind_method(D_METHOD("set_blocked_data_layer", "layer"), &TileMapPathGraphOptions::set_blocked_data_layer);
	ClassDB::bind_method(D_METHOD("get_blocked_data_layer"), &TileMapPathGraphOptions::get_blocked_data_layer);
	ClassDB::bind_method(D_METHOD("set_default_cost", "cost"), &TileMapPathGraphOptions::set_default_cost);
	ClassDB::bind_method(D_METHOD("get_default_cost"), &TileMapPathGraphOptions::get_default_cost);
	ClassDB::bind_method(D_METHOD("set_empty_cells_blocked", "blocked"), &TileMapPathGraphOptions::set_empty_cells_blocked);
	ClassDB::bind_method(D_METHOD("is_empty_cells_blocked"), &TileMapPathGraphOptions::is_empty_cells_blocked);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "neighbor_mode", PROPERTY_HINT_ENUM, "Sides,Sides and Corners"), "set_neighbor_mode", "get_neighbor_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "diagonal_mode", PROPERTY_HINT_ENUM, "Always,Never,At Least One Walkable,Only If No Obstacles"), "set_diagonal_mode", "get_diagonal_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cost_data_layer"), "set_cost_data_layer", "get_cost_data_layer");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "blocked_data_layer"), "set_blocked_data_layer", "get_blocked_data_layer");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_cost", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater"), "set_default_cost", "get_default_cost");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "empty_cells_blocked"), "set_empty_cells_blocked", "is_empty_cells_blocked");

	BIND_ENUM_CONSTANT(NEIGHBOR_MODE_SIDES);
	BIND_ENUM_CONSTANT(NEIGHBOR_MODE_SIDES_AND_CORNERS);
	BIND_ENUM_CONSTANT(NEIGHBOR_MODE_MAX);
}

// TileMapPathGraph.

static constexpr int CELL_MIN = -32768; // TileMapLayer saves each coordinate as int16.
static constexpr int CELL_MAX = 32767;

TileMapPathGraph::Backend TileMapPathGraph::_get_backend_for(const Ref<TileSet> &p_tile_set) {
	// Square cells ignore the layout. In these cases the sides are the axis neighbors and the corners the
	// diagonals, whatever the coordinate system is, which is the neighborhood of AStarGrid2D.
	const TileSet::TileShape shape = p_tile_set->get_tile_shape();
	const TileSet::TileLayout layout = p_tile_set->get_tile_layout();
	if (shape == TileSet::TILE_SHAPE_SQUARE || (shape == TileSet::TILE_SHAPE_ISOMETRIC && (layout == TileSet::TILE_LAYOUT_DIAMOND_RIGHT || layout == TileSet::TILE_LAYOUT_DIAMOND_DOWN))) {
		return BACKEND_GRID;
	}
	return BACKEND_GRAPH;
}

int64_t TileMapPathGraph::cell_to_point_id(const Vector2i &p_cell) {
	ERR_FAIL_COND_V_MSG(p_cell.x < CELL_MIN || p_cell.x > CELL_MAX || p_cell.y < CELL_MIN || p_cell.y > CELL_MAX, -1, vformat("Cell %s is outside the tile map range (-32768 to 32767 per coordinate).", p_cell));
	return (int64_t(p_cell.x - CELL_MIN) << 16) | int64_t(p_cell.y - CELL_MIN);
}

Vector2i TileMapPathGraph::point_id_to_cell(int64_t p_id) {
	ERR_FAIL_COND_V_MSG(p_id < 0 || p_id > 0xFFFFFFFFLL, Vector2i(), vformat("Point id %d is not a tile map cell id.", p_id));
	return Vector2i(int(p_id >> 16) + CELL_MIN, int(p_id & 0xFFFF) + CELL_MIN);
}

Ref<TileMapPathGraph> TileMapPathGraph::create(TileMapLayer *p_layer, const Ref<TileMapPathGraphOptions> &p_options) {
	Ref<TileMapPathGraph> graph;
	graph.instantiate();
	graph->set_options(p_options);
	graph->set_layer(p_layer);
	if (p_layer) {
		graph->update_all();
	}
	return graph;
}

void TileMapPathGraph::set_options(const Ref<TileMapPathGraphOptions> &p_options) {
	if (p_options.is_valid()) {
		options = p_options;
	} else {
		options.instantiate();
	}
}

Ref<TileMapPathGraphOptions> TileMapPathGraph::get_options() const {
	return options;
}

void TileMapPathGraph::set_layer(TileMapLayer *p_layer) {
	layer_id = p_layer ? p_layer->get_instance_id() : ObjectID();
}

TileMapLayer *TileMapPathGraph::get_layer() const {
	return _get_layer_or_null();
}

TileMapLayer *TileMapPathGraph::_get_layer_or_null() const {
	return ObjectDB::get_instance<TileMapLayer>(layer_id);
}

void TileMapPathGraph::_compute_cell(const TileMapLayer *p_layer, const Vector2i &p_cell, bool &r_blocked, real_t &r_cost) const {
	const bool empty = p_layer->get_cell_source_id(p_cell) == TileSet::INVALID_SOURCE;
	TileData *tile_data = empty ? nullptr : p_layer->get_cell_tile_data(p_cell);

	if (!GDVIRTUAL_CALL(_is_cell_blocked, p_cell, tile_data, r_blocked)) {
		if (empty) {
			r_blocked = options->is_empty_cells_blocked();
		} else {
			r_blocked = tile_data && blocked_layer_index >= 0 && bool(tile_data->get_custom_data_by_layer_id(blocked_layer_index));
		}
	}

	if (!GDVIRTUAL_CALL(_get_cell_cost, p_cell, tile_data, r_cost)) {
		r_cost = options->get_default_cost();
		if (tile_data && cost_layer_index >= 0) {
			// Unpainted custom data is 0, so 0 or less means "not set".
			const real_t cost = tile_data->get_custom_data_by_layer_id(cost_layer_index);
			if (cost > 0) {
				r_cost = cost;
			}
		}
	}
	r_cost = MAX(r_cost, (real_t)0); // AStar rejects negative weights.
}

void TileMapPathGraph::_apply_cell(const Vector2i &p_cell, bool p_blocked, real_t p_cost) {
	if (backend == BACKEND_GRID) {
		astar_grid->set_point_solid(p_cell, p_blocked);
		astar_grid->set_point_weight_scale(p_cell, p_cost);
	} else {
		const int64_t id = cell_to_point_id(p_cell);
		astar->set_point_disabled(id, p_blocked);
		astar->set_point_weight_scale(id, p_cost);
	}
}

Error TileMapPathGraph::update_all() {
	TileMapLayer *layer = _get_layer_or_null();
	ERR_FAIL_NULL_V_MSG(layer, ERR_UNCONFIGURED, "TileMapPathGraph has no TileMapLayer. Call set_layer() first.");
	const Ref<TileSet> layer_tile_set = layer->get_tile_set();
	ERR_FAIL_COND_V_MSG(layer_tile_set.is_null(), ERR_UNCONFIGURED, "The TileMapLayer has no TileSet.");

	const Rect2i used_rect = layer->get_used_rect();
	const Backend new_backend = _get_backend_for(layer_tile_set);
	ERR_FAIL_COND_V_MSG(new_backend == BACKEND_GRAPH && used_rect.has_area() && (used_rect.position.x < CELL_MIN || used_rect.position.y < CELL_MIN || used_rect.get_end().x - 1 > CELL_MAX || used_rect.get_end().y - 1 > CELL_MAX), ERR_PARAMETER_RANGE_ERROR, vformat("The used cells %s are outside the tile map range (-32768 to 32767 per coordinate).", used_rect));

	tile_set = layer_tile_set;
	backend = new_backend;
	region = used_rect;

	// Resolve the custom data layers once per build.
	cost_layer_index = -1;
	blocked_layer_index = -1;
	const String cost_name = options->get_cost_data_layer();
	if (!cost_name.is_empty()) {
		cost_layer_index = tile_set->get_custom_data_layer_by_name(cost_name);
		if (cost_layer_index < 0) {
			WARN_PRINT(vformat("TileMapPathGraph: the TileSet has no custom data layer \"%s\"; default_cost is used.", cost_name));
		}
	}
	const String blocked_name = options->get_blocked_data_layer();
	if (!blocked_name.is_empty()) {
		blocked_layer_index = tile_set->get_custom_data_layer_by_name(blocked_name);
		if (blocked_layer_index < 0) {
			WARN_PRINT(vformat("TileMapPathGraph: the TileSet has no custom data layer \"%s\"; no tile is blocked by data.", blocked_name));
		}
	}

	const bool corners = options->get_neighbor_mode() == TileMapPathGraphOptions::NEIGHBOR_MODE_SIDES_AND_CORNERS;
	const Vector2i end = region.get_end();

	if (backend == BACKEND_GRID) {
		astar.unref();
		if (astar_grid.is_null()) {
			astar_grid.instantiate();
		}
		astar_grid->set_region(region);
		astar_grid->set_diagonal_mode(corners ? options->get_diagonal_mode() : AStarGrid2D::DIAGONAL_MODE_NEVER);
		astar_grid->update();
	} else {
		astar_grid.unref();
		if (astar.is_null()) {
			astar.instantiate();
		}
		astar->clear();
		astar->reserve_space(int64_t(region.size.x) * region.size.y);

		// Even CellNeighbor values are sides, odd values are corners.
		LocalVector<TileSet::CellNeighbor> neighbors;
		for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
			const TileSet::CellNeighbor neighbor = TileSet::CellNeighbor(i);
			if (tile_set->is_existing_neighbor(neighbor) && (corners || i % 2 == 0)) {
				neighbors.push_back(neighbor);
			}
		}

		for (int y = region.position.y; y < end.y; y++) {
			for (int x = region.position.x; x < end.x; x++) {
				const Vector2i cell(x, y);
				astar->add_point(cell_to_point_id(cell), tile_set->map_to_local(cell));
			}
		}
		for (int y = region.position.y; y < end.y; y++) {
			for (int x = region.position.x; x < end.x; x++) {
				const Vector2i cell(x, y);
				const int64_t id = cell_to_point_id(cell);
				for (const TileSet::CellNeighbor neighbor : neighbors) {
					const Vector2i neighbor_cell = tile_set->get_neighbor_cell(cell, neighbor);
					const int64_t neighbor_id = region.has_point(neighbor_cell) ? cell_to_point_id(neighbor_cell) : -1;
					// Neighborhoods are symmetric: connect each pair once, both ways.
					if (neighbor_id > id) {
						astar->connect_points(id, neighbor_id);
					}
				}
			}
		}
	}

	for (int y = region.position.y; y < end.y; y++) {
		for (int x = region.position.x; x < end.x; x++) {
			const Vector2i cell(x, y);
			bool blocked = false;
			real_t cost = 1.0;
			_compute_cell(layer, cell, blocked, cost);
			_apply_cell(cell, blocked, cost);
		}
	}

	emit_signal(SNAME("updated"));
	return OK;
}

void TileMapPathGraph::update_cells(const TypedArray<Vector2i> &p_cells) {
	TileMapLayer *layer = _get_layer_or_null();
	ERR_FAIL_NULL_MSG(layer, "TileMapPathGraph has no TileMapLayer. Call set_layer() first.");
	if (astar_grid.is_null() && astar.is_null()) {
		update_all();
		return;
	}

	for (int i = 0; i < p_cells.size(); i++) {
		const Vector2i cell = p_cells[i];
		if (!region.has_point(cell)) {
			if (layer->get_cell_source_id(cell) != TileSet::INVALID_SOURCE) {
				update_all(); // The map grew beyond the graph.
				return;
			}
			continue;
		}
		bool blocked = false;
		real_t cost = 1.0;
		_compute_cell(layer, cell, blocked, cost);
		_apply_cell(cell, blocked, cost);
	}

	emit_signal(SNAME("updated"));
}

TypedArray<Vector2i> TileMapPathGraph::get_cell_path(const Vector2i &p_from, const Vector2i &p_to, bool p_allow_partial_path) {
	if (!region.has_point(p_from) || !region.has_point(p_to)) {
		return TypedArray<Vector2i>();
	}
	if (astar_grid.is_valid()) {
		return astar_grid->get_id_path(p_from, p_to, p_allow_partial_path);
	}
	TypedArray<Vector2i> path;
	if (astar.is_valid()) {
		const Vector<int64_t> ids = astar->get_id_path(cell_to_point_id(p_from), cell_to_point_id(p_to), p_allow_partial_path);
		path.resize(ids.size());
		for (int i = 0; i < ids.size(); i++) {
			path[i] = point_id_to_cell(ids[i]);
		}
	}
	return path;
}

PackedVector2Array TileMapPathGraph::get_point_path(const Vector2i &p_from, const Vector2i &p_to, bool p_allow_partial_path) {
	const TypedArray<Vector2i> cells = get_cell_path(p_from, p_to, p_allow_partial_path);
	PackedVector2Array points;
	points.resize(cells.size());
	Vector2 *points_ptr = points.ptrw();
	for (int i = 0; i < cells.size(); i++) {
		const Vector2i cell = cells[i];
		points_ptr[i] = tile_set->map_to_local(cell);
	}
	return points;
}

bool TileMapPathGraph::is_cell_blocked(const Vector2i &p_cell) const {
	if (!region.has_point(p_cell)) {
		return true;
	}
	if (astar_grid.is_valid()) {
		return astar_grid->is_point_solid(p_cell);
	}
	if (astar.is_valid()) {
		return astar->is_point_disabled(cell_to_point_id(p_cell));
	}
	return true;
}

real_t TileMapPathGraph::get_cell_cost(const Vector2i &p_cell) const {
	if (!region.has_point(p_cell)) {
		return 0;
	}
	if (astar_grid.is_valid()) {
		return astar_grid->get_point_weight_scale(p_cell);
	}
	if (astar.is_valid()) {
		return astar->get_point_weight_scale(cell_to_point_id(p_cell));
	}
	return 0;
}

Rect2i TileMapPathGraph::get_region() const {
	return region;
}

TileMapPathGraph::Backend TileMapPathGraph::get_backend() const {
	return backend;
}

Ref<AStarGrid2D> TileMapPathGraph::get_astar_grid() const {
	return astar_grid;
}

Ref<AStar2D> TileMapPathGraph::get_astar() const {
	return astar;
}

void TileMapPathGraph::_bind_methods() {
	ClassDB::bind_static_method("TileMapPathGraph", D_METHOD("create", "layer", "options"), &TileMapPathGraph::create, DEFVAL(Ref<TileMapPathGraphOptions>()));
	ClassDB::bind_static_method("TileMapPathGraph", D_METHOD("cell_to_point_id", "cell"), &TileMapPathGraph::cell_to_point_id);
	ClassDB::bind_static_method("TileMapPathGraph", D_METHOD("point_id_to_cell", "id"), &TileMapPathGraph::point_id_to_cell);

	ClassDB::bind_method(D_METHOD("set_options", "options"), &TileMapPathGraph::set_options);
	ClassDB::bind_method(D_METHOD("get_options"), &TileMapPathGraph::get_options);
	ClassDB::bind_method(D_METHOD("set_layer", "layer"), &TileMapPathGraph::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer"), &TileMapPathGraph::get_layer);

	ClassDB::bind_method(D_METHOD("update_all"), &TileMapPathGraph::update_all);
	ClassDB::bind_method(D_METHOD("update_cells", "cells"), &TileMapPathGraph::update_cells);

	ClassDB::bind_method(D_METHOD("get_cell_path", "from", "to", "allow_partial_path"), &TileMapPathGraph::get_cell_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_point_path", "from", "to", "allow_partial_path"), &TileMapPathGraph::get_point_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("is_cell_blocked", "cell"), &TileMapPathGraph::is_cell_blocked);
	ClassDB::bind_method(D_METHOD("get_cell_cost", "cell"), &TileMapPathGraph::get_cell_cost);
	ClassDB::bind_method(D_METHOD("get_region"), &TileMapPathGraph::get_region);

	ClassDB::bind_method(D_METHOD("get_backend"), &TileMapPathGraph::get_backend);
	ClassDB::bind_method(D_METHOD("get_astar_grid"), &TileMapPathGraph::get_astar_grid);
	ClassDB::bind_method(D_METHOD("get_astar"), &TileMapPathGraph::get_astar);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "options", PROPERTY_HINT_RESOURCE_TYPE, "TileMapPathGraphOptions"), "set_options", "get_options");

	ADD_SIGNAL(MethodInfo("updated"));

	GDVIRTUAL_BIND(_get_cell_cost, "cell", "tile_data");
	GDVIRTUAL_BIND(_is_cell_blocked, "cell", "tile_data");

	BIND_ENUM_CONSTANT(BACKEND_GRID);
	BIND_ENUM_CONSTANT(BACKEND_GRAPH);
	BIND_ENUM_CONSTANT(BACKEND_MAX);
}

TileMapPathGraph::TileMapPathGraph() {
	options.instantiate();
}
