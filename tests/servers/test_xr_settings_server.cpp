/**************************************************************************/
/*  test_xr_settings_server.cpp                                           */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_xr_settings_server)

#include "modules/modules_enabled.gen.h" // For openxr.

#if defined(MODULE_OPENXR_ENABLED) && !defined(XR_DISABLED)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_defs.h"
#include "core/object/class_db.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/xr_settings.h"
#include "servers/settings/xr_settings_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestXRSettingsServer {

// Stands in for OpenXRInterface: the same bound properties, methods and signals, no runtime.
class XRSettingsTestInterface : public Object {
	GDCLASS(XRSettingsTestInterface, Object);

public:
	bool initialized = false;
	bool foveation_supported = true;
	bool hand_tracking_supported = true;
	Array refresh_rates;

	int foveation_level = 0;
	bool foveation_dynamic = false;
	bool foveation_with_subsampled_images = false;
	double render_target_size_multiplier = 1.0;
	double display_refresh_rate = 0.0;
	double vrs_min_radius = 20.0;
	double vrs_strength = 1.0;
	int cpu_level = -1;
	int gpu_level = -1;
	int motion_range[2] = { 0, 0 };

	bool is_initialized() const { return initialized; }
	bool is_foveation_supported() const { return foveation_supported; }
	bool is_hand_tracking_supported() const { return hand_tracking_supported; }
	Array get_available_display_refresh_rates() const { return initialized ? refresh_rates : Array(); }

