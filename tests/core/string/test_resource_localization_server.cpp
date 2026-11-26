/**************************************************************************/
/*  test_resource_localization_server.cpp                                 */
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

TEST_FORCE_LINK(test_resource_localization_server)

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/string/localization_catalog.h"
#include "core/string/resource_localization_server.h"
#include "core/string/translation_server.h"
#include "scene/resources/image_texture.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestResourceLocalizationServer {

struct ServerState {
	ResourceLocalizationServer *server = ResourceLocalizationServer::get_singleton();
	String previous_locale = TranslationServer::get_singleton()->get_locale();

	~ServerState() {
		server->clear_catalogs();
		TranslationServer::get_singleton()->set_locale(previous_locale);
		MessageQueue::get_singleton()->flush();
	}
};

static String make_folder(const String &p_name) {
	const String folder = TestUtils::get_temp_path(p_name);
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	dir->make_dir_recursive(folder);
	return folder;
}

static void wait_for_relocalization(ResourceLocalizationServer *p_server) {
	for (int attempt = 0; attempt < 5000; attempt++) {
		MessageQueue::get_singleton()->flush();
		if (!p_server->is_relocalizing()) {
			return;
		}
		OS::get_singleton()->delay_usec(1000);
	}
}

TEST_CASE("[ResourceLocalizationServer] Paths, layers and locale matching") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	const String voice = "res://voice/intro.ogg";

	Ref<ResourceLocalizationCatalog> game;
	game.instantiate();
	game->set_localized_path(voice, "de", "res://voice/de/intro.ogg");
	game->set_localized_path(voice, "de_AT", "res://voice/de_AT/intro.ogg");
	game->set_localized_path(voice, "fr", "res://voice/fr/intro.ogg");
	Ref<ResourceLocalizationCatalog> mod;
	mod.instantiate();
	mod->set_localized_path(voice, "pl", "user://mods/polish/voice/intro.ogg");
	mod->set_localized_path(voice, "fr", "user://mods/polish/voice/intro_fr.ogg");
	server->add_catalog(game);
	server->add_catalog(mod, 10);

	CHECK(server->has_localization(voice));
	CHECK_FALSE(server->has_localization("res://voice/other.ogg"));
	CHECK(server->get_localized_path("res://voice/other.ogg", "de") == "res://voice/other.ogg");

	// The locale chain: `de_DE` -> `de` -> the closest other locale (`de_AT`) -> fallback.
	CHECK(server->get_localized_path(voice, "de_DE") == "res://voice/de/intro.ogg");
	CHECK(server->get_localized_path(voice, "de_AT") == "res://voice/de_AT/intro.ogg");
	CHECK(server->get_localized_path(voice, "pl") == "user://mods/polish/voice/intro.ogg");
	CHECK(server->get_localized_path(voice, "fr") == "user://mods/polish/voice/intro_fr.ogg"); // Higher layer.
	CHECK(server->get_localized_path(voice, "ja") == voice);

	game->set_localized_path(voice, "de_DE", "res://voice/de_DE/intro.ogg");
	MessageQueue::get_singleton()->flush(); // Catalog changes are applied deferred.
	CHECK(server->get_localized_path(voice, "de_DE") == "res://voice/de_DE/intro.ogg");

	// The active locale comes from the snapshot.
	TranslationServer::get_singleton()->set_locale("pl");
	CHECK(server->get_localized_path(voice) == "user://mods/polish/voice/intro.ogg");
	TranslationServer::get_singleton()->set_locale("de_CH");
	CHECK(server->get_localized_path(voice) == "res://voice/de/intro.ogg");

	const Dictionary paths = server->get_localized_paths(voice);
	CHECK(paths.size() == 5);
	CHECK(String(paths["fr"]) == "user://mods/polish/voice/intro_fr.ogg");
	CHECK(String(paths["de_AT"]) == "res://voice/de_AT/intro.ogg");

	const PackedStringArray locales = server->get_available_locales();
	CHECK(locales.size() == 5);
	game->set_localized_path("res://ui/logo.png", "de", "res://ui/logo_de.png");
	MessageQueue::get_singleton()->flush();
	const PackedStringArray missing = server->get_missing_locales("res://ui/logo.png");
	CHECK(missing.size() == 4);
	CHECK_FALSE(missing.has("de"));
	CHECK(server->get_missing_locales(voice).is_empty());
}

