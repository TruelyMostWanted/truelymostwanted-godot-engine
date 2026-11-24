/**************************************************************************/
/*  video_shadow_settings_server.h                                        */
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
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_server.h"
#include "servers/settings/settings_server.h"

// Settings domain `video_shadow` (#23): the directional shadow atlas (size, 16-bit depth), the positional shadow
// atlas (size, 16-bit depth, quadrant subdivisions) and the soft shadow filter quality (VideoShadowSettings).
// Applied in APPLY_PHASE_SCENE: the root Viewport exists and holds the project values; values set earlier wait.
// The positional atlas values also go to the viewports passed to add_viewport() and to every Viewport in the
// group get_viewport_group() when it enters the tree. The directional atlas and both filter qualities are global
// (RenderingServer).
// Atlas sizes are rounded up to a power of two (like the renderers do) and clamped to 256 to 16384; a positional
// atlas size of 0 disables positional shadows.
// Defaults are the project settings. Precedence: player's file > project settings.
class VideoShadowSettingsServer : public SettingsServer {
	GDCLASS(VideoShadowSettingsServer, SettingsServer);

	static VideoShadowSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed before the root viewport or RenderingServer existed (headless tools, early startup): delivered later.
	HashSet<StringName> undelivered;
	LocalVector<ObjectID> extra_viewports;
	bool defaults_captured = false;

	static Variant _get_project_value(const StringName &p_name);
	static bool _is_global_setting(const StringName &p_name);
	static Viewport *_get_root();
	static int _get_quadrant(const StringName &p_name);

	Variant _get_current(const StringName &p_name) const;
	void _apply_to_viewport(Viewport *p_viewport, const StringName &p_name, const Variant &p_value) const;
	void _apply_all_to_viewport(Viewport *p_viewport);
	void _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_undelivered();
	void _push_shadow_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_shadow_value(const StringName &p_name);
	void _connect_tree();

	void _on_node_added(Node *p_node);
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static VideoShadowSettingsServer *get_singleton();

	// The group of viewports that get the positional atlas values when they enter the tree.
	static StringName get_viewport_group();
	// True if the current renderer uses the value (no check that the domain has it).
	static bool is_renderer_supported(const StringName &p_name);

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;
	PackedStringArray get_unsupported_setting_names() const;

	// Directional shadows (global).
	void set_directional_shadow_atlas_size(int p_size);
	int get_directional_shadow_atlas_size() const;
	void set_directional_shadow_16_bits(bool p_enabled);
	bool is_directional_shadow_16_bits_enabled() const;
	void set_directional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality);
	RenderingServerEnums::ShadowQuality get_directional_soft_shadow_filter_quality() const;

	// Positional shadows (omni and spot lights).
	void set_positional_shadow_atlas_size(int p_size);
	int get_positional_shadow_atlas_size() const;
	void set_positional_shadow_atlas_16_bits(bool p_enabled);
	bool is_positional_shadow_atlas_16_bits_enabled() const;
	void set_positional_shadow_atlas_quadrant_subdiv(int p_quadrant, Viewport::PositionalShadowAtlasQuadrantSubdiv p_subdiv);
	Viewport::PositionalShadowAtlasQuadrantSubdiv get_positional_shadow_atlas_quadrant_subdiv(int p_quadrant) const;
	void set_positional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality);
	RenderingServerEnums::ShadowQuality get_positional_soft_shadow_filter_quality() const;

	// Further viewports (for example a SubViewport that renders the game world).
	void add_viewport(Viewport *p_viewport);
	void remove_viewport(Viewport *p_viewport);
	bool has_viewport(Viewport *p_viewport) const;

	VideoShadowSettingsServer();
	~VideoShadowSettingsServer();
};
