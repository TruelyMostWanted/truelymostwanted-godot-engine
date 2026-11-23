/**************************************************************************/
/*  texture_atlas.cpp                                                     */
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

#include "texture_atlas.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_text_serializer.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/templates/hash_set.h"
#include "scene/resources/sprite_frames.h"

static const char *KEY_TEXTURE_PATH = "TexturePath";
static const char *KEY_ATLAS_IMAGES = "AtlasImages";
static const char *KEY_NAME = "Name";
static const char *KEY_X = "X";
static const char *KEY_Y = "Y";
static const char *KEY_WIDTH = "Width";
static const char *KEY_HEIGHT = "Height";
static const char *KEY_MARGIN_X = "MarginX";
static const char *KEY_MARGIN_Y = "MarginY";
static const char *KEY_SOURCE_WIDTH = "SourceWidth";
static const char *KEY_SOURCE_HEIGHT = "SourceHeight";
static const char *INI_SECTION_ATLAS = "Atlas";

/////////////////////////////////////////////////////////////////////////////
// Helpers.

// Reads an integer from a JSON number, an INI integer or a numeric string.
static bool _variant_to_int(const Variant &p_value, int &r_value) {
	switch (p_value.get_type()) {
		case Variant::INT: {
			r_value = int(int64_t(p_value));
			return true;
		}
		case Variant::FLOAT: {
			const double value = p_value;
			if (Math::floor(value) != value) {
				return false;
			}
			r_value = int(value);
			return true;
		}
		case Variant::STRING:
		case Variant::STRING_NAME: {
			const String text = String(p_value).strip_edges();
			if (!text.is_valid_int()) {
				return false;
			}
			r_value = int(text.to_int());
			return true;
		}
		default:
			return false;
	}
}

// Reads the keys X, Y, Width, Height and the optional MarginX, MarginY, SourceWidth, SourceHeight of one sprite.
static Error _dictionary_to_entry(const String &p_name, const Dictionary &p_values, TextureAtlas::Entry &r_entry, String &r_error) {
	const char *required[4] = { KEY_X, KEY_Y, KEY_WIDTH, KEY_HEIGHT };
	int values[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < 4; i++) {
		const Variant *value = p_values.getptr(String(required[i]));
		if (value == nullptr) {
			r_error = vformat("Sprite \"%s\": the key \"%s\" is missing.", p_name, required[i]);
			return ERR_PARSE_ERROR;
		}
		if (!_variant_to_int(*value, values[i])) {
			r_error = vformat("Sprite \"%s\": \"%s\" must be an integer.", p_name, required[i]);
			return ERR_PARSE_ERROR;
		}
	}
	r_entry.name = p_name;
	r_entry.rect = Rect2i(values[0], values[1], values[2], values[3]);

	const char *optional[4] = { KEY_MARGIN_X, KEY_MARGIN_Y, KEY_SOURCE_WIDTH, KEY_SOURCE_HEIGHT };
	int margin[4] = { 0, 0, values[2], values[3] };
	for (int i = 0; i < 4; i++) {
		const Variant *value = p_values.getptr(String(optional[i]));
		if (value == nullptr) {
			continue;
		}
		if (!_variant_to_int(*value, margin[i])) {
			r_error = vformat("Sprite \"%s\": \"%s\" must be an integer.", p_name, optional[i]);
			return ERR_PARSE_ERROR;
		}
	}
	r_entry.margin = Rect2i(margin[0], margin[1], margin[2] - values[2], margin[3] - values[3]);
	return OK;
}

// The keys of one sprite, in the order of the file formats.
static Dictionary _entry_to_dictionary(const Rect2i &p_rect, const Rect2i &p_margin) {
	Dictionary values;
	values[KEY_X] = p_rect.position.x;
	values[KEY_Y] = p_rect.position.y;
	values[KEY_WIDTH] = p_rect.size.x;
	values[KEY_HEIGHT] = p_rect.size.y;
	if (p_margin != Rect2i()) {
		values[KEY_MARGIN_X] = p_margin.position.x;
		values[KEY_MARGIN_Y] = p_margin.position.y;
		values[KEY_SOURCE_WIDTH] = p_rect.size.x + p_margin.size.x;
		values[KEY_SOURCE_HEIGHT] = p_rect.size.y + p_margin.size.y;
	}
	return values;
}

static Error _parse_json_root(const String &p_text, Dictionary &r_root, String &r_error) {
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_text) != OK) {
		r_error = vformat("JSON parse error in line %d: %s", json->get_error_line(), json->get_error_message());
		return ERR_PARSE_ERROR;
	}
	const Variant data = json->get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		r_error = "The JSON root must be an object.";
		return ERR_PARSE_ERROR;
	}
	r_root = data;
	return OK;
}

bool TextureAtlas::is_valid_region_name(const String &p_name) {
	if (p_name.is_empty() || p_name != p_name.strip_edges()) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if (c == '/' || c == ':' || c == '"' || c < 32 || c == 127) {
			return false;
		}
	}
	return true;
}