TEST_CASE("[ResourceLocalizationServer] Channels by type and a channel change") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	TranslationServer *translation_server = TranslationServer::get_singleton();
	translation_server->set_locale("en");

	Ref<ResourceLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_localized_path("res://voice/intro.ogg", "ja", "res://voice/ja/intro.ogg");
	catalog->set_expected_type("res://voice/intro.ogg", "AudioStreamOggVorbis");
	catalog->set_localized_path("res://ui/logo.png", "ja", "res://ui/logo_ja.png");
	catalog->set_expected_type("res://ui/logo.png", "Texture2D");
	CHECK(catalog->get_channel() == StringName()); // By type.
	server->add_catalog(catalog);

	// Only the audio channel in Japanese: the voice follows it, the logo stays with the general locale.
	translation_server->set_locale_for("audio", "ja");
	CHECK(server->get_localized_path("res://voice/intro.ogg") == "res://voice/ja/intro.ogg");
	CHECK(server->get_localized_path("res://ui/logo.png") == "res://ui/logo.png");
	translation_server->clear_locale_for("audio");
	CHECK(server->get_localized_path("res://voice/intro.ogg") == "res://voice/intro.ogg");

	// A fixed channel overrides the type.
	catalog->set_channel("interface");
	MessageQueue::get_singleton()->flush();
	translation_server->set_locale_for("interface", "ja");
	CHECK(server->get_localized_path("res://voice/intro.ogg") == "res://voice/ja/intro.ogg");
	CHECK(server->get_localized_path("res://ui/logo.png") == "res://ui/logo_ja.png");
	translation_server->clear_locale_for("interface");
}

TEST_CASE("[ResourceLocalizationServer] Same results as translation_remaps") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	const String folder = make_folder("resource_localization_remaps");
	const PackedStringArray files = { "logo.bin", "logo_de.bin", "logo_at.bin", "logo_pt.bin", "logo_en.bin", "intro.bin", "intro_fr.bin" };
	for (const String &file : files) {
		Ref<FileAccess> f = FileAccess::open(folder.path_join(file), FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_8(0);
	}
	const String logo = folder.path_join("logo.bin");
	const String intro = folder.path_join("intro.bin");
	Dictionary remaps;
	remaps[logo] = PackedStringArray({ folder.path_join("logo_de.bin") + ":de", folder.path_join("logo_at.bin") + ":de_AT", folder.path_join("logo_pt.bin") + ":pt" });
	remaps[intro] = PackedStringArray({ folder.path_join("intro_fr.bin") + ":fr", folder.path_join("intro_missing.bin") + ":ja" });

	const Variant previous_remaps = ProjectSettings::get_singleton()->get_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, Dictionary());
	ProjectSettings::get_singleton()->set_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, remaps);
	ResourceLoader::load_translation_remaps(); // Godot's table, for comparison.
	CHECK(server->load_project_catalogs() == 1); // The remaps, imported as a layer.
	CHECK(server->has_localization(logo));

	const PackedStringArray locales = { "en", "de", "de_DE", "de_AT", "de_CH", "pt_BR", "fr", "fr_CA", "ja", "zh_TW" };
	for (const String &locale : locales) {
		TranslationServer::get_singleton()->set_locale(locale);
		for (const String &path : PackedStringArray({ logo, intro })) {
			const ResourceLoader::PathLocalizer localizer = ResourceLoader::path_localizer;
			ResourceLoader::path_localizer = nullptr;
			ERR_PRINT_OFF;
			const String godot_path = ResourceLoader::path_remap(path);
			ERR_PRINT_ON;
			ResourceLoader::path_localizer = localizer;
			ERR_PRINT_OFF;
			const String server_path = ResourceLoader::path_remap(path);
			ERR_PRINT_ON;
			CHECK_MESSAGE(server_path == godot_path, vformat("%s %s", locale, path);
		}
	}
	// `ja` points to a missing file: both fall back to the original path.
	TranslationServer::get_singleton()->set_locale("ja");
	ERR_PRINT_OFF;
	CHECK(ResourceLoader::path_remap(intro) == intro);
	ERR_PRINT_ON;
	CHECK(server->get_localized_path(intro) == folder.path_join("intro_missing.bin")); // The catalog entry itself.

	// A project catalog overrides the imported remaps on the same priority.
	Ref<ResourceLocalizationCatalog> project;
	project.instantiate();
	project->set_localized_path(logo, "de", folder.path_join("logo_en.bin"));
	server->add_catalog(project);
	CHECK(server->get_localized_path(logo, "de") == folder.path_join("logo_en.bin"));

	ProjectSettings::get_singleton()->set_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, previous_remaps);
}

