/**************************************************************************/
/*  resource_translation_parser_plugin.cpp                                */
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

#include "resource_translation_parser_plugin.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

void ResourceTranslationParserPlugin::get_recognized_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("tres");
	r_extensions->push_back("res");
	r_extensions->push_back("mmd"); // Mermaid flow graphs (modules/flow_graph).
	// Subtitle files (.srt, .vtt, ...): Subtitle has get_translatable_strings() (#323).
	List<String> subtitle_extensions;
	ResourceLoader::get_recognized_extensions_for_type("Subtitle", &subtitle_extensions);
	for (const String &extension : subtitle_extensions) {
		if (r_extensions->find(extension) == nullptr) {
			r_extensions->push_back(extension);
		}
	}
}

Error ResourceTranslationParserPlugin::parse_file(const String &p_path, Vector<Vector<String>> *r_translations) {
	const String extension = p_path.get_extension().to_lower();
	if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_path), "PackedScene")) {
		// Binary scenes keep going to the scene parser.
		for (const Ref<EditorTranslationParserPlugin> &parser : EditorTranslationParser::get_singleton()->standard_parsers) {
			List<String> extensions;
			parser->get_recognized_extensions(&extensions);
			if (parser.ptr() != this && extensions.find(extension) != nullptr) {
				return parser->parse_file(p_path, r_translations);
			}
		}
		return OK;
	}

	Error err = OK;
	const Ref<Resource> resource = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	ERR_FAIL_COND_V_MSG(resource.is_null(), err == OK ? ERR_CANT_OPEN : err, vformat("Failed to load '%s'.", p_path));
	if (!resource->has_method(SNAME("get_translatable_strings"))) {
		return OK;
	}
	const Array strings = resource->call(SNAME("get_translatable_strings"));
	for (int i = 0; i < strings.size(); i++) {
		const PackedStringArray entry = strings[i];
		if (!entry.is_empty() && !entry[0].is_empty()) {
			r_translations->push_back(entry);
		}
	}
	return OK;
}
