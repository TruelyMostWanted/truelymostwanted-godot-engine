/**************************************************************************/
/*  pck_extract_result.h                                                  */
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
#include "core/variant/dictionary.h"

// The result of `PckPackageServer::extract()`. Read-only after creation.
class PckExtractResult : public RefCounted {
	GDCLASS(PckExtractResult, RefCounted);

	friend class PckPackageServer;

	Error error = OK;
	// Target paths of the files that were written, in pack order.
	PackedStringArray written;
	// Pack paths (`res://…`) of the files that were not written.
	PackedStringArray skipped;
	// Pack path → target path, for files that were written under a new name.
	Dictionary renamed;
	// Pack paths of converted or remapped files that can't be edited as sources.
	PackedStringArray not_editable;
	// UIDs (`uid://…`) that were already used by other files and were dropped.
	PackedStringArray dropped_uids;
	PackedStringArray errors;

	void _add_error(Error p_error, const String &p_message);

protected:
	static void _bind_methods();

public:
	Error get_error() const { return error; }
	bool is_ok() const { return error == OK; }
	PackedStringArray get_written() const { return written; }
	PackedStringArray get_skipped() const { return skipped; }
	Dictionary get_renamed() const { return renamed.duplicate(); }
	PackedStringArray get_not_editable() const { return not_editable; }
	PackedStringArray get_dropped_uids() const { return dropped_uids; }
	PackedStringArray get_errors() const { return errors; }
};
