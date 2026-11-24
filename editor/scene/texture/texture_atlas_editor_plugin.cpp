/**************************************************************************/
/*  texture_atlas_editor_plugin.cpp                                       */
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

#include "texture_atlas_editor_plugin.h"

#include "core/input/input_event.h"
#include "core/io/image.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"
#include "scene/resources/sprite_frames.h"

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasEditing

void TextureAtlasEditing::commit(const String &p_action, const Ref<SpriteSheet> &p_atlas, const TypedDictionary<StringName, Rect2i> &p_old_regions, const TypedDictionary<StringName, Rect2i> &p_old_margins, const TypedDictionary<StringName, Rect2i> &p_new_regions, const TypedDictionary<StringName, Rect2i> &p_new_margins, UndoRedo::MergeMode p_merge) {
	ERR_FAIL_COND(p_atlas.is_null());
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(p_action, p_merge, p_atlas.ptr());
	undo_redo->add_do_method(p_atlas.ptr(), "set_regions", p_new_regions);
	undo_redo->add_do_method(p_atlas.ptr(), "set_margins", p_new_margins);
	undo_redo->add_undo_method(p_atlas.ptr(), "set_regions", p_old_regions);
	undo_redo->add_undo_method(p_atlas.ptr(), "set_margins", p_old_margins);
	undo_redo->commit_action();
}

void TextureAtlasEditing::commit_regions(const String &p_action, const Ref<SpriteSheet> &p_atlas, const TypedDictionary<StringName, Rect2i> &p_new_regions, const TypedDictionary<StringName, Rect2i> &p_new_margins, UndoRedo::MergeMode p_merge) {
	ERR_FAIL_COND(p_atlas.is_null());
	commit(p_action, p_atlas, p_atlas->get_regions(), p_atlas->get_margins(), p_new_regions, p_new_margins, p_merge);
}

void TextureAtlasEditing::remove(const Ref<SpriteSheet> &p_atlas, const PackedStringArray &p_names) {
	ERR_FAIL_COND(p_atlas.is_null());
	if (p_names.is_empty()) {
		return;
	}
	TypedDictionary<StringName, Rect2i> regions = p_atlas->get_regions();
	TypedDictionary<StringName, Rect2i> margins = p_atlas->get_margins();
	for (const String &name : p_names) {
		regions.erase(StringName(name));
		margins.erase(StringName(name));
	}
	commit_regions(p_names.size() == 1 ? TTR("Delete Region") : TTR("Delete Regions"), p_atlas, regions, margins);
}

void TextureAtlasEditing::rename(const Ref<SpriteSheet> &p_atlas, const StringName &p_from, const StringName &p_to) {
	ERR_FAIL_COND(p_atlas.is_null());
	if (p_from == p_to) {
		return;
	}
	if (!SpriteSheet::is_valid_region_name(p_to)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("\"%s\" is not a valid region name: it must not be empty and must not contain '/', ':', '\"', control characters or surrounding spaces."), p_to));
		return;
	}
	if (p_atlas->has_region(p_to)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("A region named \"%s\" exists already."), p_to));
		return;
	}
	// Rebuild the dictionaries so the renamed region keeps its position.
	const TypedDictionary<StringName, Rect2i> old_regions = p_atlas->get_regions();
	const TypedDictionary<StringName, Rect2i> old_margins = p_atlas->get_margins();
	TypedDictionary<StringName, Rect2i> regions;
	TypedDictionary<StringName, Rect2i> margins;
	for (const KeyValue<Variant, Variant> &E : old_regions) {
		const StringName key = E.key;
		regions[key == p_from ? p_to : key] = E.value;
	}
	for (const KeyValue<Variant, Variant> &E : old_margins) {
		const StringName key = E.key;
		margins[key == p_from ? p_to : key] = E.value;
	}
	commit(TTR("Rename Region"), p_atlas, old_regions, old_margins, regions, margins);
}

void TextureAtlasEditing::add(const Ref<SpriteSheet> &p_atlas, const String &p_action, const TypedArray<Rect2i> &p_rects, const String &p_prefix) {
	ERR_FAIL_COND(p_atlas.is_null());
	if (p_rects.is_empty()) {
		return;
	}
	// Names and validation from the runtime API, applied to a copy so the change goes through undo/redo.
	Ref<SpriteSheet> copy;
	copy.instantiate();
	copy->set_texture(p_atlas->get_sheet_texture());
	copy->set_regions(p_atlas->get_regions());
	copy->set_margins(p_atlas->get_margins());
	if (copy->add_regions(p_rects, p_prefix).is_empty()) {
		return; // The error was printed by add_regions().
	}
	commit_regions(p_action, p_atlas, copy->get_regions(), copy->get_margins());
}

bool TextureAtlasEditing::parse_rect(const String &p_text, Rect2i &r_rect) {
	const PackedStringArray parts = p_text.replace_chars("()[]", ' ').split(",", false);
	if (parts.size() != 4) {
		return false;
	}
	int values[4];
	for (int i = 0; i < 4; i++) {
		const String part = parts[i].strip_edges();
		if (!part.is_valid_int()) {
			return false;
		}
		values[i] = part.to_int();
	}
	r_rect = Rect2i(values[0], values[1], values[2], values[3]);
	return true;
}

String TextureAtlasEditing::get_texture_warning(const Ref<SpriteSheet> &p_atlas) {
	if (p_atlas.is_null()) {
		return String();
	}
	const Ref<Texture2D> texture = p_atlas->get_sheet_texture();
	if (texture.is_null()) {
		if (!p_atlas->get_texture_path().is_empty()) {
			return vformat(TTR("The texture \"%s\" could not be loaded. Regions cannot be checked against its size."), p_atlas->get_texture_path());
		}
		return TTR("No texture: set the sprite sheet in \"Texture\" to see and slice the regions.");
	}
	if (texture->get_width() <= 0 || texture->get_height() <= 0) {
		return TTR("The texture has no size.");
	}
	const Ref<Image> image = texture->get_image();
	if (image.is_null() || image->is_empty()) {
		return TTR("The texture has no image data: Auto Slice is not available.");
	}
	return String();
}

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasRegionList

void TextureAtlasRegionList::_atlas_changed() {
	if (update_queued) {
		return;
	}
	update_queued = true;
	callable_mp(this, &TextureAtlasRegionList::update_list).call_deferred();
}

void TextureAtlasRegionList::set_atlas(const Ref<SpriteSheet> &p_atlas) {
	if (atlas == p_atlas) {
		return;
	}
	if (atlas.is_valid()) {
		atlas->disconnect_changed(callable_mp(this, &TextureAtlasRegionList::_atlas_changed));
	}
	atlas = p_atlas;
	if (atlas.is_valid()) {
		atlas->connect_changed(callable_mp(this, &TextureAtlasRegionList::_atlas_changed));
	}
	update_list();
}

