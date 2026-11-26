/**************************************************************************/
/*  localization_catalog.h                                                */
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
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_text_serializer.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

class FileAccess;
class PluralRules;
class Translation;

// Localized strings of many keys and locales (#341). Data-oriented: every locale is one column of cells,
// stored as one UTF-8 buffer plus `uint32` offsets; the keys are a column of the same layout
// ("context\x04key", like Translation::get_message_list()). Columns of a catalog loaded from a binary `.lcat`
// file are read on first use, one locale at a time.
class LocalizationCatalog : public Resource {
	GDCLASS(LocalizationCatalog, Resource);

public:
	// UTF-8 strings addressed by row: cell `r` is bytes[offsets[r], offsets[r + 1]).
	struct Cells {
		LocalVector<uint32_t> offsets;
		LocalVector<char> bytes;

		uint32_t size() const { return offsets.size() - 1; }
		uint32_t get_length(uint32_t p_row) const { return offsets[p_row + 1] - offsets[p_row]; }
		String get(uint32_t p_row) const;
		bool equals(uint32_t p_row, const CharString &p_utf8) const;
		void set(uint32_t p_row, const char *p_utf8, uint32_t p_length);
		void append(const char *p_utf8, uint32_t p_length);
		void remove(uint32_t p_row);

		Cells() { offsets.push_back(0); }
	};

	struct LocaleColumn {
		String locale;
		String plural_rule; // Empty: the default rule of the locale.
		Cells cells;
		HashMap<uint32_t, Vector<String>> plural_forms; // Sparse: row -> all forms (form 0 is also the cell).
		uint32_t translated = 0; // Non-empty cells; known before the column is loaded.
		uint64_t block_offset = 0; // Position of the column in `source_path`; 0 = not backed by the file.
		bool loaded = true;
	};

private:
	// Translator notes (#357): the `_` columns of the text formats other than `_key`, `_context` and the extra one.
	struct NoteColumn {
		StringName name; // Without the `_`.
		HashMap<uint32_t, String> notes; // Sparse: row -> note.
	};

	Cells ids; // "key" or "context\x04key" per row.
	LocalVector<uint32_t> index; // Open addressing: row + 1 per slot, 0 = empty.
	LocalVector<LocaleColumn *> columns;
	HashMap<uint32_t, String> row_extras; // Sparse: plural key (text) or expected type (resource) per row.
	LocalVector<NoteColumn> note_columns; // Never in `.lcat`.
	bool skip_notes = false; // Set by the loader outside the editor: runtime lookups don't need notes.
	String source_path; // Binary file the unloaded columns are read from.
	StringName channel;
	mutable Mutex load_mutex;

	static CharString _make_id(const StringName &p_key, const StringName &p_context);
	void _index_insert(uint32_t p_row);
	void _rebuild_index();
	int _find_row(const CharString &p_id) const;
	Error _load_column(uint32_t p_column) const;
	// Loads every column and drops the link to the file (rows change).
	void _detach_columns();

	Dictionary _get_data() const;
	void _set_data(const Dictionary &p_data);
	Error _load_rows(const Array &p_rows);
	Array _get_rows() const;
	Error _read_binary(const Ref<FileAccess> &p_file, const String &p_path);
	Error _write_binary(const Ref<FileAccess> &p_file);
	// The note column of `p_field` (`_name`), added if missing; null for the reserved fields or when notes are skipped.
	NoteColumn *_get_note_column(const String &p_field);
	void _set_note(uint32_t p_row, NoteColumn *p_column, const String &p_note);

	friend class ResourceFormatLoaderLocalizationCatalog;

protected:
	static void _bind_methods();
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	// "plural" for text catalogs, "type" for resource catalogs: `_plural` field, `?plural` CSV column, XML attribute.
	virtual String _get_extra_name() const = 0;
	virtual uint32_t _get_kind() const = 0;

	// Column of `p_locale` with its cells loaded, or null.
	LocaleColumn *_get_column(const String &p_locale) const;
	// Column of `p_locale`, added if missing.
	LocaleColumn *_get_or_add_column(const String &p_locale);
	Error _load_all_columns() const;
	void _set_cell(LocaleColumn *p_column, uint32_t p_row, const String &p_value);
	// Form 0 also goes into the cell; one form or none removes the plural entry.
	void _set_plural_forms(LocaleColumn *p_column, uint32_t p_row, const PackedStringArray &p_forms);
	void _set_row_extra(uint32_t p_row, const String &p_value);
	String _get_row_extra(int p_row) const;

public:
	enum Kind {
		KIND_TEXT,
		KIND_RESOURCE,
	};

	static constexpr const char *BINARY_EXTENSION = "lcat";

	// The file extensions of catalogs: `lcat` and the text formats (the one list for settings, manifests and loader).
	static PackedStringArray get_file_extensions();
	// One compiled rule per rule text, shared and immutable (any thread): `p_rule`, else the default rule of
	// `p_locale`, else English. Freed by finish_plural_rules() on shutdown.
	static const PluralRules *get_plural_rules(const String &p_rule, const String &p_locale);
	// PluralRules caches results: evaluated under a lock.
	static int evaluate_plural_rules(const PluralRules *p_rules, int p_n);
	static void finish_plural_rules();

