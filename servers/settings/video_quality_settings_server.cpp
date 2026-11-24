/**************************************************************************/
/*  video_quality_settings_server.cpp                                     */
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

#include "video_quality_settings_server.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_quality_settings.h"

#include "modules/modules_enabled.gen.h" // For raycast.

#ifdef RD_ENABLED
#include "servers/rendering/rendering_device.h"
#endif

// The values of the domain, in the order of VideoQualitySettings.
static const char *video_quality_setting_names[] = {
	"scaling_3d_mode",
	"scaling_3d_scale",
	"fsr_sharpness",
	"mesh_lod_threshold",
	"msaa_2d",
	"msaa_3d",
	"screen_space_aa",
	"use_taa",
	"use_debanding",
	"vrs_mode",
	"use_occlusion_culling",
	"occlusion_rays_per_thread",
	"occlusion_culling_build_quality",
};

static constexpr const char *VIDEO_QUALITY_VIEWPORT_GROUP = "video_quality_settings";

static Variant video_quality_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	// With the feature overrides (`.mobile`, `.ios`, ...), like Viewport and SceneTree read them.
	return project_settings->get_setting_with_override(p_name);
}

#ifdef RD_ENABLED
static bool video_quality_has_rd_feature(RenderingDevice::Features p_feature) {
	const RenderingDevice *rd = RenderingDevice::get_singleton();
	return rd != nullptr && rd->has_feature(p_feature);
}
#endif

// The upper limit of an enum value, or 0 for settings that are no enum.
static int video_quality_enum_size(const StringName &p_name) {
	if (p_name == SNAME("scaling_3d_mode")) {
		return Viewport::SCALING_3D_MODE_MAX;
	} else if (p_name == SNAME("msaa_2d") || p_name == SNAME("msaa_3d")) {
		return Viewport::MSAA_MAX;
	} else if (p_name == SNAME("screen_space_aa")) {
		return Viewport::SCREEN_SPACE_AA_MAX;
	} else if (p_name == SNAME("vrs_mode")) {
		return Viewport::VRS_MAX;
	} else if (p_name == SNAME("occlusion_culling_build_quality")) {
		return RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_HIGH + 1;
	}
	return 0;
}

static bool video_quality_range(const StringName &p_name, double &r_min, double &r_max, bool &r_integer) {
	r_integer = false;
	if (p_name == SNAME("scaling_3d_scale")) {
		// What Viewport::set_scaling_3d_scale() accepts.
		r_min = 0.1;
		r_max = 2.0;
	} else if (p_name == SNAME("fsr_sharpness")) {
		r_min = 0.0;
		r_max = 2.0;
	} else if (p_name == SNAME("mesh_lod_threshold")) {
		r_min = 0.0;
		r_max = 1024.0;
	} else if (p_name == SNAME("occlusion_rays_per_thread")) {
		r_min = 1;
		r_max = 2048;
		r_integer = true;
	} else {
		return false;
	}
	return true;
}

VideoQualitySettingsServer *VideoQualitySettingsServer::singleton = nullptr;

VideoQualitySettingsServer *VideoQualitySettingsServer::get_singleton() {
	return singleton;
}

StringName VideoQualitySettingsServer::get_viewport_group() {
	return VIDEO_QUALITY_VIEWPORT_GROUP;
}

/////////////////////////////////////////////////////////////////////////////
// Renderer support.

String VideoQualitySettingsServer::_get_rendering_method() {
	const OS *os = OS::get_singleton();
	return os != nullptr ? os->get_current_rendering_method() : String();
}

