/**************************************************************************/
/*  localized_text.cpp                                                    */
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

#include "localized_text.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/string/text_insertion_server.h"
#include "core/string/text_localization_server.h"
#include "core/string/translation_domain.h"
#include "core/string/translation_server.h"
#include "core/templates/hash_map.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

// Shared by all tracked texts. Used on the main thread; `mutex` guards `texts` and `index_dirty` against texts freed
// on other threads.
struct LocalizedText::Registry {
	struct Target {
		uint32_t bindings = 0;
		Node::AutoTranslateMode auto_translate_mode = Node::AUTO_TRANSLATE_MODE_INHERIT; // Before the first binding.
	};

	BinaryMutex mutex;
	LocalVector<LocalizedText *> texts; // Tracked: bound or read with get_text().
	HashMap<StringName, LocalVector<ObjectID>> index; // Placeholder or count argument -> texts using it.
	bool index_dirty = false;
	LocalVector<ObjectID> dirty_texts;
	bool flush_queued = false;
	uint32_t every_frame_count = 0;
	HashMap<ObjectID, Target> targets; // Bound nodes.
	bool servers_connected = false;
	bool translation_server_connected = false;
	ObjectID tree;
};

LocalizedText::Registry *LocalizedText::registry = nullptr;
static bool localized_text_finished = false;

void LocalizedText::_ensure_registry() {
	if (localized_text_finished) {
		return;
	}
	if (!Thread::is_main_thread()) {
		MessageQueue::get_main_singleton()->push_callable(callable_mp_static(&LocalizedText::_ensure_registry));
		return;
	}
	if (!registry) {
		registry = memnew(Registry);
	}
	TextInsertionServer *insertion = TextInsertionServer::get_singleton();
	TextLocalizationServer *localization = TextLocalizationServer::get_singleton();
	if (!registry->servers_connected && insertion && localization) {
		registry->servers_connected = true;
		insertion->connect(SNAME("value_changed"), callable_mp_static(&LocalizedText::_on_value_changed));
		insertion->connect(SNAME("values_changed"), callable_mp_static(&LocalizedText::_mark_all_dirty));
		localization->connect(SNAME("catalogs_changed"), callable_mp_static(&LocalizedText::_mark_all_dirty));
	}
	TranslationServer *translation_server = TranslationServer::get_singleton();
	if (!registry->translation_server_connected && translation_server) {
		registry->translation_server_connected = true;
		translation_server->connect(SNAME("locale_changed"), callable_mp_static(&LocalizedText::_on_locale_changed));
		translation_server->connect(SNAME("fallback_locale_changed"), callable_mp_static(&LocalizedText::_mark_all_dirty).unbind(1));
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree && tree->get_instance_id() != registry->tree) {
		registry->tree = tree->get_instance_id();
		tree->connect(SNAME("process_frame"), callable_mp_static(&LocalizedText::_on_frame));
		// Metadata bindings apply at run time only: the editor would save the translated text into the scene.
		if (!Engine::get_singleton()->is_editor_hint()) {
			tree->connect(SNAME("node_added"), callable_mp_static(&LocalizedText::_on_node_added));
			tree->connect(SNAME("node_removed"), callable_mp_static(&LocalizedText::_on_node_removed));
			if (tree->get_root()) {
				_scan_tree(tree->get_root());
			}
		}
	}
}

void LocalizedText::_queue_flush() {
	if (registry && !registry->flush_queued) {
		registry->flush_queued = true;
		callable_mp_static(&LocalizedText::_flush).call_deferred();
	}
}

void LocalizedText::_flush() {
	if (!registry) {
		return;
	}
	registry->flush_queued = false;
	const LocalVector<ObjectID> ids = std::move(registry->dirty_texts);
	for (const ObjectID &id : ids) {
		LocalizedText *localized = ObjectDB::get_instance<LocalizedText>(id);
		if (localized && localized->dirty) {
			localized->_refresh();
		}
	}
}

void LocalizedText::_mark_all_dirty() {
	if (!registry) {
		return;
	}
	{
		MutexLock lock(registry->mutex);
		for (LocalizedText *localized : registry->texts) {
			localized->_mark_dirty();
		}
	}
	_queue_flush();
}

