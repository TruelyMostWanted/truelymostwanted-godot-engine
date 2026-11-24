/**************************************************************************/
/*  engine_settings_server.h                                              */
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

#include "servers/settings/settings_server.h"

// Settings domain `engine` (#26): frame rate limit, time scale, physics rate and interpolation, audio playback
// speed and low-processor mode (EngineSettings), pushed to Engine, SceneTree, AudioServer and OS at once.
// Applied in APPLY_PHASE_SCENE (SceneTree exists). Defaults are the project settings, not the engine state.
// Simulation values (time scale, physics rate, audio speed) change game behavior: unless the project setting
// `settings/engine/allow_simulation_values` is enabled, they stay at the project values.
// Precedence: command line (`--max-fps`, `--time-scale`: session overrides) > player's file > project settings.
// Base API used: bind_setting_callbacks(), set_session_override(), has_session_override(), get_default_value(),
// get_applied_value(), set_value(), get_value(), is_started(), the `loaded` signal and the virtuals below.
class EngineSettingsServer : public SettingsServer {
	GDCLASS(EngineSettingsServer, SettingsServer);

	static EngineSettingsServer *singleton;

	bool defaults_captured = false;
	bool command_line_applied = false;
	bool accepting_command_line = false;

	static Variant _get_project_value(const StringName &p_name);
	static bool _has_range(const StringName &p_name, double &r_min, double &r_max, bool &r_integer);

	void _push_engine_value(const Variant &p_value, const StringName &p_name);
	Variant _pull_engine_value(const StringName &p_name);
	void _on_loaded();

protected:
	static void _bind_methods();

	virtual Ref<Resource> _create_default_settings() const override;
	virtual void _read_settings_from_engine(const Ref<Resource> &p_settings) override;
	virtual Variant _validate_setting(const StringName &p_name, const Variant &p_value) const override;

public:
	static EngineSettingsServer *get_singleton();
	static void register_settings();

	// Time scale, physics rate and audio speed: values that change the game's behavior.
	static PackedStringArray get_simulation_setting_names();
	static bool is_simulation_setting(const StringName &p_name);
	// True unless the project setting `settings/engine/allow_simulation_values` is enabled.
	static bool is_simulation_locked();

	virtual StringName get_domain() const override;
	virtual ApplyPhase get_apply_phase() const override;
	virtual bool is_value_supported(const StringName &p_name) const override;

	void set_max_fps(int p_max_fps);
	int get_max_fps() const;
	void set_time_scale(double p_time_scale);
	double get_time_scale() const;
	void set_physics_ticks_per_second(int p_ticks_per_second);
	int get_physics_ticks_per_second() const;
	void set_max_physics_steps_per_frame(int p_max_physics_steps);
	int get_max_physics_steps_per_frame() const;
	void set_physics_jitter_fix(double p_threshold);
	double get_physics_jitter_fix() const;
	void set_physics_interpolation_enabled(bool p_enabled);
	bool is_physics_interpolation_enabled() const;
	void set_audio_playback_speed_scale(float p_scale);
	float get_audio_playback_speed_scale() const;
	void set_low_processor_mode_enabled(bool p_enabled);
	bool is_low_processor_mode_enabled() const;
	void set_low_processor_mode_sleep_usec(int p_usec);
	int get_low_processor_mode_sleep_usec() const;

	EngineSettingsServer();
	~EngineSettingsServer();
};
