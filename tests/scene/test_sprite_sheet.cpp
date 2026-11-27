/**************************************************************************/
/*  test_sprite_sheet.cpp                                                 */
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

TEST_FORCE_LINK(test_sprite_sheet)

#include "core/io/image.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"
#include "scene/resources/sprite_sheet.h"

namespace TestSpriteSheet {

static Ref<ImageTexture> make_sheet(int p_width = 128, int p_height = 64) {
	return ImageTexture::create_from_image(Image::create_empty(p_width, p_height, false, Image::FORMAT_RGBA8));
}

// An atlas with three regions of different sizes and a fixed texture path, so text formats round-trip without a file.
static Ref<SpriteSheet> make_atlas(const Ref<Texture2D> &p_texture, const String &p_texture_path = "res://ui/icons.png") {
	Ref<SpriteSheet> atlas;
	atlas.instantiate();
	atlas->set_texture(p_texture);
	atlas->set_texture_path(p_texture_path);
	atlas->set_region("My_Sprite", Rect2i(0, 0, 100, 40));
	atlas->set_region("Coin", Rect2i(100, 0, 16, 16));
	atlas->set_region("Gem", Rect2i(100, 16, 28, 48));
	return atlas;
}

static void check_same_regions(const Ref<SpriteSheet> &p_a, const Ref<SpriteSheet> &p_b) {
	CHECK(p_a->get_region_names() == p_b->get_region_names());
	for (const String &name : p_a->get_region_names()) {
		CHECK(p_a->get_region(name) == p_b->get_region(name));
		CHECK(p_a->get_region_margin(name) == p_b->get_region_margin(name));
	}
}

TEST_CASE("[SceneTree][SpriteSheet] Regions keep their order and are validated") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);

	CHECK(atlas->get_region_count() == 3);
	CHECK(atlas->get_region_names() == PackedStringArray({ "My_Sprite", "Coin", "Gem" }));
	CHECK(atlas->get_region("Coin") == Rect2i(100, 0, 16, 16));

	// Rename keeps the position.
	CHECK(atlas->rename_region("Coin", "Gold") == OK);
	CHECK(atlas->get_region_names() == PackedStringArray({ "My_Sprite", "Gold", "Gem" }));

	ERR_PRINT_OFF;
	CHECK(atlas->rename_region("Gold", "Gem") == ERR_ALREADY_EXISTS);
	CHECK(atlas->set_region("Outside", Rect2i(120, 0, 16, 16)) == ERR_INVALID_PARAMETER);
	CHECK(atlas->set_region("Empty", Rect2i(0, 0, 0, 10)) == ERR_INVALID_PARAMETER);
	CHECK(atlas->set_region("a/b", Rect2i(0, 0, 1, 1)) == ERR_INVALID_PARAMETER);
	CHECK(atlas->set_region(" padded", Rect2i(0, 0, 1, 1)) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK_FALSE(atlas->has_region("Outside"));
	CHECK(atlas->get_region_count() == 3);

	atlas->remove_region("My_Sprite");
	CHECK(atlas->get_region_names() == PackedStringArray({ "Gold", "Gem" }));
	atlas->clear_regions();
	CHECK(atlas->get_region_count() == 0);

	CHECK(SpriteSheet::is_valid_region_name("walk_01"));
	CHECK_FALSE(SpriteSheet::is_valid_region_name(""));
	CHECK_FALSE(SpriteSheet::is_valid_region_name("a:b"));
	CHECK_FALSE(SpriteSheet::is_valid_region_name("a\"b"));
}

