/**************************************************************************/
/*  localization_server.cpp                                               */
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

#include "localization_server.h"

#include "core/config/project_settings.h"
#include "core/io/files_manifest.h"
#include "core/io/files_manifest_server.h"
#include "core/io/pck_package_server.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/string/translation_server.h"
#include "core/templates/hash_set.h"

// All live servers (created and freed on the main thread), for get_all_available_locales().
static LocalVector<LocalizationServer *> live_servers;

static String _standardize_locale(const String &p_locale) {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	return translation_server ? translation_server->standardize_locale(p_locale) : p_locale;
}

static void _append_with_parents(const String &p_locale, PackedStringArray &r_chain) {
	String locale = p_locale;
	while (!locale.is_empty()) {
		if (!r_chain.has(locale)) {
			r_chain.push_back(locale);
		}
		const int separator = locale.rfind_char('_');
		locale = separator < 0 ? String() : locale.left(separator);
	}
}

// A copy of a catalog manifest for listing its files. Its type hint names the catalog kind and is not used as a
// filter (text formats have no resource type before they are loaded); without extensions, the catalog formats.
static Ref<FilesManifest> _get_catalog_files(const Ref<FilesManifest> &p_manifest) {
	Ref<FilesManifest> files = p_manifest->duplicate();
	files->set_type_hint(StringName());
	if (files->get_extensions().is_empty()) {
		PackedStringArray extensions = LocalizationCatalog::get_file_extensions();
		extensions.push_back("tres");
		extensions.push_back("res");
		files->set_extensions(extensions);
	}
	return files;
}

void LocalizationServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::ARRAY, SETTING_CATALOG_MANIFESTS, PROPERTY_HINT_ARRAY_TYPE, "FilesManifest"), Array());
}

PackedStringArray LocalizationServer::build_locale_chain(const String &p_locale, const PackedStringArray &p_available, bool p_fallback) {
	PackedStringArray chain;
	const String locale = _standardize_locale(p_locale);
	_append_with_parents(locale, chain);
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	if (!translation_server) {
		return chain;
	}
	// The closest other locale, like Godot's resource remaps: `de_DE` takes `de_AT` if there is no `de`.
	String closest;
	int closest_score = 0;
	for (const String &available : p_available) {
		const int score = chain.has(available) ? 0 : translation_server->compare_locales(locale, available);
		if (score > closest_score) {
			closest_score = score;
			closest = available;
		}
	}
	if (!closest.is_empty()) {
		chain.push_back(closest);
	}
	if (p_fallback) {
		_append_with_parents(_standardize_locale(translation_server->get_fallback_locale()), chain);
	}
	return chain;
}

PackedStringArray LocalizationServer::get_locale_chain(const String &p_locale) const {
	return build_locale_chain(p_locale, _get_data()->keys->locales);
}

void LocalizationServer::_add_layer(const Ref<LocalizationCatalog> &p_catalog, int p_priority) {
	uint32_t position = layers.size();
	while (position > 0 && layers[position - 1].priority > p_priority) {
		position--;
	}
	layers.insert(position, Layer{ p_catalog, p_priority });
	p_catalog->connect_changed(callable_mp(this, &LocalizationServer::_on_catalog_changed));
}

bool LocalizationServer::_remove_layer(const Ref<LocalizationCatalog> &p_catalog) {
	for (uint32_t i = 0; i < layers.size(); i++) {
		if (layers[i].catalog == p_catalog) {
			p_catalog->disconnect_changed(callable_mp(this, &LocalizationServer::_on_catalog_changed));
			layers.remove_at(i);
			return true;
		}
	}
	return false;
}

bool LocalizationServer::_takes_manifest(const Ref<FilesManifest> &p_manifest) const {
	// The type hint names the catalog kind; without one, a manifest lists text catalogs.
	const StringName kind = p_manifest->get_type_hint().is_empty() ? StringName("TextLocalizationCatalog") : p_manifest->get_type_hint();
	return kind == _get_catalog_class();
}

