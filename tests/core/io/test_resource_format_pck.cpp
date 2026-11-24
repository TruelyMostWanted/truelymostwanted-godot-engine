/**************************************************************************/
/*  test_resource_format_pck.cpp                                          */
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

TEST_FORCE_LINK(test_resource_format_pck)

#include "core/io/file_access.h"
#include "core/io/pck_package_server.h"
#include "core/io/pck_packer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "tests/test_utils.h"

namespace TestResourceFormatPck {

static const String KEY_A = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

static PackedByteArray _bytes(const String &p_text) {
	return p_text.to_utf8_buffer();
}

static String _text(const PackedByteArray &p_data) {
	return String::utf8((const char *)p_data.ptr(), p_data.size());
}

// Writes a pack with PCKPacker and the key compiled into the engine. `p_files` maps target paths to text content.
static void _write_pack(const String &p_path, const Dictionary &p_files) {
	Vector<uint8_t> key;
	REQUIRE(PCKPackage::decode_key(String(), key) == OK);
	PCKPacker packer;
	REQUIRE(packer.pck_start(p_path, 32, String::hex_encode_buffer(key.ptr(), key.size()), false) == OK);
	for (const KeyValue<Variant, Variant> &kv : p_files) {
		REQUIRE(packer.add_file_from_buffer(kv.key, _bytes(kv.value), false) == OK);
	}
	REQUIRE(packer.flush() == OK);
}

TEST_CASE("[ResourceFormatPck] Load a .pck as PCKPackage without mounting it") {
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	REQUIRE(server);
	const String pack_path = TestUtils::get_temp_path("resource_format_pck_load.pck");
	Dictionary files;
	files["a.txt"] = "Alpha";
	files["sub/b.txt"] = "Beta";
	_write_pack(pack_path, files);

	CHECK(ResourceLoader::get_resource_type(pack_path) == "PCKPackage");
	Error err = FAILED;
	Ref<PCKPackage> package = ResourceLoader::load(pack_path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	CHECK(err == OK);
	REQUIRE(package.is_valid());
	CHECK_FALSE(package->is_description());
	CHECK(package->get_pack_path() == pack_path);
	CHECK(package->get_base_pack() == pack_path);
	CHECK(package->get_file_paths() == PackedStringArray({ "res://a.txt", "res://sub/b.txt" }));
	CHECK(_text(server->read_file(package, "res://sub/b.txt")) == "Beta");
	CHECK_FALSE(server->is_mounted(pack_path));
	CHECK_FALSE(FileAccess::exists("res://sub/b.txt"));

	// Loaded resources are cached by path.
	Ref<PCKPackage> again = ResourceLoader::load(pack_path);
	CHECK(again == package);

	// CACHE_MODE_REPLACE after a rebuild: the cached object sees the new entries and offsets.
	Dictionary new_files;
	new_files["a.txt"] = "Alpha 2";
	new_files["c.txt"] = "Gamma";
	_write_pack(pack_path, new_files);
	Ref<PCKPackage> replaced = ResourceLoader::load(pack_path, "", ResourceFormatLoader::CACHE_MODE_REPLACE);
	CHECK(replaced == package);
	CHECK(package->get_file_paths() == PackedStringArray({ "res://a.txt", "res://c.txt" }));
	CHECK(_text(server->read_file(package, "res://a.txt")) == "Alpha 2");

	// reload_from_file() opens the pack again.
	_write_pack(pack_path, files);
	package->reload_from_file();
	CHECK(package->get_file_paths() == PackedStringArray({ "res://a.txt", "res://sub/b.txt" }));
	CHECK(_text(server->read_file(package, "res://a.txt")) == "Alpha");

	// duplicate() keeps the opened pack and its entries.
	Ref<PCKPackage> copy = package->duplicate();
	REQUIRE(copy.is_valid());
	CHECK(copy != package);
	CHECK(copy->get_pack_path() == pack_path);
	CHECK(copy->get_file_paths() == package->get_file_paths());
	CHECK(_text(server->read_file(copy, "res://sub/b.txt")) == "Beta");

	ERR_PRINT_OFF;
	CHECK(ResourceLoader::load(TestUtils::get_temp_path("resource_format_pck_missing.pck")).is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[ResourceFormatPck] Pack descriptions in .tres") {
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	const String base_path = TestUtils::get_temp_path("resource_format_pck_base.pck");
	const String source_path = TestUtils::get_temp_path("resource_format_pck_source.txt");
	const String description_path = TestUtils::get_temp_path("resource_format_pck_description.tres");
	Dictionary files;
	files["old/b.txt"] = "Beta";
	files["old/c.txt"] = "Gamma";
	_write_pack(base_path, files);
	{
		Ref<FileAccess> f = FileAccess::open(source_path, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("Source");
	}

	Ref<PCKPackage> description;
	description.instantiate();
	CHECK(description->is_description());
	CHECK(description->get_pack_path().is_empty());
	CHECK(description->set_key(KEY_A) == OK);
	description->set_base_pack(base_path);
	description->set_alignment(64);
	CHECK(description->add_file("textures/a.txt", source_path, true) == OK);
	CHECK(description->add_file_from_buffer("config.json", _bytes("{}")) == OK);
	description->set("files", description->get("files")); // Round trip through the storage property.
	Array stored_files = description->get("files");
	Dictionary from_pack;
	from_pack["path"] = "new/b.txt";
	from_pack["from_pack"] = "old/b.txt";
	stored_files.push_back(from_pack);
	description->set("files", stored_files);
	CHECK(description->add_removal("old/c.txt") == OK);

	const TypedArray<PCKPackageEntry> entries = description->get_entries();
	REQUIRE(entries.size() == 4);
	const Ref<PCKPackageEntry> file_entry = entries[0];
	REQUIRE(file_entry.is_valid());
	CHECK(file_entry->get_source_type() == PCKPackage::SOURCE_FILE);
	CHECK(file_entry->get_source() == source_path);
	CHECK(file_entry->is_encrypted());
	const Ref<PCKPackageEntry> pack_entry = entries[2];
	REQUIRE(pack_entry.is_valid());
	CHECK(pack_entry->get_source_type() == PCKPackage::SOURCE_PACK);
	CHECK(pack_entry->get_source() == "res://old/b.txt");
	CHECK(pack_entry->is_pending());
	const Ref<PCKPackageEntry> removal_entry = entries[3];
	REQUIRE(removal_entry.is_valid());
	CHECK(removal_entry->is_removal());
	CHECK(removal_entry->get_path() == "res://old/c.txt");

	// Saved as text and loaded again: the same description, without the key.
	REQUIRE(ResourceSaver::save(description, description_path) == OK);
	const String text = FileAccess::get_file_as_string(description_path);
	CHECK(text.contains("base_pack"));
	CHECK(text.contains("from_pack"));
	CHECK_FALSE(text.contains(KEY_A));
	CHECK_FALSE(text.contains("pack_path"));

	Ref<PCKPackage> loaded = ResourceLoader::load(description_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->is_description());
	CHECK(loaded->get_base_pack() == base_path);
	CHECK(loaded->get_alignment() == 64);
	CHECK(loaded->get_file_paths() == description->get_file_paths());
	const Array loaded_files = loaded->get("files");
	const Array description_files = description->get("files");
	CHECK(loaded_files.size() == 3);
	CHECK(loaded_files == description_files);
	const PackedStringArray loaded_removals = loaded->get("removals");
	CHECK(loaded_removals == PackedStringArray({ "old/c.txt" }));
	CHECK(loaded->get_key().is_empty());
	CHECK(_text(server->read_file(loaded, "res://new/b.txt")) == "Beta");
	CHECK(_text(server->read_file(loaded, "res://textures/a.txt")) == "Source");

	// duplicate() keeps the entries.
	Ref<PCKPackage> copy = loaded->duplicate();
	REQUIRE(copy.is_valid());
	const Array copy_files = copy->get("files");
	CHECK(copy_files == loaded_files);
	CHECK(copy->get_file_count() == loaded->get_file_count());

	// Saving to .pck builds the pack.
	const String built_path = TestUtils::get_temp_path("resource_format_pck_built.pck");
	REQUIRE(ResourceSaver::save(loaded, built_path) == OK);
	Ref<PCKPackage> built = server->open(built_path);
	REQUIRE(built.is_valid());
	CHECK(built->get_file_paths() == PackedStringArray({ "res://textures/a.txt", "res://config.json", "res://new/b.txt" }));
	CHECK(built->has_file("res://old/c.txt")); // The removal entry.
	CHECK(_text(server->read_file(built, "res://new/b.txt")) == "Beta");
	CHECK(_text(server->read_file(built, "res://textures/a.txt")) == "Source");
	CHECK(server->verify(built) == OK);
	CHECK(loaded->is_description());
}

TEST_CASE("[ResourceFormatPck] Missing sources are reported by build") {
	PCKPackageServer *server = PCKPackageServer::get_singleton();
	const String missing_source = TestUtils::get_temp_path("resource_format_pck_not_there.txt");
	const String target_path = TestUtils::get_temp_path("resource_format_pck_missing_source.pck");
	Ref<PCKPackage> description;
	description.instantiate();
	// Recording a missing source is allowed: the file may be created later.
	CHECK(description->add_file("a.txt", missing_source) == OK);
	CHECK(description->has_file("res://a.txt"));

	ERR_PRINT_OFF;
	CHECK(server->build(description, target_path) == ERR_FILE_NOT_FOUND);
	ERR_PRINT_ON;
	CHECK_FALSE(FileAccess::exists(target_path));

	{
		Ref<FileAccess> f = FileAccess::open(missing_source, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("Now here");
	}
	CHECK(server->build(description, target_path) == OK);
	Ref<PCKPackage> built = server->open(target_path);
	REQUIRE(built.is_valid());
	CHECK(_text(server->read_file(built, "res://a.txt")) == "Now here");
}

} // namespace TestResourceFormatPck