void TextureAtlasRegionList::update_list() {
	update_queued = false;
	const PackedStringArray selected = get_selected_names();
	updating = true;
	tree->clear();
	TreeItem *root = tree->create_item();
	if (atlas.is_null()) {
		updating = false;
		return;
	}
	const Ref<Texture2D> texture = atlas->get_sheet_texture();
	const String filter_text = filter->get_text().strip_edges();
	const int thumbnail_size = int(32 * EDSCALE);
	for (const String &name : atlas->get_region_names()) {
		if (!filter_text.is_empty() && !name.containsn(filter_text)) {
			continue;
		}
		const Rect2i rect = atlas->get_region(name);
		TreeItem *item = tree->create_item(root);
		item->set_text(0, name);
		item->set_metadata(0, name);
		item->set_editable(0, true);
		item->set_text(1, vformat("%d, %d, %d, %d", rect.position.x, rect.position.y, rect.size.x, rect.size.y));
		item->set_editable(1, true);
		item->set_tooltip_text(1, TTR("X, Y, width, height in pixels of the texture."));
		if (texture.is_valid()) {
			// A thumbnail of its own, so the list does not create the cached textures of the atlas.
			Ref<AtlasTexture> thumbnail;
			thumbnail.instantiate();
			thumbnail->set_atlas(texture);
			thumbnail->set_region(Rect2(rect));
			item->set_icon(0, thumbnail);
			item->set_icon_max_width(0, thumbnail_size);
		}
		if (selected.has(name)) {
			item->select(0);
		}
	}
	updating = false;
}

void TextureAtlasRegionList::_item_edited() {
	if (updating || atlas.is_null()) {
		return;
	}
	TreeItem *item = tree->get_edited();
	ERR_FAIL_NULL(item);
	const StringName old_name = item->get_metadata(0);
	if (tree->get_edited_column() == 0) {
		const String new_name = item->get_text(0);
		if (new_name != String(old_name)) {
			TextureAtlasEditing::rename(atlas, old_name, new_name);
		}
	} else {
		Rect2i rect;
		if (!TextureAtlasEditing::parse_rect(item->get_text(1), rect)) {
			EditorNode::get_singleton()->show_warning(TTR("Enter the rectangle as \"x, y, width, height\"."));
		} else if (rect != atlas->get_region(old_name)) {
			const Ref<Texture2D> texture = atlas->get_sheet_texture();
			if (rect.size.x <= 0 || rect.size.y <= 0 || (texture.is_valid() && !Rect2i(Point2i(), Size2i(texture->get_size())).encloses(rect))) {
				EditorNode::get_singleton()->show_warning(TTR("The rectangle must have a size greater than 0 and lie inside the texture."));
			} else {
				TypedDictionary<StringName, Rect2i> regions = atlas->get_regions();
				regions[old_name] = rect;
				TextureAtlasEditing::commit_regions(TTR("Set Region Rectangle"), atlas, regions, atlas->get_margins());
			}
		}
	}
	_atlas_changed(); // Shows the stored values again if the change was refused.
}

void TextureAtlasRegionList::_multi_selected(Object *p_item, int p_column, bool p_selected) {
	if (updating) {
		return;
	}
	// Tree emits one signal per changed cell; report the selection once.
	callable_mp(this, &TextureAtlasRegionList::_emit_selection_changed).call_deferred();
}

void TextureAtlasRegionList::_emit_selection_changed() {
	if (selection_changed_callback.is_valid()) {
		selection_changed_callback.call();
	}
}

PackedStringArray TextureAtlasRegionList::get_selected_names() const {
	PackedStringArray names;
	TreeItem *item = tree->get_next_selected(nullptr);
	while (item) {
		const String name = item->get_metadata(0);
		if (!names.has(name)) {
			names.push_back(name);
		}
		item = tree->get_next_selected(item);
	}
	return names;
}

void TextureAtlasRegionList::set_selected_names(const PackedStringArray &p_names) {
	TreeItem *root = tree->get_root();
	if (root == nullptr) {
		return;
	}
	updating = true;
	tree->deselect_all();
	TreeItem *first = nullptr;
	for (TreeItem *item = root->get_first_child(); item; item = item->get_next()) {
		if (p_names.has(String(item->get_metadata(0)))) {
			item->select(0);
			if (first == nullptr) {
				first = item;
			}
		}
	}
	if (first) {
		tree->scroll_to_item(first);
	}
	updating = false;
}

void TextureAtlasRegionList::set_tree_minimum_height(int p_height) {
	tree->set_custom_minimum_size(Size2(0, p_height));
}

Variant TextureAtlasRegionList::_get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (atlas.is_null()) {
		return Variant();
	}
	TreeItem *item = tree->get_item_at_position(p_point);
	if (item == nullptr) {
		return Variant();
	}
	const StringName name = item->get_metadata(0);
	if (!atlas->has_region(name)) {
		return Variant();
	}
	// The cached AtlasTexture of the atlas: it follows later changes of the region while the editor runs.
	const Ref<AtlasTexture> texture = atlas->get_region_texture(name);
	Dictionary drag_data;
	drag_data["type"] = "resource";
	drag_data["resource"] = texture;

	Label *preview = memnew(Label);
	preview->set_text(name);
	preview->set_focus_mode(FOCUS_NONE);
	set_drag_preview(preview);
	return drag_data;
}

TextureAtlasRegionList::TextureAtlasRegionList() {
	filter = memnew(LineEdit);
	filter->set_placeholder(TTR("Filter Regions"));
	filter->set_clear_button_enabled(true);
	filter->set_accessibility_name(TTRC("Filter Regions"));
	filter->connect(SceneStringName(text_changed), callable_mp(this, &TextureAtlasRegionList::update_list).unbind(1));
	add_child(filter);

	tree = memnew(Tree);
	tree->set_columns(2);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Name"));
	tree->set_column_title(1, TTR("Rectangle"));
	tree->set_column_expand(0, true);
	tree->set_column_expand(1, true);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_custom_minimum_size(Size2(0, 160 * EDSCALE));
	tree->set_accessibility_name(TTRC("Regions"));
	tree->connect("item_edited", callable_mp(this, &TextureAtlasRegionList::_item_edited));
	tree->connect("multi_selected", callable_mp(this, &TextureAtlasRegionList::_multi_selected));
	tree->set_drag_forwarding(callable_mp(this, &TextureAtlasRegionList::_get_drag_data_fw).bind(tree), Callable(), Callable());
	add_child(tree);
}

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasCanvas

