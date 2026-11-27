/**************************************************************************/
/*  text_insertion_server.cpp                                             */
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

#include "text_insertion_server.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation_server.h"

static const char *SETTING_DEFAULT_VALUES = "internationalization/text_insertion/default_values";
static const char *SETTING_WARN_MISSING = "internationalization/text_insertion/warn_missing_values";
static const char *SETTING_ESCAPE_BBCODE = "internationalization/text_insertion/escape_bbcode";

TextInsertionServer *TextInsertionServer::singleton = nullptr;

// Groups the integer digits by thousands with the separators of the active locale's language
// and uses the native digits of the locale where TranslationServer knows them.
static String _format_number(const Variant &p_value) {
	String plain;
	switch (p_value.get_type()) {
		case Variant::INT:
			plain = String::num_int64((int64_t)p_value);
			break;
		case Variant::FLOAT:
			plain = String::num((double)p_value);
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
			plain = p_value;
			break;
		default:
			return p_value.stringify();
	}

	// Only plain decimal numbers are grouped; exponents, `inf` and other text stay as they are.
	const char32_t *src = plain.ptr();
	const int length = plain.length();
	const int start = (length > 0 && (src[0] == U'-' || src[0] == U'+')) ? 1 : 0;
	int end = length;
	for (int i = start; i < length; i++) {
		if (src[i] == U'.' && end == length) {
			end = i;
		} else if (src[i] < U'0' || src[i] > U'9') {
			return plain;
		}
	}

	TranslationServer *translation_server = TranslationServer::get_singleton();
	const String locale = translation_server ? translation_server->get_locale() : String("en");
	const String language = locale.get_slicec('_', 0);
	static const char *const DOT_GROUPS[] = { "da", "de", "el", "es", "hr", "id", "is", "it", "nl", "pt", "ro", "sl", "sr", "tr", "vi", nullptr };
	static const char *const SPACE_GROUPS[] = { "be", "bg", "cs", "et", "fi", "fr", "hu", "kk", "lt", "lv", "nb", "nn", "no", "pl", "ru", "sk", "sv", "uk", nullptr };
	char32_t group = U',';
	char32_t decimal = U'.';
	for (int k = 0; DOT_GROUPS[k]; k++) {
		if (language == DOT_GROUPS[k]) {
			group = U'.';
			decimal = U',';
		}
	}
	for (int k = 0; SPACE_GROUPS[k]; k++) {
		if (language == SPACE_GROUPS[k]) {
			group = 0x00A0;
			decimal = U',';
		}
	}

	// TranslationServer maps digits and the decimal point one to one for locales with native numerals.
	const String digits = (translation_server && !locale.is_empty()) ? translation_server->format_number(plain, locale) : plain;
	const bool native = digits != plain;
	String result;
	result.reserve(length + length / 3 + 1);
	for (int i = 0; i < length; i++) {
		if (i > start && i < end && (end - i) % 3 == 0) {
			result += group;
		}
		result += (i == end && !native) ? decimal : digits[i];
	}
	return result;
}

void TextInsertionServer::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, SETTING_DEFAULT_VALUES, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::STRING, PROPERTY_HINT_FILE, "*.tres,*.res")), PackedStringArray());
	GLOBAL_DEF(SETTING_WARN_MISSING, true);
	GLOBAL_DEF(SETTING_ESCAPE_BBCODE, false);
}

void TextInsertionServer::_load_settings() {
	if (settings_loaded.is_set()) {
		return;
	}
	settings_loaded.set();
	const ProjectSettings *project = ProjectSettings::get_singleton();
	if (!project) {
		return;
	}
	warn_missing.set_to(project->get_setting(SETTING_WARN_MISSING, true));
	escape_bbcode.set_to(project->get_setting(SETTING_ESCAPE_BBCODE, false));
	const PackedStringArray paths = project->get_setting(SETTING_DEFAULT_VALUES, PackedStringArray());
	for (const String &path : paths) {
		const Ref<TextInsertionValues> loaded = ResourceLoader::load(path);
		if (loaded.is_null()) {
			ERR_PRINT(vformat("TextInsertionServer: Cannot load the default values \"%s\".", path));
			continue;
		}
		add_values(loaded);
	}
}

void TextInsertionServer::_emit_value_changed(const StringName &p_name) {
	// Signals are sent on the main thread.
	if (Thread::is_main_thread()) {
		emit_signal(SNAME("value_changed"), p_name);
	} else {
		call_deferred(SNAME("emit_signal"), SNAME("value_changed"), p_name);
	}
}

