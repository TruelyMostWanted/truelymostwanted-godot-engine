/**************************************************************************/
/*  test_localization_catalog.cpp                                         */
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

TEST_FORCE_LINK(test_localization_catalog)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/localization_catalog.h"
#include "core/string/translation.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestLocalizationCatalog {

static const char *PLURAL_RULE = "nplurals=2; plural=(n != 1);";

// The rules of ResourceImporterCSVTranslation: `?context`, `?plural` + continuation rows, `_` columns ignored,
// `?pluralrule` row, translations c_unescape()d (keys not), rows without a key skipped.
static Ref<TextLocalizationCatalog> make_text_catalog() {
	const String csv = String(U"keys,?context,?plural,_notes,en,de\n"
							  U"HELLO,,,greeting,Hello,Hallo\n"
							  U"HELLO,menu,,,Hello (menu),Hallo (Menü)\n"
							  U"?pluralrule,,,,,nplurals=2; plural=(n != 1);\n"
							  U"APPLE,,APPLES,,%d apple,%d Apfel\n"
							  U",,,,%d apples,%d Äpfel\n"
							  U"LINE,,,,One\\nTwo,Eins\\nZwei\n"
							  U",,,,ignored row,\n"
							  U"BYE,,,,Bye,\n");
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	REQUIRE(catalog->load_from_text(csv, ResourceTextSerializer::FORMAT_CSV) == OK);
	return catalog;
}

TEST_CASE("[LocalizationCatalog] CSV follows the importer rules") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	CHECK(catalog->get_key_count() == 5);
	CHECK(catalog->get_locales() == PackedStringArray({ "en", "de" }));
	CHECK(catalog->get_text("HELLO", "de") == "Hallo");
	CHECK(catalog->get_text("HELLO", "de", "menu") == String(U"Hallo (Menü)"));
	CHECK(catalog->get_text("LINE", "en") == "One\nTwo");
	CHECK(catalog->get_plural_key("APPLE") == "APPLES");
	CHECK(catalog->get_plural_texts("APPLE", "de") == PackedStringArray({ String(U"%d Apfel"), String(U"%d Äpfel") }));
	CHECK(catalog->get_plural_text("APPLE", "de", 1) == String(U"%d Apfel"));
	CHECK(catalog->get_plural_text("APPLE", "de", 5) == String(U"%d Äpfel"));
	CHECK(catalog->get_plural_rule("de") == PLURAL_RULE);
	CHECK(catalog->get_text("BYE", "de").is_empty());
	CHECK(catalog->get_translated_count("de") == 4);
	CHECK(catalog->get_missing_rows("de") == PackedInt32Array({ 4 }));
	CHECK(catalog->get_key(1) == "HELLO");
	CHECK(catalog->get_key_context(1) == "menu");
	CHECK(catalog->find_key("HELLO", "menu") == 1);
	CHECK_FALSE(catalog->has_key("HELLO", "other"));
}

TEST_CASE("[LocalizationCatalog] Keys, locales and cells") {
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_text("A", "en", "a");
	catalog->set_text("B", "en", "b");
	catalog->set_text("C", "de", "c");
	catalog->set_text("B", "en", "a much longer text"); // Grows a cell in the middle.
	CHECK(catalog->get_text("A", "en") == "a");
	CHECK(catalog->get_text("B", "en") == "a much longer text");
	CHECK(catalog->get_text("C", "de") == "c");
	CHECK(catalog->get_translated_count("en") == 2);

	catalog->remove_key("A");
	CHECK(catalog->get_key_count() == 2);
	CHECK(catalog->find_key("B") == 0);
	CHECK(catalog->get_text("B", "en") == "a much longer text");
	CHECK(catalog->get_translated_count("en") == 1);

	catalog->set_text("B", "en", "");
	CHECK(catalog->get_translated_count("en") == 0);
	catalog->remove_locale("en");
	CHECK(catalog->get_locales() == PackedStringArray({ "de" }));

	// Many keys: the index grows.
	for (int i = 0; i < 1000; i++) {
		catalog->set_text(vformat("KEY_%d", i), "de", itos(i));
	}
	CHECK(catalog->get_text("KEY_777", "de") == "777");
	CHECK(catalog->get_text("C", "de") == "c");
}