bool VideoQualitySettingsServer::is_scaling_3d_mode_supported(Viewport::Scaling3DMode p_mode) {
	const String method = _get_rendering_method();
	const bool compatibility = method == "gl_compatibility";
	const bool mobile = method == "mobile";
	switch (p_mode) {
		case Viewport::SCALING_3D_MODE_BILINEAR:
		case Viewport::SCALING_3D_MODE_NEAREST: {
			return true;
		}
		case Viewport::SCALING_3D_MODE_FSR:
		case Viewport::SCALING_3D_MODE_FSR2: {
			// RendererViewport falls back to bilinear scaling on Mobile and Compatibility.
			return !compatibility && !mobile;
		}
		case Viewport::SCALING_3D_MODE_METALFX_SPATIAL: {
#ifdef RD_ENABLED
			return !compatibility && video_quality_has_rd_feature(RenderingDevice::SUPPORTS_METALFX_SPATIAL);
#else
			return false;
#endif
		}
		case Viewport::SCALING_3D_MODE_METALFX_TEMPORAL: {
#ifdef RD_ENABLED
			return !compatibility && !mobile && video_quality_has_rd_feature(RenderingDevice::SUPPORTS_METALFX_TEMPORAL);
#else
			return false;
#endif
		}
		default: {
			return false;
		}
	}
}

