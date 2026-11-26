/**************************************************************************/
/*  localization_catalog.cpp                                              */
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

#include "localization_catalog.h"

#include "core/config/engine.h"
#include "core/io/csv_format.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/serializer_options.h"
#include "core/io/text_data_format.h"
#include "core/io/xml_format.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "core/string/plural_rules.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"

// Binary `.lcat` layout (little-endian): magic, version, kind, row count, locale count; key cells; row extras;
// locale table (locale, plural rule, translated count, block offset); one block per locale (cells, plural forms).
// Cells are stored as raw `uint32` offsets + UTF-8 bytes, the same layout as in memory.
static constexpr uint32_t LCAT_MAGIC = 0x5441434c; // "LCAT".
static constexpr uint32_t LCAT_VERSION = 2; // 2: the channel after the kind (#357).
static constexpr const char *CATALOG_ENTRY = "?catalog";

/////////////////////////////////////////////////////////////////////////////
// Cells.

String LocalizationCatalog::Cells::get(uint32_t p_row) const {
	const uint32_t length = get_length(p_row);
	return length == 0 ? String() : String::utf8(bytes.ptr() + offsets[p_row], (int)length);
}

bool LocalizationCatalog::Cells::equals(uint32_t p_row, const CharString &p_utf8) const {
	const uint32_t length = get_length(p_row);
	return length == (uint32_t)p_utf8.length() && (length == 0 || memcmp(bytes.ptr() + offsets[p_row], p_utf8.get_data(), length) == 0);
}

void LocalizationCatalog::Cells::set(uint32_t p_row, const char *p_utf8, uint32_t p_length) {
	const uint32_t start = offsets[p_row];
	const uint32_t end = offsets[p_row + 1];
	const uint32_t tail = bytes.size() - end;
	const int64_t delta = int64_t(p_length) - int64_t(end - start);
	if (delta > 0) {
		bytes.resize(bytes.size() + uint32_t(delta));
	}
	if (delta != 0 && tail > 0) {
		memmove(bytes.ptr() + start + p_length, bytes.ptr() + end, tail);
	}
	if (delta < 0) {
		bytes.resize(bytes.size() - uint32_t(-delta));
	}
	if (p_length > 0) {
		memcpy(bytes.ptr() + start, p_utf8, p_length);
	}
	if (delta != 0) {
		for (uint32_t i = p_row + 1; i < offsets.size(); i++) {
			offsets[i] = uint32_t(int64_t(offsets[i]) + delta);
		}
	}
}

void LocalizationCatalog::Cells::append(const char *p_utf8, uint32_t p_length) {
	const uint32_t start = bytes.size();
	if (p_length > 0) {
		bytes.resize(start + p_length);
		memcpy(bytes.ptr() + start, p_utf8, p_length);
	}
	offsets.push_back(bytes.size());
}

void LocalizationCatalog::Cells::remove(uint32_t p_row) {
	set(p_row, nullptr, 0);
	offsets.remove_at(p_row + 1);
}

static String _read_cells(const Ref<FileAccess> &p_file, uint32_t p_rows, LocalizationCatalog::Cells &r_cells) {
	const uint64_t offsets_size = uint64_t(p_rows + 1) * sizeof(uint32_t);
	if (p_rows == UINT32_MAX || p_file->get_position() + offsets_size > p_file->get_length()) {
		return "Unexpected end of file.";
	}
	r_cells.offsets.resize(p_rows + 1);
	p_file->get_buffer((uint8_t *)r_cells.offsets.ptr(), offsets_size);
	if (r_cells.offsets[0] != 0) {
		return "Invalid cell offsets.";
	}
	for (uint32_t i = 1; i <= p_rows; i++) {
		if (r_cells.offsets[i] < r_cells.offsets[i - 1]) {
			return "Invalid cell offsets.";
		}
	}
	const uint32_t total = r_cells.offsets[p_rows];
	if (p_file->get_position() + total > p_file->get_length()) {
		return "Unexpected end of file.";
	}
	r_cells.bytes.resize(total);
	if (total > 0) {
		p_file->get_buffer((uint8_t *)r_cells.bytes.ptr(), total);
	}
	return String();
}

static void _write_cells(const Ref<FileAccess> &p_file, const LocalizationCatalog::Cells &p_cells) {
	p_file->store_buffer((const uint8_t *)p_cells.offsets.ptr(), p_cells.offsets.size() * sizeof(uint32_t));
	if (p_cells.bytes.size() > 0) {
		p_file->store_buffer((const uint8_t *)p_cells.bytes.ptr(), p_cells.bytes.size());
	}
}

static String _read_plural_forms(const Ref<FileAccess> &p_file, uint32_t p_rows, HashMap<uint32_t, Vector<String>> &r_forms) {
	const uint32_t count = p_file->get_32();
	for (uint32_t i = 0; i < count && !p_file->eof_reached(); i++) {
		const uint32_t row = p_file->get_32();
		const uint32_t form_count = p_file->get_32();
		if (row >= p_rows || form_count > 64) {
			return "Invalid plural forms.";
		}
		Vector<String> forms;
		for (uint32_t form = 0; form < form_count; form++) {
			forms.push_back(p_file->get_pascal_string());
		}
		r_forms[row] = forms;
	}
	return p_file->eof_reached() ? String("Unexpected end of file.") : String();
}

template <typename T>
static void _remove_row(HashMap<uint32_t, T> &r_map, uint32_t p_row) {
	HashMap<uint32_t, T> result;
	for (const KeyValue<uint32_t, T> &E : r_map) {
		if (E.key != p_row) {
			result.insert(E.key > p_row ? E.key - 1 : E.key, E.value);
		}
	}
	r_map = result;
}

static void _split_id(const String &p_id, String &r_key, String &r_context) {
	const int separator = p_id.find_char(0x04);
	r_key = separator < 0 ? p_id : p_id.substr(separator + 1);
	r_context = separator < 0 ? String() : p_id.substr(0, separator);
}

/////////////////////////////////////////////////////////////////////////////
// LocalizationCatalog.

static Mutex plural_rules_mutex;
static HashMap<String, PluralRules *> plural_rules_cache; // Rule text -> compiled rule.

PackedStringArray LocalizationCatalog::get_file_extensions() {
	return PackedStringArray({ BINARY_EXTENSION, "csv", "json", "yaml", "yml", "toml", "ini", "xml" });
}

const PluralRules *LocalizationCatalog::get_plural_rules(const String &p_rule, const String &p_locale) {
	String rule = p_rule;
	if (rule.is_empty() && TranslationServer::get_singleton()) {
		rule = TranslationServer::get_singleton()->get_plural_rules(p_locale);
	}
	if (rule.is_empty()) {
		rule = "nplurals=2; plural=(n != 1);";
	}
	{
		MutexLock lock(plural_rules_mutex);
		PluralRules **known = plural_rules_cache.getptr(rule);
		if (known) {
			return *known;
		}
	}
	// Parsed outside of the lock: parse() may print.
	PluralRules *parsed = PluralRules::parse(rule);
	if (!parsed) {
		parsed = PluralRules::parse("nplurals=2; plural=(n != 1);");
	}
	MutexLock lock(plural_rules_mutex);
	PluralRules **known = plural_rules_cache.getptr(rule);
	if (known) {
		if (parsed) {
			memdelete(parsed);
		}
		return *known;
	}
	plural_rules_cache.insert(rule, parsed);
	return parsed;
}

int LocalizationCatalog::evaluate_plural_rules(const PluralRules *p_rules, int p_n) {
	if (!p_rules) {
		return p_n == 1 ? 0 : 1;
	}
	MutexLock lock(plural_rules_mutex);
	return p_rules->evaluate(p_n);
}

void LocalizationCatalog::finish_plural_rules() {
	MutexLock lock(plural_rules_mutex);
	for (const KeyValue<String, PluralRules *> &E : plural_rules_cache) {
		if (E.value) {
			memdelete(E.value);
		}
	}
	plural_rules_cache.clear();
}

CharString LocalizationCatalog::_make_id(const StringName &p_key, const StringName &p_context) {
	if (p_context.is_empty()) {
		return String(p_key).utf8();
	}
	return (String(p_context) + String::chr(0x04) + String(p_key)).utf8();
}

