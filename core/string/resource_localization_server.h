/**************************************************************************/
/*  resource_localization_server.h                                        */
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

#pragma once

#include "core/io/resource.h"
#include "core/object/worker_thread_pool.h"
#include "core/string/localization_server.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/typed_dictionary.h"

// Localized resource paths from ResourceLocalizationCatalogs (#344); keys are the original paths. On a locale or
// catalog change the paths of the active locale are resolved into one snapshot that ResourceLoader asks on every
// load() (lock-free). Cached resources whose path changed are loaded again in the background and updated in place.
// Replaces `internationalization/locale/translation_remaps`, which is imported as the first project layer.
class ResourceLocalizationServer : public LocalizationServer {
	GDCLASS(ResourceLocalizationServer, LocalizationServer);

	static ResourceLocalizationServer *singleton;

	struct ResourceData : public Data {
		HashMap<String, String> paths; // Every key -> its path for the active locale (the key itself if none).
	};

	struct Candidate {
		uint32_t layer = 0;
		const LocalizationCatalog::LocaleColumn *column = nullptr;
		int rank = 0;
	};

	struct Relocalization {
		PackedStringArray paths;
		LocalVector<Ref<Resource>> resources; // Loaded on the workers, copied into the cached ones on the main thread.
		WorkerThreadPool::GroupID group = -1;
		SafeNumeric<uint32_t> remaining;
	};

	Relocalization *relocalization = nullptr;
	PackedStringArray pending_paths;
	Ref<ResourceLocalizationCatalog> remaps_catalog;

	_FORCE_INLINE_ const ResourceData *_get_resource_data() const { return static_cast<const ResourceData *>(_get_data()); }
	static void _get_candidates(const KeyTable *p_keys, const String &p_locale, const PackedStringArray &p_chain, LocalVector<Candidate> &r_candidates);
	static String _find_path(const KeyTable *p_keys, const LocalVector<Candidate> &p_candidates, uint32_t p_row);
	static bool _localize_path(const String &p_path, String &r_path);
	static void _relocalize_resource(void *p_userdata, uint32_t p_index);
	void _relocalize(const PackedStringArray &p_paths);
	void _relocalization_done();

protected:
	static void _bind_methods();

	virtual StringName _get_catalog_class() const override { return SNAME("ResourceLocalizationCatalog"); }
	virtual Data *_create_data() const override { return memnew(ResourceData); }
	virtual void _resolve(Data *p_data) const override;
	virtual void _data_published(const Data *p_data, const Data *p_previous) override;
	virtual const char *_get_catalogs_setting() const override { return SETTING_RESOURCE_CATALOGS; }
	virtual const char *_get_external_catalogs_setting() const override { return SETTING_RESOURCE_EXTERNAL_CATALOGS; }
	virtual Ref<LocalizationCatalog> _get_project_catalog() override;

public:
	static constexpr const char *SETTING_RESOURCE_CATALOGS = "internationalization/localization/resource_catalogs";
	static constexpr const char *SETTING_RESOURCE_EXTERNAL_CATALOGS = "internationalization/localization/resource_external_catalogs";
	static constexpr const char *SETTING_TRANSLATION_REMAPS = "internationalization/locale/translation_remaps";

	static ResourceLocalizationServer *get_singleton() { return singleton; }
	static void register_settings();

	String get_localized_path(const String &p_path, const String &p_locale = String()) const;
	bool has_localization(const String &p_path) const;
	TypedDictionary<String, String> get_localized_paths(const String &p_path) const;
	PackedStringArray get_missing_locales(const String &p_path) const;
	bool is_relocalizing() const { return relocalization != nullptr; }

	ResourceLocalizationServer();
	~ResourceLocalizationServer();
};
