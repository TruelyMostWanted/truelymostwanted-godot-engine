/**************************************************************************/
/*  sprite_sheet_editor_plugin.h                                          */
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

#include "core/object/undo_redo.h"
#include "editor/docks/editor_dock.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/resources/sprite_sheet.h"

class Button;
class ConfirmationDialog;
class EditorFileDialog;
class Label;
class LineEdit;
class OptionButton;
class SpinBox;
class Tree;
class SpriteSheetEditor;
class TextureRegionEditor;
class TreeItem;

// Undoable changes of the regions of a SpriteSheet: every change is a snapshot of `regions` and `margins`
// (do: the new state, undo: the old state), so deletes, renames and sorting restore the order as well.
class SpriteSheetEditing {
public:
	static void commit(const String &p_action, const Ref<SpriteSheet> &p_atlas, const TypedDictionary<StringName, Rect2i> &p_old_regions, const TypedDictionary<StringName, Rect2i> &p_old_margins, const TypedDictionary<StringName, Rect2i> &p_new_regions, const TypedDictionary<StringName, Rect2i> &p_new_margins, UndoRedo::MergeMode p_merge = UndoRedo::MERGE_DISABLE);
	// Old state = the current state of the atlas.
	static void commit_regions(const String &p_action, const Ref<SpriteSheet> &p_atlas, const TypedDictionary<StringName, Rect2i> &p_new_regions, const TypedDictionary<StringName, Rect2i> &p_new_margins, UndoRedo::MergeMode p_merge = UndoRedo::MERGE_DISABLE);
	static void remove(const Ref<SpriteSheet> &p_atlas, const PackedStringArray &p_names);
	static void rename(const Ref<SpriteSheet> &p_atlas, const StringName &p_from, const StringName &p_to);
	static PackedStringArray add(const Ref<SpriteSheet> &p_atlas, const String &p_action, const TypedArray<Rect2i> &p_rects, const String &p_prefix);
	// Text "x, y, width, height" to a rectangle.
	static bool parse_rect(const String &p_text, Rect2i &r_rect);
	// The rectangle of the region editor dialog (float) in whole pixels inside the texture (no clamping if
	// p_texture_size is empty). False if nothing of it is left.
	static bool rect_from_region_editor(const Rect2 &p_rect, const Size2i &p_texture_size, Rect2i &r_rect);
	// A warning for the inspector and the dock, empty if the texture is usable.
	static String get_texture_warning(const Ref<SpriteSheet> &p_atlas);
};

// List of the regions with thumbnail, name and rectangle; rename and rectangle editing in place, filter,
// multi-select, and drag of a sprite onto a Texture2D property. Used in the inspector and in the dock.
class SpriteSheetRegionList : public VBoxContainer {
	GDCLASS(SpriteSheetRegionList, VBoxContainer);

	Ref<SpriteSheet> atlas;
	LineEdit *filter = nullptr;
	Tree *tree = nullptr;
	bool update_queued = false;
	bool updating = false;

	void _atlas_changed();
	void _item_edited();
	void _multi_selected(Object *p_item, int p_column, bool p_selected);
	void _button_clicked(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _icon_double_clicked();
	void _edit_region(TreeItem *p_item);
	void _emit_selection_changed();
	Variant _get_drag_data_fw(const Point2 &p_point, Control *p_from);

public:
	// Called (deferred) when the user changes the selection.
	Callable selection_changed_callback;

	void set_atlas(const Ref<SpriteSheet> &p_atlas);
	void update_list();
	PackedStringArray get_selected_names() const;
	void set_selected_names(const PackedStringArray &p_names);
	void set_tree_minimum_height(int p_height);

	SpriteSheetRegionList();
};

// The sprite sheet with all regions: zoom, pan, select, draw, move, resize, snap and nudge.
class SpriteSheetCanvas : public Control {
	GDCLASS(SpriteSheetCanvas, Control);

	friend class SpriteSheetEditor;

	enum DragMode {
		DRAG_NONE,
		DRAG_PAN,
		DRAG_CREATE,
		DRAG_MOVE,
		DRAG_RESIZE,
	};

	SpriteSheetEditor *editor = nullptr;
	float zoom = 1.0f;
	Vector2 view_offset; // Screen position of the texture origin.
	bool fit_pending = false; // fit() ran while the dock was hidden: fit again at the first real size.
	DragMode drag_mode = DRAG_NONE;
	int drag_handle = -1;
	Vector2 drag_from; // In texture pixels (DRAG_PAN: in screen pixels).
	Rect2i drag_start_rect;
	Rect2i drag_new_rect;
	StringName drag_name;
	TypedDictionary<StringName, Rect2i> drag_old_regions;
	TypedDictionary<StringName, Rect2i> drag_old_margins;