String TextureAtlas::texture_packer_name_to_region_name(const String &p_file_name) {
	String name = p_file_name.strip_edges();
	const String extension = name.get_extension().to_lower();
	if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "webp" || extension == "bmp" || extension == "tga" || extension == "gif" || extension == "svg" || extension == "ktx" || extension == "dds") {
		name = name.get_basename();
	}
	name = name.replace_char('/', '_').replace_char('\\', '_').replace_char(':', '_').replace_char('"', '_');
	return name.strip_edges();
}

int TextureAtlas::split_trailing_number(const String &p_name, String &r_base) {
	int start = p_name.length();
	while (start > 0 && is_digit(p_name[start - 1])) {
		start--;
	}
	if (start == p_name.length()) {
		r_base = p_name;
		return -1;
	}
	const String digits = p_name.substr(start);
	r_base = p_name.substr(0, start);
	// Up to 9 digits fit into an int; longer numbers are clamped (only the order matters).
	if (digits.length() > 9) {
		return 999999999;
	}
	return int(digits.to_int());
}

/////////////////////////////////////////////////////////////////////////////
// Regions and cached textures.

void TextureAtlas::_update_cached_texture(Region &r_region) const {
	if (r_region.texture.is_null()) {
		return;
	}
	r_region.texture->set_atlas(texture);
	r_region.texture->set_region(Rect2(r_region.rect));
	r_region.texture->set_margin(Rect2(r_region.margin));
	r_region.texture->set_filter_clip(filter_clip);
}

void TextureAtlas::_update_all_cached_textures() {
	for (KeyValue<StringName, Region> &E : regions) {
		_update_cached_texture(E.value);
	}
}

