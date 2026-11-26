/**************************************************************************/
/*  text_insertion_template.h                                             */
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

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

// A text parsed once by TextInsertionServer: literal text and placeholders as flat segments.
// Immutable after parsing, so one template can be inserted from several threads.
class TextInsertionTemplate : public RefCounted {
	GDCLASS(TextInsertionTemplate, RefCounted);

	friend class TextInsertionServer;

public:
	enum ModifierKind : uint8_t {
		MODIFIER_CUSTOM,
		MODIFIER_T,
		MODIFIER_UPPER,
		MODIFIER_LOWER,
		MODIFIER_CAPITALIZE,
		MODIFIER_NUMBER,
		MODIFIER_F,
	};

private:
	struct Modifier {
		StringName name;
		ModifierKind kind = MODIFIER_CUSTOM;
	};

	// A literal segment has an empty name. A placeholder keeps its source text, inserted when no value is found.
	struct Segment {
		String text;
		StringName name;
		uint32_t modifier_begin = 0;
		uint32_t modifier_count = 0;
	};

	String source;
	LocalVector<Segment> segments;
	LocalVector<Modifier> modifiers;
	LocalVector<StringName> names;
	int64_t literal_length = 0;

	void _add_literal(String &r_literal);
	void _parse(const String &p_text);

protected:
	static void _bind_methods();

public:
	static ModifierKind get_builtin_modifier(const StringName &p_name);

	String get_source() const { return source; }
	TypedArray<StringName> get_names() const;
	bool has_name(const StringName &p_name) const;
};