TEST_CASE("[LocalizationCatalog] Text formats round trip") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const Variant data = catalog->get("data");
	for (int format = 0; format < ResourceTextSerializer::FORMAT_MAX; format++) {
		const char *format_classes[] = { "JSON", "INI", "CSV", "XML", "TOML", "YAML" };
		if (!ClassDB::class_exists(format_classes[format])) {
			continue; // Module disabled.
		}
		CAPTURE(format_classes[format]);
		const String text = catalog->to_text(ResourceTextSerializer::Format(format));
		REQUIRE_FALSE(text.is_empty());
		Ref<TextLocalizationCatalog> copy;
		copy.instantiate();
		REQUIRE(copy->load_from_text(text, ResourceTextSerializer::Format(format)) == OK);
		CHECK(copy->get("data") == data);
		CHECK(copy->get_plural_text("APPLE", "de", 3) == String(U"%d Äpfel"));
	}

	// `.tres`/`.res` store the `data` property.
	Ref<TextLocalizationCatalog> duplicate = catalog->duplicate();
	CHECK(duplicate->get_text("HELLO", "de", "menu") == String(U"Hallo (Menü)"));
}

TEST_CASE("[LocalizationCatalog] Binary format loads one locale at a time") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const String path = TestUtils::get_temp_path("localization_catalog.lcat");
	REQUIRE(catalog->save_to_file(path) == OK);

	Ref<TextLocalizationCatalog> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_key_count() == 5);
	CHECK_FALSE(loaded->is_locale_loaded("en"));
	CHECK_FALSE(loaded->is_locale_loaded("de"));
	CHECK(loaded->get_translated_count("de") == 4); // From the locale table.

	SIGNAL_WATCH(loaded.ptr(), SNAME("locale_loaded"));
	CHECK(loaded->get_text("HELLO", "de", "menu") == String(U"Hallo (Menü)"));
	SIGNAL_CHECK("locale_loaded", { { "de" } });
	SIGNAL_UNWATCH(loaded.ptr(), SNAME("locale_loaded"));
	CHECK(loaded->is_locale_loaded("de"));
	CHECK_FALSE(loaded->is_locale_loaded("en"));
	CHECK(loaded->get_plural_texts("APPLE", "de").size() == 2);

	loaded->unload_locale("de");
	CHECK_FALSE(loaded->is_locale_loaded("de"));
	CHECK(loaded->load_locale("de") == OK);
	CHECK(loaded->get("data") == catalog->get("data"));

	// A changed catalog no longer reads from the file.
	loaded->set_text("NEW", "en", "new");
	CHECK(loaded->is_locale_loaded("en"));
	CHECK(loaded->get_text("BYE", "en") == "Bye");

	// The wrong kind is refused.
	Ref<ResourceLocalizationCatalog> wrong;
	wrong.instantiate();
	ERR_PRINT_OFF;
	CHECK(wrong->load_from_file(path) == ERR_INVALID_DATA);
	ERR_PRINT_ON;
	DirAccess::remove_absolute(path);
}

TEST_CASE("[LocalizationCatalog] Conversion from and to Translation") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const TypedArray<Translation> translations = catalog->to_translations();
	REQUIRE(translations.size() == 2);
	Ref<Translation> german = translations[1];
	CHECK(german->get_locale() == "de");
	CHECK(german->get_message("HELLO") == "Hallo");
	CHECK(german->get_message("HELLO", "menu") == String(U"Hallo (Menü)"));
	CHECK(german->get_plural_message("APPLE", "APPLES", 5) == String(U"%d Äpfel"));
	CHECK(german->get_plural_rules_override() == PLURAL_RULE);

	Ref<TextLocalizationCatalog> back;
	back.instantiate();
	for (const Variant &translation : translations) {
		back->add_translation(translation);
	}
	CHECK(back->get_text("HELLO", "de", "menu") == String(U"Hallo (Menü)"));
	CHECK(back->get_plural_texts("APPLE", "de").size() == 2);
	CHECK(back->get_plural_rule("de") == PLURAL_RULE);
	CHECK(back->get_text("LINE", "en") == "One\nTwo");
}

