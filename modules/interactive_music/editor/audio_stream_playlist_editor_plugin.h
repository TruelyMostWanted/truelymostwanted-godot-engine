/**************************************************************************/
/*  audio_stream_playlist_editor_plugin.h                                 */
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

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class Button;
class VBoxContainer;

// Edits `streams` of an AudioStreamPlaylist: one row per stream (resource picker, move up/down, remove)
// and an add button, like the path lists of FilesManifest (#354). Every change is one undo step.
class EditorPropertyAudioStreamList : public EditorProperty {
	GDCLASS(EditorPropertyAudioStreamList, EditorProperty);

	VBoxContainer *rows = nullptr;
	Button *add_button = nullptr;

	void _set_streams(const Array &p_streams);
	void _stream_changed(const Ref<Resource> &p_stream, int p_index);
	void _stream_selected(const Ref<Resource> &p_stream, bool p_inspect);
	void _move(int p_index, int p_offset);
	void _remove(int p_index);
	void _add();

protected:
	void _notification(int p_what);
	virtual void _set_read_only(bool p_read_only) override;

public:
	virtual void update_property() override;

	EditorPropertyAudioStreamList();
};

class EditorInspectorPluginAudioStreamPlaylist : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAudioStreamPlaylist, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

class AudioStreamPlaylistEditorPlugin : public EditorPlugin {
	GDCLASS(AudioStreamPlaylistEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "AudioStreamPlaylist"; }

	AudioStreamPlaylistEditorPlugin();
};