static const real_t HANDLE_RADIUS = 6;

Vector2 TextureAtlasCanvas::_to_texture(const Vector2 &p_screen) const {
	return (p_screen - view_offset) / zoom;
}

Vector2 TextureAtlasCanvas::_to_screen(const Vector2 &p_texture) const {
	return p_texture * zoom + view_offset;
}

Vector2i TextureAtlasCanvas::_snap(const Vector2 &p_texture) const {
	if (editor->is_grid_snap()) {
		const Vector2i step = editor->get_grid_step();
		const Vector2i offset = editor->get_grid_offset();
		return Vector2i(
				int(Math::round((p_texture.x - offset.x) / step.x)) * step.x + offset.x,
				int(Math::round((p_texture.y - offset.y) / step.y)) * step.y + offset.y);
	}
	return Vector2i(int(Math::round(p_texture.x)), int(Math::round(p_texture.y)));
}

Rect2i TextureAtlasCanvas::_clamp_to_texture(const Rect2i &p_rect) const {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	if (atlas.is_null() || atlas->get_sheet_texture().is_null()) {
		return p_rect;
	}
	const Size2i size = Size2i(atlas->get_sheet_texture()->get_size());
	Rect2i rect = p_rect;
	rect.size = rect.size.min(size).maxi(1);
	rect.position = rect.position.clamp(Vector2i(), size - rect.size);
	return rect;
}

StringName TextureAtlasCanvas::_region_at(const Vector2 &p_texture) const {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	StringName found;
	int64_t found_area = INT64_MAX;
	// The smallest region under the cursor wins, so nested regions stay reachable.
	for (const String &name : atlas->get_region_names()) {
		const Rect2i rect = atlas->get_region(name);
		if (Rect2(rect).has_point(p_texture) && int64_t(rect.get_area()) < found_area) {
			found = name;
			found_area = rect.get_area();
		}
	}
	return found;
}

void TextureAtlasCanvas::_handle_positions(const Rect2i &p_rect, Vector2 r_points[8]) const {
	const Vector2 from = _to_screen(Vector2(p_rect.position));
	const Vector2 to = _to_screen(Vector2(p_rect.get_end()));
	const Vector2 mid = (from + to) / 2;
	// Clockwise from the top-left corner.
	r_points[0] = from;
	r_points[1] = Vector2(mid.x, from.y);
	r_points[2] = Vector2(to.x, from.y);
	r_points[3] = Vector2(to.x, mid.y);
	r_points[4] = to;
	r_points[5] = Vector2(mid.x, to.y);
	r_points[6] = Vector2(from.x, to.y);
	r_points[7] = Vector2(from.x, mid.y);
}

int TextureAtlasCanvas::_handle_at(const Vector2 &p_screen) const {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	const PackedStringArray &selection = editor->get_selection();
	if (atlas.is_null() || selection.size() != 1 || !atlas->has_region(selection[0])) {
		return -1;
	}
	Vector2 points[8];
	_handle_positions(atlas->get_region(selection[0]), points);
	for (int i = 0; i < 8; i++) {
		if (points[i].distance_to(p_screen) <= HANDLE_RADIUS * EDSCALE) {
			return i;
		}
	}
	return -1;
}

void TextureAtlasCanvas::set_zoom(float p_zoom, const Vector2 &p_center) {
	const float new_zoom = CLAMP(p_zoom, 0.0625f, 64.0f);
	const Vector2 texture_point = _to_texture(p_center);
	zoom = new_zoom;
	view_offset = p_center - texture_point * zoom;
	queue_redraw();
}

void TextureAtlasCanvas::fit() {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	if (atlas.is_null() || atlas->get_sheet_texture().is_null() || get_size().x <= 0 || get_size().y <= 0) {
		zoom = 1.0f;
		view_offset = Vector2(8, 8) * EDSCALE;
		queue_redraw();
		return;
	}
	const Size2 texture_size = atlas->get_sheet_texture()->get_size();
	const Size2 available = get_size() - Size2(16, 16) * EDSCALE;
	zoom = MAX(0.0625f, MIN(available.x / texture_size.x, available.y / texture_size.y));
	// Whole steps for pixel art when the sheet is smaller than the view.
	if (zoom > 1.0f) {
		zoom = Math::floor(zoom);
	}
	view_offset = ((get_size() - texture_size * zoom) / 2).floor();
	queue_redraw();
}

void TextureAtlasCanvas::_draw_canvas() {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	if (atlas.is_null()) {
		return;
	}
	const Ref<Texture2D> texture = atlas->get_sheet_texture();
	const Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	const Color outline = Color(1, 1, 1, 0.5);
	const Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Label"));
	const int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Label"));

	Size2 texture_size;
	if (texture.is_valid()) {
		texture_size = texture->get_size();
		draw_texture_rect(texture, Rect2(view_offset, texture_size * zoom));
		draw_rect(Rect2(view_offset, texture_size * zoom), Color(0.5, 0.5, 0.5, 0.6), false, 1);
	}

	// Grid lines of the grid snap, only when they are far enough apart.
	if (editor->is_grid_snap() && texture.is_valid()) {
		const Vector2i step = editor->get_grid_step();
		const Vector2i offset = editor->get_grid_offset();
		if (step.x * zoom >= 6 && step.y * zoom >= 6) {
			const Color grid_color = Color(1, 1, 1, 0.12);
			for (int x = offset.x % step.x; x <= texture_size.x; x += step.x) {
				draw_line(_to_screen(Vector2(x, 0)), _to_screen(Vector2(x, texture_size.y)), grid_color);
			}
			for (int y = offset.y % step.y; y <= texture_size.y; y += step.y) {
				draw_line(_to_screen(Vector2(0, y)), _to_screen(Vector2(texture_size.x, y)), grid_color);
			}
		}
	}

	const PackedStringArray &selection = editor->get_selection();
	for (const String &name : atlas->get_region_names()) {
		const Rect2i rect = atlas->get_region(name);
		const Rect2 screen_rect(_to_screen(Vector2(rect.position)), Vector2(rect.size) * zoom);
		const bool selected = selection.has(name);
		if (selected) {
			draw_rect(screen_rect, Color(accent, 0.15));
		}
		draw_rect(screen_rect, selected ? accent : outline, false, selected ? 2 : 1);
		if (screen_rect.size.y >= font_size + 2 && screen_rect.size.x >= font_size * 2) {
			draw_string(font, screen_rect.position + Vector2(2, font_size), name, HORIZONTAL_ALIGNMENT_LEFT, screen_rect.size.x - 4, int(font_size * 0.8f), selected ? accent : Color(1, 1, 1, 0.8));
		}
	}

	if (drag_mode == DRAG_CREATE) {
		const Rect2 screen_rect(_to_screen(Vector2(drag_new_rect.position)), Vector2(drag_new_rect.size) * zoom);
		draw_rect(screen_rect, accent, false, 2);
	}

	if (selection.size() == 1 && atlas->has_region(selection[0])) {
		Vector2 points[8];
		_handle_positions(atlas->get_region(selection[0]), points);
		for (int i = 0; i < 8; i++) {
			draw_rect(Rect2(points[i] - Vector2(3, 3) * EDSCALE, Vector2(6, 6) * EDSCALE), accent);
		}
	}
}

