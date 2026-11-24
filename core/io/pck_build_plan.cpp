/**************************************************************************/
/*  pck_build_plan.cpp                                                    */
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

#include "pck_build_plan.h"

#include "core/object/class_db.h"

void PCKBuildPlan::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &PCKBuildPlan::is_valid);
	ClassDB::bind_method(D_METHOD("get_export_sources"), &PCKBuildPlan::get_export_sources);
	ClassDB::bind_method(D_METHOD("get_raw_files"), &PCKBuildPlan::get_raw_files);
	ClassDB::bind_method(D_METHOD("get_removals"), &PCKBuildPlan::get_removals);
	ClassDB::bind_method(D_METHOD("get_skipped"), &PCKBuildPlan::get_skipped);
	ClassDB::bind_method(D_METHOD("get_errors"), &PCKBuildPlan::get_errors);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid"), "", "is_valid");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "export_sources"), "", "get_export_sources");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "raw_files"), "", "get_raw_files");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "removals"), "", "get_removals");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "skipped"), "", "get_skipped");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "errors"), "", "get_errors");
}
