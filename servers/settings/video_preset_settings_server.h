/**************************************************************************/
/*  video_preset_settings_server.h                                        */
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

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "servers/settings/settings_server.h"
#include "servers/settings/video_preset.h"

// Settings domain `video_preset` (#25): graphics presets ("Low", "Medium", "High", "Ultra" or the project's own)
// over the video domains `render_quality`, `video_texture`, `video_shadow` and `video_effect`.
// apply_preset() sets all values of each domain resource of a preset (RenderQualitySettings, ...) in one step (one
// apply() and at most one confirmation per domain); a null resource leaves its domain as it is, and values a
// domain does not support on the current renderer keep their value. The domain stores only
// the name of the chosen preset (VideoPresetSettings); the values stay in the files of the video domains.
// A change of a single value in a video domain makes the current preset "Custom" (unless the values then match
// another preset). Window settings (#20) are not part of a preset.
// Presets: the four built-in ones (create_builtin_preset()) and the `.tres` files of the project setting
// `settings/video_preset/presets` (same name: replaces the built-in one).
// Main-thread API: call it from the main thread only; signals are emitted on the main thread.
class VideoPresetSettingsServer : public SettingsServer {
	GDCLASS(VideoPresetSettingsServer, SettingsServer);

	static VideoPresetSettingsServer *singleton;

	LocalVector<Ref<VideoPreset>> presets;
	bool presets_loaded = false;
	// Domain servers used instead of the registered ones (C++ only, for tests and custom setups).
	HashMap<StringName, ObjectID> domain_overrides;
	bool applying_preset = false;
	String last_current;
	bool last_current_known = false;

	void _ensure_presets() const;
	void _load_presets();
	int _find_preset(const String &p_name) const;
	SettingsServer *_get_domain_server(const StringName &p_domain) const;
	void _connect_domain(SettingsServer *p_server);
	void _connect_domains();
	bool _matches(const Ref<VideoPreset> &p_preset) const;
	String _detect(const String &p_preferred) const;
	void _set_stored_preset(const String &p_name);
	void _update_current();

	void _on_domain_value_changed(const StringName &p_name, const Variant &p_value);
	void _on_own_value_changed(const StringName &p_name, const Variant &p_value);
	void _on_own_changed();
	void _on_server_registered(const StringName &p_domain);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _apply_settings(const Ref<Resource> &p_settings, const PackedStringArray &p_names) override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static VideoPresetSettingsServer *get_singleton();
	static void register_settings();

	// The name of the current preset after a single change: "Custom". Reserved; no preset can have it.
	static String get_custom_preset_name();
	static PackedStringArray get_builtin_preset_names();
	// A new preset with the values of the built-in preset `p_name` ("Low", "Medium", "High" or "Ultra"): all four
	// domains, based on the domains' defaults (project settings) with the preset's quality values.
	static Ref<VideoPreset> create_builtin_preset(const String &p_name);

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;

	// Presets.
	TypedArray<VideoPreset> get_presets() const;
	PackedStringArray get_preset_names() const;
	Ref<VideoPreset> get_preset(const String &p_name) const;
	bool has_preset(const String &p_name) const;
	void add_preset(const Ref<VideoPreset> &p_preset);
	void remove_preset(const String &p_name);
	void reload_presets();

	// Current preset.
	Error apply_preset(const String &p_name);
	String get_current_preset() const;
	bool is_custom() const;
	String detect_preset() const;
	bool matches_preset(const String &p_name) const;
	String suggest_preset() const;

	// C++ only: use `p_server` for `p_domain` instead of the server registered with SettingsServerManager
	// (nullptr: the registered one again).
	void set_domain_server(const StringName &p_domain, SettingsServer *p_server);

	VideoPresetSettingsServer();
	~VideoPresetSettingsServer();
};
