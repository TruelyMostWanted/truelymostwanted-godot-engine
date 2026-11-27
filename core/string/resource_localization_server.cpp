/**************************************************************************/
/*  resource_localization_server.cpp                                      */
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

#include "resource_localization_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"

ResourceLocalizationServer *ResourceLocalizationServer::singleton = nullptr;

void ResourceLocalizationServer::_resolve(Data *p_data, const Data *p_previous, const LocalVector<uint8_t> *p_dirty) const {
	ResourceData *resource_data = static_cast<ResourceData *>(p_data);
	const KeyTable *table = resource_data->keys;
	if (p_previous) {
		resource_data->paths = static_cast<const ResourceData *>(p_previous)->paths;
	} else {
		resource_data->paths.reserve(table->keys.size());
	}
	LocalVector<const LocalizationCatalog::LocaleColumn *> columns;
	for (uint32_t channel = 0; channel < table->channels.size(); channel++) {
		if (p_dirty && !(*p_dirty)[channel]) {
			continue;
		}
		const LocalVector<Source> &sources = resource_data->channels[channel].sources;
		columns.clear();
		for (const Source &source : sources) {
			columns.push_back(table->layers[source.layer]->get_locale_column(source.column));
		}
		for (const uint32_t row : table->channel_rows[channel]) {
			const String key = table->keys[row].key;
			String path = key;
			for (uint32_t i = 0; i < sources.size(); i++) {
				const int32_t catalog_row = table->get_catalog_row(sources[i].layer, row);
				if (catalog_row >= 0 && columns[i] && columns[i]->cells.get_length(catalog_row) > 0) {
					path = columns[i]->cells.get(catalog_row);
					break;
				}
			}
			resource_data->paths[key] = path;
		}
	}
}

// ResourceLoader::path_localizer, called from any thread on every load.
bool ResourceLocalizationServer::_localize_path(const String &p_path, String &r_path) {
	if (!singleton || Engine::get_singleton()->is_editor_hint()) {
		return false; // Like translation_remaps, the editor edits the original resources.
	}
	const ResourceData *resource_data = singleton->_get_resource_data();
	const String *path = resource_data ? resource_data->paths.getptr(p_path) : nullptr;
	if (!path) {
		return false;
	}
	r_path = *path; // ResourceLoader falls back to p_path if the file does not exist.
	return true;
}

void ResourceLocalizationServer::_data_published(const Data *p_data, const Data *p_previous) {
	const ResourceData *published = static_cast<const ResourceData *>(p_data);
	const ResourceData *previous = static_cast<const ResourceData *>(p_previous);
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// Cached resources whose path changed are loaded again.
	PackedStringArray changed;
	for (const KeyValue<String, String> &entry : published->paths) {
		const String *old_path = previous ? previous->paths.getptr(entry.key) : nullptr;
		if ((old_path ? *old_path : entry.key) != entry.value && ResourceCache::has(entry.key)) {
			changed.push_back(entry.key);
		}
	}
	if (previous) {
		for (const KeyValue<String, String> &entry : previous->paths) {
			if (entry.value != entry.key && !published->paths.has(entry.key) && ResourceCache::has(entry.key)) {
				changed.push_back(entry.key);
			}
		}
	}
	if (!changed.is_empty()) {
		_relocalize(changed);
	}
}

// Loads the localized file without the cache, under its own path: a load() of the original path on the main thread
// meanwhile gets the cached instance instead of waiting for this thread. The new data is copied into the cached
// instance on the main thread (_relocalization_done()), because copy_from() may need it (ImageTexture reads its image
// back from the RenderingServer, which waits for the main thread).
void ResourceLocalizationServer::_relocalize_resource(void *p_userdata, uint32_t p_index) {
	Relocalization *job = static_cast<Relocalization *>(p_userdata);
	job->resources[p_index] = ResourceLoader::load(ResourceLoader::path_remap(job->paths[p_index]), "", ResourceFormatLoader::CACHE_MODE_IGNORE);
	if (job->remaining.decrement() == 0) {
		MessageQueue::get_main_singleton()->push_callable(callable_mp(singleton, &ResourceLocalizationServer::_relocalization_done));
	}
}

void ResourceLocalizationServer::_relocalize(const PackedStringArray &p_paths) {
	if (relocalization) {
		// One run at a time: the newer locale must not be overwritten by the older run.
		for (const String &path : p_paths) {
			if (!pending_paths.has(path)) {
				pending_paths.push_back(path);
			}
		}
		return;
	}
	relocalization = memnew(Relocalization);
	relocalization->paths = p_paths;
	relocalization->resources.resize((uint32_t)p_paths.size());
	relocalization->remaining.set((uint32_t)p_paths.size());
	// Without worker threads (Web), the loads run here.
	relocalization->group = WorkerThreadPool::get_singleton()->add_native_group_task(&ResourceLocalizationServer::_relocalize_resource, relocalization, (int)p_paths.size(), -1, false, "Relocalize resources");
}

