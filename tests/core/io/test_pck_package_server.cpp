/**************************************************************************/
/*  test_pck_package_server.cpp                                           */
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

TEST_FORCE_LINK(test_pck_package_server)

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/pck_package_server.h"
#include "core/io/pck_packer.h"
#include "core/io/resource_uid.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestPckPackageServer {

static const String KEY_A = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
static const String KEY_B = "ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100";

static PackedByteArray _bytes(const String &p_text) {
	return p_text.to_utf8_buffer();
}

static String _text(const PackedByteArray &p_data) {
	return String::utf8((const char *)p_data.ptr(), p_data.size());
}

// Writes a pack with PCKPacker. `p_files` maps target paths to text content.
static void _write_pack(const String &p_path, const Dictionary &p_files, const String &p_key = String(), bool p_encrypt = false) {
	PCKPacker packer;
	const String key = p_key.is_empty() ? String("0000000000000000000000000000000000000000000000000000000000000000") : p_key;
	REQUIRE(packer.pck_start(p_path, 32, key, p_encrypt) == OK);
	for (const KeyValue<Variant, Variant> &kv : p_files) {
		REQUIRE(packer.add_file_from_buffer(kv.key, _bytes(kv.value), p_encrypt) == OK);
	}
	REQUIRE(packer.flush() == OK);
}

TEST_CASE("[PckPackageServer] Open, list, read and verify without mounting") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	REQUIRE(server);
	const String pack_path = TestUtils::get_temp_path("pck_package_open.pck");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["sub/b.txt"] = "Beta";
	_write_pack(pack_path, files);

	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());
	CHECK(package->get_pack_path() == pack_path);
	CHECK(package->get_format_version() == PACK_FORMAT_VERSION);
	CHECK_FALSE(package->get_engine_version().is_empty());
	CHECK(package->get_alignment() >= 32);
	CHECK_FALSE(package->is_encrypted_directory());
	CHECK(package->get_file_count() == 2);
	CHECK(package->get_paths() == PackedStringArray({ "res://a.txt", "res://sub/b.txt" }));
	CHECK(package->has_file("res://sub/b.txt"));
	CHECK(package->has_file("sub/b.txt"));

	const TypedArray<Dictionary> entries = package->get_entries();
	REQUIRE(entries.size() == 2);
	const Dictionary first = entries[0];
	CHECK(String(first["path"]) == "res://a.txt");
	CHECK(int64_t(first["size"]) == 5);
	CHECK(String(first["md5"]) == String("Alpha").md5_text());
	CHECK_FALSE(bool(first["encrypted"]));
	CHECK_FALSE(bool(first["pending"]));

	CHECK(_text(server->read_file(package, "res://a.txt")) == "Alpha");
	CHECK(_text(server->read_file(package, "sub/b.txt")) == "Beta");
	CHECK(server->verify(package) == OK);
	CHECK_FALSE(server->is_mounted(pack_path));
	CHECK_FALSE(FileAccess::exists("res://sub/b.txt"));

	ERR_PRINT_OFF;
	CHECK(server->read_file(package, "res://missing.txt").is_empty());
	CHECK(server->open(TestUtils::get_temp_path("pck_package_missing.pck")).is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[PckPackageServer] Edit in memory and build") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	const String pack_path = TestUtils::get_temp_path("pck_package_edit.pck");
	const String built_path = TestUtils::get_temp_path("pck_package_edit_built.pck");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["sub/b.txt"] = "Beta";
	_write_pack(pack_path, files);

	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());
	CHECK(package->add_file_from_buffer("res://c.txt", _bytes("Gamma")) == OK);
	CHECK(package->remove_file("res://a.txt") == OK);
	CHECK(package->rename_file("res://sub/b.txt", "res://sub/b2.txt") == OK);
	CHECK(package->mark_removal("res://old.txt") == OK);
	ERR_PRINT_OFF;
	CHECK(package->remove_file("res://a.txt") == ERR_DOES_NOT_EXIST);
	CHECK(package->rename_file("res://c.txt", "res://sub/b2.txt") == ERR_ALREADY_EXISTS);
	ERR_PRINT_ON;
	CHECK(_text(server->read_file(package, "res://c.txt")) == "Gamma");
	CHECK(package->get_paths() == PackedStringArray({ "res://c.txt", "res://sub/b2.txt" }));

	CHECK(server->build(package, built_path) == OK);
	Ref<PckPackage> built = server->open(built_path);
	REQUIRE(built.is_valid());
	CHECK(built->get_paths() == PackedStringArray({ "res://c.txt", "res://sub/b2.txt" }));
	CHECK(built->get_file_count() == 3); // Including the removal entry.
	CHECK(_text(server->read_file(built, "res://sub/b2.txt")) == "Beta");
	CHECK(_text(server->read_file(built, "res://c.txt")) == "Gamma");
	CHECK(server->verify(built) == OK);

	// Building over the pack of the package replaces it and re-reads it.
	CHECK(built->add_file_from_buffer("res://d.txt", _bytes("Delta")) == OK);
	CHECK(server->build(built, built_path) == OK);
	CHECK(built->get_pack_path() == built_path);
	const Dictionary rebuilt_first = built->get_entries()[0];
	CHECK_FALSE(bool(rebuilt_first["pending"]));
	CHECK(_text(server->read_file(built, "res://d.txt")) == "Delta");
	CHECK(_text(server->read_file(built, "res://c.txt")) == "Gamma");

	// A new package is an empty pack.
	Ref<PckPackage> empty;
	empty.instantiate();
	CHECK(empty->get_file_count() == 0);
	CHECK(empty->add_file_from_buffer("x.txt", _bytes("X")) == OK);
	CHECK(server->build(empty, TestUtils::get_temp_path("pck_package_new.pck")) == OK);
	CHECK(server->open(TestUtils::get_temp_path("pck_package_new.pck"))->get_file_count() == 1);
}