TEST_CASE("[SceneTree][SpriteSheet] Handed-out AtlasTextures stay linked to the atlas") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);

	Ref<AtlasTexture> coin = atlas->get_region_texture("Coin");
	REQUIRE(coin.is_valid());
	CHECK(coin == atlas->get_region_texture("Coin"));
	CHECK(coin->get_atlas() == sheet);
	CHECK(coin->get_region() == Rect2(100, 0, 16, 16));
	CHECK(coin->get_size() == Size2(16, 16));

	atlas->set_region("Coin", Rect2i(96, 8, 32, 32));
	CHECK(coin->get_region() == Rect2(96, 8, 32, 32));

	atlas->set_region_margin("Coin", Rect2i(2, 3, 4, 6));
	CHECK(coin->get_margin() == Rect2(2, 3, 4, 6));
	CHECK(coin->get_size() == Size2(36, 38));

	atlas->set_filter_clip(true);
	CHECK(coin->has_filter_clip());

	Ref<ImageTexture> bigger = make_sheet(256, 256);
	atlas->set_texture(bigger);
	CHECK(coin->get_atlas() == bigger);
	CHECK(atlas->get_texture_path().is_empty());

	// Rename keeps the handed-out texture.
	atlas->rename_region("Coin", "Gold");
	CHECK(atlas->get_region_texture("Gold") == coin);

	// A smaller texture keeps the regions but reports them.
	ERR_PRINT_OFF;
	atlas->set_texture(make_sheet(8, 8));
	CHECK(atlas->get_region_texture("Missing").is_null());
	ERR_PRINT_ON;
	CHECK(atlas->get_region_count() == 3);

	// set_regions() keeps regions outside the texture, so undo can restore them.
	TypedDictionary<StringName, Rect2i> regions = atlas->get_regions();
	atlas->set_regions(regions);
	CHECK(atlas->get_region_count() == 3);
	CHECK(atlas->get_region_texture("Gold") == coin);

	// A region that comes back under its name (undo of a delete, an import) gets its texture again.
	atlas->set_regions(TypedDictionary<StringName, Rect2i>());
	CHECK(atlas->get_region_count() == 0);
	atlas->set_regions(regions);
	CHECK(atlas->get_region_texture("Gold") == coin);
	atlas->remove_region("Gold");
	CHECK(atlas->set_region("Gold", Rect2i(0, 0, 4, 4)) == OK);
	CHECK(atlas->get_region_texture("Gold") == coin);
	CHECK(coin->get_region() == Rect2(0, 0, 4, 4));
}

TEST_CASE("[SceneTree][SpriteSheet] Works with GradientTexture2D") {
	Ref<GradientTexture2D> gradient;
	gradient.instantiate();
	gradient->set_width(64);
	gradient->set_height(32);
	Ref<SpriteSheet> atlas;
	atlas.instantiate();
	atlas->set_texture(gradient);
	CHECK(atlas->set_region("left", Rect2i(0, 0, 32, 32)) == OK);
	ERR_PRINT_OFF;
	CHECK(atlas->set_region("wide", Rect2i(0, 0, 65, 32)) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
	CHECK(atlas->get_region_texture("left")->get_atlas() == gradient);
}

TEST_CASE("[SceneTree][SpriteSheet] JSON round-trips with order and a prefixed texture path") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet, "mods://icons/sheet.png");
	atlas->set_region_margin("Gem", Rect2i(1, 2, 3, 4));

	const String json = atlas->to_text(ResourceTextSerializer::FORMAT_JSON);
	CHECK(json.contains("\"TexturePath\": \"mods://icons/sheet.png\""));
	CHECK(json.find("My_Sprite") < json.find("Coin"));
	CHECK(json.find("Coin") < json.find("Gem"));
	CHECK(json.contains("\"SourceWidth\": 31"));

	// Same texture path: the current texture is kept, nothing is loaded.
	Ref<SpriteSheet> copy;
	copy.instantiate();
	copy->set_texture(sheet);
	copy->set_texture_path("mods://icons/sheet.png");
	CHECK(copy->load_from_text(json, ResourceTextSerializer::FORMAT_JSON) == OK);
	check_same_regions(atlas, copy);
	CHECK(copy->get_texture_path() == "mods://icons/sheet.png");
	CHECK(copy->to_text(ResourceTextSerializer::FORMAT_JSON) == json);

	// Unknown keys are ignored.
	String path;
	LocalVector<SpriteSheet::Entry> entries;
	String error;
	CHECK(SpriteSheet::parse_json("{\"TexturePath\": \"res://a.png\", \"Extra\": 1, \"AtlasImages\": {\"a\": {\"X\": 1, \"Y\": 2, \"Width\": 3, \"Height\": 4, \"Note\": \"x\"}}}", path, entries, error) == OK);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].rect == Rect2i(1, 2, 3, 4));
	CHECK(path == "res://a.png");

	// Missing TexturePath and missing keys are errors.
	CHECK(SpriteSheet::parse_json("{\"AtlasImages\": {}}", path, entries, error) == ERR_PARSE_ERROR);
	CHECK(SpriteSheet::parse_json("{\"TexturePath\": \"res://a.png\", \"AtlasImages\": {\"a\": {\"X\": 1}}}", path, entries, error) == ERR_PARSE_ERROR);
	CHECK(error.contains("\"a\""));
}