void LocalizationCatalog::_index_insert(uint32_t p_row) {
	if (index.size() < ids.size() * 2) {
		_rebuild_index();
		return;
	}
	const uint32_t mask = index.size() - 1;
	uint32_t slot = hash_murmur3_buffer(ids.bytes.ptr() + ids.offsets[p_row], ids.get_length(p_row)) & mask;
	while (index[slot] != 0) {
		slot = (slot + 1) & mask;
	}
	index[slot] = p_row + 1;
}

void LocalizationCatalog::_rebuild_index() {
	uint32_t capacity = 16;
	while (capacity < ids.size() * 2) {
		capacity <<= 1;
	}
	index.clear();
	index.resize_initialized(capacity);
	for (uint32_t row = 0; row < ids.size(); row++) {
		_index_insert(row);
	}
}

int LocalizationCatalog::_find_row(const CharString &p_id) const {
	if (index.is_empty()) {
		return -1;
	}
	const uint32_t mask = index.size() - 1;
	uint32_t slot = hash_murmur3_buffer(p_id.get_data(), p_id.length()) & mask;
	while (index[slot] != 0) {
		if (ids.equals(index[slot] - 1, p_id)) {
			return index[slot] - 1;
		}
		slot = (slot + 1) & mask;
	}
	return -1;
}

Error LocalizationCatalog::_load_column(uint32_t p_column) const {
	LocaleColumn *column = columns[p_column];
	{
		MutexLock lock(load_mutex);
		if (column->loaded) {
			return OK;
		}
	}
	// Read outside of the lock (no printing under a lock); a concurrent load of the same column only wastes time.
	Cells cells;
	HashMap<uint32_t, Vector<String>> plural_forms;
	String error;
	Ref<FileAccess> file = FileAccess::open(source_path, FileAccess::READ);
	if (file.is_null()) {
		error = "Can't open the file.";
	} else {
		file->seek(column->block_offset);
		error = _read_cells(file, ids.size(), cells);
		if (error.is_empty()) {
			error = _read_plural_forms(file, ids.size(), plural_forms);
		}
	}
	ERR_FAIL_COND_V_MSG(!error.is_empty(), ERR_FILE_CORRUPT, vformat("Can't load locale '%s' of the localization catalog '%s': %s", column->locale, source_path, error));
	{
		MutexLock lock(load_mutex);
		if (column->loaded) {
			return OK;
		}
		column->cells.offsets = std::move(cells.offsets);
		column->cells.bytes = std::move(cells.bytes);
		column->plural_forms = std::move(plural_forms);
		column->loaded = true;
	}
	LocalizationCatalog *self = const_cast<LocalizationCatalog *>(this);
	if (Thread::is_main_thread()) {
		self->emit_signal(SNAME("locale_loaded"), column->locale);
	} else {
		self->call_deferred(SNAME("emit_signal"), SNAME("locale_loaded"), column->locale);
	}
	return OK;
}

Error LocalizationCatalog::_load_all_columns() const {
	for (uint32_t i = 0; i < columns.size(); i++) {
		const Error err = _load_column(i);
		if (err != OK) {
			return err;
		}
	}
	return OK;
}

void LocalizationCatalog::_detach_columns() {
	_load_all_columns();
	source_path = String();
	for (LocaleColumn *column : columns) {
		column->block_offset = 0;
	}
}

LocalizationCatalog::LocaleColumn *LocalizationCatalog::_get_column(const String &p_locale) const {
	const int column = find_locale(p_locale);
	if (column < 0 || _load_column(column) != OK) {
		return nullptr;
	}
	return columns[column];
}

LocalizationCatalog::LocaleColumn *LocalizationCatalog::_get_or_add_column(const String &p_locale) {
	LocaleColumn *column = _get_column(p_locale);
	if (column) {
		return column;
	}
	ERR_FAIL_COND_V(find_locale(p_locale) >= 0, nullptr); // Exists but failed to load.
	column = memnew(LocaleColumn);
	column->locale = p_locale;
	column->cells.offsets.resize_initialized(ids.size() + 1);
	columns.push_back(column);
	emit_changed();
	return column;
}

void LocalizationCatalog::_set_cell(LocaleColumn *p_column, uint32_t p_row, const String &p_value) {
	ERR_FAIL_NULL(p_column);
	const CharString utf8 = p_value.utf8();
	const bool had_text = p_column->cells.get_length(p_row) > 0;
	p_column->cells.set(p_row, utf8.get_data(), utf8.length());
	if (had_text && utf8.length() == 0) {
		p_column->translated--;
	} else if (!had_text && utf8.length() > 0) {
		p_column->translated++;
	}
	p_column->block_offset = 0; // Changed: no longer the block of the file.
}

void LocalizationCatalog::_set_plural_forms(LocaleColumn *p_column, uint32_t p_row, const PackedStringArray &p_forms) {
	ERR_FAIL_NULL(p_column);
	_set_cell(p_column, p_row, p_forms.is_empty() ? String() : p_forms[0]);
	if (p_forms.size() > 1) {
		p_column->plural_forms[p_row] = p_forms;
	} else {
		p_column->plural_forms.erase(p_row);
	}
}

void LocalizationCatalog::_set_row_extra(uint32_t p_row, const String &p_value) {
	if (p_value.is_empty()) {
		row_extras.erase(p_row);
	} else {
		row_extras[p_row] = p_value;
	}
}

String LocalizationCatalog::_get_row_extra(int p_row) const {
	const String *extra = p_row < 0 ? nullptr : row_extras.getptr(p_row);
	return extra ? *extra : String();
}

int LocalizationCatalog::find_locale(const String &p_locale) const {
	for (uint32_t i = 0; i < columns.size(); i++) {
		if (columns[i]->locale == p_locale) {
			return i;
		}
	}
	return -1;
}

const LocalizationCatalog::LocaleColumn *LocalizationCatalog::get_locale_column(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)columns.size(), nullptr);
	return _load_column(p_index) == OK ? columns[p_index] : nullptr;
}

bool LocalizationCatalog::read_cell(int p_column, uint32_t p_row, String &r_text, Vector<String> *r_forms) const {
	if (p_column < 0 || p_column >= (int)columns.size() || _load_column(p_column) != OK) {
		return false;
	}
	MutexLock lock(load_mutex);
	const LocaleColumn *column = columns[p_column];
	if (!column->loaded || p_row >= column->cells.size() || column->cells.get_length(p_row) == 0) {
		return false; // Unloaded by the main thread in between: treated as missing.
	}
	r_text = column->cells.get(p_row);
	if (r_forms) {
		const Vector<String> *forms = column->plural_forms.getptr(p_row);
		*r_forms = forms ? *forms : Vector<String>();
	}
	return true;
}

void LocalizationCatalog::set_channel(const StringName &p_channel) {
	if (channel == p_channel) {
		return;
	}
	channel = p_channel;
	emit_changed();
}

int LocalizationCatalog::add_key(const StringName &p_key, const StringName &p_context) {
	const CharString id = _make_id(p_key, p_context);
	int row = _find_row(id);
	if (row >= 0) {
		return row;
	}
	_detach_columns();
	row = ids.size();
	ids.append(id.get_data(), id.length());
	for (LocaleColumn *column : columns) {
		column->cells.append(nullptr, 0);
	}
	_index_insert(row);
	emit_changed();
	return row;
}

void LocalizationCatalog::remove_key(const StringName &p_key, const StringName &p_context) {
	const int row = find_key(p_key, p_context);
	if (row < 0) {
		return;
	}
	_detach_columns();
	ids.remove(row);
	for (LocaleColumn *column : columns) {
		if (column->cells.get_length(row) > 0) {
			column->translated--;
		}
		column->cells.remove(row);
		_remove_row(column->plural_forms, row);
	}
	_remove_row(row_extras, row);
	for (NoteColumn &note_column : note_columns) {
		_remove_row(note_column.notes, row);
	}
	_rebuild_index();
	emit_changed();
}

bool LocalizationCatalog::has_key(const StringName &p_key, const StringName &p_context) const {
	return find_key(p_key, p_context) >= 0;
}

int LocalizationCatalog::find_key(const StringName &p_key, const StringName &p_context) const {
	return _find_row(_make_id(p_key, p_context));
}

