/**************************************************************************/
/*  video_texture_streaming_settings_server.h                             */
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

#include "modules/modules_enabled.gen.h" // For texture_streaming.

#ifdef MODULE_TEXTURE_STREAMING_ENABLED

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "servers/settings/settings_server.h"

// Settings domain `video_texture_streaming` (#22): texture streaming (VideoTextureStreamingSettings).
// Only built and registered with the `texture_streaming` module (not on the Web: it needs a RenderingDevice).
// Applied in APPLY_PHASE_PRE_DISPLAY: `enabled` is written to the project settings before the renderers compile
// their shaders and the TextureStreaming singleton starts, so the stored value takes effect at the next start
// (SETTING_FLAG_RESTART_REQUIRED). `memory_budget_enabled` goes to the project settings, which TextureStreaming
// reads again on `ProjectSettings.settings_changed`. `memory_budget_mb`, `min_lod` and `max_lod` go to the
// override properties of TextureStreaming once the singleton exists (after the scene-level modules).
// The Compatibility renderer has no texture streaming: every value is stored, but not pushed.
// Defaults are the project settings `rendering/textures/streaming/*`. Precedence: player's file > project settings.
class VideoTextureStreamingSettingsServer : public SettingsServer {
	GDCLASS(VideoTextureStreamingSettingsServer, SettingsServer);

	static VideoTextureStreamingSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Project values before this domain wrote them (`enabled`, `memory_budget_enabled`): the defaults stay the project's.
	HashMap<StringName, Variant> original_project_values;
	// Pushed before the TextureStreaming singleton existed: delivered after APPLY_PHASE_SERVERS.
	HashSet<StringName> undelivered;
	// Settings whose skipped value was already reported, so the warning is printed once per value.
	HashSet<StringName> warned;
	bool defaults_captured = false;

	static String _get_project_setting_path(const StringName &p_name);
	static bool _is_project_setting_value(const StringName &p_name);
	static String _get_rendering_method();

	Variant _get_project_value(const StringName &p_name) const;
	Variant _get_current(const StringName &p_name) const;
	bool _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_undelivered();
	void _push_streaming_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_streaming_value(const StringName &p_name);

	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static VideoTextureStreamingSettingsServer *get_singleton();

	// True if the current renderer can stream textures (Forward+ and Mobile).
	static bool is_renderer_supported();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;
	PackedStringArray get_unsupported_setting_names() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;
	void set_memory_budget_enabled(bool p_enabled);
	bool is_memory_budget_enabled() const;
	void set_memory_budget_mb(int p_mb);
	int get_memory_budget_mb() const;
	void set_min_lod(int p_lod);
	int get_min_lod() const;
	void set_max_lod(int p_lod);
	int get_max_lod() const;
	// The texture memory used by streamed textures now (TextureStreaming.get_memory_budget_bytes_used()), or 0.
	int64_t get_memory_budget_bytes_used() const;

	VideoTextureStreamingSettingsServer();
	~VideoTextureStreamingSettingsServer();
};

#endif // MODULE_TEXTURE_STREAMING_ENABLED
