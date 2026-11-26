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
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/string/translation_server.h"

// Flagged as translation-remapped: TranslationServer::set_locale() calls reload_from_file() on it before the scene
// tree is notified, so a locale change from anywhere (scripts, editor, command line) reaches the server first.
class LocalizationLocaleHook : public Resource {
	GDSOFTCLASS(LocalizationLocaleHook, Resource);

public:
	LocalizationServer *server = nullptr;

	virtual void reload_from_file() override {
		if (server) {
			server->update_locale();
		}
	}
};

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

void LocalizationServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_CATALOGS, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::STRING, PROPERTY_HINT_FILE, "*.lcat,*.tres,*.res,*.json,*.csv,*.xml,*.ini,*.toml,*.yaml")), PackedStringArray());
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_EXTERNAL_CATALOGS), PackedStringArray());
	GLOBAL_DEF(SETTING_EXTERNAL_CATALOGS_PRIORITY, 100);
}

void LocalizationServer::set_locale(const String &p_locale) {
	ERR_FAIL_NULL(TranslationServer::get_singleton());
	TranslationServer::get_singleton()->set_locale(p_locale);
}

String LocalizationServer::get_locale() {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	return translation_server ? translation_server->get_locale() : String();
}

String LocalizationServer::get_fallback_locale() {
	const TranslationServer *translation_server = TranslationServer::get_singleton();
	return translation_server ? translation_server->get_fallback_locale() : String();
}

PackedStringArray LocalizationServer::get_locale_chain(const String &p_locale) {
	PackedStringArray chain;
	_append_with_parents(_standardize_locale(p_locale), chain);
	_append_with_parents(_standardize_locale(get_fallback_locale()), chain);
	return chain;
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

Ref<LocalizationCatalog> LocalizationServer::_load_catalog(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (extension == LocalizationCatalog::BINARY_EXTENSION || extension == "tres" || extension == "res") {
		if (!ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_path), _get_catalog_class())) {
			return Ref<LocalizationCatalog>(); // A catalog of another server.
		}
		return ResourceLoader::load(p_path);
	}
	// Text formats (JSON, CSV, XML, INI, TOML, YAML) don't name their kind: they are read as this server's class.
	Ref<LocalizationCatalog> catalog = Object::cast_to<LocalizationCatalog>(ClassDB::instantiate(_get_catalog_class()));
	ERR_FAIL_COND_V(catalog.is_null(), Ref<LocalizationCatalog>());
	return catalog->load_from_file(p_path) == OK ? catalog : Ref<LocalizationCatalog>();
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

void LocalizationServer::_rebuild(bool p_keys) {
	Data *new_data = _create_data();
	new_data->locale = get_locale();
	new_data->chain = new_data->locale.is_empty() ? PackedStringArray() : get_locale_chain(new_data->locale);

	if (p_keys || !keys) {
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
		keys = table;
	}
	new_data->keys = keys;
	_resolve(new_data);

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

	// On-demand locales: unload file-backed columns outside of the chain, report the newly loaded locales.
	PackedStringArray present;
	for (const Ref<LocalizationCatalog> &catalog : keys->layers) {
		for (const String &locale : catalog->get_locales()) {
			if (new_data->chain.has(locale)) {
				if (!present.has(locale)) {
					present.push_back(locale);
				}
			} else if (catalog->is_locale_loaded(locale)) {
				const LocalizationCatalog::LocaleColumn *column = catalog->get_locale_column(catalog->find_locale(locale));
				if (column && column->block_offset != 0) {
					catalog->unload_locale(locale);
				}
			}
		}
	}
	const PackedStringArray previous = chain_locales;
	chain_locales = present;
	for (const String &locale : present) {
		if (!previous.has(locale)) {
			_emit_on_main_thread(SNAME("locale_loaded"), locale);
		}
	}
}

void LocalizationServer::update_locale() {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &LocalizationServer::update_locale).call_deferred();
		return;
	}
	const String previous_locale = _get_data() ? _get_data()->locale : String();
	const String locale = get_locale();
	if (locale == previous_locale) {
		return;
	}
	_rebuild(false);
	emit_signal(SNAME("locale_changed"), locale, previous_locale);
}

void LocalizationServer::restore_locale_hooks() {
	for (LocalizationServer *server : live_servers) {
		server->locale_hook->set_as_translation_remapped(true);
		server->update_locale();
	}
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
	_catalogs_changed();
}