StringName LocalizationCatalog::get_key(int p_row) const {
	ERR_FAIL_INDEX_V(p_row, get_key_count(), StringName());
	String key, context;
	_split_id(ids.get(p_row), key, context);
	return key;
}

StringName LocalizationCatalog::get_key_context(int p_row) const {
	ERR_FAIL_INDEX_V(p_row, get_key_count(), StringName());
	String key, context;
	_split_id(ids.get(p_row), key, context);
	return context;
}

void LocalizationCatalog::add_locale(const String &p_locale) {
	ERR_FAIL_COND(p_locale.is_empty());
	_get_or_add_column(p_locale);
}

void LocalizationCatalog::remove_locale(const String &p_locale) {
	const int column = find_locale(p_locale);
	if (column < 0) {
		return;
	}
	memdelete(columns[column]);
	columns.remove_at(column);
	emit_changed();
}

bool LocalizationCatalog::has_locale(const String &p_locale) const {
	return find_locale(p_locale) >= 0;
}

PackedStringArray LocalizationCatalog::get_locales() const {
	PackedStringArray locales;
	for (const LocaleColumn *column : columns) {
		locales.push_back(column->locale);
	}
	return locales;
}

Error LocalizationCatalog::load_locale(const String &p_locale) {
	const int column = find_locale(p_locale);
	ERR_FAIL_COND_V_MSG(column < 0, ERR_DOES_NOT_EXIST, vformat("The localization catalog has no locale '%s'.", p_locale));
	return _load_column(column);
}

void LocalizationCatalog::unload_locale(const String &p_locale) {
	const int index_of_column = find_locale(p_locale);
	if (index_of_column < 0) {
		return;
	}
	LocaleColumn *column = columns[index_of_column];
	ERR_FAIL_COND_MSG(source_path.is_empty() || column->block_offset == 0, vformat("Locale '%s' can't be unloaded: it was changed or not loaded from a .lcat file.", p_locale));
	MutexLock lock(load_mutex);
	column->cells = Cells();
	column->plural_forms.clear();
	column->loaded = false;
}

bool LocalizationCatalog::is_locale_loaded(const String &p_locale) const {
	const int column = find_locale(p_locale);
	MutexLock lock(load_mutex);
	return column >= 0 && columns[column]->loaded;
}

int LocalizationCatalog::get_translated_count(const String &p_locale) const {
	const int column = find_locale(p_locale);
	return column < 0 ? 0 : columns[column]->translated;
}

PackedInt32Array LocalizationCatalog::get_missing_rows(const String &p_locale) const {
	PackedInt32Array rows;
	const LocaleColumn *column = _get_column(p_locale);
	for (uint32_t row = 0; row < ids.size(); row++) {
		if (!column || column->cells.get_length(row) == 0) {
			rows.push_back(row);
		}
	}
	return rows;
}

void LocalizationCatalog::clear() {
	ids = Cells();
	index.clear();
	for (LocaleColumn *column : columns) {
		memdelete(column);
	}
	columns.clear();
	row_extras.clear();
	note_columns.clear();
	source_path = String();
	emit_changed();
}

LocalizationCatalog::NoteColumn *LocalizationCatalog::_get_note_column(const String &p_field) {
	if (skip_notes || p_field == "_key" || p_field == "_context" || p_field == "_" + _get_extra_name()) {
		return nullptr;
	}
	const StringName column_name = p_field.substr(1);
	for (NoteColumn &note_column : note_columns) {
		if (note_column.name == column_name) {
			return &note_column;
		}
	}
	if (column_name.is_empty()) {
		return nullptr;
	}
	NoteColumn note_column;
	note_column.name = column_name;
	note_columns.push_back(note_column);
	return &note_columns[note_columns.size() - 1];
}

void LocalizationCatalog::_set_note(uint32_t p_row, NoteColumn *p_column, const String &p_note) {
	if (p_note.is_empty()) {
		p_column->notes.erase(p_row);
	} else {
		p_column->notes[p_row] = p_note;
	}
}

void LocalizationCatalog::set_note(const StringName &p_key, const String &p_note, const StringName &p_context, const StringName &p_column) {
	NoteColumn *note_column = _get_note_column("_" + String(p_column));
	ERR_FAIL_NULL_MSG(note_column, vformat("\"%s\" is not a note column: \"key\", \"context\" and \"%s\" are reserved.", p_column, _get_extra_name()));
	_set_note(add_key(p_key, p_context), note_column, p_note);
	emit_changed();
}

String LocalizationCatalog::get_note(const StringName &p_key, const StringName &p_context, const StringName &p_column) const {
	const int row = find_key(p_key, p_context);
	for (const NoteColumn &note_column : note_columns) {
		if (note_column.name == p_column) {
			const String *note = row < 0 ? nullptr : note_column.notes.getptr(row);
			return note ? *note : String();
		}
	}
	return String();
}

