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
#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/string/localization_catalog.h"
#include "core/string/plural_rules.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"
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
	CHECK(catalog->get_note("HELLO") == "greeting");
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

	// Renaming keeps the row and its texts (#391).
	const int row = catalog->find_key("KEY_5");
	catalog->rename_key("KEY_5", "KEY_FIVE", StringName(), "ctx");
	CHECK(catalog->find_key("KEY_FIVE", "ctx") == row);
	CHECK_FALSE(catalog->has_key("KEY_5"));
	CHECK(catalog->get_text("KEY_FIVE", "de", "ctx") == "5");
	ERR_PRINT_OFF;
	catalog->rename_key("KEY_FIVE", "KEY_6", "ctx"); // Exists already.
	ERR_PRINT_ON;
	CHECK(catalog->find_key("KEY_FIVE", "ctx") == row);
}

TEST_CASE("[LocalizationCatalog] Note columns without notes are kept") {
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->set_text("A", "en", "a");
	catalog->set_note("A", "", StringName(), "max_length");
	REQUIRE(catalog->get_note_columns() == PackedStringArray({ "max_length" }));
	Ref<TextLocalizationCatalog> copy;
	copy.instantiate();
	REQUIRE(copy->load_from_text(catalog->to_text(ResourceTextSerializer::FORMAT_JSON), ResourceTextSerializer::FORMAT_JSON) == OK);
	CHECK(copy->get_note_columns() == PackedStringArray({ "max_length" }));
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
	MessageQueue::get_singleton()->flush(); // Deferred when not on the main thread.
	SIGNAL_CHECK("locale_loaded", { { "de" } });
	SIGNAL_UNWATCH(loaded.ptr(), SNAME("locale_loaded"));
	CHECK(loaded->is_locale_loaded("de"));
	CHECK_FALSE(loaded->is_locale_loaded("en"));
	CHECK(loaded->get_plural_texts("APPLE", "de").size() == 2);

	loaded->unload_locale("de");
	CHECK_FALSE(loaded->is_locale_loaded("de"));
	CHECK(loaded->load_locale("de") == OK);
	// Translator notes are not stored in the binary format.
	CHECK(loaded->get_note_columns().is_empty());
	catalog->set_note("HELLO", String());
	// Without notes, the kept (empty) note column is the only difference.
	Dictionary expected = Dictionary(catalog->get("data")).duplicate(true);
	Dictionary(expected["?catalog"]).erase("notes");
	CHECK(loaded->get("data") == expected);

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

TEST_CASE("[LocalizationCatalog] Translator notes round trip in every text format") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	CHECK(catalog->get_note_columns() == PackedStringArray({ "notes" }));
	CHECK(catalog->get_note("HELLO", "menu").is_empty());
	catalog->set_note("HELLO", "Main menu button", "menu");
	catalog->set_note("APPLE", "12", StringName(), "max_length");
	catalog->set_note("LINE", "Two lines\nkeep it short");
	CHECK(catalog->get_note_columns() == PackedStringArray({ "notes", "max_length" }));
	CHECK(catalog->get_note("APPLE", StringName(), "max_length") == "12");
	CHECK(catalog->get_note("MISSING").is_empty());
	// The reserved columns are no notes.
	ERR_PRINT_OFF;
	catalog->set_note("HELLO", "x", StringName(), "plural");
	ERR_PRINT_ON;
	CHECK(catalog->get_note("HELLO", StringName(), "plural").is_empty());
	CHECK(catalog->get_note_columns().size() == 2);

	for (int format = 0; format < ResourceTextSerializer::FORMAT_MAX; format++) {
		const char *format_classes[] = { "JSON", "INI", "CSV", "XML", "TOML", "YAML" };
		if (!ClassDB::class_exists(format_classes[format])) {
			continue; // Module disabled.
		}
		CAPTURE(format_classes[format]);
		Ref<TextLocalizationCatalog> copy;
		copy.instantiate();
		REQUIRE(copy->load_from_text(catalog->to_text(ResourceTextSerializer::Format(format)), ResourceTextSerializer::Format(format)) == OK);
		CHECK(copy->get_note("HELLO") == "greeting");
		CHECK(copy->get_note("HELLO", "menu") == "Main menu button");
		CHECK(copy->get_note("APPLE", StringName(), "max_length") == "12");
		CHECK(copy->get_note("LINE") == "Two lines\nkeep it short");
		CHECK(copy->get_plural_text("APPLE", "de", 3) == String(U"%d Äpfel"));
		CHECK(copy->get("data") == catalog->get("data"));
	}

	// Notes move with their rows.
	catalog->remove_key("HELLO");
	CHECK(catalog->get_note("HELLO", "menu") == "Main menu button");
	CHECK(catalog->get_note("APPLE", StringName(), "max_length") == "12");

	// ResourceLoader (the localization servers) loads text catalogs without notes outside the editor.
	const String path = TestUtils::get_temp_path("localization_catalog_notes.csv");
	REQUIRE(catalog->save_to_file(path) == OK);
	const Ref<TextLocalizationCatalog> loaded = ResourceLoader::load(path, "TextLocalizationCatalog", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_text("HELLO", "de", "menu") == String(U"Hallo (Menü)"));
	CHECK(loaded->get_note_columns().is_empty());
	Ref<TextLocalizationCatalog> tool;
	tool.instantiate();
	REQUIRE(tool->load_from_file(path) == OK);
	CHECK(tool->get_note("HELLO", "menu") == "Main menu button");
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

TEST_CASE("[LocalizationCatalog] Resource catalog resolves the expected type on demand") {
	const String path = TestUtils::get_temp_path("localization_catalog_type").path_join("item.tres");
	REQUIRE(DirAccess::make_dir_recursive_absolute(path.get_base_dir()) == OK);
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}
	Dictionary remaps;
	remaps[path] = PackedStringArray({ "res://item_de.tres:de" });
	Ref<ResourceLocalizationCatalog> catalog;
	catalog.instantiate();
	catalog->add_translation_remaps(remaps);
	// No file yet: adding the remaps stored no type.
	CHECK(catalog->get_expected_type(path) == StringName());
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("[gd_resource type=\"Gradient\" format=3]\n\n[resource]\n");
	}
	CHECK(catalog->get_expected_type(path) == "Gradient");
	// A stored type wins.
	catalog->set_expected_type(path, "Resource");
	CHECK(catalog->get_expected_type(path) == "Resource");
	CHECK(catalog->get_expected_type("res://unknown.tres") == StringName());
	DirAccess::remove_absolute(path);
}

