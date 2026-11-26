/**************************************************************************/
/*  text_localization_server.cpp                                          */
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

#include "text_localization_server.h"

#include "core/object/class_db.h"
#include "core/string/plural_rules.h"
#include "core/string/text_insertion_server.h"
#include "core/string/translation.h"
#include "core/string/translation_domain.h"
#include "core/string/translation_server.h"

// The tr() adapter: one Translation in the main domain that answers from the resolved column of the active chain.
// Its locale is the active locale, so TranslationDomain matches it with one string compare.
class TextLocalizationTranslation : public Translation {
	GDSOFTCLASS(TextLocalizationTranslation, Translation);

public:
	virtual StringName get_message(const StringName &p_src_text, const StringName &p_context = "") const override {
		const TextLocalizationServer *server = TextLocalizationServer::get_singleton();
		return server ? server->get_message(p_src_text, p_context) : StringName();
	}

	virtual StringName get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context = "") const override {
		const TextLocalizationServer *server = TextLocalizationServer::get_singleton();
		return server ? server->get_plural_message(p_src_text, p_n, p_context) : StringName();
	}

	virtual void get_message_list(List<StringName> *r_messages) const override {
		const TextLocalizationServer *server = TextLocalizationServer::get_singleton();
		if (server) {
			server->get_message_list(r_messages);
		}
	}

	virtual int get_message_count() const override {
		List<StringName> messages;
		get_message_list(&messages);
		return messages.size();
	}
};

TextLocalizationServer *TextLocalizationServer::singleton = nullptr;

TextLocalizationServer::TextData::~TextData() {
	for (PluralRules *rule : rules) {
		if (rule) {
			memdelete(rule);
		}
	}
}

LocalizationServer::Data *TextLocalizationServer::_create_data() const {
	return memnew(TextData);
}

void TextLocalizationServer::_resolve(Data *p_data) const {
	TextData *text_data = static_cast<TextData *>(p_data);
	const KeyTable *table = text_data->keys;
	const uint32_t row_count = table->keys.size();
	text_data->texts.resize(row_count);
	text_data->names.resize(row_count);
	// The chain first (a `de_DE` text beats a `de` text), then the layers from the highest priority.
	for (uint32_t chain_index = 0; chain_index < (uint32_t)text_data->chain.size(); chain_index++) {
		const String &locale = text_data->chain[chain_index];
		String rule;
		for (uint32_t layer = table->layers.size(); layer-- > 0;) {
			const LocalizationCatalog *catalog = table->layers[layer].ptr();
			const int column_index = catalog->find_locale(locale);
			const LocalizationCatalog::LocaleColumn *column = column_index < 0 ? nullptr : catalog->get_locale_column(column_index);
			if (!column) {
				continue;
			}
			if (rule.is_empty()) {
				rule = column->plural_rule;
			}
			for (uint32_t row = 0; row < row_count; row++) {
				const int32_t catalog_row = table->get_catalog_row(layer, row);
				if (catalog_row < 0 || !text_data->texts[row].is_empty() || column->cells.get_length(catalog_row) == 0) {
					continue;
				}
				text_data->texts[row] = column->cells.get(catalog_row);
				const Vector<String> *forms = column->plural_forms.getptr(catalog_row);
				if (forms) {
					text_data->plurals.insert(row, PluralEntry{ *forms, chain_index });
				}
			}
		}
		if (rule.is_empty() && TranslationServer::get_singleton()) {
			rule = TranslationServer::get_singleton()->get_plural_rules(locale);
		}
		text_data->rules.push_back(PluralRules::parse(rule.is_empty() ? String("nplurals=2; plural=(n != 1);") : rule));
	}
}

void TextLocalizationServer::_data_published(const Data *p_data, const Data *p_previous) {
	TranslationServer *translation_server = TranslationServer::get_singleton();
	if (!translation_server) {
		return;
	}
	if (!p_data->locale.is_empty()) {
		adapter->set_locale(p_data->locale);
	}
	// The adapter is only in the main domain while there are catalogs: no cost for projects without them.
	const Ref<TranslationDomain> domain = translation_server->get_main_domain();
	const bool needed = !p_data->keys->layers.is_empty();
	if (needed != domain->has_translation(adapter)) {
		if (needed) {
			domain->add_translation(adapter);
		} else {
			domain->remove_translation(adapter);
		}
	}
}

String TextLocalizationServer::_get_text(const TextData *p_data, uint32_t p_row, int p_count) const {
	if (p_count >= 0) {
		const PluralEntry *plural = p_data->plurals.getptr(p_row);
		if (plural) {
			const PluralRules *rules = p_data->rules[plural->rule];
			const int form = rules ? rules->evaluate(p_count) : (p_count == 1 ? 0 : 1);
			return plural->forms[CLAMP(form, 0, (int)plural->forms.size() - 1)];
		}
	}
	return p_data->texts[p_row];
}

