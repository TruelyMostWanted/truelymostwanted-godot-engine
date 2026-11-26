/**************************************************************************/
/*  test_tile_map_path_graph.h                                            */
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

#include "../tile_map_layer.h"
#include "../tile_map_path_graph.h"
#include "../tile_set.h"

#include "core/templates/hash_set.h"
#include "scene/resources/placeholder_textures.h"
#include "tests/test_macros.h"

namespace TestTileMapPathGraph {

static const Vector2i FLOOR(0, 0);
static const Vector2i WALL(1, 0); // "blocked" = true.
static const Vector2i MUD(2, 0); // "cost" = 20.

static Ref<TileSet> make_tile_set(TileSet::TileShape p_shape, TileSet::TileLayout p_layout, TileSet::TileOffsetAxis p_offset_axis = TileSet::TILE_OFFSET_AXIS_HORIZONTAL, int p_system = 0) {
	Ref<TileSet> tile_set;
	tile_set.instantiate();
	tile_set->set_tile_shape(p_shape);
	tile_set->set_tile_layout(p_layout);
	tile_set->set_tile_offset_axis(p_offset_axis);
	TileSet::CoordinateSystem system;
	system.flip_x = (p_system & 1) != 0;
	system.flip_y = (p_system & 2) != 0;
	system.swap_x_and_y_axis = (p_system & 4) != 0;
	tile_set->set_coordinate_system(system);
	tile_set->set_tile_size(Size2i(16, 16));
	tile_set->add_custom_data_layer();
	tile_set->set_custom_data_layer_name(0, "blocked");
	tile_set->set_custom_data_layer_type(0, Variant::BOOL);
	tile_set->add_custom_data_layer();
	tile_set->set_custom_data_layer_name(1, "cost");
	tile_set->set_custom_data_layer_type(1, Variant::FLOAT);

	Ref<PlaceholderTexture2D> texture;
	texture.instantiate();
	texture->set_size(Size2(48, 16));
	Ref<TileSetAtlasSource> atlas;
	atlas.instantiate();
	atlas->set_texture(texture);
	tile_set->add_source(atlas, 0);
	atlas->create_tile(FLOOR);
	atlas->create_tile(WALL);
	atlas->create_tile(MUD);
	atlas->get_tile_data(WALL, 0)->set_custom_data("blocked", true);
	atlas->get_tile_data(MUD, 0)->set_custom_data("cost", 20.0);
	return tile_set;
}

static void fill(TileMapLayer *p_layer, const Rect2i &p_rect, const Vector2i &p_tile) {
	for (int y = p_rect.position.y; y < p_rect.get_end().y; y++) {
		for (int x = p_rect.position.x; x < p_rect.get_end().x; x++) {
			p_layer->set_cell(Vector2i(x, y), 0, p_tile);
		}
	}
}

static Ref<TileMapPathGraphOptions> make_options(bool p_corners = false) {
	Ref<TileMapPathGraphOptions> options;
	options.instantiate();
	options->set_blocked_data_layer("blocked");
	options->set_cost_data_layer("cost");
	if (p_corners) {
		options->set_neighbor_mode(TileMapPathGraphOptions::NEIGHBOR_MODE_SIDES_AND_CORNERS);
	}
	return options;
}

static bool are_neighbors(const Ref<TileSet> &p_tile_set, const Vector2i &p_a, const Vector2i &p_b, bool p_corners) {
	for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
		const TileSet::CellNeighbor neighbor = TileSet::CellNeighbor(i);
		if (p_tile_set->is_existing_neighbor(neighbor) && (p_corners || i % 2 == 0) && p_tile_set->get_neighbor_cell(p_a, neighbor) == p_b) {
			return true;
		}
	}
	return false;
}