TEST_CASE("[LocalizationCatalog] Channel, plural rules and text formats through ResourceLoader") {
	Ref<TextLocalizationCatalog> text;
	text.instantiate();
	CHECK(text->get_channel() == StringName("interface"));
	text->set_channel("subtitles");
	text->set_text("HELLO", "de", "Hallo");
	text->set_plural_texts("APPLE", "pl", PackedStringArray({ "jablko", "jablka", "jablek" }));

	// One shared compiled rule per rule text; the locale's default rule without one.
	const PluralRules *polish = LocalizationCatalog::get_plural_rules(String(), "pl");
	CHECK(polish == LocalizationCatalog::get_plural_rules(TranslationServer::get_singleton()->get_plural_rules("pl"), "pl"));
	CHECK(LocalizationCatalog::evaluate_plural_rules(polish, 5) == 2);
	CHECK(text->get_plural_form("pl", 2) == 1);

	const String folder = TestUtils::get_temp_path("localization_catalog_loader");
	REQUIRE(DirAccess::make_dir_recursive_absolute(folder) == OK);
	const String json_path = folder.path_join("texts.json");
	const String lcat_path = folder.path_join("texts.lcat");
	REQUIRE(text->save_to_file(json_path) == OK);
	REQUIRE(text->save_to_file(lcat_path) == OK);

	// Text formats only with a catalog type hint, cached under their path; the channel is kept in both formats.
	const Ref<TextLocalizationCatalog> loaded = ResourceLoader::load(json_path, "TextLocalizationCatalog");
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_channel() == StringName("subtitles"));
	CHECK(loaded->get_text("HELLO", "de") == "Hallo");
	CHECK(ResourceLoader::load(json_path, "TextLocalizationCatalog") == loaded);
	const Ref<TextLocalizationCatalog> binary = ResourceLoader::load(lcat_path);
	REQUIRE(binary.is_valid());
	CHECK(binary->get_channel() == StringName("subtitles"));
	CHECK(LocalizationCatalog::get_file_extensions().has("yaml"));

	DirAccess::remove_absolute(json_path);
	DirAccess::remove_absolute(lcat_path);
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

