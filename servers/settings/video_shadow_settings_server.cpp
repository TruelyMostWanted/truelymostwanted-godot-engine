/**************************************************************************/
/*  video_shadow_settings_server.cpp                                      */
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

#include "video_shadow_settings_server.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/math/math_funcs_binary.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_shadow_settings.h"

// The values of the domain, in the order of VideoShadowSettings.
static const char *video_shadow_setting_names[] = {
	"directional_shadow_atlas_size",
	"directional_shadow_16_bits",
	"directional_soft_shadow_filter_quality",
	"positional_shadow_atlas_size",
	"positional_shadow_atlas_16_bits",
	"positional_shadow_atlas_quad_0",
	"positional_shadow_atlas_quad_1",
	"positional_shadow_atlas_quad_2",
	"positional_shadow_atlas_quad_3",
	"positional_soft_shadow_filter_quality",
};

static constexpr const char *VIDEO_SHADOW_VIEWPORT_GROUP = "video_shadow_settings";

// The range of the project settings `rendering/lights_and_shadows/*_shadow/*size`.
static constexpr int VIDEO_SHADOW_ATLAS_SIZE_MIN = 256;
static constexpr int VIDEO_SHADOW_ATLAS_SIZE_MAX = 16384;

// The project settings `rendering/lights_and_shadows/positional_shadow/atlas_quadrant_N_subdiv` (defaults of SceneTree).
static const char *video_shadow_quadrant_project_settings[4] = {
	"rendering/lights_and_shadows/positional_shadow/atlas_quadrant_0_subdiv",
	"rendering/lights_and_shadows/positional_shadow/atlas_quadrant_1_subdiv",
	"rendering/lights_and_shadows/positional_shadow/atlas_quadrant_2_subdiv",
	"rendering/lights_and_shadows/positional_shadow/atlas_quadrant_3_subdiv",
};
static constexpr int video_shadow_quadrant_defaults[4] = { 2, 2, 3, 4 };

static Variant video_shadow_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	// With the feature overrides (`.mobile`, `.web`, ...), like SceneTree and the renderers read them.
	return project_settings->get_setting_with_override(p_name);
}

static bool video_shadow_is_quality(const StringName &p_name) {
	return p_name == SNAME("directional_soft_shadow_filter_quality") || p_name == SNAME("positional_soft_shadow_filter_quality");
}

static bool video_shadow_is_atlas_size(const StringName &p_name) {
	return p_name == SNAME("directional_shadow_atlas_size") || p_name == SNAME("positional_shadow_atlas_size");
}

VideoShadowSettingsServer *VideoShadowSettingsServer::singleton = nullptr;

VideoShadowSettingsServer *VideoShadowSettingsServer::get_singleton() {
	return singleton;
}

StringName VideoShadowSettingsServer::get_viewport_group() {
	return VIDEO_SHADOW_VIEWPORT_GROUP;
}

/////////////////////////////////////////////////////////////////////////////
// Renderer support.

