/**************************************************************************/
/*  test_tile_coordinate_axes.h                                           */
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
#include "../tile_set.h"

#include "tests/test_macros.h"

namespace TestTileCoordinateAxes {

// The eight combinations of the coordinate system, by index: bit 0 is flip X, bit 1 flip Y, bit 2 the swap
// (the values of the former `coordinate_axes` property).
static TileSet::CoordinateSystem make_system(int p_index) {
	TileSet::CoordinateSystem system;
	system.flip_x = (p_index & 1) != 0;
	system.flip_y = (p_index & 2) != 0;
	system.swap_x_and_y_axis = (p_index & 4) != 0;
	return system;
}

static Ref<TileSet> make_tile_set(TileSet::TileShape p_shape, TileSet::TileLayout p_layout, TileSet::TileOffsetAxis p_offset_axis, int p_system) {
	Ref<TileSet> tile_set;
	tile_set.instantiate();
	tile_set->set_tile_shape(p_shape);
	tile_set->set_tile_layout(p_layout);
	tile_set->set_tile_offset_axis(p_offset_axis);
	tile_set->set_coordinate_system(make_system(p_system));
	tile_set->set_tile_size(Size2i(64, 32));
	return tile_set;
}

static const TileSet::TileShape SHAPES[] = { TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_SHAPE_HALF_OFFSET_SQUARE, TileSet::TILE_SHAPE_HEXAGON };
static const TileSet::TileLayout LAYOUTS[] = { TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_LAYOUT_STACKED_OFFSET, TileSet::TILE_LAYOUT_STAIRS_RIGHT, TileSet::TILE_LAYOUT_STAIRS_DOWN, TileSet::TILE_LAYOUT_DIAMOND_RIGHT, TileSet::TILE_LAYOUT_DIAMOND_DOWN };
static const TileSet::TileOffsetAxis OFFSET_AXES[] = { TileSet::TILE_OFFSET_AXIS_HORIZONTAL, TileSet::TILE_OFFSET_AXIS_VERTICAL };
static const int SYSTEM_COUNT = 8;

TEST_CASE("[TileSet][CoordinateSystem] Properties") {
	Ref<TileSet> tile_set;
	tile_set.instantiate();
	CHECK_FALSE(tile_set->is_swap_x_and_y_axis());
	CHECK_FALSE(tile_set->is_flip_x());
	CHECK_FALSE(tile_set->is_flip_y());
	CHECK(tile_set->get_coordinate_system().is_default());

	tile_set->set_swap_x_and_y_axis(true);
	tile_set->set_flip_y(true);
	CHECK(tile_set->is_swap_x_and_y_axis());
	CHECK_FALSE(tile_set->is_flip_x());
	CHECK(tile_set->is_flip_y());
	CHECK(tile_set->get_coordinate_system() == make_system(6));

	// The properties are bound.
	tile_set->set("flip_x", true);
	CHECK(bool(tile_set->get("flip_x")));
	CHECK(bool(tile_set->get("swap_x_and_y_axis")));
	CHECK(tile_set->get_coordinate_system() == make_system(7));
}

TEST_CASE("[TileSet][CoordinateSystem] The former coordinate_axes value is read into the three properties") {
	for (int index = 0; index < SYSTEM_COUNT; index++) {
		Ref<TileSet> tile_set;
		tile_set.instantiate();
		tile_set->set_flip_x(true);
		bool valid = false;
		tile_set->set("coordinate_axes", index, &valid);
		INFO("coordinate_axes ", index);
		CHECK(valid);
		CHECK(tile_set->get_coordinate_system() == make_system(index));
	}

	// The old property is neither listed nor saved.
	Ref<TileSet> tile_set;
	tile_set.instantiate();
	List<PropertyInfo> properties;
	tile_set->get_property_list(&properties);
	bool listed = false;
	for (const PropertyInfo &property : properties) {
		listed = listed || property.name == "coordinate_axes";
	}
	CHECK_FALSE(listed);
}

TEST_CASE("[TileSet][CoordinateSystem] Conversion between map and layout coordinates") {
	for (int index = 0; index < SYSTEM_COUNT; index++) {
		const TileSet::CoordinateSystem system = make_system(index);
		for (int x = -3; x <= 3; x++) {
			for (int y = -3; y <= 3; y++) {
				const Vector2i coords(x, y);
				const Vector2i layout = TileSet::map_to_layout_coords_with_system(coords, system);
				CHECK(TileSet::layout_to_map_coords_with_system(layout, system) == coords);
			}
		}
	}

	// All eight combinations: the swap first, then the flips.
	const Vector2i expected[SYSTEM_COUNT] = {
		Vector2i(1, 2), // Default.
		Vector2i(-1, 2), // Flip X.
		Vector2i(1, -2), // Flip Y.
		Vector2i(-1, -2), // Flip X and Y.
		Vector2i(2, 1), // Swap.
		Vector2i(-2, 1), // Swap, flip X.
		Vector2i(2, -1), // Swap, flip Y.
		Vector2i(-2, -1), // Swap, flip X and Y.
	};
	for (int index = 0; index < SYSTEM_COUNT; index++) {
		INFO("combination ", index);
		CHECK(TileSet::layout_to_map_coords_with_system(Vector2i(1, 2), make_system(index)) == expected[index]);
		const Ref<TileSet> tile_set = make_tile_set(TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_OFFSET_AXIS_HORIZONTAL, index);
		CHECK(tile_set->layout_to_map_coords(Vector2i(1, 2)) == expected[index]);
		CHECK(tile_set->map_to_layout_coords(expected[index]) == Vector2i(1, 2));
	}
}

TEST_CASE("[TileSet][CoordinateSystem] Every shape, layout and combination: positions and neighbors stay on screen") {
	for (TileSet::TileShape shape : SHAPES) {
		for (TileSet::TileLayout layout : LAYOUTS) {
			for (TileSet::TileOffsetAxis offset_axis : OFFSET_AXES) {
				const Ref<TileSet> reference = make_tile_set(shape, layout, offset_axis, 0);
				for (int index = 0; index < SYSTEM_COUNT; index++) {
					const Ref<TileSet> tile_set = make_tile_set(shape, layout, offset_axis, index);
					bool positions_ok = true;
					bool round_trip_ok = true;
					bool neighbors_ok = true;
					for (int x = -3; x <= 3; x++) {
						for (int y = -3; y <= 3; y++) {
							const Vector2i coords(x, y);
							const Vector2i layout_coords = tile_set->map_to_layout_coords(coords);
							const Vector2 position = tile_set->map_to_local(coords);
							positions_ok = positions_ok && position.is_equal_approx(reference->map_to_local(layout_coords));
							round_trip_ok = round_trip_ok && tile_set->local_to_map(position) == coords;
							for (int side = 0; side < TileSet::CELL_NEIGHBOR_MAX; side++) {
								const TileSet::CellNeighbor neighbor = TileSet::CellNeighbor(side);
								if (!tile_set->is_existing_neighbor(neighbor)) {
									continue;
								}
								const Vector2 expected = reference->map_to_local(reference->get_neighbor_cell(layout_coords, neighbor));
								neighbors_ok = neighbors_ok && tile_set->map_to_local(tile_set->get_neighbor_cell(coords, neighbor)).is_equal_approx(expected);
							}
						}
					}
					INFO("shape ", shape, ", layout ", layout, ", offset axis ", offset_axis, ", combination ", index);
					CHECK(positions_ok);
					CHECK(round_trip_ok);
					CHECK(neighbors_ok);
				}
			}
		}
	}
}

TEST_CASE("[TileSet][CoordinateSystem] Examples") {
	// Isometric Diamond Right + swap: +X goes down right, +Y goes up right.
	Ref<TileSet> iso = make_tile_set(TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_LAYOUT_DIAMOND_RIGHT, TileSet::TILE_OFFSET_AXIS_HORIZONTAL, 4);
	CHECK(iso->is_swap_x_and_y_axis());
	const Vector2 origin = iso->map_to_local(Vector2i(0, 0));
	const Vector2 plus_x = iso->map_to_local(Vector2i(1, 0)) - origin;
	const Vector2 plus_y = iso->map_to_local(Vector2i(0, 1)) - origin;
	CHECK(plus_x.x > 0);
	CHECK(plus_x.y > 0);
	CHECK(plus_y.x > 0);
	CHECK(plus_y.y < 0);

	// Square + flip Y: +Y goes up.
	Ref<TileSet> square = make_tile_set(TileSet::TILE_SHAPE_SQUARE, TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_OFFSET_AXIS_HORIZONTAL, 2);
	CHECK(square->is_flip_y());
	CHECK(square->map_to_local(Vector2i(0, 1)).y < square->map_to_local(Vector2i(0, 0)).y);
	CHECK(square->map_to_local(Vector2i(1, 0)).x > square->map_to_local(Vector2i(0, 0)).x);

	// The default is today's behavior.
	Ref<TileSet> plain = make_tile_set(TileSet::TILE_SHAPE_ISOMETRIC, TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_OFFSET_AXIS_HORIZONTAL, 0);
	CHECK(plain->get_coordinate_system().is_default());
	CHECK(plain->map_to_layout_coords(Vector2i(3, -2)) == Vector2i(3, -2));
}

TEST_CASE("[TileSet][CoordinateSystem] convert_map_coords() keeps cells in place") {
	for (TileSet::TileShape shape : SHAPES) {
		for (TileSet::TileOffsetAxis offset_axis : OFFSET_AXES) {
			for (TileSet::TileLayout from_layout : LAYOUTS) {
				for (TileSet::TileLayout to_layout : LAYOUTS) {
					for (int from_index : { 0, 3, 4, 7 }) {
						for (int to_index : { 0, 1, 6 }) {
							const TileSet::CoordinateSystem from_system = make_system(from_index);
							const Ref<TileSet> before = make_tile_set(shape, from_layout, offset_axis, from_index);
							const Ref<TileSet> after = make_tile_set(shape, shape == TileSet::TILE_SHAPE_SQUARE ? from_layout : to_layout, offset_axis, to_index);
							// The map keeps its look: positions relative to cell (0, 0) stay the same. Layouts whose cell
							// centers lie on different grids (Stacked and Stacked Offset) shift the whole map by half a cell.
							const Vector2 before_origin = before->map_to_local(Vector2i());
							const Vector2 after_origin = after->map_to_local(after->convert_map_coords_from_system(Vector2i(), from_layout, from_system));
							bool ok = true;
							HashSet<Vector2i> seen;
							for (int x = -3; x <= 3; x++) {
								for (int y = -3; y <= 3; y++) {
									const Vector2i coords(x, y);
									const Vector2i converted = after->convert_map_coords(coords, from_layout, from_system.swap_x_and_y_axis, from_system.flip_x, from_system.flip_y);
									ok = ok && converted == after->convert_map_coords_from_system(coords, from_layout, from_system);
									ok = ok && (after->map_to_local(converted) - after_origin).is_equal_approx(before->map_to_local(coords) - before_origin);
									ok = ok && !seen.has(converted);
									seen.insert(converted);
								}
							}
							INFO("shape ", shape, ", offset axis ", offset_axis, ", layout ", from_layout, " -> ", to_layout, ", combination ", from_index, " -> ", to_index);
							CHECK(ok);
						}
					}
				}
			}
		}
	}
}

TEST_CASE("[SceneTree][TileMapLayer][CoordinateSystem] convert_cell_coordinates() and patterns") {
	Ref<TileSet> tile_set = make_tile_set(TileSet::TILE_SHAPE_HEXAGON, TileSet::TILE_LAYOUT_STACKED, TileSet::TILE_OFFSET_AXIS_HORIZONTAL, 0);
	TileMapLayer *layer = memnew(TileMapLayer);
	layer->set_tile_set(tile_set);
	const Vector2i cells[] = { Vector2i(0, 0), Vector2i(1, 0), Vector2i(1, 1), Vector2i(2, 3), Vector2i(-1, 2) };
	HashMap<Vector2i, Vector2> positions;
	for (const Vector2i &cell : cells) {
		layer->set_cell(cell, 0, Vector2i(cell.x + 5, 1), 0);
		positions[cell] = tile_set->map_to_local(cell);
	}

	tile_set->set_tile_layout(TileSet::TILE_LAYOUT_DIAMOND_DOWN);
	tile_set->set_swap_x_and_y_axis(true);
	tile_set->set_flip_y(true);
	layer->convert_cell_coordinates(TileSet::TILE_LAYOUT_STACKED, false, false, false);
	REQUIRE(layer->get_used_cells().size() == 5);
	for (const Vector2i &old_cell : cells) {
		const Vector2i new_cell = tile_set->local_to_map(positions[old_cell]);
		CHECK(layer->get_cell_source_id(new_cell) == 0);
		CHECK(layer->get_cell_atlas_coords(new_cell) == Vector2i(old_cell.x + 5, 1));
	}

	// A pattern keeps its shape: placed where it was taken, it covers the same cells.
	TypedArray<Vector2i> used = layer->get_used_cells();
	Ref<TileMapPattern> pattern = layer->get_pattern(used);
	Vector2i min_layout = tile_set->map_to_layout_coords(used[0]);
	for (int i = 1; i < used.size(); i++) {
		min_layout = min_layout.min(tile_set->map_to_layout_coords(used[i]));
	}
	const Vector2i origin = tile_set->layout_to_map_coords(min_layout);
	HashSet<Vector2i> placed;
	const TypedArray<Vector2i> pattern_cells = pattern->get_used_cells();
	for (int i = 0; i < pattern_cells.size(); i++) {
		placed.insert(tile_set->map_pattern(origin, pattern_cells[i], pattern));
	}
	CHECK(placed.size() == used.size());
	for (int i = 0; i < used.size(); i++) {
		CHECK(placed.has(Vector2i(used[i])));
	}

	// Converting back through the binding, from the settings the cells were painted with, restores them.
	tile_set->set_tile_layout(TileSet::TILE_LAYOUT_STACKED);
	tile_set->set_coordinate_system(TileSet::CoordinateSystem());
	layer->call("convert_cell_coordinates", int(TileSet::TILE_LAYOUT_DIAMOND_DOWN), true, false, true);
	REQUIRE(layer->get_used_cells().size() == 5);
	for (const Vector2i &old_cell : cells) {
		CHECK(layer->get_cell_source_id(old_cell) == 0);
		CHECK(layer->get_cell_atlas_coords(old_cell) == Vector2i(old_cell.x + 5, 1));
	}
	memdelete(layer);
}

} // namespace TestTileCoordinateAxes