Ref<LocalizationCatalog> LocalizationServer::_load_catalog(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (extension == LocalizationCatalog::BINARY_EXTENSION || !LocalizationCatalog::get_file_extensions().has(extension)) {
		// `.lcat`, `.tres`, `.res` name their kind: skip catalogs of the other server and other resources unloaded.
		if (!ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_path), _get_catalog_class())) {
			return Ref<LocalizationCatalog>();
		}
	}
	// Text formats are read as the class of the type hint, through ResourceLoader: cached under their path, so a
	// catalog loaded twice is the same layer. A `.csv` imported as Translation is not a catalog: it is cached as
	// a Translation under the same path, fails the cast below and is skipped (import it as "Keep File").
	const Ref<LocalizationCatalog> catalog = ResourceLoader::load(p_path, _get_catalog_class());
	return catalog.is_valid() && catalog->is_class(_get_catalog_class()) ? catalog : Ref<LocalizationCatalog>();
}

int LocalizationServer::_add_catalog_paths(const PackedStringArray &p_paths, int p_priority, bool p_project) {
	int added = 0;
	for (const String &path : p_paths) {
		const Ref<LocalizationCatalog> catalog = _load_catalog(path);
		if (catalog.is_valid()) {
			_remove_layer(catalog);
			_add_layer(catalog, p_priority);
			if (p_project) {
				project_catalogs.push_back(catalog);
			}
			added++;
		}
	}
	return added;
}

void LocalizationServer::_on_catalog_changed() {
	if (!rebuild_queued) {
		rebuild_queued = true;
		callable_mp(this, &LocalizationServer::_catalogs_changed).call_deferred();
	}
}

void LocalizationServer::_catalogs_changed() {
	rebuild_queued = false;
	_rebuild(true);
	emit_signal(SNAME("catalogs_changed"));
}

void LocalizationServer::_free_retired() {
	for (Data *retired : retired_data) {
		memdelete(retired);
	}
	for (KeyTable *retired : retired_keys) {
		memdelete(retired);
	}
	retired_data.clear();
	retired_keys.clear();
	free_queued = false;
}

void LocalizationServer::_connect_translation_server() {
	// TranslationServer is created after the localization servers: connected on the first rebuild that sees it.
	TranslationServer *translation_server = TranslationServer::get_singleton();
	const Callable on_locale_changed = callable_mp(this, &LocalizationServer::_on_locale_changed);
	if (translation_server && !translation_server->is_connected(SNAME("locale_changed"), on_locale_changed)) {
		translation_server->connect(SNAME("locale_changed"), on_locale_changed);
		translation_server->connect(SNAME("fallback_locale_changed"), callable_mp(this, &LocalizationServer::_on_fallback_locale_changed));
	}
}

void LocalizationServer::_on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale) {
	_update_locales(false);
}

void LocalizationServer::_on_fallback_locale_changed(const String &p_locale) {
	_update_locales(true);
}

void LocalizationServer::_update_locales(bool p_all) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &LocalizationServer::_update_locales).call_deferred(p_all);
		return;
	}
	const Data *current = _get_data();
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	if (!current || !translation_server || p_all) {
		_rebuild(false);
		return;
	}
	// Only the channels whose locale changed: a general change reaches every channel without its own locale.
	LocalVector<uint8_t> dirty;
	dirty.resize_initialized(keys->channels.size());
	bool changed = false;
	for (uint32_t channel = 0; channel < keys->channels.size(); channel++) {
		if (translation_server->get_locale_for(keys->channels[channel]) != current->channels[channel].locale) {
			dirty[channel] = 1;
			changed = true;
		}
	}
	if (changed) {
		_rebuild(false, &dirty);
	}
}

