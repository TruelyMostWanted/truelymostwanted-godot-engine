/**************************************************************************/
/*  test_translation.cpp                                                  */
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

TEST_FORCE_LINK(test_translation)

#include "core/io/csv_format.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/string/optimized_translation.h"
#include "core/string/plural_rules.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"

#ifdef TOOLS_ENABLED
#include "editor/import/resource_importer_csv_translation.h"
#endif

#include "tests/test_utils.h"

namespace TestTranslation {

TEST_CASE("[Translation] Messages") {
	Ref<Translation> translation;
	translation.instantiate();
	translation->set_locale("fr");
	translation->add_message("Hello", "Bonjour");
	CHECK(translation->get_message("Hello") == "Bonjour");

	translation->erase_message("Hello");
	// The message no longer exists, so it returns an empty string instead.
	CHECK(translation->get_message("Hello") == "");

	List<StringName> messages;
	translation->get_message_list(&messages);
	CHECK(translation->get_message_count() == 0);
	CHECK(messages.size() == 0);

	translation->add_message("Hello2", "Bonjour2");
	translation->add_message("Hello3", "Bonjour3");
	messages.clear();
	translation->get_message_list(&messages);
	CHECK(translation->get_message_count() == 2);
	CHECK(messages.size() == 2);
	// Messages are stored in a Map, don't assume ordering.
	CHECK(messages.find("Hello2"));
	CHECK(messages.find("Hello3"));
}

TEST_CASE("[Translation] Messages with context") {
	Ref<Translation> translation;
	translation.instantiate();
	translation->set_locale("fr");
	translation->add_message("Hello", "Bonjour");
	translation->add_message("Hello", "Salut", "friendly");
	CHECK(translation->get_message("Hello") == "Bonjour");
	CHECK(translation->get_message("Hello", "friendly") == "Salut");
	CHECK(translation->get_message("Hello", "nonexistent_context") == "");

	// Only remove the message for the default context, not the "friendly" context.
	translation->erase_message("Hello");
	// The message no longer exists, so it returns an empty string instead.
	CHECK(translation->get_message("Hello") == "");
	CHECK(translation->get_message("Hello", "friendly") == "Salut");
	CHECK(translation->get_message("Hello", "nonexistent_context") == "");

	List<StringName> messages;
	translation->get_message_list(&messages);

	CHECK(translation->get_message_count() == 1);
	CHECK(messages.size() == 1);

	translation->add_message("Hello2", "Bonjour2");
	translation->add_message("Hello2", "Salut2", "friendly");
	translation->add_message("Hello3", "Bonjour3");
	messages.clear();
	translation->get_message_list(&messages);

	CHECK(translation->get_message_count() == 4);
	CHECK(messages.size() == 4);
	// Messages are stored in a Map, don't assume ordering.
	CHECK(messages.find("Hello2"));
	CHECK(messages.find("Hello3"));
	// Context and untranslated string are separated by EOT.
	CHECK(messages.find("friendly\x04Hello2"));
}

TEST_CASE("[Translation] Plural messages") {
	{
		Ref<Translation> translation;
		translation.instantiate();
		translation->set_locale("fr");
		CHECK(translation->get_nplurals() == 2);
	}

	{
		Ref<Translation> translation;
		translation.instantiate();
		translation->set_locale("invalid");
		CHECK(translation->get_nplurals() == 2);
	}

	{
		Ref<Translation> translation;
		translation.instantiate();
		translation->set_plural_rules_override("Plural-Forms: nplurals=2; plural=(n >= 2);");
		CHECK(translation->get_nplurals() == 2);

		PackedStringArray plurals;
		plurals.push_back("Il y a %d pomme");
		plurals.push_back("Il y a %d pommes");
		translation->add_plural_message("There are %d apples", plurals);
		ERR_PRINT_OFF;
		// This is invalid, as the number passed to `get_plural_message()` may not be negative.
		CHECK(vformat(translation->get_plural_message("There are %d apples", "", -1), -1) == "");
		ERR_PRINT_ON;
		CHECK(vformat(translation->get_plural_message("There are %d apples", "", 0), 0) == "Il y a 0 pomme");
		CHECK(vformat(translation->get_plural_message("There are %d apples", "", 1), 1) == "Il y a 1 pomme");
		CHECK(vformat(translation->get_plural_message("There are %d apples", "", 2), 2) == "Il y a 2 pommes");
	}
}

TEST_CASE("[Translation] Plural rules parsing") {
	ERR_PRINT_OFF;
	{
		CHECK(PluralRules::parse("") == nullptr);

		CHECK(PluralRules::parse("plurals=(n != 1);") == nullptr);
		CHECK(PluralRules::parse("nplurals; plurals=(n != 1);") == nullptr);
		CHECK(PluralRules::parse("nplurals=; plurals=(n != 1);") == nullptr);
		CHECK(PluralRules::parse("nplurals=0; plurals=(n != 1);") == nullptr);
		CHECK(PluralRules::parse("nplurals=-1; plurals=(n != 1);") == nullptr);

		CHECK(PluralRules::parse("nplurals=2;") == nullptr);
		CHECK(PluralRules::parse("nplurals=2; plurals;") == nullptr);
		CHECK(PluralRules::parse("nplurals=2; plurals=;") == nullptr);
	}
	ERR_PRINT_ON;

	{
		PluralRules *pr = PluralRules::parse("nplurals=3; plural=(n==0 ? 0 : n==1 ? 1 : 2);");
		REQUIRE(pr != nullptr);

		CHECK(pr->get_nplurals() == 3);
		CHECK(pr->get_plural() == "(n==0 ? 0 : n==1 ? 1 : 2)");

		CHECK(pr->evaluate(0) == 0);
		CHECK(pr->evaluate(1) == 1);
		CHECK(pr->evaluate(2) == 2);
		CHECK(pr->evaluate(3) == 2);

		memdelete(pr);
	}

	{
		PluralRules *pr = PluralRules::parse("nplurals=1; plural=0;");
		REQUIRE(pr != nullptr);

		CHECK(pr->get_nplurals() == 1);
		CHECK(pr->get_plural() == "0");

		CHECK(pr->evaluate(0) == 0);
		CHECK(pr->evaluate(1) == 0);
		CHECK(pr->evaluate(2) == 0);
		CHECK(pr->evaluate(3) == 0);

		memdelete(pr);
	}
}

TEST_CASE("[TranslationCSV] validate_translation() reports header problems, duplicate keys and missing translations") {
	Ref<CSV> csv;
	csv.instantiate();
	// Column 2 is empty (invalid), 'xx' is an unknown language, '_notes' is ignored, '?context' is special.
	REQUIRE(csv->parse("keys,en,,de,xx,_notes,?context\nHELLO,Hello,,Hallo,hi,note,\nBYE,Bye,,,bye,,\nHELLO,Hi again,,Hallo,hi,,\nHELLO,Hello,,Hallo,hi,,formal\n?pluralrule,,,,,,\n,,,,,,\n") == OK);

	const Ref<DataValidation> result = csv->validate_translation();
	REQUIRE(result.is_valid());
	int invalid_locales = 0;
	int duplicates = 0;
	int missing = 0;
	for (int i = 0; i < result->get_issue_count(); i++) {
		switch (result->get_issue_kind(i)) {
			case DataValidation::ISSUE_KIND_INVALID_LOCALE: {
				invalid_locales++;
				CHECK(result->get_issue_row(i) == DataValidation::POSITION_NONE);
				const int column = result->get_issue_column(i);
				CHECK((column == 2 || column == 4));
			} break;
			case DataValidation::ISSUE_KIND_DUPLICATE_KEY: {
				duplicates++;
				CHECK(result->get_issue_row(i) == 2);
				CHECK(result->get_issue_column(i) == 0);
			} break;
			case DataValidation::ISSUE_KIND_EMPTY_REQUIRED: {
				missing++;
				CHECK(result->get_issue_row(i) == 1);
				CHECK(result->get_issue_column(i) == 3);
				CHECK(result->get_issue_field(i) == StringName("de"));
			} break;
			default: {
				FAIL("Unexpected issue: ", result->get_issue_message(i));
			} break;
		}
	}
	CHECK(invalid_locales == 2);
	CHECK_MESSAGE(duplicates == 1, "The same key in another context is no duplicate.");
	CHECK(missing == 1);
}

TEST_CASE("[TranslationCSV] validate_translation() of a data CSV and a CSV without header") {
	Ref<CSV> csv;
	csv.instantiate();
	REQUIRE(csv->parse("name,health,speed\nknight,100,4\n") == OK);
	Ref<DataValidation> result = csv->validate_translation();
	CHECK_MESSAGE(result->get_issue_count() == 3, "Two unknown language codes and one 'not a translation table' issue, no row checks.");
	CHECK(result->get_issue_kind(2) == DataValidation::ISSUE_KIND_INVALID_LOCALE);
	CHECK(result->get_issue_column(2) == DataValidation::POSITION_NONE);

	csv->set_header(PackedStringArray());
	result = csv->validate_translation();
	CHECK(result->get_issue_count() == 1);
	CHECK(result->get_issue_kind(0) == DataValidation::ISSUE_KIND_MISSING_FIELD);

	REQUIRE(csv->parse("keys,en,fr\nHELLO,Hello,Bonjour\n") == OK);
	CHECK(csv->validate_translation()->is_valid());
}

#ifdef TOOLS_ENABLED
TEST_CASE("[OptimizedTranslation] Generate from Translation and read messages") {
	Ref<Translation> translation = memnew(Translation);
	translation->set_locale("fr");
	translation->add_message("Hello", "Bonjour");
	translation->add_message("Hello2", "Bonjour2");
	translation->add_message("Hello3", "Bonjour3");

	Ref<OptimizedTranslation> optimized_translation = memnew(OptimizedTranslation);
	optimized_translation->generate(translation);
	CHECK(optimized_translation->get_message("Hello") == "Bonjour");
	CHECK(optimized_translation->get_message("Hello2") == "Bonjour2");
	CHECK(optimized_translation->get_message("Hello3") == "Bonjour3");
	CHECK(optimized_translation->get_message("DoesNotExist") == "");

	List<StringName> messages;
	// `get_message_list()` can't return the list of messages stored in an OptimizedTranslation.
	ERR_PRINT_OFF;
	optimized_translation->get_message_list(&messages);
	CHECK(optimized_translation->get_message_count() == 0);
	ERR_PRINT_ON;
	CHECK(messages.size() == 0);
}

TEST_CASE("[TranslationCSV] CSV import") {
	Ref<ResourceImporterCSVTranslation> import_csv_translation = memnew(ResourceImporterCSVTranslation);

	HashMap<StringName, Variant> options;
	options["compress"] = false;
	options["delimiter"] = 0;

	List<String> gen_files;

	Error result = import_csv_translation->import(0, TestUtils::get_data_path("translations.csv"),
			"", options, nullptr, &gen_files);
	CHECK(result == OK);
	CHECK(gen_files.size() == 4);

	Ref<TranslationDomain> td = TranslationServer::get_singleton()->get_or_add_domain("godot.test");
	for (const String &file : gen_files) {
		Ref<Translation> translation = ResourceLoader::load(file);
		CHECK(translation.is_valid());
		td->add_translation(translation);
	}

	td->set_locale_override("en");

	CHECK(td->translate("GOOD_MORNING", StringName()) == "Good Morning");
	CHECK(td->translate("GOOD_EVENING", StringName()) == "Good Evening");

	td->set_locale_override("de");

	CHECK(td->translate("GOOD_MORNING", StringName()) == "Guten Morgen");
	CHECK(td->translate("GOOD_EVENING", StringName()) == "Good Evening"); // Left blank in CSV, should source from 'en'.

	td->set_locale_override("ja");

	CHECK(td->translate("GOOD_MORNING", StringName()) == String::utf8("おはよう"));
	CHECK(td->translate("GOOD_EVENING", StringName()) == String::utf8("こんばんは"));

	/* FIXME: This passes, but triggers a chain reaction that makes test_viewport
	 * and test_text_edit explode in a billion glittery Unicode particles.
	td->set_locale_override("fa");

	CHECK(td->translate("GOOD_MORNING", String()) == String::utf8("صبح بخیر"));
	CHECK(td->translate("GOOD_EVENING", String()) == String::utf8("عصر بخیر"));
	*/

	TranslationServer::get_singleton()->remove_domain("godot.test");
}

// Counts errors and warnings printed while it is alive.
struct CSVImportMessageCounter {
	ErrorHandlerList handler;
	int errors = 0;
	int warnings = 0;

