/**************************************************************************/
/*  xr_settings_server.cpp                                                */
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

#include "xr_settings_server.h"

#if defined(MODULE_OPENXR_ENABLED) && !defined(XR_DISABLED)

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/xr/xr_interface.h"
#include "servers/xr/xr_server.h"

// The values of the domain pushed to the interface, in the order of XRSettings.
static const char *xr_setting_names[] = {
	"foveation_level",
	"foveation_dynamic",
	"foveation_with_subsampled_images",
	"render_target_size_multiplier",
	"display_refresh_rate",
	"vrs_min_radius",
	"vrs_strength",
	"cpu_level",
	"gpu_level",
	"left_hand_motion_range",
	"right_hand_motion_range",
};

// Stored for the game only.
static const char *xr_game_setting_names[] = {
	"hand_tracking_enabled",
};

static Variant xr_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_name);
}

XRSettingsServer *XRSettingsServer::singleton = nullptr;

XRSettingsServer *XRSettingsServer::get_singleton() {
	return singleton;
}

StringName XRSettingsServer::get_interface_name() {
	return SNAME("OpenXR");
}

/////////////////////////////////////////////////////////////////////////////
// Interface.

Object *XRSettingsServer::get_interface() const {
	if (has_interface_override) {
		return ObjectDB::get_instance(interface_override);
	}
	const XRServer *xr_server = XRServer::get_singleton();
	if (xr_server == nullptr) {
		return nullptr;
	}
	// XRServer keeps a reference, so the pointer stays valid after the Ref goes out of scope.
	const Ref<XRInterface> xr_interface = xr_server->find_interface(get_interface_name());
	return xr_interface.ptr();
}

void XRSettingsServer::set_interface_override(Object *p_interface) {
	if (p_interface == nullptr) {
		clear_interface_override();
		return;
	}
	interface_override = p_interface->get_instance_id();
	has_interface_override = true;
	session_running = false;
	_connect_signals();
	_deliver_all();
}

void XRSettingsServer::clear_interface_override() {
	interface_override = ObjectID();
	has_interface_override = false;
	session_running = false;
	_connect_signals();
	_deliver_all();
}

bool XRSettingsServer::_is_interface_initialized(Object *p_interface) {
	if (p_interface == nullptr || !p_interface->has_method(SNAME("is_initialized"))) {
		return false;
	}
	return bool(p_interface->call(SNAME("is_initialized")));
}

bool XRSettingsServer::is_interface_ready() const {
	return _is_interface_initialized(get_interface());
}

bool XRSettingsServer::is_session_running() const {
	return session_running && is_interface_ready();
}

bool XRSettingsServer::_is_supported_by(Object *p_interface, const StringName &p_name) {
	if (p_name == SNAME("foveation_level") || p_name == SNAME("foveation_dynamic") || p_name == SNAME("foveation_with_subsampled_images")) {
		// XR_FB_foveation (Meta Quest, Pico); reported after the interface is initialized.
		return p_interface->has_method(SNAME("is_foveation_supported")) && bool(p_interface->call(SNAME("is_foveation_supported")));
	} else if (p_name == SNAME("display_refresh_rate")) {
		// XR_FB_display_refresh_rate: the runtime lists its rates.
		if (!p_interface->has_method(SNAME("get_available_display_refresh_rates"))) {
			return false;
		}
		const Array rates = p_interface->call(SNAME("get_available_display_refresh_rates"));
		return !rates.is_empty();
	} else if (p_name == SNAME("left_hand_motion_range") || p_name == SNAME("right_hand_motion_range") || p_name == SNAME("hand_tracking_enabled")) {
		return p_interface->has_method(SNAME("is_hand_tracking_supported")) && bool(p_interface->call(SNAME("is_hand_tracking_supported")));
	} else if (p_name == SNAME("cpu_level") || p_name == SNAME("gpu_level")) {
		// The interface does not report XR_EXT_performance_settings; runtimes without it ignore the calls.
		return p_interface->has_method(SNAME("set_cpu_level"));
	}
	// render_target_size_multiplier, vrs_min_radius, vrs_strength: every runtime.
	return true;
}

bool XRSettingsServer::is_value_supported(const StringName &p_name) const {
	if (!SettingsServer::is_value_supported(p_name)) {
		return false;
	}
	Object *xr_interface = get_interface();
	if (!_is_interface_initialized(xr_interface)) {
		// Unknown before the interface is initialized: the value is deferred, not unsupported.
		return true;
	}
	return _is_supported_by(xr_interface, p_name);
}