bool VideoShadowSettingsServer::is_renderer_supported(const StringName &p_name) {
	// Forward+, Mobile and Compatibility use every value. Compatibility has only three filter levels
	// (Hard and Soft Very Low: hard, Soft Low and Soft Medium: PCF 5, Soft High and Soft Ultra: PCF 13);
	// the value is still applied. Kept for the same API as the other video domains.
	return !p_name.is_empty();
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

int VideoShadowSettingsServer::_get_quadrant(const StringName &p_name) {
	for (int i = 0; i < 4; i++) {
		if (p_name == StringName(video_shadow_setting_names[5 + i])) {
			return i;
		}
	}
	return -1;
}

Variant VideoShadowSettingsServer::_get_project_value(const StringName &p_name) {
	// What SceneTree and the renderers read from the project settings.
	if (p_name == SNAME("directional_shadow_atlas_size")) {
		return int(video_shadow_project_setting("rendering/lights_and_shadows/directional_shadow/size", 4096));
	} else if (p_name == SNAME("directional_shadow_16_bits")) {
		return bool(video_shadow_project_setting("rendering/lights_and_shadows/directional_shadow/16_bits", true));
	} else if (p_name == SNAME("directional_soft_shadow_filter_quality")) {
		return int(video_shadow_project_setting("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality", int(RenderingServerEnums::SHADOW_QUALITY_SOFT_LOW)));
	} else if (p_name == SNAME("positional_shadow_atlas_size")) {
		return int(video_shadow_project_setting("rendering/lights_and_shadows/positional_shadow/atlas_size", 4096));
	} else if (p_name == SNAME("positional_shadow_atlas_16_bits")) {
		return bool(video_shadow_project_setting("rendering/lights_and_shadows/positional_shadow/atlas_16_bits", true));
	} else if (p_name == SNAME("positional_soft_shadow_filter_quality")) {
		return int(video_shadow_project_setting("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality", int(RenderingServerEnums::SHADOW_QUALITY_SOFT_LOW)));
	}
	const int quadrant = _get_quadrant(p_name);
	if (quadrant >= 0) {
		return int(video_shadow_project_setting(video_shadow_quadrant_project_settings[quadrant], video_shadow_quadrant_defaults[quadrant]));
	}
	return Variant();
}

bool VideoShadowSettingsServer::_is_global_setting(const StringName &p_name) {
	return p_name == SNAME("directional_shadow_atlas_size") || p_name == SNAME("directional_shadow_16_bits") || video_shadow_is_quality(p_name);
}

Viewport *VideoShadowSettingsServer::_get_root() {
	SceneTree *tree = SceneTree::get_singleton();
	return tree != nullptr ? tree->get_root().ptr() : nullptr;
}

Variant VideoShadowSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

void VideoShadowSettingsServer::_apply_to_viewport(Viewport *p_viewport, const StringName &p_name, const Variant &p_value) const {
	if (p_name == SNAME("positional_shadow_atlas_size")) {
		p_viewport->set_positional_shadow_atlas_size(int(p_value));
	} else if (p_name == SNAME("positional_shadow_atlas_16_bits")) {
		p_viewport->set_positional_shadow_atlas_16_bits(bool(p_value));
	} else {
		const int quadrant = _get_quadrant(p_name);
		if (quadrant >= 0) {
			p_viewport->set_positional_shadow_atlas_quadrant_subdiv(quadrant, Viewport::PositionalShadowAtlasQuadrantSubdiv(int(p_value)));
		}
	}
}

void VideoShadowSettingsServer::_apply_all_to_viewport(Viewport *p_viewport) {
	for (const char *setting_name : video_shadow_setting_names) {
		const StringName name_key = setting_name;
		if (!_is_global_setting(name_key) && is_renderer_supported(name_key)) {
			_apply_to_viewport(p_viewport, name_key, _get_current(name_key));
		}
	}
}

void VideoShadowSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	if (!is_renderer_supported(p_name)) {
		return;
	}

	if (_is_global_setting(p_name)) {
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server == nullptr) {
			return; // Delivered when it exists (undelivered).
		}
		if (p_name == SNAME("directional_soft_shadow_filter_quality")) {
			rendering_server->directional_soft_shadow_filter_set_quality(RenderingServerEnums::ShadowQuality(int(p_value)));
		} else if (p_name == SNAME("positional_soft_shadow_filter_quality")) {
			rendering_server->positional_soft_shadow_filter_set_quality(RenderingServerEnums::ShadowQuality(int(p_value)));
		} else {
			// One call for size and bit depth. The renderers skip it if nothing changed (no reallocation).
			const int size = p_name == SNAME("directional_shadow_atlas_size") ? int(p_value) : int(_get_current(SNAME("directional_shadow_atlas_size")));
			const bool use_16_bits = p_name == SNAME("directional_shadow_16_bits") ? bool(p_value) : bool(_get_current(SNAME("directional_shadow_16_bits")));
			rendering_server->directional_shadow_atlas_set_size(size, use_16_bits);
		}
		return;
	}

	Viewport *root = _get_root();
	if (root != nullptr) {
		_apply_to_viewport(root, p_name, p_value);
	}
	for (uint32_t i = 0; i < extra_viewports.size();) {
		Viewport *viewport = ObjectDB::get_instance<Viewport>(extra_viewports[i]);
		if (viewport == nullptr) {
			extra_viewports.remove_at_unordered(i); // Freed without remove_viewport().
			continue;
		}
		_apply_to_viewport(viewport, p_name, p_value);
		i++;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree != nullptr) {
		for (Node *node : tree->get_nodes_in_group(get_viewport_group())) {
			Viewport *viewport = Object::cast_to<Viewport>(node);
			if (viewport != nullptr && viewport != root && !has_viewport(viewport)) {
				_apply_to_viewport(viewport, p_name, p_value);
			}
		}
	}
}