void LocalizationServer::_rebuild(bool p_keys, const LocalVector<uint8_t> *p_dirty) {
	_connect_translation_server();
	const Data *previous_data = _get_data();
	if (p_keys || !keys) {
		p_dirty = nullptr;
		KeyTable *table = memnew(KeyTable);
		if (keys) {
			table->index = keys->index;
			table->keys = keys->keys;
			retired_keys.push_back(keys);
		}
		// Rows of all layers; new keys are appended so the rows of known keys keep their number.
		LocalVector<uint32_t> found;
		for (const Layer &layer : layers) {
			table->layers.push_back(layer.catalog);
			for (const String &locale : layer.catalog->get_locales()) {
				if (!table->locales.has(locale)) {
					table->locales.push_back(locale);
				}
			}
			const LocalizationCatalog::Cells &ids = layer.catalog->get_id_cells();
			for (uint32_t catalog_row = 0; catalog_row < ids.size(); catalog_row++) {
				const String id = ids.get(catalog_row);
				const int separator = id.find_char(0x04);
				const Key key = separator < 0 ? Key{ id, StringName() } : Key{ id.substr(separator + 1), id.left(separator) };
				const uint32_t *row = table->index.getptr(key);
				if (row) {
					found.push_back(*row);
				} else {
					found.push_back(table->keys.size());
					table->index.insert(key, table->keys.size());
					table->keys.push_back(key);
				}
			}
		}
		const uint32_t row_count = table->keys.size();
		table->catalog_rows.resize(table->layers.size() * row_count);
		for (int32_t &catalog_row : table->catalog_rows) {
			catalog_row = -1;
		}
		uint32_t position = 0;
		for (uint32_t layer = 0; layer < table->layers.size(); layer++) {
			const uint32_t count = table->layers[layer]->get_id_cells().size();
			for (uint32_t catalog_row = 0; catalog_row < count; catalog_row++) {
				table->catalog_rows[layer * row_count + found[position++]] = catalog_row;
			}
		}
		// A key follows the channel of the highest-priority catalog that has it.
		table->row_channels.resize(row_count);
		LocalVector<LocalVector<uint32_t>> layer_channels; // Layer -> the channels of its rows.
		layer_channels.resize(table->layers.size());
		for (uint32_t row = 0; row < row_count; row++) {
			StringName channel = SNAME("interface"); // Rows of removed catalogs.
			for (uint32_t layer = table->layers.size(); layer-- > 0;) {
				const int32_t catalog_row = table->get_catalog_row(layer, row);
				if (catalog_row >= 0) {
					channel = table->layers[layer]->get_row_channel(catalog_row);
					break;
				}
			}
			int64_t channel_index = table->channels.find(channel);
			if (channel_index < 0) {
				channel_index = table->channels.size();
				table->channels.push_back(channel);
				table->channel_rows.resize(table->channels.size());
			}
			table->row_channels[row] = (uint32_t)channel_index;
			table->channel_rows[channel_index].push_back(row);
			// Every layer with the row serves the channel through the chain walk, not only the winning one.
			for (uint32_t layer = 0; layer < table->layers.size(); layer++) {
				if (table->get_catalog_row(layer, row) >= 0 && !layer_channels[layer].has((uint32_t)channel_index)) {
					layer_channels[layer].push_back((uint32_t)channel_index);
				}
			}
		}
		table->channel_locales.resize(table->channels.size());
		for (uint32_t layer = 0; layer < table->layers.size(); layer++) {
			for (const uint32_t channel_index : layer_channels[layer]) {
				for (const String &locale : table->layers[layer]->get_locales()) {
					if (!table->channel_locales[channel_index].has(locale)) {
						table->channel_locales[channel_index].push_back(locale);
					}
				}
			}
		}
		keys = table;
	}

	Data *new_data = _create_data();
	new_data->keys = keys;
	new_data->channels.resize(keys->channels.size());
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	for (uint32_t channel_index = 0; channel_index < keys->channels.size(); channel_index++) {
		Channel &channel = new_data->channels[channel_index];
		if (p_dirty && !(*p_dirty)[channel_index]) {
			const Channel &previous = previous_data->channels[channel_index];
			channel.locale = previous.locale;
			channel.chain = previous.chain;
			channel.sources = previous.sources;
			continue;
		}
		channel.locale = translation_server ? translation_server->get_locale_for(keys->channels[channel_index]) : String();
		channel.chain = channel.locale.is_empty() ? PackedStringArray() : build_locale_chain(channel.locale, keys->locales);
		for (const String &locale : channel.chain) {
			for (uint32_t layer = keys->layers.size(); layer-- > 0;) {
				const int column = keys->layers[layer]->find_locale(locale);
				if (column >= 0) {
					channel.sources.push_back(Source{ layer, column });
				}
			}
		}
	}
	_resolve(new_data, p_dirty ? previous_data : nullptr, p_dirty);

	Data *old_data = data.exchange(new_data, std::memory_order_acq_rel);
	if (old_data) {
		// Readers on other threads may still hold the old data: free it on the next idle frame.
		retired_data.push_back(old_data);
	}
	if (!retired_data.is_empty() && !free_queued && MessageQueue::get_singleton()) {
		free_queued = true;
		callable_mp(this, &LocalizationServer::_free_retired).call_deferred();
	}
	_data_published(new_data, old_data);

	// On-demand locales: unload file-backed columns no chain reads, report the newly loaded locales.
	HashSet<uint64_t> used;
	for (const Channel &channel : new_data->channels) {
		for (const Source &source : channel.sources) {
			used.insert(((uint64_t)source.layer << 32) | (uint32_t)source.column);
		}
	}
	PackedStringArray present;
	for (uint32_t layer = 0; layer < keys->layers.size(); layer++) {
		const Ref<LocalizationCatalog> &catalog = keys->layers[layer];
		const PackedStringArray locales = catalog->get_locales();
		for (int column = 0; column < locales.size(); column++) {
			if (used.has(((uint64_t)layer << 32) | (uint32_t)column)) {
				if (!present.has(locales[column])) {
					present.push_back(locales[column]);
				}
			} else if (catalog->is_locale_loaded(locales[column]) && catalog->get_locale_column(column)->block_offset != 0) {
				catalog->unload_locale(locales[column]);
			}
		}
	}
	const PackedStringArray previous_locales = chain_locales;
	chain_locales = present;
	for (const String &locale : present) {
		if (!previous_locales.has(locale)) {
			_emit_on_main_thread(SNAME("locale_loaded"), locale);
		}
	}
}

