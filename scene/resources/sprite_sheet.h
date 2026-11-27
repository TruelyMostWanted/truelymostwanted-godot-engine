/**************************************************************************/
/*  sprite_sheet.h                                                        */
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

#include "core/io/resource.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/serializer_options.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "core/variant/typed_dictionary.h"
#include "scene/resources/atlas_texture.h"

class Image;
class SpriteFrames;

// One sprite sheet (any Texture2D) with named regions of any size. Every region is available as an
// AtlasTexture that references the atlas texture and is kept in sync with the atlas.
// Like other resources, a SpriteSheet is not thread-safe: use it from one thread at a time.
class SpriteSheet : public Resource {
	GDCLASS(SpriteSheet, Resource);

public:
	// One region read from a text file or an import, before it is applied.
	struct Entry {
		StringName name;
		Rect2i rect;
		Rect2i margin;
	};

private:
	struct Region {
		Rect2i rect;
		Rect2i margin;
		Ref<AtlasTexture> texture; // Created on the first get_region_texture() call.
	};

	Ref<Texture2D> texture;
	String texture_path;
	bool filter_clip = false;
	HashMap<StringName, Region> regions;
	// Textures handed out for regions that are gone. A region that comes back under the same name (undo of a delete,
	// an import) gets its texture again, so the nodes that use it follow the region.
	HashMap<StringName, Ref<AtlasTexture>> detached_textures;

	void _update_cached_texture(Region &r_region) const;
	void _update_all_cached_textures();
	void _detach_texture(const StringName &p_name, const Region &p_region);
	Ref<AtlasTexture> _take_detached_texture(const StringName &p_name);
	void _replace_regions(HashMap<StringName, Region> &p_regions);
	static Error _check_rect(const String &p_name, const Rect2i &p_rect, const Ref<Texture2D> &p_texture, String &r_error);
	static Error _check_margin(const String &p_name, const Rect2i &p_margin, String &r_error);
	String _get_written_texture_path() const;
	String _write_json(const String &p_indent) const;
	String _write_csv() const;
	String _write_ini() const;
	Error _load_entries(Error p_parse_error, const String &p_texture_path, const LocalVector<Entry> &p_entries, const String &p_error, const String &p_format);

protected:
	static void _bind_methods();

public:
	static bool is_valid_region_name(const String &p_name);
	// TexturePacker file name ("hero/walk_01.png") to a region name ("hero_walk_01").
	static String texture_packer_name_to_region_name(const String &p_file_name);
	// Splits "walk_10" into "walk" and 10. Returns -1 (and the whole name) if the name does not end with a number.
	static int split_trailing_number(const String &p_name, String &r_base);

	// Auto slice: boxes around groups of touching opaque pixels, sorted in rows (top to bottom, left to right).
	static TypedArray<Rect2i> detect_sprite_rects(const Ref<Image> &p_image, float p_alpha_threshold = 0.1f);
	// Grid slice: all cells fully inside the area, row by row.
	static TypedArray<Rect2i> make_grid_rects(const Vector2i &p_area_size, const Vector2i &p_cell_size, const Vector2i &p_separation = Vector2i(), const Vector2i &p_offset = Vector2i());

	void set_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_sheet_texture() const;

	void set_texture_path(const String &p_path);
	String get_texture_path() const;

	void set_filter_clip(bool p_enable);
	bool is_filter_clip() const;

	void set_regions(const TypedDictionary<StringName, Rect2i> &p_regions);
	TypedDictionary<StringName, Rect2i> get_regions() const;

	void set_margins(const TypedDictionary<StringName, Rect2i> &p_margins);
	TypedDictionary<StringName, Rect2i> get_margins() const;

	// Storage only: the text resource format sorts dictionary keys, so the order of the regions is saved separately.
	void _set_region_order(const PackedStringArray &p_order);
	PackedStringArray _get_region_order() const;

	Error set_region(const StringName &p_name, const Rect2i &p_rect);
	void remove_region(const StringName &p_name);
	Error rename_region(const StringName &p_from, const StringName &p_to);
	bool has_region(const StringName &p_name) const;
	Rect2i get_region(const StringName &p_name) const;
	PackedStringArray get_region_names() const;
	int get_region_count() const;
	void clear_regions();

	String make_unique_region_name(const String &p_prefix) const;
	PackedStringArray add_regions(const TypedArray<Rect2i> &p_rects, const String &p_prefix = "sprite_");
	PackedStringArray auto_slice(const String &p_prefix = "sprite_", float p_alpha_threshold = 0.1f);

	Error set_region_margin(const StringName &p_name, const Rect2i &p_margin);
	Rect2i get_region_margin(const StringName &p_name) const;

	Ref<AtlasTexture> get_region_texture(const StringName &p_name);
	Ref<SpriteFrames> create_sprite_frames(const String &p_prefix = String(), double p_fps = 5.0);

	// Fixed text layouts (see the class reference): FORMAT_JSON, FORMAT_CSV and FORMAT_INI; other formats are ERR_UNAVAILABLE.
	Error load_from_text(const String &p_text, ResourceTextSerializer::Format p_format);
	String to_text(ResourceTextSerializer::Format p_format, const Ref<SerializerOptions> &p_options = Ref<SerializerOptions>()) const;
	// TexturePacker JSON (Hash or Array). `meta.image` is relative to `p_base_dir`.
	Error import_texture_packer_json(const String &p_text, const String &p_base_dir = String());

	// By extension (ResourceTextSerializer::get_format_for_extension()): .json (own layout, or TexturePacker if it
	// has a "frames" key), .csv, .ini.
	Error load_from_file(const String &p_path);
	Error save_to_file(const String &p_path) const;

	// Parsers without side effects or printing. The atlas is changed only by apply_entries().
	static Error parse_json(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error);
	static Error parse_csv(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error);
	static Error parse_ini(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error);
	static Error parse_texture_packer_json(const String &p_text, const String &p_base_dir, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error);
	// Loads the texture (unless the path names the current one), validates every entry and replaces all regions.
	// On error, the atlas is unchanged.
	Error apply_entries(const String &p_texture_path, const LocalVector<Entry> &p_entries, String &r_error);
};
