/**************************************************************************/
/*  render_quality_settings_server.h                                      */
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

// Settings domain `render_quality` (#21): 3D scaling (mode, scale, FSR sharpness), mesh LOD, anti-aliasing
// (MSAA 2D/3D, FXAA/SMAA, TAA, debanding), variable rate shading and occlusion culling (RenderQualitySettings).
// Applied in APPLY_PHASE_SCENE: the root Viewport exists and holds the project values; values set earlier wait.
// The per-viewport values also go to the viewports passed to add_viewport() and, by viewport_target, to the viewports
// in the group get_viewport_group() or to every viewport in ViewportServer, also when they enter the tree. The two occlusion values are global (RenderingServer).
// Values the current renderer, hardware or build ignores are stored, but not pushed (is_value_supported()).
// Defaults are the project settings. Precedence: player's file > project settings.
class RenderQualitySettingsServer : public SettingsServer {
	GDCLASS(RenderQualitySettingsServer, SettingsServer);

	static RenderQualitySettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed before the root viewport existed (before APPLY_PHASE_SCENE, headless tools): delivered later.
	HashSet<StringName> undelivered;
	// Settings whose skipped value was already reported, so the warning is printed once per value.
	HashSet<StringName> warned;
	LocalVector<ObjectID> extra_viewports;
	ViewportTargetMode viewport_target = VIEWPORT_TARGET_GROUP;
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
	void _push_quality_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_quality_value(const StringName &p_name);
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

	// `video_quality`: the domain name before #243.
	virtual String _get_legacy_file_name() const override;

public:
	static RenderQualitySettingsServer *get_singleton();

	// The group of viewports that get the per-viewport values when they enter the tree.
	static StringName get_viewport_group();
	// True if the current renderer, hardware and build can use the 3D scaling mode.
	static bool is_scaling_3d_mode_supported(Viewport::Scaling3DMode p_mode);
	// True if the current renderer, hardware and build use the value (no check that the domain has it).
	static bool is_renderer_supported(const StringName &p_name);

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;
	PackedStringArray get_unsupported_setting_names() const;

	// 3D scaling.
	void set_scaling_3d_mode(Viewport::Scaling3DMode p_mode);
	Viewport::Scaling3DMode get_scaling_3d_mode() const;
	void set_scaling_3d_scale(float p_scale);
	float get_scaling_3d_scale() const;
	void set_fsr_sharpness(float p_sharpness);
	float get_fsr_sharpness() const;
	void set_mesh_lod_threshold(float p_pixels);
	float get_mesh_lod_threshold() const;

	// Anti-aliasing.
	void set_msaa_2d(Viewport::MSAA p_msaa);
	Viewport::MSAA get_msaa_2d() const;
	void set_msaa_3d(Viewport::MSAA p_msaa);
	Viewport::MSAA get_msaa_3d() const;
	void set_screen_space_aa(Viewport::ScreenSpaceAA p_screen_space_aa);
	Viewport::ScreenSpaceAA get_screen_space_aa() const;
	void set_use_taa(bool p_use_taa);
	bool is_using_taa() const;
	void set_use_debanding(bool p_use_debanding);
	bool is_using_debanding() const;

	// Variable rate shading and occlusion culling.
	void set_vrs_mode(Viewport::VRSMode p_vrs_mode);
	Viewport::VRSMode get_vrs_mode() const;
	void set_use_occlusion_culling(bool p_use_occlusion_culling);
	bool is_using_occlusion_culling() const;
	void set_occlusion_rays_per_thread(int p_rays_per_thread);
	int get_occlusion_rays_per_thread() const;
	void set_occlusion_culling_build_quality(RenderingServerEnums::ViewportOcclusionCullingBuildQuality p_quality);
	RenderingServerEnums::ViewportOcclusionCullingBuildQuality get_occlusion_culling_build_quality() const;

	// Further viewports (for example a SubViewport that renders the 3D world).
	void add_viewport(Viewport *p_viewport);
	void remove_viewport(Viewport *p_viewport);
	bool has_viewport(Viewport *p_viewport) const;
	void set_viewport_target(ViewportTargetMode p_target);
	ViewportTargetMode get_viewport_target() const;

	RenderQualitySettingsServer();
	~RenderQualitySettingsServer();
};