int LocalizationServer::_read_row(const KeyTable *p_keys, const PackedStringArray &p_chain, uint32_t p_row, String &r_text, Vector<String> *r_forms, String *r_locale) const {
	for (const String &locale : p_chain) {
		for (uint32_t layer = p_keys->layers.size(); layer-- > 0;) {
			const int32_t catalog_row = p_keys->get_catalog_row(layer, p_row);
			const LocalizationCatalog *catalog = p_keys->layers[layer].ptr();
			if (catalog_row >= 0 && catalog->read_cell(catalog->find_locale(locale), catalog_row, r_text, r_forms)) {
				if (r_locale) {
					*r_locale = locale;
				}
				return (int)layer;
			}
		}
	}
	return -1;
}

void LocalizationServer::add_catalog(const Ref<LocalizationCatalog> &p_catalog, int p_priority) {
	ERR_FAIL_COND(p_catalog.is_null());
	ERR_FAIL_COND_MSG(!p_catalog->is_class(_get_catalog_class()), vformat("%s only takes catalogs of the class %s.", get_class(), _get_catalog_class()));
	if (!Thread::is_main_thread()) {
		callable_mp(this, &LocalizationServer::add_catalog).call_deferred(p_catalog, p_priority);
		return;
	}
	_remove_layer(p_catalog);
	_add_layer(p_catalog, p_priority);
	_catalogs_changed();
}

