/**************************************************************************/
/*  video_texture_settings_server.h                                       */
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

// Settings domain `video_texture` (#22): texture filtering (anisotropic filtering, mipmap bias, light projector
// and decal filter) and Canvas2D (default canvas item texture filter and repeat, pixel snap, SDF, canvas shadow
// atlas size) (VideoTextureSettings).
// Applied in APPLY_PHASE_SCENE: the root Viewport exists and holds the project values; values set earlier wait.
// The per-viewport values also go to the viewports passed to add_viewport() and to every Viewport in the group
// get_viewport_group() when it enters the tree. The projector filter, decal filter and canvas shadow atlas size
// are global (RenderingServer).
// Values the current renderer ignores are stored, but not pushed (is_value_supported()).
// Defaults are the project settings. Precedence: player's file > project settings.
class VideoTextureSettingsServer : public SettingsServer {
	GDCLASS(VideoTextureSettingsServer, SettingsServer);

	static VideoTextureSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed before the root viewport existed (before APPLY_PHASE_SCENE, headless tools): delivered later.
	HashSet<StringName> undelivered;
	// Settings whose skipped value was already reported, so the warning is printed once per value.
	HashSet<StringName> warned;
	LocalVector<ObjectID> extra_viewports;
	bool defaults_captured = false;

	static Variant _get_project_value(const StringName &p_name);
	static bool _is_global_setting(const StringName &p_name);
	static Viewport *_get_root();
	static String _get_rendering_method();

	Variant _get_current(const StringName &p_name) const;
	Variant _get_effective_value(const StringName &p_name, const Variant &p_value);
	void _apply_to_viewport(Viewport *p_viewport, const StringName &p_name, const Variant &p_value) const;
	void _apply_all_to_viewport(Viewport *p_viewport);
	void _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_undelivered();
	void _push_texture_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_texture_value(const StringName &p_name);
	void _connect_tree();

	void _on_node_added(Node *p_node);
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	// Emits the typed `<value>_changed` signal of an applied value.
	virtual void _notify_value_applied(const StringName &p_name, const Variant &p_value) override;

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static VideoTextureSettingsServer *get_singleton();

	// The group of viewports that get the per-viewport values when they enter the tree.
	static StringName get_viewport_group();
	// True if the current renderer uses the value (no check that the domain has it).
	static bool is_renderer_supported(const StringName &p_name);

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;
	PackedStringArray get_unsupported_setting_names() const;

	// Texture filtering.
	void set_anisotropic_filtering_level(Viewport::AnisotropicFiltering p_level);
	Viewport::AnisotropicFiltering get_anisotropic_filtering_level() const;
	void set_texture_mipmap_bias(float p_bias);
	float get_texture_mipmap_bias() const;
	void set_light_projector_filter(RenderingServerEnums::LightProjectorFilter p_filter);
	RenderingServerEnums::LightProjectorFilter get_light_projector_filter() const;
	void set_decal_filter(RenderingServerEnums::DecalFilter p_filter);
	RenderingServerEnums::DecalFilter get_decal_filter() const;

	// Canvas 2D.
	void set_canvas_item_default_texture_filter(Viewport::DefaultCanvasItemTextureFilter p_filter);
	Viewport::DefaultCanvasItemTextureFilter get_canvas_item_default_texture_filter() const;
	void set_canvas_item_default_texture_repeat(Viewport::DefaultCanvasItemTextureRepeat p_repeat);
	Viewport::DefaultCanvasItemTextureRepeat get_canvas_item_default_texture_repeat() const;
	void set_snap_2d_transforms_to_pixel(bool p_enabled);
	bool is_snap_2d_transforms_to_pixel_enabled() const;
	void set_snap_2d_vertices_to_pixel(bool p_enabled);
	bool is_snap_2d_vertices_to_pixel_enabled() const;
	void set_sdf_oversize(Viewport::SDFOversize p_oversize);
	Viewport::SDFOversize get_sdf_oversize() const;
	void set_sdf_scale(Viewport::SDFScale p_scale);
	Viewport::SDFScale get_sdf_scale() const;
	void set_canvas_shadow_atlas_size(int p_size);
	int get_canvas_shadow_atlas_size() const;

	// Further viewports (for example a SubViewport that renders the game world).
	void add_viewport(Viewport *p_viewport);
	void remove_viewport(Viewport *p_viewport);
	bool has_viewport(Viewport *p_viewport) const;

	VideoTextureSettingsServer();
	~VideoTextureSettingsServer();
};