	void set_foveation_level(int p_level) { foveation_level = p_level; }
	int get_foveation_level() const { return foveation_level; }
	void set_foveation_dynamic(bool p_enabled) { foveation_dynamic = p_enabled; }
	bool get_foveation_dynamic() const { return foveation_dynamic; }
	void set_foveation_with_subsampled_images(bool p_enabled) { foveation_with_subsampled_images = p_enabled; }
	bool get_foveation_with_subsampled_images() const { return foveation_with_subsampled_images; }
	void set_render_target_size_multiplier(double p_multiplier) { render_target_size_multiplier = p_multiplier; }
	double get_render_target_size_multiplier() const { return render_target_size_multiplier; }
	void set_display_refresh_rate(double p_rate) { display_refresh_rate = p_rate; }
	double get_display_refresh_rate() const { return display_refresh_rate; }
	void set_vrs_min_radius(double p_radius) { vrs_min_radius = p_radius; }
	double get_vrs_min_radius() const { return vrs_min_radius; }
	void set_vrs_strength(double p_strength) { vrs_strength = p_strength; }
	double get_vrs_strength() const { return vrs_strength; }
	void set_cpu_level(int p_level) { cpu_level = p_level; }
	void set_gpu_level(int p_level) { gpu_level = p_level; }
	void set_motion_range(int p_hand, int p_range) { motion_range[CLAMP(p_hand, 0, 1)] = p_range; }
	int get_motion_range(int p_hand) const { return motion_range[CLAMP(p_hand, 0, 1)]; }

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("is_initialized"), &XRSettingsTestInterface::is_initialized);
		ClassDB::bind_method(D_METHOD("is_foveation_supported"), &XRSettingsTestInterface::is_foveation_supported);
		ClassDB::bind_method(D_METHOD("is_hand_tracking_supported"), &XRSettingsTestInterface::is_hand_tracking_supported);
		ClassDB::bind_method(D_METHOD("get_available_display_refresh_rates"), &XRSettingsTestInterface::get_available_display_refresh_rates);
		ClassDB::bind_method(D_METHOD("set_foveation_level", "level"), &XRSettingsTestInterface::set_foveation_level);
		ClassDB::bind_method(D_METHOD("get_foveation_level"), &XRSettingsTestInterface::get_foveation_level);
		ClassDB::bind_method(D_METHOD("set_foveation_dynamic", "enabled"), &XRSettingsTestInterface::set_foveation_dynamic);
		ClassDB::bind_method(D_METHOD("get_foveation_dynamic"), &XRSettingsTestInterface::get_foveation_dynamic);
		ClassDB::bind_method(D_METHOD("set_foveation_with_subsampled_images", "enabled"), &XRSettingsTestInterface::set_foveation_with_subsampled_images);
		ClassDB::bind_method(D_METHOD("get_foveation_with_subsampled_images"), &XRSettingsTestInterface::get_foveation_with_subsampled_images);
		ClassDB::bind_method(D_METHOD("set_render_target_size_multiplier", "multiplier"), &XRSettingsTestInterface::set_render_target_size_multiplier);
		ClassDB::bind_method(D_METHOD("get_render_target_size_multiplier"), &XRSettingsTestInterface::get_render_target_size_multiplier);
		ClassDB::bind_method(D_METHOD("set_display_refresh_rate", "rate"), &XRSettingsTestInterface::set_display_refresh_rate);
		ClassDB::bind_method(D_METHOD("get_display_refresh_rate"), &XRSettingsTestInterface::get_display_refresh_rate);
		ClassDB::bind_method(D_METHOD("set_vrs_min_radius", "radius"), &XRSettingsTestInterface::set_vrs_min_radius);
		ClassDB::bind_method(D_METHOD("get_vrs_min_radius"), &XRSettingsTestInterface::get_vrs_min_radius);
		ClassDB::bind_method(D_METHOD("set_vrs_strength", "strength"), &XRSettingsTestInterface::set_vrs_strength);
		ClassDB::bind_method(D_METHOD("get_vrs_strength"), &XRSettingsTestInterface::get_vrs_strength);
		ClassDB::bind_method(D_METHOD("set_cpu_level", "level"), &XRSettingsTestInterface::set_cpu_level);
		ClassDB::bind_method(D_METHOD("set_gpu_level", "level"), &XRSettingsTestInterface::set_gpu_level);
		ClassDB::bind_method(D_METHOD("set_motion_range", "hand", "motion_range"), &XRSettingsTestInterface::set_motion_range);
		ClassDB::bind_method(D_METHOD("get_motion_range", "hand"), &XRSettingsTestInterface::get_motion_range);

		ADD_PROPERTY(PropertyInfo(Variant::INT, "foveation_level"), "set_foveation_level", "get_foveation_level");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foveation_dynamic"), "set_foveation_dynamic", "get_foveation_dynamic");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foveation_with_subsampled_images"), "set_foveation_with_subsampled_images", "get_foveation_with_subsampled_images");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_target_size_multiplier"), "set_render_target_size_multiplier", "get_render_target_size_multiplier");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "display_refresh_rate"), "set_display_refresh_rate", "get_display_refresh_rate");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_min_radius"), "set_vrs_min_radius", "get_vrs_min_radius");
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_strength"), "set_vrs_strength", "get_vrs_strength");

		ADD_SIGNAL(MethodInfo("session_begun"));
		ADD_SIGNAL(MethodInfo("session_stopping"));
	}
};