void LocalizationServer::remove_catalog(const Ref<LocalizationCatalog> &p_catalog) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &LocalizationServer::remove_catalog).call_deferred(p_catalog);
		return;
	}
	if (_remove_layer(p_catalog)) {
		_catalogs_changed();
	}
}

bool LocalizationServer::has_catalog(const Ref<LocalizationCatalog> &p_catalog) const {
	for (const Layer &layer : layers) {
		if (layer.catalog == p_catalog) {
			return true;
		}
	}
	return false;
}

TypedArray<LocalizationCatalog> LocalizationServer::get_catalogs() const {
	TypedArray<LocalizationCatalog> catalogs;
	for (const Layer &layer : layers) {
		catalogs.push_back(layer.catalog);
	}
	return catalogs;
}

void LocalizationServer::clear_catalogs() {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &LocalizationServer::clear_catalogs).call_deferred();
		return;
	}
	while (!layers.is_empty()) {
		_remove_layer(layers[layers.size() - 1].catalog);
	}
	project_catalogs.clear();
	_catalogs_changed();
}

int LocalizationServer::add_catalog_manifest(const Ref<FilesManifest> &p_manifest, int p_priority) {
	ERR_FAIL_COND_V(p_manifest.is_null(), 0);
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), 0, "Catalog manifests can only be added on the main thread.");
	ERR_FAIL_COND_V_MSG(!_takes_manifest(p_manifest), 0, vformat("The manifest's type_hint \"%s\" is not %s.", p_manifest->get_type_hint(), _get_catalog_class()));
	const int added = _add_catalog_paths(_get_catalog_files(p_manifest)->get_paths(), p_priority, false);
	if (added > 0) {
		_catalogs_changed();
	}
	return added;
}

int LocalizationServer::load_project_catalogs() {
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), 0, "Project catalogs can only be loaded on the main thread.");
	for (const Ref<LocalizationCatalog> &catalog : project_catalogs) {
		_remove_layer(catalog);
	}
	project_catalogs.clear();
	int added = 0;
	const Ref<LocalizationCatalog> project_catalog = _get_project_catalog();
	if (project_catalog.is_valid()) {
		_remove_layer(project_catalog);
		_add_layer(project_catalog, 0);
		project_catalogs.push_back(project_catalog);
		added++;
	}

	// The manifests in list order: a later manifest wins over an earlier one. Registered in FilesManifestServer,
	// so its file signals (watching) and mounted or unmounted packs reload the catalogs.
	FilesManifestServer *manifest_server = FilesManifestServer::get_singleton();
	const Array manifests = GLOBAL_GET(SETTING_CATALOG_MANIFESTS);
	manifest_names.clear();
	for (int i = 0; i < manifests.size(); i++) {
		const Ref<FilesManifest> manifest = manifests[i];
		if (manifest.is_null() || !_takes_manifest(manifest)) {
			continue;
		}
		const Ref<FilesManifest> files = _get_catalog_files(manifest);
		const String name = vformat("localization_catalogs_%d", i);
		manifest_names.push_back(name);
		PackedStringArray paths;
		if (manifest_server) {
			if (manifest_server->has_manifest(name)) {
				manifest_server->refresh(name);
			} else {
				manifest_server->register_manifest(name, files);
			}
			paths = manifest_server->get_file_paths(name);
		} else {
			paths = files->get_paths();
		}
		added += _add_catalog_paths(paths, 0, true);
	}

	if (manifest_server && !manifest_server->is_connected(SNAME("file_added"), callable_mp(this, &LocalizationServer::_on_manifest_file))) {
		manifest_server->connect(SNAME("file_added"), callable_mp(this, &LocalizationServer::_on_manifest_file));
		manifest_server->connect(SNAME("file_removed"), callable_mp(this, &LocalizationServer::_on_manifest_file));
	}
	PCKPackageServer *pack_server = PCKPackageServer::get_singleton();
	if (pack_server && !pack_server->is_connected(SNAME("pack_unmounted"), callable_mp(this, &LocalizationServer::_on_pack))) {
		pack_server->connect(SNAME("pack_mounted"), callable_mp(this, &LocalizationServer::_on_pack));
		pack_server->connect(SNAME("pack_unmounted"), callable_mp(this, &LocalizationServer::_on_pack));
	}
	_catalogs_changed();
	return added;
}