TEST_CASE("[SceneTree][SpriteSheet] to_text uses SerializerOptions and rejects other formats") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);

	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_indent("\t");
	CHECK(atlas->to_text(ResourceTextSerializer::FORMAT_JSON, options).contains("\n\t\"AtlasImages\""));
	options->set_indent("");
	const String one_line = atlas->to_text(ResourceTextSerializer::FORMAT_JSON, options);
	CHECK_FALSE(one_line.contains("\n"));

	Ref<SpriteSheet> copy = make_atlas(sheet);
	copy->clear_regions();
	CHECK(copy->load_from_text(one_line, ResourceTextSerializer::FORMAT_JSON) == OK);
	check_same_regions(atlas, copy);

	ERR_PRINT_OFF;
	CHECK(atlas->to_text(ResourceTextSerializer::FORMAT_XML).is_empty());
	CHECK(copy->load_from_text("<SpriteSheet/>", ResourceTextSerializer::FORMAT_XML) == ERR_UNAVAILABLE);
	CHECK(copy->load_from_text("", ResourceTextSerializer::FORMAT_TOML) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	check_same_regions(atlas, copy);
}

TEST_CASE("[SceneTree][SpriteSheet] CSV and INI round-trip") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);
	atlas->set_region("Name, with comma", Rect2i(0, 40, 10, 10));

	SUBCASE("CSV without margins") {
		const String csv = atlas->to_text(ResourceTextSerializer::FORMAT_CSV);
		CHECK(csv.begins_with("TexturePath,res://ui/icons.png\nName,X,Y,Width,Height\nMy_Sprite,0,0,100,40\n"));
		Ref<SpriteSheet> copy = make_atlas(sheet);
		copy->clear_regions();
		CHECK(copy->load_from_text(csv, ResourceTextSerializer::FORMAT_CSV) == OK);
		check_same_regions(atlas, copy);
		CHECK(copy->to_text(ResourceTextSerializer::FORMAT_CSV) == csv);
	}

	SUBCASE("CSV with margins") {
		atlas->set_region_margin("Coin", Rect2i(1, 1, 2, 2));
		const String csv = atlas->to_text(ResourceTextSerializer::FORMAT_CSV);
		CHECK(csv.contains("Name,X,Y,Width,Height,MarginX,MarginY,SourceWidth,SourceHeight\n"));
		Ref<SpriteSheet> copy = make_atlas(sheet);
		CHECK(copy->load_from_text(csv, ResourceTextSerializer::FORMAT_CSV) == OK);
		check_same_regions(atlas, copy);
	}

	SUBCASE("INI") {
		atlas->set_region_margin("Gem", Rect2i(0, 0, 4, 0));
		const String ini = atlas->to_text(ResourceTextSerializer::FORMAT_INI);
		CHECK(ini.begins_with("[Atlas]\nTexturePath=res://ui/icons.png\n\n[My_Sprite]\nX=0\nY=0\nWidth=100\nHeight=40\n"));
		Ref<SpriteSheet> copy = make_atlas(sheet);
		copy->clear_regions();
		CHECK(copy->load_from_text(ini, ResourceTextSerializer::FORMAT_INI) == OK);
		check_same_regions(atlas, copy);
		CHECK(copy->to_text(ResourceTextSerializer::FORMAT_INI) == ini);
	}
}

TEST_CASE("[SceneTree][SpriteSheet] A failed load leaves the atlas unchanged") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);
	LocalVector<SpriteSheet::Entry> entries;
	SpriteSheet::Entry inside;
	inside.name = "inside";
	inside.rect = Rect2i(0, 0, 8, 8);
	SpriteSheet::Entry outside;
	outside.name = "outside";
	outside.rect = Rect2i(120, 60, 16, 16);
	entries.push_back(inside);
	entries.push_back(outside);

	String error;
	CHECK(atlas->apply_entries("res://ui/icons.png", entries, error) == ERR_INVALID_DATA);
	CHECK(error.contains("\"outside\""));
	CHECK(atlas->get_region_names() == PackedStringArray({ "My_Sprite", "Coin", "Gem" }));

	entries.clear();
	entries.push_back(inside);
	entries.push_back(inside);
	CHECK(atlas->apply_entries("res://ui/icons.png", entries, error) == ERR_INVALID_DATA);
	CHECK(atlas->get_region_count() == 3);
}

