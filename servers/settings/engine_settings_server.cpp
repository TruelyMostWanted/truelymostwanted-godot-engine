/**************************************************************************/
/*  engine_settings_server.cpp                                            */
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

#include "engine_settings_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "servers/audio/audio_server.h"
#include "servers/settings/engine_settings.h"

static constexpr const char *SETTING_ALLOW_SIMULATION_VALUES = "settings/engine/allow_simulation_values";

// The values of the domain, in the order of EngineSettings.
static const char *engine_setting_names[] = {
	"max_fps",
	"time_scale",
	"physics_ticks_per_second",
	"physics_jitter_fix",
	"physics_interpolation",
	"max_physics_steps_per_frame",
	"audio_playback_speed_scale",
	"low_processor_mode",
	"low_processor_mode_sleep_usec",
};

static const char *simulation_setting_names[] = {
	"time_scale",
	"physics_ticks_per_second",
	"max_physics_steps_per_frame",
	"physics_jitter_fix",
	"audio_playback_speed_scale",
};

// Options of Main::get_command_line_engine_options() that become session overrides.
static const char *command_line_setting_names[] = {
	"max_fps",
	"time_scale",
};

static Variant engine_project_setting(const String &p_name, const Variant &p_default) {
	const ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr || !project_settings->has_setting(p_name)) {
		return p_default;
	}
	return project_settings->get_setting_with_override(p_name);
}

EngineSettingsServer *EngineSettingsServer::singleton = nullptr;

EngineSettingsServer *EngineSettingsServer::get_singleton() {
	return singleton;
}

void EngineSettingsServer::register_settings() {
	GLOBAL_DEF(SETTING_ALLOW_SIMULATION_VALUES, false);
}

/////////////////////////////////////////////////////////////////////////////
// Simulation values.

PackedStringArray EngineSettingsServer::get_simulation_setting_names() {
	PackedStringArray names;
	for (const char *simulation_name : simulation_setting_names) {
		names.push_back(simulation_name);
	}
	return names;
}

bool EngineSettingsServer::is_simulation_setting(const StringName &p_name) {
	for (const char *simulation_name : simulation_setting_names) {
		if (p_name == StringName(simulation_name)) {
			return true;
		}
	}
	return false;
}

bool EngineSettingsServer::is_simulation_locked() {
	return !bool(engine_project_setting(SETTING_ALLOW_SIMULATION_VALUES, false));
}

/////////////////////////////////////////////////////////////////////////////
// Engine targets. Without a SceneTree or an AudioServer (headless tools, unit tests) those values are not pushed.

Variant EngineSettingsServer::_get_project_value(const StringName &p_name) {
	// What Main::setup() and SceneTree read from the project settings.
	if (p_name == SNAME("max_fps")) {
		return int(engine_project_setting("application/run/max_fps", 0));
	} else if (p_name == SNAME("time_scale")) {
		return 1.0;
	} else if (p_name == SNAME("physics_ticks_per_second")) {
		return int(engine_project_setting("physics/common/physics_ticks_per_second", 60));
	} else if (p_name == SNAME("max_physics_steps_per_frame")) {
		return int(engine_project_setting("physics/common/max_physics_steps_per_frame", 8));
	} else if (p_name == SNAME("physics_jitter_fix")) {
		return double(engine_project_setting("physics/common/physics_jitter_fix", 0.5));
	} else if (p_name == SNAME("physics_interpolation")) {
		return bool(engine_project_setting("physics/common/physics_interpolation", false));
	} else if (p_name == SNAME("audio_playback_speed_scale")) {
		return 1.0f;
	} else if (p_name == SNAME("low_processor_mode")) {
		return bool(engine_project_setting("application/run/low_processor_mode", false));
	} else if (p_name == SNAME("low_processor_mode_sleep_usec")) {
		return int(engine_project_setting("application/run/low_processor_mode_sleep_usec", 6900));
	}
	return Variant();
}

bool EngineSettingsServer::_has_range(const StringName &p_name, double &r_min, double &r_max, bool &r_integer) {
	r_integer = true;
	if (p_name == SNAME("max_fps")) {
		r_min = 0; // 0: unlimited.
		r_max = double(INT32_MAX);
	} else if (p_name == SNAME("time_scale")) {
		// Not 0: a game frozen at every start could not be fixed from inside the game.
		r_min = 0.01;
		r_max = 100.0;
		r_integer = false;
	} else if (p_name == SNAME("physics_ticks_per_second")) {
		r_min = 1;
		r_max = 1000;
	} else if (p_name == SNAME("max_physics_steps_per_frame")) {
		r_min = 1;
		r_max = 100;
	} else if (p_name == SNAME("physics_jitter_fix")) {
		r_min = 0.0;
		r_max = 2.0;
		r_integer = false;
	} else if (p_name == SNAME("audio_playback_speed_scale")) {
		r_min = 0.01;
		r_max = 4.0;
		r_integer = false;
	} else if (p_name == SNAME("low_processor_mode_sleep_usec")) {
		r_min = 0;
		r_max = 1000000; // One second.
	} else {
		return false;
	}
	return true;
}