void TextureAtlasCanvas::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			_draw_canvas();
		} break;
	}
}

Control::CursorShape TextureAtlasCanvas::get_cursor_shape(const Point2 &p_pos) const {
	if (drag_mode == DRAG_PAN) {
		return CURSOR_DRAG;
	}
	const int handle = drag_mode == DRAG_RESIZE ? drag_handle : _handle_at(p_pos);
	switch (handle) {
		case 0:
		case 4:
			return CURSOR_FDIAGSIZE;
		case 2:
		case 6:
			return CURSOR_BDIAGSIZE;
		case 1:
		case 5:
			return CURSOR_VSIZE;
		case 3:
		case 7:
			return CURSOR_HSIZE;
		default:
			break;
	}
	if (drag_mode == DRAG_MOVE) {
		return CURSOR_MOVE;
	}
	return CURSOR_CROSS;
}

void TextureAtlasCanvas::gui_input(const Ref<InputEvent> &p_event) {
	const Ref<SpriteSheet> atlas = editor->get_atlas();
	if (atlas.is_null()) {
		return;
	}

	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		const Vector2 position = mb->get_position();
		if (mb->is_pressed() && (mb->get_button_index() == MouseButton::WHEEL_UP || mb->get_button_index() == MouseButton::WHEEL_DOWN)) {
			set_zoom(zoom * (mb->get_button_index() == MouseButton::WHEEL_UP ? 1.25f : 0.8f), position);
			accept_event();
			return;
		}
		if (mb->get_button_index() == MouseButton::MIDDLE || mb->get_button_index() == MouseButton::RIGHT) {
			drag_mode = mb->is_pressed() ? DRAG_PAN : DRAG_NONE;
			drag_from = position;
			accept_event();
			return;
		}
		if (mb->get_button_index() != MouseButton::LEFT) {
			return;
		}
		grab_focus();
		if (mb->is_pressed()) {
			const Vector2 texture_point = _to_texture(position);
			drag_old_regions = atlas->get_regions();
			drag_old_margins = atlas->get_margins();
			const int handle = _handle_at(position);
			if (handle != -1) {
				drag_mode = DRAG_RESIZE;
				drag_handle = handle;
				drag_name = editor->get_selection()[0];
				drag_start_rect = atlas->get_region(drag_name);
				drag_from = texture_point;
				accept_event();
				return;
			}
			const StringName name = _region_at(texture_point);
			if (name != StringName()) {
				PackedStringArray selection = editor->get_selection();
				if (mb->is_command_or_control_pressed() || mb->is_shift_pressed()) {
					if (selection.has(name)) {
						selection.erase(name);
					} else {
						selection.push_back(name);
					}
					editor->set_selection(selection);
					accept_event();
					return;
				}
				if (!selection.has(name)) {
					editor->set_selection(PackedStringArray({ name }));
				}
				drag_mode = DRAG_MOVE;
				drag_name = name;
				drag_start_rect = atlas->get_region(name);
				drag_from = texture_point;
			} else {
				editor->set_selection(PackedStringArray());
				drag_mode = DRAG_CREATE;
				drag_from = Vector2(_snap(texture_point));
				drag_new_rect = Rect2i(Vector2i(drag_from), Vector2i());
			}
			accept_event();
			return;
		}

		// Release: one undo action for the whole drag.
		const DragMode finished = drag_mode;
		drag_mode = DRAG_NONE;
		drag_handle = -1;
		if (finished == DRAG_CREATE) {
			Rect2i rect = drag_new_rect;
			const Ref<Texture2D> texture = atlas->get_sheet_texture();
			if (texture.is_valid()) {
				rect = rect.intersection(Rect2i(Point2i(), Size2i(texture->get_size())));
			}
			if (rect.size.x > 0 && rect.size.y > 0) {
				const String name = editor->make_new_region_name();
				if (!name.is_empty()) {
					TypedDictionary<StringName, Rect2i> regions = atlas->get_regions();
					regions[StringName(name)] = rect;
					TextureAtlasEditing::commit_regions(TTR("Add Region"), atlas, regions, atlas->get_margins());
					editor->set_selection(PackedStringArray({ name }));
				}
			}
		} else if (finished == DRAG_MOVE || finished == DRAG_RESIZE) {
			const TypedDictionary<StringName, Rect2i> new_regions = atlas->get_regions();
			if (!(Dictionary(new_regions) == Dictionary(drag_old_regions))) {
				TextureAtlasEditing::commit(finished == DRAG_MOVE ? TTR("Move Region") : TTR("Resize Region"), atlas, drag_old_regions, drag_old_margins, new_regions, atlas->get_margins());
			}
		}
		queue_redraw();
		accept_event();
		return;
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (drag_mode == DRAG_NONE) {
			return;
		}
		if (drag_mode == DRAG_PAN) {
			view_offset += mm->get_position() - drag_from;
			drag_from = mm->get_position();
			queue_redraw();
			accept_event();
			return;
		}
		const Vector2 texture_point = _to_texture(mm->get_position());
		if (drag_mode == DRAG_CREATE) {
			const Vector2i to = _snap(texture_point);
			const Vector2i from = Vector2i(drag_from);
			drag_new_rect = Rect2i(from.min(to), (to - from).abs());
			queue_redraw();
		} else if (drag_mode == DRAG_MOVE) {
			// Preview on the atlas (no undo); the release commits one action.
			const Vector2i delta = _snap(Vector2(drag_start_rect.position) + texture_point - drag_from) - drag_start_rect.position;
			const PackedStringArray &selection = editor->get_selection();
			for (const String &name : selection) {
				if (!drag_old_regions.has(StringName(name))) {
					continue;
				}
				Rect2i rect = drag_old_regions[StringName(name)];
				rect.position += delta;
				atlas->set_region(name, _clamp_to_texture(rect));
			}
		} else if (drag_mode == DRAG_RESIZE) {
			const Vector2i point = _snap(texture_point);
			Vector2i from = drag_start_rect.position;
			Vector2i to = drag_start_rect.get_end();
			// Handles: 0 top-left, 1 top, 2 top-right, 3 right, 4 bottom-right, 5 bottom, 6 bottom-left, 7 left.
			if (drag_handle == 0 || drag_handle == 6 || drag_handle == 7) {
				from.x = MIN(point.x, to.x - 1);
			}
			if (drag_handle == 2 || drag_handle == 3 || drag_handle == 4) {
				to.x = MAX(point.x, from.x + 1);
			}
			if (drag_handle == 0 || drag_handle == 1 || drag_handle == 2) {
				from.y = MIN(point.y, to.y - 1);
			}
			if (drag_handle == 4 || drag_handle == 5 || drag_handle == 6) {
				to.y = MAX(point.y, from.y + 1);
			}
			Rect2i rect(from, to - from);
			const Ref<Texture2D> texture = atlas->get_sheet_texture();
			if (texture.is_valid()) {
				rect = rect.intersection(Rect2i(Point2i(), Size2i(texture->get_size())));
			}
			if (rect.size.x > 0 && rect.size.y > 0) {
				atlas->set_region(drag_name, rect);
			}
		}
		accept_event();
		return;
	}

	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && drag_mode == DRAG_NONE) {
		const PackedStringArray &selection = editor->get_selection();
		if (selection.is_empty()) {
			return;
		}
		if (k->get_keycode() == Key::KEY_DELETE) {
			TextureAtlasEditing::remove(atlas, selection);
			editor->set_selection(PackedStringArray());
			accept_event();
			return;
		}
		Vector2i direction;
		switch (k->get_keycode()) {
			case Key::LEFT:
				direction = Vector2i(-1, 0);
				break;
			case Key::RIGHT:
				direction = Vector2i(1, 0);
				break;
			case Key::UP:
				direction = Vector2i(0, -1);
				break;
			case Key::DOWN:
				direction = Vector2i(0, 1);
				break;
			default:
				return;
		}
		// Arrow keys move the selected regions by one pixel, with Shift by one grid step.
		const Vector2i step = k->is_shift_pressed() ? editor->get_grid_step() : Vector2i(1, 1);
		TypedDictionary<StringName, Rect2i> regions = atlas->get_regions();
		for (const String &name : selection) {
			if (!regions.has(StringName(name))) {
				continue;
			}
			Rect2i rect = regions[StringName(name)];
			rect.position += direction * step;
			regions[StringName(name)] = _clamp_to_texture(rect);
		}
		TextureAtlasEditing::commit_regions(TTR("Nudge Regions"), atlas, regions, atlas->get_margins(), UndoRedo::MERGE_ENDS);
		accept_event();
	}
}