TEST_CASE("[ResourceLocalizationServer] load() and background relocalization") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	TranslationServer::get_singleton()->set_locale("en");
	const String folder = make_folder("resource_localization_load");
	for (const String &locale : PackedStringArray({ "en", "de", "pl" })) {
		Ref<Resource> resource;
		resource.instantiate();
		resource->set_name(locale);
		const String file = locale == "en" ? String("voice.tres") : vformat("voice_%s.tres", locale);
		REQUIRE(ResourceSaver::save(resource, folder.path_join(file)) == OK);
	}
	const String voice = folder.path_join("voice.tres");
	Ref<ResourceLocalizationCatalog> game;
	game.instantiate();
	game->set_localized_path(voice, "de", folder.path_join("voice_de.tres"));
	server->add_catalog(game);

	const Ref<Resource> loaded = ResourceLoader::load(voice);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_name() == "en");
	CHECK(loaded->get_path() == voice); // Cached under the original path.

	SIGNAL_WATCH(server, SNAME("resources_relocalized"));
	TranslationServer::get_singleton()->set_locale("de");
	wait_for_relocalization(server);
	CHECK(loaded->get_name() == "de"); // Updated in place.
	CHECK(ResourceLoader::load(voice) == loaded);
	Array relocalized;
	relocalized.push_back(Array({ PackedStringArray({ voice }) }));
	SIGNAL_CHECK("resources_relocalized", relocalized);

	// A mod adds Polish: the cached resource follows when the locale is switched.
	Ref<ResourceLocalizationCatalog> mod;
	mod.instantiate();
	mod->set_localized_path(voice, "pl", folder.path_join("voice_pl.tres"));
	server->add_catalog(mod, 100);
	TranslationServer::get_singleton()->set_locale("pl");
	wait_for_relocalization(server);
	CHECK(loaded->get_name() == "pl");
	SIGNAL_CHECK("resources_relocalized", relocalized);

	// Removing the mod goes back to the original resource; a locale change without new paths reloads nothing.
	server->remove_catalog(mod);
	wait_for_relocalization(server);
	CHECK(loaded->get_name() == "en");
	SIGNAL_DISCARD("resources_relocalized");
	TranslationServer::get_singleton()->set_locale("fr");
	wait_for_relocalization(server);
	SIGNAL_CHECK_FALSE("resources_relocalized");
	SIGNAL_UNWATCH(server, SNAME("resources_relocalized"));
}

// Relocalizing textures needs the main thread (ImageTexture::copy_from() reads the image back from the
// RenderingServer). A load() of a relocalized path on the main thread meanwhile must not wait for the workers.
TEST_CASE("[SceneTree][ResourceLocalizationServer] load() on the main thread during a relocalization") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	TranslationServer::get_singleton()->set_locale("en");
	const String folder = make_folder("resource_localization_textures");
	Ref<ResourceLocalizationCatalog> catalog;
	catalog.instantiate();
	PackedStringArray originals;
	for (int index = 0; index < 32; index++) {
		for (const String &locale : PackedStringArray({ "en", "de" })) {
			const int size = locale == "en" ? 64 : 32; // The dummy RenderingServer keeps no replaced image data.
			Ref<Image> image = Image::create_empty(size, size, false, Image::FORMAT_RGBA8);
			REQUIRE(ResourceSaver::save(ImageTexture::create_from_image(image), folder.path_join(vformat("%s_%d.res", locale, index))) == OK);
		}
		originals.push_back(folder.path_join(vformat("en_%d.res", index)));
		catalog->set_localized_path(originals[index], "de", folder.path_join(vformat("de_%d.res", index)));
	}
	server->add_catalog(catalog);
	MessageQueue::get_singleton()->flush();
	LocalVector<Ref<ImageTexture>> cached;
	for (const String &path : originals) {
		cached.push_back(Ref<ImageTexture>(ResourceLoader::load(path)));
		REQUIRE(cached[cached.size() - 1].is_valid());
	}

	TranslationServer::get_singleton()->set_locale("de");
	CHECK(server->is_relocalizing());
	for (int index = 0; index < originals.size(); index++) {
		CHECK(ResourceLoader::load(originals[index]).ptr() == cached[index].ptr()); // Hung here before (#344).
	}
	wait_for_relocalization(server);
	REQUIRE_FALSE(server->is_relocalizing());
	for (const Ref<ImageTexture> &texture : cached) {
		CHECK(texture->get_width() == 32);
	}
}

