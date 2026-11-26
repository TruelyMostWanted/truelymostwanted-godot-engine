/**************************************************************************/
/*  test_streaming_assets_pack_mode.cpp                                   */
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

TEST_FORCE_LINK(test_streaming_assets_pack_mode)

#ifdef TOOLS_ENABLED

#include "core/io/pck_package.h"
#include "editor/export/pck_package_exporter.h"
#include "editor/export/streaming_assets_pack_builder.h"

namespace TestStreamingAssetsPackMode {

TEST_CASE("[StreamingAssetsPackBuilder] The streaming pack goes through the export pipeline") {
	const PackedStringArray files = { "a.png", "levels/l1.tscn", "data/b.json", "scripts/s.gd" };
	const Ref<PCKPackage> description = StreamingAssetsPackBuilder::make_description_for_files("res://streaming_assets", files);
	REQUIRE(description.is_valid());

	const Ref<PCKBuildPlan> plan = PCKPackageExporter::plan_description(description);
	REQUIRE(plan.is_valid());
	CHECK(plan->is_valid());
	CHECK(plan->get_errors().is_empty());
	// Every file keeps its `res://` path: the export writes its `.import` file, imported artifacts and remaps.
	CHECK(plan->get_export_sources().size() == 4);
	CHECK(plan->get_export_sources().has("res://streaming_assets/a.png"));
	CHECK(plan->get_export_sources().has("res://streaming_assets/levels/l1.tscn"));
	CHECK(plan->get_export_sources().has("res://streaming_assets/data/b.json"));
	CHECK(plan->get_export_sources().has("res://streaming_assets/scripts/s.gd"));
	// Nothing is copied as it is, and nothing is removed.
	CHECK(plan->get_raw_files().is_empty());
	CHECK(plan->get_removals().is_empty());
	CHECK(plan->get_skipped().is_empty());
}

TEST_CASE("[StreamingAssetsPackBuilder] An empty folder gives an empty plan") {
	const Ref<PCKPackage> description = StreamingAssetsPackBuilder::make_description_for_files("res://streaming_assets", PackedStringArray());
	REQUIRE(description.is_valid());
	const Ref<PCKBuildPlan> plan = PCKPackageExporter::plan_description(description);
	CHECK(plan->is_valid());
	CHECK(plan->get_export_sources().is_empty());
	CHECK(plan->get_raw_files().is_empty());
}

TEST_CASE("[StreamingAssetsPackBuilder] A folder outside res:// gives no plan") {
	ERR_PRINT_OFF;
	const Ref<PCKPackage> description = StreamingAssetsPackBuilder::make_description("user://streaming_assets");
	ERR_PRINT_ON;
	CHECK(description.is_null());
	ERR_PRINT_OFF;
	const Ref<PCKBuildPlan> plan = PCKPackageExporter::plan_description(description);
	ERR_PRINT_ON;
	CHECK_FALSE(plan->is_valid());
}

} // namespace TestStreamingAssetsPackMode

#endif // TOOLS_ENABLED