PackedStringArray XRSettingsServer::get_unsupported_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : xr_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	for (const char *setting_name : xr_game_setting_names) {
		if (!is_value_supported(setting_name)) {
			names.push_back(setting_name);
		}
	}
	return names;
}

PackedStringArray XRSettingsServer::get_deferred_setting_names() const {
	PackedStringArray names;
	for (const char *setting_name : xr_setting_names) {
		if (deferred.has(StringName(setting_name))) {
			names.push_back(setting_name);
		}
	}
	return names;
}

PackedFloat64Array XRSettingsServer::get_available_display_refresh_rates() const {
	PackedFloat64Array rates;
	Object *xr_interface = get_interface();
	if (!_is_interface_initialized(xr_interface) || !xr_interface->has_method(SNAME("get_available_display_refresh_rates"))) {
		return rates;
	}
	const Array available = xr_interface->call(SNAME("get_available_display_refresh_rates"));
	for (const Variant &rate : available) {
		rates.push_back(double(rate));
	}
	rates.sort();
	return rates;
}

double XRSettingsServer::_get_nearest_refresh_rate(Object *p_interface, double p_rate) {
	const Array available = p_interface->call(SNAME("get_available_display_refresh_rates"));
	double nearest = p_rate;
	double nearest_distance = -1.0;
	for (const Variant &rate : available) {
		const double distance = Math::abs(double(rate) - p_rate);
		if (nearest_distance < 0.0 || distance < nearest_distance) {
			nearest = rate;
			nearest_distance = distance;
		}
	}
	return nearest;
}