	Vector2 _to_texture(const Vector2 &p_screen) const;
	Vector2 _to_screen(const Vector2 &p_texture) const;
	Vector2i _snap(const Vector2 &p_texture) const;
	Rect2i _get_texture_area() const;
	Rect2i _clamp_to_texture(const Rect2i &p_rect) const;
	StringName _region_at(const Vector2 &p_texture) const;
	int _handle_at(const Vector2 &p_screen) const;
	void _handle_positions(const Rect2i &p_rect, Vector2 r_points[8]) const;
	void _draw_canvas();

protected:
	void _notification(int p_what);

public:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;

	void set_zoom(float p_zoom, const Vector2 &p_center);
	float get_zoom() const { return zoom; }
	void fit();

	SpriteSheetCanvas();
};

// Bottom dock "SpriteSheet": canvas, region list, slicing, import and export.
class SpriteSheetEditor : public EditorDock {
	GDCLASS(SpriteSheetEditor, EditorDock);

	friend class SpriteSheetCanvas;

	enum SnapMode {
		SNAP_PIXEL,
		SNAP_GRID,
	};

	static SpriteSheetEditor *singleton;

	Ref<SpriteSheet> atlas;
	PackedStringArray selection;

	Label *warning = nullptr;
	OptionButton *snap_mode = nullptr;
	SpinBox *grid_step_x = nullptr;
	SpinBox *grid_step_y = nullptr;
	SpinBox *grid_offset_x = nullptr;
	SpinBox *grid_offset_y = nullptr;
	LineEdit *name_prefix = nullptr;
	SpriteSheetCanvas *canvas = nullptr;
	SpriteSheetRegionList *region_list = nullptr;

	ConfirmationDialog *grid_dialog = nullptr;
	SpinBox *grid_cell[2] = {};
	SpinBox *grid_separation[2] = {};
	SpinBox *grid_offset[2] = {};

	EditorFileDialog *import_dialog = nullptr;
	EditorFileDialog *export_dialog = nullptr;
	EditorFileDialog *frames_dialog = nullptr;

	// One region editor dialog for all regions, created on first use.
	TextureRegionEditor *region_dialog = nullptr;
	Ref<SpriteSheet> region_dialog_atlas;
	StringName region_dialog_name;
	Ref<AtlasTexture> region_dialog_texture;

	void _atlas_changed();
	void _inspector_object_changed();
	void _filesystem_changed();
	void _region_dialog_confirmed();
	void _region_dialog_visibility_changed();
	void _region_dialog_release();
	void _list_selection_changed();
	void _snap_changed(int p_index);
	void _grid_value_changed(double p_value);
	String _get_prefix() const;

	void _auto_slice();
	void _add_from_grid();
	void _add_from_grid_confirmed();
	void _sort();
	void _delete_selected();
	void _create_sprite_frames();
	void _create_sprite_frames_confirmed(const String &p_path);
	void _assign_to_nodes();
	void _import();
	void _import_confirmed(const String &p_path);
	void _export();
	void _export_confirmed(const String &p_path);
	void _zoom(float p_factor);
	void _zoom_reset();

protected:
	void _notification(int p_what);

public:
	static SpriteSheetEditor *get_singleton() { return singleton; }

	void edit(const Ref<SpriteSheet> &p_atlas);
	Ref<SpriteSheet> get_atlas() const { return atlas; }

	void set_selection(const PackedStringArray &p_names);
	const PackedStringArray &get_selection() const { return selection; }
	bool is_grid_snap() const;
	Vector2i get_grid_step() const;
	Vector2i get_grid_offset() const;
	String make_new_region_name() const;
	// Opens Godot's region editor dialog for one region; OK writes the rectangle back with one undo step.
	void edit_region(const Ref<SpriteSheet> &p_atlas, const StringName &p_name);

	SpriteSheetEditor();
	~SpriteSheetEditor();
};

// Inspector of a SpriteSheet: texture warning, "Edit Regions" button and the region list with add and delete.
class SpriteSheetInspector : public VBoxContainer {
	GDCLASS(SpriteSheetInspector, VBoxContainer);

	Ref<SpriteSheet> atlas;
	Label *warning = nullptr;
	SpriteSheetRegionList *region_list = nullptr;
	Button *delete_button = nullptr;

	void _atlas_changed();
	void _edit_regions();
	void _add_region();
	void _delete_regions();
	void _selection_changed();

public:
	SpriteSheetInspector(const Ref<SpriteSheet> &p_atlas);
};

class EditorInspectorPluginSpriteSheet : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginSpriteSheet, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_category(Object *p_object, const String &p_category) override;
};

class SpriteSheetEditorPlugin : public EditorPlugin {
	GDCLASS(SpriteSheetEditorPlugin, EditorPlugin);

	SpriteSheetEditor *atlas_editor = nullptr;

public:
	virtual String get_plugin_name() const override { return "SpriteSheet"; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	SpriteSheetEditorPlugin();
};
