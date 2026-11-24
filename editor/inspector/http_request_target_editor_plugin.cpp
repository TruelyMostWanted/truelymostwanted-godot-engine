/**************************************************************************/
/*  http_request_target_editor_plugin.cpp                                 */
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

#include "http_request_target_editor_plugin.h"

#include "core/core_string_names.h"
#include "core/object/callable_mp.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"
#include "scene/gui/text_edit.h"

static const char *_method_names[HTTPClient::METHOD_MAX] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", "CONNECT", "PATCH" };

void HTTPRequestTargetPreview::_update_preview() {
	if (target.is_null()) {
		return;
	}

	const int method = int(target->get_method());
	String preview = String(method >= 0 && method < HTTPClient::METHOD_MAX ? _method_names[method] : "?") + " " + target->build_url() + "\n";
	for (const String &header : target->build_headers()) {
		preview += header + "\n";
	}

	const PackedByteArray body = target->build_body();
	if (!body.is_empty()) {
		preview += "\n";
		if (target->get_body_type() == HTTPRequestTarget::BODY_TYPE_BINARY) {
			preview += vformat(TTR("(%d bytes of binary data)"), body.size());
		} else {
			preview += String::utf8(reinterpret_cast<const char *>(body.ptr()), body.size());
		}
	}
	text->set_text(preview);
}

void HTTPRequestTargetPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (target.is_valid() && !target->is_connected(CoreStringName(changed), callable_mp(this, &HTTPRequestTargetPreview::_update_preview))) {
				target->connect(CoreStringName(changed), callable_mp(this, &HTTPRequestTargetPreview::_update_preview));
			}
			_update_preview();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (target.is_valid() && target->is_connected(CoreStringName(changed), callable_mp(this, &HTTPRequestTargetPreview::_update_preview))) {
				target->disconnect(CoreStringName(changed), callable_mp(this, &HTTPRequestTargetPreview::_update_preview));
			}
		} break;
	}
}

HTTPRequestTargetPreview::HTTPRequestTargetPreview(const Ref<HTTPRequestTarget> &p_target) {
	target = p_target;

	title = memnew(Label);
	title->set_text(TTR("Request Preview (without placeholder values)"));
	add_child(title);

	text = memnew(TextEdit);
	text->set_editable(false);
	text->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	text->set_custom_minimum_size(Size2(0, 120 * EDSCALE));
	add_child(text);
}

bool EditorInspectorPluginHTTPRequestTarget::can_handle(Object *p_object) {
	return Object::cast_to<HTTPRequestTarget>(p_object) != nullptr;
}

void EditorInspectorPluginHTTPRequestTarget::parse_end(Object *p_object) {
	Ref<HTTPRequestTarget> target(Object::cast_to<HTTPRequestTarget>(p_object));
	if (target.is_null()) {
		return;
	}
	add_custom_control(memnew(HTTPRequestTargetPreview(target)));
}

HTTPRequestTargetEditorPlugin::HTTPRequestTargetEditorPlugin() {
	Ref<EditorInspectorPluginHTTPRequestTarget> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
