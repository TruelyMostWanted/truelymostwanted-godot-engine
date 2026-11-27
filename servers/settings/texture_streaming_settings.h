/**************************************************************************/
/*  texture_streaming_settings.h                                          */
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

#include "modules/modules_enabled.gen.h" // For texture_streaming.

#ifdef MODULE_TEXTURE_STREAMING_ENABLED

#include "core/io/resource.h"

// Data of the `texture_streaming` settings domain (#22): texture streaming on or off (at the next start),
// the memory budget and the best and worst mip level. Only built with the `texture_streaming` module.
// TextureStreamingSettingsServer pushes the values to the project settings and the TextureStreaming singleton.
class TextureStreamingSettings : public Resource {
	GDCLASS(TextureStreamingSettings, Resource);

	bool enabled = false;
	bool memory_budget_enabled = false;
	int memory_budget_mb = 512;
	int min_lod = 0;
	int max_lod = 3;

protected:
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;
	void set_memory_budget_enabled(bool p_enabled);
	bool is_memory_budget_enabled() const;
	void set_memory_budget_mb(int p_mb);
	int get_memory_budget_mb() const;
	void set_min_lod(int p_lod);
	int get_min_lod() const;
	void set_max_lod(int p_lod);
	int get_max_lod() const;
};

#endif // MODULE_TEXTURE_STREAMING_ENABLED