PackedStringArray LocalizationCatalog::get_note_columns() const {
	PackedStringArray names;
	for (const NoteColumn &note_column : note_columns) {
		names.push_back(note_column.name);
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////
// Dictionary shape (JSON, YAML, TOML, INI, XML, `.tres` storage).

Dictionary LocalizationCatalog::_get_data() const {
	Dictionary data;
	ERR_FAIL_COND_V(_load_all_columns() != OK, data);
	const String extra_field = "_" + _get_extra_name();
	Array locales;
	Dictionary rules;
	for (const LocaleColumn *column : columns) {
		locales.push_back(column->locale);
		if (!column->plural_rule.is_empty()) {
			rules[column->locale] = column->plural_rule;
		}
	}
	Dictionary meta;
	meta["locales"] = locales;
	if (!rules.is_empty()) {
		meta["plural_rules"] = rules;
	}
	meta["channel"] = channel;
	data[CATALOG_ENTRY] = meta;

	for (uint32_t row = 0; row < ids.size(); row++) {
		String key, context;
		_split_id(ids.get(row), key, context);
		Dictionary entry;
		String entry_name = key;
		if (!context.is_empty()) {
			entry_name = key + "@" + context;
			entry["_key"] = key;
			entry["_context"] = context;
		}
		const String extra = _get_row_extra(row);
		if (!extra.is_empty()) {
			entry[extra_field] = extra;
		}
		for (const NoteColumn &note_column : note_columns) {
			const String *note = note_column.notes.getptr(row);
			if (note) {
				entry["_" + String(note_column.name)] = *note;
			}
		}
		for (const LocaleColumn *column : columns) {
			const Vector<String> *forms = column->plural_forms.getptr(row);
			if (forms) {
				Array values;
				for (const String &form : *forms) {
					values.push_back(form);
				}
				entry[column->locale] = values;
			} else if (column->cells.get_length(row) > 0) {
				entry[column->locale] = column->cells.get(row);
			}
		}
		data[entry_name] = entry;
	}
	return data;
}

void LocalizationCatalog::_set_data(const Dictionary &p_data) {
	clear();
	TranslationServer *ts = TranslationServer::get_singleton();
	HashMap<String, LocaleColumn *> column_of_field; // Field name -> column (standardize_locale() once per name).
	const auto column_for = [&](const String &p_field) -> LocaleColumn * {
		LocaleColumn **known = column_of_field.getptr(p_field);
		if (known) {
			return *known;
		}
		const String locale = ts->standardize_locale(p_field);
		LocaleColumn *column = locale.is_empty() ? nullptr : _get_or_add_column(locale);
		column_of_field[p_field] = column;
		return column;
	};

	const Dictionary meta = p_data.get(CATALOG_ENTRY, Dictionary());
	if (meta.has("channel")) {
		channel = meta["channel"];
	}
	const Array locales = meta.get("locales", Array());
	for (const Variant &locale : locales) {
		column_for(locale);
	}
	const Dictionary rules = meta.get("plural_rules", Dictionary());
	for (const KeyValue<Variant, Variant> &kv : rules) {
		LocaleColumn *column = column_for(kv.key);
		if (column) {
			column->plural_rule = kv.value;
		}
	}

	const String extra_field = "_" + _get_extra_name();
	for (const KeyValue<Variant, Variant> &kv : p_data) {
		const String entry_name = kv.key;
		if (entry_name.is_empty() || entry_name.begins_with("?") || kv.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary entry = kv.value;
		const String key = entry.get("_key", entry_name);
		const String context = entry.get("_context", String());
		const int row = add_key(key, context);
		_set_row_extra(row, entry.get(extra_field, String()));
		for (const KeyValue<Variant, Variant> &field : entry) {
			const String field_name = field.key;
			if (field_name.begins_with("_")) {
				NoteColumn *note_column = _get_note_column(field_name);
				if (note_column) {
					_set_note(row, note_column, field.value);
				}
				continue;
			}
			LocaleColumn *column = column_for(field_name);
			if (!column) {
				continue;
			}
			const Variant::Type type = field.value.get_type();
			if (type == Variant::ARRAY || type == Variant::PACKED_STRING_ARRAY) {
				_set_plural_forms(column, row, field.value);
			} else {
				_set_cell(column, row, field.value);
			}
		}
	}
	emit_changed();
}

// XML: <catalog locales="en,de"><plural_rule locale="..">..</plural_rule><message key="" context="" plural="">
// <note name="notes">..</note><text locale="">..</text></message></catalog>
static Dictionary _xml_to_data(const Dictionary &p_root, const String &p_extra_name) {
	Dictionary data;
	Dictionary rules;
	const Dictionary root_attributes = p_root.get(XML::KEY_ATTRIBUTES, Dictionary());
	const String locales = root_attributes.get("locales", String());
	const Array children = p_root.get(XML::KEY_CHILDREN, Array());
	for (const Variant &child_variant : children) {
		if (child_variant.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary child = child_variant;
		const String element_name = child.get(XML::KEY_NAME, String());
		const Dictionary attributes = child.get(XML::KEY_ATTRIBUTES, Dictionary());
		if (element_name == "plural_rule") {
			rules[attributes.get("locale", String())] = child.get(XML::KEY_TEXT, String());
			continue;
		}
		if (element_name != "message") {
			continue;
		}
		const String key = attributes.get("key", String());
		const String context = attributes.get("context", String());
		Dictionary entry;
		entry["_key"] = key;
		entry["_context"] = context;
		entry["_" + p_extra_name] = attributes.get(p_extra_name, String());
		Dictionary forms_of_locale;
		const Array texts = child.get(XML::KEY_CHILDREN, Array());
		for (const Variant &text_variant : texts) {
			if (text_variant.get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary text = text_variant;
			const String text_element = text.get(XML::KEY_NAME, String());
			if (text_element == "note") {
				entry["_" + String(Dictionary(text.get(XML::KEY_ATTRIBUTES, Dictionary())).get("name", String()))] = text.get(XML::KEY_TEXT, String());
				continue;
			}
			if (text_element != "text") {
				continue;
			}
			const String locale = Dictionary(text.get(XML::KEY_ATTRIBUTES, Dictionary())).get("locale", String());
			Array forms = forms_of_locale.get(locale, Array());
			forms.push_back(text.get(XML::KEY_TEXT, String()));
			forms_of_locale[locale] = forms;
		}
		for (const KeyValue<Variant, Variant> &kv : forms_of_locale) {
			const Array forms = kv.value;
			entry[kv.key] = forms.size() == 1 ? forms[0] : Variant(forms);
		}
		data[context.is_empty() ? key : key + "@" + context] = entry;
	}
	Dictionary meta;
	meta["locales"] = locales.is_empty() ? Variant(Array()) : Variant(locales.split(","));
	meta["plural_rules"] = rules;
	data[CATALOG_ENTRY] = meta;
	return data;
}

static Dictionary _make_xml_element(const String &p_name, const Dictionary &p_attributes, const Variant &p_text = Variant()) {
	Dictionary element;
	element[XML::KEY_NAME] = p_name;
	element[XML::KEY_ATTRIBUTES] = p_attributes;
	if (p_text.get_type() != Variant::NIL) {
		element[XML::KEY_TEXT] = p_text;
	}
	return element;
}

static Dictionary _data_to_xml(const Dictionary &p_data, const String &p_extra_name) {
	Array children;
	const Dictionary meta = p_data.get(CATALOG_ENTRY, Dictionary());
	const Dictionary rules = meta.get("plural_rules", Dictionary());
	for (const KeyValue<Variant, Variant> &kv : rules) {
		Dictionary attributes;
		attributes["locale"] = kv.key;
		children.push_back(_make_xml_element("plural_rule", attributes, kv.value));
	}
	for (const KeyValue<Variant, Variant> &kv : p_data) {
		const String entry_name = kv.key;
		if (entry_name.begins_with("?")) {
			continue;
		}
		const Dictionary entry = kv.value;
		Dictionary attributes;
		attributes["key"] = entry.get("_key", entry_name);
		const String context = entry.get("_context", String());
		if (!context.is_empty()) {
			attributes["context"] = context;
		}
		const String extra = entry.get("_" + p_extra_name, String());
		if (!extra.is_empty()) {
			attributes[p_extra_name] = extra;
		}
		Array texts;
		for (const KeyValue<Variant, Variant> &field : entry) {
			const String locale = field.key;
			if (locale == "_key" || locale == "_context" || locale == "_" + p_extra_name) {
				continue;
			}
			if (locale.begins_with("_")) {
				Dictionary note_attributes;
				note_attributes["name"] = locale.substr(1);
				texts.push_back(_make_xml_element("note", note_attributes, field.value));
				continue;
			}
			Dictionary text_attributes;
			text_attributes["locale"] = locale;
			PackedStringArray forms;
			if (field.value.get_type() == Variant::ARRAY) {
				forms = field.value;
			} else {
				forms.push_back(field.value);
			}
			for (const String &form : forms) {
				texts.push_back(_make_xml_element("text", text_attributes, form));
			}
		}
		Dictionary message = _make_xml_element("message", attributes);
		message[XML::KEY_CHILDREN] = texts;
		children.push_back(message);
	}
	Dictionary root_attributes;
	const PackedStringArray locales = meta.get("locales", Array());
	root_attributes["locales"] = String(",").join(locales);
	Dictionary root = _make_xml_element("catalog", root_attributes);
	root[XML::KEY_CHILDREN] = children;
	return root;
}

/////////////////////////////////////////////////////////////////////////////
// CSV, with the rules of ResourceImporterCSVTranslation.

Error LocalizationCatalog::_load_rows(const Array &p_rows) {
	clear();
	ERR_FAIL_COND_V_MSG(p_rows.is_empty(), ERR_INVALID_DATA, "The CSV text has no header row.");
	const PackedStringArray header_row = p_rows[0];
	CSV::TranslationHeader header;
	Ref<DataValidation> issues;
	issues.instantiate();
	CSV::check_translation_header(header_row, header, issues.ptr());
	ERR_FAIL_COND_V_MSG(!header.has_known_language, ERR_INVALID_DATA, "No column of the CSV header row starts with a known language code: this is not a localization table.");

	// Text catalogs: the `?plural` column holds the plural key and starts plural rows. Resource catalogs: `?type`.
	const bool plurals = _get_kind() == KIND_TEXT;
	const int plural_column = plurals ? header.plural_column : -1;
	int extra_column = plural_column;
	if (!plurals) {
		for (int i = 1; i < header_row.size(); i++) {
			if (header_row[i].to_lower() == "?" + _get_extra_name()) {
				extra_column = i;
				header.locale_columns.erase(i);
			}
		}
	}
	HashMap<int, LocaleColumn *> column_of;
	for (const KeyValue<int, String> &E : header.locale_columns) {
		column_of[E.key] = _get_or_add_column(E.value);
	}
	// `_` columns, which the importer ignores: notes. Indices, as note_columns may grow.
	HashMap<int, uint32_t> note_column_of;
	for (int i = 1; i < header_row.size(); i++) {
		const NoteColumn *note_column = header_row[i].begins_with("_") ? _get_note_column(header_row[i]) : nullptr;
		if (note_column) {
			note_column_of[i] = uint32_t(note_column - note_columns.ptr());
		}
	}

	int plural_row = -1; // Row of the open plural block.
	HashMap<int, PackedStringArray> plural_texts; // CSV column -> forms.
	const auto close_plural_block = [&]() {
		if (plural_row >= 0) {
			for (const KeyValue<int, PackedStringArray> &E : plural_texts) {
				_set_plural_forms(column_of[E.key], plural_row, E.value);
			}
		}
		plural_row = -1;
		plural_texts.clear();
	};

	for (int i = 1; i < p_rows.size(); i++) {
		const PackedStringArray line = p_rows[i];
		if (line.is_empty() || (line.size() == 1 && line[0].is_empty())) {
			continue;
		}
		if (line[0].to_lower() == "?pluralrule") {
			for (const KeyValue<int, LocaleColumn *> &E : column_of) {
				if (E.key < line.size() && !line[E.key].is_empty() && E.value->plural_rule.is_empty()) {
					E.value->plural_rule = line[E.key];
				}
			}
			continue;
		}
		const String &key = line[0];
		const String context = (header.context_column != -1 && header.context_column < line.size()) ? line[header.context_column] : String();
		const String extra = (extra_column != -1 && extra_column < line.size()) ? line[extra_column] : String();
		if (plural_row >= 0 && (!key.is_empty() || !context.is_empty() || !extra.is_empty())) {
			close_plural_block();
		}
		if (plural_row < 0 && key.is_empty()) {
			continue;
		}
		int row = plural_row;
		if (plural_row < 0) {
			row = add_key(key, context);
			_set_row_extra(row, extra);
			for (const KeyValue<int, uint32_t> &E : note_column_of) {
				if (E.key < line.size()) {
					_set_note(row, &note_columns[E.value], line[E.key]);
				}
			}
			if (plural_column != -1 && !extra.is_empty()) {
				plural_row = row;
			}
		}
		for (const KeyValue<int, LocaleColumn *> &E : column_of) {
			if (E.key >= line.size()) {
				continue;
			}
			const String text = line[E.key].c_unescape();
			if (text.is_empty()) {
				continue;
			}
			if (plural_row >= 0) {
				plural_texts[E.key].push_back(text);
			} else {
				_set_cell(E.value, row, text);
			}
		}
	}
	close_plural_block();
	emit_changed();
	return OK;
}

Array LocalizationCatalog::_get_rows() const {
	Array rows;
	ERR_FAIL_COND_V(_load_all_columns() != OK, rows);
	bool has_context = false;
	bool has_extra = !row_extras.is_empty();
	bool has_rules = false;
	for (uint32_t row = 0; row < ids.size() && !has_context; row++) {
		has_context = ids.get_length(row) > 0 && memchr(ids.bytes.ptr() + ids.offsets[row], 0x04, ids.get_length(row)) != nullptr;
	}
	for (const LocaleColumn *column : columns) {
		has_extra = has_extra || (_get_kind() == KIND_TEXT && !column->plural_forms.is_empty());
		has_rules = has_rules || !column->plural_rule.is_empty();
	}
	PackedStringArray header;
	header.push_back("keys");
	const int context_column = has_context ? (int)header.size() : -1;
	if (has_context) {
		header.push_back("?context");
	}
	const int extra_column = has_extra ? (int)header.size() : -1;
	if (has_extra) {
		header.push_back("?" + _get_extra_name());
	}
	const int first_locale = (int)header.size();
	for (const LocaleColumn *column : columns) {
		header.push_back(column->locale);
	}
	const int first_note = (int)header.size();
	for (const NoteColumn &note_column : note_columns) {
		header.push_back("_" + String(note_column.name));
	}
	rows.push_back(header);

	// Values are read with c_unescape() (like the importer), so backslashes are escaped.
	const auto escape = [](const String &p_text) { return p_text.replace("\\", "\\\\"); };
	PackedStringArray empty_line;
	empty_line.resize(header.size());
	if (has_rules) {
		PackedStringArray line = empty_line;
		line.set(0, "?pluralrule");
		for (uint32_t i = 0; i < columns.size(); i++) {
			line.set(first_locale + i, columns[i]->plural_rule);
		}
		rows.push_back(line);
	}
	for (uint32_t row = 0; row < ids.size(); row++) {
		String key, context;
		_split_id(ids.get(row), key, context);
		PackedStringArray line = empty_line;
		line.set(0, key);
		if (context_column != -1) {
			line.set(context_column, context);
		}
		int form_count = 1;
		for (uint32_t i = 0; i < columns.size(); i++) {
			const Vector<String> *forms = columns[i]->plural_forms.getptr(row);
			form_count = forms ? MAX(form_count, (int)forms->size()) : form_count;
			line.set(first_locale + i, escape(columns[i]->cells.get(row)));
		}
		for (uint32_t i = 0; i < note_columns.size(); i++) {
			const String *note = note_columns[i].notes.getptr(row);
			if (note) {
				line.set(first_note + i, *note);
			}
		}
		if (extra_column != -1) {
			const String extra = _get_row_extra(row);
			// A plural block needs a plural key.
			line.set(extra_column, extra.is_empty() && form_count > 1 ? key : extra);
		}
		rows.push_back(line);
		for (int form = 1; form < form_count; form++) {
			PackedStringArray form_line = empty_line;
			for (uint32_t i = 0; i < columns.size(); i++) {
				const Vector<String> *forms = columns[i]->plural_forms.getptr(row);
				if (forms && form < forms->size()) {
					form_line.set(first_locale + i, escape((*forms)[form]));
				}
			}
			rows.push_back(form_line);
		}
	}
	return rows;
}

/////////////////////////////////////////////////////////////////////////////
// Text formats and files.

static Ref<TextDataFormat> _create_text_format(ResourceTextSerializer::Format p_format) {
	// JSON is not a TextDataFormat (handled by the callers).
	const char *format_classes[ResourceTextSerializer::FORMAT_MAX] = { nullptr, "INI", "CSV", "XML", "TOML", "YAML" };
	ERR_FAIL_INDEX_V((int)p_format, (int)ResourceTextSerializer::FORMAT_MAX, Ref<TextDataFormat>());
	ERR_FAIL_NULL_V(format_classes[p_format], Ref<TextDataFormat>());
	const StringName format_class = format_classes[p_format];
	ERR_FAIL_COND_V_MSG(!ClassDB::class_exists(format_class), Ref<TextDataFormat>(), vformat("The %s format is not available in this build.", format_class));
	Ref<TextDataFormat> format = Object::cast_to<TextDataFormat>(ClassDB::instantiate(format_class));
	ERR_FAIL_COND_V(format.is_null(), format);
	if (p_format == ResourceTextSerializer::FORMAT_CSV) {
		Ref<SerializerOptions> options;
		options.instantiate();
		options->set_csv_headers(false);
		format->set_options(options);
	}
	return format;
}

Error LocalizationCatalog::load_from_text(const String &p_text, ResourceTextSerializer::Format p_format) {
	Variant data;
	Error err = OK;
	String error;
	int line = 0;
	if (p_format == ResourceTextSerializer::FORMAT_JSON) {
		Ref<JSON> json;
		json.instantiate();
		err = json->parse(p_text);
		data = json->get_data();
		error = json->get_error_message();
		line = json->get_error_line();
	} else {
		const Ref<TextDataFormat> format = _create_text_format(p_format);
		ERR_FAIL_COND_V(format.is_null(), ERR_UNAVAILABLE);
		err = format->parse(p_text);
		data = format->get_data();
		error = format->get_error_message();
		line = format->get_error_line();
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't parse the localization catalog (line %d): %s", line, error));
	if (p_format == ResourceTextSerializer::FORMAT_CSV) {
		return _load_rows(data);
	}
	ERR_FAIL_COND_V_MSG(data.get_type() != Variant::DICTIONARY, ERR_INVALID_DATA, "A localization catalog must be a Dictionary of keys.");
	_set_data(p_format == ResourceTextSerializer::FORMAT_XML ? _xml_to_data(data, _get_extra_name()) : Dictionary(data));
	return OK;
}

String LocalizationCatalog::to_text(ResourceTextSerializer::Format p_format) const {
	Variant data;
	if (p_format == ResourceTextSerializer::FORMAT_CSV) {
		data = _get_rows();
	} else {
		data = p_format == ResourceTextSerializer::FORMAT_XML ? _data_to_xml(_get_data(), _get_extra_name()) : _get_data();
	}
	if (p_format == ResourceTextSerializer::FORMAT_JSON) {
		return JSON::stringify(data, "\t", false);
	}
	const Ref<TextDataFormat> format = _create_text_format(p_format);
	ERR_FAIL_COND_V(format.is_null(), String());
	format->set_data(data);
	String text, error;
	const Error err = format->get_save_text(text, error);
	ERR_FAIL_COND_V_MSG(err != OK, String(), error);
	return text;
}

Error LocalizationCatalog::_read_binary(const Ref<FileAccess> &p_file, const String &p_path) {
	clear();
	ERR_FAIL_COND_V_MSG(p_file->get_32() != LCAT_MAGIC || p_file->get_32() != LCAT_VERSION, ERR_FILE_UNRECOGNIZED, vformat("'%s' is not a localization catalog of version %d.", p_path, LCAT_VERSION));
	ERR_FAIL_COND_V_MSG(p_file->get_32() != _get_kind(), ERR_INVALID_DATA, vformat("'%s' holds another kind of localization catalog than %s.", p_path, get_class()));
	channel = p_file->get_pascal_string();
	const uint32_t row_count = p_file->get_32();
	const uint32_t locale_count = p_file->get_32();
	String error = _read_cells(p_file, row_count, ids);
	if (error.is_empty()) {
		const uint32_t extra_count = p_file->get_32();
		for (uint32_t i = 0; i < extra_count && !p_file->eof_reached(); i++) {
			const uint32_t row = p_file->get_32();
			const String extra = p_file->get_pascal_string();
			if (row < row_count) {
				row_extras[row] = extra;
			}
		}
		for (uint32_t i = 0; i < locale_count && !p_file->eof_reached(); i++) {
			LocaleColumn *column = memnew(LocaleColumn);
			column->locale = p_file->get_pascal_string();
			column->plural_rule = p_file->get_pascal_string();
			column->translated = p_file->get_32();
			column->block_offset = p_file->get_64();
			column->loaded = false;
			columns.push_back(column);
		}
		if (p_file->eof_reached()) {
			error = "Unexpected end of file.";
		}
	}
	if (!error.is_empty()) {
		clear();
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, vformat("Can't read the localization catalog '%s': %s", p_path, error));
	}
	source_path = p_path;
	_rebuild_index();
	emit_changed();
	return OK;
}

Error LocalizationCatalog::_write_binary(const Ref<FileAccess> &p_file) {
	p_file->store_32(LCAT_MAGIC);
	p_file->store_32(LCAT_VERSION);
	p_file->store_32(_get_kind());
	p_file->store_pascal_string(channel);
	p_file->store_32(ids.size());
	p_file->store_32(columns.size());
	_write_cells(p_file, ids);
	p_file->store_32(row_extras.size());
	for (const KeyValue<uint32_t, String> &E : row_extras) {
		p_file->store_32(E.key);
		p_file->store_pascal_string(E.value);
	}
	const uint64_t table_position = p_file->get_position();
	LocalVector<uint64_t> block_offsets;
	block_offsets.resize_initialized(columns.size());
	const auto store_table = [&]() {
		for (uint32_t i = 0; i < columns.size(); i++) {
			p_file->store_pascal_string(columns[i]->locale);
			p_file->store_pascal_string(columns[i]->plural_rule);
			p_file->store_32(columns[i]->translated);
			p_file->store_64(block_offsets[i]);
		}
	};
	store_table();
	for (uint32_t i = 0; i < columns.size(); i++) {
		block_offsets[i] = p_file->get_position();
		_write_cells(p_file, columns[i]->cells);
		p_file->store_32(columns[i]->plural_forms.size());
		for (const KeyValue<uint32_t, Vector<String>> &E : columns[i]->plural_forms) {
			p_file->store_32(E.key);
			p_file->store_32(E.value.size());
			for (const String &form : E.value) {
				p_file->store_pascal_string(form);
			}
		}
	}
	const uint64_t end = p_file->get_position();
	p_file->seek(table_position);
	store_table();
	p_file->seek(end);
	return p_file->get_error() == OK || p_file->get_error() == ERR_FILE_EOF ? OK : ERR_CANT_CREATE;
}

Error LocalizationCatalog::load_from_file(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	Error err = OK;
	if (extension == BINARY_EXTENSION) {
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
		ERR_FAIL_COND_V_MSG(file.is_null(), err, vformat("Can't open the localization catalog '%s'.", p_path));
		return _read_binary(file, p_path);
	}
	const ResourceTextSerializer::Format format = ResourceTextSerializer::get_format_for_extension(extension);
	ERR_FAIL_COND_V_MSG(format == ResourceTextSerializer::FORMAT_MAX, ERR_FILE_UNRECOGNIZED, vformat("'%s' has no localization catalog extension (%s).", p_path, String(", ").join(get_file_extensions())));
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Can't read the localization catalog '%s'.", p_path));
	return load_from_text(text, format);
}

Error LocalizationCatalog::save_to_file(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	// Read every locale first: the file may be the one the catalog was loaded from.
	Error err = _load_all_columns();
	ERR_FAIL_COND_V(err != OK, err);
	String text;
	if (extension != BINARY_EXTENSION) {
		const ResourceTextSerializer::Format format = ResourceTextSerializer::get_format_for_extension(extension);
		ERR_FAIL_COND_V_MSG(format == ResourceTextSerializer::FORMAT_MAX, ERR_FILE_UNRECOGNIZED, vformat("'%s' has no localization catalog extension (%s).", p_path, String(", ").join(get_file_extensions())));
		text = to_text(format);
		ERR_FAIL_COND_V(text.is_empty(), ERR_INVALID_DATA);
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, vformat("Can't open '%s' for writing.", p_path));
	if (extension == BINARY_EXTENSION) {
		return _write_binary(file);
	}
	file->store_string(text);
	return file->get_error() == OK || file->get_error() == ERR_FILE_EOF ? OK : ERR_CANT_CREATE;
}

bool LocalizationCatalog::_get(const StringName &p_name, Variant &r_ret) const {
	const String property = p_name;
	if (!property.begins_with("summary/")) {
		return false;
	}
	const int column = find_locale(property.trim_prefix("summary/"));
	if (column < 0) {
		return false;
	}
	const uint32_t translated = columns[column]->translated;
	r_ret = vformat("%d of %d translated, %d missing", translated, ids.size(), ids.size() - translated);
	return true;
}

void LocalizationCatalog::_get_property_list(List<PropertyInfo> *p_list) const {
	// Read-only summary in the inspector; known without loading the locales.
	for (const LocaleColumn *column : columns) {
		p_list->push_back(PropertyInfo(Variant::STRING, "summary/" + column->locale, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
	}
}

void LocalizationCatalog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_key_count"), &LocalizationCatalog::get_key_count);
	ClassDB::bind_method(D_METHOD("add_key", "key", "context"), &LocalizationCatalog::add_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("remove_key", "key", "context"), &LocalizationCatalog::remove_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("has_key", "key", "context"), &LocalizationCatalog::has_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("find_key", "key", "context"), &LocalizationCatalog::find_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_key", "row"), &LocalizationCatalog::get_key);
	ClassDB::bind_method(D_METHOD("get_key_context", "row"), &LocalizationCatalog::get_key_context);

	ClassDB::bind_method(D_METHOD("add_locale", "locale"), &LocalizationCatalog::add_locale);
	ClassDB::bind_method(D_METHOD("remove_locale", "locale"), &LocalizationCatalog::remove_locale);
	ClassDB::bind_method(D_METHOD("has_locale", "locale"), &LocalizationCatalog::has_locale);
	ClassDB::bind_method(D_METHOD("get_locales"), &LocalizationCatalog::get_locales);
	ClassDB::bind_method(D_METHOD("load_locale", "locale"), &LocalizationCatalog::load_locale);
	ClassDB::bind_method(D_METHOD("unload_locale", "locale"), &LocalizationCatalog::unload_locale);
	ClassDB::bind_method(D_METHOD("is_locale_loaded", "locale"), &LocalizationCatalog::is_locale_loaded);
	ClassDB::bind_method(D_METHOD("get_translated_count", "locale"), &LocalizationCatalog::get_translated_count);
	ClassDB::bind_method(D_METHOD("get_missing_rows", "locale"), &LocalizationCatalog::get_missing_rows);
	ClassDB::bind_method(D_METHOD("clear"), &LocalizationCatalog::clear);
	ClassDB::bind_method(D_METHOD("set_note", "key", "note", "context", "column"), &LocalizationCatalog::set_note, DEFVAL(StringName()), DEFVAL(StringName("notes")));
	ClassDB::bind_method(D_METHOD("get_note", "key", "context", "column"), &LocalizationCatalog::get_note, DEFVAL(StringName()), DEFVAL(StringName("notes")));
	ClassDB::bind_method(D_METHOD("get_note_columns"), &LocalizationCatalog::get_note_columns);

	ClassDB::bind_method(D_METHOD("load_from_text", "text", "format"), &LocalizationCatalog::load_from_text);
	ClassDB::bind_method(D_METHOD("to_text", "format"), &LocalizationCatalog::to_text);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &LocalizationCatalog::load_from_file);
	ClassDB::bind_method(D_METHOD("save_to_file", "path"), &LocalizationCatalog::save_to_file);

	ClassDB::bind_method(D_METHOD("_set_data", "data"), &LocalizationCatalog::_set_data);
	ClassDB::bind_method(D_METHOD("_get_data"), &LocalizationCatalog::_get_data);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &LocalizationCatalog::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &LocalizationCatalog::get_channel);
	ClassDB::bind_static_method("LocalizationCatalog", D_METHOD("get_file_extensions"), &LocalizationCatalog::get_file_extensions);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "channel"), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "key_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_key_count");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "locales", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_locales");

	ADD_SIGNAL(MethodInfo("locale_loaded", PropertyInfo(Variant::STRING, "locale")));
}

LocalizationCatalog::~LocalizationCatalog() {
	for (LocaleColumn *column : columns) {
		memdelete(column);
	}
}

/////////////////////////////////////////////////////////////////////////////
// TextLocalizationCatalog.

void TextLocalizationCatalog::set_text(const StringName &p_key, const String &p_locale, const String &p_text, const StringName &p_context) {
	const int row = add_key(p_key, p_context);
	_set_cell(_get_or_add_column(p_locale), row, p_text);
	emit_changed();
}

String TextLocalizationCatalog::get_text(const StringName &p_key, const String &p_locale, const StringName &p_context) const {
	const int row = find_key(p_key, p_context);
	const LocaleColumn *column = row < 0 ? nullptr : _get_column(p_locale);
	return column ? column->cells.get(row) : String();
}

void TextLocalizationCatalog::set_plural_texts(const StringName &p_key, const String &p_locale, const PackedStringArray &p_texts, const StringName &p_context) {
	const int row = add_key(p_key, p_context);
	_set_plural_forms(_get_or_add_column(p_locale), row, p_texts);
	emit_changed();
}

PackedStringArray TextLocalizationCatalog::get_plural_texts(const StringName &p_key, const String &p_locale, const StringName &p_context) const {
	const int row = find_key(p_key, p_context);
	const LocaleColumn *column = row < 0 ? nullptr : _get_column(p_locale);
	if (!column) {
		return PackedStringArray();
	}
	const Vector<String> *forms = column->plural_forms.getptr(row);
	if (forms) {
		return *forms;
	}
	return column->cells.get_length(row) > 0 ? PackedStringArray({ column->cells.get(row) }) : PackedStringArray();
}

String TextLocalizationCatalog::get_plural_text(const StringName &p_key, const String &p_locale, int p_n, const StringName &p_context) const {
	const int row = find_key(p_key, p_context);
	const LocaleColumn *column = row < 0 ? nullptr : _get_column(p_locale);
	if (!column) {
		return String();
	}
	const Vector<String> *forms = column->plural_forms.getptr(row);
	if (!forms) {
		return column->cells.get(row);
	}
	const int form = get_plural_form(p_locale, p_n);
	return (*forms)[CLAMP(form, 0, (int)forms->size() - 1)];
}

void TextLocalizationCatalog::set_plural_key(const StringName &p_key, const String &p_plural_key, const StringName &p_context) {
	_set_row_extra(add_key(p_key, p_context), p_plural_key);
	emit_changed();
}

String TextLocalizationCatalog::get_plural_key(const StringName &p_key, const StringName &p_context) const {
	return _get_row_extra(find_key(p_key, p_context));
}

void TextLocalizationCatalog::set_plural_rule(const String &p_locale, const String &p_rule) {
	LocaleColumn *column = _get_or_add_column(p_locale);
	ERR_FAIL_NULL(column);
	column->plural_rule = p_rule;
	emit_changed();
}

String TextLocalizationCatalog::get_plural_rule(const String &p_locale) const {
	const LocaleColumn *column = _get_column(p_locale);
	return column ? column->plural_rule : String();
}

int TextLocalizationCatalog::get_plural_form(const String &p_locale, int p_n) const {
	// Same rule order as Translation: the catalog's rule, the locale's default rule, English.
	const LocaleColumn *column = _get_column(p_locale);
	return evaluate_plural_rules(get_plural_rules(column ? column->plural_rule : String(), p_locale), p_n);
}

TextLocalizationCatalog::TextLocalizationCatalog() {
	set_channel(SNAME("interface"));
}

void TextLocalizationCatalog::add_translation(const Ref<Translation> &p_translation) {
	ERR_FAIL_COND(p_translation.is_null());
	LocaleColumn *column = _get_or_add_column(p_translation->get_locale());
	ERR_FAIL_NULL(column);
	if (!p_translation->get_plural_rules_override().is_empty()) {
		set_plural_rule(column->locale, p_translation->get_plural_rules_override());
	}
	// The `messages` property has all plural forms; OptimizedTranslation and script translations only list messages.
	const Dictionary messages = p_translation->get("messages");
	if (!messages.is_empty()) {
		for (const KeyValue<Variant, Variant> &kv : messages) {
			const bool with_context = kv.key.get_type() == Variant::ARRAY;
			const StringName key = with_context ? Array(kv.key)[1] : kv.key;
			const StringName context = with_context ? Array(kv.key)[0] : Variant(StringName());
			_set_plural_forms(column, add_key(key, context), kv.value);
		}
	} else {
		List<StringName> message_list;
		p_translation->get_message_list(&message_list);
		for (const StringName &id : message_list) {
			String key, context;
			_split_id(id, key, context);
			_set_cell(column, add_key(key, context), p_translation->get_message(key, context));
		}
	}
	emit_changed();
}

TypedArray<Translation> TextLocalizationCatalog::to_translations() {
	TypedArray<Translation> translations;
	ERR_FAIL_COND_V(_load_all_columns() != OK, translations);
	LocalVector<StringName> keys;
	LocalVector<StringName> contexts;
	for (int row = 0; row < get_key_count(); row++) {
		String key, context;
		_split_id(get_id_cells().get(row), key, context);
		keys.push_back(key);
		contexts.push_back(context);
	}
	const int locale_count = get_locales().size();
	for (int i = 0; i < locale_count; i++) {
		const LocaleColumn *column = get_locale_column(i);
		Ref<Translation> translation;
		translation.instantiate();
		translation->set_locale(column->locale);
		translation->set_plural_rules_override(column->plural_rule);
		for (uint32_t row = 0; row < keys.size(); row++) {
			const Vector<String> *forms = column->plural_forms.getptr(row);
			if (forms) {
				translation->add_plural_message(keys[row], *forms, contexts[row]);
			} else if (column->cells.get_length(row) > 0) {
				translation->add_message(keys[row], column->cells.get(row), contexts[row]);
			}
		}
		translations.push_back(translation);
	}
	return translations;
}

void TextLocalizationCatalog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "key", "locale", "text", "context"), &TextLocalizationCatalog::set_text, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_text", "key", "locale", "context"), &TextLocalizationCatalog::get_text, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("set_plural_texts", "key", "locale", "texts", "context"), &TextLocalizationCatalog::set_plural_texts, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_texts", "key", "locale", "context"), &TextLocalizationCatalog::get_plural_texts, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_text", "key", "locale", "n", "context"), &TextLocalizationCatalog::get_plural_text, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("set_plural_key", "key", "plural_key", "context"), &TextLocalizationCatalog::set_plural_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_key", "key", "context"), &TextLocalizationCatalog::get_plural_key, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("set_plural_rule", "locale", "rule"), &TextLocalizationCatalog::set_plural_rule);
	ClassDB::bind_method(D_METHOD("get_plural_rule", "locale"), &TextLocalizationCatalog::get_plural_rule);
	ClassDB::bind_method(D_METHOD("get_plural_form", "locale", "n"), &TextLocalizationCatalog::get_plural_form);
	ClassDB::bind_method(D_METHOD("add_translation", "translation"), &TextLocalizationCatalog::add_translation);
	ClassDB::bind_method(D_METHOD("to_translations"), &TextLocalizationCatalog::to_translations);
}