void VideoShadowSettingsServer::_deliver_undelivered() {
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

void VideoShadowSettingsServer::_push_shadow_value(const Variant &p_value, const StringName &p_name) {
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

Variant VideoShadowSettingsServer::_pull_shadow_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the engine state (which a game may have changed already).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	const Viewport *root = _get_root();
	if (root == nullptr || _is_global_setting(p_name) || undelivered.has(p_name)) {
		// RenderingServer has no getters for the global values: they stay as pushed.
		return _get_current(p_name);
	}
	if (p_name == SNAME("positional_shadow_atlas_size")) {
		return root->get_positional_shadow_atlas_size();
	} else if (p_name == SNAME("positional_shadow_atlas_16_bits")) {
		return root->get_positional_shadow_atlas_16_bits();
	}
	const int quadrant = _get_quadrant(p_name);
	if (quadrant >= 0) {
		return int(root->get_positional_shadow_atlas_quadrant_subdiv(quadrant));
	}
	return _get_current(p_name);
}

void VideoShadowSettingsServer::_connect_tree() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	const Callable callback = callable_mp(this, &VideoShadowSettingsServer::_on_node_added);
	if (!tree->is_connected(SNAME("node_added"), callback)) {
		tree->connect(SNAME("node_added"), callback);
	}
}

void VideoShadowSettingsServer::_on_node_added(Node *p_node) {
	Viewport *viewport = Object::cast_to<Viewport>(p_node);
	if (viewport == nullptr || viewport == _get_root() || has_viewport(viewport) || !viewport->is_in_group(get_viewport_group())) {
		return;
	}
	_apply_all_to_viewport(viewport);
}

void VideoShadowSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// After SceneTree has set the project values of the root viewport.
		_deliver_undelivered();
		_connect_tree();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName VideoShadowSettingsServer::get_domain() const {
	return SNAME("video_shadow");
}

SettingsServer::ApplyPhase VideoShadowSettingsServer::get_apply_phase() const {
	// The root viewport exists and has its project values; the main scene is not loaded yet.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> VideoShadowSettingsServer::_create_default_settings() const {
	Ref<VideoShadowSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void VideoShadowSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant VideoShadowSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) {
		return value;
	}
	const double number = value;

	int enum_size = 0;
	if (video_shadow_is_quality(p_name)) {
		enum_size = RenderingServerEnums::SHADOW_QUALITY_MAX;
	} else if (_get_quadrant(p_name) >= 0) {
		enum_size = Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_MAX;
	}
	if (enum_size > 0) {
		if (Math::is_nan(number) || number < 0 || number >= enum_size) {
			const Variant default_value = get_default_value(p_name);
			WARN_PRINT(vformat("Video shadow settings: %s is not a valid value for \"%s\"; %s is used.", value, p_name, default_value));
			return default_value;
		}
		return int64_t(number);
	}

	if (video_shadow_is_atlas_size(p_name)) {
		if (Math::is_nan(number)) {
			WARN_PRINT(vformat("Video shadow settings: \"%s\" is not a number; the default is used.", p_name));
			return get_default_value(p_name);
		}
		if (p_name == SNAME("positional_shadow_atlas_size") && number <= 0.0) {
			if (number < 0.0) {
				WARN_PRINT(vformat("Video shadow settings: %s is out of range for \"%s\" (0, or %d to %d); 0 is used.", value, p_name, VIDEO_SHADOW_ATLAS_SIZE_MIN, VIDEO_SHADOW_ATLAS_SIZE_MAX));
			}
			return int64_t(0); // No positional shadows at all.
		}
		const double clamped = CLAMP(number, double(VIDEO_SHADOW_ATLAS_SIZE_MIN), double(VIDEO_SHADOW_ATLAS_SIZE_MAX));
		if (clamped != number) {
			WARN_PRINT(vformat("Video shadow settings: %s is out of range for \"%s\" (%d to %d); %s is used.", value, p_name, VIDEO_SHADOW_ATLAS_SIZE_MIN, VIDEO_SHADOW_ATLAS_SIZE_MAX, Variant(clamped)));
		}
		// Rounded up to a power of two, like the renderers do: the stored value is the size the atlas gets.
		return int64_t(Math::next_power_of_2(uint32_t(Math::round(clamped))));
	}
	return value;
}

