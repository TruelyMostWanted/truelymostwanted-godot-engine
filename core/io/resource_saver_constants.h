/**************************************************************************/
/*  resource_saver_constants.h                                            */
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

#include "core/variant/type_info.h"

namespace ResourceSaverConstants {

// Declared outside of `ResourceSaver`, so that `ResourceFormatSaver` (declared before it) can use the type.
enum SaverFlags {
	FLAG_NONE = 0,
	FLAG_RELATIVE_PATHS = 1,
	FLAG_BUNDLE_RESOURCES = 2,
	FLAG_CHANGE_PATH = 4,
	FLAG_OMIT_EDITOR_PROPERTIES = 8,
	FLAG_SAVE_BIG_ENDIAN = 16,
	FLAG_COMPRESS = 32,
	FLAG_REPLACE_SUBRESOURCE_PATHS = 64,
};

} // namespace ResourceSaverConstants

// Exposed as `ResourceSaver.SaverFlags`, the enum of the `ResourceSaver` singleton.
VARIANT_BITFIELD_CAST_EXT(ResourceSaverConstants::SaverFlags, ResourceSaver::SaverFlags)