// A path is valid when it goes from p_from to p_to, every step goes to a neighbor and no cell is blocked.
static bool is_valid_path(const Ref<TileMapPathGraph> &p_graph, const Ref<TileSet> &p_tile_set, const TypedArray<Vector2i> &p_path, const Vector2i &p_from, const Vector2i &p_to, bool p_corners) {
	if (p_path.is_empty() || Vector2i(p_path[0]) != p_from || Vector2i(p_path[p_path.size() - 1]) != p_to) {
		return false;
	}
	for (int i = 0; i < p_path.size(); i++) {
		const Vector2i cell = p_path[i];
		if (p_graph->is_cell_blocked(cell)) {
			return false;
		}
		if (i > 0 && !are_neighbors(p_tile_set, p_path[i - 1], cell, p_corners)) {
			return false;
		}
	}
	return true;
}

TEST_CASE("[TileMapPathGraph] Point ids are non-negative, unique and reversible in the int16 cell range") {
	CHECK(TileMapPathGraph::cell_to_point_id(Vector2i(-32768, -32768)) == 0);
	CHECK(TileMapPathGraph::cell_to_point_id(Vector2i(32767, 32767)) == 0xFFFFFFFFLL);

	const Vector2i cells[] = {
		Vector2i(-32768, -32768),
		Vector2i(-32768, 32767),
		Vector2i(32767, -32768),
		Vector2i(32767, 32767),
		Vector2i(0, 0),
		Vector2i(-1, 0),
		Vector2i(0, -1),
		Vector2i(-1, -1),
		Vector2i(1, -1),
		Vector2i(123, -4567),
	};
	const uint32_t cell_count = sizeof(cells) / sizeof(cells[0]);
	HashSet<int64_t> ids;
	Ref<AStar2D> astar;
	astar.instantiate();
	for (const Vector2i &cell : cells) {
		const int64_t id = TileMapPathGraph::cell_to_point_id(cell);
		INFO("cell ", cell);
		CHECK(id >= 0);
		CHECK(TileMapPathGraph::point_id_to_cell(id) == cell);
		ids.insert(id);
		astar->add_point(id, Vector2(cell));
		CHECK(astar->has_point(id));
	}
	CHECK(ids.size() == cell_count);

	ERR_PRINT_OFF;
	CHECK(TileMapPathGraph::cell_to_point_id(Vector2i(32768, 0)) == -1);
	CHECK(TileMapPathGraph::cell_to_point_id(Vector2i(0, -32769)) == -1);
	CHECK(TileMapPathGraph::point_id_to_cell(-1) == Vector2i());
	// AStar2D rejects negative ids, which is why the ids are biased.
	astar->add_point(-1, Vector2());
	CHECK_FALSE(astar->has_point(-1));
	ERR_PRINT_ON;
}

TEST_CASE("[SceneTree][TileMapPathGraph] TileMapLayer saves each cell coordinate as int16") {
	const Ref<TileSet> tile_set = make_tile_set(TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_LAYOUT_STACKED);
	TileMapLayer *layer = memnew(TileMapLayer);
	layer->set_tile_set(tile_set);
	layer->set_cell(Vector2i(-32768, 32767), 0, FLOOR);
	layer->set_cell(Vector2i(32767, -32768), 0, WALL);
	layer->set_cell(Vector2i(32768, 5), 0, MUD); // One past the range.

	TileMapLayer *loaded = memnew(TileMapLayer);
	loaded->set_tile_set(tile_set);
	loaded->set_tile_map_data_from_array(layer->get_tile_map_data_as_array());
	CHECK(loaded->get_cell_atlas_coords(Vector2i(-32768, 32767)) == FLOOR);
	CHECK(loaded->get_cell_atlas_coords(Vector2i(32767, -32768)) == WALL);
	CHECK(loaded->get_cell_source_id(Vector2i(32768, 5)) == TileSet::INVALID_SOURCE);
	CHECK(loaded->get_cell_atlas_coords(Vector2i(-32768, 5)) == MUD); // Wrapped around.

	memdelete(loaded);
	memdelete(layer);
}

