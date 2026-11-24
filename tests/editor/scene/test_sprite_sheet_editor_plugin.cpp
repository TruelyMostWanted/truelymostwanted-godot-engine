/**************************************************************************/
/*  test_sprite_sheet_editor_plugin.cpp                                   */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_sprite_sheet_editor_plugin)

#ifdef TOOLS_ENABLED

#include "editor/scene/texture/sprite_sheet_editor_plugin.h"

namespace TestSpriteSheetEditorPlugin {

TEST_CASE("[SpriteSheetEditing] Rectangle of the region editor dialog in whole pixels") {
	Rect2i rect;
	CHECK(SpriteSheetEditing::rect_from_region_editor(Rect2(16, 32, 8, 8), Size2i(64, 64), rect));
	CHECK(rect == Rect2i(16, 32, 8, 8));

	// Fractions from free dragging are rounded at both edges.
	CHECK(SpriteSheetEditing::rect_from_region_editor(Rect2(1.4, 2.6, 9.2, 3.8), Size2i(64, 64), rect));
	CHECK(rect == Rect2i(1, 3, 10, 3));

	// Negative sizes (dragged up or left) are normalized.
	CHECK(SpriteSheetEditing::rect_from_region_editor(Rect2(20, 20, -10, -5), Size2i(64, 64), rect));
	CHECK(rect == Rect2i(10, 15, 10, 5));
}

TEST_CASE("[SpriteSheetEditing] Rectangle of the region editor dialog is clamped to the texture") {
	Rect2i rect;
	CHECK(SpriteSheetEditing::rect_from_region_editor(Rect2(-4, 60, 10, 10), Size2i(64, 64), rect));
	CHECK(rect == Rect2i(0, 60, 6, 4));

	// No texture size: no clamping.
	CHECK(SpriteSheetEditing::rect_from_region_editor(Rect2(-4, 60, 10, 10), Size2i(), rect));
	CHECK(rect == Rect2i(-4, 60, 10, 10));

	// Nothing left: refused, the output is untouched.
	rect = Rect2i(1, 2, 3, 4);
	CHECK_FALSE(SpriteSheetEditing::rect_from_region_editor(Rect2(70, 70, 8, 8), Size2i(64, 64), rect));
	CHECK_FALSE(SpriteSheetEditing::rect_from_region_editor(Rect2(5, 5, 0.2, 8), Size2i(64, 64), rect));
	CHECK(rect == Rect2i(1, 2, 3, 4));
}

TEST_CASE("[SpriteSheetEditing] Rectangle text of the region list") {
	Rect2i rect;
	CHECK(SpriteSheetEditing::parse_rect("1, 2, 3, 4", rect));
	CHECK(rect == Rect2i(1, 2, 3, 4));
	CHECK(SpriteSheetEditing::parse_rect("[P: (5, 6), S: (7, 8)]", rect) == false);
	CHECK(SpriteSheetEditing::parse_rect("(5, 6, 7, 8)", rect));
	CHECK(rect == Rect2i(5, 6, 7, 8));
	CHECK_FALSE(SpriteSheetEditing::parse_rect("1, 2, 3", rect));
	CHECK_FALSE(SpriteSheetEditing::parse_rect("1, 2, x, 4", rect));
}

} // namespace TestSpriteSheetEditorPlugin

#endif // TOOLS_ENABLED