String TextLocalizationServer::_get_raw_text(const KeyTable *p_keys, const PackedStringArray &p_chain, uint32_t p_row, int p_count) const {
	for (const String &locale : p_chain) {
		for (uint32_t layer = p_keys->layers.size(); layer-- > 0;) {
			const int32_t catalog_row = p_keys->get_catalog_row(layer, p_row);
			if (catalog_row < 0) {
				continue;
			}
			const TextLocalizationCatalog *catalog = static_cast<const TextLocalizationCatalog *>(p_keys->layers[layer].ptr());
			const int column_index = catalog->find_locale(locale);
			const LocalizationCatalog::LocaleColumn *column = column_index < 0 ? nullptr : catalog->get_locale_column(column_index);
			if (!column || column->cells.get_length(catalog_row) == 0) {
				continue;
			}
			const Vector<String> *forms = p_count >= 0 ? column->plural_forms.getptr(catalog_row) : nullptr;
			if (forms) {
				return (*forms)[CLAMP(catalog->get_plural_form(locale, p_count), 0, (int)forms->size() - 1)];
			}
			return column->cells.get(catalog_row);
		}
	}
	return String();
}

String TextLocalizationServer::_finish(const TextData *p_data, int p_row, const StringName &p_key, int p_count, const Dictionary &p_args) const {
	const String text = p_row < 0 ? String() : _get_text(p_data, p_row, p_count);
	if (text.is_empty()) {
#ifdef DEBUG_ENABLED
		bool first = false;
		{
			MutexLock lock(p_data->mutex);
			if (!p_data->reported_missing.has(p_key)) {
				p_data->reported_missing.insert(p_key);
				first = true;
			}
		}
		if (first) {
			const_cast<TextLocalizationServer *>(this)->_emit_on_main_thread(SNAME("key_missing"), p_key, p_data->locale);
		}
#endif
		// Keys are often source texts: their placeholders are filled too.
		return p_args.is_empty() ? String(p_key) : TextInsertionServer::get_singleton()->insert(p_key, p_args);
	}
	if (p_args.is_empty()) {
		return text;
	}
	if (p_count >= 0 && p_data->plurals.has(p_row)) {
		return TextInsertionServer::get_singleton()->insert(text, p_args);
	}
	// The parsed template of the row is cached with the data of the chain.
	Ref<TextInsertionTemplate> text_template;
	{
		MutexLock lock(p_data->mutex);
		const Ref<TextInsertionTemplate> *cached = p_data->templates.getptr(p_row);
		if (cached) {
			text_template = *cached;
		}
	}
	if (text_template.is_null()) {
		text_template = TextInsertionServer::get_singleton()->parse(text);
		MutexLock lock(p_data->mutex);
		p_data->templates.insert(p_row, text_template);
	}
	return TextInsertionServer::get_singleton()->insert_template(text_template, p_args);
}

String TextLocalizationServer::translate(const StringName &p_key, const StringName &p_context, int p_count, const Dictionary &p_args) const {
	const TextData *text_data = _get_text_data();
	return _finish(text_data, text_data->keys->find(p_key, p_context), p_key, p_count, p_args);
}

String TextLocalizationServer::translate_for_locale(const String &p_locale, const StringName &p_key, const StringName &p_context, int p_count, const Dictionary &p_args) const {
	const TextData *text_data = _get_text_data();
	const PackedStringArray chain = get_locale_chain(p_locale);
	if (!chain.is_empty() && chain[0] == text_data->locale) {
		return translate(p_key, p_context, p_count, p_args);
	}
	const int row = text_data->keys->find(p_key, p_context);
	const String text = row < 0 ? String() : _get_raw_text(text_data->keys, chain, row, p_count);
	const String result = text.is_empty() ? String(p_key) : text;
	return p_args.is_empty() ? result : TextInsertionServer::get_singleton()->insert(result, p_args);
}

int TextLocalizationServer::get_key_id(const StringName &p_key, const StringName &p_context) const {
	return _get_text_data()->keys->find(p_key, p_context);
}

String TextLocalizationServer::translate_id(int p_id, int p_count, const Dictionary &p_args) const {
	const TextData *text_data = _get_text_data();
	ERR_FAIL_INDEX_V(p_id, (int)text_data->keys->keys.size(), String());
	return _finish(text_data, p_id, text_data->keys->keys[p_id].key, p_count, p_args);
}

PackedStringArray TextLocalizationServer::get_translation_keys(const String &p_text) const {
	const TextData *text_data = _get_text_data();
	PackedStringArray result;
	MutexLock lock(text_data->mutex);
	if (!text_data->reverse_built) {
		for (uint32_t row = 0; row < text_data->texts.size(); row++) {
			if (!text_data->texts[row].is_empty()) {
				text_data->reverse[text_data->texts[row]].push_back(row);
			}
		}
		text_data->reverse_built = true;
	}
	const LocalVector<uint32_t> *rows = text_data->reverse.getptr(p_text);
	if (rows) {
		for (const uint32_t row : *rows) {
			result.push_back(text_data->keys->keys[row].key);
		}
	}
	return result;
}