void TextInsertionServer::_emit_values_changed() {
	if (Thread::is_main_thread()) {
		emit_signal(SNAME("values_changed"));
	} else {
		call_deferred(SNAME("emit_signal"), SNAME("values_changed"));
	}
}

void TextInsertionServer::_warn_once(const String &p_key, const String &p_message) const {
	if (!warn_missing.is_set()) {
		return;
	}
	bool first = false;
	{
		MutexLock lock(mutex);
		first = !warned.has(p_key);
		if (first) {
			warned.insert(p_key);
		}
	}
	if (first) {
		WARN_PRINT(p_message);
	}
}

bool TextInsertionServer::_resolve(const StringName &p_name, const Dictionary &p_args, Variant &r_value) const {
	if (!p_args.is_empty()) {
		// Script dictionaries usually have String keys, which do not match StringName keys.
		const Variant *arg = p_args.getptr(p_name);
		if (!arg) {
			arg = p_args.getptr(String(p_name));
		}
		if (arg) {
			r_value = *arg;
			return true;
		}
	}

	Callable provider;
	{
		MutexLock lock(mutex);
		// Providers read runtime state of the game: only called on the main thread.
		const Callable *found = Thread::is_main_thread() ? providers.getptr(p_name) : nullptr;
		if (!found) {
			const Variant *value = values.getptr(p_name);
			for (uint32_t idx = 0; !value && idx < values_resources.size(); idx++) {
				value = values_resources[idx].values->get_value_ptr(p_name);
			}
			if (value) {
				r_value = *value;
			}
			return value != nullptr;
		}
		provider = *found;
	}
	r_value = provider.call();
	return true;
}

Variant TextInsertionServer::_apply_modifier(const TextInsertionTemplate::Modifier &p_modifier, const Variant &p_value, const Dictionary &p_args, int p_depth) const {
	switch (p_modifier.kind) {
		case TextInsertionTemplate::MODIFIER_T: {
			const StringName key = p_value.operator String();
			const TextLocalizationServer *text_localization_server = TextLocalizationServer::get_singleton();
			if (text_localization_server && text_localization_server->has_translation(key)) {
				return text_localization_server->translate(key);
			}
			const TranslationServer *translation_server = TranslationServer::get_singleton();
			return translation_server ? String(translation_server->translate(key)) : String(key);
		}
		case TextInsertionTemplate::MODIFIER_UPPER:
			return p_value.operator String().to_upper();
		case TextInsertionTemplate::MODIFIER_LOWER:
			return p_value.operator String().to_lower();
		case TextInsertionTemplate::MODIFIER_CAPITALIZE:
			return p_value.operator String().capitalize();
		case TextInsertionTemplate::MODIFIER_NUMBER:
			return _format_number(p_value);
		case TextInsertionTemplate::MODIFIER_F: {
			if (p_depth >= MAX_DEPTH) {
				return p_value;
			}
			const Ref<TextInsertionTemplate> nested = _get_template(p_value);
			return _insert(nested.ptr(), p_args, false, p_depth + 1);
		}
		case TextInsertionTemplate::MODIFIER_CUSTOM:
			break;
	}

	Callable modifier;
	{
		MutexLock lock(mutex);
		const Callable *found = modifiers.getptr(p_modifier.name);
		if (found) {
			modifier = *found;
		}
	}
	if (modifier.is_valid()) {
		return modifier.call(p_value);
	}
#ifdef DEBUG_ENABLED
	_warn_once(":" + String(p_modifier.name), vformat("TextInsertionServer: Unknown modifier \"%s\".", p_modifier.name));
#endif
	return p_value;
}

String TextInsertionServer::_insert(const TextInsertionTemplate *p_template, const Dictionary &p_args, bool p_escape, int p_depth) const {
	String result;
	result.reserve(p_template->literal_length + 16 * (int64_t)p_template->segments.size() + 1);
	for (const TextInsertionTemplate::Segment &segment : p_template->segments) {
		if (segment.name.is_empty()) {
			result.append_utf32_unchecked(segment.text.span());
			continue;
		}
		Variant value;
		if (!_resolve(segment.name, p_args, value)) {
#ifdef DEBUG_ENABLED
			_warn_once(segment.name, vformat("TextInsertionServer: No value for \"%s\", the placeholder is kept.", segment.text));
#endif
			result.append_utf32_unchecked(segment.text.span());
			continue;
		}
		for (uint32_t idx = 0; idx < segment.modifier_count; idx++) {
			value = _apply_modifier(p_template->modifiers[segment.modifier_begin + idx], value, p_args, p_depth);
		}
		String text = value;
		if (p_escape && text.find_char(U'[') >= 0) {
			text = text.replace("[", "[lb]");
		}
		result.append_utf32_unchecked(text.span());
	}
	return result;
}