void EngineSettingsServer::_push_engine_value(const Variant &p_value, const StringName &p_name) {
	Engine *engine = Engine::get_singleton();
	if (p_name == SNAME("max_fps")) {
		engine->set_max_fps(int(p_value));
	} else if (p_name == SNAME("time_scale")) {
		engine->set_time_scale(double(p_value));
	} else if (p_name == SNAME("physics_ticks_per_second")) {
		engine->set_physics_ticks_per_second(int(p_value));
	} else if (p_name == SNAME("max_physics_steps_per_frame")) {
		engine->set_max_physics_steps_per_frame(int(p_value));
	} else if (p_name == SNAME("physics_jitter_fix")) {
		engine->set_physics_jitter_fix(double(p_value));
	} else if (p_name == SNAME("physics_interpolation")) {
		SceneTree *tree = SceneTree::get_singleton();
		if (tree != nullptr) {
			tree->set_physics_interpolation_enabled(bool(p_value));
		}
	} else if (p_name == SNAME("audio_playback_speed_scale")) {
		AudioServer *audio_server = AudioServer::get_singleton();
		if (audio_server != nullptr) {
			audio_server->set_playback_speed_scale(float(p_value));
		}
	} else if (p_name == SNAME("low_processor_mode")) {
		OS::get_singleton()->set_low_processor_usage_mode(bool(p_value));
	} else if (p_name == SNAME("low_processor_mode_sleep_usec")) {
		OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(p_value));
	}
}

Variant EngineSettingsServer::_pull_engine_value(const StringName &p_name) {
	if (!defaults_captured) {
		// The defaults: the project values, not the engine state (which already holds `--max-fps` and `--time-scale`).
		return _get_project_value(p_name);
	}
	if (has_session_override(p_name)) {
		// Reading the engine back must not turn a command-line value into a stored one.
		return get_applied_value(p_name);
	}
	const Engine *engine = Engine::get_singleton();
	if (p_name == SNAME("max_fps")) {
		return engine->get_max_fps();
	} else if (p_name == SNAME("time_scale")) {
		return engine->get_time_scale();
	} else if (p_name == SNAME("physics_ticks_per_second")) {
		return engine->get_physics_ticks_per_second();
	} else if (p_name == SNAME("max_physics_steps_per_frame")) {
		return engine->get_max_physics_steps_per_frame();
	} else if (p_name == SNAME("physics_jitter_fix")) {
		return engine->get_physics_jitter_fix();
	} else if (p_name == SNAME("physics_interpolation")) {
		const SceneTree *tree = SceneTree::get_singleton();
		return tree != nullptr ? Variant(tree->is_physics_interpolation_enabled()) : get_applied_value(p_name);
	} else if (p_name == SNAME("audio_playback_speed_scale")) {
		const AudioServer *audio_server = AudioServer::get_singleton();
		return audio_server != nullptr ? Variant(audio_server->get_playback_speed_scale()) : get_applied_value(p_name);
	} else if (p_name == SNAME("low_processor_mode")) {
		return OS::get_singleton()->is_in_low_processor_usage_mode();
	} else if (p_name == SNAME("low_processor_mode_sleep_usec")) {
		return OS::get_singleton()->get_low_processor_usage_mode_sleep_usec();
	}
	return get_applied_value(p_name);
}

void EngineSettingsServer::_on_loaded() {
	// Only the load of startup(), after the player's file is read and before it is applied.
	if (!is_started() || command_line_applied) {
		return;
	}
	command_line_applied = true;
	const Dictionary options = Main::get_command_line_engine_options();
	accepting_command_line = true;
	for (const char *option_name : command_line_setting_names) {
		if (options.has(option_name)) {
			// Pushed now, never saved; a later set_value() replaces it.
			set_session_override(option_name, options[option_name]);
		}
	}
	accepting_command_line = false;
}

/////////////////////////////////////////////////////////////////////////////
// Domain.

StringName EngineSettingsServer::get_domain() const {
	return SNAME("engine");
}

SettingsServer::ApplyPhase EngineSettingsServer::get_apply_phase() const {
	// SceneTree (physics interpolation) exists; the main scene is not loaded yet.
	return APPLY_PHASE_SCENE;
}

Ref<Resource> EngineSettingsServer::_create_default_settings() const {
	Ref<EngineSettings> default_settings;
	default_settings.instantiate();
	return default_settings;
}

