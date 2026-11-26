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
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

#include <atomic>

class LocalizationLocaleHook;

// Base of the localization servers (#343): catalog layers by priority, one key index over all layers, the locale
// chain (`de_DE` -> `de` -> fallback) and on-demand locales. The locale itself lives in TranslationServer.
// Readers are lock-free: the data of the active locale chain is an immutable snapshot swapped atomically.
// Writers (catalog or locale changes) run on the main thread; calls from other threads are deferred.
class LocalizationServer : public Object {
	GDCLASS(LocalizationServer, Object);

public:
	static constexpr const char *SETTING_CATALOGS = "internationalization/localization/catalogs";
	static constexpr const char *SETTING_EXTERNAL_CATALOGS = "internationalization/localization/external_catalogs";
	static constexpr const char *SETTING_EXTERNAL_CATALOGS_PRIORITY = "internationalization/localization/external_catalogs_priority";

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

		_FORCE_INLINE_ int32_t get_catalog_row(uint32_t p_layer, uint32_t p_row) const { return catalog_rows[p_layer * keys.size() + p_row]; }
		int find(const StringName &p_key, const StringName &p_context) const {
			const uint32_t *row = index.getptr(Key{ p_key, p_context });
			return row ? int(*row) : -1;
		}
	};

	// Data of the active locale chain. Immutable once published; subclasses add their resolved columns.
	struct Data {
		const KeyTable *keys = nullptr;
		String locale;
		PackedStringArray chain;

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
	PackedStringArray chain_locales; // Locales loaded for the current chain.
	Ref<LocalizationLocaleHook> locale_hook;

	void _add_layer(const Ref<LocalizationCatalog> &p_catalog, int p_priority);
	bool _remove_layer(const Ref<LocalizationCatalog> &p_catalog);
	Ref<LocalizationCatalog> _load_catalog(const String &p_path) const;
	void _on_catalog_changed();
	void _catalogs_changed();
	void _free_retired();

protected:
	static void _bind_methods();

	// Class of the catalogs this server takes, e.g. "TextLocalizationCatalog".
	virtual StringName _get_catalog_class() const = 0;
	virtual Data *_create_data() const = 0;
	// Fills the subclass part of `p_data` from `p_data->keys` and `p_data->chain`.
	virtual void _resolve(Data *p_data) const = 0;
	// `p_previous` (null at first) stays valid until the next idle frame.
	virtual void _data_published(const Data *p_data, const Data *p_previous) {}
	// Project settings read by load_project_catalogs(), and a catalog of the subclass added before them as priority 0.
	virtual const char *_get_catalogs_setting() const { return SETTING_CATALOGS; }
	virtual const char *_get_external_catalogs_setting() const { return SETTING_EXTERNAL_CATALOGS; }
	virtual Ref<LocalizationCatalog> _get_project_catalog() { return Ref<LocalizationCatalog>(); }

	_FORCE_INLINE_ const Data *_get_data() const { return data.load(std::memory_order_acquire); }
	// Builds and publishes new data for the current locale; `p_keys` also rebuilds the key table.
	void _rebuild(bool p_keys);

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

	// The locale lives in TranslationServer, so controls, the editor and resource remaps follow it too.
	static void set_locale(const String &p_locale);
	static String get_locale();
	static String get_fallback_locale();
	static PackedStringArray get_locale_chain(const String &p_locale);

	void add_catalog(const Ref<LocalizationCatalog> &p_catalog, int p_priority = 0);
	void remove_catalog(const Ref<LocalizationCatalog> &p_catalog);
	bool has_catalog(const Ref<LocalizationCatalog> &p_catalog) const;
	TypedArray<LocalizationCatalog> get_catalogs() const;
	void clear_catalogs();
	int add_catalog_list(const Ref<LocalizationCatalogList> &p_list);
	int load_project_catalogs();

	PackedStringArray get_available_locales() const;
	// The locales of the catalogs of all localization servers, standardized and sorted (for LanguageSettingsServer).
	static PackedStringArray get_all_available_locales();
	int get_key_count() const;

	// Called on every TranslationServer::set_locale() through the locale hook.
	void update_locale();
	// ResourceLoader::clear_translation_remaps() (EditorNode) drops the locale hooks: registers them again and
	// catches up with the active locale.
	static void restore_locale_hooks();

	LocalizationServer();
	virtual ~LocalizationServer();
};