TEST_CASE("[LocalizationCatalog] Entries in the dictionary shape") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const Dictionary hello = catalog->get_entry("HELLO");
	CHECK(hello["en"] == "Hello");
	CHECK(hello["_notes"] == "greeting");
	CHECK_FALSE(hello.has("_key")); // Only keys with a context name their key and context.
	const Dictionary hello_menu = catalog->get_entry("HELLO", "menu");
	CHECK(hello_menu["_context"] == "menu");
	const Dictionary apple = catalog->get_entry("APPLE");
	CHECK(apple["_plural"] == "APPLES");
	CHECK(PackedStringArray(apple["de"]) == PackedStringArray({ String(U"%d Apfel"), String(U"%d Äpfel") }));
	CHECK(catalog->get_entry("MISSING").is_empty());

	// set_entry() replaces the texts, notes and plural key of the key.
	Dictionary entry;
	entry["de"] = "Servus";
	entry["fr"] = "Salut";
	entry["_notes"] = "informal";
	catalog->set_entry("HELLO", entry);
	CHECK(catalog->get_text("HELLO", "de") == "Servus");
	CHECK(catalog->get_text("HELLO", "en").is_empty());
	CHECK(catalog->get_text("HELLO", "fr") == "Salut");
	CHECK(catalog->get_note("HELLO") == "informal");
	CHECK(catalog->get_translated_count("en") == 4);

	// Undo in the editor: the old entry restores the key.
	catalog->set_entry("HELLO", hello);
	CHECK(catalog->get_entry("HELLO") == hello);
	catalog->set_entry("APPLE", Dictionary());
	CHECK(catalog->get_plural_key("APPLE").is_empty());
	CHECK(catalog->get_plural_texts("APPLE", "de").is_empty());
	catalog->set_entry("APPLE", apple);
	CHECK(catalog->get_plural_text("APPLE", "de", 5) == String(U"%d Äpfel"));
	catalog->set_entry("NEW", entry, "ctx");
	CHECK(catalog->get_text("NEW", "fr", "ctx") == "Salut");
}