void EngineSettingsServer::_read_settings_from_engine(const Ref<Resource> &p_settings) {
	SettingsServer::_read_settings_from_engine(p_settings);
	// The first read fills the defaults; later ones read the engine.
	defaults_captured = true;
}

Variant EngineSettingsServer::_validate_setting(const StringName &p_name, const Variant &p_value) const {
	Variant value = SettingsServer::_validate_setting(p_name, p_value);

	double min_value = 0.0;
	double max_value = 0.0;
	bool integer = true;
	if (_has_range(p_name, min_value, max_value, integer) && (value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT)) {
		const double number = value;
		if (Math::is_nan(number)) {
			WARN_PRINT(vformat("Engine settings: \"%s\" is not a number; the default is used.", p_name));
			value = get_default_value(p_name);
		} else {
			const double clamped = CLAMP(number, min_value, max_value);
			if (clamped != number) {
				WARN_PRINT(vformat("Engine settings: %s is out of range for \"%s\" (%s to %s); %s is used.", value, p_name, Variant(min_value), Variant(max_value), Variant(clamped)));
			}
			value = integer ? Variant(int64_t(Math::round(clamped))) : Variant(clamped);
		}
	}

	if (is_simulation_setting(p_name) && is_simulation_locked() && !accepting_command_line) {
		// A hand-edited file or a game without a speed option must not change the simulation.
		const Variant locked_value = get_default_value(p_name);
		if (!values_equal(value, locked_value)) {
			WARN_PRINT(vformat("Engine settings: \"%s\" is a simulation value and stays %s; enable the project setting \"%s\" to let players change it.", p_name, locked_value, SETTING_ALLOW_SIMULATION_VALUES));
		}
		return locked_value;
	}
	return value;
}