/////////////////////////////////////////////////////////////////////////////
// ResourceLocalizationCatalog.

void ResourceLocalizationCatalog::set_localized_path(const StringName &p_key, const String &p_locale, const String &p_path) {
	const int row = add_key(p_key);
	_set_cell(_get_or_add_column(p_locale), row, p_path);
	emit_changed();
}

String ResourceLocalizationCatalog::get_localized_path(const StringName &p_key, const String &p_locale) const {
	const int row = find_key(p_key);
	const LocaleColumn *column = row < 0 ? nullptr : _get_column(p_locale);
	return column ? column->cells.get(row) : String();
}

void ResourceLocalizationCatalog::set_expected_type(const StringName &p_key, const StringName &p_type) {
	_set_row_extra(add_key(p_key), p_type);
	emit_changed();
}

StringName ResourceLocalizationCatalog::get_expected_type(const StringName &p_key) const {
	const int row = find_key(p_key);
	if (row < 0) {
		return StringName();
	}
	// Without a stored type the key's file decides, resolved on demand: adding remaps opens no files.
	const String type = _get_row_extra(row);
	return type.is_empty() ? StringName(ResourceLoader::get_resource_type(p_key)) : StringName(type);
}

void ResourceLocalizationCatalog::add_translation_remaps(const Dictionary &p_remaps) {
	for (const KeyValue<Variant, Variant> &kv : p_remaps) {
		const String path = kv.key;
		const int row = add_key(path);
		const PackedStringArray remaps = kv.value;
		for (const String &remap : remaps) {
			// "res://texture_de.png:de": the locale follows the last colon.
			const int separator = remap.rfind(":");
			if (separator > 0) {
				_set_cell(_get_or_add_column(remap.substr(separator + 1)), row, remap.substr(0, separator));
			}
		}
	}
	emit_changed();
}

