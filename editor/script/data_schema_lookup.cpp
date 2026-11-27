/**************************************************************************/
/*  data_schema_lookup.cpp                                                */
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

#include "data_schema_lookup.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"

Ref<DataSchema> EditorDataSchemaLookup::find_schema(const String &p_path) {
	if (p_path.is_empty()) {
		return Ref<DataSchema>();
	}
	const PackedStringArray schema_files = GLOBAL_GET("filesystem/data/schema_files");
	for (const String &schema_file : schema_files) {
		// Loaded schemas stay in the resource cache, so the lookup is cheap on every validation. A missing
		// file is skipped without an error (the check runs while typing).
		if (!ResourceLoader::exists(schema_file, "DataSchema")) {
			continue;
		}
		Ref<DataSchema> schema = ResourceLoader::load(schema_file, "DataSchema");
		if (schema.is_null()) {
			continue;
		}
		for (const String &pattern : schema->get_file_patterns()) {
			if (p_path.match(pattern)) {
				return schema;
			}
		}
	}
	return Ref<DataSchema>();
}
