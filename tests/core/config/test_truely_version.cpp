/**************************************************************************/
/*  test_truely_version.cpp                                               */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_truely_version)

#include "core/config/engine.h"
#include "core/truely_version.h"
#include "core/variant/dictionary.h"

namespace TestTruelyVersion {

TEST_CASE("[TruelyVersion] Version macros are consistent") {
	CHECK_EQ(String(TRUELY_VERSION_NUMBER), vformat("%d.%d.%d", TRUELY_VERSION_MAJOR, TRUELY_VERSION_MINOR, TRUELY_VERSION_PATCH));
	CHECK_EQ(TRUELY_VERSION_HEX, (TRUELY_VERSION_MAJOR << 16) | (TRUELY_VERSION_MINOR << 8) | TRUELY_VERSION_PATCH);
	const String shown = TRUELY_VERSION_PATCH == 0 ? vformat("%d.%d", TRUELY_VERSION_MAJOR, TRUELY_VERSION_MINOR) : String(TRUELY_VERSION_NUMBER);
	CHECK_EQ(String(TRUELY_VERSION_SHORT), shown);
	CHECK_EQ(String(TRUELY_VERSION_FULL_NAME), String("Truely v") + shown);
}

TEST_CASE("[TruelyVersion] Engine.get_version_info() has the Truely version") {
	const Dictionary info = Engine::get_singleton()->get_version_info();

	REQUIRE(info.has("truely_major"));
	REQUIRE(info.has("truely_minor"));
	REQUIRE(info.has("truely_patch"));
	REQUIRE(info.has("truely_hex"));
	REQUIRE(info.has("truely_version"));

	CHECK_EQ(info["truely_major"].get_type(), Variant::INT);
	CHECK_EQ(info["truely_minor"].get_type(), Variant::INT);
	CHECK_EQ(info["truely_patch"].get_type(), Variant::INT);
	CHECK_EQ(info["truely_hex"].get_type(), Variant::INT);
	CHECK_EQ(info["truely_version"].get_type(), Variant::STRING);

	CHECK_EQ(int(info["truely_major"]), TRUELY_VERSION_MAJOR);
	CHECK_EQ(int(info["truely_minor"]), TRUELY_VERSION_MINOR);
	CHECK_EQ(int(info["truely_patch"]), TRUELY_VERSION_PATCH);
	CHECK_EQ(int(info["truely_hex"]), TRUELY_VERSION_HEX);
	CHECK_EQ(String(info["truely_version"]), String(TRUELY_VERSION_NUMBER));
	CHECK_EQ(String(info["truely_version"]), vformat("%d.%d.%d", int(info["truely_major"]), int(info["truely_minor"]), int(info["truely_patch"])));
}

TEST_CASE("[TruelyVersion] Godot version keys are unchanged") {
	const Dictionary info = Engine::get_singleton()->get_version_info();

	CHECK_EQ(int(info["major"]), GODOT_VERSION_MAJOR);
	CHECK_EQ(int(info["minor"]), GODOT_VERSION_MINOR);
	CHECK_EQ(int(info["patch"]), GODOT_VERSION_PATCH);
	CHECK_EQ(int(info["hex"]), GODOT_VERSION_HEX);
	CHECK_EQ(String(info["status"]), String(GODOT_VERSION_STATUS));
	CHECK_EQ(String(info["build"]), String(GODOT_VERSION_BUILD));
}

} // namespace TestTruelyVersion