bool VideoShadowSettingsServer::is_value_supported(const StringName &p_name) const {
	return SettingsServer::is_value_supported(p_name) && is_renderer_supported(p_name);
}

PackedStringArray VideoShadowSettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : video_shadow_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Further viewports.

void VideoShadowSettingsServer::add_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	if (has_viewport(p_viewport)) {
		return;
	}
	extra_viewports.push_back(p_viewport->get_instance_id());
	_apply_all_to_viewport(p_viewport);
}

void VideoShadowSettingsServer::remove_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	extra_viewports.erase(p_viewport->get_instance_id());
}

bool VideoShadowSettingsServer::has_viewport(Viewport *p_viewport) const {
	return p_viewport != nullptr && extra_viewports.has(p_viewport->get_instance_id());
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void VideoShadowSettingsServer::set_directional_shadow_atlas_size(int p_size) {
	set_value(SNAME("directional_shadow_atlas_size"), p_size);
}

int VideoShadowSettingsServer::get_directional_shadow_atlas_size() const {
	return get_value(SNAME("directional_shadow_atlas_size"));
}

void VideoShadowSettingsServer::set_directional_shadow_16_bits(bool p_enabled) {
	set_value(SNAME("directional_shadow_16_bits"), p_enabled);
}

bool VideoShadowSettingsServer::is_directional_shadow_16_bits_enabled() const {
	return get_value(SNAME("directional_shadow_16_bits"));
}

void VideoShadowSettingsServer::set_directional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality) {
	set_value(SNAME("directional_soft_shadow_filter_quality"), int(p_quality));
}

RenderingServerEnums::ShadowQuality VideoShadowSettingsServer::get_directional_soft_shadow_filter_quality() const {
	return RenderingServerEnums::ShadowQuality(int(get_value(SNAME("directional_soft_shadow_filter_quality"))));
}

void VideoShadowSettingsServer::set_positional_shadow_atlas_size(int p_size) {
	set_value(SNAME("positional_shadow_atlas_size"), p_size);
}

int VideoShadowSettingsServer::get_positional_shadow_atlas_size() const {
	return get_value(SNAME("positional_shadow_atlas_size"));
}

void VideoShadowSettingsServer::set_positional_shadow_atlas_16_bits(bool p_enabled) {
	set_value(SNAME("positional_shadow_atlas_16_bits"), p_enabled);
}

bool VideoShadowSettingsServer::is_positional_shadow_atlas_16_bits_enabled() const {
	return get_value(SNAME("positional_shadow_atlas_16_bits"));
}

void VideoShadowSettingsServer::set_positional_shadow_atlas_quadrant_subdiv(int p_quadrant, Viewport::PositionalShadowAtlasQuadrantSubdiv p_subdiv) {
	ERR_FAIL_INDEX(p_quadrant, 4);
	set_value(StringName(video_shadow_setting_names[5 + p_quadrant]), int(p_subdiv));
}