Error TextureAtlas::_check_rect(const String &p_name, const Rect2i &p_rect, const Ref<Texture2D> &p_texture, String &r_error) {
	if (p_rect.size.x <= 0 || p_rect.size.y <= 0) {
		r_error = vformat("Sprite \"%s\": width and height must be greater than 0 (%s).", p_name, p_rect);
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture.is_valid()) {
		const Size2 size = p_texture->get_size();
		if (!Rect2i(0, 0, int(size.x), int(size.y)).encloses(p_rect)) {
			r_error = vformat("Sprite \"%s\": the region %s is outside the texture (%dx%d).", p_name, p_rect, int(size.x), int(size.y));
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}

Error TextureAtlas::_check_margin(const String &p_name, const Rect2i &p_margin, String &r_error) {
	if (p_margin.position.x < 0 || p_margin.position.y < 0 || p_margin.size.x < 0 || p_margin.size.y < 0) {
		r_error = vformat("Sprite \"%s\": the margin %s must not be negative.", p_name, p_margin);
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}

void TextureAtlas::set_texture(const Ref<Texture2D> &p_texture) {
	if (texture == p_texture) {
		return;
	}
	texture = p_texture;
	texture_path = String();

	PackedStringArray outside;
	if (texture.is_valid()) {
		for (const KeyValue<StringName, Region> &E : regions) {
			String error;
			if (_check_rect(E.key, E.value.rect, texture, error) != OK) {
				outside.push_back(E.key);
			}
		}
	}
	_update_all_cached_textures();
	emit_changed();
	if (!outside.is_empty()) {
		ERR_PRINT(vformat("TextureAtlas: %d region(s) lie outside the new texture: %s.", outside.size(), String(", ").join(outside)));
	}
}

Ref<Texture2D> TextureAtlas::get_sheet_texture() const {
	return texture;
}

void TextureAtlas::set_texture_path(const String &p_path) {
	if (texture_path == p_path) {
		return;
	}
	texture_path = p_path;
	emit_changed();
}

String TextureAtlas::get_texture_path() const {
	return texture_path;
}

void TextureAtlas::set_filter_clip(bool p_enable) {
	if (filter_clip == p_enable) {
		return;
	}
	filter_clip = p_enable;
	_update_all_cached_textures();
	emit_changed();
}

bool TextureAtlas::is_filter_clip() const {
	return filter_clip;
}

void TextureAtlas::set_regions(const TypedDictionary<StringName, Rect2i> &p_regions) {
	HashMap<StringName, Region> new_regions;
	new_regions.reserve(p_regions.size());
	PackedStringArray errors;
	for (const KeyValue<Variant, Variant> &E : p_regions) {
		const StringName region_name = E.key;
		const Rect2i rect = E.value;
		String error;
		if (!is_valid_region_name(region_name)) {
			errors.push_back(vformat("\"%s\" is not a valid region region_name.", region_name));
			continue;
		}
		if (_check_rect(region_name, rect, texture, error) != OK) {
			errors.push_back(error);
			continue;
		}
		Region region;
		region.rect = rect;
		const Region *old = regions.getptr(region_name);
		if (old != nullptr) {
			region.margin = old->margin;
			region.texture = old->texture;
		}
		new_regions.insert(region_name, region);
	}
	regions = new_regions;
	_update_all_cached_textures();
	emit_changed();
	for (const String &error : errors) {
		ERR_PRINT("TextureAtlas: " + error);
	}
}

TypedDictionary<StringName, Rect2i> TextureAtlas::get_regions() const {
	TypedDictionary<StringName, Rect2i> ret;
	for (const KeyValue<StringName, Region> &E : regions) {
		ret[E.key] = E.value.rect;
	}
	return ret;
}

void TextureAtlas::set_margins(const TypedDictionary<StringName, Rect2i> &p_margins) {
	for (KeyValue<StringName, Region> &E : regions) {
		E.value.margin = Rect2i();
	}
	PackedStringArray errors;
	for (const KeyValue<Variant, Variant> &E : p_margins) {
		const StringName region_name = E.key;
		const Rect2i margin = E.value;
		Region *region = regions.getptr(region_name);
		if (region == nullptr) {
			errors.push_back(vformat("The margin of the unknown region \"%s\" is ignored.", region_name));
			continue;
		}
		String error;
		if (_check_margin(region_name, margin, error) != OK) {
			errors.push_back(error);
			continue;
		}
		region->margin = margin;
	}
	_update_all_cached_textures();
	emit_changed();
	for (const String &error : errors) {
		ERR_PRINT("TextureAtlas: " + error);
	}
}

TypedDictionary<StringName, Rect2i> TextureAtlas::get_margins() const {
	TypedDictionary<StringName, Rect2i> ret;
	for (const KeyValue<StringName, Region> &E : regions) {
		if (E.value.margin != Rect2i()) {
			ret[E.key] = E.value.margin;
		}
	}
	return ret;
}

Error TextureAtlas::set_region(const StringName &p_name, const Rect2i &p_rect) {
	ERR_FAIL_COND_V_MSG(!is_valid_region_name(p_name), ERR_INVALID_PARAMETER, vformat("TextureAtlas: \"%s\" is not a valid region name (not empty, no '/', ':', '\"', control characters or surrounding spaces).", p_name));
	String error;
	if (_check_rect(p_name, p_rect, texture, error) != OK) {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "TextureAtlas: " + error);
	}
	Region *region = regions.getptr(p_name);
	if (region == nullptr) {
		Region new_region;
		new_region.rect = p_rect;
		regions.insert(p_name, new_region);
	} else {
		if (region->rect == p_rect) {
			return OK;
		}
		region->rect = p_rect;
		_update_cached_texture(*region);
	}
	emit_changed();
	return OK;
}

void TextureAtlas::remove_region(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!regions.has(p_name), vformat("TextureAtlas: the region \"%s\" does not exist.", p_name));
	regions.erase(p_name);
	emit_changed();
}

Error TextureAtlas::rename_region(const StringName &p_from, const StringName &p_to) {
	ERR_FAIL_COND_V_MSG(!regions.has(p_from), ERR_DOES_NOT_EXIST, vformat("TextureAtlas: the region \"%s\" does not exist.", p_from));
	if (p_from == p_to) {
		return OK;
	}
	ERR_FAIL_COND_V_MSG(!is_valid_region_name(p_to), ERR_INVALID_PARAMETER, vformat("TextureAtlas: \"%s\" is not a valid region name.", p_to));
	ERR_FAIL_COND_V_MSG(regions.has(p_to), ERR_ALREADY_EXISTS, vformat("TextureAtlas: the region \"%s\" exists already.", p_to));
	regions.replace_key(p_from, p_to);
	emit_changed();
	return OK;
}

bool TextureAtlas::has_region(const StringName &p_name) const {
	return regions.has(p_name);
}

Rect2i TextureAtlas::get_region(const StringName &p_name) const {
	const Region *region = regions.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(region, Rect2i(), vformat("TextureAtlas: the region \"%s\" does not exist.", p_name));
	return region->rect;
}

void TextureAtlas::_set_region_order(const PackedStringArray &p_order) {
	// Regions named in the order first, in that order; the rest keep their current order.
	HashMap<StringName, Region> ordered;
	ordered.reserve(regions.size());
	for (const String &region_name : p_order) {
		const StringName key = region_name;
		const Region *region = regions.getptr(key);
		if (region != nullptr && !ordered.has(key)) {
			ordered.insert(key, *region);
		}
	}
	for (const KeyValue<StringName, Region> &E : regions) {
		if (!ordered.has(E.key)) {
			ordered.insert(E.key, E.value);
		}
	}
	regions = ordered;
}

PackedStringArray TextureAtlas::_get_region_order() const {
	return get_region_names();
}

PackedStringArray TextureAtlas::get_region_names() const {
	PackedStringArray names;
	names.resize(regions.size());
	String *ptr = names.ptrw();
	int i = 0;
	for (const KeyValue<StringName, Region> &E : regions) {
		ptr[i++] = E.key;
	}
	return names;
}

int TextureAtlas::get_region_count() const {
	return regions.size();
}

void TextureAtlas::clear_regions() {
	if (regions.is_empty()) {
		return;
	}
	regions.clear();
	emit_changed();
}

Error TextureAtlas::set_region_margin(const StringName &p_name, const Rect2i &p_margin) {
	Region *region = regions.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(region, ERR_DOES_NOT_EXIST, vformat("TextureAtlas: the region \"%s\" does not exist.", p_name));
	String error;
	if (_check_margin(p_name, p_margin, error) != OK) {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "TextureAtlas: " + error);
	}
	if (region->margin == p_margin) {
		return OK;
	}
	region->margin = p_margin;
	_update_cached_texture(*region);
	emit_changed();
	return OK;
}

Rect2i TextureAtlas::get_region_margin(const StringName &p_name) const {
	const Region *region = regions.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(region, Rect2i(), vformat("TextureAtlas: the region \"%s\" does not exist.", p_name));
	return region->margin;
}