int LocalizationServer::add_catalog_list(const Ref<LocalizationCatalogList> &p_list) {
	ERR_FAIL_COND_V(p_list.is_null(), 0);
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), 0, "Catalog lists can only be added on the main thread.");
	int added = 0;
	for (int entry = 0; entry < p_list->get_entry_count(); entry++) {
		for (const String &path : p_list->get_entry_catalog_paths(entry)) {
			const Ref<LocalizationCatalog> catalog = _load_catalog(path);
			if (catalog.is_valid()) {
				_remove_layer(catalog);
				_add_layer(catalog, p_list->get_entry_priority(entry));
				added++;
			}
		}
	}
	if (added > 0) {
		_catalogs_changed();
	}
	return added;
}

int LocalizationServer::load_project_catalogs() {
	const Ref<LocalizationCatalog> project_catalog = _get_project_catalog();
	if (project_catalog.is_valid()) {
		_remove_layer(project_catalog);
		_add_layer(project_catalog, 0);
	}
	Ref<LocalizationCatalogList> list;
	list.instantiate();
	for (const String &path : PackedStringArray(GLOBAL_GET(_get_catalogs_setting()))) {
		list->add_entry(path, 0);
	}
	const int external_priority = GLOBAL_GET(SETTING_EXTERNAL_CATALOGS_PRIORITY);
	for (const String &path : PackedStringArray(GLOBAL_GET(_get_external_catalogs_setting()))) {
		list->add_entry(path, external_priority);
	}
	const int added = add_catalog_list(list);
	if (project_catalog.is_null()) {
		return added;
	}
	if (added == 0) {
		_catalogs_changed();
	}
	return added + 1;
}

PackedStringArray LocalizationServer::get_available_locales() const {
	PackedStringArray locales;
	for (const Layer &layer : layers) {
		for (const String &locale : layer.catalog->get_locales()) {
			if (!locales.has(locale)) {
				locales.push_back(locale);
			}
		}
	}
	return locales;
}

PackedStringArray LocalizationServer::get_all_available_locales() {
	PackedStringArray locales;
	for (const LocalizationServer *server : live_servers) {
		for (const String &locale : server->get_available_locales()) {
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
	ClassDB::bind_static_method("LocalizationServer", D_METHOD("set_locale", "locale"), &LocalizationServer::set_locale);
	ClassDB::bind_static_method("LocalizationServer", D_METHOD("get_locale"), &LocalizationServer::get_locale);
	ClassDB::bind_static_method("LocalizationServer", D_METHOD("get_fallback_locale"), &LocalizationServer::get_fallback_locale);
	ClassDB::bind_static_method("LocalizationServer", D_METHOD("get_locale_chain", "locale"), &LocalizationServer::get_locale_chain);

	ClassDB::bind_method(D_METHOD("add_catalog", "catalog", "priority"), &LocalizationServer::add_catalog, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("remove_catalog", "catalog"), &LocalizationServer::remove_catalog);
	ClassDB::bind_method(D_METHOD("has_catalog", "catalog"), &LocalizationServer::has_catalog);
	ClassDB::bind_method(D_METHOD("get_catalogs"), &LocalizationServer::get_catalogs);
	ClassDB::bind_method(D_METHOD("clear_catalogs"), &LocalizationServer::clear_catalogs);
	ClassDB::bind_method(D_METHOD("add_catalog_list", "list"), &LocalizationServer::add_catalog_list);
	ClassDB::bind_method(D_METHOD("load_project_catalogs"), &LocalizationServer::load_project_catalogs);
	ClassDB::bind_method(D_METHOD("get_available_locales"), &LocalizationServer::get_available_locales);
	ClassDB::bind_method(D_METHOD("get_key_count"), &LocalizationServer::get_key_count);

	ADD_SIGNAL(MethodInfo("locale_changed", PropertyInfo(Variant::STRING, "locale"), PropertyInfo(Variant::STRING, "previous_locale")));
	ADD_SIGNAL(MethodInfo("locale_loaded", PropertyInfo(Variant::STRING, "locale")));
	ADD_SIGNAL(MethodInfo("catalogs_changed"));
}

LocalizationServer::LocalizationServer() {
	locale_hook.instantiate();
	locale_hook->server = this;
	locale_hook->set_as_translation_remapped(true);
	live_servers.push_back(this);
}

LocalizationServer::~LocalizationServer() {
	live_servers.erase(this);
	locale_hook->server = nullptr;
	locale_hook->set_as_translation_remapped(false);
	_free_retired();
	Data *current = data.exchange(nullptr);
	if (current) {
		memdelete(current);
	}
	if (keys) {
		memdelete(keys);
	}
}
