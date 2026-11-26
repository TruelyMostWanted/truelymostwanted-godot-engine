/**************************************************************************/
/*  progress_bar_style.cpp                                                */
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

#include "progress_bar_style.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void ProgressBarStyle::_set_style_box(Ref<StyleBox> &r_member, const Ref<StyleBox> &p_style_box) {
	if (r_member == p_style_box) {
		return;
	}
	// Changes inside a StyleBox redraw the bars as well.
	if (r_member.is_valid()) {
		r_member->disconnect_changed(callable_mp((Resource *)this, &Resource::emit_changed));
	}
	r_member = p_style_box;
	if (r_member.is_valid()) {
		r_member->connect_changed(callable_mp((Resource *)this, &Resource::emit_changed));
	}
	emit_changed();
}

void ProgressBarStyle::set_size(const Size2 &p_size) {
	if (size != p_size) {
		size = p_size;
		emit_changed();
	}
}

void ProgressBarStyle::set_offset(const Vector2 &p_offset) {
	if (offset != p_offset) {
		offset = p_offset;
		emit_changed();
	}
}

void ProgressBarStyle::set_background(const Ref<StyleBox> &p_background) {
	_set_style_box(background, p_background);
}

void ProgressBarStyle::set_fill(const Ref<StyleBox> &p_fill) {
	_set_style_box(fill, p_fill);
}

void ProgressBarStyle::set_fill_mode(ProgressBar::FillMode p_fill_mode) {
	ERR_FAIL_INDEX(p_fill_mode, ProgressBar::FILL_MODE_MAX);
	if (fill_mode != p_fill_mode) {
		fill_mode = p_fill_mode;
		emit_changed();
	}
}

void ProgressBarStyle::set_follow_mode(FollowMode p_follow_mode) {
	ERR_FAIL_INDEX(p_follow_mode, FOLLOW_MODE_MAX);
	if (follow_mode != p_follow_mode) {
		follow_mode = p_follow_mode;
		emit_changed();
	}
}

void ProgressBarStyle::set_hide_when_full(bool p_hide) {
	if (hide_when_full != p_hide) {
		hide_when_full = p_hide;
		emit_changed();
	}
}

void ProgressBarStyle::set_hide_when_empty(bool p_hide) {
	if (hide_when_empty != p_hide) {
		hide_when_empty = p_hide;
		emit_changed();
	}
}

void ProgressBarStyle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &ProgressBarStyle::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &ProgressBarStyle::get_size);
	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &ProgressBarStyle::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &ProgressBarStyle::get_offset);
	ClassDB::bind_method(D_METHOD("set_background", "background"), &ProgressBarStyle::set_background);
	ClassDB::bind_method(D_METHOD("get_background"), &ProgressBarStyle::get_background);
	ClassDB::bind_method(D_METHOD("set_fill", "fill"), &ProgressBarStyle::set_fill);
	ClassDB::bind_method(D_METHOD("get_fill"), &ProgressBarStyle::get_fill);
	ClassDB::bind_method(D_METHOD("set_fill_mode", "fill_mode"), &ProgressBarStyle::set_fill_mode);
	ClassDB::bind_method(D_METHOD("get_fill_mode"), &ProgressBarStyle::get_fill_mode);
	ClassDB::bind_method(D_METHOD("set_follow_mode", "follow_mode"), &ProgressBarStyle::set_follow_mode);
	ClassDB::bind_method(D_METHOD("get_follow_mode"), &ProgressBarStyle::get_follow_mode);
	ClassDB::bind_method(D_METHOD("set_hide_when_full", "hide"), &ProgressBarStyle::set_hide_when_full);
	ClassDB::bind_method(D_METHOD("is_hide_when_full"), &ProgressBarStyle::is_hide_when_full);
	ClassDB::bind_method(D_METHOD("set_hide_when_empty", "hide"), &ProgressBarStyle::set_hide_when_empty);
	ClassDB::bind_method(D_METHOD("is_hide_when_empty"), &ProgressBarStyle::is_hide_when_empty);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "suffix:px"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "background", PROPERTY_HINT_RESOURCE_TYPE, StyleBox::get_class_static()), "set_background", "get_background");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fill", PROPERTY_HINT_RESOURCE_TYPE, StyleBox::get_class_static()), "set_fill", "get_fill");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fill_mode", PROPERTY_HINT_ENUM, "Begin to End,End to Begin,Top to Bottom,Bottom to Top"), "set_fill_mode", "get_fill_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "follow_mode", PROPERTY_HINT_ENUM, "Inherit,Position Only"), "set_follow_mode", "get_follow_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_when_full"), "set_hide_when_full", "is_hide_when_full");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_when_empty"), "set_hide_when_empty", "is_hide_when_empty");

	BIND_ENUM_CONSTANT(FOLLOW_MODE_INHERIT);
	BIND_ENUM_CONSTANT(FOLLOW_MODE_POSITION_ONLY);
	BIND_ENUM_CONSTANT(FOLLOW_MODE_MAX);
}