Ref<AtlasTexture> TextureAtlas::get_texture(const StringName &p_name) {
	Region *region = regions.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(region, Ref<AtlasTexture>(), vformat("TextureAtlas: the region \"%s\" does not exist.", p_name));
	if (region->texture.is_null()) {
		region->texture.instantiate();
		_update_cached_texture(*region);
	}
	return region->texture;
}

/////////////////////////////////////////////////////////////////////////////
// SpriteFrames.

namespace {

struct AtlasFrame {
	int number = -1;
	int order = 0;
	StringName name;
};

struct AtlasFrameComparator {
	_FORCE_INLINE_ bool operator()(const AtlasFrame &p_a, const AtlasFrame &p_b) const {
		if (p_a.number != p_b.number) {
			return p_a.number < p_b.number;
		}
		return p_a.order < p_b.order;
	}
};

String strip_trailing_separators(const String &p_text) {
	int end = p_text.length();
	while (end > 0) {
		const char32_t c = p_text[end - 1];
		if (c != '_' && c != '-' && c != '.' && c != ' ') {
			break;
		}
		end--;
	}
	return p_text.substr(0, end);
}

} // namespace

Ref<SpriteFrames> TextureAtlas::create_sprite_frames(const String &p_prefix, double p_fps) {
	ERR_FAIL_COND_V_MSG(p_fps < 0.0, Ref<SpriteFrames>(), "TextureAtlas: fps must not be negative.");
	HashMap<String, LocalVector<AtlasFrame>> animations;
	int order = 0;
	for (const KeyValue<StringName, Region> &E : regions) {
		const String region_name = E.key;
		if (!region_name.begins_with(p_prefix)) {
			continue;
		}
		String base;
		AtlasFrame frame;
		frame.number = split_trailing_number(region_name, base);
		frame.order = order++;
		frame.name = E.key;
		String animation = strip_trailing_separators(base);
		if (animation.is_empty()) {
			animation = strip_trailing_separators(p_prefix);
		}
		if (animation.is_empty()) {
			animation = region_name;
		}
		animations[animation].push_back(frame); // Inserts the animation on first use, in region order.
	}

	Ref<SpriteFrames> frames;
	frames.instantiate();
	const String default_animation = "default";
	for (KeyValue<String, LocalVector<AtlasFrame>> &E : animations) {
		E.value.sort_custom<AtlasFrameComparator>();
		const StringName animation = E.key;
		if (!frames->has_animation(animation)) {
			frames->add_animation(animation);
		}
		frames->set_animation_speed(animation, p_fps);
		for (const AtlasFrame &frame : E.value) {
			frames->add_frame(animation, get_texture(frame.name));
		}
	}
	if (!animations.is_empty() && !animations.has(default_animation) && frames->has_animation(default_animation)) {
		frames->remove_animation(default_animation);
	}
	return frames;
}

/////////////////////////////////////////////////////////////////////////////
// Applying parsed entries.

