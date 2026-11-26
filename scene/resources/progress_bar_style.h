/**************************************************************************/
/*  progress_bar_style.h                                                  */
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

#include "core/io/resource.h"
#include "core/variant/binder_common.h"
#include "scene/gui/progress_bar.h"
#include "scene/resources/style_box.h"

// Shared look of world-space bars drawn by ProgressBarServer (#285). Changing a style (or one of its
// StyleBoxes) emits `changed`, which redraws every bar that uses it.
class ProgressBarStyle : public Resource {
	GDCLASS(ProgressBarStyle, Resource);

public:
	enum FollowMode {
		FOLLOW_MODE_INHERIT,
		FOLLOW_MODE_POSITION_ONLY,
		FOLLOW_MODE_MAX,
	};

private:
	Size2 size = Size2(32, 4);
	Vector2 offset = Vector2(0, -20);
	Ref<StyleBox> background;
	Ref<StyleBox> fill;
	ProgressBar::FillMode fill_mode = ProgressBar::FILL_BEGIN_TO_END;
	FollowMode follow_mode = FOLLOW_MODE_INHERIT;
	bool hide_when_full = false;
	bool hide_when_empty = false;

	void _set_style_box(Ref<StyleBox> &r_member, const Ref<StyleBox> &p_style_box);

protected:
	static void _bind_methods();

public:
	void set_size(const Size2 &p_size);
	Size2 get_size() const { return size; }
	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const { return offset; }
	void set_background(const Ref<StyleBox> &p_background);
	Ref<StyleBox> get_background() const { return background; }
	void set_fill(const Ref<StyleBox> &p_fill);
	Ref<StyleBox> get_fill() const { return fill; }
	void set_fill_mode(ProgressBar::FillMode p_fill_mode);
	ProgressBar::FillMode get_fill_mode() const { return fill_mode; }
	void set_follow_mode(FollowMode p_follow_mode);
	FollowMode get_follow_mode() const { return follow_mode; }
	void set_hide_when_full(bool p_hide);
	bool is_hide_when_full() const { return hide_when_full; }
	void set_hide_when_empty(bool p_hide);
	bool is_hide_when_empty() const { return hide_when_empty; }
};

VARIANT_ENUM_CAST(ProgressBarStyle::FollowMode);