	static void _count(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
		CSVImportMessageCounter *self = static_cast<CSVImportMessageCounter *>(p_self);
		if (p_type == ERR_HANDLER_WARNING) {
			self->warnings++;
		} else {
			self->errors++;
		}
	}

	CSVImportMessageCounter() {
		handler.errfunc = _count;
		handler.userdata = this;
		add_error_handler(&handler);
	}

	~CSVImportMessageCounter() {
		remove_error_handler(&handler);
	}
};

static String write_temp_csv(const String &p_name, const String &p_content) {
	const String path = TestUtils::get_temp_path(p_name);
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_string(p_content);
	}
	return path;
}

TEST_CASE("[TranslationCSV] Data CSV logs one warning, not one error per column") {
	Ref<ResourceImporterCSVTranslation> import_csv_translation = memnew(ResourceImporterCSVTranslation);
	HashMap<StringName, Variant> options;
	options["compress"] = 0;
	options["delimiter"] = 0;

	const String path = write_temp_csv("data_units.csv", "name,health,speed,armor\nknight,100,4,20\narcher,60,6,5\n");
	List<String> gen_files;
	Error result;
	int errors;
	int warnings;
	{
		CSVImportMessageCounter counter;
		result = import_csv_translation->import(2810001, path, "", options, nullptr, &gen_files);
		errors = counter.errors;
		warnings = counter.warnings;
	}

	CHECK(result == OK);
	CHECK(gen_files.is_empty());
	CHECK_MESSAGE(errors == 0, "A data CSV must not log an error per column.");
	CHECK_MESSAGE(warnings == 1, "A data CSV must log exactly one warning for the file.");

	DirAccess::remove_absolute(path);
}

