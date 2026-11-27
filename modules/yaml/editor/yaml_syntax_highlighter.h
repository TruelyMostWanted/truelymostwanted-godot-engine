/**************************************************************************/
/*  yaml_syntax_highlighter.h                                             */
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

#include "editor/script/syntax_highlighters.h"

// Script editor highlighter for YAML files: keys, strings, comments. Registered by the module, so it is
// gone when the module is disabled.
class EditorYAMLSyntaxHighlighter : public EditorKeyValueSyntaxHighlighter {
	GDCLASS(EditorYAMLSyntaxHighlighter, EditorKeyValueSyntaxHighlighter)

protected:
	virtual int _find_key(const String &p_line, int &r_start, bool &r_section) const override;

public:
	virtual void _update_cache() override;

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "yaml", "yml" }; }
	virtual String _get_name() const override { return TTR("YAML"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;
};