bool EngineSettingsServer::is_value_supported(const StringName &p_name) const {
	if (!SettingsServer::is_value_supported(p_name)) {
		return false;
	}
	if (is_simulation_setting(p_name)) {
		return !is_simulation_locked();
	}
	if (p_name == SNAME("low_processor_mode_sleep_usec")) {
		// The browser schedules the frames: the single-threaded Web export does not sleep between them.
		return !OS::get_singleton()->has_feature("web");
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Typed API.

void EngineSettingsServer::set_max_fps(int p_max_fps) {
	set_value(SNAME("max_fps"), p_max_fps);
}

int EngineSettingsServer::get_max_fps() const {
	return get_value(SNAME("max_fps"));
}

void EngineSettingsServer::set_time_scale(double p_time_scale) {
	set_value(SNAME("time_scale"), p_time_scale);
}

double EngineSettingsServer::get_time_scale() const {
	return get_value(SNAME("time_scale"));
}

void EngineSettingsServer::set_physics_ticks_per_second(int p_ticks_per_second) {
	set_value(SNAME("physics_ticks_per_second"), p_ticks_per_second);
}

int EngineSettingsServer::get_physics_ticks_per_second() const {
	return get_value(SNAME("physics_ticks_per_second"));
}

void EngineSettingsServer::set_max_physics_steps_per_frame(int p_max_physics_steps) {
	set_value(SNAME("max_physics_steps_per_frame"), p_max_physics_steps);
}

int EngineSettingsServer::get_max_physics_steps_per_frame() const {
	return get_value(SNAME("max_physics_steps_per_frame"));
}

void EngineSettingsServer::set_physics_jitter_fix(double p_threshold) {
	set_value(SNAME("physics_jitter_fix"), p_threshold);
}

double EngineSettingsServer::get_physics_jitter_fix() const {
	return get_value(SNAME("physics_jitter_fix"));
}

void EngineSettingsServer::set_physics_interpolation_enabled(bool p_enabled) {
	set_value(SNAME("physics_interpolation"), p_enabled);
}

bool EngineSettingsServer::is_physics_interpolation_enabled() const {
	return get_value(SNAME("physics_interpolation"));
}

void EngineSettingsServer::set_audio_playback_speed_scale(float p_scale) {
	set_value(SNAME("audio_playback_speed_scale"), p_scale);
}

float EngineSettingsServer::get_audio_playback_speed_scale() const {
	return get_value(SNAME("audio_playback_speed_scale"));
}

void EngineSettingsServer::set_low_processor_mode_enabled(bool p_enabled) {
	set_value(SNAME("low_processor_mode"), p_enabled);
}

bool EngineSettingsServer::is_low_processor_mode_enabled() const {
	return get_value(SNAME("low_processor_mode"));
}

void EngineSettingsServer::set_low_processor_mode_sleep_usec(int p_usec) {
	set_value(SNAME("low_processor_mode_sleep_usec"), p_usec);
}

int EngineSettingsServer::get_low_processor_mode_sleep_usec() const {
	return get_value(SNAME("low_processor_mode_sleep_usec"));
}

void EngineSettingsServer::_notify_value_applied(const StringName &p_name, const Variant &p_value) {
	SettingsServer::_notify_value_applied(p_name, p_value);
	// The values with a typed signal `<value>_changed` (see the class reference).
	static const char *const SIGNAL_VALUES[] = {
		"max_fps",
		"time_scale",
		"physics_ticks_per_second",
		"physics_interpolation",
		"low_processor_mode",
	};
	for (const char *value_name : SIGNAL_VALUES) {
		if (p_name == StringName(value_name)) {
			emit_signal(StringName(String(value_name) + "_changed"), p_value);
			return;
		}
	}
}

void EngineSettingsServer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("max_fps_changed", PropertyInfo(Variant::INT, "fps")));
	ADD_SIGNAL(MethodInfo("time_scale_changed", PropertyInfo(Variant::FLOAT, "scale")));
	ADD_SIGNAL(MethodInfo("physics_ticks_per_second_changed", PropertyInfo(Variant::INT, "ticks")));
	ADD_SIGNAL(MethodInfo("physics_interpolation_changed", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("low_processor_mode_changed", PropertyInfo(Variant::BOOL, "enabled")));

	ClassDB::bind_method(D_METHOD("set_max_fps", "max_fps"), &EngineSettingsServer::set_max_fps);
	ClassDB::bind_method(D_METHOD("get_max_fps"), &EngineSettingsServer::get_max_fps);
	ClassDB::bind_method(D_METHOD("set_time_scale", "time_scale"), &EngineSettingsServer::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &EngineSettingsServer::get_time_scale);
	ClassDB::bind_method(D_METHOD("set_physics_ticks_per_second", "ticks_per_second"), &EngineSettingsServer::set_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("get_physics_ticks_per_second"), &EngineSettingsServer::get_physics_ticks_per_second);
	ClassDB::bind_method(D_METHOD("set_max_physics_steps_per_frame", "max_physics_steps"), &EngineSettingsServer::set_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_physics_steps_per_frame"), &EngineSettingsServer::get_max_physics_steps_per_frame);
	ClassDB::bind_method(D_METHOD("set_physics_jitter_fix", "threshold"), &EngineSettingsServer::set_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("get_physics_jitter_fix"), &EngineSettingsServer::get_physics_jitter_fix);
	ClassDB::bind_method(D_METHOD("set_physics_interpolation_enabled", "enabled"), &EngineSettingsServer::set_physics_interpolation_enabled);
	ClassDB::bind_method(D_METHOD("is_physics_interpolation_enabled"), &EngineSettingsServer::is_physics_interpolation_enabled);
	ClassDB::bind_method(D_METHOD("set_audio_playback_speed_scale", "scale"), &EngineSettingsServer::set_audio_playback_speed_scale);
	ClassDB::bind_method(D_METHOD("get_audio_playback_speed_scale"), &EngineSettingsServer::get_audio_playback_speed_scale);
	ClassDB::bind_method(D_METHOD("set_low_processor_mode_enabled", "enabled"), &EngineSettingsServer::set_low_processor_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_low_processor_mode_enabled"), &EngineSettingsServer::is_low_processor_mode_enabled);
	ClassDB::bind_method(D_METHOD("set_low_processor_mode_sleep_usec", "usec"), &EngineSettingsServer::set_low_processor_mode_sleep_usec);
	ClassDB::bind_method(D_METHOD("get_low_processor_mode_sleep_usec"), &EngineSettingsServer::get_low_processor_mode_sleep_usec);

	ClassDB::bind_static_method("EngineSettingsServer", D_METHOD("get_simulation_setting_names"), &EngineSettingsServer::get_simulation_setting_names);
	ClassDB::bind_static_method("EngineSettingsServer", D_METHOD("is_simulation_setting", "name"), &EngineSettingsServer::is_simulation_setting);
	ClassDB::bind_static_method("EngineSettingsServer", D_METHOD("is_simulation_locked"), &EngineSettingsServer::is_simulation_locked);
}

EngineSettingsServer::EngineSettingsServer() {
	if (singleton == nullptr) {
		singleton = this;
	}
	register_settings();
	for (const char *setting_name : engine_setting_names) {
		const StringName bound_name = setting_name;
		bind_setting_callbacks(bound_name, callable_mp(this, &EngineSettingsServer::_push_engine_value).bind(bound_name), callable_mp(this, &EngineSettingsServer::_pull_engine_value).bind(bound_name));
	}
	connect(SNAME("loaded"), callable_mp(this, &EngineSettingsServer::_on_loaded));
}

EngineSettingsServer::~EngineSettingsServer() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