TEST_CASE("[TranslationCSV] Header problems are reported once per file") {
	Ref<ResourceImporterCSVTranslation> import_csv_translation = memnew(ResourceImporterCSVTranslation);
	HashMap<StringName, Variant> options;
	options["compress"] = 0;
	options["delimiter"] = 0;

	// Column 3 is empty (invalid locale), 'comment' and 'remark' have unknown language codes, '_notes' is ignored.
	const String path = write_temp_csv("mixed_columns.csv", "keys,en,,comment,remark,_notes\nHELLO,Hello,x,greeting,first,ignored\n");
	List<String> gen_files;
	Error result;
	int errors;
	int warnings;
	{
		CSVImportMessageCounter counter;
		result = import_csv_translation->import(2810002, path, "", options, nullptr, &gen_files);
		errors = counter.errors;
		warnings = counter.warnings;
	}

	CHECK(result == OK);
	// 'en' and the unknown codes 'comment' and 'remark' are imported (custom locales stay possible).
	CHECK(gen_files.size() == 3);
	CHECK_MESSAGE(errors == 1, "The invalid column must be reported in one error.");
	CHECK_MESSAGE(warnings == 1, "Both unknown language codes must be reported in one warning.");

	for (const String &file : gen_files) {
		DirAccess::remove_absolute(file);
	}
	DirAccess::remove_absolute(path);
}

TEST_CASE("[TranslationCSV] A second ?context column is reported and ignored") {
	Ref<ResourceImporterCSVTranslation> import_csv_translation = memnew(ResourceImporterCSVTranslation);
	HashMap<StringName, Variant> options;
	options["compress"] = 0;
	options["delimiter"] = 0;

	const String path = write_temp_csv("double_context.csv", "keys,en,?context,?CONTEXT\nHELLO,Hello,a,b\n");
	List<String> gen_files;
	Error result;
	int errors;
	{
		CSVImportMessageCounter counter;
		result = import_csv_translation->import(2800001, path, "", options, nullptr, &gen_files);
		errors = counter.errors;
	}

	CHECK(result == OK);
	CHECK(gen_files.size() == 1);
	CHECK(errors == 1);
	if (gen_files.size() == 1) {
		Ref<Translation> translation = ResourceLoader::load(gen_files.front()->get());
		REQUIRE(translation.is_valid());
		CHECK(translation->get_message("HELLO", "a") == "Hello");
	}

	for (const String &file : gen_files) {
		DirAccess::remove_absolute(file);
	}
	DirAccess::remove_absolute(path);
}

#endif // TOOLS_ENABLED

} // namespace TestTranslation
