/**************************************************************************/
/*  text_localization_server.h                                            */
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

#include "core/os/mutex.h"
#include "core/string/localization_server.h"
#include "core/string/text_insertion_template.h"

class PluralRules;
class Translation;

// Texts with context and plural forms from TextLocalizationCatalogs (#343). On a locale change one resolved column
// (a String per row) is built for the locale chain over all layers, so translate() is one hash lookup and one array
// read, translate_id() one array read. One adapter Translation per channel domain (`interface` = the main
// TranslationDomain, `subtitles` = the domain "subtitles", ...) answers tr(), atr() and auto translation from the
// same column, for the keys of its channel.
class TextLocalizationServer : public LocalizationServer {
	GDCLASS(TextLocalizationServer, LocalizationServer);

	static TextLocalizationServer *singleton;

	struct PluralEntry {
		Vector<String> forms;
		const PluralRules *rule = nullptr; // Of the column the forms come from (shared, see LocalizationCatalog).
	};

	struct TextData : public Data {
		LocalVector<String> texts; // Row -> text of its channel's chain, empty = missing.
		HashMap<uint32_t, PluralEntry> plurals;

		// Caches, filled on demand.
		mutable Mutex mutex;
		mutable LocalVector<StringName> names; // Row -> text as StringName, for the tr() adapter.
		mutable HashMap<uint32_t, Ref<TextInsertionTemplate>> templates;
		mutable HashMap<String, LocalVector<uint32_t>> reverse; // Text -> rows.
		mutable bool reverse_built = false;
		mutable HashSet<StringName> reported_missing;
	};

	HashMap<StringName, Ref<Translation>> adapters; // Channel -> the tr() adapter in the channel's domain.

	_FORCE_INLINE_ const TextData *_get_text_data() const { return static_cast<const TextData *>(_get_data()); }
	static int _find_channel_row(const TextData *p_data, const StringName &p_key, const StringName &p_context, const StringName &p_channel);
	String _get_text(const TextData *p_data, uint32_t p_row, int p_count) const;
	String _finish(const TextData *p_data, int p_row, const StringName &p_key, int p_count, const Dictionary &p_args) const;

protected:
	static void _bind_methods();

	virtual StringName _get_catalog_class() const override { return SNAME("TextLocalizationCatalog"); }
	virtual Data *_create_data() const override;
	virtual void _resolve(Data *p_data, const Data *p_previous, const LocalVector<uint8_t> *p_dirty) const override;
	virtual void _data_published(const Data *p_data, const Data *p_previous) override;

public:
	static TextLocalizationServer *get_singleton() { return singleton; }

	String translate(const StringName &p_key, const StringName &p_context = StringName(), int p_count = -1, const Dictionary &p_args = Dictionary()) const;
	String translate_for_locale(const String &p_locale, const StringName &p_key, const StringName &p_context = StringName(), int p_count = -1, const Dictionary &p_args = Dictionary()) const;
	int get_key_id(const StringName &p_key, const StringName &p_context = StringName()) const;
	String translate_id(int p_id, int p_count = -1, const Dictionary &p_args = Dictionary()) const;

	PackedStringArray get_translation_keys(const String &p_text) const;

	bool has_translation(const StringName &p_key, const StringName &p_context = StringName()) const;
	PackedStringArray get_missing_keys(const String &p_locale = String()) const;

	// For the tr() adapters, one per channel domain: StringName() when the key is not in `p_channel` or has no text.
	StringName get_message(const StringName &p_key, const StringName &p_context, const StringName &p_channel) const;
	StringName get_plural_message(const StringName &p_key, int p_count, const StringName &p_context, const StringName &p_channel) const;
	void get_message_list(List<StringName> *r_messages, const StringName &p_channel) const;
	bool is_adapter(const Ref<Translation> &p_translation) const;

	TextLocalizationServer();
	~TextLocalizationServer();
};