TEST_CASE("[SceneTree][TileMapPathGraph] Backend and neighbors of every shape, layout and coordinate system") {
	static const TileSet::TileShape shapes[] = { TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_SHAPE_HALF_OFFSET_SQUARE, TileSet::TILE_SHAPE_HEXAGON };
	static const TileSet::TileLayout layouts[] = { TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_LAYOUT_STACKED_OFFSET, TileSet::TILE_LAYOUT_STAIRS_RIGHT, TileSet::TILE_LAYOUT_STAIRS_DOWN, TileSet::TILE_LAYOUT_DIAMOND_RIGHT, TileSet::TILE_LAYOUT_DIAMOND_DOWN };
	static const TileSet::TileOffsetAxis offset_axes[] = { TileSet::TILE_OFFSET_AXIS_HORIZONTAL, TileSet::TILE_OFFSET_AXIS_VERTICAL };

	TileMapLayer *layer = memnew(TileMapLayer);
	for (TileSet::TileShape shape : shapes) {
		for (TileSet::TileLayout layout : layouts) {
			for (TileSet::TileOffsetAxis offset_axis : offset_axes) {
				for (int system = 0; system < 8; system++) {
					const Ref<TileSet> tile_set = make_tile_set(shape, layout, offset_axis, system);
					layer->set_tile_set(tile_set);
					layer->set_cell(Vector2i(), 0, FLOOR);
					const Ref<TileMapPathGraph> graph = TileMapPathGraph::create(layer);
					const bool grid = shape == TileSet::TILE_SHAPE_SQUARE || (shape == TileSet::TILE_SHAPE_ISOMETRIC && (layout == TileSet::TILE_LAYOUT_DIAMOND_RIGHT || layout == TileSet::TILE_LAYOUT_DIAMOND_DOWN));

					// The grid backend needs sides on the axes and corners on the diagonals.
					// The graph backend connects each pair once, so neighborhoods must be symmetric.
					bool grid_ok = true;
					bool symmetric = true;
					for (int x = -2; x <= 2; x++) {
						for (int y = -2; y <= 2; y++) {
							const Vector2i cell(x, y);
							for (int i = 0; i < TileSet::CELL_NEIGHBOR_MAX; i++) {
								if (!tile_set->is_existing_neighbor(TileSet::CellNeighbor(i))) {
									continue;
								}
								const Vector2i neighbor_cell = tile_set->get_neighbor_cell(cell, TileSet::CellNeighbor(i));
								const Vector2i step = (neighbor_cell - cell).abs();
								if (grid) {
									grid_ok = grid_ok && (i % 2 == 0 ? step.x + step.y == 1 : (step.x == 1 && step.y == 1));
								}
								bool back = false;
								for (int j = i % 2; j < TileSet::CELL_NEIGHBOR_MAX; j += 2) {
									back = back || (tile_set->is_existing_neighbor(TileSet::CellNeighbor(j)) && tile_set->get_neighbor_cell(neighbor_cell, TileSet::CellNeighbor(j)) == cell);
								}
								symmetric = symmetric && back;
							}
						}
					}
					INFO("shape ", shape, ", layout ", layout, ", offset axis ", offset_axis, ", system ", system);
					CHECK(graph->get_backend() == (grid ? TileMapPathGraph::BACKEND_GRID : TileMapPathGraph::BACKEND_GRAPH));
					CHECK(graph->get_astar_grid().is_valid() == grid);
					CHECK(graph->get_astar().is_valid() == !grid);
					CHECK(grid_ok);
					CHECK(symmetric);
				}
			}
		}
	}
	memdelete(layer);
}