bool VideoQualitySettingsServer::is_renderer_supported(const StringName &p_name) {
	const String method = _get_rendering_method();
	const bool compatibility = method == "gl_compatibility";
	const bool mobile = method == "mobile";
	if (p_name == SNAME("fsr_sharpness")) {
		// Only used by FSR 1.0 and FSR 2.2: Forward+ only.
		return !compatibility && !mobile;
	} else if (p_name == SNAME("screen_space_aa") || p_name == SNAME("use_debanding")) {
		// RenderingDevice effects; the Compatibility renderer has neither FXAA/SMAA nor debanding.
		return !compatibility;
	} else if (p_name == SNAME("use_taa")) {
		return !compatibility && !mobile;
	} else if (p_name == SNAME("vrs_mode")) {
#ifdef RD_ENABLED
		return !compatibility && video_quality_has_rd_feature(RenderingDevice::SUPPORTS_ATTACHMENT_VRS);
#else
		return false;
#endif
	} else if (p_name == SNAME("use_occlusion_culling") || p_name == SNAME("occlusion_rays_per_thread") || p_name == SNAME("occlusion_culling_build_quality")) {
		// Occlusion culling is done on the CPU with Embree (raycast module), in every renderer.
#ifdef MODULE_RAYCAST_ENABLED
		return true;
#else
		return false;
#endif
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

Variant VideoQualitySettingsServer::_get_project_value(const StringName &p_name) {
	// What Viewport, SceneTree and RenderingServer read from the project settings.
	if (p_name == SNAME("scaling_3d_mode")) {
		return int(video_quality_project_setting("rendering/scaling_3d/mode", int(Viewport::SCALING_3D_MODE_BILINEAR)));
	} else if (p_name == SNAME("scaling_3d_scale")) {
		return float(video_quality_project_setting("rendering/scaling_3d/scale", 1.0));
	} else if (p_name == SNAME("fsr_sharpness")) {
		return float(video_quality_project_setting("rendering/scaling_3d/fsr_sharpness", 0.2));
	} else if (p_name == SNAME("mesh_lod_threshold")) {
		return float(video_quality_project_setting("rendering/mesh_lod/lod_change/threshold_pixels", 1.0));
	} else if (p_name == SNAME("msaa_2d")) {
		return int(video_quality_project_setting("rendering/anti_aliasing/quality/msaa_2d", int(Viewport::MSAA_DISABLED)));
	} else if (p_name == SNAME("msaa_3d")) {
		return int(video_quality_project_setting("rendering/anti_aliasing/quality/msaa_3d", int(Viewport::MSAA_DISABLED)));
	} else if (p_name == SNAME("screen_space_aa")) {
		return int(video_quality_project_setting("rendering/anti_aliasing/quality/screen_space_aa", int(Viewport::SCREEN_SPACE_AA_DISABLED)));
	} else if (p_name == SNAME("use_taa")) {
		return bool(video_quality_project_setting("rendering/anti_aliasing/quality/use_taa", false));
	} else if (p_name == SNAME("use_debanding")) {
		return bool(video_quality_project_setting("rendering/anti_aliasing/quality/use_debanding", false));
	} else if (p_name == SNAME("vrs_mode")) {
		return int(video_quality_project_setting("rendering/vrs/mode", int(Viewport::VRS_DISABLED)));
	} else if (p_name == SNAME("use_occlusion_culling")) {
		return bool(video_quality_project_setting("rendering/occlusion_culling/use_occlusion_culling", false));
	} else if (p_name == SNAME("occlusion_rays_per_thread")) {
		return int(video_quality_project_setting("rendering/occlusion_culling/occlusion_rays_per_thread", 512));
	} else if (p_name == SNAME("occlusion_culling_build_quality")) {
		return int(video_quality_project_setting("rendering/occlusion_culling/bvh_build_quality", int(RenderingServerEnums::VIEWPORT_OCCLUSION_BUILD_QUALITY_HIGH)));
	}
	return Variant();
}

bool VideoQualitySettingsServer::_is_global_setting(const StringName &p_name) {
	return p_name == SNAME("occlusion_rays_per_thread") || p_name == SNAME("occlusion_culling_build_quality");
}

Viewport *VideoQualitySettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

Variant VideoQualitySettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

Variant VideoQualitySettingsServer::_get_effective_value(const StringName &p_name, const Variant &p_value) {
	// What reaches the engine: the value, a fallback, or nothing (Variant()) if the renderer ignores it.
	String skipped_as;
	Variant effective = p_value;
	if (p_name == SNAME("scaling_3d_mode")) {
		if (!is_scaling_3d_mode_supported(Viewport::Scaling3DMode(int(p_value)))) {
			effective = int(Viewport::SCALING_3D_MODE_BILINEAR);
			skipped_as = "bilinear scaling is used";
		}
	} else if (!is_renderer_supported(p_name)) {
		effective = Variant();
		skipped_as = "the value is not applied";
	}

	if (skipped_as.is_empty()) {
		warned.erase(p_name);
	} else if (!warned.has(p_name) && !values_equal(p_value, _get_project_value(p_name))) {
		// The player's (or the game's) choice, not the project value: worth a warning, once.
		warned.insert(p_name);
		const String method = _get_rendering_method();
		WARN_PRINT(vformat("Video quality settings: \"%s\" = %s is not supported by the current renderer (\"%s\"), hardware or build; %s. See VideoQualitySettingsServer.is_value_supported().", p_name, p_value, method, skipped_as));
	}
	return effective;
}

void VideoQualitySettingsServer::_apply_to_viewport(Viewport *p_viewport, const StringName &p_name, const Variant &p_value) const {
	if (p_name == SNAME("scaling_3d_mode")) {
		p_viewport->set_scaling_3d_mode(Viewport::Scaling3DMode(int(p_value)));
	} else if (p_name == SNAME("scaling_3d_scale")) {
		p_viewport->set_scaling_3d_scale(float(p_value));
	} else if (p_name == SNAME("fsr_sharpness")) {
		p_viewport->set_fsr_sharpness(float(p_value));
	} else if (p_name == SNAME("mesh_lod_threshold")) {
		p_viewport->set_mesh_lod_threshold(float(p_value));
	} else if (p_name == SNAME("msaa_2d")) {
		p_viewport->set_msaa_2d(Viewport::MSAA(int(p_value)));
	} else if (p_name == SNAME("msaa_3d")) {
		p_viewport->set_msaa_3d(Viewport::MSAA(int(p_value)));
	} else if (p_name == SNAME("screen_space_aa")) {
		p_viewport->set_screen_space_aa(Viewport::ScreenSpaceAA(int(p_value)));
	} else if (p_name == SNAME("use_taa")) {
		p_viewport->set_use_taa(bool(p_value));
	} else if (p_name == SNAME("use_debanding")) {
		p_viewport->set_use_debanding(bool(p_value));
	} else if (p_name == SNAME("vrs_mode")) {
		p_viewport->set_vrs_mode(Viewport::VRSMode(int(p_value)));
	} else if (p_name == SNAME("use_occlusion_culling")) {
		p_viewport->set_use_occlusion_culling(bool(p_value));
	}
}

void VideoQualitySettingsServer::_apply_all_to_viewport(Viewport *p_viewport) {
	for (const char *setting_name : video_quality_setting_names) {
		const StringName name_key = setting_name;
		if (_is_global_setting(name_key)) {
			continue;
		}
		const Variant effective = _get_effective_value(name_key, _get_current(name_key));
		if (effective.get_type() != Variant::NIL) {
			_apply_to_viewport(p_viewport, name_key, effective);
		}
	}
}

void VideoQualitySettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	const Variant effective = _get_effective_value(p_name, p_value);
	if (effective.get_type() == Variant::NIL) {
		return;
	}

	if (_is_global_setting(p_name)) {
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server == nullptr) {
			return; // Delivered when it exists (undelivered).
		}
		if (p_name == SNAME("occlusion_rays_per_thread")) {
			rendering_server->viewport_set_occlusion_rays_per_thread(int(effective));
		} else {
			rendering_server->viewport_set_occlusion_culling_build_quality(RenderingServerEnums::ViewportOcclusionCullingBuildQuality(int(effective)));
		}
		return;
	}

	Viewport *root = _get_root();
	if (root != nullptr) {
		_apply_to_viewport(root, p_name, effective);
	}
	for (uint32_t i = 0; i < extra_viewports.size();) {
		Viewport *viewport = ObjectDB::get_instance<Viewport>(extra_viewports[i]);
		if (viewport == nullptr) {
			extra_viewports.remove_at_unordered(i); // Freed without remove_viewport().
			continue;
		}
		_apply_to_viewport(viewport, p_name, effective);
		i++;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree != nullptr) {
		for (Node *node : tree->get_nodes_in_group(get_viewport_group())) {
			Viewport *viewport = Object::cast_to<Viewport>(node);
			if (viewport != nullptr && viewport != root && !has_viewport(viewport)) {
				_apply_to_viewport(viewport, p_name, effective);
			}
		}
	}
}

