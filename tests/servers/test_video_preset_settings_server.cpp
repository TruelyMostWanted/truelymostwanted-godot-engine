/**************************************************************************/
/*  test_video_preset_settings_server.cpp                                 */
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

TEST_FORCE_LINK(test_video_preset_settings_server)

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "servers/settings/settings_server_manager.h"
#include "servers/settings/video_effect_settings_server.h"
#include "servers/settings/video_preset.h"
#include "servers/settings/video_preset_settings.h"
#include "servers/settings/video_preset_settings_server.h"
#include "servers/settings/video_quality_settings_server.h"
#include "servers/settings/video_shadow_settings_server.h"
#include "servers/settings/video_texture_settings_server.h"
#include "tests/test_utils.h"

namespace TestVideoPresetSettingsServer {

// Autosave off; the project settings a test changes are restored afterwards.
struct TestSetup {
	TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", false);
	}
	~TestSetup() {
		ProjectSettings::get_singleton()->set_setting("settings/storage/autosave", true);
		ProjectSettings::get_singleton()->set_setting("settings/video_preset/presets", PackedStringArray());
		ProjectSettings::get_singleton()->set_setting("settings/video_preset/use_builtin_presets", true);
	}
};

static String temp_file(const String &p_name) {
	const String path = TestUtils::get_temp_path("video_preset_settings_server").path_join(p_name);
	DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	return path;
}

// A preset server with its own four video domains (not the registered engine singletons).
struct Servers {
	VideoPresetSettingsServer *preset = nullptr;
	SettingsServer *domains[4] = {};

	explicit Servers(const String &p_suffix = String(), bool p_remove_files = true) {
		domains[0] = memnew(VideoQualitySettingsServer);
		domains[1] = memnew(VideoTextureSettingsServer);
		domains[2] = memnew(VideoShadowSettingsServer);
		domains[3] = memnew(VideoEffectSettingsServer);
		preset = memnew(VideoPresetSettingsServer);
		preset->set_file_path(_path("video_preset" + p_suffix + ".ini", p_remove_files));
		for (SettingsServer *domain : domains) {
			domain->set_file_path(_path(String(domain->get_domain()) + p_suffix + ".ini", p_remove_files));
			preset->set_domain_server(domain->get_domain(), domain);
		}
	}
	~Servers() {
		memdelete(preset);
		for (SettingsServer *domain : domains) {
			memdelete(domain);
		}
	}

	SettingsServer *get(const StringName &p_domain) const {
		for (SettingsServer *domain : domains) {
			if (domain->get_domain() == p_domain) {
				return domain;
			}
		}
		return nullptr;
	}

	static String _path(const String &p_name, bool p_remove) {
		if (p_remove) {
			return temp_file(p_name);
		}
		return TestUtils::get_temp_path("video_preset_settings_server").path_join(p_name);
	}
};

static bool preset_value_equal(const Variant &p_a, const Variant &p_b) {
	if ((p_a.get_type() == Variant::INT || p_a.get_type() == Variant::FLOAT) && (p_b.get_type() == Variant::INT || p_b.get_type() == Variant::FLOAT)) {
		return Math::is_equal_approx(float(double(p_a)), float(double(p_b)));
	}
	return p_a == p_b;
}

// True if every supported value of `p_preset` is set in the domains of `p_servers`.
static bool domains_have_preset(const Servers &p_servers, const Ref<VideoPreset> &p_preset) {
	for (const String &domain : VideoPreset::get_domains()) {
		const SettingsServer *server = p_servers.get(domain);
		const Dictionary values = p_preset->get_domain_values(domain);
		for (const KeyValue<Variant, Variant> &kv : values) {
			const StringName setting_name = kv.key;
			if (server->is_value_supported(setting_name) && !preset_value_equal(server->get_value(setting_name), kv.value)) {
				return false;
			}
		}
	}
	return true;
}

static int preset_changed_count = 0;
static String preset_changed_name;

static void on_preset_changed(const String &p_name) {
	preset_changed_count++;
	preset_changed_name = p_name;
}

TEST_CASE("[VideoPresetSettingsServer] The domain is an engine singleton registered with the manager") {
	VideoPresetSettingsServer *server = VideoPresetSettingsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("VideoPresetSettingsServer"));
	CHECK(SettingsServerManager::get_singleton()->get_server("video_preset") == server);
	CHECK(server->get_domain() == StringName("video_preset"));
	CHECK(server->get_apply_phase() == SettingsServer::APPLY_PHASE_SCENE);
	CHECK(Object::cast_to<VideoPresetSettings>(server->get_settings().ptr()) != nullptr);
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/video_preset/presets"));
	CHECK(ProjectSettings::get_singleton()->has_setting("settings/video_preset/use_builtin_presets"));
}