TEST_CASE("[SceneTree][TileMapPathGraph] Paths respect blocked cells and costs; update_cells() changes only the given cells") {
	struct MapType {
		const char *name;
		TileSet::TileShape shape;
		TileSet::TileLayout layout;
		TileMapPathGraph::Backend backend;
	};
	static const MapType map_types[] = {
		{ "square", TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_LAYOUT_STACKED, TileMapPathGraph::BACKEND_GRID },
		{ "diamond isometric", TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_LAYOUT_DIAMOND_RIGHT, TileMapPathGraph::BACKEND_GRID },
		{ "stacked isometric", TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_LAYOUT_STACKED, TileMapPathGraph::BACKEND_GRAPH },
		{ "half-offset square", TileSet::TILE_SHAPE_HALF_OFFSET_SQUARE, TileSet::TILE_LAYOUT_STACKED, TileMapPathGraph::BACKEND_GRAPH },
		{ "hexagon", TileSet::TILE_SHAPE_HEXAGON, TileSet::TILE_LAYOUT_STACKED, TileMapPathGraph::BACKEND_GRAPH },
	};
	// Negative and positive cells. Every neighbor step changes x by at most 1, so the column x = 0 splits the map.
	const Rect2i rect(-4, -3, 9, 7);
	const Vector2i from(-4, 0);
	const Vector2i to(4, 0);
	const Vector2i gap(0, 3);

	for (const MapType &map_type : map_types) {
		for (int corner_mode = 0; corner_mode < 2; corner_mode++) {
			const bool corners = corner_mode == 1;
			INFO(map_type.name, corners ? ", sides and corners" : ", sides");
			const Ref<TileSet> tile_set = make_tile_set(map_type.shape, map_type.layout);
			TileMapLayer *layer = memnew(TileMapLayer);
			layer->set_tile_set(tile_set);
			fill(layer, rect, FLOOR);
			const Ref<TileMapPathGraph> graph = TileMapPathGraph::create(layer, make_options(corners));
			CHECK(graph->get_backend() == map_type.backend);
			CHECK(graph->get_region() == rect);
			CHECK(is_valid_path(graph, tile_set, graph->get_cell_path(from, to), from, to, corners));

			// Positions are the local positions of the cells.
			const TypedArray<Vector2i> cell_path = graph->get_cell_path(from, to);
			const PackedVector2Array point_path = graph->get_point_path(from, to);
			REQUIRE(point_path.size() == cell_path.size());
			CHECK(point_path[0].is_equal_approx(layer->map_to_local(from)));
			CHECK(point_path[point_path.size() - 1].is_equal_approx(layer->map_to_local(to)));

			// A wall at x = 0 with one gap.
			TypedArray<Vector2i> changed;
			for (int y = rect.position.y; y < rect.get_end().y; y++) {
				if (Vector2i(0, y) != gap) {
					layer->set_cell(Vector2i(0, y), 0, WALL);
					changed.push_back(Vector2i(0, y));
				}
			}
			graph->update_cells(changed);
			CHECK(graph->is_cell_blocked(Vector2i(0, 0)));
			CHECK_FALSE(graph->is_cell_blocked(gap));
			TypedArray<Vector2i> path = graph->get_cell_path(from, to);
			CHECK(is_valid_path(graph, tile_set, path, from, to, corners));
			CHECK(path.has(gap));

			// Close the gap in the layer but update another cell: the graph keeps the gap open.
			layer->set_cell(gap, 0, WALL);
			TypedArray<Vector2i> other;
			other.push_back(from);
			graph->update_cells(other);
			CHECK(graph->get_cell_path(from, to).has(gap));

			// Update the gap: no path any more, a partial path ends on the left side.
			TypedArray<Vector2i> gap_cells;
			gap_cells.push_back(gap);
			graph->update_cells(gap_cells);
			CHECK(graph->get_cell_path(from, to).is_empty());
			const TypedArray<Vector2i> partial = graph->get_cell_path(from, to, true);
			REQUIRE_FALSE(partial.is_empty());
			CHECK(Vector2i(partial[0]) == from);
			CHECK(Vector2i(partial[partial.size() - 1]).x < 0);

			// Costs: mud everywhere at x = 0 except the gap. The cheap way goes through the gap.
			for (int i = 0; i < changed.size(); i++) {
				layer->set_cell(changed[i], 0, MUD);
			}
			layer->set_cell(gap, 0, FLOOR);
			changed.push_back(gap);
			graph->update_cells(changed);
			CHECK(graph->get_cell_cost(Vector2i(0, 0)) == doctest::Approx(20.0));
			CHECK(graph->get_cell_cost(gap) == doctest::Approx(1.0));
			CHECK_FALSE(graph->is_cell_blocked(Vector2i(0, 0)));
			path = graph->get_cell_path(from, to);
			CHECK(is_valid_path(graph, tile_set, path, from, to, corners));
			CHECK(path.has(gap));

			memdelete(layer);
		}
	}
}