TEST_CASE("[PckPackageServer] Encrypted packs") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	const String pack_path = TestUtils::get_temp_path("pck_package_encrypted.pck");
	Dictionary files;
	files["secret.txt"] = "Hidden";
	_write_pack(pack_path, files, KEY_A, true);

	Ref<PckPackage> package = server->open(pack_path, KEY_A);
	REQUIRE(package.is_valid());
	CHECK(package->is_encrypted_directory());
	const Dictionary secret_entry = package->get_entries()[0];
	CHECK(bool(secret_entry["encrypted"]));
	CHECK(_text(server->read_file(package, "res://secret.txt")) == "Hidden");
	CHECK(server->verify(package) == OK);

	ERR_PRINT_OFF;
	CHECK(server->open(pack_path, KEY_B).is_null());
	CHECK(server->open(pack_path, "not a key").is_null());
	ERR_PRINT_ON;

	// Rebuild with another key: the files stay encrypted.
	const String rekeyed_path = TestUtils::get_temp_path("pck_package_rekeyed.pck");
	CHECK(server->build(package, rekeyed_path, KEY_B) == OK);
	Ref<PckPackage> rekeyed = server->open(rekeyed_path, KEY_B);
	REQUIRE(rekeyed.is_valid());
	CHECK(_text(server->read_file(rekeyed, "res://secret.txt")) == "Hidden");

	// Only the files encrypted, not the directory: the directory opens, the wrong key fails on read.
	const String files_only_path = TestUtils::get_temp_path("pck_package_files_only.pck");
	PCKPacker packer;
	REQUIRE(packer.pck_start(files_only_path, 32, KEY_A, false) == OK);
	REQUIRE(packer.add_file_from_buffer("secret.txt", _bytes("Hidden"), true) == OK);
	REQUIRE(packer.flush() == OK);
	Ref<PckPackage> wrong = server->open(files_only_path, KEY_B);
	REQUIRE(wrong.is_valid());
	ERR_PRINT_OFF;
	CHECK(server->read_file(wrong, "res://secret.txt").is_empty());
	CHECK(server->verify(wrong) != OK);
	ERR_PRINT_ON;
}

// An empty folder for extraction tests.
static String _clean_dir(const String &p_name) {
	const String dir = TestUtils::get_temp_path(p_name);
	Ref<DirAccess> da = DirAccess::open(dir);
	if (da.is_valid()) {
		da->erase_contents_recursive();
	}
	DirAccess::make_dir_recursive_absolute(dir);
	return dir;
}