Dictionary ResourceLocalizationCatalog::to_translation_remaps() {
	Dictionary remaps;
	ERR_FAIL_COND_V(_load_all_columns() != OK, remaps);
	const PackedStringArray locales = get_locales();
	for (int row = 0; row < get_key_count(); row++) {
		PackedStringArray paths;
		for (int i = 0; i < locales.size(); i++) {
			const LocaleColumn *column = get_locale_column(i);
			if (column->cells.get_length(row) > 0) {
				paths.push_back(column->cells.get(row) + ":" + locales[i]);
			}
		}
		if (!paths.is_empty()) {
			remaps[String(get_key(row))] = paths;
		}
	}
	return remaps;
}

StringName ResourceLocalizationCatalog::get_row_channel(int p_row) const {
	if (!get_channel().is_empty()) {
		return get_channel();
	}
	String type = _get_row_extra(p_row);
	if (type.is_empty()) {
		String key, context;
		_split_id(get_id_cells().get(p_row), key, context);
		type = ResourceLoader::get_resource_type(key); // Once per key and key table, like add_translation_remaps().
	}
	if (!type.is_empty() && ClassDB::is_parent_class(type, SNAME("AudioStream"))) {
		return SNAME("audio");
	}
	if (!type.is_empty() && ClassDB::is_parent_class(type, SNAME("Subtitle"))) {
		return SNAME("subtitles");
	}
	return SNAME("interface");
}

