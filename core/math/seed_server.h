/**************************************************************************/
/*  seed_server.h                                                         */
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
#include "core/math/random_number_generator.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

// Holds one global seed and derives named random streams and noise generators from it.
// The derivation uses fixed hashes (FNV-1a 64 and SplitMix64) on the UTF-8 bytes of the names,
// so a seed gives the same numbers on every platform and in every engine version.
class SeedServer : public Object {
	GDCLASS(SeedServer, Object);

	static SeedServer *singleton;

	mutable Mutex mutex;
	uint64_t global_seed = 0;
	bool seed_global_functions = false;
	HashMap<String, Ref<RandomNumberGenerator>> rngs;
	HashMap<String, Ref<Resource>> noises;

	static uint64_t _fnv1a_64(const String &p_text);
	static uint64_t _splitmix64(uint64_t p_value);
	static uint64_t _derive(uint64_t p_global_seed, const String &p_name);
	static int32_t _noise_seed(uint64_t p_derived);
	static Ref<RandomNumberGenerator> _create_rng(uint64_t p_global_seed, const String &p_name);
	static bool _apply_noise_seed(const Ref<Resource> &p_noise, uint64_t p_global_seed, const String &p_name);
	static PackedStringArray _sorted_names(const LocalVector<String> &p_names);

	void _reseed_locked();
	void _emit_seed_changed(int64_t p_seed);

protected:
	static void _bind_methods();

public:
	static SeedServer *get_singleton();

	// Seeds.
	void set_global_seed(int64_t p_seed);
	int64_t get_global_seed() const;
	int64_t text_to_seed(const String &p_text) const;
	int64_t set_seed_from_text(const String &p_text);
	int64_t randomize_seed();
	int64_t get_derived_seed(const String &p_name) const;

	void set_seed_global_functions(bool p_enabled);
	bool is_seed_global_functions() const;

	// Random streams.
	Ref<RandomNumberGenerator> get_rng(const String &p_name);
	bool has_rng(const String &p_name) const;
	void remove_rng(const String &p_name);
	PackedStringArray get_rng_names() const;

	// Noise generators.
	Ref<Resource> get_noise(const String &p_name);
	void register_noise(const String &p_name, const Ref<Resource> &p_noise);
	bool has_noise(const String &p_name) const;
	void remove_noise(const String &p_name);
	PackedStringArray get_noise_names() const;

	// Save and restore.
	Dictionary get_state() const;
	void set_state(const Dictionary &p_state);

	void clear();

	SeedServer();
	~SeedServer();
};