Error TextureAtlas::apply_entries(const String &p_texture_path, const LocalVector<Entry> &p_entries, String &r_error) {
	HashSet<StringName> names;
	for (const Entry &entry : p_entries) {
		if (!is_valid_region_name(entry.name)) {
			r_error = vformat("\"%s\" is not a valid sprite name (not empty, no '/', ':', '\"', control characters or surrounding spaces).", entry.name);
			return ERR_INVALID_DATA;
		}
		if (names.has(entry.name)) {
			r_error = vformat("The sprite \"%s\" is defined twice.", entry.name);
			return ERR_INVALID_DATA;
		}
		names.insert(entry.name);
	}

	if (p_texture_path.is_empty()) {
		r_error = vformat("\"%s\" is missing or empty.", KEY_TEXTURE_PATH);
		return ERR_INVALID_DATA;
	}
	Ref<Texture2D> new_texture = texture;
	if (texture.is_null() || (p_texture_path != texture_path && p_texture_path != texture->get_path())) {
		Error err = OK;
		const Ref<Resource> resource = ResourceLoader::load(p_texture_path, "Texture2D", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
		new_texture = resource;
		if (new_texture.is_null()) {
			r_error = vformat("The texture \"%s\" cannot be loaded as Texture2D.", p_texture_path);
			return err != OK ? err : ERR_FILE_UNRECOGNIZED;
		}
	}

	for (const Entry &entry : p_entries) {
		if (_check_rect(entry.name, entry.rect, new_texture, r_error) != OK) {
			return ERR_INVALID_DATA;
		}
		if (_check_margin(entry.name, entry.margin, r_error) != OK) {
			return ERR_INVALID_DATA;
		}
	}

	HashMap<StringName, Region> new_regions;
	new_regions.reserve(p_entries.size());
	for (const Entry &entry : p_entries) {
		Region region;
		region.rect = entry.rect;
		region.margin = entry.margin;
		const Region *old = regions.getptr(entry.name);
		if (old != nullptr) {
			region.texture = old->texture; // Handed-out textures stay linked.
		}
		new_regions.insert(entry.name, region);
	}
	texture = new_texture;
	texture_path = p_texture_path;
	regions = new_regions;
	_update_all_cached_textures();
	emit_changed();
	return OK;
}

Error TextureAtlas::_load_entries(Error p_parse_error, const String &p_texture_path, const LocalVector<Entry> &p_entries, const String &p_error, const String &p_format) {
	if (p_parse_error != OK) {
		ERR_FAIL_V_MSG(p_parse_error, vformat("TextureAtlas: %s: %s", p_format, p_error));
	}
	String error;
	const Error err = apply_entries(p_texture_path, p_entries, error);
	if (err != OK) {
		ERR_FAIL_V_MSG(err, vformat("TextureAtlas: %s: %s", p_format, error));
	}
	return OK;
}

String TextureAtlas::_get_written_texture_path() const {
	if (!texture_path.is_empty()) {
		return texture_path;
	}
	if (texture.is_null()) {
		return String();
	}
	const String path = texture->get_path();
	if (path.is_empty() || path.contains("::")) {
		return path;
	}
	const ResourceUID::ID uid = ResourceLoader::get_resource_uid(path);
	if (uid != ResourceUID::INVALID_ID) {
		return ResourceUID::get_singleton()->id_to_text(uid);
	}
	return path;
}

/////////////////////////////////////////////////////////////////////////////
// JSON.

Error TextureAtlas::parse_json(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error) {
	r_entries.clear();
	Dictionary root;
	if (_parse_json_root(p_text, root, r_error) != OK) {
		return ERR_PARSE_ERROR;
	}
	const Variant path = root.get(KEY_TEXTURE_PATH, Variant());
	if (path.get_type() != Variant::STRING) {
		r_error = vformat("\"%s\" is missing or not a string.", KEY_TEXTURE_PATH);
		return ERR_PARSE_ERROR;
	}
	r_texture_path = path;
	const Variant images = root.get(KEY_ATLAS_IMAGES, Dictionary());
	if (images.get_type() != Variant::DICTIONARY) {
		r_error = vformat("\"%s\" must be an object.", KEY_ATLAS_IMAGES);
		return ERR_PARSE_ERROR;
	}
	const Dictionary sprites = images;
	for (const KeyValue<Variant, Variant> &E : sprites) {
		const String name = E.key;
		if (E.value.get_type() != Variant::DICTIONARY) {
			r_error = vformat("Sprite \"%s\" must be an object.", name);
			return ERR_PARSE_ERROR;
		}
		Entry entry;
		if (_dictionary_to_entry(name, E.value, entry, r_error) != OK) {
			return ERR_PARSE_ERROR;
		}
		r_entries.push_back(entry);
	}
	return OK;
}

String TextureAtlas::save_as_json(const String &p_indent) const {
	Dictionary root;
	root[KEY_TEXTURE_PATH] = _get_written_texture_path();
	Dictionary sprites;
	for (const KeyValue<StringName, Region> &E : regions) {
		sprites[String(E.key)] = _entry_to_dictionary(E.value.rect, E.value.margin);
	}
	root[KEY_ATLAS_IMAGES] = sprites;
	return JSON::stringify(root, p_indent, false);
}

Error TextureAtlas::load_from_json(const String &p_text) {
	String path;
	LocalVector<Entry> entries;
	String error;
	const Error err = parse_json(p_text, path, entries, error);
	return _load_entries(err, path, entries, error, "JSON");
}

/////////////////////////////////////////////////////////////////////////////
// CSV.

Error TextureAtlas::parse_csv(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error) {
	r_entries.clear();
	const Vector<Vector<String>> rows = ResourceTextSerializer::parse_csv_rows(p_text, ',');
	if (rows.size() < 2 || rows[0].size() < 2 || rows[0][0].strip_edges() != KEY_TEXTURE_PATH) {
		r_error = vformat("The first row must be \"%s,<path>\" and the second row the column names.", KEY_TEXTURE_PATH);
		return ERR_PARSE_ERROR;
	}
	r_texture_path = rows[0][1].strip_edges();

	// Column index per key; -1 if the column is missing.
	const char *keys[9] = { KEY_NAME, KEY_X, KEY_Y, KEY_WIDTH, KEY_HEIGHT, KEY_MARGIN_X, KEY_MARGIN_Y, KEY_SOURCE_WIDTH, KEY_SOURCE_HEIGHT };
	int columns[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	const Vector<String> &header = rows[1];
	for (int c = 0; c < header.size(); c++) {
		const String column = header[c].strip_edges().to_lower();
		for (int k = 0; k < 9; k++) {
			if (column == String(keys[k]).to_lower()) {
				columns[k] = c;
			}
		}
	}
	for (int k = 0; k < 5; k++) {
		if (columns[k] < 0) {
			r_error = vformat("The column \"%s\" is missing.", keys[k]);
			return ERR_PARSE_ERROR;
		}
	}

	for (int r = 2; r < rows.size(); r++) {
		const Vector<String> &row = rows[r];
		if (columns[0] >= row.size()) {
			r_error = vformat("Row %d: the name is missing.", r + 1);
			return ERR_PARSE_ERROR;
		}
		const String name = row[columns[0]];
		Dictionary values;
		for (int k = 1; k < 9; k++) {
			if (columns[k] >= 0 && columns[k] < row.size() && !row[columns[k]].strip_edges().is_empty()) {
				values[keys[k]] = row[columns[k]];
			}
		}
		Entry entry;
		if (_dictionary_to_entry(name, values, entry, r_error) != OK) {
			return ERR_PARSE_ERROR;
		}
		r_entries.push_back(entry);
	}
	return OK;
}

String TextureAtlas::save_as_csv() const {
	bool has_margins = false;
	for (const KeyValue<StringName, Region> &E : regions) {
		if (E.value.margin != Rect2i()) {
			has_margins = true;
			break;
		}
	}
	Vector<String> first_row;
	first_row.push_back(KEY_TEXTURE_PATH);
	first_row.push_back(_get_written_texture_path());
	String text = ResourceTextSerializer::write_csv_row(first_row, ',');

	Vector<String> header;
	header.push_back(KEY_NAME);
	header.push_back(KEY_X);
	header.push_back(KEY_Y);
	header.push_back(KEY_WIDTH);
	header.push_back(KEY_HEIGHT);
	if (has_margins) {
		header.push_back(KEY_MARGIN_X);
		header.push_back(KEY_MARGIN_Y);
		header.push_back(KEY_SOURCE_WIDTH);
		header.push_back(KEY_SOURCE_HEIGHT);
	}
	text += ResourceTextSerializer::write_csv_row(header, ',');

	for (const KeyValue<StringName, Region> &E : regions) {
		const Rect2i &rect = E.value.rect;
		const Rect2i &margin = E.value.margin;
		Vector<String> row;
		row.push_back(String(E.key));
		row.push_back(itos(rect.position.x));
		row.push_back(itos(rect.position.y));
		row.push_back(itos(rect.size.x));
		row.push_back(itos(rect.size.y));
		if (has_margins) {
			row.push_back(itos(margin.position.x));
			row.push_back(itos(margin.position.y));
			row.push_back(itos(rect.size.x + margin.size.x));
			row.push_back(itos(rect.size.y + margin.size.y));
		}
		text += ResourceTextSerializer::write_csv_row(row, ',');
	}
	return text;
}

Error TextureAtlas::load_from_csv(const String &p_text) {
	String path;
	LocalVector<Entry> entries;
	String error;
	const Error err = parse_csv(p_text, path, entries, error);
	return _load_entries(err, path, entries, error, "CSV");
}

/////////////////////////////////////////////////////////////////////////////
// INI.

Error TextureAtlas::parse_ini(const String &p_text, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error) {
	r_entries.clear();
	LocalVector<ResourceTextSerializer::Section> sections;
	if (ResourceTextSerializer::parse_ini_sections(p_text, sections, r_error) != OK) {
		return ERR_PARSE_ERROR;
	}
	bool has_header = false;
	for (const ResourceTextSerializer::Section &section : sections) {
		if (section.first.is_empty()) {
			continue; // Keys before the first section.
		}
		if (!has_header && section.first == INI_SECTION_ATLAS) {
			has_header = true;
			const Variant path = section.second.get(KEY_TEXTURE_PATH, Variant());
			r_texture_path = path.get_type() == Variant::NIL ? String() : String(path);
			continue;
		}
		Entry entry;
		if (_dictionary_to_entry(section.first, section.second, entry, r_error) != OK) {
			return ERR_PARSE_ERROR;
		}
		r_entries.push_back(entry);
	}
	if (!has_header) {
		r_error = vformat("The section [%s] is missing.", INI_SECTION_ATLAS);
		return ERR_PARSE_ERROR;
	}
	return OK;
}

String TextureAtlas::save_as_ini() const {
	String text = vformat("[%s]\n%s=%s\n", INI_SECTION_ATLAS, KEY_TEXTURE_PATH, _get_written_texture_path());
	for (const KeyValue<StringName, Region> &E : regions) {
		text += vformat("\n[%s]\n", E.key);
		const Dictionary values = _entry_to_dictionary(E.value.rect, E.value.margin);
		for (const KeyValue<Variant, Variant> &V : values) {
			text += vformat("%s=%s\n", V.key, V.value);
		}
	}
	return text;
}

Error TextureAtlas::load_from_ini(const String &p_text) {
	String path;
	LocalVector<Entry> entries;
	String error;
	const Error err = parse_ini(p_text, path, entries, error);
	return _load_entries(err, path, entries, error, "INI");
}

/////////////////////////////////////////////////////////////////////////////
// TexturePacker JSON.

static Error _texture_packer_frame_to_entry(const String &p_file_name, const Dictionary &p_frame, TextureAtlas::Entry &r_entry, String &r_error) {
	if (bool(p_frame.get("rotated", false))) {
		r_error = vformat("Frame \"%s\" is rotated; rotated frames are not supported. Export the sheet without rotation.", p_file_name);
		return ERR_INVALID_DATA;
	}
	const Variant frame = p_frame.get("frame", Variant());
	if (frame.get_type() != Variant::DICTIONARY) {
		r_error = vformat("Frame \"%s\": \"frame\" is missing.", p_file_name);
		return ERR_PARSE_ERROR;
	}
	const Dictionary rect = frame;
	const char *keys[4] = { "x", "y", "w", "h" };
	int values[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < 4; i++) {
		if (!_variant_to_int(rect.get(keys[i], Variant()), values[i])) {
			r_error = vformat("Frame \"%s\": \"frame.%s\" must be an integer.", p_file_name, keys[i]);
			return ERR_PARSE_ERROR;
		}
	}
	r_entry.name = TextureAtlas::texture_packer_name_to_region_name(p_file_name);
	r_entry.rect = Rect2i(values[0], values[1], values[2], values[3]);
	r_entry.margin = Rect2i();

	if (bool(p_frame.get("trimmed", false))) {
		const Variant sprite_source = p_frame.get("spriteSourceSize", Variant());
		const Variant source = p_frame.get("sourceSize", Variant());
		if (sprite_source.get_type() != Variant::DICTIONARY || source.get_type() != Variant::DICTIONARY) {
			r_error = vformat("Frame \"%s\" is trimmed, but \"spriteSourceSize\" or \"sourceSize\" is missing.", p_file_name);
			return ERR_PARSE_ERROR;
		}
		const Dictionary offset = sprite_source;
		const Dictionary size = source;
		int offset_x = 0;
		int offset_y = 0;
		int source_w = 0;
		int source_h = 0;
		if (!_variant_to_int(offset.get("x", Variant()), offset_x) || !_variant_to_int(offset.get("y", Variant()), offset_y) || !_variant_to_int(size.get("w", Variant()), source_w) || !_variant_to_int(size.get("h", Variant()), source_h)) {
			r_error = vformat("Frame \"%s\": \"spriteSourceSize\" and \"sourceSize\" must contain integers.", p_file_name);
			return ERR_PARSE_ERROR;
		}
		r_entry.margin = Rect2i(offset_x, offset_y, source_w - values[2], source_h - values[3]);
	}
	return OK;
}

Error TextureAtlas::parse_texture_packer_json(const String &p_text, const String &p_base_dir, String &r_texture_path, LocalVector<Entry> &r_entries, String &r_error) {
	r_entries.clear();
	Dictionary root;
	if (_parse_json_root(p_text, root, r_error) != OK) {
		return ERR_PARSE_ERROR;
	}
	if (!root.has("frames") && root.has("textures")) {
		r_error = "Multi-texture TexturePacker files (\"textures\") are not supported; export one sheet per file.";
		return ERR_INVALID_DATA;
	}
	const Variant meta = root.get("meta", Variant());
	String image;
	if (meta.get_type() == Variant::DICTIONARY) {
		image = Dictionary(meta).get("image", String());
	}
	if (image.is_empty()) {
		r_error = "\"meta.image\" is missing.";
		return ERR_PARSE_ERROR;
	}
	if (p_base_dir.is_empty() || image.is_absolute_path() || image.contains("://")) {
		r_texture_path = image;
	} else {
		r_texture_path = p_base_dir.path_join(image).simplify_path();
	}

	const Variant frames = root.get("frames", Variant());
	if (frames.get_type() == Variant::DICTIONARY) {
		// Hash: file name -> frame.
		const Dictionary hash = frames;
		for (const KeyValue<Variant, Variant> &E : hash) {
			const String file_name = E.key;
			if (E.value.get_type() != Variant::DICTIONARY) {
				r_error = vformat("Frame \"%s\" must be an object.", file_name);
				return ERR_PARSE_ERROR;
			}
			Entry entry;
			const Error err = _texture_packer_frame_to_entry(file_name, E.value, entry, r_error);
			if (err != OK) {
				return err;
			}
			r_entries.push_back(entry);
		}
	} else if (frames.get_type() == Variant::ARRAY) {
		// Array: frames with a "filename" key.
		const Array array = frames;
		for (int i = 0; i < array.size(); i++) {
			if (array[i].get_type() != Variant::DICTIONARY) {
				r_error = vformat("Frame %d must be an object.", i);
				return ERR_PARSE_ERROR;
			}
			const Dictionary frame = array[i];
			const Variant file_name = frame.get("filename", Variant());
			if (file_name.get_type() != Variant::STRING) {
				r_error = vformat("Frame %d: \"filename\" is missing.", i);
				return ERR_PARSE_ERROR;
			}
			Entry entry;
			const Error err = _texture_packer_frame_to_entry(file_name, frame, entry, r_error);
			if (err != OK) {
				return err;
			}
			r_entries.push_back(entry);
		}
	} else {
		r_error = "\"frames\" must be an object (Hash) or an array (Array).";
		return ERR_PARSE_ERROR;
	}
	return OK;
}

Error TextureAtlas::import_texture_packer_json(const String &p_text, const String &p_base_dir) {
	String path;
	LocalVector<Entry> entries;
	String error;
	const Error err = parse_texture_packer_json(p_text, p_base_dir, path, entries, error);
	return _load_entries(err, path, entries, error, "TexturePacker JSON");
}

/////////////////////////////////////////////////////////////////////////////
// Files.

Error TextureAtlas::load_from_file(const String &p_path) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("TextureAtlas: cannot read \"%s\".", p_path));
	const String extension = p_path.get_extension().to_lower();
	if (extension == "json") {
		Dictionary root;
		String error;
		if (_parse_json_root(text, root, error) == OK && root.has("frames")) {
			return import_texture_packer_json(text, p_path.get_base_dir());
		}
		return load_from_json(text);
	}
	if (extension == "csv") {
		return load_from_csv(text);
	}
	if (extension == "ini") {
		return load_from_ini(text);
	}
	ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("TextureAtlas: unknown file extension of \"%s\" (json, csv or ini expected).", p_path));
}

