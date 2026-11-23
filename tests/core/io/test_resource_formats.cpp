/**************************************************************************/
/*  test_resource_formats.cpp                                             */
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

TEST_FORCE_LINK(test_resource_formats)

#include "core/io/resource_format_binary.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "tests/test_utils.h"

namespace TestResourceFormats {

// Recognizes one extension and returns a new Resource that records which loader created it.
class LimitTestLoader : public ResourceFormatLoader {
public:
	String extension;
	int id = 0;
	uint32_t delay_usec = 0;

	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override {
		if (delay_usec > 0) {
			OS::get_singleton()->delay_usec(delay_usec);
		}
		Ref<Resource> res;
		res.instantiate();
		res->set_meta("loader_id", id);
		if (r_error) {
			*r_error = OK;
		}
		return res;
	}
	virtual void get_recognized_extensions(List<String> *p_extensions) const override {
		p_extensions->push_back(extension);
	}
	virtual bool handles_type(const String &p_type) const override {
		return p_type == "Resource";
	}
	virtual String get_resource_type(const String &p_path) const override {
		return p_path.has_extension(extension) ? "Resource" : "";
	}
	virtual bool exists(const String &p_path) const override {
		return true;
	}
};

// Recognizes one extension and counts its calls instead of writing a file.
class LimitTestSaver : public ResourceFormatSaver {
public:
	String extension;
	int saved = 0;

	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override {
		saved++;
		return OK;
	}
	virtual bool recognize(const Ref<Resource> &p_resource) const override {
		return true;
	}
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override {
		p_extensions->push_back(extension);
	}
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const override {
		return p_path.has_extension(extension);
	}
};

static Ref<LimitTestLoader> make_loader(const String &p_extension, int p_id) {
	Ref<LimitTestLoader> loader;
	loader.instantiate();
	loader->extension = p_extension;
	loader->id = p_id;
	return loader;
}

static int loaded_id(const String &p_path) {
	Ref<Resource> res = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	return res.is_valid() ? int(res->get_meta("loader_id", -1)) : -1;
}

TEST_CASE("[ResourceLoader][ResourceSaver] More than 64 loaders and savers can be registered and used") {
	const int count = 200;
	Vector<Ref<LimitTestLoader>> loaders;
	Vector<Ref<LimitTestSaver>> savers;
	for (int i = 0; i < count; i++) {
		Ref<LimitTestLoader> loader = make_loader(vformat("fmtlimit%d", i), i);
		ResourceLoader::add_resource_format_loader(loader);
		loaders.push_back(loader);

		Ref<LimitTestSaver> saver;
		saver.instantiate();
		saver->extension = vformat("fmtlimit%d", i);
		ResourceSaver::add_resource_format_saver(saver);
		savers.push_back(saver);
	}

	int loaded = 0;
	int saved = 0;
	Ref<Resource> res;
	res.instantiate();
	for (int i = 0; i < count; i++) {
		const String path = vformat("res://format_limit_test.fmtlimit%d", i);
		if (loaded_id(path) == i) {
			loaded++;
		}
		if (ResourceSaver::save(res, path) == OK && savers[i]->saved == 1) {
			saved++;
		}
	}
	CHECK_MESSAGE(loaded == count, "Every one of the 200 loaders is used for its extension.");
	CHECK_MESSAGE(saved == count, "Every one of the 200 savers is used for its extension.");

	for (int i = 0; i < count; i++) {
		ResourceLoader::remove_resource_format_loader(loaders[i]);
		ResourceSaver::remove_resource_format_saver(savers[i]);
	}
	ERR_PRINT_OFF;
	CHECK_MESSAGE(loaded_id("res://format_limit_test.fmtlimit0") == -1, "Removed loaders are no longer used.");
	ERR_PRINT_ON;
}

TEST_CASE("[ResourceLoader] A loader added at the front wins over older loaders") {
	Ref<LimitTestLoader> older = make_loader("fmtorder", 1);
	Ref<LimitTestLoader> front = make_loader("fmtorder", 2);
	Ref<LimitTestLoader> back = make_loader("fmtorder", 3);
	ResourceLoader::add_resource_format_loader(older);
	ResourceLoader::add_resource_format_loader(back);
	CHECK_MESSAGE(loaded_id("res://order.fmtorder") == 1, "The first registered loader wins.");

	ResourceLoader::add_resource_format_loader(front, true);
	CHECK_MESSAGE(loaded_id("res://order.fmtorder") == 2, "A loader added with at_front wins.");

	ResourceLoader::remove_resource_format_loader(front);
	CHECK_MESSAGE(loaded_id("res://order.fmtorder") == 1, "After removing the front loader, the older one wins again.");

	ResourceLoader::remove_resource_format_loader(older);
	ResourceLoader::remove_resource_format_loader(back);
}

TEST_CASE("[ResourceLoader] Threaded loads while loaders are added and removed") {
	Ref<LimitTestLoader> stable = make_loader("fmtthread", 7);
	stable->delay_usec = 200;
	ResourceLoader::add_resource_format_loader(stable);

	const int requests = 32;
	for (int i = 0; i < requests; i++) {
		ResourceLoader::load_threaded_request(vformat("res://thread_%d.fmtthread", i), "", false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	}

	// Change the list while the loads run on worker threads.
	for (int round = 0; round < 20; round++) {
		Vector<Ref<LimitTestLoader>> churn;
		for (int i = 0; i < 100; i++) {
			Ref<LimitTestLoader> loader = make_loader(vformat("fmtchurn%d", i), 1000 + i);
			ResourceLoader::add_resource_format_loader(loader, (i % 2) == 0);
			churn.push_back(loader);
		}
		for (const Ref<LimitTestLoader> &loader : churn) {
			ResourceLoader::remove_resource_format_loader(loader);
		}
	}

	int valid = 0;
	for (int i = 0; i < requests; i++) {
		Ref<Resource> res = ResourceLoader::load_threaded_get(vformat("res://thread_%d.fmtthread", i));
		if (res.is_valid() && int(res->get_meta("loader_id", -1)) == 7) {
			valid++;
		}
	}
	CHECK_MESSAGE(valid == requests, "Every threaded load finished with the stable loader.");

	ResourceLoader::remove_resource_format_loader(stable);
}

// Recognizes a list of extensions (or, when path based, every path ending with `ending`) without loading anything.
class IndexTestLoader : public ResourceFormatLoader {
public:
	Vector<String> extensions;
	String ending; // Non-empty: recognized by path, not by extension.

	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override {
		return Ref<Resource>();
	}
	virtual void get_recognized_extensions(List<String> *p_extensions) const override {
		for (const String &E : extensions) {
			p_extensions->push_back(E);
		}
	}
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override {
		return ending.is_empty() ? ResourceFormatLoader::recognize_path(p_path, p_for_type) : p_path.ends_with(ending);
	}
	virtual bool recognizes_path_by_extension() const override {
		return ending.is_empty();
	}
	virtual bool handles_type(const String &p_type) const override {
		return true;
	}
};

static Ref<IndexTestLoader> make_index_loader(const Vector<String> &p_extensions, const String &p_ending = String()) {
	Ref<IndexTestLoader> loader;
	loader.instantiate();
	loader->extensions = p_extensions;
	loader->ending = p_ending;
	return loader;
}

// Positions of the loaders that recognize the path: through the index, and by asking every loader.
static Vector<int> recognized_by_index(const ResourceLoaderFormatSnapshot &p_snapshot, const String &p_path, const String &p_type = String()) {
	Vector<int> ret;
	for (const Ref<ResourceFormatLoader> &format_loader : p_snapshot.candidates_for(p_path)) {
		if (format_loader->recognize_path(p_path, p_type)) {
			ret.push_back(p_snapshot.formats.find(format_loader));
		}
	}
	return ret;
}

static Vector<int> recognized_by_all(const ResourceLoaderFormatSnapshot &p_snapshot, const String &p_path, const String &p_type = String()) {
	Vector<int> ret;
	for (int i = 0; i < p_snapshot.formats.size(); i++) {
		if (p_snapshot.formats[i]->recognize_path(p_path, p_type)) {
			ret.push_back(i);
		}
	}
	return ret;
}

TEST_CASE("[ResourceLoader] Extension index keeps the order of the loaders") {
	ResourceLoaderFormatSnapshot snapshot;
	snapshot.formats.push_back(make_index_loader({ "aaa", "bbb" }));
	snapshot.formats.push_back(make_index_loader({}, ".special.bbb"));
	snapshot.formats.push_back(make_index_loader({ "BBB" }));
	snapshot.formats.push_back(make_index_loader({ "tar.gz", ".dotted" }));
	snapshot.formats.push_back(make_index_loader({ "aaa" }));
	snapshot.formats.push_back(make_index_loader({}, "/plain"));
	snapshot.build_index();

	CHECK(snapshot.always_asked == Vector<uint32_t>({ 1, 5 }));
	CHECK(snapshot.index_extensions == Vector<String>({ "aaa", "bbb", "dotted", "gz" }));

	CHECK(recognized_by_index(snapshot, "res://x.aaa") == Vector<int>({ 0, 4 }));
	CHECK(recognized_by_index(snapshot, "res://x.bbb") == Vector<int>({ 0, 2 }));
	CHECK(recognized_by_index(snapshot, "res://x.special.bbb") == Vector<int>({ 0, 1, 2 }));
	CHECK(recognized_by_index(snapshot, "res://X.AaA") == Vector<int>({ 0, 4 }));
	CHECK(recognized_by_index(snapshot, "res://x.tar.gz") == Vector<int>({ 3 }));
	CHECK(recognized_by_index(snapshot, "res://x.gz").is_empty());
	CHECK(recognized_by_index(snapshot, "res://x.DOTTED") == Vector<int>({ 3 }));
	CHECK(recognized_by_index(snapshot, "res://dir.aaa/plain") == Vector<int>({ 5 }));
	CHECK(recognized_by_index(snapshot, "res://none.zzz").is_empty());

	const char *paths[] = { "res://x.aaa", "res://x.bbb", "res://x.special.bbb", "res://X.AaA", "res://x.tar.gz", "res://x.gz", "res://x.DOTTED", "res://dir.aaa/plain", "res://none.zzz", "res://noext", "res://x.", "" };
	for (const char *path : paths) {
		CHECK_MESSAGE(recognized_by_index(snapshot, path) == recognized_by_all(snapshot, path), path);
	}

	// Candidates are visited in registration order: indexed and always-asked loaders are merged.
	Vector<int> order;
	for (const Ref<ResourceFormatLoader> &format_loader : snapshot.candidates_for("res://x.bbb")) {
		order.push_back(snapshot.formats.find(format_loader));
	}
	CHECK(order == Vector<int>({ 0, 1, 2, 5 }));
}

TEST_CASE("[ResourceLoader] Extension index finds the same loaders as asking every loader") {
	const ResourceLoaderFormatSnapshot snapshot = ResourceLoader::get_format_snapshot();
	REQUIRE(snapshot.formats.size() > 0);

	Vector<String> paths = { "res://icon.png", "res://icon.png.import", "res://scene.tscn", "res://data.res", "res://noext", "res://dir.tres/file", "res://script.gd", "res://script.cs" };
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("", &extensions);
	for (const String &E : extensions) {
		paths.push_back("res://test." + E);
		paths.push_back("res://TEST." + E.to_upper());
		paths.push_back("res://dir/sub.test." + E);
	}

	const char *types[] = { "", "Resource", "PackedScene", "Texture2D", "Script", "Image" };
	int mismatches = 0;
	for (const String &path : paths) {
		for (const char *type : types) {
			if (recognized_by_index(snapshot, path, type) != recognized_by_all(snapshot, path, type)) {
				mismatches++;
				MESSAGE("Different loaders for ", path, " with type hint ", type);
			}
		}
	}
	CHECK(mismatches == 0);

	ResourceLoader::add_resource_format_loader(make_index_loader({ "idxfront" }), true);
	const ResourceLoaderFormatSnapshot changed = ResourceLoader::get_format_snapshot();
	CHECK_MESSAGE(recognized_by_index(changed, "res://a.idxfront") == Vector<int>({ 0 }), "A loader added at the front is indexed at position 0.");
	CHECK_MESSAGE(recognized_by_index(snapshot, "res://a.idxfront").is_empty(), "An older snapshot keeps its own index.");
	ResourceLoader::remove_resource_format_loader(changed.formats[0]);
	CHECK(recognized_by_index(ResourceLoader::get_format_snapshot(), "res://a.idxfront").is_empty());
}

// The first non-empty type through the candidates of the path, and by asking every loader.
static String type_by_index(const ResourceLoaderFormatSnapshot &p_snapshot, const String &p_path) {
	for (const Ref<ResourceFormatLoader> &format_loader : p_snapshot.candidates_for(p_path)) {
		const String type = format_loader->get_resource_type(p_path);
		if (!type.is_empty()) {
			return type;
		}
	}
	return String();
}

static String type_by_all(const ResourceLoaderFormatSnapshot &p_snapshot, const String &p_path) {
	for (const Ref<ResourceFormatLoader> &format_loader : p_snapshot) {
		const String type = format_loader->get_resource_type(p_path);
		if (!type.is_empty()) {
			return type;
		}
	}
	return String();
}

TEST_CASE("[ResourceLoader] Extension index reports the same resource types as asking every loader") {
	const String saved = TestUtils::get_temp_path("index_type.res");
	Ref<Resource> resource;
	resource.instantiate();
	REQUIRE(ResourceSaver::save(resource, saved) == OK);

	Vector<String> paths = { saved, "res://icon.png", "res://icon.png.import", "res://noext", "res://dir.tres/file" };
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("", &extensions);
	for (const String &E : extensions) {
		paths.push_back("res://test." + E);
		paths.push_back("res://TEST." + E.to_upper());
	}

	const ResourceLoaderFormatSnapshot snapshot = ResourceLoader::get_format_snapshot();
	int mismatches = 0;
	for (const String &path : paths) {
		if (type_by_index(snapshot, path) != type_by_all(snapshot, path)) {
			mismatches++;
			MESSAGE("Different type for ", path);
		}
	}
	CHECK(mismatches == 0);
	CHECK(ResourceLoader::get_resource_type(saved) == "Resource");
}

TEST_CASE("[ResourceLoader] The binary loader is indexed and follows new ClassDB resource extensions") {
	int binary = -1;
	ResourceLoaderFormatSnapshot snapshot = ResourceLoader::get_format_snapshot();
	for (int i = 0; i < snapshot.formats.size(); i++) {
		if (Object::cast_to<ResourceFormatLoaderBinary>(snapshot.formats[i].ptr())) {
			binary = i;
			break;
		}
	}
	REQUIRE(binary >= 0);
	CHECK_MESSAGE(!snapshot.always_asked.has(uint32_t(binary)), "The binary loader is found through the index.");
	CHECK(recognized_by_index(snapshot, "res://a.res").has(binary));
	CHECK_FALSE(recognized_by_index(snapshot, "res://a.idxbintest").has(binary));

	const uint32_t version = ClassDB::get_resource_base_extensions_version();
	ClassDB::add_resource_base_extension("idxbintest", "X509Certificate");
	CHECK(ClassDB::get_resource_base_extensions_version() != version);
	snapshot = ResourceLoader::get_format_snapshot();
	CHECK_MESSAGE(recognized_by_index(snapshot, "res://a.idxbintest").has(binary), "The index is rebuilt when ClassDB adds a resource extension.");
	CHECK(recognized_by_index(snapshot, "res://a.idxbintest") == recognized_by_all(snapshot, "res://a.idxbintest"));

	const uint32_t same = ClassDB::get_resource_base_extensions_version();
	ClassDB::add_resource_base_extension("idxbintest", "X509Certificate");
	CHECK_MESSAGE(ClassDB::get_resource_base_extensions_version() == same, "Adding a known extension again changes nothing.");
}

TEST_CASE("[ResourceLoader] refresh_format_index() reads changed extension lists again") {
	Ref<IndexTestLoader> loader = make_index_loader({ "idxold" });
	ResourceLoader::add_resource_format_loader(loader, true);
	CHECK(recognized_by_index(ResourceLoader::get_format_snapshot(), "res://a.idxold") == Vector<int>({ 0 }));

	loader->extensions = { "idxnew", "IDXNEW2" };
	ResourceLoaderFormatSnapshot snapshot = ResourceLoader::get_format_snapshot();
	CHECK_MESSAGE(recognized_by_index(snapshot, "res://a.idxnew").is_empty(), "A changed list is not seen before the refresh.");
	CHECK_MESSAGE(recognized_by_index(snapshot, "res://a.idxold").is_empty(), "A removed extension is still checked with recognize_path().");

	ResourceLoader::refresh_format_index();
	snapshot = ResourceLoader::get_format_snapshot();
	CHECK(recognized_by_index(snapshot, "res://a.idxnew") == Vector<int>({ 0 }));
	CHECK(recognized_by_index(snapshot, "res://a.idxnew2") == Vector<int>({ 0 }));
	CHECK(recognized_by_index(snapshot, "res://a.idxold").is_empty());
	CHECK(snapshot.entries.size() == snapshot.formats.size());
	CHECK(snapshot.entries[0].keys == Vector<String>({ "idxnew", "idxnew2" }));
	CHECK_FALSE(snapshot.entries[0].scripted);

	ResourceLoader::remove_resource_format_loader(loader);
	snapshot = ResourceLoader::get_format_snapshot();
	CHECK(recognized_by_index(snapshot, "res://a.idxnew").is_empty());
	CHECK(snapshot.entries.size() == snapshot.formats.size());
}

// Run with --test --test-case="*Extension index measurement*" --no-skip; prints the time to resolve the loaders of 10,000 paths.
// The extra loaders are added at the front, so a lookup that asks every loader has to pass them.
TEST_CASE("[ResourceLoader] Extension index measurement" * doctest::skip()) {
	List<String> builtin_extensions;
	ResourceLoader::get_recognized_extensions_for_type("", &builtin_extensions);
	Vector<String> recognized;
	for (int i = 0; recognized.size() < 10000; i++) {
		for (const String &E : builtin_extensions) {
			recognized.push_back(vformat("res://measure/%d.%s", i, E));
		}
	}
	recognized.resize(10000);
	Vector<String> unrecognized;
	for (int i = 0; i < 10000; i++) {
		unrecognized.push_back(vformat("res://measure/%d.nosuchext", i));
	}

	for (int extra : { 0, 100 }) {
		Vector<Ref<IndexTestLoader>> added;
		for (int i = 0; i < extra; i++) {
			added.push_back(make_index_loader({ vformat("extra%d", i) }));
			ResourceLoader::add_resource_format_loader(added[i], true);
		}
		const ResourceLoaderFormatSnapshot snapshot = ResourceLoader::get_format_snapshot();

		for (int set = 0; set < 2; set++) {
			const Vector<String> &paths = set == 0 ? recognized : unrecognized;
			int found_all = 0;
			uint64_t start = Time::get_singleton()->get_ticks_usec();
			for (const String &path : paths) {
				for (const Ref<ResourceFormatLoader> &format_loader : snapshot) {
					if (format_loader->recognize_path(path)) {
						found_all++;
						break;
					}
				}
			}
			const uint64_t all_usec = Time::get_singleton()->get_ticks_usec() - start;

			int found_index = 0;
			start = Time::get_singleton()->get_ticks_usec();
			for (const String &path : paths) {
				for (const Ref<ResourceFormatLoader> &format_loader : snapshot.candidates_for(path)) {
					if (format_loader->recognize_path(path)) {
						found_index++;
						break;
					}
				}
			}
			const uint64_t index_usec = Time::get_singleton()->get_ticks_usec() - start;

			print_line(vformat("10000 %s paths, %d loaders (%d extra at the front): asking every loader %d usec, extension index %d usec", set == 0 ? "recognized" : "unrecognized", snapshot.formats.size(), extra, all_usec, index_usec));
			CHECK(found_all == found_index);
		}

		for (const Ref<IndexTestLoader> &loader : added) {
			ResourceLoader::remove_resource_format_loader(loader);
		}
	}
}

} // namespace TestResourceFormats