	// C++ access for the servers (#343, #344). get_locale_column() is for the main thread (the servers' rebuilds).
	const Cells &get_id_cells() const { return ids; }
	int find_locale(const String &p_locale) const;
	const LocaleColumn *get_locale_column(int p_index) const;
	// Copies a non-empty cell (and its plural forms) of any column, loading it if needed. Read under the load lock,
	// so it is safe on any thread while the main thread unloads columns (#357).
	bool read_cell(int p_column, uint32_t p_row, String &r_text, Vector<String> *r_forms = nullptr) const;
	// The locale channel the key of `p_row` follows (TranslationServer.get_locale_for()).
	virtual StringName get_row_channel(int p_row) const { return channel; }

	void set_channel(const StringName &p_channel);
	StringName get_channel() const { return channel; }

	int get_key_count() const { return ids.size(); }
	int add_key(const StringName &p_key, const StringName &p_context = StringName());
	void remove_key(const StringName &p_key, const StringName &p_context = StringName());
	bool has_key(const StringName &p_key, const StringName &p_context = StringName()) const;
	int find_key(const StringName &p_key, const StringName &p_context = StringName()) const;
	StringName get_key(int p_row) const;
	StringName get_key_context(int p_row) const;

	void add_locale(const String &p_locale);
	void remove_locale(const String &p_locale);
	bool has_locale(const String &p_locale) const;
	PackedStringArray get_locales() const;
	Error load_locale(const String &p_locale);
	void unload_locale(const String &p_locale);
	bool is_locale_loaded(const String &p_locale) const;
	int get_translated_count(const String &p_locale) const;
	PackedInt32Array get_missing_rows(const String &p_locale) const;
	void clear();

	void set_note(const StringName &p_key, const String &p_note, const StringName &p_context = StringName(), const StringName &p_column = StringName("notes"));
	String get_note(const StringName &p_key, const StringName &p_context = StringName(), const StringName &p_column = StringName("notes")) const;
	PackedStringArray get_note_columns() const;

	Error load_from_text(const String &p_text, ResourceTextSerializer::Format p_format);
	String to_text(ResourceTextSerializer::Format p_format) const;
	Error load_from_file(const String &p_path);
	Error save_to_file(const String &p_path);

	~LocalizationCatalog();
};

// Texts per key and locale, with context, plural forms and a plural rule per locale.
class TextLocalizationCatalog : public LocalizationCatalog {
	GDCLASS(TextLocalizationCatalog, LocalizationCatalog);

protected:
	static void _bind_methods();
	virtual String _get_extra_name() const override { return "plural"; }
	virtual uint32_t _get_kind() const override { return KIND_TEXT; }

public:
	void set_text(const StringName &p_key, const String &p_locale, const String &p_text, const StringName &p_context = StringName());
	String get_text(const StringName &p_key, const String &p_locale, const StringName &p_context = StringName()) const;
	void set_plural_texts(const StringName &p_key, const String &p_locale, const PackedStringArray &p_texts, const StringName &p_context = StringName());
	PackedStringArray get_plural_texts(const StringName &p_key, const String &p_locale, const StringName &p_context = StringName()) const;
	String get_plural_text(const StringName &p_key, const String &p_locale, int p_n, const StringName &p_context = StringName()) const;
	void set_plural_key(const StringName &p_key, const String &p_plural_key, const StringName &p_context = StringName());
	String get_plural_key(const StringName &p_key, const StringName &p_context = StringName()) const;
	void set_plural_rule(const String &p_locale, const String &p_rule);
	String get_plural_rule(const String &p_locale) const;
	int get_plural_form(const String &p_locale, int p_n) const;

	void add_translation(const Ref<Translation> &p_translation);
	TypedArray<Translation> to_translations();

	TextLocalizationCatalog();
};

// Localized resource paths per key (the original path) and locale, with the expected resource type per key.
class ResourceLocalizationCatalog : public LocalizationCatalog {
	GDCLASS(ResourceLocalizationCatalog, LocalizationCatalog);

protected:
	static void _bind_methods();
	virtual String _get_extra_name() const override { return "type"; }
	virtual uint32_t _get_kind() const override { return KIND_RESOURCE; }

public:
	void set_localized_path(const StringName &p_key, const String &p_locale, const String &p_path);
	String get_localized_path(const StringName &p_key, const String &p_locale) const;
	void set_expected_type(const StringName &p_key, const StringName &p_type);
	StringName get_expected_type(const StringName &p_key) const;

	void add_translation_remaps(const Dictionary &p_remaps);
	Dictionary to_translation_remaps();

	// The channel, or if it is empty the one of the key's type: AudioStream -> `audio`, Subtitle -> `subtitles`,
	// otherwise `interface`.
	virtual StringName get_row_channel(int p_row) const override;
};

class ResourceFormatLoaderLocalizationCatalog : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderLocalizationCatalog, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	// Text formats only with a catalog type hint: they don't name their kind, and other loaders own them otherwise.
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverLocalizationCatalog : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverLocalizationCatalog, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};