void LocalizedText::_rebuild_index() {
	MutexLock lock(registry->mutex);
	registry->index_dirty = false;
	registry->index.clear();
	for (LocalizedText *localized : registry->texts) {
		const ObjectID id = localized->get_instance_id();
		for (const StringName &name : localized->names) {
			registry->index[name].push_back(id);
		}
		if (localized->count_argument != StringName() && !localized->names.has(localized->count_argument)) {
			registry->index[localized->count_argument].push_back(id);
		}
	}
}

void LocalizedText::_on_locale_changed(const StringName &p_channel, const String &p_locale, const String &p_previous_locale) {
	if (!registry) {
		return;
	}
	{
		// The general locale (empty channel) reaches every channel that has no own locale.
		MutexLock lock(registry->mutex);
		for (LocalizedText *localized : registry->texts) {
			if (p_channel == StringName() || localized->_uses_channel(p_channel)) {
				localized->_mark_dirty();
			}
		}
	}
	// Deferred: the localization servers rebuild their columns on the same signal.
	_queue_flush();
}

void LocalizedText::_on_value_changed(const StringName &p_name) {
	if (!registry) {
		return;
	}
	if (registry->index_dirty) {
		_rebuild_index();
	}
	const LocalVector<ObjectID> *ids = registry->index.getptr(p_name);
	if (!ids) {
		return;
	}
	for (const ObjectID &id : *ids) {
		LocalizedText *localized = ObjectDB::get_instance<LocalizedText>(id);
		if (localized) {
			localized->_mark_dirty();
		}
	}
	_queue_flush();
}

void LocalizedText::_on_frame() {
	if (!registry || registry->every_frame_count == 0) {
		return;
	}
	LocalVector<ObjectID> ids;
	{
		MutexLock lock(registry->mutex);
		for (LocalizedText *localized : registry->texts) {
			if (localized->refresh_mode == REFRESH_EVERY_FRAME) {
				ids.push_back(localized->get_instance_id());
			}
		}
	}
	for (const ObjectID &id : ids) {
		LocalizedText *localized = ObjectDB::get_instance<LocalizedText>(id);
		if (localized) {
			localized->_refresh();
		}
	}
}

void LocalizedText::_apply_meta(Node *p_node, bool p_bind) {
	// One fixed key: a single hash lookup, no allocation, for the many nodes without a binding.
	if (!p_node->has_meta(SNAME("localized_text"))) {
		return;
	}
	const auto apply = [p_node, p_bind](const Ref<LocalizedText> &p_localized, const StringName &p_property) {
		if (p_localized.is_null()) {
			return;
		}
		if (p_bind) {
			p_localized->_bind(p_node, p_property);
		} else {
			p_localized->_unbind(p_node, p_property);
		}
	};
	const Variant value = p_node->get_meta(SNAME("localized_text"));
	if (value.get_type() != Variant::DICTIONARY) {
		apply(value, SNAME("text"));
		return;
	}
	const Dictionary properties = value;
	const Array keys = properties.keys();
	for (int idx = 0; idx < keys.size(); idx++) {
		const StringName property = keys[idx];
		apply(properties[keys[idx]], property);
	}
}

void LocalizedText::_on_node_added(Node *p_node) {
	_apply_meta(p_node, true);
}

void LocalizedText::_on_node_removed(Node *p_node) {
	_apply_meta(p_node, false);
}

void LocalizedText::_scan_tree(Node *p_node) {
	_apply_meta(p_node, true);
	for (int idx = 0; idx < p_node->get_child_count(); idx++) {
		_scan_tree(p_node->get_child(idx));
	}
}

void LocalizedText::_release_target(ObjectID p_target) {
	Registry::Target *target = registry ? registry->targets.getptr(p_target) : nullptr;
	if (!target || --target->bindings > 0) {
		return;
	}
	Node *node = ObjectDB::get_instance<Node>(p_target);
	if (node) {
		node->set_auto_translate_mode(target->auto_translate_mode);
	}
	registry->targets.erase(p_target);
}

void LocalizedText::_track() {
	if (registry_index >= 0 || !Thread::is_main_thread()) {
		return;
	}
	_ensure_registry();
	if (!registry) {
		return;
	}
	MutexLock lock(registry->mutex);
	registry_index = (int32_t)registry->texts.size();
	registry->texts.push_back(this);
	registry->index_dirty = true;
	if (refresh_mode == REFRESH_EVERY_FRAME) {
		registry->every_frame_count++;
	}
}