TextureAtlasCanvas::TextureAtlasCanvas() {
	set_clip_contents(true);
	set_focus_mode(FOCUS_ALL);
	set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_custom_minimum_size(Size2(200, 150) * EDSCALE);
}

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasEditor

TextureAtlasEditor *TextureAtlasEditor::singleton = nullptr;

void TextureAtlasEditor::edit(const Ref<SpriteSheet> &p_atlas) {
	if (atlas == p_atlas) {
		return;
	}
	if (atlas.is_valid()) {
		atlas->disconnect_changed(callable_mp(this, &TextureAtlasEditor::_atlas_changed));
	}
	atlas = p_atlas;
	selection.clear();
	if (atlas.is_valid()) {
		atlas->connect_changed(callable_mp(this, &TextureAtlasEditor::_atlas_changed));
	}
	region_list->set_atlas(atlas);
	_atlas_changed();
	callable_mp(canvas, &TextureAtlasCanvas::fit).call_deferred();
}

void TextureAtlasEditor::_atlas_changed() {
	// Drop names that do not exist any more (deleted, renamed or undone).
	PackedStringArray kept;
	for (const String &name : selection) {
		if (atlas.is_valid() && atlas->has_region(name)) {
			kept.push_back(name);
		}
	}
	selection = kept;
	const String text = TextureAtlasEditing::get_texture_warning(atlas);
	warning->set_text(text);
	warning->set_visible(!text.is_empty());
	canvas->queue_redraw();
}

void TextureAtlasEditor::set_selection(const PackedStringArray &p_names) {
	selection = p_names;
	region_list->set_selected_names(selection);
	canvas->queue_redraw();
}

void TextureAtlasEditor::_list_selection_changed() {
	selection = region_list->get_selected_names();
	canvas->queue_redraw();
}

bool TextureAtlasEditor::is_grid_snap() const {
	return snap_mode->get_selected() == SNAP_GRID;
}

Vector2i TextureAtlasEditor::get_grid_step() const {
	return Vector2i(MAX(1, int(grid_step_x->get_value())), MAX(1, int(grid_step_y->get_value())));
}

Vector2i TextureAtlasEditor::get_grid_offset() const {
	return Vector2i(int(grid_offset_x->get_value()), int(grid_offset_y->get_value()));
}

String TextureAtlasEditor::_get_prefix() const {
	return name_prefix->get_text();
}

String TextureAtlasEditor::make_new_region_name() const {
	if (atlas.is_null()) {
		return String();
	}
	if (!SpriteSheet::is_valid_region_name(_get_prefix() + "0")) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("\"%s\" cannot start a region name."), _get_prefix()));
		return String();
	}
	return atlas->make_unique_region_name(_get_prefix());
}

void TextureAtlasEditor::_snap_changed(int p_index) {
	const bool grid = p_index == SNAP_GRID;
	grid_step_x->set_editable(grid);
	grid_step_y->set_editable(grid);
	grid_offset_x->set_editable(grid);
	grid_offset_y->set_editable(grid);
	canvas->queue_redraw();
}

void TextureAtlasEditor::_grid_value_changed(double p_value) {
	canvas->queue_redraw();
}

void TextureAtlasEditor::_auto_slice() {
	if (atlas.is_null()) {
		return;
	}
	const String warning_text = TextureAtlasEditing::get_texture_warning(atlas);
	if (!warning_text.is_empty()) {
		EditorNode::get_singleton()->show_warning(warning_text);
		return;
	}
	const TypedArray<Rect2i> found = SpriteSheet::detect_sprite_rects(atlas->get_sheet_texture()->get_image());
	// Like SpriteSheet.auto_slice(): sprites that are regions already are skipped.
	TypedArray<Rect2i> new_rects;
	const TypedDictionary<StringName, Rect2i> regions = atlas->get_regions();
	const Array existing = Dictionary(regions).values();
	for (int i = 0; i < found.size(); i++) {
		if (!existing.has(found[i])) {
			new_rects.push_back(found[i]);
		}
	}
	if (new_rects.is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("Auto Slice found no new sprites."));
		return;
	}
	TextureAtlasEditing::add(atlas, TTR("Auto Slice"), new_rects, _get_prefix());
}