TEST_CASE("[VideoPresetSettingsServer] Built-in presets") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;

	CHECK(VideoPresetSettingsServer::get_custom_preset_name() == "Custom");
	const PackedStringArray builtin = VideoPresetSettingsServer::get_builtin_preset_names();
	REQUIRE(builtin.size() == 4);
	CHECK(builtin[0] == "Low");
	CHECK(builtin[3] == "Ultra");
	CHECK(server->get_preset_names() == builtin);
	CHECK(server->get_presets().size() == 4);

	const Ref<VideoPreset> low = VideoPresetSettingsServer::create_builtin_preset("Low");
	const Ref<VideoPreset> ultra = VideoPresetSettingsServer::create_builtin_preset("Ultra");
	REQUIRE(low.is_valid());
	REQUIRE(ultra.is_valid());
	CHECK(low->get_preset_name() == "Low");
	// Every domain gets values; the values grow from Low to Ultra.
	for (const String &domain : VideoPreset::get_domains()) {
		CHECK_FALSE(low->get_domain_values(domain).is_empty());
	}
	CHECK(int(low->get_value("video_texture", "anisotropic_filtering_level")) < int(ultra->get_value("video_texture", "anisotropic_filtering_level")));
	CHECK(int(low->get_value("video_shadow", "directional_shadow_atlas_size")) < int(ultra->get_value("video_shadow", "directional_shadow_atlas_size")));
	CHECK(int(low->get_value("video_effect", "ssao_quality")) < int(ultra->get_value("video_effect", "ssao_quality")));

	// Every value of a built-in preset exists in its domain.
	for (const String &domain : VideoPreset::get_domains()) {
		const SettingsServer *domain_server = servers.get(domain);
		for (const KeyValue<Variant, Variant> &kv : ultra->get_domain_values(domain)) {
			CHECK_MESSAGE(domain_server->has_setting(kv.key), vformat("%s/%s", domain, kv.key));
		}
	}

	ERR_PRINT_OFF;
	CHECK(VideoPresetSettingsServer::create_builtin_preset("Custom").is_null());
	CHECK(VideoPresetSettingsServer::create_builtin_preset("Potato").is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[VideoPresetSettingsServer] apply_preset() sets every value of the four domains in one step") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;
	// One domain in manual mode keeps the values pending; the others apply them at once.
	SettingsServer *texture = servers.get("video_texture");
	texture->set_apply_mode(SettingsServer::APPLY_MODE_MANUAL);

	preset_changed_count = 0;
	server->connect("preset_changed", callable_mp_static(&on_preset_changed));

	ERR_PRINT_OFF;
	CHECK(server->apply_preset("Low") == OK);
	ERR_PRINT_ON;
	CHECK(domains_have_preset(servers, server->get_preset("Low")));
	CHECK(server->get_current_preset() == "Low");
	CHECK(server->get_value("preset") == Variant("Low"));
	CHECK_FALSE(server->is_custom());
	CHECK(server->matches_preset("Low"));
	CHECK_FALSE(server->matches_preset("Ultra"));
	CHECK(preset_changed_count >= 1);
	CHECK(preset_changed_name == "Low");

	CHECK_FALSE(servers.get("video_quality")->is_dirty());
	CHECK_FALSE(servers.get("video_shadow")->is_dirty());
	CHECK_FALSE(servers.get("video_effect")->is_dirty());
	CHECK(texture->is_dirty());
	CHECK(texture->get_apply_mode() == SettingsServer::APPLY_MODE_MANUAL);
	CHECK(servers.get("video_quality")->get_apply_mode() == SettingsServer::APPLY_MODE_IMMEDIATE);

	ERR_PRINT_OFF;
	CHECK(server->apply_preset("Ultra") == OK);
	ERR_PRINT_ON;
	CHECK(domains_have_preset(servers, server->get_preset("Ultra")));
	CHECK(server->get_current_preset() == "Ultra");
	CHECK(preset_changed_name == "Ultra");

	ERR_PRINT_OFF;
	CHECK(server->apply_preset("Custom") == ERR_INVALID_PARAMETER);
	CHECK(server->apply_preset("Potato") == ERR_DOES_NOT_EXIST);
	ERR_PRINT_ON;
	CHECK(server->get_current_preset() == "Ultra");
}