void VideoQualitySettingsServer::_deliver_undelivered() {
	const bool has_root = _get_root() != nullptr;
	const bool has_rendering_server = RenderingServer::get_singleton() != nullptr;
	LocalVector<StringName> names;
	for (const StringName &undelivered_name : undelivered) {
		if (_is_global_setting(undelivered_name) ? has_rendering_server : has_root) {
			names.push_back(undelivered_name);
		}
	}
	for (const StringName &undelivered_name : names) {
		undelivered.erase(undelivered_name);
		_deliver(undelivered_name, _get_current(undelivered_name));
	}
}

void VideoQualitySettingsServer::_push_quality_value(const Variant &p_value, const StringName &p_name) {
	pushed_values[p_name] = p_value;
	const bool deliverable = _is_global_setting(p_name) ? RenderingServer::get_singleton() != nullptr : _get_root() != nullptr;
	if (deliverable) {
		undelivered.erase(p_name);
	} else {
		// Before the root viewport exists: given to it in APPLY_PHASE_SCENE.
		undelivered.insert(p_name);
	}
	_deliver(p_name, p_value);
	_connect_tree();
}

Variant VideoQualitySettingsServer::_pull_quality_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the engine state (which a game may have changed already).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	const Viewport *root = _get_root();
	if (root == nullptr || _is_global_setting(p_name) || undelivered.has(p_name) || !is_renderer_supported(p_name)) {
		// RenderingServer has no getters for the global values; skipped values stay as stored.
		return _get_current(p_name);
	}
	if (p_name == SNAME("scaling_3d_mode")) {
		const Variant current = _get_current(p_name);
		if (!is_scaling_3d_mode_supported(Viewport::Scaling3DMode(int(current)))) {
			return current; // The root has the bilinear fallback.
		}
		return int(root->get_scaling_3d_mode());
	} else if (p_name == SNAME("scaling_3d_scale")) {
		return root->get_scaling_3d_scale();
	} else if (p_name == SNAME("fsr_sharpness")) {
		return root->get_fsr_sharpness();
	} else if (p_name == SNAME("mesh_lod_threshold")) {
		return root->get_mesh_lod_threshold();
	} else if (p_name == SNAME("msaa_2d")) {
		return int(root->get_msaa_2d());
	} else if (p_name == SNAME("msaa_3d")) {
		return int(root->get_msaa_3d());
	} else if (p_name == SNAME("screen_space_aa")) {
		return int(root->get_screen_space_aa());
	} else if (p_name == SNAME("use_taa")) {
		return root->is_using_taa();
	} else if (p_name == SNAME("use_debanding")) {
		return root->is_using_debanding();
	} else if (p_name == SNAME("vrs_mode")) {
		return int(root->get_vrs_mode());
	} else if (p_name == SNAME("use_occlusion_culling")) {
		return root->is_using_occlusion_culling();
	}
	return _get_current(p_name);
}

void VideoQualitySettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &VideoQualitySettingsServer::_on_node_added);
	if (!tree->is_connected(SNAME("node_added"), callback)) {
		tree->connect(SNAME("node_added"), callback);
	}
}

void VideoQualitySettingsServer::_on_node_added(Node *p_node) {
	Viewport *viewport = Object::cast_to<Viewport>(p_node);
	if (viewport == nullptr || viewport == _get_root() || has_viewport(viewport) || !viewport->is_in_group(get_viewport_group())) {
		return;
	}
	_apply_all_to_viewport(viewport);
}

void VideoQualitySettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// After SceneTree has set the project values of the root viewport.
		_deliver_undelivered();
		_connect_tree();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName VideoQualitySettingsServer::get_domain() const {
	return SNAME("video_quality");
}

SettingsServer::ApplyPhase VideoQualitySettingsServer::get_apply_phase() const {
	// The root viewport exists and has its project values; the main scene is not loaded yet.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> VideoQualitySettingsServer::_create_default_settings() const {
	Ref<VideoQualitySettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void VideoQualitySettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant VideoQualitySettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}

	const int enum_size = video_quality_enum_size(p_name);
	if (enum_size > 0) {
		const double number = value;
		if (Math::is_nan(number) || number < 0 || number >= enum_size) {
			const Variant default_value = get_default_value(p_name);
			WARN_PRINT(vformat("Video quality settings: %s is not a valid value for \"%s\"; %s is used.", value, p_name, default_value));
			return default_value;
		}
		return int64_t(number);
	}

	double min_value = 0.0;
	double max_value = 0.0;
	bool integer = false;
	if (video_quality_range(p_name, min_value, max_value, integer)) {
		const double number = value;
		if (Math::is_nan(number)) {
			WARN_PRINT(vformat("Video quality settings: \"%s\" is not a number; the default is used.", p_name));
			return get_default_value(p_name);
		}
		const double clamped = CLAMP(number, min_value, max_value);
		if (clamped != number) {
			WARN_PRINT(vformat("Video quality settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(min_value), Variant(max_value), Variant(clamped)));
		}
		return integer ? Variant(int64_t(Math::round(clamped))) : Variant(clamped);
	}
	return value;
}

bool VideoQualitySettingsServer::is_value_supported(const StringName &p_name) const {
	return SettingsServer::is_value_supported(p_name) && is_renderer_supported(p_name);
}

PackedStringArray VideoQualitySettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : video_quality_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Further viewports.

void VideoQualitySettingsServer::add_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	if (has_viewport(p_viewport)) {
		return;
	}
	extra_viewports.push_back(p_viewport->get_instance_id());
	_apply_all_to_viewport(p_viewport);
}

void VideoQualitySettingsServer::remove_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	extra_viewports.erase(p_viewport->get_instance_id());
}