TEST_CASE("[LocalizationCatalog] validate() reports missing translations, duplicates and invalid locales") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	Ref<DataValidation> result = catalog->validate();
	REQUIRE(result->get_issue_count() == 1);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_EMPTY_REQUIRED);
	CHECK(result->get_issue_row(0) == 4);
	CHECK(result->get_issue_column(0) == 1);
	CHECK(result->get_issue_field(0) == StringName("de"));
	CHECK(result->get_issue_path(0) == "BYE");

	REQUIRE(catalog->load_from_text("keys,en,de\nA,a,\nB,b,\nA,a2,\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(catalog->get_key_count() == 2);
	result = catalog->validate();
	REQUIRE(result->get_issue_count() == 3);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_DUPLICATE_KEY);
	CHECK(result->get_issue_row(0) == 0);
	CHECK(result->get_issue_kind(1) == DataValidation::ISSUE_KIND_EMPTY_REQUIRED);
	CHECK(result->get_issue_kind(2) == DataValidation::ISSUE_KIND_EMPTY_REQUIRED);
	catalog->clear();
	CHECK(catalog->validate()->is_valid()); // Duplicates belong to the last load.

	Ref<TextLocalizationCatalog> invalid_locale;
	invalid_locale.instantiate();
	invalid_locale->set_text("A", "qqq", "a");
	result = invalid_locale->validate();
	REQUIRE(result->get_issue_count() == 1);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_INVALID_LOCALE);

	// Options limit the issues.
	Ref<DataValidationOptions> options;
	options.instantiate();
	options->set_stop_on_first(true);
	REQUIRE(catalog->load_from_text("keys,en,de\nA,,\nB,,\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(catalog->validate(options)->get_issue_count() == 1);
	CHECK(catalog->validate()->get_issue_count() == 4);
}

TEST_CASE("[LocalizationCatalog] validate() gives lines for catalogs loaded from text files") {
	// CSV: the line of the key row, also after quoted line breaks and plural rows.
	const String csv_path = TestUtils::get_temp_path("validate_catalog.csv");
	{
		Ref<FileAccess> file = FileAccess::open(csv_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("keys,?plural,en,de\nA,,a,\"Zeile\nzwei\"\nAPPLE,APPLES,apple,\n,,apples,\nB,,b,\n");
	}
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	REQUIRE(catalog->load_from_file(csv_path) == OK);
	catalog->set_path_cache(csv_path);
	Ref<DataValidation> result = catalog->validate();
	REQUIRE(result->get_issue_count() == 2);
	CHECK(result->get_issue_path(0) == "APPLE");
	CHECK(result->get_issue_line(0) == 4);
	CHECK(result->get_issue_path(1) == "B");
	CHECK(result->get_issue_line(1) == 6);
	DirAccess::remove_absolute(csv_path);

	// JSON: the line of the entry.
	const String json_path = TestUtils::get_temp_path("validate_catalog.json");
	{
		Ref<FileAccess> file = FileAccess::open(json_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("{\n\t\"A\": {\n\t\t\"en\": \"a\",\n\t\t\"de\": \"a\"\n\t},\n\t\"B\": {\n\t\t\"en\": \"b\"\n\t}\n}\n");
	}
	REQUIRE(catalog->load_from_file(json_path) == OK);
	catalog->set_path_cache(json_path);
	result = catalog->validate();
	REQUIRE(result->get_issue_count() == 1);
	CHECK(result->get_issue_line(0) == 6);
	DirAccess::remove_absolute(json_path);
}

TEST_CASE("[LocalizationCatalog] CSV delimiters and serializer options") {
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	REQUIRE(catalog->load_from_text("keys;en;de\nHELLO;Hello, world;Hallo, Welt\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(catalog->get_text("HELLO", "de") == "Hallo, Welt");
	REQUIRE(catalog->load_from_text("keys\ten\tde\nHELLO\tHello\tHallo\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(catalog->get_text("HELLO", "de") == "Hallo");

	Ref<SerializerOptions> options;
	options.instantiate();
	options->set_csv_delimiter(";");
	const String text = catalog->to_text(ResourceTextSerializer::FORMAT_CSV, options);
	CHECK(text.begins_with("keys;en;de"));
	Ref<TextLocalizationCatalog> copy;
	copy.instantiate();
	REQUIRE(copy->load_from_text(text, ResourceTextSerializer::FORMAT_CSV, options) == OK);
	CHECK(copy->get_text("HELLO", "en") == "Hello");
	CHECK(options->is_csv_headers()); // The caller's options are not changed.

	CHECK(ResourceTextSerializer::create_data_format(ResourceTextSerializer::FORMAT_JSON).is_null());
	const Ref<TextDataFormat> format = ResourceTextSerializer::create_data_format(ResourceTextSerializer::FORMAT_CSV, options);
	REQUIRE(format.is_valid());
	CHECK(format->get_options() == options);
}

TEST_CASE("[LocalizationCatalog] A new catalog saved as CSV is a localization table") {
	// #346: the header with the context column and the tool locale, loaded back as a catalog.
	const String tool_locale = TranslationServer::get_singleton()->get_tool_locale();
	Ref<TextLocalizationCatalog> text_catalog;
	text_catalog.instantiate();
	Ref<ResourceLocalizationCatalog> resource_catalog;
	resource_catalog.instantiate();
	for (const Ref<LocalizationCatalog> &catalog : { Ref<LocalizationCatalog>(text_catalog), Ref<LocalizationCatalog>(resource_catalog) }) {
		const String text = catalog->to_text(ResourceTextSerializer::FORMAT_CSV);
		CHECK(text.strip_edges() == "keys,?context," + tool_locale);
		Ref<LocalizationCatalog> copy = Object::cast_to<LocalizationCatalog>(ClassDB::instantiate(catalog->get_class()));
		REQUIRE(copy.is_valid());
		REQUIRE(copy->load_from_text(text, ResourceTextSerializer::FORMAT_CSV) == OK);
		CHECK(copy->get_key_count() == 0);
		CHECK(copy->get_locales() == PackedStringArray({ tool_locale }));
	}

	// Without locale columns the catalog is empty; unknown column names are still no localization table.
	CHECK(text_catalog->load_from_text("keys\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(text_catalog->load_from_text("keys,?context,_notes\nHELLO,,note\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(text_catalog->get_key_count() == 1);
	ERR_PRINT_OFF;
	CHECK(text_catalog->load_from_text("id,1234\n1,a\n", ResourceTextSerializer::FORMAT_CSV) == ERR_INVALID_DATA);
	ERR_PRINT_ON;
}

TEST_CASE("[LocalizationCatalog] Empty cells and empty locale columns are valid") {
	// #346: a partly filled and a completely empty locale load, save and validate as missing translations only.
	Ref<TextLocalizationCatalog> catalog;
	catalog.instantiate();
	REQUIRE(catalog->load_from_text("keys,en,es,pt\nHELLO,Hello,Hola,\nBYE,Bye,,\n", ResourceTextSerializer::FORMAT_CSV) == OK);
	CHECK(catalog->get_locales() == PackedStringArray({ "en", "es", "pt" }));
	CHECK(catalog->get_missing_rows("es") == PackedInt32Array({ 1 }));
	CHECK(catalog->get_missing_rows("pt") == PackedInt32Array({ 0, 1 }));
	const Ref<DataValidation> result = catalog->validate();
	REQUIRE(result->get_issue_count() == 3);
	for (int i = 0; i < result->get_issue_count(); i++) {
		CHECK(result->get_issue_kind(i) == DataValidation::ISSUE_KIND_EMPTY_REQUIRED);
	}
	CHECK(catalog->to_text(ResourceTextSerializer::FORMAT_CSV).begins_with("keys,en,es,pt\n"));

	const String path = TestUtils::get_temp_path("gaps.lcat");
	REQUIRE(catalog->save_to_file(path) == OK);
	Ref<TextLocalizationCatalog> copy;
	copy.instantiate();
	REQUIRE(copy->load_from_file(path) == OK);
	CHECK(copy->get_locales() == PackedStringArray({ "en", "es", "pt" }));
	CHECK(copy->get_text("HELLO", "es") == "Hola");
	CHECK(copy->get_text("HELLO", "pt").is_empty());
	DirAccess::remove_absolute(path);
}

TEST_CASE("[LocalizationCatalog] ResourceSaver writes catalogs in their text format") {
	// The editor saves catalogs through ResourceSaver: not the generic resource JSON of the text resource savers.
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const String path = TestUtils::get_temp_path("saved_catalog.json");
	REQUIRE(ResourceSaver::save(catalog, path) == OK);
	Ref<TextLocalizationCatalog> copy;
	copy.instantiate();
	REQUIRE(copy->load_from_file(path) == OK);
	CHECK(copy->get_plural_text("APPLE", "de", 5) == String(U"%d Äpfel"));
	CHECK(copy->get_note("HELLO") == "greeting");
	DirAccess::remove_absolute(path);
}

TEST_CASE("[LocalizationCatalog] POT and PO files") {
	Ref<TextLocalizationCatalog> catalog = make_text_catalog();
	const String pot = catalog->to_po();
	CHECK(pot.contains("#. greeting\nmsgid \"HELLO\"\nmsgstr \"\"\n"));
	CHECK(pot.contains("msgctxt \"menu\"\nmsgid \"HELLO\"\n"));
	CHECK(pot.contains("msgid \"APPLE\"\nmsgid_plural \"APPLES\"\nmsgstr[0] \"\"\nmsgstr[1] \"\"\n"));

	// The PO file of a locale loads as Translation with Godot's PO loader.
	const String path = TestUtils::get_temp_path("catalog_de.po");
	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string(catalog->to_po("de"));
	}
	const Ref<Translation> translation = ResourceLoader::load(path, "Translation", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(translation.is_valid());
	CHECK(translation->get_locale() == "de");
	CHECK(translation->get_message("HELLO") == "Hallo");
	CHECK(translation->get_message("HELLO", "menu") == String(U"Hallo (Menü)"));
	CHECK(translation->get_message("LINE") == "Eins\nZwei");
	CHECK(translation->get_plural_message("APPLE", "APPLES", 5) == String(U"%d Äpfel"));
	DirAccess::remove_absolute(path);
}

} // namespace TestLocalizationCatalog