void LocalizedText::_untrack() {
	if (registry_index < 0 || !registry) {
		return;
	}
	MutexLock lock(registry->mutex);
	LocalizedText *last = registry->texts[registry->texts.size() - 1];
	registry->texts[registry_index] = last;
	last->registry_index = registry_index;
	registry->texts.resize(registry->texts.size() - 1);
	registry->index_dirty = true;
	if (refresh_mode == REFRESH_EVERY_FRAME) {
		registry->every_frame_count--;
	}
	registry_index = -1;
}

void LocalizedText::_mark_dirty() {
	if (dirty || registry_index < 0 || !registry) {
		return;
	}
	dirty = true;
	registry->dirty_texts.push_back(get_instance_id());
}

StringName LocalizedText::_get_channel(const StringName &p_binding_channel) const {
	if (channel != StringName()) {
		return channel;
	}
	return p_binding_channel != StringName() ? p_binding_channel : SNAME("interface");
}

bool LocalizedText::_uses_channel(const StringName &p_channel) const {
	if (_get_channel(StringName()) == p_channel) {
		return true;
	}
	for (const Binding &binding : bindings) {
		if (_get_channel(binding.channel) == p_channel) {
			return true;
		}
	}
	return false;
}

int LocalizedText::_get_count() const {
	if (count_argument == StringName()) {
		return -1;
	}
	// The arguments first (String and StringName keys are the same key), then the server's values.
	Variant value = args.get(count_argument, Variant());
	if (value.get_type() == Variant::NIL && TextInsertionServer::get_singleton()) {
		value = TextInsertionServer::get_singleton()->get_value(count_argument);
	}
	return value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT ? (int)(int64_t)value : -1;
}

String LocalizedText::_evaluate(const StringName &p_channel) {
	if (key == StringName()) {
		return String();
	}
	const int count = _get_count();
	const TextLocalizationServer *localization = TextLocalizationServer::get_singleton();
	TranslationServer *translation_server = TranslationServer::get_singleton();
	const bool is_interface = p_channel == SNAME("interface");
	String raw = key;
	if (localization && localization->has_translation(key, context)) {
		raw = is_interface || !translation_server ? localization->translate(key, context, count) : localization->translate_for_locale(translation_server->get_locale_for(p_channel), key, context, count);
	} else if (translation_server) {
		// Keys of Godot's Translation resources, through the domain of the channel.
		const Ref<TranslationDomain> domain = is_interface ? translation_server->get_main_domain() : translation_server->get_or_add_domain(p_channel);
		raw = count >= 0 ? String(domain->translate_plural(key, key, count, context)) : String(domain->translate(key, context));
	}

	TextInsertionServer *insertion = TextInsertionServer::get_singleton();
	if (!Thread::is_main_thread()) {
		return insertion->insert(raw, args);
	}
	// The template is parsed again only when the translation changed (locale, catalog, key).
	if (text_template.is_null() || text_template->get_source() != raw) {
		text_template = insertion->parse(raw);
		names.clear();
		const TypedArray<StringName> template_names = text_template->get_names();
		for (int idx = 0; idx < template_names.size(); idx++) {
			const Variant name_value = template_names[idx];
			names.push_back(name_value.operator StringName());
		}
		if (registry_index >= 0 && registry) {
			MutexLock lock(registry->mutex);
			registry->index_dirty = true;
		}
	}
	return insertion->insert_template(text_template, args);
}

void LocalizedText::_refresh() {
	const Ref<LocalizedText> keep(this); // A property setter may free the last owner.
	dirty = false;
	StringName last_channel;
	String last_text;
	bool evaluated = false;
	for (uint32_t idx = 0; idx < bindings.size();) {
		Object *target = ObjectDB::get_instance(bindings[idx].target);
		if (!target) {
			_release_target(bindings[idx].target);
			bindings.remove_at_unordered(idx);
			continue;
		}
		const StringName binding_channel = _get_channel(bindings[idx].channel);
		if (!evaluated || binding_channel != last_channel) {
			last_text = _evaluate(binding_channel);
			last_channel = binding_channel;
			evaluated = true;
		}
		// Only changed texts are set: an unchanged label is not shaped again.
		if (bindings[idx].text != last_text) {
			bindings[idx].text = last_text;
			const StringName property = bindings[idx].property;
			target->set(property, last_text);
		}
		idx++;
	}
	if (has_connections(SNAME("text_changed"))) {
		const StringName own_channel = _get_channel(StringName());
		const String own_text = evaluated && own_channel == last_channel ? last_text : _evaluate(own_channel);
		if (own_text != text) {
			text = own_text;
			emit_signal(SNAME("text_changed"), text);
		}
	}
}