bool VideoQualitySettingsServer::has_viewport(Viewport *p_viewport) const {
	return p_viewport != nullptr && extra_viewports.has(p_viewport->get_instance_id());
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void VideoQualitySettingsServer::set_scaling_3d_mode(Viewport::Scaling3DMode p_mode) {
	set_value(SNAME("scaling_3d_mode"), int(p_mode));
}

Viewport::Scaling3DMode VideoQualitySettingsServer::get_scaling_3d_mode() const {
	return Viewport::Scaling3DMode(int(get_value(SNAME("scaling_3d_mode"))));
}

void VideoQualitySettingsServer::set_scaling_3d_scale(float p_scale) {
	set_value(SNAME("scaling_3d_scale"), p_scale);
}

float VideoQualitySettingsServer::get_scaling_3d_scale() const {
	return get_value(SNAME("scaling_3d_scale"));
}

void VideoQualitySettingsServer::set_fsr_sharpness(float p_sharpness) {
	set_value(SNAME("fsr_sharpness"), p_sharpness);
}

float VideoQualitySettingsServer::get_fsr_sharpness() const {
	return get_value(SNAME("fsr_sharpness"));
}

void VideoQualitySettingsServer::set_mesh_lod_threshold(float p_pixels) {
	set_value(SNAME("mesh_lod_threshold"), p_pixels);
}

float VideoQualitySettingsServer::get_mesh_lod_threshold() const {
	return get_value(SNAME("mesh_lod_threshold"));
}

void VideoQualitySettingsServer::set_msaa_2d(Viewport::MSAA p_msaa) {
	set_value(SNAME("msaa_2d"), int(p_msaa));
}

Viewport::MSAA VideoQualitySettingsServer::get_msaa_2d() const {
	return Viewport::MSAA(int(get_value(SNAME("msaa_2d"))));
}

void VideoQualitySettingsServer::set_msaa_3d(Viewport::MSAA p_msaa) {
	set_value(SNAME("msaa_3d"), int(p_msaa));
}

Viewport::MSAA VideoQualitySettingsServer::get_msaa_3d() const {
	return Viewport::MSAA(int(get_value(SNAME("msaa_3d"))));
}

void VideoQualitySettingsServer::set_screen_space_aa(Viewport::ScreenSpaceAA p_screen_space_aa) {
	set_value(SNAME("screen_space_aa"), int(p_screen_space_aa));
}

Viewport::ScreenSpaceAA VideoQualitySettingsServer::get_screen_space_aa() const {
	return Viewport::ScreenSpaceAA(int(get_value(SNAME("screen_space_aa"))));
}

void VideoQualitySettingsServer::set_use_taa(bool p_use_taa) {
	set_value(SNAME("use_taa"), p_use_taa);
}

bool VideoQualitySettingsServer::is_using_taa() const {
	return get_value(SNAME("use_taa"));
}

void VideoQualitySettingsServer::set_use_debanding(bool p_use_debanding) {
	set_value(SNAME("use_debanding"), p_use_debanding);
}

bool VideoQualitySettingsServer::is_using_debanding() const {
	return get_value(SNAME("use_debanding"));
}

void VideoQualitySettingsServer::set_vrs_mode(Viewport::VRSMode p_vrs_mode) {
	set_value(SNAME("vrs_mode"), int(p_vrs_mode));
}

Viewport::VRSMode VideoQualitySettingsServer::get_vrs_mode() const {
	return Viewport::VRSMode(int(get_value(SNAME("vrs_mode"))));
}

void VideoQualitySettingsServer::set_use_occlusion_culling(bool p_use_occlusion_culling) {
	set_value(SNAME("use_occlusion_culling"), p_use_occlusion_culling);
}

bool VideoQualitySettingsServer::is_using_occlusion_culling() const {
	return get_value(SNAME("use_occlusion_culling"));
}

void VideoQualitySettingsServer::set_occlusion_rays_per_thread(int p_rays_per_thread) {
	set_value(SNAME("occlusion_rays_per_thread"), p_rays_per_thread);
}

int VideoQualitySettingsServer::get_occlusion_rays_per_thread() const {
	return get_value(SNAME("occlusion_rays_per_thread"));
}

void VideoQualitySettingsServer::set_occlusion_culling_build_quality(RenderingServerEnums::ViewportOcclusionCullingBuildQuality p_quality) {
	set_value(SNAME("occlusion_culling_build_quality"), int(p_quality));
}

RenderingServerEnums::ViewportOcclusionCullingBuildQuality VideoQualitySettingsServer::get_occlusion_culling_build_quality() const {
	return RenderingServerEnums::ViewportOcclusionCullingBuildQuality(int(get_value(SNAME("occlusion_culling_build_quality"))));
}

void VideoQualitySettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scaling_3d_mode", "mode"), &VideoQualitySettingsServer::set_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_mode"), &VideoQualitySettingsServer::get_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("set_scaling_3d_scale", "scale"), &VideoQualitySettingsServer::set_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_scale"), &VideoQualitySettingsServer::get_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("set_fsr_sharpness", "sharpness"), &VideoQualitySettingsServer::set_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("get_fsr_sharpness"), &VideoQualitySettingsServer::get_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("set_mesh_lod_threshold", "pixels"), &VideoQualitySettingsServer::set_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_threshold"), &VideoQualitySettingsServer::get_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("set_msaa_2d", "msaa"), &VideoQualitySettingsServer::set_msaa_2d);
	ClassDB::bind_method(D_METHOD("get_msaa_2d"), &VideoQualitySettingsServer::get_msaa_2d);
	ClassDB::bind_method(D_METHOD("set_msaa_3d", "msaa"), &VideoQualitySettingsServer::set_msaa_3d);
	ClassDB::bind_method(D_METHOD("get_msaa_3d"), &VideoQualitySettingsServer::get_msaa_3d);
	ClassDB::bind_method(D_METHOD("set_screen_space_aa", "screen_space_aa"), &VideoQualitySettingsServer::set_screen_space_aa);
	ClassDB::bind_method(D_METHOD("get_screen_space_aa"), &VideoQualitySettingsServer::get_screen_space_aa);
	ClassDB::bind_method(D_METHOD("set_use_taa", "enable"), &VideoQualitySettingsServer::set_use_taa);
	ClassDB::bind_method(D_METHOD("is_using_taa"), &VideoQualitySettingsServer::is_using_taa);
	ClassDB::bind_method(D_METHOD("set_use_debanding", "enable"), &VideoQualitySettingsServer::set_use_debanding);
	ClassDB::bind_method(D_METHOD("is_using_debanding"), &VideoQualitySettingsServer::is_using_debanding);
	ClassDB::bind_method(D_METHOD("set_vrs_mode", "mode"), &VideoQualitySettingsServer::set_vrs_mode);
	ClassDB::bind_method(D_METHOD("get_vrs_mode"), &VideoQualitySettingsServer::get_vrs_mode);
	ClassDB::bind_method(D_METHOD("set_use_occlusion_culling", "enable"), &VideoQualitySettingsServer::set_use_occlusion_culling);
	ClassDB::bind_method(D_METHOD("is_using_occlusion_culling"), &VideoQualitySettingsServer::is_using_occlusion_culling);
	ClassDB::bind_method(D_METHOD("set_occlusion_rays_per_thread", "rays_per_thread"), &VideoQualitySettingsServer::set_occlusion_rays_per_thread);
	ClassDB::bind_method(D_METHOD("get_occlusion_rays_per_thread"), &VideoQualitySettingsServer::get_occlusion_rays_per_thread);
	ClassDB::bind_method(D_METHOD("set_occlusion_culling_build_quality", "quality"), &VideoQualitySettingsServer::set_occlusion_culling_build_quality);
	ClassDB::bind_method(D_METHOD("get_occlusion_culling_build_quality"), &VideoQualitySettingsServer::get_occlusion_culling_build_quality);

	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &VideoQualitySettingsServer::get_unsupported_setting_names);
	ClassDB::bind_method(D_METHOD("add_viewport", "viewport"), &VideoQualitySettingsServer::add_viewport);
	ClassDB::bind_method(D_METHOD("remove_viewport", "viewport"), &VideoQualitySettingsServer::remove_viewport);
	ClassDB::bind_method(D_METHOD("has_viewport", "viewport"), &VideoQualitySettingsServer::has_viewport);

	ClassDB::bind_static_method("VideoQualitySettingsServer", D_METHOD("get_viewport_group"), &VideoQualitySettingsServer::get_viewport_group);
	ClassDB::bind_static_method("VideoQualitySettingsServer", D_METHOD("is_scaling_3d_mode_supported", "mode"), &VideoQualitySettingsServer::is_scaling_3d_mode_supported);
	ClassDB::bind_static_method("VideoQualitySettingsServer", D_METHOD("is_renderer_supported", "name"), &VideoQualitySettingsServer::is_renderer_supported);
}

VideoQualitySettingsServer::VideoQualitySettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : video_quality_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &VideoQualitySettingsServer::_push_quality_value).bind(bound_name), callable_mp(this, &VideoQualitySettingsServer::_pull_quality_value).bind(bound_name));
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &VideoQualitySettingsServer::_on_startup_applied));
	}
}

VideoQualitySettingsServer::~VideoQualitySettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