void XRSettingsServer::_connect_signals() {
	XRServer *xr_server = XRServer::get_singleton();
	if (xr_server != nullptr && !xr_server_connected) {
		xr_server->connect(SNAME("interface_added"), callable_mp(this, &XRSettingsServer::_on_interface_added));
		xr_server_connected = true;
	}

	Object *xr_interface = get_interface();
	const ObjectID interface_id = xr_interface != nullptr ? xr_interface->get_instance_id() : ObjectID();
	if (interface_id == connected_interface) {
		return;
	}
	Object *old_interface = ObjectDB::get_instance(connected_interface);
	if (old_interface != nullptr) {
		const Callable begun = callable_mp(this, &XRSettingsServer::_on_session_begun);
		const Callable stopping = callable_mp(this, &XRSettingsServer::_on_session_stopping);
		if (old_interface->is_connected(SNAME("session_begun"), begun)) {
			old_interface->disconnect(SNAME("session_begun"), begun);
		}
		if (old_interface->is_connected(SNAME("session_stopping"), stopping)) {
			old_interface->disconnect(SNAME("session_stopping"), stopping);
		}
	}
	connected_interface = interface_id;
	if (xr_interface == nullptr) {
		return;
	}
	// OpenXRInterface signals; other interfaces may not have them.
	if (xr_interface->has_signal(SNAME("session_begun"))) {
		xr_interface->connect(SNAME("session_begun"), callable_mp(this, &XRSettingsServer::_on_session_begun));
	}
	if (xr_interface->has_signal(SNAME("session_stopping"))) {
		xr_interface->connect(SNAME("session_stopping"), callable_mp(this, &XRSettingsServer::_on_session_stopping));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets.

Variant XRSettingsServer::_get_project_value(const StringName &p_name) {
	if (p_name == SNAME("foveation_level")) {
		return CLAMP(int(xr_project_setting("xr/openxr/foveation_level", 0)), int(XRSettings::FOVEATION_LEVEL_OFF), int(XRSettings::FOVEATION_LEVEL_HIGH));
	} else if (p_name == SNAME("foveation_dynamic")) {
		return bool(xr_project_setting("xr/openxr/foveation_dynamic", false));
	} else if (p_name == SNAME("foveation_with_subsampled_images")) {
		return bool(xr_project_setting("xr/openxr/foveation_with_subsampled_images", true));
	} else if (p_name == SNAME("render_target_size_multiplier")) {
		return 1.0;
	} else if (p_name == SNAME("display_refresh_rate")) {
		return 0.0;
	} else if (p_name == SNAME("vrs_min_radius")) {
		return 20.0;
	} else if (p_name == SNAME("vrs_strength")) {
		return 1.0;
	} else if (p_name == SNAME("cpu_level") || p_name == SNAME("gpu_level")) {
		return int(XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	} else if (p_name == SNAME("left_hand_motion_range") || p_name == SNAME("right_hand_motion_range")) {
		return int(XRSettings::HAND_MOTION_RANGE_UNOBSTRUCTED);
	}
	return Variant();
}

Variant XRSettingsServer::_get_current(const StringName &p_name) const {
	const Variant *value = pushed_values.getptr(p_name);
	return value != nullptr ? *value : _get_project_value(p_name);
}

bool XRSettingsServer::_deliver(const StringName &p_name, const Variant &p_value) {
	// Returns false if the interface can't take the value yet.
	Object *xr_interface = get_interface();
	if (xr_interface == nullptr) {
		return false;
	}
	if (p_name == SNAME("render_target_size_multiplier")) {
		// OpenXRInterface keeps it before initialize(); it must be set before the swapchains are created.
		xr_interface->set(p_name, double(p_value));
		return true;
	}
	if (!_is_interface_initialized(xr_interface)) {
		return false;
	}
	if (!_is_supported_by(xr_interface, p_name)) {
		if (!warned.has(p_name) && !values_equal(p_value, _get_project_value(p_name))) {
			warned.insert(p_name);
			WARN_PRINT(vformat("XR settings: \"%s\" = %s is not supported by the OpenXR runtime; the value is stored but not applied. See XRSettingsServer.get_unsupported_setting_names().", p_name, p_value));
		}
		return true;
	}
	warned.erase(p_name);

	if (p_name == SNAME("foveation_level") || p_name == SNAME("foveation_dynamic") || p_name == SNAME("foveation_with_subsampled_images") || p_name == SNAME("vrs_min_radius") || p_name == SNAME("vrs_strength")) {
		xr_interface->set(p_name, p_value);
	} else if (p_name == SNAME("display_refresh_rate")) {
		const double rate = p_value;
		if (rate > 0.0) {
			// The runtime only takes one of its own rates.
			xr_interface->set(p_name, _get_nearest_refresh_rate(xr_interface, rate));
		}
	} else if (p_name == SNAME("cpu_level") || p_name == SNAME("gpu_level")) {
		const int level = p_value;
		if (level != XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT) {
			// OpenXRInterface.PerfSettingsLevel starts at POWER_SAVINGS = 0.
			const StringName method = p_name == SNAME("cpu_level") ? SNAME("set_cpu_level") : SNAME("set_gpu_level");
			xr_interface->call(method, level - 1);
		}
	} else if (p_name == SNAME("left_hand_motion_range") || p_name == SNAME("right_hand_motion_range")) {
		// OpenXRInterface.Hand: HAND_LEFT = 0, HAND_RIGHT = 1.
		const int hand = p_name == SNAME("left_hand_motion_range") ? 0 : 1;
		xr_interface->call(SNAME("set_motion_range"), hand, int(p_value));
	}
	return true;
}

void XRSettingsServer::_deliver_all() {
	LocalVector<StringName> names;
	for (const KeyValue<StringName, Variant> &kv : pushed_values) {
		names.push_back(kv.key);
	}
	for (const StringName &pushed_name : names) {
		if (_deliver(pushed_name, pushed_values[pushed_name])) {
			deferred.erase(pushed_name);
		} else {
			deferred.insert(pushed_name);
		}
	}
}

void XRSettingsServer::_push_xr_value(const Variant &p_value, const StringName &p_name) {
	_connect_signals();
	pushed_values[p_name] = p_value;
	if (_deliver(p_name, p_value)) {
		deferred.erase(p_name);
	} else {
		// No initialized interface yet: pushed when XRServer adds it or its session begins.
		deferred.insert(p_name);
	}
}

Variant XRSettingsServer::_pull_xr_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the runtime state.
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a session value into a stored one.
		return get_applied_value(p_name);
	}
	Object *xr_interface = get_interface();
	// Only a running session reports real values (foveation reads 0 before it); some values have no getter.
	if (!session_running || deferred.has(p_name) || !_is_interface_initialized(xr_interface) || !_is_supported_by(xr_interface, p_name)) {
		return _get_current(p_name);
	}
	if (p_name == SNAME("foveation_level")) {
		return int(xr_interface->get(p_name));
	} else if (p_name == SNAME("foveation_dynamic") || p_name == SNAME("foveation_with_subsampled_images")) {
		return bool(xr_interface->get(p_name));
	} else if (p_name == SNAME("render_target_size_multiplier") || p_name == SNAME("vrs_min_radius") || p_name == SNAME("vrs_strength")) {
		return double(xr_interface->get(p_name));
	} else if ((p_name == SNAME("left_hand_motion_range") || p_name == SNAME("right_hand_motion_range")) && xr_interface->has_method(SNAME("get_motion_range"))) {
		const int hand = p_name == SNAME("left_hand_motion_range") ? 0 : 1;
		return int(xr_interface->call(SNAME("get_motion_range"), hand));
	}
	// display_refresh_rate (the runtime reports its rate, not the player's choice), cpu_level, gpu_level.
	return _get_current(p_name);
}

void XRSettingsServer::_on_interface_added(const StringName &p_interface_name) {
	if (p_interface_name == get_interface_name() && !has_interface_override) {
		_connect_signals();
		_deliver_all();
	}
}

void XRSettingsServer::_on_session_begun() {
	// Foveation and the refresh rate need a running session: push every value again.
	session_running = true;
	_deliver_all();
}

void XRSettingsServer::_on_session_stopping() {
	session_running = false;
}

void XRSettingsServer::_on_startup_applied(ApplyPhase p_phase) {
	if (p_phase >= APPLY_PHASE_SCENE) {
		// The openxr module has added its interface to XRServer.
		_connect_signals();
		_deliver_all();
	}
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName XRSettingsServer::get_domain() const {
	return SNAME("xr");
}

SettingsServer::ApplyPhase XRSettingsServer::get_apply_phase() const {
	return APPLY_PHASE_SCENE;
}

Ref<Resource> XRSettingsServer::_create_default_settings() const {
	Ref<XRSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void XRSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant XRSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	const Variant value = SettingsServer::_validate_setting(p_name, p_value);
	if (value.get_type() == Variant::FLOAT && Math::is_nan(double(value))) {
		WARN_PRINT(vformat("XR settings: \"%s\" is not a number; the default is used.", p_name));
		return get_default_value(p_name);
	}
	return value;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void XRSettingsServer::set_foveation_level(XRSettings::FoveationLevel p_level) {
	set_value(SNAME("foveation_level"), p_level);
}

XRSettings::FoveationLevel XRSettingsServer::get_foveation_level() const {
	return XRSettings::FoveationLevel(int(get_value(SNAME("foveation_level"))));
}

void XRSettingsServer::set_foveation_dynamic(bool p_enabled) {
	set_value(SNAME("foveation_dynamic"), p_enabled);
}

bool XRSettingsServer::is_foveation_dynamic() const {
	return get_value(SNAME("foveation_dynamic"));
}

void XRSettingsServer::set_foveation_with_subsampled_images(bool p_enabled) {
	set_value(SNAME("foveation_with_subsampled_images"), p_enabled);
}

bool XRSettingsServer::is_foveation_with_subsampled_images() const {
	return get_value(SNAME("foveation_with_subsampled_images"));
}

void XRSettingsServer::set_render_target_size_multiplier(float p_multiplier) {
	set_value(SNAME("render_target_size_multiplier"), p_multiplier);
}

float XRSettingsServer::get_render_target_size_multiplier() const {
	return get_value(SNAME("render_target_size_multiplier"));
}

void XRSettingsServer::set_display_refresh_rate(float p_rate) {
	set_value(SNAME("display_refresh_rate"), p_rate);
}

float XRSettingsServer::get_display_refresh_rate() const {
	return get_value(SNAME("display_refresh_rate"));
}

void XRSettingsServer::set_vrs_min_radius(float p_radius) {
	set_value(SNAME("vrs_min_radius"), p_radius);
}

float XRSettingsServer::get_vrs_min_radius() const {
	return get_value(SNAME("vrs_min_radius"));
}

void XRSettingsServer::set_vrs_strength(float p_strength) {
	set_value(SNAME("vrs_strength"), p_strength);
}

float XRSettingsServer::get_vrs_strength() const {
	return get_value(SNAME("vrs_strength"));
}

void XRSettingsServer::set_cpu_level(XRSettings::PerformanceLevel p_level) {
	set_value(SNAME("cpu_level"), p_level);
}

XRSettings::PerformanceLevel XRSettingsServer::get_cpu_level() const {
	return XRSettings::PerformanceLevel(int(get_value(SNAME("cpu_level"))));
}

void XRSettingsServer::set_gpu_level(XRSettings::PerformanceLevel p_level) {
	set_value(SNAME("gpu_level"), p_level);
}

XRSettings::PerformanceLevel XRSettingsServer::get_gpu_level() const {
	return XRSettings::PerformanceLevel(int(get_value(SNAME("gpu_level"))));
}

void XRSettingsServer::set_left_hand_motion_range(XRSettings::HandMotionRange p_range) {
	set_value(SNAME("left_hand_motion_range"), p_range);
}

XRSettings::HandMotionRange XRSettingsServer::get_left_hand_motion_range() const {
	return XRSettings::HandMotionRange(int(get_value(SNAME("left_hand_motion_range"))));
}

void XRSettingsServer::set_right_hand_motion_range(XRSettings::HandMotionRange p_range) {
	set_value(SNAME("right_hand_motion_range"), p_range);
}

XRSettings::HandMotionRange XRSettingsServer::get_right_hand_motion_range() const {
	return XRSettings::HandMotionRange(int(get_value(SNAME("right_hand_motion_range"))));
}

void XRSettingsServer::set_hand_tracking_enabled(bool p_enabled) {
	set_value(SNAME("hand_tracking_enabled"), p_enabled);
}

bool XRSettingsServer::is_hand_tracking_enabled() const {
	return get_value(SNAME("hand_tracking_enabled"));
}

void XRSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// Every value has a typed signal `<value>_changed` (see the class reference).
	for (const char *setting_name : xr_setting_names) {
		if (p_name == StringName(setting_name)) {
			emit_signal(StringName(String(setting_name) + "_changed"), p_value);
			return;
		}
	}
	for (const char *setting_name : xr_game_setting_names) {
		if (p_name == StringName(setting_name)) {
			emit_signal(StringName(String(setting_name) + "_changed"), p_value);
			return;
		}
	}
}

void XRSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("foveation_level_changed", PropertyInfo(Variant::INT, "level", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "XRSettings.FoveationLevel")));
	ADD_SIGNAL(MethodInfo("foveation_dynamic_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("foveation_with_subsampled_images_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("render_target_size_multiplier_changed", PropertyInfo(Variant::FLOAT, "multiplier")));
	ADD_SIGNAL(MethodInfo("display_refresh_rate_changed", PropertyInfo(Variant::FLOAT, "rate")));
	ADD_SIGNAL(MethodInfo("vrs_min_radius_changed", PropertyInfo(Variant::FLOAT, "radius")));
	ADD_SIGNAL(MethodInfo("vrs_strength_changed", PropertyInfo(Variant::FLOAT, "strength")));
	ADD_SIGNAL(MethodInfo("cpu_level_changed", PropertyInfo(Variant::INT, "level", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "XRSettings.PerformanceLevel")));
	ADD_SIGNAL(MethodInfo("gpu_level_changed", PropertyInfo(Variant::INT, "level", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "XRSettings.PerformanceLevel")));
	ADD_SIGNAL(MethodInfo("left_hand_motion_range_changed", PropertyInfo(Variant::INT, "range", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "XRSettings.HandMotionRange")));
	ADD_SIGNAL(MethodInfo("right_hand_motion_range_changed", PropertyInfo(Variant::INT, "range", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CLASS_IS_ENUM, "XRSettings.HandMotionRange")));
	ADD_SIGNAL(MethodInfo("hand_tracking_enabled_changed", PropertyInfo(Variant::BOOL, "enabled")));

	ClassDB::bind_method(D_METHOD("set_foveation_level", "level"), &XRSettingsServer::set_foveation_level);
	ClassDB::bind_method(D_METHOD("get_foveation_level"), &XRSettingsServer::get_foveation_level);
	ClassDB::bind_method(D_METHOD("set_foveation_dynamic", "enabled"), &XRSettingsServer::set_foveation_dynamic);
	ClassDB::bind_method(D_METHOD("is_foveation_dynamic"), &XRSettingsServer::is_foveation_dynamic);
	ClassDB::bind_method(D_METHOD("set_foveation_with_subsampled_images", "enabled"), &XRSettingsServer::set_foveation_with_subsampled_images);
	ClassDB::bind_method(D_METHOD("is_foveation_with_subsampled_images"), &XRSettingsServer::is_foveation_with_subsampled_images);
	ClassDB::bind_method(D_METHOD("set_render_target_size_multiplier", "multiplier"), &XRSettingsServer::set_render_target_size_multiplier);
	ClassDB::bind_method(D_METHOD("get_render_target_size_multiplier"), &XRSettingsServer::get_render_target_size_multiplier);
	ClassDB::bind_method(D_METHOD("set_display_refresh_rate", "rate"), &XRSettingsServer::set_display_refresh_rate);
	ClassDB::bind_method(D_METHOD("get_display_refresh_rate"), &XRSettingsServer::get_display_refresh_rate);
	ClassDB::bind_method(D_METHOD("set_vrs_min_radius", "radius"), &XRSettingsServer::set_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("get_vrs_min_radius"), &XRSettingsServer::get_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("set_vrs_strength", "strength"), &XRSettingsServer::set_vrs_strength);
	ClassDB::bind_method(D_METHOD("get_vrs_strength"), &XRSettingsServer::get_vrs_strength);
	ClassDB::bind_method(D_METHOD("set_cpu_level", "level"), &XRSettingsServer::set_cpu_level);
	ClassDB::bind_method(D_METHOD("get_cpu_level"), &XRSettingsServer::get_cpu_level);
	ClassDB::bind_method(D_METHOD("set_gpu_level", "level"), &XRSettingsServer::set_gpu_level);
	ClassDB::bind_method(D_METHOD("get_gpu_level"), &XRSettingsServer::get_gpu_level);
	ClassDB::bind_method(D_METHOD("set_left_hand_motion_range", "range"), &XRSettingsServer::set_left_hand_motion_range);
	ClassDB::bind_method(D_METHOD("get_left_hand_motion_range"), &XRSettingsServer::get_left_hand_motion_range);
	ClassDB::bind_method(D_METHOD("set_right_hand_motion_range", "range"), &XRSettingsServer::set_right_hand_motion_range);
	ClassDB::bind_method(D_METHOD("get_right_hand_motion_range"), &XRSettingsServer::get_right_hand_motion_range);
	ClassDB::bind_method(D_METHOD("set_hand_tracking_enabled", "enabled"), &XRSettingsServer::set_hand_tracking_enabled);
	ClassDB::bind_method(D_METHOD("is_hand_tracking_enabled"), &XRSettingsServer::is_hand_tracking_enabled);

	ClassDB::bind_method(D_METHOD("set_interface_override", "interface"), &XRSettingsServer::set_interface_override);
	ClassDB::bind_method(D_METHOD("clear_interface_override"), &XRSettingsServer::clear_interface_override);
	ClassDB::bind_method(D_METHOD("get_interface"), &XRSettingsServer::get_interface);
	ClassDB::bind_method(D_METHOD("is_interface_ready"), &XRSettingsServer::is_interface_ready);
	ClassDB::bind_method(D_METHOD("is_session_running"), &XRSettingsServer::is_session_running);
	ClassDB::bind_method(D_METHOD("get_unsupported_setting_names"), &XRSettingsServer::get_unsupported_setting_names);
	ClassDB::bind_method(D_METHOD("get_deferred_setting_names"), &XRSettingsServer::get_deferred_setting_names);
	ClassDB::bind_method(D_METHOD("get_available_display_refresh_rates"), &XRSettingsServer::get_available_display_refresh_rates);

	ClassDB::bind_static_method("XRSettingsServer", D_METHOD("get_interface_name"), &XRSettingsServer::get_interface_name);
}

XRSettingsServer::XRSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	for (const char *setting_name : xr_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &XRSettingsServer::_push_xr_value).bind(bound_name), callable_mp(this, &XRSettingsServer::_pull_xr_value).bind(bound_name));
	}
	for (const char *setting_name : xr_game_setting_names) {
		set_setting_flags(StringName(setting_name), SETTING_FLAG_NO_ENGINE_TARGET);
	}
	SettingsServerManager *manager = SettingsServerManager::get_singleton();
	if (manager != nullptr) {
		manager->connect(SNAME("startup_applied"), callable_mp(this, &XRSettingsServer::_on_startup_applied));
	}
}

XRSettingsServer::~XRSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif // MODULE_OPENXR_ENABLED && !XR_DISABLED