static const char *TEXTURE_PACKER_HASH = R"({
	"frames": {
		"hero/walk_01.png": { "frame": {"x": 0, "y": 0, "w": 30, "h": 40}, "rotated": false, "trimmed": true,
			"spriteSourceSize": {"x": 1, "y": 2, "w": 30, "h": 40}, "sourceSize": {"w": 32, "h": 48} },
		"coin.png": { "frame": {"x": 30, "y": 0, "w": 16, "h": 16}, "rotated": false, "trimmed": false,
			"spriteSourceSize": {"x": 0, "y": 0, "w": 16, "h": 16}, "sourceSize": {"w": 16, "h": 16} }
	},
	"meta": { "app": "https://www.codeandweb.com/texturepacker", "image": "sheet.png", "size": {"w": 128, "h": 64}, "scale": "1" }
})";

static const char *TEXTURE_PACKER_ARRAY = R"({
	"frames": [
		{ "filename": "coin.png", "frame": {"x": 30, "y": 0, "w": 16, "h": 16}, "rotated": false, "trimmed": false },
		{ "filename": "gem", "frame": {"x": 46, "y": 0, "w": 8, "h": 12}, "rotated": false, "trimmed": false }
	],
	"meta": { "image": "res://other/sheet.png" }
})";

TEST_CASE("[SpriteSheet] TexturePacker JSON, Hash and Array") {
	String path;
	LocalVector<SpriteSheet::Entry> entries;
	String error;

	CHECK(SpriteSheet::parse_texture_packer_json(TEXTURE_PACKER_HASH, "res://ui", path, entries, error) == OK);
	CHECK(path == "res://ui/sheet.png");
	REQUIRE(entries.size() == 2);
	CHECK(String(entries[0].name) == "hero_walk_01");
	CHECK(entries[0].rect == Rect2i(0, 0, 30, 40));
	CHECK(entries[0].margin == Rect2i(1, 2, 2, 8)); // Trimmed: original size 32x48.
	CHECK(String(entries[1].name) == "coin");
	CHECK(entries[1].margin == Rect2i());

	CHECK(SpriteSheet::parse_texture_packer_json(TEXTURE_PACKER_ARRAY, "res://ui", path, entries, error) == OK);
	CHECK(path == "res://other/sheet.png");
	REQUIRE(entries.size() == 2);
	CHECK(String(entries[1].name) == "gem");
	CHECK(entries[1].rect == Rect2i(46, 0, 8, 12));

	const String rotated = String(TEXTURE_PACKER_ARRAY).replace_first("\"rotated\": false", "\"rotated\": true");
	CHECK(SpriteSheet::parse_texture_packer_json(rotated, "res://ui", path, entries, error) == ERR_INVALID_DATA);
	CHECK(error.contains("rotated"));
	CHECK(error.contains("coin.png"));

	CHECK(SpriteSheet::texture_packer_name_to_region_name("a/b:c.PNG") == "a_b_c");
	CHECK(SpriteSheet::texture_packer_name_to_region_name("frame.001") == "frame.001");
}

TEST_CASE("[SceneTree][SpriteSheet] TexturePacker import applies trimmed margins") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas;
	atlas.instantiate();
	atlas->set_texture(sheet);
	atlas->set_texture_path("res://ui/sheet.png");
	CHECK(atlas->import_texture_packer_json(TEXTURE_PACKER_HASH, "res://ui") == OK);
	CHECK(atlas->get_region_names() == PackedStringArray({ "hero_walk_01", "coin" }));
	Ref<AtlasTexture> hero = atlas->get_region_texture("hero_walk_01");
	CHECK(hero->get_size() == Size2(32, 48));
	CHECK(hero->get_margin() == Rect2(1, 2, 2, 8));
}

