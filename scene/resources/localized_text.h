/**************************************************************************/
/*  localized_text.h                                                      */
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
#include "core/string/text_insertion_template.h"
#include "core/templates/local_vector.h"
#include "core/variant/binder_common.h"

class Node;

// A translated text with inserted values (#345): `key` + `context` from TextLocalizationServer, `{name}` placeholders
// from `args` and TextInsertionServer, the plural form from `count_argument`. Bound to an object property through
// the metadata `localized_<property>`, it refreshes that property only when its locale channel, one of its
// placeholder values, a catalog or the resource itself changed.
// All tracked texts share one registry (static, main thread): a name index, a dirty list flushed once per frame and
// a handful of signal connections in total, none per binding.
class LocalizedText : public Resource {
	GDCLASS(LocalizedText, Resource);

public:
	enum RefreshMode {
		REFRESH_ON_CHANGE,
		REFRESH_EVERY_FRAME,
	};

	static constexpr const char *META_PREFIX = "localized_";

private:
	struct Binding {
		ObjectID target;
		StringName property;
		StringName channel; // The target's translation domain at bind time; empty = `interface`.
		String text; // Last text set.
	};

	struct Registry;
	static Registry *registry;

	StringName key;
	StringName context;
	Dictionary args;
	StringName count_argument;
	StringName channel;
	RefreshMode refresh_mode = REFRESH_ON_CHANGE;

	LocalVector<Binding> bindings;
	String text; // Of the own channel, for `text_changed`.
	Ref<TextInsertionTemplate> text_template; // Of the current raw translation.
	LocalVector<StringName> names; // Placeholders of `text_template`.
	int32_t registry_index = -1; // In Registry::texts; -1 = not tracked.
	bool dirty = false;

	StringName _get_channel(const StringName &p_binding_channel) const;
	bool _uses_channel(const StringName &p_channel) const;
	int _get_count() const;
	String _evaluate(const StringName &p_channel);
	void _bind(Object *p_target, const StringName &p_property);
	void _unbind(Object *p_target, const StringName &p_property);
	void _refresh();
	void _changed();
	void _track();
	void _untrack();
	void _mark_dirty();

	static void _ensure_registry();
	static void _queue_flush();
	static void _flush();
	static void _mark_all_dirty();
	static void _rebuild_index();
	static void _on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale);
	static void _on_value_changed(const StringName &p_name);
	static void _on_frame();
	static void _on_node_added(Node *p_node);
	static void _on_node_removed(Node *p_node);
	static void _apply_meta(Node *p_node, bool p_bind);
	static void _scan_tree(Node *p_node);
	static void _release_target(ObjectID p_target);

protected:
	static void _bind_methods();

public:
	static void finish();

	void set_key(const StringName &p_key);
	StringName get_key() const { return key; }
	void set_context(const StringName &p_context);
	StringName get_context() const { return context; }
	void set_args(const Dictionary &p_args);
	Dictionary get_args() const { return args; }
	void set_count_argument(const StringName &p_count_argument);
	StringName get_count_argument() const { return count_argument; }
	void set_channel(const StringName &p_channel);
	StringName get_channel() const { return channel; }
	void set_refresh_mode(RefreshMode p_refresh_mode);
	RefreshMode get_refresh_mode() const { return refresh_mode; }

	String get_text();

	void bind(Object *p_target, const StringName &p_property = "text");
	void unbind(Object *p_target, const StringName &p_property = "text");
	bool is_bound(Object *p_target, const StringName &p_property = "text") const;

	LocalizedText();
	~LocalizedText();
};

VARIANT_ENUM_CAST(LocalizedText::RefreshMode);