Error TextureAtlas::save_to_file(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	String text;
	if (extension == "json") {
		text = save_as_json();
	} else if (extension == "csv") {
		text = save_as_csv();
	} else if (extension == "ini") {
		text = save_as_ini();
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("TextureAtlas: unknown file extension of \"%s\" (json, csv or ini expected).", p_path));
	}
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err != OK ? err : ERR_CANT_OPEN, vformat("TextureAtlas: cannot write \"%s\".", p_path));
	file->store_string(text);
	return OK;
}

/////////////////////////////////////////////////////////////////////////////
// Bindings.

void TextureAtlas::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &TextureAtlas::set_texture);
	ClassDB::bind_method(D_METHOD("get_sheet_texture"), &TextureAtlas::get_sheet_texture);
	ClassDB::bind_method(D_METHOD("set_texture_path", "path"), &TextureAtlas::set_texture_path);
	ClassDB::bind_method(D_METHOD("get_texture_path"), &TextureAtlas::get_texture_path);
	ClassDB::bind_method(D_METHOD("set_filter_clip", "enable"), &TextureAtlas::set_filter_clip);
	ClassDB::bind_method(D_METHOD("is_filter_clip"), &TextureAtlas::is_filter_clip);
	ClassDB::bind_method(D_METHOD("set_regions", "regions"), &TextureAtlas::set_regions);
	ClassDB::bind_method(D_METHOD("get_regions"), &TextureAtlas::get_regions);
	ClassDB::bind_method(D_METHOD("set_margins", "margins"), &TextureAtlas::set_margins);
	ClassDB::bind_method(D_METHOD("_set_region_order", "order"), &TextureAtlas::_set_region_order);
	ClassDB::bind_method(D_METHOD("_get_region_order"), &TextureAtlas::_get_region_order);
	ClassDB::bind_method(D_METHOD("get_margins"), &TextureAtlas::get_margins);

	ClassDB::bind_method(D_METHOD("set_region", "name", "rect"), &TextureAtlas::set_region);
	ClassDB::bind_method(D_METHOD("remove_region", "name"), &TextureAtlas::remove_region);
	ClassDB::bind_method(D_METHOD("rename_region", "from", "to"), &TextureAtlas::rename_region);
	ClassDB::bind_method(D_METHOD("has_region", "name"), &TextureAtlas::has_region);
	ClassDB::bind_method(D_METHOD("get_region", "name"), &TextureAtlas::get_region);
	ClassDB::bind_method(D_METHOD("get_region_names"), &TextureAtlas::get_region_names);
	ClassDB::bind_method(D_METHOD("get_region_count"), &TextureAtlas::get_region_count);
	ClassDB::bind_method(D_METHOD("clear_regions"), &TextureAtlas::clear_regions);
	ClassDB::bind_method(D_METHOD("set_region_margin", "name", "margin"), &TextureAtlas::set_region_margin);
	ClassDB::bind_method(D_METHOD("get_region_margin", "name"), &TextureAtlas::get_region_margin);

	ClassDB::bind_method(D_METHOD("get_texture", "name"), &TextureAtlas::get_texture);
	ClassDB::bind_method(D_METHOD("create_sprite_frames", "prefix", "fps"), &TextureAtlas::create_sprite_frames, DEFVAL(String()), DEFVAL(5.0));

	ClassDB::bind_method(D_METHOD("save_as_json", "indent"), &TextureAtlas::save_as_json, DEFVAL("\t"));
	ClassDB::bind_method(D_METHOD("load_from_json", "json"), &TextureAtlas::load_from_json);
	ClassDB::bind_method(D_METHOD("save_as_csv"), &TextureAtlas::save_as_csv);
	ClassDB::bind_method(D_METHOD("load_from_csv", "csv"), &TextureAtlas::load_from_csv);
	ClassDB::bind_method(D_METHOD("save_as_ini"), &TextureAtlas::save_as_ini);
	ClassDB::bind_method(D_METHOD("load_from_ini", "ini"), &TextureAtlas::load_from_ini);
	ClassDB::bind_method(D_METHOD("import_texture_packer_json", "json", "base_dir"), &TextureAtlas::import_texture_packer_json, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &TextureAtlas::load_from_file);
	ClassDB::bind_method(D_METHOD("save_to_file", "path"), &TextureAtlas::save_to_file);

	ClassDB::bind_static_method("TextureAtlas", D_METHOD("is_valid_region_name", "name"), &TextureAtlas::is_valid_region_name);
	ClassDB::bind_static_method("TextureAtlas", D_METHOD("texture_packer_name_to_region_name", "file_name"), &TextureAtlas::texture_packer_name_to_region_name);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_sheet_texture");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path"), "set_texture_path", "get_texture_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "filter_clip"), "set_filter_clip", "is_filter_clip");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "regions", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;Rect2i"), "set_regions", "get_regions");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "margins", PROPERTY_HINT_DICTIONARY_TYPE, "StringName;Rect2i"), "set_margins", "get_margins");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "region_order", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_region_order", "_get_region_order");
}
