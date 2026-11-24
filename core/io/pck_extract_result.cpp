/**************************************************************************/
/*  pck_extract_result.cpp                                                */
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

#include "pck_extract_result.h"

#include "core/object/class_db.h"

void PCKExtractResult::_add_error(Error p_error, const String &p_message) {
	if (error == OK) {
		error = p_error;
	}
	errors.push_back(p_message);
}

void PCKExtractResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_error"), &PCKExtractResult::get_error);
	ClassDB::bind_method(D_METHOD("is_ok"), &PCKExtractResult::is_ok);
	ClassDB::bind_method(D_METHOD("get_written"), &PCKExtractResult::get_written);
	ClassDB::bind_method(D_METHOD("get_skipped"), &PCKExtractResult::get_skipped);
	ClassDB::bind_method(D_METHOD("get_renamed"), &PCKExtractResult::get_renamed);
	ClassDB::bind_method(D_METHOD("get_not_editable"), &PCKExtractResult::get_not_editable);
	ClassDB::bind_method(D_METHOD("get_dropped_uids"), &PCKExtractResult::get_dropped_uids);
	ClassDB::bind_method(D_METHOD("get_errors"), &PCKExtractResult::get_errors);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "written"), "", "get_written");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "skipped"), "", "get_skipped");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "renamed"), "", "get_renamed");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "not_editable"), "", "get_not_editable");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "dropped_uids"), "", "get_dropped_uids");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "errors"), "", "get_errors");
}
