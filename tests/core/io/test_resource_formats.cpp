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

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"

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

} // namespace TestResourceFormats