TEST_CASE("[VideoPresetSettingsServer] Values the renderer does not support keep their value") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;
	const Ref<VideoPreset> low = server->get_preset("Low");

	HashMap<String, Variant> before;
	for (const String &domain : VideoPreset::get_domains()) {
		const SettingsServer *domain_server = servers.get(domain);
		for (const KeyValue<Variant, Variant> &kv : low->get_domain_values(domain)) {
			before[domain + "/" + String(kv.key)] = domain_server->get_value(kv.key);
		}
	}
	ERR_PRINT_OFF;
	CHECK(server->apply_preset("Low") == OK);
	ERR_PRINT_ON;
	for (const String &domain : VideoPreset::get_domains()) {
		const SettingsServer *domain_server = servers.get(domain);
		for (const KeyValue<Variant, Variant> &kv : low->get_domain_values(domain)) {
			const Variant value = domain_server->get_value(kv.key);
			if (domain_server->is_value_supported(kv.key)) {
				CHECK_MESSAGE(preset_value_equal(value, kv.value), vformat("%s/%s", domain, kv.key));
			} else {
				CHECK_MESSAGE(preset_value_equal(value, before[domain + "/" + String(kv.key)]), vformat("%s/%s", domain, kv.key));
			}
		}
	}
}

TEST_CASE("[VideoPresetSettingsServer] A single change gives Custom; saving and loading keeps it") {
	TestSetup setup;
	{
		Servers servers("_custom");
		VideoPresetSettingsServer *server = servers.preset;
		SettingsServer *shadow = servers.get("video_shadow");
		ERR_PRINT_OFF;
		CHECK(server->apply_preset("High") == OK);
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "High");

		// A value the preset doesn't set changes nothing.
		ERR_PRINT_OFF;
		shadow->set_value("positional_shadow_atlas_quad_0", int(Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_16));
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "High");

		// A value of the preset changed: "Custom".
		ERR_PRINT_OFF;
		shadow->set_value("directional_shadow_atlas_size", 2048);
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "Custom");
		CHECK(server->is_custom());
		CHECK(server->get_value("preset") == Variant("Custom"));
		CHECK(server->detect_preset() == "Custom");
		// Changed back to the preset value: the preset again.
		ERR_PRINT_OFF;
		shadow->set_value("directional_shadow_atlas_size", 4096);
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "High");

		ERR_PRINT_OFF;
		shadow->set_value("directional_shadow_atlas_size", 2048);
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "Custom");

		// save() writes the applied values (all domains are in APPLY_MODE_IMMEDIATE here).
		for (SettingsServer *domain : servers.domains) {
			CHECK(domain->save() == OK);
		}
		CHECK(server->save() == OK);
	}

	Servers servers("_custom", false);
	VideoPresetSettingsServer *server = servers.preset;
	ERR_PRINT_OFF;
	for (SettingsServer *domain : servers.domains) {
		domain->load();
	}
	CHECK(server->load() == OK);
	ERR_PRINT_ON;
	CHECK(server->get_current_preset() == "Custom");
	CHECK(int(servers.get("video_shadow")->get_value("directional_shadow_atlas_size")) == 2048);
	CHECK(int(servers.get("video_shadow")->get_value("positional_shadow_atlas_quad_0")) == int(Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_16));
}

TEST_CASE("[VideoPresetSettingsServer] Without a picked preset, the values decide") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;
	CHECK(server->get_value("preset") == Variant(""));

	// The values of "Medium" set by hand, value by value.
	const Ref<VideoPreset> medium = server->get_preset("Medium");
	ERR_PRINT_OFF;
	for (const String &domain : VideoPreset::get_domains()) {
		SettingsServer *domain_server = servers.get(domain);
		for (const KeyValue<Variant, Variant> &kv : medium->get_domain_values(domain)) {
			domain_server->set_value(kv.key, kv.value);
		}
	}
	ERR_PRINT_ON;
	CHECK(server->get_current_preset() == "Medium");
	CHECK(server->detect_preset() == "Medium");
	CHECK(server->get_value("preset") == Variant(""));
}