StringName TextLocalizationServer::get_translation_key(const String &p_text) const {
	const PackedStringArray found = get_translation_keys(p_text);
	return found.is_empty() ? StringName() : StringName(found[0]);
}

StringName TextLocalizationServer::get_translation_key_for_locale(const String &p_locale, const String &p_text) const {
	const TextData *text_data = _get_text_data();
	const PackedStringArray chain = get_locale_chain(p_locale);
	if (!chain.is_empty() && chain[0] == text_data->locale) {
		return get_translation_key(p_text);
	}
	for (uint32_t row = 0; row < text_data->keys->keys.size(); row++) {
		if (_get_raw_text(text_data->keys, chain, row, -1) == p_text) {
			return text_data->keys->keys[row].key;
		}
	}
	return StringName();
}

bool TextLocalizationServer::has_translation(const StringName &p_key, const StringName &p_context) const {
	const TextData *text_data = _get_text_data();
	const int row = text_data->keys->find(p_key, p_context);
	return row >= 0 && !text_data->texts[row].is_empty();
}

PackedStringArray TextLocalizationServer::get_missing_keys(const String &p_locale) const {
	const TextData *text_data = _get_text_data();
	const PackedStringArray full_chain = get_locale_chain(p_locale.is_empty() ? text_data->locale : p_locale);
	// Only the locale and its parents: a key that shows the fallback locale is missing.
	PackedStringArray chain;
	for (const String &locale : full_chain) {
		if (locale == full_chain[0] || full_chain[0].begins_with(locale + "_")) {
			chain.push_back(locale);
		}
	}
	PackedStringArray missing;
	for (uint32_t row = 0; row < text_data->keys->keys.size(); row++) {
		if (_get_raw_text(text_data->keys, chain, row, -1).is_empty()) {
			missing.push_back(text_data->keys->keys[row].key);
		}
	}
	return missing;
}

StringName TextLocalizationServer::get_message(const StringName &p_key, const StringName &p_context) const {
	const TextData *text_data = _get_text_data();
	const int row = text_data->keys->find(p_key, p_context);
	if (row < 0 || text_data->texts[row].is_empty()) {
		return StringName();
	}
	MutexLock lock(text_data->mutex);
	StringName &name = text_data->names[row];
	if (name.is_empty()) {
		name = text_data->texts[row];
	}
	return name;
}

StringName TextLocalizationServer::get_plural_message(const StringName &p_key, int p_count, const StringName &p_context) const {
	const TextData *text_data = _get_text_data();
	const int row = text_data->keys->find(p_key, p_context);
	if (row < 0) {
		return StringName();
	}
	const String text = _get_text(text_data, row, p_count);
	return text.is_empty() ? StringName() : StringName(text);
}

void TextLocalizationServer::get_message_list(List<StringName> *r_messages) const {
	const TextData *text_data = _get_text_data();
	for (uint32_t row = 0; row < text_data->texts.size(); row++) {
		if (!text_data->texts[row].is_empty()) {
			r_messages->push_back(text_data->keys->keys[row].key);
		}
	}
}

void TextLocalizationServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("translate", "key", "context", "count", "args"), &TextLocalizationServer::translate, DEFVAL(StringName()), DEFVAL(-1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("translate_for_locale", "locale", "key", "context", "count", "args"), &TextLocalizationServer::translate_for_locale, DEFVAL(StringName()), DEFVAL(-1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_key_id", "key", "context"), &TextLocalizationServer::get_key_id, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("translate_id", "id", "count", "args"), &TextLocalizationServer::translate_id, DEFVAL(-1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_translation_key", "text"), &TextLocalizationServer::get_translation_key);
	ClassDB::bind_method(D_METHOD("get_translation_key_for_locale", "locale", "text"), &TextLocalizationServer::get_translation_key_for_locale);
	ClassDB::bind_method(D_METHOD("get_translation_keys", "text"), &TextLocalizationServer::get_translation_keys);
	ClassDB::bind_method(D_METHOD("has_translation", "key", "context"), &TextLocalizationServer::has_translation, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_missing_keys", "locale"), &TextLocalizationServer::get_missing_keys, DEFVAL(String()));

	ADD_SIGNAL(MethodInfo("key_missing", PropertyInfo(Variant::STRING_NAME, "key"), PropertyInfo(Variant::STRING, "locale")));
}

TextLocalizationServer::TextLocalizationServer() {
	singleton = this;
	adapter = Ref<Translation>(memnew(TextLocalizationTranslation));
	_rebuild(true);
}

TextLocalizationServer::~TextLocalizationServer() {
	singleton = nullptr;
}
