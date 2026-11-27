/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "audio_stream_opus.h"
#include "audio_stream_opus_resource_format.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "resource_importer_opus.h"

#include "editor/audio/audio_stream_editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/inspector/editor_inspector.h"

// Waveform, player and format line in the Inspector, like the MP3 and Ogg Vorbis streams.
class EditorInspectorPluginAudioStreamOpus : public EditorInspectorPluginAudioStream {
	GDCLASS(EditorInspectorPluginAudioStreamOpus, EditorInspectorPluginAudioStream);

public:
	virtual bool can_handle(Object *p_object) override {
		return Object::cast_to<AudioStreamOpus>(p_object) != nullptr;
	}
};

static void _editor_init() {
	Ref<ResourceImporterOpus> opus_importer;
	opus_importer.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(opus_importer);

	Ref<EditorInspectorPluginAudioStreamOpus> plugin;
	plugin.instantiate();
	EditorInspector::add_inspector_plugin(plugin);
}
#endif

static Ref<ResourceFormatLoaderAudioStreamOpus> resource_loader_opus;

void initialize_opus_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(AudioStreamOpus);
		GDREGISTER_CLASS(AudioStreamPlaybackOpus);

		// Not at the front: ResourceFormatImporter keeps precedence for imported Opus files.
		resource_loader_opus.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_opus);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(ResourceImporterOpus);

		EditorNode::add_init_callback(_editor_init);
	}
#endif
}

void uninitialize_opus_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && resource_loader_opus.is_valid()) {
		ResourceLoader::remove_resource_format_loader(resource_loader_opus);
		resource_loader_opus.unref();
	}
}