Viewport::PositionalShadowAtlasQuadrantSubdiv VideoShadowSettingsServer::get_positional_shadow_atlas_quadrant_subdiv(int p_quadrant) const {
	ERR_FAIL_INDEX_V(p_quadrant, 4, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	return Viewport::PositionalShadowAtlasQuadrantSubdiv(int(get_value(StringName(video_shadow_setting_names[5 + p_quadrant]))));
}

void VideoShadowSettingsServer::set_positional_soft_shadow_filter_quality(RenderingServerEnums::ShadowQuality p_quality) {
	set_value(SNAME("positional_soft_shadow_filter_quality"), int(p_quality));
}

RenderingServerEnums::ShadowQuality VideoShadowSettingsServer::get_positional_soft_shadow_filter_quality() const {
	return RenderingServerEnums::ShadowQuality(int(get_value(SNAME("positional_soft_shadow_filter_quality"))));
}

void VideoShadowSettingsServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_directional_shadow_atlas_size", "size"), &VideoShadowSettingsServer::set_directional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_directional_shadow_atlas_size"), &VideoShadowSettingsServer::get_directional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("set_directional_shadow_16_bits", "enabled"), &VideoShadowSettingsServer::set_directional_shadow_16_bits);
	ClassDB::bind_method(D_METHOD("is_directional_shadow_16_bits_enabled"), &VideoShadowSettingsServer::is_directional_shadow_16_bits_enabled);
	ClassDB::bind_method(D_METHOD("set_directional_soft_shadow_filter_quality", "quality"), &VideoShadowSettingsServer::set_directional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("get_directional_soft_shadow_filter_quality"), &VideoShadowSettingsServer::get_directional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_size", "size"), &VideoShadowSettingsServer::set_positional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_size"), &VideoShadowSettingsServer::get_positional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_16_bits", "enabled"), &VideoShadowSettingsServer::set_positional_shadow_atlas_16_bits);
	ClassDB::bind_method(D_METHOD("is_positional_shadow_atlas_16_bits_enabled"), &VideoShadowSettingsServer::is_positional_shadow_atlas_16_bits_enabled);
	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_quadrant_subdiv", "quadrant", "subdiv"), &VideoShadowSettingsServer::set_positional_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_quadrant_subdiv", "quadrant"), &VideoShadowSettingsServer::get_positional_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("set_positional_soft_shadow_filter_quality", "quality"), &VideoShadowSettingsServer::set_positional_soft_shadow_filter_quality);
	ClassDB::bind_method(D_METHOD("get_positional_soft_shadow_filter_quality"), &VideoShadowSettingsServer::get_positional_soft_shadow_filter_quality);

	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &VideoShadowSettingsServer::get_unsupported_setting_names);
	ClassDB::bind_method(D_METHOD("add_viewport", "viewport"), &VideoShadowSettingsServer::add_viewport);
	ClassDB::bind_method(D_METHOD("remove_viewport", "viewport"), &VideoShadowSettingsServer::remove_viewport);
	ClassDB::bind_method(D_METHOD("has_viewport", "viewport"), &VideoShadowSettingsServer::has_viewport);

	ClassDB::bind_static_method("VideoShadowSettingsServer", D_METHOD("get_viewport_group"), &VideoShadowSettingsServer::get_viewport_group);
	ClassDB::bind_static_method("VideoShadowSettingsServer", D_METHOD("is_renderer_supported", "name"), &VideoShadowSettingsServer::is_renderer_supported);
}

VideoShadowSettingsServer::VideoShadowSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : video_shadow_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &VideoShadowSettingsServer::_push_shadow_value).bind(bound_name), callable_mp(this, &VideoShadowSettingsServer::_pull_shadow_value).bind(bound_name));
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &VideoShadowSettingsServer::_on_startup_applied));
	}
}

VideoShadowSettingsServer::~VideoShadowSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