Ref<TextInsertionTemplate> TextInsertionServer::_get_template(const String &p_text) const {
	{
		MutexLock lock(mutex);
		const Ref<TextInsertionTemplate> *cached = cache.getptr(p_text);
		if (cached) {
			return *cached;
		}
	}
	const Ref<TextInsertionTemplate> parsed = parse(p_text);
	MutexLock lock(mutex);
	cache.insert(p_text, parsed);
	return parsed;
}

String TextInsertionServer::insert(const String &p_text, const Dictionary &p_args, Escape p_escape) {
	if (p_text.find_char(U'{') < 0 && p_text.find_char(U'}') < 0) {
		return p_text;
	}
	_load_settings();
	const Ref<TextInsertionTemplate> parsed = _get_template(p_text);
	return _insert(parsed.ptr(), p_args, p_escape == ESCAPE_BBCODE || (p_escape == ESCAPE_DEFAULT && escape_bbcode.is_set()), 0);
}

Ref<TextInsertionTemplate> TextInsertionServer::parse(const String &p_text) const {
	Ref<TextInsertionTemplate> parsed;
	parsed.instantiate();
	parsed->_parse(p_text);
	return parsed;
}

String TextInsertionServer::insert_template(const Ref<TextInsertionTemplate> &p_template, const Dictionary &p_args, Escape p_escape) {
	ERR_FAIL_COND_V(p_template.is_null(), String());
	_load_settings();
	return _insert(p_template.ptr(), p_args, p_escape == ESCAPE_BBCODE || (p_escape == ESCAPE_DEFAULT && escape_bbcode.is_set()), 0);
}

void TextInsertionServer::register_provider(const StringName &p_name, const Callable &p_provider) {
	ERR_FAIL_COND_MSG(!p_provider.is_valid(), vformat("TextInsertionServer: The provider for \"%s\" is not a valid Callable.", p_name));
	{
		MutexLock lock(mutex);
		providers.insert(p_name, p_provider);
	}
	_emit_values_changed();
}

void TextInsertionServer::unregister_provider(const StringName &p_name) {
	bool erased = false;
	{
		MutexLock lock(mutex);
		erased = providers.erase(p_name);
	}
	if (erased) {
		_emit_values_changed();
	}
}

bool TextInsertionServer::has_provider(const StringName &p_name) const {
	MutexLock lock(mutex);
	return providers.has(p_name);
}

void TextInsertionServer::set_value(const StringName &p_name, const Variant &p_value) {
	{
		MutexLock lock(mutex);
		values.insert(p_name, p_value);
	}
	_emit_value_changed(p_name);
}

Variant TextInsertionServer::get_value(const StringName &p_name, const Variant &p_default) const {
	MutexLock lock(mutex);
	const Variant *value = values.getptr(p_name);
	return value ? *value : p_default;
}

bool TextInsertionServer::has_value(const StringName &p_name) const {
	MutexLock lock(mutex);
	return values.has(p_name);
}

void TextInsertionServer::remove_value(const StringName &p_name) {
	bool erased = false;
	{
		MutexLock lock(mutex);
		erased = values.erase(p_name);
	}
	if (erased) {
		_emit_value_changed(p_name);
	}
}

void TextInsertionServer::notify_changed(const StringName &p_name) {
	_emit_value_changed(p_name);
}

void TextInsertionServer::_on_values_resource_changed() {
	{
		MutexLock lock(mutex);
		values_resources.sort();
	}
	_emit_values_changed();
}

void TextInsertionServer::add_values(const Ref<TextInsertionValues> &p_values) {
	ERR_FAIL_COND(p_values.is_null());
	_load_settings();
	bool added = false;
	{
		MutexLock lock(mutex);
		bool present = false;
		for (const ValuesEntry &existing : values_resources) {
			present = present || existing.values == p_values;
		}
		if (!present) {
			ValuesEntry entry;
			entry.values = p_values;
			entry.order = values_order++;
			values_resources.push_back(entry);
			values_resources.sort();
			added = true;
		}
	}
	ERR_FAIL_COND_MSG(!added, "TextInsertionServer: These values are already added.");
	p_values->connect_changed(callable_mp(this, &TextInsertionServer::_on_values_resource_changed));
	_emit_values_changed();
}