void TextureAtlasEditor::_add_from_grid() {
	if (atlas.is_null()) {
		return;
	}
	grid_dialog->popup_centered();
}

void TextureAtlasEditor::_add_from_grid_confirmed() {
	if (atlas.is_null() || atlas->get_sheet_texture().is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Add from Grid needs a texture."));
		return;
	}
	const Vector2i cell(int(grid_cell[0]->get_value()), int(grid_cell[1]->get_value()));
	const Vector2i separation(int(grid_separation[0]->get_value()), int(grid_separation[1]->get_value()));
	const Vector2i offset(int(grid_offset[0]->get_value()), int(grid_offset[1]->get_value()));
	const TypedArray<Rect2i> cells = SpriteSheet::make_grid_rects(Vector2i(atlas->get_sheet_texture()->get_size()), cell, separation, offset);
	if (cells.is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("No cell of this size fits into the texture."));
		return;
	}
	TextureAtlasEditing::add(atlas, TTR("Add Regions from Grid"), cells, _get_prefix());
}

void TextureAtlasEditor::_sort() {
	if (atlas.is_null()) {
		return;
	}
	Vector<String> names;
	for (const String &name : atlas->get_region_names()) {
		names.push_back(name);
	}
	names.sort_custom<NaturalNoCaseComparator>();
	const TypedDictionary<StringName, Rect2i> old_regions = atlas->get_regions();
	TypedDictionary<StringName, Rect2i> regions;
	for (const String &name : names) {
		regions[StringName(name)] = old_regions[StringName(name)];
	}
	TextureAtlasEditing::commit_regions(TTR("Sort Regions"), atlas, regions, atlas->get_margins());
}

void TextureAtlasEditor::_delete_selected() {
	if (atlas.is_null()) {
		return;
	}
	TextureAtlasEditing::remove(atlas, selection);
	set_selection(PackedStringArray());
}

void TextureAtlasEditor::_create_sprite_frames() {
	if (atlas.is_null() || atlas->get_region_count() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("The atlas has no regions."));
		return;
	}
	if (!atlas->get_path().is_empty() && atlas->get_path().is_resource_file()) {
		frames_dialog->set_current_path(atlas->get_path().get_basename() + "_frames.tres");
	}
	frames_dialog->popup_file_dialog();
}

void TextureAtlasEditor::_create_sprite_frames_confirmed(const String &p_path) {
	if (atlas.is_null()) {
		return;
	}
	// The selected regions (all if none is selected) in a copy of the atlas, so names and frame order
	// follow SpriteSheet.create_sprite_frames().
	Ref<SpriteSheet> copy;
	copy.instantiate();
	copy->set_texture(atlas->get_sheet_texture());
	copy->set_filter_clip(atlas->is_filter_clip());
	TypedDictionary<StringName, Rect2i> regions;
	TypedDictionary<StringName, Rect2i> margins;
	for (const String &name : atlas->get_region_names()) {
		if (!selection.is_empty() && !selection.has(name)) {
			continue;
		}
		regions[StringName(name)] = atlas->get_region(name);
		margins[StringName(name)] = atlas->get_region_margin(name);
	}
	copy->set_regions(regions);
	copy->set_margins(margins);
	const Ref<SpriteFrames> frames = copy->create_sprite_frames();
	const Error err = ResourceSaver::save(frames, p_path, ResourceSaver::FLAG_CHANGE_PATH);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Saving \"%s\" failed: %s."), p_path, error_names[err]));
		return;
	}
	EditorFileSystem::get_singleton()->update_file(p_path);
	EditorNode::get_singleton()->edit_resource(frames);
}

void TextureAtlasEditor::_assign_to_nodes() {
	if (atlas.is_null() || selection.size() != 1) {
		EditorNode::get_singleton()->show_warning(TTR("Select exactly one region."));
		return;
	}
	const StringName name = selection[0];
	List<Node *> nodes = EditorNode::get_singleton()->get_editor_selection()->get_top_selected_node_list();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	bool any = false;
	for (Node *node : nodes) {
		// Sprite2D, Sprite3D, TextureRect, TextureButton (normal) and any node with a Texture2D "texture" property.
		bool valid = false;
		const Variant old_texture = node->get("texture", &valid);
		if (!valid || (old_texture.get_type() != Variant::NIL && !Object::cast_to<Texture2D>(old_texture.get_validated_object()))) {
			continue;
		}
		if (!any) {
			undo_redo->create_action(TTR("Assign Region Texture"));
			any = true;
		}
		undo_redo->add_do_property(node, "texture", atlas->get_region_texture(name));
		undo_redo->add_undo_property(node, "texture", old_texture);
	}
	if (!any) {
		EditorNode::get_singleton()->show_warning(TTR("Select a Sprite2D (or another node with a \"texture\" property) in the Scene dock first."));
		return;
	}
	undo_redo->commit_action();
}

void TextureAtlasEditor::_import() {
	if (atlas.is_null()) {
		return;
	}
	import_dialog->popup_file_dialog();
}

void TextureAtlasEditor::_import_confirmed(const String &p_path) {
	if (atlas.is_null()) {
		return;
	}
	// Load into a copy (all-or-nothing, errors are printed by the atlas), then apply it with undo.
	Ref<SpriteSheet> copy;
	copy.instantiate();
	copy->set_texture(atlas->get_sheet_texture());
	copy->set_texture_path(atlas->get_texture_path());
	if (copy->load_from_file(p_path) != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Importing \"%s\" failed. See the Output panel for details."), p_path));
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Import Regions"), UndoRedo::MERGE_DISABLE, atlas.ptr());
	// Regions are cleared first, so a smaller texture does not report the old regions as outside.
	undo_redo->add_do_method(atlas.ptr(), "clear_regions");
	undo_redo->add_do_method(atlas.ptr(), "set_texture", copy->get_sheet_texture());
	undo_redo->add_do_method(atlas.ptr(), "set_texture_path", copy->get_texture_path());
	undo_redo->add_do_method(atlas.ptr(), "set_regions", copy->get_regions());
	undo_redo->add_do_method(atlas.ptr(), "set_margins", copy->get_margins());
	undo_redo->add_undo_method(atlas.ptr(), "clear_regions");
	undo_redo->add_undo_method(atlas.ptr(), "set_texture", atlas->get_sheet_texture());
	undo_redo->add_undo_method(atlas.ptr(), "set_texture_path", atlas->get_texture_path());
	undo_redo->add_undo_method(atlas.ptr(), "set_regions", atlas->get_regions());
	undo_redo->add_undo_method(atlas.ptr(), "set_margins", atlas->get_margins());
	undo_redo->commit_action();
	callable_mp(canvas, &TextureAtlasCanvas::fit).call_deferred();
}

