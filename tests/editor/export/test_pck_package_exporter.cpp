/**************************************************************************/
/*  test_pck_package_exporter.cpp                                         */
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

TEST_FORCE_LINK(test_pck_package_exporter)

#ifdef TOOLS_ENABLED

#include "core/io/pck_package.h"
#include "editor/export/pck_package_exporter.h"

namespace TestPCKPackageExporter {

using Exporter = PCKPackageExporter;

static Vector<uint8_t> bytes(const String &p_text) {
	return p_text.to_utf8_buffer();
}

TEST_CASE("[PCKPackageExporter] plan_description sorts the entries") {
	Ref<PCKPackage> description;
	description.instantiate();
	REQUIRE(description->add_file("art/a.png", "res://art/a.png") == OK);
	REQUIRE(description->add_file("res://scenes/b.tscn", "res://scenes/b.tscn", true) == OK);
	REQUIRE(description->add_file("config/user.cfg", "user://settings.cfg") == OK);
	REQUIRE(description->add_file_from_buffer("data/c.json", bytes("{}")) == OK);
	REQUIRE(description->add_removal("old/d.png") == OK);

	const Ref<PCKBuildPlan> plan = Exporter::plan_description(description);
	CHECK(plan->get_errors().is_empty());
	CHECK(plan->get_export_sources().size() == 2);
	CHECK(plan->get_export_sources().has("res://art/a.png"));
	CHECK(plan->get_export_sources().has("res://scenes/b.tscn"));
	CHECK(plan->get_raw_files().size() == 2);
	CHECK(plan->get_raw_files().has("config/user.cfg"));
	CHECK(plan->get_raw_files().has("data/c.json"));
	CHECK(plan->get_removals().size() == 1);
	CHECK(plan->get_removals()[0] == "old/d.png");
	CHECK(plan->get_skipped().is_empty());
}

TEST_CASE("[PCKPackageExporter] plan_description refuses renamed res:// targets") {
	Ref<PCKPackage> description;
	description.instantiate();
	REQUIRE(description->add_file("textures/a.png", "res://art/a.png") == OK);
	REQUIRE(description->add_file("art/b.png", "res://art/./b.png") == OK);

	const Ref<PCKBuildPlan> plan = Exporter::plan_description(description);
	REQUIRE(plan->get_errors().size() == 1);
	// The message names the entry and the source.
	CHECK(plan->get_errors()[0].contains("textures/a.png"));
	CHECK(plan->get_errors()[0].contains("art/a.png"));
	// A source that only needs simplifying is not renamed.
	CHECK(plan->get_export_sources().size() == 1);
	CHECK(plan->get_export_sources()[0] == "res://art/b.png");
}

TEST_CASE("[PCKPackageExporter] plan_description handles .import, .remap and .uid entries") {
	Ref<PCKPackage> description;
	description.instantiate();
	REQUIRE(description->add_file("art/a.png", "res://art/a.png") == OK);
	REQUIRE(description->add_file("art/a.png.import", "res://art/a.png.import") == OK);
	REQUIRE(description->add_file("scripts/s.gd.uid", "res://scripts/s.gd.uid") == OK);

	const Ref<PCKBuildPlan> plan = Exporter::plan_description(description);
	// The export writes the .import file of a.png itself.
	CHECK(plan->get_skipped().size() == 1);
	CHECK(plan->get_skipped()[0] == "art/a.png.import");
	// A sidecar without its source can't be exported.
	REQUIRE(plan->get_errors().size() == 1);
	CHECK(plan->get_errors()[0].contains("scripts/s.gd.uid"));
	CHECK(plan->get_export_sources().size() == 1);
}

TEST_CASE("[PCKPackageExporter] plan_description copies sidecars from outside res:// as they are") {
	Ref<PCKPackage> description;
	description.instantiate();
	REQUIRE(description->add_file("mods/x.png.import", "user://mods/x.png.import") == OK);

	const Ref<PCKBuildPlan> plan = Exporter::plan_description(description);
	CHECK(plan->get_errors().is_empty());
	CHECK(plan->get_raw_files().size() == 1);
	CHECK(plan->get_export_sources().is_empty());
}

TEST_CASE("[PCKPackageExporter] plan_description with a null description") {
	const Ref<PCKBuildPlan> plan = Exporter::plan_description(Ref<PCKPackage>());
	CHECK(plan->get_errors().size() == 1);
}

TEST_CASE("[PCKPackageExporter] plan_description returns a PCKBuildPlan") {
	Ref<PCKPackage> description;
	description.instantiate();
	REQUIRE(description->add_file("art/a.png", "res://art/a.png") == OK);
	REQUIRE(description->add_removal("old/d.png") == OK);

	const Ref<PCKBuildPlan> plan = Exporter::plan_description(description);
	REQUIRE(plan.is_valid());
	CHECK(plan->is_valid());
	CHECK(bool(plan->get("valid")));
	CHECK(PackedStringArray(plan->get("export_sources")).size() == 1);
	CHECK(PackedStringArray(plan->get("removals")).size() == 1);
	CHECK(PackedStringArray(plan->get("errors")).is_empty());

	CHECK(plan->get_export_sources()[0] == "res://art/a.png");

	const Ref<PCKBuildPlan> invalid = Exporter::plan_description(Ref<PCKPackage>());
	CHECK_FALSE(invalid->is_valid());
}

TEST_CASE("[PCKPackageExporter] matches_encryption_filters") {
	PackedStringArray include;
	include.push_back("*.gd");
	include.push_back("secret/*");
	PackedStringArray exclude;
	exclude.push_back("secret/public_*");

	CHECK(Exporter::matches_encryption_filters("res://scripts/a.gd", include, exclude));
	CHECK(Exporter::matches_encryption_filters("scripts/a.gd", include, exclude));
	CHECK(Exporter::matches_encryption_filters("res://secret/a.png", include, exclude));
	CHECK(Exporter::matches_encryption_filters("res://SECRET/A.PNG", include, exclude));
	CHECK_FALSE(Exporter::matches_encryption_filters("res://secret/public_a.png", include, exclude));
	CHECK_FALSE(Exporter::matches_encryption_filters("res://art/a.png", include, exclude));
	CHECK_FALSE(Exporter::matches_encryption_filters("res://art/a.png", PackedStringArray(), PackedStringArray()));

	PackedStringArray all;
	all.push_back("*");
	CHECK(Exporter::matches_encryption_filters("res://dlc/a.pck", all, PackedStringArray()));
	PackedStringArray no_packs;
	no_packs.push_back("*.pck");
	CHECK_FALSE(Exporter::matches_encryption_filters("res://dlc/a.pck", all, no_packs));
}

TEST_CASE("[PCKPackageExporter] should_encrypt without a preset") {
	CHECK_FALSE(Exporter::should_encrypt(Ref<EditorExportPreset>(), "res://a.gd"));
}

TEST_CASE("[PCKPackageExporter] export_target_to_res_path") {
	const String project = "C:/Projects/Game";
	CHECK(Exporter::export_target_to_res_path("build/game.pck", project) == "res://build/game.pck");
	CHECK(Exporter::export_target_to_res_path("./build/../out/game.pck", project) == "res://out/game.pck");
	CHECK(Exporter::export_target_to_res_path("build\\game.pck", project) == "res://build/game.pck");
	CHECK(Exporter::export_target_to_res_path("res://build/game.pck", project) == "res://build/game.pck");
	CHECK(Exporter::export_target_to_res_path("C:/Projects/Game/build/game.exe", project) == "res://build/game.exe");
	CHECK(Exporter::export_target_to_res_path("c:/projects/game/build/game.pck", project + "/") == "res://build/game.pck");

	// Outside the project folder.
	CHECK(Exporter::export_target_to_res_path("../builds/game.pck", project).is_empty());
	CHECK(Exporter::export_target_to_res_path("C:/Builds/game.pck", project).is_empty());
	CHECK(Exporter::export_target_to_res_path("C:/Projects/GameOther/game.pck", project).is_empty());
	CHECK(Exporter::export_target_to_res_path("user://game.pck", project).is_empty());
	CHECK(Exporter::export_target_to_res_path("", project).is_empty());
	CHECK(Exporter::export_target_to_res_path("/home/user/game/build/game.pck", "/home/user/game") == "res://build/game.pck");
}

TEST_CASE("[PCKPackageExporter] No presets without the editor") {
	// The test runner has no EditorExport singleton: no presets, no targets, nothing running.
	CHECK(Exporter::get_preset_names().is_empty());
	CHECK(Exporter::find_preset("Windows Desktop").is_null());
	CHECK(Exporter::get_export_target_paths().is_empty());
	CHECK_FALSE(Exporter::is_export_target("res://build/game.pck"));
	CHECK_FALSE(Exporter::is_exporting());
	CHECK_FALSE(Exporter::is_building());
}

TEST_CASE("[PCKPackageExporter] Export counter") {
	Exporter::notify_export_begin();
	CHECK(Exporter::is_exporting());
	Exporter::notify_export_end();
	CHECK_FALSE(Exporter::is_exporting());
	// An unmatched end never makes the counter negative.
	Exporter::notify_export_end();
	CHECK_FALSE(Exporter::is_exporting());
}

TEST_CASE("[PCKPackageExporter] Command line request") {
	CHECK_FALSE(Exporter::has_command_line_request());
	Exporter::request_command_line_export("Windows Desktop", "res://packs/dlc.tres", "out.pck", false);
	CHECK(Exporter::has_command_line_request());
	// Without the editor, loading the description fails; the request is consumed anyway.
	ERR_PRINT_OFF;
	CHECK(Exporter::run_command_line_request() != OK);
	ERR_PRINT_ON;
	CHECK_FALSE(Exporter::has_command_line_request());
}

TEST_CASE("[PCKPackageExporter] export_description refuses invalid input") {
	Ref<PCKPackage> description;
	description.instantiate();
	ERR_PRINT_OFF;
	CHECK(Exporter::export_description(description, "No Such Preset", "user://x.pck") == ERR_DOES_NOT_EXIST);
	CHECK(Exporter::export_description_with_preset(Ref<PCKPackage>(), Ref<EditorExportPreset>(), "user://x.pck") == ERR_INVALID_PARAMETER);
	CHECK(Exporter::export_description_with_preset(description, Ref<EditorExportPreset>(), "user://x.pck") == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

} // namespace TestPCKPackageExporter

#endif // TOOLS_ENABLED
