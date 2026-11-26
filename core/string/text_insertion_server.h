/**************************************************************************/
/*  text_insertion_server.h                                               */
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
#include "core/os/mutex.h"
#include "core/string/text_insertion_template.h"
#include "core/string/text_insertion_values.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/lru.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/binder_common.h"

// Fills `{name}` placeholders in texts from call arguments, providers, values and TextInsertionValues resources.
class TextInsertionServer : public Object {
	GDCLASS(TextInsertionServer, Object);

public:
	enum Escape {
		ESCAPE_DEFAULT,
		ESCAPE_NONE,
		ESCAPE_BBCODE,
	};

	static constexpr int MAX_DEPTH = 4;
	static constexpr int CACHE_SIZE = 256;

private:
	static TextInsertionServer *singleton;

	struct ValuesEntry {
		Ref<TextInsertionValues> values;
		uint64_t order = 0;

		// Higher priority first; on equal priority the values added later win.
		bool operator<(const ValuesEntry &p_other) const {
			const int priority = values->get_priority();
			const int other_priority = p_other.values->get_priority();
			return priority != other_priority ? priority > other_priority : order > p_other.order;
		}
	};

	mutable Mutex mutex;
	HashMap<StringName, Callable> providers;
	HashMap<StringName, Callable> modifiers;
	HashMap<StringName, Variant> values;
	LocalVector<ValuesEntry> values_resources;
	uint64_t values_order = 0;
	mutable LRUCache<String, Ref<TextInsertionTemplate>> cache{ CACHE_SIZE };
	mutable HashSet<String> warned;

	SafeFlag settings_loaded;
	SafeFlag warn_missing;
	SafeFlag escape_bbcode;

	void _load_settings();
	void _sort_values_resources();
	void _on_values_resource_changed();
	void _emit_value_changed(const StringName &p_name);
	void _emit_values_changed();
	void _warn_once(const String &p_key, const String &p_message) const;

	bool _resolve(const StringName &p_name, const Dictionary &p_args, Variant &r_value) const;
	Variant _apply_modifier(const TextInsertionTemplate::Modifier &p_modifier, const Variant &p_value, const Dictionary &p_args, int p_depth) const;
	String _insert(const TextInsertionTemplate *p_template, const Dictionary &p_args, bool p_escape, int p_depth) const;
	Ref<TextInsertionTemplate> _get_template(const String &p_text) const;

protected:
	static void _bind_methods();

public:
	static TextInsertionServer *get_singleton() { return singleton; }
	static void register_settings();

	// Inserting.
	String insert(const String &p_text, const Dictionary &p_args = Dictionary(), Escape p_escape = ESCAPE_DEFAULT);
	Ref<TextInsertionTemplate> parse(const String &p_text) const;
	String insert_template(const Ref<TextInsertionTemplate> &p_template, const Dictionary &p_args = Dictionary(), Escape p_escape = ESCAPE_DEFAULT);

	// Providers.
	void register_provider(const StringName &p_name, const Callable &p_provider);
	void unregister_provider(const StringName &p_name);
	bool has_provider(const StringName &p_name) const;

	// Values.
	void set_value(const StringName &p_name, const Variant &p_value);
	Variant get_value(const StringName &p_name, const Variant &p_default = Variant()) const;
	bool has_value(const StringName &p_name) const;
	void remove_value(const StringName &p_name);
	void notify_changed(const StringName &p_name);

	void add_values(const Ref<TextInsertionValues> &p_values);
	void remove_values(const Ref<TextInsertionValues> &p_values);

	// Modifiers.
	void register_modifier(const StringName &p_name, const Callable &p_modifier);
	void unregister_modifier(const StringName &p_name);
	bool has_modifier(const StringName &p_name) const;

	TextInsertionServer();
	~TextInsertionServer();
};

VARIANT_ENUM_CAST(TextInsertionServer::Escape);