void TextInsertionServer::remove_values(const Ref<TextInsertionValues> &p_values) {
	ERR_FAIL_COND(p_values.is_null());
	bool removed = false;
	{
		MutexLock lock(mutex);
		for (uint32_t idx = 0; idx < values_resources.size(); idx++) {
			if (values_resources[idx].values == p_values) {
				values_resources.remove_at(idx);
				removed = true;
				break;
			}
		}
	}
	if (removed) {
		p_values->disconnect_changed(callable_mp(this, &TextInsertionServer::_on_values_resource_changed));
		_emit_values_changed();
	}
}

void TextInsertionServer::register_modifier(const StringName &p_name, const Callable &p_modifier) {
	ERR_FAIL_COND_MSG(TextInsertionTemplate::get_builtin_modifier(p_name) != TextInsertionTemplate::MODIFIER_CUSTOM, vformat("TextInsertionServer: \"%s\" is a built-in modifier and cannot be replaced.", p_name));
	ERR_FAIL_COND_MSG(!p_modifier.is_valid(), vformat("TextInsertionServer: The modifier \"%s\" is not a valid Callable.", p_name));
	{
		MutexLock lock(mutex);
		modifiers.insert(p_name, p_modifier);
	}
	_emit_values_changed();
}

void TextInsertionServer::unregister_modifier(const StringName &p_name) {
	bool erased = false;
	{
		MutexLock lock(mutex);
		erased = modifiers.erase(p_name);
	}
	if (erased) {
		_emit_values_changed();
	}
}

bool TextInsertionServer::has_modifier(const StringName &p_name) const {
	MutexLock lock(mutex);
	return modifiers.has(p_name);
}

void TextInsertionServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("insert", "text", "args", "escape"), &TextInsertionServer::insert, DEFVAL(Dictionary()), DEFVAL(ESCAPE_DEFAULT));
	ClassDB::bind_method(D_METHOD("parse", "text"), &TextInsertionServer::parse);
	ClassDB::bind_method(D_METHOD("insert_template", "template", "args", "escape"), &TextInsertionServer::insert_template, DEFVAL(Dictionary()), DEFVAL(ESCAPE_DEFAULT));

	ClassDB::bind_method(D_METHOD("register_provider", "name", "provider"), &TextInsertionServer::register_provider);
	ClassDB::bind_method(D_METHOD("unregister_provider", "name"), &TextInsertionServer::unregister_provider);
	ClassDB::bind_method(D_METHOD("has_provider", "name"), &TextInsertionServer::has_provider);

	ClassDB::bind_method(D_METHOD("set_value", "name", "value"), &TextInsertionServer::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "name", "default"), &TextInsertionServer::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "name"), &TextInsertionServer::has_value);
	ClassDB::bind_method(D_METHOD("remove_value", "name"), &TextInsertionServer::remove_value);
	ClassDB::bind_method(D_METHOD("notify_changed", "name"), &TextInsertionServer::notify_changed);

	ClassDB::bind_method(D_METHOD("add_values", "values"), &TextInsertionServer::add_values);
	ClassDB::bind_method(D_METHOD("remove_values", "values"), &TextInsertionServer::remove_values);

	ClassDB::bind_method(D_METHOD("register_modifier", "name", "modifier"), &TextInsertionServer::register_modifier);
	ClassDB::bind_method(D_METHOD("unregister_modifier", "name"), &TextInsertionServer::unregister_modifier);
	ClassDB::bind_method(D_METHOD("has_modifier", "name"), &TextInsertionServer::has_modifier);

	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING_NAME, "name")));
	ADD_SIGNAL(MethodInfo("values_changed"));

	BIND_ENUM_CONSTANT(ESCAPE_DEFAULT);
	BIND_ENUM_CONSTANT(ESCAPE_NONE);
	BIND_ENUM_CONSTANT(ESCAPE_BBCODE);
}

void TextInsertionServer::finish() {
	HashMap<StringName, Callable> old_providers;
	HashMap<StringName, Callable> old_modifiers;
	HashMap<StringName, Variant> old_values;
	LocalVector<ValuesEntry> old_values_resources;
	{
		MutexLock lock(mutex);
		old_providers = std::move(providers);
		old_modifiers = std::move(modifiers);
		old_values = std::move(values);
		old_values_resources = std::move(values_resources);
		cache.clear();
	}
	// Freed here, outside of the lock: releasing a script object may call back into the server.
}

TextInsertionServer::TextInsertionServer() {
	singleton = this;
	warn_missing.set();
}

TextInsertionServer::~TextInsertionServer() {
	singleton = nullptr;
}
