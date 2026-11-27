/**************************************************************************/
/*  text_editor.h                                                         */
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

#include "core/io/data_validation.h"
#include "editor/script/script_editor_base.h"

class TextEditor : public TextEditorBase {
	GDCLASS(TextEditor, TextEditorBase);

	static ScriptEditorBase *create_editor(const Ref<Resource> &p_resource);

	static inline EditMenus *edit_menus = nullptr;

	// Live errors of data files: at most this many schema issues per check, all with a line are marked.
	static constexpr int MAX_ISSUES = 200;
	Ref<DataValidationOptions> validation_options;
	LocalVector<int> issue_lines; // Marked lines (0-based), sorted.

	void _caret_changed();
	bool _requires_space_indent() const;
	void _apply_space_indent();

protected:
	class EditMenusTEB : public EditMenus {
		GDCLASS(EditMenusTEB, EditMenus);

	public:
		virtual bool handles(ScriptEditorBase *p_seb) override { return Object::cast_to<TextEditor>(p_seb); }

		EditMenusTEB(DocumentEditorContainer *p_document_editor_container) : EditMenus(p_document_editor_container) {}
	};

	virtual void _validate_script() override;

public:
	virtual void apply_code() override;
	virtual void set_edited_resource(const Ref<Resource> &p_res) override;
	virtual void update_settings() override;
	virtual void convert_indent() override;

	virtual EditMenusBase *create_edit_menu(DocumentEditorContainer *p_document_editor_container) override { return memnew(EditMenusTEB(p_document_editor_container)); }

	static void register_editor();

	TextEditor();
};