void ResourceLocalizationServer::_relocalization_done() {
	ERR_FAIL_NULL(relocalization);
	WorkerThreadPool::get_singleton()->wait_for_group_task_completion(relocalization->group);
	const PackedStringArray paths = relocalization->paths;
	for (uint32_t index = 0; index < relocalization->resources.size(); index++) {
		const Ref<Resource> &loaded = relocalization->resources[index];
		const Ref<Resource> cached = ResourceCache::get_ref(paths[index]);
		if (loaded.is_valid() && cached.is_valid() && cached != loaded) {
			cached->copy_from(loaded);
		}
	}
	memdelete(relocalization);
	relocalization = nullptr;
	if (!pending_paths.is_empty()) {
		const PackedStringArray next = pending_paths;
		pending_paths.clear();
		_relocalize(next);
	}
	emit_signal(SNAME("resources_relocalized"), paths);
}

Ref<LocalizationCatalog> ResourceLocalizationServer::_get_project_catalog() {
	if (remaps_catalog.is_null()) {
		const Dictionary remaps = ProjectSettings::get_singleton()->has_setting(SETTING_TRANSLATION_REMAPS) ? Dictionary(GLOBAL_GET(SETTING_TRANSLATION_REMAPS)) : Dictionary();
		if (remaps.is_empty()) {
			return Ref<LocalizationCatalog>();
		}
		remaps_catalog.instantiate();
		remaps_catalog->add_translation_remaps(remaps);
	}
	return remaps_catalog;
}

String ResourceLocalizationServer::get_localized_path(const String &p_path, const String &p_locale) const {
	const ResourceData *resource_data = _get_resource_data();
	const String *path = resource_data->paths.getptr(p_path);
	if (!path || p_locale.is_empty()) {
		return path ? *path : p_path;
	}
	const int row = resource_data->keys->find(p_path, StringName());
	const PackedStringArray chain = get_locale_chain(p_locale);
	if (!chain.is_empty() && chain[0] == resource_data->channels[resource_data->keys->row_channels[row]].locale) {
		return *path;
	}
	String localized;
	return _read_row(resource_data->keys, chain, row, localized) >= 0 ? localized : p_path;
}

bool ResourceLocalizationServer::has_localization(const String &p_path) const {
	return _get_resource_data()->paths.has(p_path);
}

TypedDictionary<String, String> ResourceLocalizationServer::get_localized_paths(const String &p_path) const {
	TypedDictionary<String, String> paths;
	const KeyTable *table = _get_resource_data()->keys;
	const int row = table->find(p_path, StringName());
	if (row < 0) {
		return paths;
	}
	for (uint32_t layer = 0; layer < table->layers.size(); layer++) {
		const int32_t catalog_row = table->get_catalog_row(layer, row);
		if (catalog_row < 0) {
			continue;
		}
		const LocalizationCatalog *catalog = table->layers[layer].ptr();
		const PackedStringArray locales = catalog->get_locales();
		String localized;
		for (int column = 0; column < locales.size(); column++) {
			if (catalog->read_cell(column, catalog_row, localized)) {
				paths[locales[column]] = localized; // A later layer overrides.
			}
		}
	}
	return paths;
}

PackedStringArray ResourceLocalizationServer::get_missing_locales(const String &p_path) const {
	const TypedDictionary<String, String> paths = get_localized_paths(p_path);
	PackedStringArray missing;
	for (const String &locale : get_available_locales()) {
		if (!paths.has(locale)) {
			missing.push_back(locale);
		}
	}
	return missing;
}

void ResourceLocalizationServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_localized_path", "path", "locale"), &ResourceLocalizationServer::get_localized_path, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("has_localization", "path"), &ResourceLocalizationServer::has_localization);
	ClassDB::bind_method(D_METHOD("get_localized_paths", "path"), &ResourceLocalizationServer::get_localized_paths);
	ClassDB::bind_method(D_METHOD("get_missing_locales", "path"), &ResourceLocalizationServer::get_missing_locales);
	ClassDB::bind_method(D_METHOD("is_relocalizing"), &ResourceLocalizationServer::is_relocalizing);

	ADD_SIGNAL(MethodInfo("resources_relocalized", PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths")));
}

ResourceLocalizationServer::ResourceLocalizationServer() {
	singleton = this;
	_rebuild(true);
	ResourceLoader::path_localizer = &ResourceLocalizationServer::_localize_path;
}

ResourceLocalizationServer::~ResourceLocalizationServer() {
	ResourceLoader::path_localizer = nullptr;
	if (relocalization) {
		if (WorkerThreadPool::get_singleton()) { // Otherwise already finished: it is freed before this server.
			WorkerThreadPool::get_singleton()->wait_for_group_task_completion(relocalization->group);
		}
		memdelete(relocalization);
	}
	singleton = nullptr;
}
