/**************************************************************************/
/*  video_preset.h                                                        */
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
#include "core/variant/dictionary.h"

// A graphics preset (#25): a name and the values it sets in the video settings domains `video_quality`,
// `video_texture`, `video_shadow` and `video_effect`. Each domain has a Dictionary (setting name -> value) that
// holds only the values the preset sets; the other values of the domain stay as they are.
// VideoPresetSettingsServer applies presets; the four built-in presets are created in code, projects add or
// replace presets with `.tres` files (project setting `settings/video_preset/presets`).
class VideoPreset : public Resource {
	GDCLASS(VideoPreset, Resource);

	String preset_name;
	Dictionary video_quality;
	Dictionary video_texture;
	Dictionary video_shadow;
	Dictionary video_effect;

	Dictionary *_get_domain_dictionary(const StringName &p_domain);
	const Dictionary *_get_domain_dictionary(const StringName &p_domain) const;

protected:
	static void _bind_methods();

public:
	// The settings domains a preset covers, in the order they are applied.
	static PackedStringArray get_domains();

	void set_preset_name(const String &p_name);
	String get_preset_name() const;

	void set_video_quality(const Dictionary &p_values);
	Dictionary get_video_quality() const;
	void set_video_texture(const Dictionary &p_values);
	Dictionary get_video_texture() const;
	void set_video_shadow(const Dictionary &p_values);
	Dictionary get_video_shadow() const;
	void set_video_effect(const Dictionary &p_values);
	Dictionary get_video_effect() const;

	// Generic access by domain name (one of get_domains()).
	void set_domain_values(const StringName &p_domain, const Dictionary &p_values);
	Dictionary get_domain_values(const StringName &p_domain) const;
	void set_value(const StringName &p_domain, const StringName &p_name, const Variant &p_value);
	Variant get_value(const StringName &p_domain, const StringName &p_name, const Variant &p_default = Variant()) const;
	bool has_value(const StringName &p_domain, const StringName &p_name) const;
	void erase_value(const StringName &p_domain, const StringName &p_name);
	void clear();
};
