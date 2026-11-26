/**************************************************************************/
/*  texture_settings_server.cpp                                           */
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

#include "texture_settings_server.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/texture_settings.h"

// The values of the domain, in the order of TextureSettings.
static const char *texture_setting_names[] = {
	"anisotropic_filtering_level",
	"texture_mipmap_bias",
	"light_projector_filter",
	"decal_filter",
	"canvas_item_default_texture_filter",
	"canvas_item_default_texture_repeat",
	"snap_2d_transforms_to_pixel",
	"snap_2d_vertices_to_pixel",
	"sdf_oversize",
	"sdf_scale",
	"canvas_shadow_atlas_size",
};

static constexpr const char *TEXTURE_VIEWPORT_GROUP = "texture_settings";

static Variant texture_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	// With the feature overrides (`.mobile`, `.web`, ...), like Viewport, SceneTree and the renderers read them.
	return project_settings->get_setting_with_override(p_name);
}

// The upper limit of an enum value, or 0 for settings that are no enum.
static int texture_enum_size(const StringName &p_name) {
	if (p_name == SNAME("anisotropic_filtering_level")) {
		return Viewport::ANISOTROPY_MAX;
	} else if (p_name == SNAME("light_projector_filter")) {
		return RenderingServerEnums::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC + 1;
	} else if (p_name == SNAME("decal_filter")) {
		return RenderingServerEnums::DECAL_FILTER_LINEAR_MIPMAPS_ANISOTROPIC + 1;
	} else if (p_name == SNAME("canvas_item_default_texture_filter")) {
		return Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_MAX;
	} else if (p_name == SNAME("canvas_item_default_texture_repeat")) {
		return Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MAX;
	} else if (p_name == SNAME("sdf_oversize")) {
		return Viewport::SDF_OVERSIZE_MAX;
	} else if (p_name == SNAME("sdf_scale")) {
		return Viewport::SDF_SCALE_MAX;
	}
	return 0;
}

static bool texture_range(const StringName &p_name, double &r_min, double &r_max, bool &r_integer) {
	r_integer = false;
	if (p_name == SNAME("texture_mipmap_bias")) {
		// The range of the project setting `rendering/textures/default_filters/texture_mipmap_bias`.
		r_min = -2.0;
		r_max = 2.0;
	} else if (p_name == SNAME("canvas_shadow_atlas_size")) {
		// The range of the project setting `rendering/2d/shadow_atlas/size`.
		r_min = 128;
		r_max = 16384;
		r_integer = true;
	} else {
		return false;
	}
	return true;
}

TextureSettingsServer *TextureSettingsServer::singleton = nullptr;

TextureSettingsServer *TextureSettingsServer::get_singleton() {
	return singleton;
}

StringName TextureSettingsServer::get_viewport_group() {
	return TEXTURE_VIEWPORT_GROUP;
}

/////////////////////////////////////////////////////////////////////////////
// Renderer support.

String TextureSettingsServer::_get_rendering_method() {
	const OS *os = OS::get_singleton();
	return os != nullptr ? os->get_current_rendering_method() : String();
}