TEST_CASE("[SceneTree][SpriteSheet] create_sprite_frames builds one animation per prefix in numeric order") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas;
	atlas.instantiate();
	atlas->set_texture(sheet);
	atlas->set_region("walk_10", Rect2i(30, 0, 10, 10));
	atlas->set_region("walk_2", Rect2i(20, 0, 10, 10));
	atlas->set_region("walk_1", Rect2i(10, 0, 10, 10));
	atlas->set_region("idle", Rect2i(0, 0, 10, 10));
	atlas->set_region("jump-0", Rect2i(0, 10, 10, 20));

	Ref<SpriteFrames> frames = atlas->create_sprite_frames("", 12.0);
	REQUIRE(frames.is_valid());
	CHECK_FALSE(frames->has_animation("default"));
	CHECK(frames->has_animation("idle"));
	CHECK(frames->has_animation("jump"));
	REQUIRE(frames->get_frame_count("walk") == 3);
	CHECK(frames->get_frame_texture("walk", 0) == atlas->get_region_texture("walk_1"));
	CHECK(frames->get_frame_texture("walk", 1) == atlas->get_region_texture("walk_2"));
	CHECK(frames->get_frame_texture("walk", 2) == atlas->get_region_texture("walk_10"));
	CHECK(frames->get_animation_speed("walk") == doctest::Approx(12.0));

	Ref<SpriteFrames> walk = atlas->create_sprite_frames("walk_");
	CHECK(walk->get_animation_names().size() == 1);
	CHECK(walk->get_frame_count("walk") == 3);
	CHECK(walk->get_animation_speed("walk") == doctest::Approx(5.0));

	// Frames stay linked: a region change updates the frame.
	atlas->set_region("walk_1", Rect2i(10, 10, 12, 12));
	CHECK(frames->get_frame_texture("walk", 0)->get_size() == Size2(12, 12));
}

TEST_CASE("[SceneTree][SpriteSheet] Properties round-trip for .tres") {
	Ref<ImageTexture> sheet = make_sheet();
	Ref<SpriteSheet> atlas = make_atlas(sheet);
	atlas->set_region_margin("Coin", Rect2i(1, 1, 0, 0));

	Ref<SpriteSheet> copy;
	copy.instantiate();
	copy->set("texture", atlas->get("texture"));
	copy->set("texture_path", atlas->get("texture_path"));
	copy->set("regions", atlas->get("regions"));
	copy->set("margins", atlas->get("margins"));
	check_same_regions(atlas, copy);
	CHECK(copy->get_texture_path() == "res://ui/icons.png");
	CHECK(Dictionary(copy->get("margins")).size() == 1);
}

// 50 sprites of mixed sizes in 5 rows of 10, one transparent pixel or more between them.
static Ref<Image> make_mixed_sheet(LocalVector<Rect2i> &r_expected) {
	Ref<Image> image = Image::create_empty(240, 120, false, Image::FORMAT_RGBA8);
	for (int i = 0; i < 50; i++) {
		const Rect2i rect((i % 10) * 24 + 1, (i / 10) * 24 + 1, 4 + (i % 5) * 4, 6 + (i % 3) * 7);
		image->fill_rect(rect, Color(1, 0, 0, 1));
		r_expected.push_back(rect);
	}
	return image;
}

TEST_CASE("[SceneTree][SpriteSheet] Auto slice finds sprites of mixed sizes") {
	LocalVector<Rect2i> expected;
	Ref<Image> image = make_mixed_sheet(expected);
	const TypedArray<Rect2i> found = SpriteSheet::detect_sprite_rects(image);
	REQUIRE(found.size() == 50);
	for (int i = 0; i < 50; i++) {
		CHECK(Rect2i(found[i]) == expected[i]);
	}

	Ref<SpriteSheet> atlas;
	atlas.instantiate();
	atlas->set_texture(ImageTexture::create_from_image(image));
	const PackedStringArray names = atlas->auto_slice("enemy_");
	REQUIRE(names.size() == 50);
	CHECK(names[0] == "enemy_0");
	CHECK(names[49] == "enemy_49");
	CHECK(atlas->get_region("enemy_12") == expected[12]);

	// Running it again finds only sprites that are not regions yet.
	CHECK(atlas->auto_slice("enemy_").is_empty());
	atlas->remove_region("enemy_3");
	const PackedStringArray again = atlas->auto_slice("enemy_");
	REQUIRE(again.size() == 1);
	CHECK(again[0] == "enemy_3");
	CHECK(atlas->get_region("enemy_3") == expected[3]);
	CHECK(atlas->get_region_count() == 50);
}

