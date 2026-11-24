/**************************************************************************/
/*  xr_settings_server.h                                                  */
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

#include "modules/modules_enabled.gen.h" // For openxr.

#if defined(MODULE_OPENXR_ENABLED) && !defined(XR_DISABLED)

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "servers/settings/settings_server.h"
#include "servers/settings/xr_settings.h"

// Settings domain `xr` (#30): OpenXR options (XRSettings) pushed to the interface named "OpenXR" in XRServer.
// Only built and registered with the openxr module (not on the Web, iOS or with `disable_xr`).
// The domain talks to the interface through its bound properties and methods, so servers/ does not include the
// module headers; set_interface_override() points it at another object with the same API (tests, extensions).
// Values are kept when there is no initialized interface yet (get_deferred_setting_names()) and pushed when
// XRServer adds the interface and again on its `session_begun` signal (foveation and the refresh rate need a
// running session). render_target_size_multiplier goes to the interface as soon as it exists, before initialize().
// Values the runtime does not support are stored but not pushed (get_unsupported_setting_names()).
// hand_tracking_enabled is for the game only: the OpenXR hand tracking extension is switched by the project
// setting `xr/openxr/extensions/hand_tracking`, which the module reads before any settings domain is applied.
// Applied in APPLY_PHASE_SCENE (the openxr module creates its interface at the scene level).
class XRSettingsServer : public SettingsServer {
	GDCLASS(XRSettingsServer, SettingsServer);

	static XRSettingsServer *singleton;

	// The last value pushed per setting (the effective one, with session overrides).
	HashMap<StringName, Variant> pushed_values;
	// Pushed values the interface could not take yet.
	HashSet<StringName> deferred;
	// Unsupported values that were warned about once.
	HashSet<StringName> warned;

	ObjectID interface_override;
	bool has_interface_override = false;
	ObjectID connected_interface;
	bool xr_server_connected = false;
	bool session_running = false;
	bool defaults_captured = false;

	static Variant _get_project_value(const StringName &p_name);
	static bool _is_interface_initialized(Object *p_interface);
	static bool _is_supported_by(Object *p_interface, const StringName &p_name);
	static double _get_nearest_refresh_rate(Object *p_interface, double p_rate);

	Variant _get_current(const StringName &p_name) const;
	void _connect_signals();
	bool _deliver(const StringName &p_name, const Variant &p_value);
	void _deliver_all();
	void _push_xr_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_xr_value(const StringName &p_name);

	void _on_interface_added(const StringName &p_interface_name);
	void _on_session_begun();
	void _on_session_stopping();
	void _on_startup_applied(ApplyPhase p_phase);

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static XRSettingsServer *get_singleton();
	// The XRServer interface the domain drives: "OpenXR".
	static StringName get_interface_name();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;

	// Interface.
	void set_interface_override(Object *p_interface);
	void clear_interface_override();
	Object *get_interface() const;
	bool is_interface_ready() const;
	bool is_session_running() const;
	PackedStringArray get_unsupported_setting_names() const;
	PackedStringArray get_deferred_setting_names() const;
	PackedFloat64Array get_available_display_refresh_rates() const;

	// Values.
	void set_foveation_level(XRSettings::FoveationLevel p_level);
	XRSettings::FoveationLevel get_foveation_level() const;
	void set_foveation_dynamic(bool p_enabled);
	bool is_foveation_dynamic() const;
	void set_foveation_with_subsampled_images(bool p_enabled);
	bool is_foveation_with_subsampled_images() const;
	void set_render_target_size_multiplier(float p_multiplier);
	float get_render_target_size_multiplier() const;
	void set_display_refresh_rate(float p_rate);
	float get_display_refresh_rate() const;
	void set_vrs_min_radius(float p_radius);
	float get_vrs_min_radius() const;
	void set_vrs_strength(float p_strength);
	float get_vrs_strength() const;
	void set_cpu_level(XRSettings::PerformanceLevel p_level);
	XRSettings::PerformanceLevel get_cpu_level() const;
	void set_gpu_level(XRSettings::PerformanceLevel p_level);
	XRSettings::PerformanceLevel get_gpu_level() const;
	void set_left_hand_motion_range(XRSettings::HandMotionRange p_range);
	XRSettings::HandMotionRange get_left_hand_motion_range() const;
	void set_right_hand_motion_range(XRSettings::HandMotionRange p_range);
	XRSettings::HandMotionRange get_right_hand_motion_range() const;
	void set_hand_tracking_enabled(bool p_enabled);
	bool is_hand_tracking_enabled() const;

	XRSettingsServer();
	~XRSettingsServer();
};

#endif // MODULE_OPENXR_ENABLED && !XR_DISABLED