bool TextureSettingsServer::is_renderer_supported(const StringName &p_name) {
	if (p_name == SNAME("light_projector_filter") || p_name == SNAME("decal_filter")) {
		// The Compatibility renderer ignores the light projector filter and has no decals.
		return _get_rendering_method() != "gl_compatibility";
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

Variant TextureSettingsServer::_get_project_value(const StringName &p_name) {
	// What Viewport, SceneTree, main.cpp and the renderers read from the project settings.
	if (p_name == SNAME("anisotropic_filtering_level")) {
		return int(texture_project_setting("rendering/textures/default_filters/anisotropic_filtering_level", int(Viewport::ANISOTROPY_4X)));
	} else if (p_name == SNAME("texture_mipmap_bias")) {
		return float(texture_project_setting("rendering/textures/default_filters/texture_mipmap_bias", 0.0));
	} else if (p_name == SNAME("light_projector_filter")) {
		return int(texture_project_setting("rendering/textures/light_projectors/filter", int(RenderingServerEnums::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS)));
	} else if (p_name == SNAME("decal_filter")) {
		return int(texture_project_setting("rendering/textures/decals/filter", int(RenderingServerEnums::DECAL_FILTER_LINEAR_MIPMAPS)));
	} else if (p_name == SNAME("canvas_item_default_texture_filter")) {
		return int(texture_project_setting("rendering/textures/canvas_textures/default_texture_filter", int(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR)));
	} else if (p_name == SNAME("canvas_item_default_texture_repeat")) {
		return int(texture_project_setting("rendering/textures/canvas_textures/default_texture_repeat", int(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED)));
	} else if (p_name == SNAME("snap_2d_transforms_to_pixel")) {
		return bool(texture_project_setting("rendering/2d/snap/snap_2d_transforms_to_pixel", false));
	} else if (p_name == SNAME("snap_2d_vertices_to_pixel")) {
		return bool(texture_project_setting("rendering/2d/snap/snap_2d_vertices_to_pixel", false));
	} else if (p_name == SNAME("sdf_oversize")) {
		return int(texture_project_setting("rendering/2d/sdf/oversize", int(Viewport::SDF_OVERSIZE_120_PERCENT)));
	} else if (p_name == SNAME("sdf_scale")) {
		return int(texture_project_setting("rendering/2d/sdf/scale", int(Viewport::SDF_SCALE_50_PERCENT)));
	} else if (p_name == SNAME("canvas_shadow_atlas_size")) {
		return int(texture_project_setting("rendering/2d/shadow_atlas/size", 2048));
	}
	return Variant();
}

bool TextureSettingsServer::_is_global_setting(const StringName &p_name) {
	return p_name == SNAME("light_projector_filter") || p_name == SNAME("decal_filter") || p_name == SNAME("canvas_shadow_atlas_size");
}

Viewport *TextureSettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

Variant TextureSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

Variant TextureSettingsServer::_get_effective_value(const StringName &p_name, const Variant &p_value) {
	// What reaches the engine: the value, or nothing (Variant()) if the renderer ignores it.
	if (is_renderer_supported(p_name)) {
		warned.erase(p_name);
		return p_value;
	}
	if (!warned.has(p_name) && !values_equal(p_value, _get_project_value(p_name))) {
		// The player's (or the game's) choice, not the project value: worth a warning, once.
		warned.insert(p_name);
		const String method = _get_rendering_method();
		WARN_PRINT(vformat("Texture settings: \"%s\" = %s is not supported by the current renderer (\"%s\"); the value is not applied. See TextureSettingsServer.is_value_supported().", p_name, p_value, method));
	}
	return Variant();
}

void TextureSettingsServer::_apply_to_viewport(Viewport *p_viewport, const StringName &p_name, const Variant &p_value) const {
#ifndef _3D_DISABLED
	if (p_name == SNAME("anisotropic_filtering_level")) {
		p_viewport->set_anisotropic_filtering_level(Viewport::AnisotropicFiltering(int(p_value)));
	} else if (p_name == SNAME("texture_mipmap_bias")) {
		p_viewport->set_texture_mipmap_bias(float(p_value));
	} else
#endif // _3D_DISABLED
		if (p_name == SNAME("canvas_item_default_texture_filter")) {
			p_viewport->set_default_canvas_item_texture_filter(Viewport::DefaultCanvasItemTextureFilter(int(p_value)));
		} else if (p_name == SNAME("canvas_item_default_texture_repeat")) {
			p_viewport->set_default_canvas_item_texture_repeat(Viewport::DefaultCanvasItemTextureRepeat(int(p_value)));
		} else if (p_name == SNAME("snap_2d_transforms_to_pixel")) {
			p_viewport->set_snap_2d_transforms_to_pixel(bool(p_value));
		} else if (p_name == SNAME("snap_2d_vertices_to_pixel")) {
			p_viewport->set_snap_2d_vertices_to_pixel(bool(p_value));
		} else if (p_name == SNAME("sdf_oversize")) {
			p_viewport->set_sdf_oversize(Viewport::SDFOversize(int(p_value)));
		} else if (p_name == SNAME("sdf_scale")) {
			p_viewport->set_sdf_scale(Viewport::SDFScale(int(p_value)));
		}
}

void TextureSettingsServer::_apply_all_to_viewport(Viewport *p_viewport) {
	for (const char *setting_name : texture_setting_names) {
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

void TextureSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	const Variant effective = _get_effective_value(p_name, p_value);
	if (effective.get_type() == Variant::NIL) {
		return;
	}

	if (_is_global_setting(p_name)) {
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server == nullptr) {
			return; // Delivered when it exists (undelivered).
		}
		if (p_name == SNAME("light_projector_filter")) {
			rendering_server->light_projectors_set_filter(RenderingServerEnums::LightProjectorFilter(int(effective)));
		} else if (p_name == SNAME("decal_filter")) {
			rendering_server->decals_set_filter(RenderingServerEnums::DecalFilter(int(effective)));
		} else {
			rendering_server->canvas_set_shadow_texture_size(int(effective));
		}
		return;
	}

	for (Viewport *viewport : _get_target_viewports(viewport_target, get_viewport_group(), extra_viewports)) {
		_apply_to_viewport(viewport, p_name, effective);
	}
}

void TextureSettingsServer::_deliver_undelivered() {
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

void TextureSettingsServer::_push_texture_value(const Variant &p_value, const StringName &p_name) {
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

Variant TextureSettingsServer::_pull_texture_value(const StringName &p_name) {
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
#ifndef _3D_DISABLED
	if (p_name == SNAME("anisotropic_filtering_level")) {
		return int(root->get_anisotropic_filtering_level());
	} else if (p_name == SNAME("texture_mipmap_bias")) {
		return root->get_texture_mipmap_bias();
	} else
#endif // _3D_DISABLED
		if (p_name == SNAME("canvas_item_default_texture_filter")) {
			return int(root->get_default_canvas_item_texture_filter());
		} else if (p_name == SNAME("canvas_item_default_texture_repeat")) {
			return int(root->get_default_canvas_item_texture_repeat());
		} else if (p_name == SNAME("snap_2d_transforms_to_pixel")) {
			return root->is_snap_2d_transforms_to_pixel_enabled();
		} else if (p_name == SNAME("snap_2d_vertices_to_pixel")) {
			return root->is_snap_2d_vertices_to_pixel_enabled();
		} else if (p_name == SNAME("sdf_oversize")) {
			return int(root->get_sdf_oversize());
		} else if (p_name == SNAME("sdf_scale")) {
			return int(root->get_sdf_scale());
		}
	return _get_current(p_name);
}

void TextureSettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &TextureSettingsServer::_on_node_added);
	if (!tree->is_connected(SNAME("node_added"), callback)) {
		tree->connect(SNAME("node_added"), callback);
	}
}

void TextureSettingsServer::_on_node_added(Node *p_node) {
	Viewport *viewport = Object::cast_to<Viewport>(p_node);
	if (viewport == nullptr || viewport == _get_root() || has_viewport(viewport) || !_is_viewport_targeted(viewport_target, get_viewport_group(), viewport)) {
		return;
	}
	_apply_all_to_viewport(viewport);
}

void TextureSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// After main.cpp and SceneTree have set the project values of the root viewport.
		_deliver_undelivered();
		_connect_tree();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName TextureSettingsServer::get_domain() const {
	return SNAME("texture");
}

String TextureSettingsServer::_get_legacy_file_name() const {
	return "video_texture";
}

SettingsServer::ApplyPhase TextureSettingsServer::get_apply_phase() const {
	// The root viewport exists and has its project values; the main scene is not loaded yet.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> TextureSettingsServer::_create_default_settings() const {
	Ref<TextureSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void TextureSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant TextureSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}

	const int enum_size = texture_enum_size(p_name);
	if (enum_size > 0) {
		const double number = value;
		if (Math::is_nan(number) || number < 0 || number >= enum_size) {
			const Variant default_value = get_default_value(p_name);
			WARN_PRINT(vformat("Texture settings: %s is not a valid value for \"%s\"; %s is used.", value, p_name, default_value));
			return default_value;
		}
		return int64_t(number);
	}

	double min_value = 0.0;
	double max_value = 0.0;
	bool integer = false;
	if (texture_range(p_name, min_value, max_value, integer)) {
		const double number = value;
		if (Math::is_nan(number)) {
			WARN_PRINT(vformat("Texture settings: \"%s\" is not a number; the default is used.", p_name));
			return get_default_value(p_name);
		}
		const double clamped = CLAMP(number, min_value, max_value);
		if (clamped != number) {
			WARN_PRINT(vformat("Texture settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(min_value), Variant(max_value), Variant(clamped)));
		}
		return integer ? Variant(int64_t(Math::round(clamped))) : Variant(clamped);
	}
	return value;
}

bool TextureSettingsServer::is_value_supported(const StringName &p_name) const {
	return SettingsServer::is_value_supported(p_name) && is_renderer_supported(p_name);
}

PackedStringArray TextureSettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : texture_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Further viewports.

void TextureSettingsServer::add_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	if (has_viewport(p_viewport)) {
		return;
	}
	extra_viewports.push_back(p_viewport->get_instance_id());
	_apply_all_to_viewport(p_viewport);
}

void TextureSettingsServer::remove_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	extra_viewports.erase(p_viewport->get_instance_id());
}

bool TextureSettingsServer::has_viewport(Viewport *p_viewport) const {
	return p_viewport != nullptr && extra_viewports.has(p_viewport->get_instance_id());
}

void TextureSettingsServer::set_viewport_target(ViewportTargetMode p_target) {
	ERR_FAIL_INDEX(int(p_target), int(VIEWPORT_TARGET_ALL_VIEWPORTS) + 1);
	viewport_target = p_target;
	// The viewports that are targets now get the current values; the root has them already.
	const Viewport *root = _get_root();
	for (Viewport *viewport : _get_target_viewports(viewport_target, get_viewport_group(), extra_viewports)) {
		if (viewport != root) {
			_apply_all_to_viewport(viewport);
		}
	}
	_connect_tree();
}

SettingsServer::ViewportTargetMode TextureSettingsServer::get_viewport_target() const {
	return viewport_target;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void TextureSettingsServer::set_anisotropic_filtering_level(Viewport::AnisotropicFiltering p_level) {
	set_value(SNAME("anisotropic_filtering_level"), int(p_level));
}

Viewport::AnisotropicFiltering TextureSettingsServer::get_anisotropic_filtering_level() const {
	return Viewport::AnisotropicFiltering(int(get_value(SNAME("anisotropic_filtering_level"))));
}

void TextureSettingsServer::set_texture_mipmap_bias(float p_bias) {
	set_value(SNAME("texture_mipmap_bias"), p_bias);
}

float TextureSettingsServer::get_texture_mipmap_bias() const {
	return get_value(SNAME("texture_mipmap_bias"));
}

void TextureSettingsServer::set_light_projector_filter(RenderingServerEnums::LightProjectorFilter p_filter) {
	set_value(SNAME("light_projector_filter"), int(p_filter));
}

RenderingServerEnums::LightProjectorFilter TextureSettingsServer::get_light_projector_filter() const {
	return RenderingServerEnums::LightProjectorFilter(int(get_value(SNAME("light_projector_filter"))));
}

void TextureSettingsServer::set_decal_filter(RenderingServerEnums::DecalFilter p_filter) {
	set_value(SNAME("decal_filter"), int(p_filter));
}

RenderingServerEnums::DecalFilter TextureSettingsServer::get_decal_filter() const {
	return RenderingServerEnums::DecalFilter(int(get_value(SNAME("decal_filter"))));
}

void TextureSettingsServer::set_canvas_item_default_texture_filter(Viewport::DefaultCanvasItemTextureFilter p_filter) {
	set_value(SNAME("canvas_item_default_texture_filter"), int(p_filter));
}

Viewport::DefaultCanvasItemTextureFilter TextureSettingsServer::get_canvas_item_default_texture_filter() const {
	return Viewport::DefaultCanvasItemTextureFilter(int(get_value(SNAME("canvas_item_default_texture_filter"))));
}

void TextureSettingsServer::set_canvas_item_default_texture_repeat(Viewport::DefaultCanvasItemTextureRepeat p_repeat) {
	set_value(SNAME("canvas_item_default_texture_repeat"), int(p_repeat));
}

Viewport::DefaultCanvasItemTextureRepeat TextureSettingsServer::get_canvas_item_default_texture_repeat() const {
	return Viewport::DefaultCanvasItemTextureRepeat(int(get_value(SNAME("canvas_item_default_texture_repeat"))));
}

void TextureSettingsServer::set_snap_2d_transforms_to_pixel(bool p_enabled) {
	set_value(SNAME("snap_2d_transforms_to_pixel"), p_enabled);
}

bool TextureSettingsServer::is_snap_2d_transforms_to_pixel_enabled() const {
	return get_value(SNAME("snap_2d_transforms_to_pixel"));
}

void TextureSettingsServer::set_snap_2d_vertices_to_pixel(bool p_enabled) {
	set_value(SNAME("snap_2d_vertices_to_pixel"), p_enabled);
}

bool TextureSettingsServer::is_snap_2d_vertices_to_pixel_enabled() const {
	return get_value(SNAME("snap_2d_vertices_to_pixel"));
}

void TextureSettingsServer::set_sdf_oversize(Viewport::SDFOversize p_oversize) {
	set_value(SNAME("sdf_oversize"), int(p_oversize));
}

Viewport::SDFOversize TextureSettingsServer::get_sdf_oversize() const {
	return Viewport::SDFOversize(int(get_value(SNAME("sdf_oversize"))));
}

void TextureSettingsServer::set_sdf_scale(Viewport::SDFScale p_scale) {
	set_value(SNAME("sdf_scale"), int(p_scale));
}

Viewport::SDFScale TextureSettingsServer::get_sdf_scale() const {
	return Viewport::SDFScale(int(get_value(SNAME("sdf_scale"))));
}

void TextureSettingsServer::set_canvas_shadow_atlas_size(int p_size) {
	set_value(SNAME("canvas_shadow_atlas_size"), p_size);
}

int TextureSettingsServer::get_canvas_shadow_atlas_size() const {
	return get_value(SNAME("canvas_shadow_atlas_size"));
}

void TextureSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"anisotropic_filtering_level",
		"texture_mipmap_bias",
		"canvas_item_default_texture_filter",
		"canvas_item_default_texture_repeat",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void TextureSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("anisotropic_filtering_level_changed", PropertyInfo(Variant::INT, "level", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Viewport.AnisotropicFiltering")));
	ADD_SIGNAL(MethodInfo("texture_mipmap_bias_changed", PropertyInfo(Variant::FLOAT, "bias")));
	ADD_SIGNAL(MethodInfo("canvas_item_default_texture_filter_changed", PropertyInfo(Variant::INT, "filter", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Viewport.DefaultCanvasItemTextureFilter")));
	ADD_SIGNAL(MethodInfo("canvas_item_default_texture_repeat_changed", PropertyInfo(Variant::INT, "repeat", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "Viewport.DefaultCanvasItemTextureRepeat")));

	ClassDB::bind_method(D_METHOD("set_anisotropic_filtering_level", "level"), &TextureSettingsServer::set_anisotropic_filtering_level);
	ClassDB::bind_method(D_METHOD("get_anisotropic_filtering_level"), &TextureSettingsServer::get_anisotropic_filtering_level);
	ClassDB::bind_method(D_METHOD("set_texture_mipmap_bias", "bias"), &TextureSettingsServer::set_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("get_texture_mipmap_bias"), &TextureSettingsServer::get_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("set_light_projector_filter", "filter"), &TextureSettingsServer::set_light_projector_filter);
	ClassDB::bind_method(D_METHOD("get_light_projector_filter"), &TextureSettingsServer::get_light_projector_filter);
	ClassDB::bind_method(D_METHOD("set_decal_filter", "filter"), &TextureSettingsServer::set_decal_filter);
	ClassDB::bind_method(D_METHOD("get_decal_filter"), &TextureSettingsServer::get_decal_filter);
	ClassDB::bind_method(D_METHOD("set_canvas_item_default_texture_filter", "filter"), &TextureSettingsServer::set_canvas_item_default_texture_filter);
	ClassDB::bind_method(D_METHOD("get_canvas_item_default_texture_filter"), &TextureSettingsServer::get_canvas_item_default_texture_filter);
	ClassDB::bind_method(D_METHOD("set_canvas_item_default_texture_repeat", "repeat"), &TextureSettingsServer::set_canvas_item_default_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_canvas_item_default_texture_repeat"), &TextureSettingsServer::get_canvas_item_default_texture_repeat);
	ClassDB::bind_method(D_METHOD("set_snap_2d_transforms_to_pixel", "enabled"), &TextureSettingsServer::set_snap_2d_transforms_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_transforms_to_pixel_enabled"), &TextureSettingsServer::is_snap_2d_transforms_to_pixel_enabled);
	ClassDB::bind_method(D_METHOD("set_snap_2d_vertices_to_pixel", "enabled"), &TextureSettingsServer::set_snap_2d_vertices_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_vertices_to_pixel_enabled"), &TextureSettingsServer::is_snap_2d_vertices_to_pixel_enabled);
	ClassDB::bind_method(D_METHOD("set_sdf_oversize", "oversize"), &TextureSettingsServer::set_sdf_oversize);
	ClassDB::bind_method(D_METHOD("get_sdf_oversize"), &TextureSettingsServer::get_sdf_oversize);
	ClassDB::bind_method(D_METHOD("set_sdf_scale", "scale"), &TextureSettingsServer::set_sdf_scale);
	ClassDB::bind_method(D_METHOD("get_sdf_scale"), &TextureSettingsServer::get_sdf_scale);
	ClassDB::bind_method(D_METHOD("set_canvas_shadow_atlas_size", "size"), &TextureSettingsServer::set_canvas_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_canvas_shadow_atlas_size"), &TextureSettingsServer::get_canvas_shadow_atlas_size);

	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &TextureSettingsServer::get_unsupported_setting_names);
	ClassDB::bind_method(D_METHOD("add_viewport", "viewport"), &TextureSettingsServer::add_viewport);
	ClassDB::bind_method(D_METHOD("remove_viewport", "viewport"), &TextureSettingsServer::remove_viewport);
	ClassDB::bind_method(D_METHOD("has_viewport", "viewport"), &TextureSettingsServer::has_viewport);
	ClassDB::bind_method(D_METHOD("set_viewport_target", "target"), &TextureSettingsServer::set_viewport_target);
	ClassDB::bind_method(D_METHOD("get_viewport_target"), &TextureSettingsServer::get_viewport_target);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "viewport_target", PROPERTY_HINT_ENUM, "Root Only,Group,All Viewports"), "set_viewport_target", "get_viewport_target");

	ClassDB::bind_static_method("TextureSettingsServer", D_METHOD("get_viewport_group"), &TextureSettingsServer::get_viewport_group);
	ClassDB::bind_static_method("TextureSettingsServer", D_METHOD("is_renderer_supported", "name"), &TextureSettingsServer::is_renderer_supported);
}

TextureSettingsServer::TextureSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : texture_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &TextureSettingsServer::_push_texture_value).bind(bound_name), callable_mp(this, &TextureSettingsServer::_pull_texture_value).bind(bound_name));
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &TextureSettingsServer::_on_startup_applied));
	}
}

TextureSettingsServer::~TextureSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