TEST_CASE("[LocalizationCatalog] Resource catalog and translation_remaps") {
	Dictionary remaps;
	remaps["res://icon.png"] = PackedStringArray({ "res://icon_de.png:de", "res://icon_fr.png:fr" });
	remaps["res://music.ogg"] = PackedStringArray({ "res://music_de.ogg:de" });
	Ref<ResourceLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->add_translation_remaps(remaps);
	CHECK(catalog->get_localized_path("res://icon.png", "fr") == "res://icon_fr.png");
	CHECK(catalog->get_localized_path("res://music.ogg", "fr").is_empty());
	CHECK(catalog->to_translation_remaps() == remaps);

	catalog->set_expected_type("res://icon.png", "Texture2D");
	CHECK(catalog->get_expected_type("res://icon.png") == "Texture2D");
	for (const ResourceTextSerializer::Format format : { ResourceTextSerializer::FORMAT_CSV, ResourceTextSerializer::FORMAT_JSON, ResourceTextSerializer::FORMAT_XML }) {
		Ref<ResourceLocalizationCatalog> copy;
		copy.instantiate();
		REQUIRE(copy->load_from_text(catalog->to_text(format), format) == OK);
		CHECK(copy->get("data") == catalog->get("data"));
		CHECK(copy->get_expected_type("res://icon.png") == "Texture2D");
	}
}

TEST_CASE("[LocalizationCatalog] List with priorities and wildcards") {
	const String folder = TestUtils::get_temp_path("localization_catalog_list");
	REQUIRE(DirAccess::make_dir_recursive_absolute(folder) == OK);
	for (const char *file_name : { "b.json", "a.lcat", "a.lcat.uid", "notes.txt" }) {
		Ref<FileAccess> file = FileAccess::open(folder.path_join(file_name), FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{}");
	}
	Ref<LocalizationCatalogList> list;
	list.instantiate();
	list->add_entry(folder.path_join("*"), 10);
	list->add_entry("res://base.csv");
	list->add_entry(folder.path_join("*.json"), 10);
	list->add_entry(folder.path_join("missing/*"));
	CHECK(list->get_catalog_paths() == PackedStringArray({ "res://base.csv", folder.path_join("a.lcat"), folder.path_join("b.json"), folder.path_join("b.json") }));

	// Inspector properties.
	CHECK(list->get("entry_1/path") == "res://base.csv");
	list->set("entry_0/priority", -1);
	CHECK(list->get_entry_priority(0) == -1);
	CHECK(list->get_catalog_paths()[0] == folder.path_join("a.lcat"));
	list->remove_entry(0);
	CHECK(list->get_entry_count() == 3);

	for (const char *file_name : { "b.json", "a.lcat", "a.lcat.uid", "notes.txt" }) {
		DirAccess::remove_absolute(folder.path_join(file_name));
	}
	DirAccess::remove_absolute(folder);
}

TEST_CASE("[LocalizationCatalog] Memory per message") {
	// The data of the #340 baseline: keys "UI_KEY_00000", ~57 bytes of text per message, 12 locales.
	const char *locales[] = { "en", "de", "fr", "es", "it", "pt", "ru", "ja", "zh", "ko", "pl", "nl" };
	const int key_count = 5000;
	const int message_count = key_count * 12;
	const String path = TestUtils::get_temp_path("localization_catalog_memory.lcat");
	uint64_t text_bytes = 0;
	{
		Ref<TextLocalizationCatalog> catalog;
		catalog.instantiate();
		for (const char *locale : locales) {
			catalog->add_locale(locale);
		}
		for (int i = 0; i < key_count; i++) {
			const String key = vformat("UI_KEY_%05d", i);
			for (const char *locale : locales) {
				const String text = vformat("%s text %d: the quick brown fox jumps over the lazy dog", locale, i);
				text_bytes += text.length();
				catalog->set_text(key, locale, text);
			}
		}
		REQUIRE(catalog->save_to_file(path) == OK);
	}

	const uint64_t before = Memory::get_mem_usage();
	Ref<TextLocalizationCatalog> loaded = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	const uint64_t keys_only = Memory::get_mem_usage();
	for (const char *locale : locales) {
		REQUIRE(loaded->load_locale(locale) == OK);
	}
	const uint64_t after = Memory::get_mem_usage();
	CHECK(loaded->get_text("UI_KEY_04999", "nl") == "nl text 4999: the quick brown fox jumps over the lazy dog");

	if (after > before) { // Memory is only tracked with DEBUG_ENABLED.
		const double bytes_per_message = double(after - before) / message_count;
		MESSAGE(vformat("LocalizationCatalog: %.1f bytes per message (%.1f bytes of text), keys only %d bytes; Translation (#340) ~380, OptimizedTranslation ~72.", bytes_per_message, double(text_bytes) / message_count, keys_only - before));
		CHECK(bytes_per_message < 100);
	}
	DirAccess::remove_absolute(path);
}

} // namespace TestLocalizationCatalog
