/**************************************************************************/
/*  localization_server.h                                                 */
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

#include "core/object/object.h"
#include "core/os/thread.h"
#include "core/string/localization_catalog.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

#include <atomic>

class FilesManifest;

// Base of the localization servers (#343, #357): catalog layers by priority, one key index over all layers, the
// locale chains (`de_DE` -> `de` -> closest other locale -> fallback) and on-demand locales. The locale lives in
// TranslationServer, per channel (TranslationServer::get_locale_for()): every key follows the channel of the
// highest-priority catalog that has it, and a channel change rebuilds only the rows of that channel.
// Readers are lock-free: the data of the active chains is an immutable snapshot swapped atomically.
// Writers (catalog or locale changes) run on the main thread; calls from other threads are deferred.
class LocalizationServer : public Object {
	GDCLASS(LocalizationServer, Object);

public:
	static constexpr const char *SETTING_CATALOG_MANIFESTS = "internationalization/localization/catalog_manifests";
	// Export: the text catalogs of the manifests are written as binary `.lcat` (#346).
	static constexpr const char *SETTING_EXPORT_CONVERT_TEXT_TO_BINARY = "internationalization/localization/export/convert_text_to_binary";

	struct Key {
		StringName key;
		StringName context;

		bool operator==(const Key &p_other) const { return key == p_other.key && context == p_other.context; }
	};

	struct KeyHasher {
		static _FORCE_INLINE_ uint32_t hash(const Key &p_key) { return hash_murmur3_one_32(p_key.key.hash(), p_key.context.hash()); }
	};

	// Rows of all catalogs. Rows are only appended, so a row (key id) stays valid for the lifetime of the server.
	struct KeyTable {
		HashMap<Key, uint32_t, KeyHasher> index;
		LocalVector<Key> keys; // Row -> key.
		LocalVector<Ref<LocalizationCatalog>> layers; // Lowest priority first.
		LocalVector<int32_t> catalog_rows; // [layer * keys.size() + row] -> row in the layer's catalog, -1 = none.
		LocalVector<StringName> channels; // The channels of the rows.
		LocalVector<uint32_t> row_channels; // Row -> index in `channels`.
		LocalVector<LocalVector<uint32_t>> channel_rows; // Index in `channels` -> its rows.
		LocalVector<PackedStringArray> channel_locales; // Index in `channels` -> locales of the layers with its rows.
		PackedStringArray locales; // Of all layers.

		_FORCE_INLINE_ int32_t get_catalog_row(uint32_t p_layer, uint32_t p_row) const { return catalog_rows[p_layer * keys.size() + p_row]; }
		int find(const StringName &p_key, const StringName &p_context) const {
			const uint32_t *row = index.getptr(Key{ p_key, p_context });
			return row ? int(*row) : -1;
		}
	};

	// A locale column of a layer. A channel lists them in lookup order: the chain position first, then the layers
	// from the highest priority.
	struct Source {
		uint32_t layer = 0;
		int column = -1;
	};

	struct Channel {
		String locale;
		PackedStringArray chain;
		LocalVector<Source> sources; // The columns of the chain, found once per locale change.
	};

	// Data of the active locale chains. Immutable once published; subclasses add their resolved columns.
	struct Data {
		const KeyTable *keys = nullptr;
		LocalVector<Channel> channels; // Same order as KeyTable::channels.

		virtual ~Data() {}
	};

private:
	struct Layer {
		Ref<LocalizationCatalog> catalog;
		int priority = 0;
	};

	LocalVector<Layer> layers; // Sorted by priority; equal priorities keep the order of adding.
	KeyTable *keys = nullptr;
	std::atomic<Data *> data{ nullptr };
	LocalVector<Data *> retired_data;
	LocalVector<KeyTable *> retired_keys;
	bool free_queued = false;
	bool rebuild_queued = false;
	bool reload_queued = false;
	PackedStringArray chain_locales; // Locales loaded for the current chains.
	LocalVector<Ref<LocalizationCatalog>> project_catalogs; // Added by load_project_catalogs().
	PackedStringArray manifest_names; // Names of the project manifests in FilesManifestServer.