// Prints the cost of one path_remap() with 1000 remapped paths: Godot's table vs the server (both check that the
// localized file exists), and the server's lookup alone.
TEST_CASE("[ResourceLocalizationServer] Remap cost") {
	ServerState state;
	ResourceLocalizationServer *server = state.server;
	const String folder = make_folder("resource_localization_cost");
	const PackedStringArray locales = { "ar", "de", "es", "fr", "it", "ja", "ko", "pl", "pt", "ru", "tr", "zh" };
	Dictionary remaps;
	for (int index = 0; index < 1000; index++) {
		PackedStringArray paths;
		for (const String &locale : locales) {
			paths.push_back(folder.path_join(vformat("%s_%d.bin", locale, index)) + ":" + locale);
		}
		remaps[folder.path_join(vformat("file_%d.bin", index))] = paths;
	}
	const String path = folder.path_join("file_500.bin");
	for (const String &file : PackedStringArray({ "file_500.bin", "pl_500.bin" })) {
		Ref<FileAccess> f = FileAccess::open(folder.path_join(file), FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_8(0);
	}
	const Variant previous_remaps = ProjectSettings::get_singleton()->get_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, Dictionary());
	ProjectSettings::get_singleton()->set_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, remaps);
	ResourceLoader::load_translation_remaps();
	ProjectSettings::get_singleton()->set_setting(ResourceLocalizationServer::SETTING_TRANSLATION_REMAPS, previous_remaps);
	Ref<ResourceLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->add_translation_remaps(remaps);
	server->add_catalog(catalog);
	TranslationServer::get_singleton()->set_locale("pl_PL");

	constexpr int calls = 2000;
	int64_t length = 0; // Keeps the calls from being optimized away.
	const ResourceLoader::PathLocalizer localizer = ResourceLoader::path_localizer;
	ResourceLoader::path_localizer = nullptr;
	uint64_t start = OS::get_singleton()->get_ticks_usec();
	for (int call = 0; call < calls; call++) {
		length += ResourceLoader::path_remap(path).length();
	}
	const double godot_ns = (OS::get_singleton()->get_ticks_usec() - start) * 1000.0 / calls;
	const String godot_path = ResourceLoader::path_remap(path);
	ResourceLoader::path_localizer = localizer;
	start = OS::get_singleton()->get_ticks_usec();
	for (int call = 0; call < calls; call++) {
		length += ResourceLoader::path_remap(path).length();
	}
	const double server_ns = (OS::get_singleton()->get_ticks_usec() - start) * 1000.0 / calls;
	start = OS::get_singleton()->get_ticks_usec();
	for (int call = 0; call < calls; call++) {
		length += server->get_localized_path(path).length();
	}
	const double lookup_ns = (OS::get_singleton()->get_ticks_usec() - start) * 1000.0 / calls;

	CHECK(length > 0);
	CHECK(ResourceLoader::path_remap(path) == godot_path);
	CHECK(godot_path == folder.path_join("pl_500.bin"));
	MESSAGE(vformat("path_remap with 1000 remaps x 12 locales: Godot %.0f ns, server %.0f ns, server lookup alone %.0f ns", godot_ns, server_ns, lookup_ns));
}

} // namespace TestResourceLocalizationServer