void LocalizedText::_changed() {
	emit_changed();
	if (registry_index >= 0 && Thread::is_main_thread()) {
		_mark_dirty();
		_queue_flush();
	}
}

void LocalizedText::_bind(Object *p_target, const StringName &p_property) {
	const ObjectID id = p_target->get_instance_id();
	for (const Binding &binding : bindings) {
		if (binding.target == id && binding.property == p_property) {
			return;
		}
	}
	_track();
	if (!registry) {
		return;
	}
	// The bound text is translated already: the node must not translate it again.
	Node *node = Object::cast_to<Node>(p_target);
	if (node) {
		Registry::Target *target = registry->targets.getptr(id);
		if (!target) {
			target = &registry->targets.insert(id, Registry::Target())->value;
			target->auto_translate_mode = node->get_auto_translate_mode();
			node->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
		}
		target->bindings++;
	}
	Binding binding;
	binding.target = id;
	binding.property = p_property;
	binding.channel = p_target->get_translation_domain();
	bindings.push_back(binding);
	_refresh();
}

void LocalizedText::_unbind(Object *p_target, const StringName &p_property) {
	const ObjectID id = p_target->get_instance_id();
	for (uint32_t idx = 0; idx < bindings.size(); idx++) {
		if (bindings[idx].target == id && bindings[idx].property == p_property) {
			bindings.remove_at_unordered(idx);
			_release_target(id);
			return;
		}
	}
}

void LocalizedText::set_key(const StringName &p_key) {
	key = p_key;
	_changed();
}

void LocalizedText::set_context(const StringName &p_context) {
	context = p_context;
	_changed();
}

void LocalizedText::set_args(const Dictionary &p_args) {
	args = p_args;
	_changed();
}

void LocalizedText::set_count_argument(const StringName &p_count_argument) {
	count_argument = p_count_argument;
	if (registry_index >= 0 && registry) {
		MutexLock lock(registry->mutex);
		registry->index_dirty = true;
	}
	_changed();
}

void LocalizedText::set_channel(const StringName &p_channel) {
	channel = p_channel;
	_changed();
}

void LocalizedText::set_refresh_mode(RefreshMode p_refresh_mode) {
	if (refresh_mode == p_refresh_mode) {
		return;
	}
	if (registry_index >= 0 && registry) {
		MutexLock lock(registry->mutex);
		if (p_refresh_mode == REFRESH_EVERY_FRAME) {
			registry->every_frame_count++;
		} else {
			registry->every_frame_count--;
		}
	}
	refresh_mode = p_refresh_mode;
	_changed();
}

String LocalizedText::get_text() {
	_track();
	const String result = _evaluate(_get_channel(StringName()));
	if (Thread::is_main_thread()) {
		text = result;
	}
	return result;
}

void LocalizedText::bind(Object *p_target, const StringName &p_property_or_empty) {
	// Empty means "text": C# passes the default of a StringName parameter as empty.
	const StringName p_property = p_property_or_empty == StringName() ? SNAME("text") : p_property_or_empty;
	ERR_FAIL_NULL(p_target);
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "LocalizedText: bind() must be called on the main thread.");
	// The binding is the metadata `localized_text`: this text (binds `text`) or a dictionary property -> text.
	const StringName meta_name = SNAME("localized_text");
	const Variant current = p_target->has_meta(meta_name) ? p_target->get_meta(meta_name) : Variant();
	const bool is_dictionary = current.get_type() == Variant::DICTIONARY;
	Dictionary properties;
	if (is_dictionary) {
		const Dictionary existing = current;
		properties = existing.duplicate();
	} else if (current.get_type() != Variant::NIL) {
		properties[SNAME("text")] = current;
	}
	const Ref<LocalizedText> previous = properties.get(p_property, Variant());
	if (previous.is_valid() && previous.ptr() != this) {
		previous->_unbind(p_target, p_property);
	}
	if (!is_dictionary && p_property == SNAME("text")) {
		p_target->set_meta(meta_name, Variant(this));
	} else {
		properties[p_property] = Variant(this);
		p_target->set_meta(meta_name, properties);
	}
	_bind(p_target, p_property);
}