	void _add_layer(const Ref<LocalizationCatalog> &p_catalog, int p_priority);
	bool _remove_layer(const Ref<LocalizationCatalog> &p_catalog);
	bool _takes_manifest(const Ref<FilesManifest> &p_manifest) const;
	Ref<LocalizationCatalog> _load_catalog(const String &p_path) const;
	int _add_catalog_paths(const PackedStringArray &p_paths, int p_priority, bool p_project);
	void _on_catalog_changed();
	void _catalogs_changed();
	void _free_retired();
	void _connect_translation_server();
	void _on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale);
	void _on_fallback_locale_changed(const String &p_locale);
	void _update_locales(bool p_all);
	void _on_manifest_file(const String &p_name, const String &p_path);
	void _on_pack(const String &p_path);
	void _reload_project_catalogs();

protected:
	static void _bind_methods();

	// Class of the catalogs this server takes, e.g. "TextLocalizationCatalog".
	virtual StringName _get_catalog_class() const = 0;
	virtual Data *_create_data() const = 0;
	// Fills the subclass part of `p_data` from `p_data->keys` and `p_data->channels`. With `p_previous` (same key
	// table), only the channels marked in `p_dirty` changed: the rest is taken from `p_previous`.
	virtual void _resolve(Data *p_data, const Data *p_previous, const LocalVector<uint8_t> *p_dirty) const = 0;
	// `p_previous` (null at first) stays valid until the next idle frame.
	virtual void _data_published(const Data *p_data, const Data *p_previous) {}
	// A catalog of the subclass added by load_project_catalogs() before the manifests.
	virtual Ref<LocalizationCatalog> _get_project_catalog() { return Ref<LocalizationCatalog>(); }

	_FORCE_INLINE_ const Data *_get_data() const { return data.load(std::memory_order_acquire); }
	// Builds and publishes new data; `p_keys` also rebuilds the key table, `p_dirty` limits it to some channels.
	void _rebuild(bool p_keys, const LocalVector<uint8_t> *p_dirty = nullptr);
	// The chain walk for an explicit locale chain, on any thread (the columns are read under the catalog lock).
	// Returns the layer of the text, or -1.
	int _read_row(const KeyTable *p_keys, const PackedStringArray &p_chain, uint32_t p_row, String &r_text, Vector<String> *r_forms = nullptr, String *r_locale = nullptr) const;

	template <typename... VarArgs>
	void _emit_on_main_thread(const StringName &p_signal, VarArgs... p_args) {
		if (Thread::is_main_thread()) {
			emit_signal(p_signal, p_args...);
		} else {
			call_deferred(SNAME("emit_signal"), p_signal, p_args...);
		}
	}

public:
	static void register_settings();
	// A copy of a catalog manifest for listing its files: no type filter, the catalog formats without extensions.
	static Ref<FilesManifest> get_catalog_files(const Ref<FilesManifest> &p_manifest);

	// The one locale matching rule (texts, resources, LanguageSettingsServer): `p_locale` and its parents, the closest
	// of `p_available` by TranslationServer::compare_locales() (`de_DE` -> `de_AT`), then the fallback and its parents.
	static PackedStringArray build_locale_chain(const String &p_locale, const PackedStringArray &p_available, bool p_fallback = true);
	PackedStringArray get_locale_chain(const String &p_locale) const;

	void add_catalog(const Ref<LocalizationCatalog> &p_catalog, int p_priority = 0);
	void remove_catalog(const Ref<LocalizationCatalog> &p_catalog);
	bool has_catalog(const Ref<LocalizationCatalog> &p_catalog) const;
	TypedArray<LocalizationCatalog> get_catalogs() const;
	void clear_catalogs();
	int add_catalog_manifest(const Ref<FilesManifest> &p_manifest, int p_priority = 0);
	int load_project_catalogs();

	PackedStringArray get_available_locales() const;
	// The locales of the catalogs that give keys of `p_channel`; empty if no catalog does.
	PackedStringArray get_available_locales_for(const StringName &p_channel) const;
	// The locales of the catalogs of all localization servers (of one channel if not empty), standardized and sorted
	// (for LanguageSettingsServer).
	static PackedStringArray get_all_available_locales(const StringName &p_channel = StringName());
	int get_key_count() const;

	LocalizationServer();
	virtual ~LocalizationServer();
};