TEST_CASE("[PckPackageServer] Extract skips engine files and reports not editable files") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	const String pack_path = TestUtils::get_temp_path("pck_extract_engine.pck");
	const String out = _clean_dir("pck_extract_engine_out");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["sub/b.txt"] = "Beta";
	files["art/icon.png"] = "PNG";
	files["art/icon.png.import"] = "[remap]\n";
	files["art/orphan.png.import"] = "[remap]\n";
	files[".godot/imported/icon.png-123.ctex"] = "CTEX";
	files["level.tscn.remap"] = "[remap]\npath=\"res://.godot/exported/level.scn\"\n";
	files["project.binary"] = "BIN";
	_write_pack(pack_path, files);
	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());

	Ref<PckExtractResult> result = server->extract(package, out);
	REQUIRE(result.is_valid());
	CHECK(result->is_ok());
	CHECK(result->get_errors().is_empty());
	CHECK(result->get_written() == PackedStringArray({ out.path_join("a.txt"), out.path_join("sub/b.txt"), out.path_join("art/icon.png"), out.path_join("art/icon.png.import") }));
	CHECK(result->get_skipped() == PackedStringArray({ "res://art/orphan.png.import", "res://.godot/imported/icon.png-123.ctex" }));
	CHECK(result->get_not_editable() == PackedStringArray({ "res://level.tscn", "res://project.binary" }));
	CHECK(result->get_renamed().is_empty());
	CHECK(FileAccess::get_file_as_string(out.path_join("sub/b.txt")) == "Beta");
	CHECK_FALSE(FileAccess::exists(out.path_join("level.tscn.remap")));
	CHECK_FALSE(DirAccess::exists(out.path_join(".godot")));

	// Without the engine file rules, everything is written as it is.
	const String raw_out = _clean_dir("pck_extract_engine_raw_out");
	Ref<PckExtractResult> raw = server->extract(package, raw_out, PackedStringArray(), PckPackageServer::EXTRACT_SKIP, false);
	CHECK(raw->is_ok());
	CHECK(raw->get_written().size() == 8);
	CHECK(FileAccess::get_file_as_string(raw_out.path_join(".godot/imported/icon.png-123.ctex")) == "CTEX");

	ERR_PRINT_OFF;
	Ref<PckPackage> null_package;
	CHECK(server->extract(null_package, out)->get_error() == ERR_INVALID_PARAMETER);
	CHECK(server->extract(package, "")->get_error() == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[PckPackageServer] Extract progress signal") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	const String pack_path = TestUtils::get_temp_path("pck_extract_progress.pck");
	const String out = _clean_dir("pck_extract_progress_out");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["b.txt"] = "Beta";
	_write_pack(pack_path, files);
	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());

	SIGNAL_WATCH(server, "extract_progress");
	CHECK(server->extract(package, out)->is_ok());
	SIGNAL_CHECK("extract_progress", Array({ Array({ "res://a.txt", 1, 2 }), Array({ "res://b.txt", 2, 2 }) }));
	SIGNAL_UNWATCH(server, "extract_progress");
}

