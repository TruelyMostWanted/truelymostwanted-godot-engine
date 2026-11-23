/**************************************************************************/
/*  test_seed_server.cpp                                                  */
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

TEST_FORCE_LINK(test_seed_server)

#include "core/math/seed_server.h"

namespace TestSeedServer {

// Golden values, computed outside the engine (FNV-1a 64 + SplitMix64 + PCG32).
static const int64_t DAILY_SEED = 1486713988171126959LL;
static const int64_t LOOT_SEED = -5897437094724455890LL;
static const uint32_t LOOT_FIRST[5] = { 1463604985u, 632838076u, 1232541005u, 3482282562u, 3706291731u };

TEST_CASE("[SeedServer] Stable seeds from text and names") {
	SeedServer server;
	CHECK(server.text_to_seed("Daily-2026-11-23") == DAILY_SEED);
	CHECK(server.set_seed_from_text("Daily-2026-11-23") == DAILY_SEED);
	CHECK(server.get_global_seed() == DAILY_SEED);

	server.set_global_seed(12345);
	CHECK(server.get_derived_seed("loot") == LOOT_SEED);
	CHECK(server.get_derived_seed("loot") != server.get_derived_seed("weather"));
}

TEST_CASE("[SeedServer] Same seed gives the same stream") {
	SeedServer server;
	server.set_global_seed(12345);
	Ref<RandomNumberGenerator> loot = server.get_rng("loot");
	REQUIRE(loot.is_valid());
	CHECK(server.get_rng("loot") == loot);
	for (int i = 0; i < 5; i++) {
		CHECK(loot->randi() == LOOT_FIRST[i]);
	}
	server.set_global_seed(12345);
	CHECK(loot->randi() == LOOT_FIRST[0]);

	Ref<RandomNumberGenerator> weather = server.get_rng("weather");
	server.set_global_seed(12345);
	CHECK(loot->randi() != weather->randi());

	PackedStringArray names = server.get_rng_names();
	REQUIRE(names.size() == 2);
	CHECK(names[0] == "loot");
	CHECK(names[1] == "weather");
	server.remove_rng("weather");
	CHECK_FALSE(server.has_rng("weather"));
}

TEST_CASE("[SeedServer] Save and restore") {
	SeedServer server;
	server.set_global_seed(777);
	Ref<RandomNumberGenerator> loot = server.get_rng("loot");
	for (int i = 0; i < 13; i++) {
		loot->randi();
	}
	const Dictionary state = server.get_state();
	uint32_t first[100];
	for (int i = 0; i < 100; i++) {
		first[i] = loot->randi();
	}
	server.set_global_seed(1);
	loot->randi();
	server.set_state(state);
	CHECK(server.get_global_seed() == 777);
	for (int i = 0; i < 100; i++) {
		CHECK(loot->randi() == first[i]);
	}

	server.clear();
	server.set_state(state);
	REQUIRE(server.has_rng("loot"));
	CHECK(server.get_rng("loot")->randi() == first[0]);
}

} // namespace TestSeedServer