void LocalizedText::unbind(Object *p_target, const StringName &p_property_or_empty) {
	// Empty means "text": C# passes the default of a StringName parameter as empty.
	const StringName p_property = p_property_or_empty == StringName() ? SNAME("text") : p_property_or_empty;
	ERR_FAIL_NULL(p_target);
	const Ref<LocalizedText> keep(this); // The metadata may hold the last reference.
	const StringName meta_name = SNAME("localized_text");
	if (p_target->has_meta(meta_name)) {
		const Variant current = p_target->get_meta(meta_name);
		if (current.get_type() == Variant::DICTIONARY) {
			const Dictionary existing = current;
			Dictionary properties = existing.duplicate();
			if (properties.get(p_property, Variant()) == Variant(this)) {
				properties.erase(p_property);
				if (properties.is_empty()) {
					p_target->remove_meta(meta_name);
				} else {
					p_target->set_meta(meta_name, properties);
				}
			}
		} else if (p_property == SNAME("text") && current == Variant(this)) {
			p_target->remove_meta(meta_name);
		}
	}
	_unbind(p_target, p_property);
}

bool LocalizedText::is_bound(Object *p_target, const StringName &p_property_or_empty) const {
	// Empty means "text": C# passes the default of a StringName parameter as empty.
	const StringName p_property = p_property_or_empty == StringName() ? SNAME("text") : p_property_or_empty;
	ERR_FAIL_NULL_V(p_target, false);
	const ObjectID id = p_target->get_instance_id();
	for (const Binding &binding : bindings) {
		if (binding.target == id && binding.property == p_property) {
			return true;
		}
	}
	return false;
}

void LocalizedText::finish() {
	localized_text_finished = true;
	if (registry) {
		memdelete(registry);
		registry = nullptr;
	}
}

void LocalizedText::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_key", "key"), &LocalizedText::set_key);
	ClassDB::bind_method(D_METHOD("get_key"), &LocalizedText::get_key);
	ClassDB::bind_method(D_METHOD("set_context", "context"), &LocalizedText::set_context);
	ClassDB::bind_method(D_METHOD("get_context"), &LocalizedText::get_context);
	ClassDB::bind_method(D_METHOD("set_args", "args"), &LocalizedText::set_args);
	ClassDB::bind_method(D_METHOD("get_args"), &LocalizedText::get_args);
	ClassDB::bind_method(D_METHOD("set_count_argument", "count_argument"), &LocalizedText::set_count_argument);
	ClassDB::bind_method(D_METHOD("get_count_argument"), &LocalizedText::get_count_argument);
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &LocalizedText::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &LocalizedText::get_channel);
	ClassDB::bind_method(D_METHOD("set_refresh_mode", "refresh_mode"), &LocalizedText::set_refresh_mode);
	ClassDB::bind_method(D_METHOD("get_refresh_mode"), &LocalizedText::get_refresh_mode);

	ClassDB::bind_method(D_METHOD("get_text"), &LocalizedText::get_text);
	ClassDB::bind_method(D_METHOD("bind", "target", "property"), &LocalizedText::bind, DEFVAL(StringName("text")));
	ClassDB::bind_method(D_METHOD("unbind", "target", "property"), &LocalizedText::unbind, DEFVAL(StringName("text")));
	ClassDB::bind_method(D_METHOD("is_bound", "target", "property"), &LocalizedText::is_bound, DEFVAL(StringName("text")));

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "key"), "set_key", "get_key");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "context"), "set_context", "get_context");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "args"), "set_args", "get_args");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "count_argument"), "set_count_argument", "get_count_argument");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "channel", PROPERTY_HINT_ENUM_SUGGESTION, "interface,audio,subtitles"), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "refresh_mode", PROPERTY_HINT_ENUM, "On Change,Every Frame"), "set_refresh_mode", "get_refresh_mode");

	ADD_SIGNAL(MethodInfo("text_changed", PropertyInfo(Variant::STRING, "text")));

	BIND_ENUM_CONSTANT(REFRESH_ON_CHANGE);
	BIND_ENUM_CONSTANT(REFRESH_EVERY_FRAME);
}

LocalizedText::LocalizedText() {
	// Connects the shared registry, so metadata bindings of scenes loaded from now on are found.
	_ensure_registry();
}

LocalizedText::~LocalizedText() {
	_untrack();
}