// Autosave off for the test; the test interface class registered once.
struct TestSetup {
	TestSetup() {
		static bool registered = false;
		if (!registered) {
			GDREGISTER_CLASS(XRSettingsTestInterface);
			registered = true;
		}
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/general/autosave", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("xr_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

// A server with its own file that drives `p_interface` instead of XRServer's OpenXR interface.
static XRSettingsServer *create_server(const String &p_file, Object *p_interface) {
	XRSettingsServer *server = memnew(XRSettingsServer);
	server->set_file_path(temp_file(p_file));
	server->set_interface_override(p_interface);
	return server;
}

TEST_CASE("[XRSettingsServer] The domain is an engine singleton registered with the manager") {
	TestSetup setup;
	XRSettingsServer *server = XRSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("XRSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("xr") == server);
	CHECK(server->get_domain() == StringName("xr"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(server->get_file_path() == "user://settings/xr.ini");
	CHECK(Object::cast_to<XRSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(XRSettingsServer::get_interface_name() == StringName("OpenXR"));

	const PackedStringArray names = server->get_setting_names();
	CHECK(names.size() == 12);
	for (const char *name : { "foveation_level", "foveation_dynamic", "foveation_with_subsampled_images", "render_target_size_multiplier", "display_refresh_rate", "vrs_min_radius", "vrs_strength", "cpu_level", "gpu_level", "left_hand_motion_range", "right_hand_motion_range" }) {
		CHECK_MESSAGE(names.has(name), name);
		CHECK_MESSAGE(server->is_setting_bound(name), name);
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
		CHECK_FALSE(server->get_setting_flags(name).has_flag(SettingsServer::SETTING_FLAG_RESTART_REQUIRED));
	}
	CHECK(names.has("hand_tracking_enabled"));
	CHECK(server->get_setting_flags("hand_tracking_enabled").has_flag(SettingsServer::SETTING_FLAG_NO_ENGINE_TARGET));
}

TEST_CASE("[XRSettingsServer] Defaults are the OpenXR project settings and the runtime defaults") {
	TestSetup setup;
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const Variant old_level = project_settings->get_setting("xr/openxr/foveation_level", 0);
	const Variant old_dynamic = project_settings->get_setting("xr/openxr/foveation_dynamic", false);
	project_settings->set_setting("xr/openxr/foveation_level", 2);
	project_settings->set_setting("xr/openxr/foveation_dynamic", true);

	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	XRSettingsServer *server = create_server("defaults.ini", xr_interface);
	CHECK(server->get_foveation_level() == XRSettings::FOVEATION_LEVEL_MEDIUM);
	CHECK(server->is_foveation_dynamic());
	CHECK(server->get_render_target_size_multiplier() == doctest::Approx(1.0));
	CHECK(server->get_display_refresh_rate() == doctest::Approx(0.0));
	CHECK(server->get_vrs_min_radius() == doctest::Approx(20.0));
	CHECK(server->get_vrs_strength() == doctest::Approx(1.0));
	CHECK(server->get_cpu_level() == XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	CHECK(server->get_gpu_level() == XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	CHECK(server->get_left_hand_motion_range() == XRSettings::HAND_MOTION_RANGE_UNOBSTRUCTED);
	CHECK(server->get_right_hand_motion_range() == XRSettings::HAND_MOTION_RANGE_UNOBSTRUCTED);
	CHECK(server->is_hand_tracking_enabled());
	CHECK(int(server->get_default_value("foveation_level")) == 2);
	memdelete(server);
	memdelete(xr_interface);

	project_settings->set_setting("xr/openxr/foveation_level", old_level);
	project_settings->set_setting("xr/openxr/foveation_dynamic", old_dynamic);
}

TEST_CASE("[XRSettingsServer] Values are validated") {
	TestSetup setup;
	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	XRSettingsServer *server = create_server("validate.ini", xr_interface);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	ERR_PRINT_OFF;
	server->set_render_target_size_multiplier(10.0);
	CHECK(server->get_render_target_size_multiplier() == doctest::Approx(3.0));
	server->set_render_target_size_multiplier(0.0);
	CHECK(server->get_render_target_size_multiplier() == doctest::Approx(0.1));
	server->set_display_refresh_rate(-60.0);
	CHECK(server->get_display_refresh_rate() == doctest::Approx(0.0));
	server->set_vrs_strength(50.0);
	CHECK(server->get_vrs_strength() == doctest::Approx(10.0));
	server->set_vrs_min_radius(Math::NaN);
	CHECK(server->get_vrs_min_radius() == doctest::Approx(20.0));
	server->set_value("foveation_level", 7);
	CHECK(server->get_foveation_level() == XRSettings::FOVEATION_LEVEL_OFF);
	server->set_value("cpu_level", 9);
	CHECK(server->get_cpu_level() == XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	ERR_PRINT_ON;

	server->set_display_refresh_rate(90.0);
	CHECK(server->get_display_refresh_rate() == doctest::Approx(90.0));
	memdelete(server);
	memdelete(xr_interface);
}

TEST_CASE("[XRSettingsServer] Without an interface every value is stored, saved and deferred") {
	TestSetup setup;
	const String path = temp_file("absent.ini");
	{
		Object *gone = memnew(Object);
		XRSettingsServer *server = memnew(XRSettingsServer);
		server->set_file_path(path);
		server->set_interface_override(gone);
		memdelete(gone); // The override points at nothing: no interface at all.
		CHECK(server->get_interface() == nullptr);
		CHECK_FALSE(server->is_interface_ready());
		CHECK_FALSE(server->is_session_running());

		server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
		server->set_foveation_level(XRSettings::FOVEATION_LEVEL_HIGH);
		server->set_render_target_size_multiplier(1.25);
		server->set_display_refresh_rate(120.0);
		server->set_cpu_level(XRSettings::PERFORMANCE_LEVEL_SUSTAINED_HIGH);
		server->set_right_hand_motion_range(XRSettings::HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
		server->set_hand_tracking_enabled(false);
		CHECK(server->apply() == OK);

		const PackedStringArray deferred = server->get_deferred_setting_names();
		for (const char *name : { "foveation_level", "render_target_size_multiplier", "display_refresh_rate", "cpu_level", "right_hand_motion_range" }) {
			CHECK_MESSAGE(deferred.has(name), name);
		}
		CHECK_FALSE(deferred.has("hand_tracking_enabled"));
		// Unknown before an interface is initialized: nothing is reported as unsupported.
		CHECK(server->get_unsupported_setting_names().is_empty());
		CHECK(server->get_available_display_refresh_rates().is_empty());
		CHECK(server->save() == OK);
		memdelete(server);
	}

	XRSettingsServer *server = memnew(XRSettingsServer);
	server->set_file_path(path);
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	CHECK(server->load() == OK);
	CHECK(server->get_foveation_level() == XRSettings::FOVEATION_LEVEL_HIGH);
	CHECK(server->get_render_target_size_multiplier() == doctest::Approx(1.25));
	CHECK(server->get_display_refresh_rate() == doctest::Approx(120.0));
	CHECK(server->get_cpu_level() == XRSettings::PERFORMANCE_LEVEL_SUSTAINED_HIGH);
	CHECK(server->get_gpu_level() == XRSettings::PERFORMANCE_LEVEL_RUNTIME_DEFAULT);
	CHECK(server->get_right_hand_motion_range() == XRSettings::HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
	CHECK_FALSE(server->is_hand_tracking_enabled());
	memdelete(server);
}

TEST_CASE("[XRSettingsServer] Values wait for the interface and are pushed when its session begins") {
	TestSetup setup;
	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	xr_interface->refresh_rates.push_back(72.0);
	xr_interface->refresh_rates.push_back(90.0);
	xr_interface->refresh_rates.push_back(120.0);
	XRSettingsServer *server = create_server("deferred.ini", xr_interface);
	CHECK(server->get_interface() == xr_interface);
	CHECK_FALSE(server->is_interface_ready());

	server->set_foveation_level(XRSettings::FOVEATION_LEVEL_LOW);
	server->set_foveation_dynamic(true);
	server->set_render_target_size_multiplier(1.5);
	server->set_display_refresh_rate(90.0);
	server->set_vrs_strength(2.0);
	server->set_gpu_level(XRSettings::PERFORMANCE_LEVEL_BOOST);
	server->set_left_hand_motion_range(XRSettings::HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);

	// The multiplier goes to the interface before initialize(); the others wait.
	CHECK(xr_interface->render_target_size_multiplier == doctest::Approx(1.5));
	CHECK(xr_interface->foveation_level == 0);
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(0.0));
	CHECK(xr_interface->gpu_level == -1);
	PackedStringArray deferred = server->get_deferred_setting_names();
	CHECK_FALSE(deferred.has("render_target_size_multiplier"));
	for (const char *name : { "foveation_level", "foveation_dynamic", "display_refresh_rate", "vrs_strength", "gpu_level", "left_hand_motion_range" }) {
		CHECK_MESSAGE(deferred.has(name), name);
	}

	// The runtime starts its session.
	xr_interface->initialized = true;
	xr_interface->emit_signal(SNAME("session_begun"));
	CHECK(server->is_interface_ready());
	CHECK(server->is_session_running());
	CHECK(server->get_deferred_setting_names().is_empty());
	CHECK(xr_interface->foveation_level == 1);
	CHECK(xr_interface->foveation_dynamic);
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(90.0));
	CHECK(xr_interface->vrs_strength == doctest::Approx(2.0));
	CHECK(xr_interface->gpu_level == 3); // OpenXRInterface.PERF_SETTINGS_LEVEL_BOOST.
	CHECK(xr_interface->cpu_level == -1); // Runtime default: never pushed.
	CHECK(xr_interface->motion_range[0] == 1);
	CHECK(xr_interface->motion_range[1] == 0);

	// During the session, changes reach the interface at once.
	server->set_foveation_level(XRSettings::FOVEATION_LEVEL_HIGH);
	server->set_cpu_level(XRSettings::PERFORMANCE_LEVEL_POWER_SAVINGS);
	CHECK(xr_interface->foveation_level == 3);
	CHECK(xr_interface->cpu_level == 0);

	// A new session gets the values again.
	xr_interface->emit_signal(SNAME("session_stopping"));
	CHECK_FALSE(server->is_session_running());
	xr_interface->foveation_level = 0;
	xr_interface->emit_signal(SNAME("session_begun"));
	CHECK(xr_interface->foveation_level == 3);

	memdelete(server);
	memdelete(xr_interface);
}

TEST_CASE("[XRSettingsServer] The refresh rate is matched to the rates of the runtime") {
	TestSetup setup;
	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	xr_interface->initialized = true;
	xr_interface->refresh_rates.push_back(120.0);
	xr_interface->refresh_rates.push_back(72.0);
	xr_interface->refresh_rates.push_back(90.0);
	XRSettingsServer *server = create_server("refresh.ini", xr_interface);

	const PackedFloat64Array rates = server->get_available_display_refresh_rates();
	REQUIRE(rates.size() == 3);
	CHECK(rates[0] == doctest::Approx(72.0));
	CHECK(rates[2] == doctest::Approx(120.0));

	server->set_display_refresh_rate(100.0);
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(90.0));
	CHECK(server->get_display_refresh_rate() == doctest::Approx(100.0)); // The player's choice stays stored.
	server->set_display_refresh_rate(144.0);
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(120.0));
	server->set_display_refresh_rate(0.0); // Runtime default: not pushed.
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(120.0));

	memdelete(server);
	memdelete(xr_interface);
}

TEST_CASE("[XRSettingsServer] Values the runtime does not support are stored but not applied") {
	TestSetup setup;
	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	xr_interface->initialized = true;
	xr_interface->foveation_supported = false;
	xr_interface->hand_tracking_supported = false;
	XRSettingsServer *server = create_server("unsupported.ini", xr_interface);

	const PackedStringArray unsupported = server->get_unsupported_setting_names();
	CHECK(unsupported.size() == 7);
	for (const char *name : { "foveation_level", "foveation_dynamic", "foveation_with_subsampled_images", "display_refresh_rate", "left_hand_motion_range", "right_hand_motion_range", "hand_tracking_enabled" }) {
		CHECK_MESSAGE(unsupported.has(name), name);
		CHECK_FALSE(server->is_value_supported(name));
	}
	CHECK(server->is_value_supported("render_target_size_multiplier"));
	CHECK(server->is_value_supported("cpu_level"));

	ERR_PRINT_OFF;
	server->set_foveation_level(XRSettings::FOVEATION_LEVEL_HIGH);
	server->set_display_refresh_rate(90.0);
	server->set_left_hand_motion_range(XRSettings::HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
	ERR_PRINT_ON;
	CHECK(server->get_foveation_level() == XRSettings::FOVEATION_LEVEL_HIGH);
	CHECK(server->get_display_refresh_rate() == doctest::Approx(90.0));
	CHECK(xr_interface->foveation_level == 0);
	CHECK(xr_interface->display_refresh_rate == doctest::Approx(0.0));
	CHECK(xr_interface->motion_range[0] == 0);
	// Not deferred either: the runtime will not take them.
	CHECK(server->get_deferred_setting_names().is_empty());

	server->set_vrs_min_radius(40.0);
	CHECK(xr_interface->vrs_min_radius == doctest::Approx(40.0));

	memdelete(server);
	memdelete(xr_interface);
}

TEST_CASE("[XRSettingsServer] Reading the engine back during a session") {
	TestSetup setup;
	XRSettingsTestInterface *xr_interface = memnew(XRSettingsTestInterface);
	xr_interface->initialized = true;
	XRSettingsServer *server = create_server("read.ini", xr_interface);
	server->set_foveation_level(XRSettings::FOVEATION_LEVEL_LOW);
	xr_interface->emit_signal(SNAME("session_begun"));

	// The game changed the interface directly.
	xr_interface->foveation_level = 2;
	xr_interface->vrs_strength = 4.0;
	xr_interface->motion_range[1] = 1;
	server->read_from_engine();
	CHECK(server->get_foveation_level() == XRSettings::FOVEATION_LEVEL_MEDIUM);
	CHECK(server->get_vrs_strength() == doctest::Approx(4.0));
	CHECK(server->get_right_hand_motion_range() == XRSettings::HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
	// The refresh rate the runtime reports is not the player's choice.
	xr_interface->display_refresh_rate = 72.0;
	server->read_from_engine();
	CHECK(server->get_display_refresh_rate() == doctest::Approx(0.0));

	memdelete(server);
	memdelete(xr_interface);
}

TEST_CASE("[XRSettingsServer] Typed signals fire once per applied change, also while the value is deferred") {
	TestSetup setup;
	Object *gone = memnew(Object);
	XRSettingsServer *server = create_server("signals.ini", gone);
	memdelete(gone); // No interface: the value is stored and deferred, the signal still fires on apply().
	server->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);
	SIGNAL_WATCH(server, "foveation_level_changed");
	SIGNAL_WATCH(server, "hand_tracking_enabled_changed");

	// A pending change does not reach the engine: no signal.
	server->set_foveation_level(XRSettings::FOVEATION_LEVEL_LOW);
	SIGNAL_CHECK_FALSE("foveation_level_changed");

	CHECK(server->apply() == OK);
	Array level_call;
	level_call.push_back(int(XRSettings::FOVEATION_LEVEL_LOW));
	Array level_args;
	level_args.push_back(level_call);
	SIGNAL_CHECK("foveation_level_changed", level_args);
	CHECK(server->get_deferred_setting_names().has("foveation_level"));

	// Nothing changed: apply() emits nothing.
	CHECK(server->apply() == OK);
	SIGNAL_CHECK_FALSE("foveation_level_changed");

	// The game-only value has a signal too.
	server->set_hand_tracking_enabled(false);
	CHECK(server->apply() == OK);
	Array hand_call;
	hand_call.push_back(false);
	Array hand_args;
	hand_args.push_back(hand_call);
	SIGNAL_CHECK("hand_tracking_enabled_changed", hand_args);
	SIGNAL_CHECK_FALSE("foveation_level_changed");

	SIGNAL_UNWATCH(server, "foveation_level_changed");
	SIGNAL_UNWATCH(server, "hand_tracking_enabled_changed");
	memdelete(server);
}

} // namespace TestXRSettingsServer

#endif // MODULE_OPENXR_ENABLED && !XR_DISABLED
