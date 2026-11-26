/**************************************************************************/
/*  truely_version.h                                                      */
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

// Version of the truely-godot-engine fork, on top of the Godot version of core/version.h.
// The number comes from truely/version.txt (SemVer) and is generated at build time into
// core/truely_version.gen.h by core/truely_version_builders.py. version.py is never changed.

#include "core/truely_version.gen.h" // IWYU pragma: export
#include "core/version.h" // IWYU pragma: export

// Name shown before the version number.
#define TRUELY_VERSION_NAME "Truely"

// Example: "0.1.0" (the patch number is always included, as SemVer requires).
#define TRUELY_VERSION_NUMBER _MKSTR(TRUELY_VERSION_MAJOR) "." _MKSTR(TRUELY_VERSION_MINOR) "." _MKSTR(TRUELY_VERSION_PATCH)

// Version number encoded as hexadecimal int with one byte for each number.
// Example: 0.1.0 will be 0x000100.
#define TRUELY_VERSION_HEX (0x10000 * TRUELY_VERSION_MAJOR + 0x100 * TRUELY_VERSION_MINOR + TRUELY_VERSION_PATCH)

// Example: "Truely v0.1.0"
#define TRUELY_VERSION_FULL_NAME TRUELY_VERSION_NAME " v" TRUELY_VERSION_NUMBER