void ResourceLocalizationCatalog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_localized_path", "key", "locale", "path"), &ResourceLocalizationCatalog::set_localized_path);
	ClassDB::bind_method(D_METHOD("get_localized_path", "key", "locale"), &ResourceLocalizationCatalog::get_localized_path);
	ClassDB::bind_method(D_METHOD("set_expected_type", "key", "type"), &ResourceLocalizationCatalog::set_expected_type);
	ClassDB::bind_method(D_METHOD("get_expected_type", "key"), &ResourceLocalizationCatalog::get_expected_type);
	ClassDB::bind_method(D_METHOD("add_translation_remaps", "remaps"), &ResourceLocalizationCatalog::add_translation_remaps);
	ClassDB::bind_method(D_METHOD("to_translation_remaps"), &ResourceLocalizationCatalog::to_translation_remaps);
}

/////////////////////////////////////////////////////////////////////////////
// Loader and saver of `.lcat` files.

// Like ResourceFormatLoaderTextTree: the type hint of recognize_path() for the following load() on this thread.
static thread_local String text_catalog_hint_path;
static thread_local String text_catalog_hint_type;

Ref<Resource> ResourceFormatLoaderLocalizationCatalog::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_UNRECOGNIZED;
	}
	String type = get_resource_type(p_path);
	if (type.is_empty() && p_path == text_catalog_hint_path) {
		type = text_catalog_hint_type; // A text format: its kind comes from the type hint.
	}
	text_catalog_hint_path = String();
	if (type.is_empty()) {
		return Ref<Resource>();
	}
	Ref<LocalizationCatalog> catalog = Object::cast_to<LocalizationCatalog>(ClassDB::instantiate(type));
	ERR_FAIL_COND_V(catalog.is_null(), Ref<Resource>());
	// Translator notes are for the editor and tools; load_from_file() called directly keeps them.
	catalog->skip_notes = !Engine::get_singleton()->is_editor_hint();
	const Error err = catalog->load_from_file(p_path);
	catalog->skip_notes = false;
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Resource>();
	}
	return catalog;
}