TEST_CASE("[SceneTree][TileMapPathGraph] Options, empty cells, growing maps and unknown data layers") {
	const Ref<TileSet> tile_set = make_tile_set(TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_LAYOUT_STACKED);
	TileMapLayer *layer = memnew(TileMapLayer);
	layer->set_tile_set(tile_set);
	fill(layer, Rect2i(0, 0, 5, 1), FLOOR);
	layer->erase_cell(Vector2i(2, 0)); // A hole in a corridor.

	const Ref<TileMapPathGraph> graph = TileMapPathGraph::create(layer);
	CHECK(graph->get_options().is_valid());
	CHECK(graph->is_cell_blocked(Vector2i(2, 0)));
	CHECK(graph->get_cell_path(Vector2i(0, 0), Vector2i(4, 0)).is_empty());

	graph->get_options()->set_empty_cells_blocked(false);
	graph->get_options()->set_default_cost(2.0);
	CHECK(graph->update_all() == OK);
	CHECK_FALSE(graph->is_cell_blocked(Vector2i(2, 0)));
	CHECK(graph->get_cell_cost(Vector2i(2, 0)) == doctest::Approx(2.0));
	CHECK(graph->get_cell_path(Vector2i(0, 0), Vector2i(4, 0)).size() == 5);

	// Cells outside the graph: blocked, no path. Painting there grows the graph.
	CHECK(graph->is_cell_blocked(Vector2i(6, 0)));
	CHECK(graph->get_cell_path(Vector2i(0, 0), Vector2i(6, 0)).is_empty());
	layer->set_cell(Vector2i(5, 0), 0, FLOOR);
	layer->set_cell(Vector2i(6, 0), 0, FLOOR);
	TypedArray<Vector2i> changed;
	changed.push_back(Vector2i(5, 0));
	changed.push_back(Vector2i(6, 0));
	graph->update_cells(changed);
	CHECK(graph->get_region() == Rect2i(0, 0, 7, 1));
	CHECK(graph->get_cell_path(Vector2i(0, 0), Vector2i(6, 0)).size() == 7);

	// Corners on an open square map: diagonal steps.
	fill(layer, Rect2i(0, 0, 7, 7), FLOOR);
	graph->set_options(make_options(true));
	CHECK(graph->update_all() == OK);
	CHECK(graph->get_cell_path(Vector2i(0, 0), Vector2i(6, 6)).size() == 7);

	// Unknown data layer names warn and fall back to the defaults.
	Ref<TileMapPathGraphOptions> unknown;
	unknown.instantiate();
	unknown->set_cost_data_layer("does_not_exist");
	unknown->set_blocked_data_layer("does_not_exist_either");
	graph->set_options(unknown);
	ERR_PRINT_OFF;
	CHECK(graph->update_all() == OK);
	ERR_PRINT_ON;
	CHECK(graph->get_cell_cost(Vector2i(1, 1)) == doctest::Approx(1.0));

	// Without a layer or tile set there is nothing to build.
	const Ref<TileMapPathGraph> empty = memnew(TileMapPathGraph);
	ERR_PRINT_OFF;
	CHECK(empty->update_all() == ERR_UNCONFIGURED);
	ERR_PRINT_ON;
	CHECK(empty->get_cell_path(Vector2i(), Vector2i(1, 0)).is_empty());
	CHECK(empty->is_cell_blocked(Vector2i()));

	memdelete(layer);
	CHECK(graph->get_layer() == nullptr);
}

} // namespace TestTileMapPathGraph