TEST_CASE("[VideoPresetSettingsServer] Projects replace and add presets") {
	TestSetup setup;
	const String high_path = temp_file("project_high.tres");
	const String potato_path = temp_file("project_potato.tres");

	Ref<VideoPreset> high;
	high.instantiate();
	high->set_preset_name("High");
	high->set_value("video_shadow", "directional_shadow_atlas_size", 16384);
	CHECK(ResourceSaver::save(high, high_path) == OK);

	Ref<VideoPreset> potato;
	potato.instantiate();
	potato->set_preset_name("Potato");
	potato->set_value("video_texture", "anisotropic_filtering_level", 0);
	potato->set_value("video_quality", "scaling_3d_scale", 0.5);
	CHECK(ResourceSaver::save(potato, potato_path) == OK);

	PackedStringArray paths;
	paths.push_back(high_path);
	paths.push_back(potato_path);
	ProjectSettings::get_singleton()->set_setting("settings/video_preset/presets", paths);

	{
		Servers servers;
		VideoPresetSettingsServer *server = servers.preset;
		const PackedStringArray names = server->get_preset_names();
		REQUIRE(names.size() == 5);
		CHECK(names[2] == "High"); // Replaced in place.
		CHECK(names[4] == "Potato");
		CHECK(int(server->get_preset("High")->get_value("video_shadow", "directional_shadow_atlas_size")) == 16384);
		CHECK_FALSE(server->get_preset("High")->has_value("video_effect", "ssao_quality"));

		ERR_PRINT_OFF;
		CHECK(server->apply_preset("Potato") == OK);
		ERR_PRINT_ON;
		CHECK(server->get_current_preset() == "Potato");
		CHECK(int(servers.get("video_texture")->get_value("anisotropic_filtering_level")) == 0);
		CHECK(float(servers.get("video_quality")->get_value("scaling_3d_scale")) == doctest::Approx(0.5));
	}

	ProjectSettings::get_singleton()->set_setting("settings/video_preset/use_builtin_presets", false);
	{
		Servers servers;
		const PackedStringArray names = servers.preset->get_preset_names();
		REQUIRE(names.size() == 2);
		CHECK(names[0] == "High");
		CHECK(names[1] == "Potato");
	}
}

TEST_CASE("[VideoPresetSettingsServer] Presets are added and removed at runtime") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;

	Ref<VideoPreset> potato;
	potato.instantiate();
	potato->set_preset_name("Potato");
	potato->set_value("video_effect", "allow_glow", false);
	server->add_preset(potato);
	CHECK(server->has_preset("Potato"));
	CHECK(server->get_preset("Potato") == potato);

	server->remove_preset("Ultra");
	CHECK_FALSE(server->has_preset("Ultra"));
	CHECK(server->get_preset_names().size() == 4);

	Ref<VideoPreset> custom;
	custom.instantiate();
	custom->set_preset_name("Custom");
	ERR_PRINT_OFF;
	server->add_preset(custom);
	server->remove_preset("Ultra");
	ERR_PRINT_ON;
	CHECK_FALSE(server->has_preset("Custom"));

	server->reload_presets();
	CHECK(server->has_preset("Ultra"));
	CHECK_FALSE(server->has_preset("Potato"));
}

TEST_CASE("[VideoPresetSettingsServer] suggest_preset() names an existing preset") {
	TestSetup setup;
	Servers servers;
	VideoPresetSettingsServer *server = servers.preset;
	const String suggestion = server->suggest_preset();
	CHECK(server->has_preset(suggestion));
	CHECK(suggestion != "Ultra");
}

TEST_CASE("[VideoPreset] Values per domain") {
	Ref<VideoPreset> preset;
	preset.instantiate();
	CHECK(VideoPreset::get_domains().size() == 4);
	preset->set_value("video_quality", "msaa_3d", 2);
	CHECK(preset->has_value("video_quality", "msaa_3d"));
	CHECK(int(preset->get_value("video_quality", "msaa_3d")) == 2);
	CHECK(preset->get_value("video_quality", "use_taa", true) == Variant(true));
	CHECK(preset->get_video_quality().size() == 1);
	preset->erase_value("video_quality", "msaa_3d");
	CHECK_FALSE(preset->has_value("video_quality", "msaa_3d"));

	Dictionary values;
	values["ssao_quality"] = 1;
	preset->set_domain_values("video_effect", values);
	CHECK(preset->get_video_effect().size() == 1);
	preset->clear();
	CHECK(preset->get_video_effect().is_empty());

	ERR_PRINT_OFF;
	preset->set_value("video_window", "mode", 0);
	CHECK(preset->get_domain_values("video_window").is_empty());
	ERR_PRINT_ON;
	CHECK_FALSE(preset->has_value("video_window", "mode"));
}

} // namespace TestVideoPresetSettingsServer