void TextureAtlasEditor::_export() {
	if (atlas.is_null()) {
		return;
	}
	if (!atlas->get_path().is_empty() && atlas->get_path().is_resource_file()) {
		export_dialog->set_current_path(atlas->get_path().get_basename() + ".json");
	}
	export_dialog->popup_file_dialog();
}

void TextureAtlasEditor::_export_confirmed(const String &p_path) {
	if (atlas.is_null()) {
		return;
	}
	const Error err = atlas->save_to_file(p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Exporting \"%s\" failed: %s."), p_path, error_names[err]));
		return;
	}
	if (p_path.begins_with("res://")) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	}
}

void TextureAtlasEditor::_zoom(float p_factor) {
	canvas->set_zoom(canvas->get_zoom() * p_factor, canvas->get_size() / 2);
}

void TextureAtlasEditor::_zoom_reset() {
	canvas->set_zoom(1.0f, canvas->get_size() / 2);
}

void TextureAtlasEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			warning->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

static SpinBox *_make_spin_box(const String &p_tooltip, double p_min, double p_value) {
	SpinBox *spin_box = memnew(SpinBox);
	spin_box->set_min(p_min);
	spin_box->set_max(16384);
	spin_box->set_step(1);
	spin_box->set_value(p_value);
	spin_box->set_tooltip_text(p_tooltip);
	spin_box->set_accessibility_name(p_tooltip);
	return spin_box;
}

static Button *_make_button(Control *p_parent, const String &p_text, const String &p_tooltip) {
	Button *button = memnew(Button);
	button->set_text(p_text);
	button->set_tooltip_text(p_tooltip);
	button->set_flat(true);
	p_parent->add_child(button);
	return button;
}

TextureAtlasEditor::TextureAtlasEditor() {
	singleton = this;

	set_name(TTRC("SpriteSheet"));
	set_icon_name("SpriteSheet");
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	// Toolbar.
	HFlowContainer *toolbar_box = memnew(HFlowContainer);
	main_vb->add_child(toolbar_box);

	snap_mode = memnew(OptionButton);
	snap_mode->add_item(TTR("Pixel Snap"), SNAP_PIXEL);
	snap_mode->add_item(TTR("Grid Snap"), SNAP_GRID);
	snap_mode->set_accessibility_name(TTRC("Snap Mode"));
	snap_mode->connect(SceneStringName(item_selected), callable_mp(this, &TextureAtlasEditor::_snap_changed));
	toolbar_box->add_child(snap_mode);

	grid_step_x = _make_spin_box(TTR("Grid Step X"), 1, 16);
	grid_step_y = _make_spin_box(TTR("Grid Step Y"), 1, 16);
	grid_offset_x = _make_spin_box(TTR("Grid Offset X"), 0, 0);
	grid_offset_y = _make_spin_box(TTR("Grid Offset Y"), 0, 0);
	for (SpinBox *spin_box : { grid_step_x, grid_step_y, grid_offset_x, grid_offset_y }) {
		spin_box->set_editable(false);
		spin_box->connect(SceneStringName(value_changed), callable_mp(this, &TextureAtlasEditor::_grid_value_changed));
		toolbar_box->add_child(spin_box);
	}

	toolbar_box->add_child(memnew(VSeparator));
	Label *prefix_label = memnew(Label);
	prefix_label->set_text(TTR("Name Prefix:"));
	toolbar_box->add_child(prefix_label);
	name_prefix = memnew(LineEdit);
	name_prefix->set_text("sprite_");
	name_prefix->set_custom_minimum_size(Size2(90, 0) * EDSCALE);
	name_prefix->set_tooltip_text(TTR("New regions are named with this prefix and a number: sprite_0, sprite_1, ..."));
	name_prefix->set_accessibility_name(TTRC("Name Prefix"));
	toolbar_box->add_child(name_prefix);

	toolbar_box->add_child(memnew(VSeparator));
	_make_button(toolbar_box, TTR("Auto Slice"), TTR("Add a region for every sprite (group of opaque pixels) that is not a region yet."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_auto_slice));
	_make_button(toolbar_box, TTR("Add from Grid..."), TTR("Add equal regions from a grid of cells."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_add_from_grid));
	_make_button(toolbar_box, TTR("Sort"), TTR("Sort the regions by name (natural order: walk_2 before walk_10)."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_sort));
	_make_button(toolbar_box, TTR("Delete"), TTR("Delete the selected regions (Delete key in the canvas)."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_delete_selected));

	toolbar_box->add_child(memnew(VSeparator));
	_make_button(toolbar_box, TTR("Create SpriteFrames..."), TTR("Save a SpriteFrames resource made from the selected regions (all regions if none is selected) and open it."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_create_sprite_frames));
	_make_button(toolbar_box, TTR("Assign to Selected Nodes"), TTR("Set the texture of the selected Sprite2D nodes (or other nodes with a \"texture\" property) to the selected region."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_assign_to_nodes));

	toolbar_box->add_child(memnew(VSeparator));
	_make_button(toolbar_box, TTR("Import..."), TTR("Replace the regions with a JSON, CSV or INI atlas file or a TexturePacker JSON file."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_import));
	_make_button(toolbar_box, TTR("Export..."), TTR("Save the regions as a JSON, CSV or INI atlas file."))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_export));

	toolbar_box->add_child(memnew(VSeparator));
	_make_button(toolbar_box, "-", TTR("Zoom Out"))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_zoom).bind(0.8f));
	_make_button(toolbar_box, "1:1", TTR("Zoom Reset"))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_zoom_reset));
	_make_button(toolbar_box, "+", TTR("Zoom In"))->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasEditor::_zoom).bind(1.25f));

	warning = memnew(Label);
	warning->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	warning->hide();
	main_vb->add_child(warning);

	HSplitContainer *split = memnew(HSplitContainer);
	split->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vb->add_child(split);

	canvas = memnew(TextureAtlasCanvas);
	canvas->editor = this;
	split->add_child(canvas);

	region_list = memnew(TextureAtlasRegionList);
	region_list->set_custom_minimum_size(Size2(260, 0) * EDSCALE);
	region_list->selection_changed_callback = callable_mp(this, &TextureAtlasEditor::_list_selection_changed);
	split->add_child(region_list);

	// Dialogs.
	grid_dialog = memnew(ConfirmationDialog);
	grid_dialog->set_title(TTR("Add Regions from Grid"));
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(3);
	grid_dialog->add_child(grid);
	const String grid_labels[3] = { TTR("Cell Size:"), TTR("Separation:"), TTR("Offset:") };
	SpinBox **grid_boxes[3] = { grid_cell, grid_separation, grid_offset };
	for (int row = 0; row < 3; row++) {
		Label *label = memnew(Label);
		label->set_text(grid_labels[row]);
		grid->add_child(label);
		for (int axis = 0; axis < 2; axis++) {
			grid_boxes[row][axis] = _make_spin_box(grid_labels[row] + (axis == 0 ? " X" : " Y"), row == 0 ? 1 : 0, row == 0 ? 32 : 0);
			grid->add_child(grid_boxes[row][axis]);
		}
	}
	grid_dialog->connect(SceneStringName(confirmed), callable_mp(this, &TextureAtlasEditor::_add_from_grid_confirmed));
	add_child(grid_dialog);

	import_dialog = memnew(EditorFileDialog);
	import_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	import_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	import_dialog->set_title(TTR("Import Atlas Regions"));
	import_dialog->add_filter("*.json", TTR("JSON (own layout or TexturePacker)"));
	import_dialog->add_filter("*.csv", TTR("CSV"));
	import_dialog->add_filter("*.ini", TTR("INI"));
	import_dialog->connect("file_selected", callable_mp(this, &TextureAtlasEditor::_import_confirmed));
	add_child(import_dialog);

	export_dialog = memnew(EditorFileDialog);
	export_dialog->set_file_mode(FileDialog::FILE_MODE_SAVE_FILE);
	export_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	export_dialog->set_title(TTR("Export Atlas Regions"));
	export_dialog->add_filter("*.json", TTR("JSON"));
	export_dialog->add_filter("*.csv", TTR("CSV"));
	export_dialog->add_filter("*.ini", TTR("INI"));
	export_dialog->connect("file_selected", callable_mp(this, &TextureAtlasEditor::_export_confirmed));
	add_child(export_dialog);

	frames_dialog = memnew(EditorFileDialog);
	frames_dialog->set_file_mode(FileDialog::FILE_MODE_SAVE_FILE);
	frames_dialog->set_access(FileDialog::ACCESS_RESOURCES);
	frames_dialog->set_title(TTR("Save SpriteFrames"));
	frames_dialog->add_filter("*.tres", TTR("SpriteFrames"));
	frames_dialog->connect("file_selected", callable_mp(this, &TextureAtlasEditor::_create_sprite_frames_confirmed));
	add_child(frames_dialog);
}