TEST_CASE("[PckPackageServer] Extract filters and conflict policies") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	const String pack_path = TestUtils::get_temp_path("pck_extract_policy.pck");
	const String out = _clean_dir("pck_extract_policy_out");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["sub/b.txt"] = "Beta";
	files["sub/c.json"] = "{}";
	files["img.png"] = "PNG";
	files["img.png.import"] = "[remap]\n";
	_write_pack(pack_path, files);
	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());

	// Filters: folders, patterns and paths with or without `res://`.
	CHECK(server->extract(package, out, PackedStringArray({ "res://sub/" }))->get_written() == PackedStringArray({ out.path_join("sub/b.txt"), out.path_join("sub/c.json") }));
	CHECK(server->extract(package, out, PackedStringArray({ "*.txt" }), PckPackageServer::EXTRACT_OVERWRITE)->get_written() == PackedStringArray({ out.path_join("a.txt"), out.path_join("sub/b.txt") }));
	CHECK(server->extract(package, out, PackedStringArray({ "img.png" }))->get_written() == PackedStringArray({ out.path_join("img.png"), out.path_join("img.png.import") }));

	// Skip: existing files stay, and sidecars follow their source.
	{
		Ref<FileAccess> f = FileAccess::open(out.path_join("a.txt"), FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("Edited");
	}
	Ref<PckExtractResult> skipped = server->extract(package, out);
	CHECK(skipped->is_ok());
	CHECK(skipped->get_written().is_empty());
	CHECK(skipped->get_skipped().size() == 5);
	CHECK(FileAccess::get_file_as_string(out.path_join("a.txt")) == "Edited");

	// Overwrite.
	Ref<PckExtractResult> overwritten = server->extract(package, out, PackedStringArray({ "a.txt" }), PckPackageServer::EXTRACT_OVERWRITE);
	CHECK(overwritten->get_written() == PackedStringArray({ out.path_join("a.txt") }));
	CHECK(FileAccess::get_file_as_string(out.path_join("a.txt")) == "Alpha");

	// Rename: the sidecar gets the new name of its source.
	Ref<PckExtractResult> renamed = server->extract(package, out, PackedStringArray({ "a.txt", "img.png" }), PckPackageServer::EXTRACT_RENAME);
	CHECK(renamed->is_ok());
	CHECK(renamed->get_written() == PackedStringArray({ out.path_join("a_2.txt"), out.path_join("img_2.png"), out.path_join("img_2.png.import") }));
	const Dictionary renamed_map = renamed->get_renamed();
	CHECK(String(renamed_map["res://a.txt"]) == out.path_join("a_2.txt"));
	CHECK(String(renamed_map["res://img.png.import"]) == out.path_join("img_2.png.import"));
	CHECK(FileAccess::get_file_as_string(out.path_join("a_2.txt")) == "Alpha");

	// A second rename picks the next free name.
	CHECK(server->extract(package, out, PackedStringArray({ "a.txt" }), PckPackageServer::EXTRACT_RENAME)->get_written() == PackedStringArray({ out.path_join("a_3.txt") }));
}

TEST_CASE("[PckPackageServer] Extract keeps free UIDs and drops colliding ones") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	ResourceUID *uids = ResourceUID::get_singleton();
	REQUIRE(uids);
	const String pack_path = TestUtils::get_temp_path("pck_extract_uid.pck");
	const String out = _clean_dir("pck_extract_uid_out");

	const ResourceUID::ID taken_script = uids->create_id();
	const ResourceUID::ID taken_resource = uids->create_id();
	const ResourceUID::ID free_script = uids->create_id();
	uids->add_id(taken_script, "res://some/other_script.gd");
	uids->add_id(taken_resource, "res://some/other_resource.tres");
	const String taken_script_text = uids->id_to_text(taken_script);
	const String taken_resource_text = uids->id_to_text(taken_resource);
	const String free_script_text = uids->id_to_text(free_script);

	Dictionary files;
	files["taken.gd"] = "extends Node";
	files["taken.gd.uid"] = taken_script_text + "\n";
	files["free.gd"] = "extends Node";
	files["free.gd.uid"] = free_script_text + "\n";
	files["data.tres"] = "[gd_resource type=\"Resource\" format=3 uid=\"" + taken_resource_text + "\"]\n\n[resource]\n";
	_write_pack(pack_path, files);
	Ref<PckPackage> package = server->open(pack_path);
	REQUIRE(package.is_valid());

	Ref<PckExtractResult> result = server->extract(package, out);
	CHECK(result->is_ok());
	CHECK(result->get_dropped_uids() == PackedStringArray({ taken_script_text, taken_resource_text }));
	CHECK(FileAccess::exists(out.path_join("taken.gd")));
	CHECK_FALSE(FileAccess::exists(out.path_join("taken.gd.uid")));
	CHECK(result->get_skipped() == PackedStringArray({ "res://taken.gd.uid" }));
	CHECK(FileAccess::get_file_as_string(out.path_join("free.gd.uid")).strip_edges() == free_script_text);
	CHECK(FileAccess::get_file_as_string(out.path_join("data.tres")) == "[gd_resource type=\"Resource\" format=3]\n\n[resource]\n");

	// Kept UIDs are registered at once.
	CHECK(uids->has_id(free_script));
	CHECK(uids->get_id_path(free_script) == ProjectSettings::get_singleton()->localize_path(out.path_join("free.gd")));

	// Extracting again to the same place keeps the UID: it already belongs to that file.
	Ref<PckExtractResult> again = server->extract(package, out, PackedStringArray({ "free.gd" }), PckPackageServer::EXTRACT_OVERWRITE);
	CHECK(again->get_dropped_uids().is_empty());
	CHECK(again->get_written().has(out.path_join("free.gd.uid")));

	uids->remove_id(taken_script);
	uids->remove_id(taken_resource);
	uids->remove_id(free_script);
}