TEST_CASE("[SceneTree][SpriteSheet] Auto slice joins touching pixels and keeps separate sprites apart") {
	Ref<Image> image = Image::create_empty(32, 16, false, Image::FORMAT_RGBA8);
	// Diagonal neighbors belong to one sprite.
	image->set_pixel(1, 1, Color(1, 1, 1, 1));
	image->set_pixel(2, 2, Color(1, 1, 1, 1));
	// Two boxes that touch after flood fill are merged (an L shape with a separate dot inside its box).
	image->fill_rect(Rect2i(6, 1, 6, 2), Color(1, 1, 1, 1));
	image->fill_rect(Rect2i(6, 1, 2, 6), Color(1, 1, 1, 1));
	image->set_pixel(10, 5, Color(1, 1, 1, 1));
	// One transparent column between two sprites keeps them apart.
	image->fill_rect(Rect2i(16, 1, 3, 3), Color(1, 1, 1, 1));
	image->fill_rect(Rect2i(20, 1, 3, 3), Color(1, 1, 1, 1));
	// Pixels at or below the alpha threshold are ignored.
	image->fill_rect(Rect2i(26, 10, 4, 4), Color(1, 1, 1, 0.05));

	const TypedArray<Rect2i> found = SpriteSheet::detect_sprite_rects(image);
	REQUIRE(found.size() == 4);
	CHECK(Rect2i(found[0]) == Rect2i(1, 1, 2, 2));
	CHECK(Rect2i(found[1]) == Rect2i(6, 1, 6, 6));
	CHECK(Rect2i(found[2]) == Rect2i(16, 1, 3, 3));
	CHECK(Rect2i(found[3]) == Rect2i(20, 1, 3, 3));

	// A lower threshold also finds the faint sprite.
	CHECK(SpriteSheet::detect_sprite_rects(image, 0.01).size() == 5);
	// An empty sheet has no sprites.
	CHECK(SpriteSheet::detect_sprite_rects(Image::create_empty(8, 8, false, Image::FORMAT_RGBA8)).is_empty());
}

TEST_CASE("[SceneTree][SpriteSheet] Grid rectangles, unique names and add_regions") {
	const TypedArray<Rect2i> cells = SpriteSheet::make_grid_rects(Vector2i(100, 50), Vector2i(16, 16), Vector2i(2, 4), Vector2i(1, 1));
	// Columns at x = 1, 19, 37, 55, 73 (91 + 16 > 100); rows at y = 1, 21 (41 + 16 > 50).
	REQUIRE(cells.size() == 10);
	CHECK(Rect2i(cells[0]) == Rect2i(1, 1, 16, 16));
	CHECK(Rect2i(cells[4]) == Rect2i(73, 1, 16, 16));
	CHECK(Rect2i(cells[5]) == Rect2i(1, 21, 16, 16));
	ERR_PRINT_OFF;
	CHECK(SpriteSheet::make_grid_rects(Vector2i(100, 50), Vector2i(0, 16)).is_empty());
	ERR_PRINT_ON;

	Ref<SpriteSheet> atlas = make_atlas(make_sheet());
	CHECK(atlas->make_unique_region_name("tile_") == "tile_0");
	atlas->set_region("tile_0", Rect2i(0, 0, 8, 8));
	atlas->set_region("tile_2", Rect2i(0, 0, 8, 8));
	CHECK(atlas->make_unique_region_name("tile_") == "tile_1");

	const PackedStringArray names = atlas->add_regions(TypedArray<Rect2i>({ Rect2i(0, 8, 8, 8), Rect2i(8, 8, 8, 8), Rect2i(16, 8, 8, 8) }), "tile_");
	CHECK(names == PackedStringArray({ "tile_1", "tile_3", "tile_4" }));
	CHECK(atlas->get_region("tile_3") == Rect2i(8, 8, 8, 8));
	CHECK(atlas->get_region_names()[atlas->get_region_count() - 1] == "tile_4");

	// All or nothing: one rectangle outside the texture adds nothing.
	const int count = atlas->get_region_count();
	ERR_PRINT_OFF;
	CHECK(atlas->add_regions(TypedArray<Rect2i>({ Rect2i(0, 0, 8, 8), Rect2i(120, 0, 16, 16) }), "tile_").is_empty());
	CHECK(atlas->make_unique_region_name("bad/") == String());
	ERR_PRINT_ON;
	CHECK(atlas->get_region_count() == count);
}

} // namespace TestSpriteSheet