TextureAtlasEditor::~TextureAtlasEditor() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasInspector

void TextureAtlasInspector::_atlas_changed() {
	const String text = TextureAtlasEditing::get_texture_warning(atlas);
	warning->set_text(text);
	warning->set_visible(!text.is_empty());
}

void TextureAtlasInspector::_edit_regions() {
	TextureAtlasEditor *editor = TextureAtlasEditor::get_singleton();
	ERR_FAIL_NULL(editor);
	editor->edit(atlas);
	editor->make_visible();
	editor->set_selection(region_list->get_selected_names());
}

void TextureAtlasInspector::_add_region() {
	Rect2i rect(0, 0, 16, 16);
	const Ref<Texture2D> texture = atlas->get_sheet_texture();
	if (texture.is_valid()) {
		rect.size = rect.size.min(Size2i(texture->get_size())).maxi(1);
	}
	TextureAtlasEditing::add(atlas, TTR("Add Region"), TypedArray<Rect2i>({ rect }), "sprite_");
}

void TextureAtlasInspector::_delete_regions() {
	TextureAtlasEditing::remove(atlas, region_list->get_selected_names());
}

void TextureAtlasInspector::_selection_changed() {
	delete_button->set_disabled(region_list->get_selected_names().is_empty());
}

TextureAtlasInspector::TextureAtlasInspector(const Ref<SpriteSheet> &p_atlas) {
	atlas = p_atlas;

	warning = memnew(Label);
	warning->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	warning->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("warning_color"), EditorStringName(Editor)));
	add_child(warning);

	EditorInspectorActionButton *edit_button = memnew(EditorInspectorActionButton(TTRC("Edit Regions"), SNAME("RegionEdit")));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasInspector::_edit_regions));
	add_child(edit_button);

	HBoxContainer *buttons = memnew(HBoxContainer);
	add_child(buttons);
	Button *add_button = memnew(Button);
	add_button->set_text(TTR("Add Region"));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasInspector::_add_region));
	buttons->add_child(add_button);
	delete_button = memnew(Button);
	delete_button->set_text(TTR("Delete Selected"));
	delete_button->set_disabled(true);
	delete_button->connect(SceneStringName(pressed), callable_mp(this, &TextureAtlasInspector::_delete_regions));
	buttons->add_child(delete_button);

	region_list = memnew(TextureAtlasRegionList);
	region_list->set_tree_minimum_height(int(220 * EDSCALE));
	region_list->selection_changed_callback = callable_mp(this, &TextureAtlasInspector::_selection_changed);
	add_child(region_list);
	region_list->set_atlas(atlas);

	atlas->connect_changed(callable_mp(this, &TextureAtlasInspector::_atlas_changed));
	_atlas_changed();
}

/////////////////////////////////////////////////////////////////////////////
// EditorInspectorPluginTextureAtlas

bool EditorInspectorPluginTextureAtlas::can_handle(Object *p_object) {
	return Object::cast_to<SpriteSheet>(p_object) != nullptr;
}

void EditorInspectorPluginTextureAtlas::parse_begin(Object *p_object) {
	Ref<SpriteSheet> atlas(Object::cast_to<SpriteSheet>(p_object));
	ERR_FAIL_COND(atlas.is_null());
	add_custom_control(memnew(TextureAtlasInspector(atlas)));
}

/////////////////////////////////////////////////////////////////////////////
// TextureAtlasEditorPlugin

void TextureAtlasEditorPlugin::edit(Object *p_object) {
	// Keep the last atlas when the selection moves to a node, so "Assign to Selected Nodes" works.
	Ref<SpriteSheet> atlas(Object::cast_to<SpriteSheet>(p_object));
	if (atlas.is_valid()) {
		atlas_editor->edit(atlas);
	}
}

bool TextureAtlasEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<SpriteSheet>(p_object) != nullptr;
}

void TextureAtlasEditorPlugin::make_visible(bool p_visible) {
	// The dock stays open when another object is edited; it is closed by the user.
	if (p_visible) {
		atlas_editor->make_visible();
	}
}

TextureAtlasEditorPlugin::TextureAtlasEditorPlugin() {
	atlas_editor = memnew(TextureAtlasEditor);
	atlas_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	EditorDockManager::get_singleton()->add_dock(atlas_editor);
	atlas_editor->close();

	Ref<EditorInspectorPluginTextureAtlas> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}