TEST_CASE("[PckPackageServer] Mount, pack of a file, unmount") {
	PckPackageServer *server = PckPackageServer::get_singleton();
	PackedData *packed_data = PackedData::get_singleton();
	REQUIRE(packed_data);
	const String first_path = TestUtils::get_temp_path("pck_package_first.pck");
	const String second_path = TestUtils::get_temp_path("pck_package_second.pck");
	Dictionary first_files;
	first_files["pck_package_test/shared.txt"] = "First";
	first_files["pck_package_test/only_first.txt"] = "Only first";
	_write_pack(first_path, first_files);
	Dictionary second_files;
	second_files["pck_package_test/shared.txt"] = "Second";
	second_files["pck_package_test/only_second.txt"] = "Only second";
	_write_pack(second_path, second_files);

	// Mounted through PackedData directly: `mount()` goes through ProjectSettings, which switches
	// the default `res://` directory access for the whole test run.
	REQUIRE(packed_data->add_pack(first_path, true, 0) == OK);
	REQUIRE(packed_data->add_pack(second_path, true, 0) == OK);
	CHECK(server->is_mounted(first_path));
	CHECK(server->is_mounted(second_path));
	CHECK_FALSE(server->is_main_pack(second_path));
	CHECK(server->get_mounted().has(first_path));
	CHECK(server->get_mounted().has(second_path));
	CHECK(server->get_pack_of("res://pck_package_test/shared.txt") == second_path);
	CHECK(server->get_pack_of("res://pck_package_test/only_first.txt") == first_path);
	CHECK(server->get_pack_of("res://pck_package_test/missing.txt").is_empty());
	CHECK(FileAccess::get_file_as_string("res://pck_package_test/shared.txt") == "Second");

	// A mounted pack can't be overwritten.
	Ref<PckPackage> package = server->open(second_path);
	REQUIRE(package.is_valid());
	ERR_PRINT_OFF;
	CHECK(server->build(package, second_path) == ERR_BUSY);
	ERR_PRINT_ON;

	SIGNAL_WATCH(server, "pack_unmounted");
	CHECK(server->unmount(second_path) == OK);
	SIGNAL_CHECK("pack_unmounted", { { second_path } });
	SIGNAL_UNWATCH(server, "pack_unmounted");

	CHECK_FALSE(server->is_mounted(second_path));
	CHECK(server->is_mounted(first_path));
	CHECK_FALSE(FileAccess::exists("res://pck_package_test/only_second.txt"));
	CHECK(FileAccess::exists("res://pck_package_test/only_first.txt"));
	CHECK(server->get_pack_of("res://pck_package_test/shared.txt") == first_path);
	CHECK(FileAccess::get_file_as_string("res://pck_package_test/shared.txt") == "First");

	ERR_PRINT_OFF;
	CHECK(server->unmount(second_path) == ERR_DOES_NOT_EXIST);
	CHECK(server->unmount("res://") == ERR_DOES_NOT_EXIST);
	ERR_PRINT_ON;

	// Main packs stay.
	packed_data->mark_main_pack(first_path);
	CHECK(server->is_main_pack(first_path));
	ERR_PRINT_OFF;
	CHECK(server->unmount(first_path) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;

	CHECK(packed_data->remove_pack(first_path) == OK);
	CHECK_FALSE(server->is_mounted(first_path));
	CHECK_FALSE(FileAccess::exists("res://pck_package_test/only_first.txt"));
}

} // namespace TestPckPackageServer
