/**************************************************************************/
/*  test_data_file_editor.cpp                                             */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_data_file_editor)

#ifdef TOOLS_ENABLED

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/resource_saver.h"
#include "editor/script/data_schema_lookup.h"
#include "editor/script/syntax_highlighters.h"
#include "scene/gui/text_edit.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h" // For toml, yaml.

#ifdef MODULE_TOML_ENABLED
#include "modules/toml/editor/toml_syntax_highlighter.h"
#endif
#ifdef MODULE_YAML_ENABLED
#include "modules/yaml/editor/yaml_syntax_highlighter.h"
#endif

namespace TestDataFileEditor {

// Colors of one line, by column, from a highlighter attached to a TextEdit with `p_text`.
[[maybe_unused]] static Vector<Dictionary> highlight_lines(const Ref<EditorSyntaxHighlighter> &p_highlighter, const String &p_text) {
	TextEdit *text_edit = memnew(TextEdit);
	SceneTree::get_singleton()->get_root()->add_child(text_edit);
	text_edit->set_text(p_text);
	text_edit->set_syntax_highlighter(p_highlighter);
	Vector<Dictionary> lines;
	for (int i = 0; i < text_edit->get_line_count(); i++) {
		lines.push_back(p_highlighter->get_line_syntax_highlighting(i));
	}
	memdelete(text_edit);
	return lines;
}

[[maybe_unused]] static Color color_at(const Dictionary &p_colors, int p_column) {
	return Dictionary(p_colors[p_column]).get("color", Color());
}

[[maybe_unused]] static bool columns_ascending(const Dictionary &p_colors) {
	int previous = -1;
	for (const Variant &key : p_colors.keys()) {
		if (int(key) <= previous) {
			return false;
		}
		previous = key;
	}
	return true;
}

TEST_CASE("[Editor][DataFileEditor] CSV columns and a quoted cell over two lines") {
	Ref<EditorCSVSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	const Vector<Dictionary> lines = highlight_lines(highlighter, "a,b,c\n\"x\ny,1\",z\n");
	REQUIRE(lines.size() >= 3);

	// Column 0, delimiter, column 1, delimiter, column 2.
	CHECK(lines[0].keys() == Array{ 0, 1, 2, 3, 4 });
	// Line 2 continues the quoted cell of column 0 (the comma inside quotes is no delimiter), then column 1.
	CHECK(lines[2].keys() == Array{ 0, 4, 5 });
	CHECK(color_at(lines[2], 0) == color_at(lines[0], 0));
	CHECK(color_at(lines[2], 5) == color_at(lines[0], 2));
}

TEST_CASE("[Editor][DataFileEditor] XML tags, attributes and a comment over two lines") {
	Ref<EditorXMLSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	const Vector<Dictionary> lines = highlight_lines(highlighter, "<a x=\"1\"><!-- c\nd --></a>");
	REQUIRE(lines.size() == 2);

	CHECK(columns_ascending(lines[0]));
	CHECK(lines[0].has(1)); // Tag name.
	CHECK(lines[0].has(3)); // Attribute name.
	CHECK(lines[0].has(5)); // Value.
	CHECK(lines[0].has(9)); // Comment start.
	// The second line starts inside the comment; `</a>` after it has the tag color again.
	CHECK(color_at(lines[1], 0) == color_at(lines[0], 9));
	CHECK(lines[1].has(7));
	CHECK(color_at(lines[1], 7) == color_at(lines[0], 1));
}

#ifdef MODULE_TOML_ENABLED
TEST_CASE("[Editor][DataFileEditor] TOML tables and keys") {
	Ref<EditorTOMLSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	const Vector<Dictionary> lines = highlight_lines(highlighter, "[server]\nport = 80 # comment\n");
	REQUIRE(lines.size() >= 2);
	CHECK(lines[0].has(0));
	CHECK(lines[1].has(0));
	CHECK(lines[1].has(4)); // End of the key.
	CHECK(columns_ascending(lines[1]));
}
#endif // MODULE_TOML_ENABLED

#ifdef MODULE_YAML_ENABLED
TEST_CASE("[Editor][DataFileEditor] YAML keys, also in sequence items") {
	Ref<EditorYAMLSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	const Vector<Dictionary> lines = highlight_lines(highlighter, "name: Sword\n- id: 1\nplain text\n");
	REQUIRE(lines.size() >= 3);
	CHECK(lines[0].has(0));
	CHECK(lines[0].has(4)); // The colon ends the key.
	CHECK(lines[1].has(2)); // The key after `- `.
	CHECK(color_at(lines[1], 2) == color_at(lines[0], 0));
	CHECK(columns_ascending(lines[1]));
}
#endif // MODULE_YAML_ENABLED

TEST_CASE("[Editor][DataFileEditor] Schema lookup by file_patterns of the schema files setting") {
	const String root = TestUtils::get_temp_path("data_schema_lookup");
	DirAccess::make_dir_recursive_absolute(root);
	Ref<DataSchema> schema;
	schema.instantiate();
	schema->set_file_patterns(PackedStringArray{ "res://data/items_*.csv" });
	const String schema_path = root.path_join("items_schema.tres");
	REQUIRE(ResourceSaver::save(schema, schema_path) == OK);

	ProjectSettings *settings = ProjectSettings::get_singleton();
	const Variant previous = settings->get_setting("filesystem/data/schema_files", PackedStringArray());
	settings->set_setting("filesystem/data/schema_files", PackedStringArray{ root.path_join("missing.tres"), schema_path });

	Ref<DataSchema> found = EditorDataSchemaLookup::find_schema("res://data/items_weapons.csv");
	REQUIRE(found.is_valid());
	CHECK(found->get_file_patterns() == schema->get_file_patterns());
	CHECK(EditorDataSchemaLookup::find_schema("res://data/enemies.csv").is_null());
	CHECK(EditorDataSchemaLookup::find_schema("").is_null());

	settings->set_setting("filesystem/data/schema_files", previous);
}

} // namespace TestDataFileEditor

#endif // TOOLS_ENABLED