void LocalizationServer::_on_manifest_file(const String &p_name, const String &p_path) {
	if (manifest_names.has(p_name)) {
		_on_pack(p_path);
	}
}

void LocalizationServer::_on_pack(const String &p_path) {
	// Several files or packs at once: one reload.
	if (!reload_queued) {
		reload_queued = true;
		callable_mp(this, &LocalizationServer::_reload_project_catalogs).call_deferred();
	}
}

void LocalizationServer::_reload_project_catalogs() {
	reload_queued = false;
	load_project_catalogs();
}

PackedStringArray LocalizationServer::get_available_locales() const {
	return _get_data()->keys->locales;
}

PackedStringArray LocalizationServer::get_available_locales_for(const StringName &p_channel) const {
	const KeyTable *table = _get_data()->keys;
	const int64_t channel_index = table->channels.find(p_channel);
	return channel_index < 0 ? PackedStringArray() : table->channel_locales[channel_index];
}

PackedStringArray LocalizationServer::get_all_available_locales(const StringName &p_channel) {
	PackedStringArray locales;
	for (const LocalizationServer *server : live_servers) {
		for (const String &locale : p_channel.is_empty() ? server->get_available_locales() : server->get_available_locales_for(p_channel)) {
			const String standardized = _standardize_locale(locale);
			if (!locales.has(standardized)) {
				locales.push_back(standardized);
			}
		}
	}
	locales.sort();
	return locales;
}

int LocalizationServer::get_key_count() const {
	return _get_data()->keys->keys.size();
}

void LocalizationServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_locale_chain", "locale"), &LocalizationServer::get_locale_chain);
	ClassDB::bind_method(D_METHOD("add_catalog", "catalog", "priority"), &LocalizationServer::add_catalog, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("remove_catalog", "catalog"), &LocalizationServer::remove_catalog);
	ClassDB::bind_method(D_METHOD("has_catalog", "catalog"), &LocalizationServer::has_catalog);
	ClassDB::bind_method(D_METHOD("get_catalogs"), &LocalizationServer::get_catalogs);
	ClassDB::bind_method(D_METHOD("clear_catalogs"), &LocalizationServer::clear_catalogs);
	ClassDB::bind_method(D_METHOD("add_catalog_manifest", "manifest", "priority"), &LocalizationServer::add_catalog_manifest, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("load_project_catalogs"), &LocalizationServer::load_project_catalogs);
	ClassDB::bind_method(D_METHOD("get_available_locales"), &LocalizationServer::get_available_locales);
	ClassDB::bind_method(D_METHOD("get_available_locales_for", "channel"), &LocalizationServer::get_available_locales_for);
	ClassDB::bind_method(D_METHOD("get_key_count"), &LocalizationServer::get_key_count);

	ADD_SIGNAL(MethodInfo("locale_loaded", PropertyInfo(Variant::STRING, "locale")));
	ADD_SIGNAL(MethodInfo("catalogs_changed"));
}

LocalizationServer::LocalizationServer() {
	live_servers.push_back(this);
}

LocalizationServer::~LocalizationServer() {
	live_servers.erase(this);
	_free_retired();
	Data *current = data.exchange(nullptr);
	if (current) {
		memdelete(current);
	}
	if (keys) {
		memdelete(keys);
	}
}