void ResourceFormatLoaderLocalizationCatalog::get_recognized_extensions(List<String> *p_extensions) const {
	// All of them, so the loader is asked for these paths; recognize_path() takes text formats only for catalogs.
	for (const String &extension : LocalizationCatalog::get_file_extensions()) {
		p_extensions->push_back(extension);
	}
}

bool ResourceFormatLoaderLocalizationCatalog::recognize_path(const String &p_path, const String &p_for_type) const {
	const String extension = p_path.get_extension().to_lower();
	if (extension == LocalizationCatalog::BINARY_EXTENSION) {
		return p_for_type.is_empty() || handles_type(p_for_type);
	}
	if (p_for_type.is_empty() || p_for_type == "Resource" || !handles_type(p_for_type) || !LocalizationCatalog::get_file_extensions().has(extension)) {
		return false;
	}
	// ResourceLoader calls load() right after this on the same thread, without the hint.
	text_catalog_hint_path = p_path;
	text_catalog_hint_type = p_for_type;
	return true;
}

bool ResourceFormatLoaderLocalizationCatalog::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class("TextLocalizationCatalog", p_type) || ClassDB::is_parent_class("ResourceLocalizationCatalog", p_type);
}

String ResourceFormatLoaderLocalizationCatalog::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() != LocalizationCatalog::BINARY_EXTENSION) {
		return String();
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null() || file->get_32() != LCAT_MAGIC) {
		return String();
	}
	file->get_32(); // Version.
	switch (file->get_32()) {
		case LocalizationCatalog::KIND_TEXT:
			return "TextLocalizationCatalog";
		case LocalizationCatalog::KIND_RESOURCE:
			return "ResourceLocalizationCatalog";
		default:
			return String();
	}
}

Error ResourceFormatSaverLocalizationCatalog::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<LocalizationCatalog> catalog = p_resource;
	ERR_FAIL_COND_V(catalog.is_null(), ERR_INVALID_PARAMETER);
	return catalog->save_to_file(p_path);
}

void ResourceFormatSaverLocalizationCatalog::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource)) {
		p_extensions->push_back(LocalizationCatalog::BINARY_EXTENSION);
	}
}

bool ResourceFormatSaverLocalizationCatalog::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<LocalizationCatalog>(p_resource.ptr()) != nullptr;
}
