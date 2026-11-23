/**************************************************************************/
/*  seed_server.cpp                                                       */
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

#include "seed_server.h"

#include "core/math/math_funcs.h"
#include "core/math/random_pcg.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"

SeedServer *SeedServer::singleton = nullptr;

SeedServer *SeedServer::get_singleton() {
	return singleton;
}

// FNV-1a 64 over the UTF-8 bytes. Unlike String::hash(), this value is fixed and does not change between engine versions.
uint64_t SeedServer::_fnv1a_64(const String &p_text) {
	const CharString utf8 = p_text.utf8();
	uint64_t hash = 0xcbf29ce484222325ULL;
	for (int i = 0; i < utf8.length(); i++) {
		hash ^= (uint8_t)utf8[i];
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

// SplitMix64 finalizer: spreads every input bit over the whole result.
uint64_t SeedServer::_splitmix64(uint64_t p_value) {
	uint64_t z = p_value + 0x9e3779b97f4a7c15ULL;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

uint64_t SeedServer::_derive(uint64_t p_global_seed, const String &p_name) {
	return _splitmix64(_splitmix64(p_global_seed) ^ _fnv1a_64(p_name));
}

// Noise generators take a 32-bit seed: use the upper half of the derived seed.
int32_t SeedServer::_noise_seed(uint64_t p_derived) {
	return (int32_t)(uint32_t)(p_derived >> 32);
}

Ref<RandomNumberGenerator> SeedServer::_create_rng(uint64_t p_global_seed, const String &p_name) {
	Ref<RandomNumberGenerator> rng;
	rng.instantiate();
	rng->set_seed(_derive(p_global_seed, p_name));
	return rng;
}

bool SeedServer::_apply_noise_seed(const Ref<Resource> &p_noise, uint64_t p_global_seed, const String &p_name) {
	bool valid = false;
	p_noise->set(SNAME("seed"), _noise_seed(_derive(p_global_seed, p_name)), &valid);
	return valid;
}

PackedStringArray SeedServer::_sorted_names(LocalVector<String> &r_names) {
	r_names.sort();
	PackedStringArray result;
	for (const String &name : r_names) {
		result.push_back(name);
	}
	return result;
}

void SeedServer::_reseed_locked() {
	for (KeyValue<String, Ref<RandomNumberGenerator>> &E : rngs) {
		E.value->set_seed(_derive(global_seed, E.key));
	}
	for (KeyValue<String, Ref<Resource>> &E : noises) {
		_apply_noise_seed(E.value, global_seed, E.key);
	}
	if (seed_global_functions) {
		Math::seed(global_seed);
	}
}

void SeedServer::_emit_seed_changed(int64_t p_seed) {
	// Signals are sent on the main thread.
	if (Thread::is_main_thread()) {
		emit_signal(SNAME("seed_changed"), p_seed);
	} else {
		call_deferred(SNAME("emit_signal"), SNAME("seed_changed"), p_seed);
	}
}

void SeedServer::set_global_seed(int64_t p_seed) {
	{
		MutexLock lock(mutex);
		global_seed = (uint64_t)p_seed;
		_reseed_locked();
	}
	_emit_seed_changed(p_seed);
}

int64_t SeedServer::get_global_seed() const {
	MutexLock lock(mutex);
	return (int64_t)global_seed;
}

int64_t SeedServer::text_to_seed(const String &p_text) const {
	return (int64_t)_splitmix64(_fnv1a_64(p_text));
}

int64_t SeedServer::set_seed_from_text(const String &p_text) {
	const int64_t seed = text_to_seed(p_text);
	set_global_seed(seed);
	return seed;
}

int64_t SeedServer::randomize_seed() {
	RandomPCG pcg;
	pcg.randomize();
	const uint64_t high = pcg.rand();
	const int64_t seed = (int64_t)((high << 32) | (uint64_t)pcg.rand());
	set_global_seed(seed);
	return seed;
}

int64_t SeedServer::get_derived_seed(const String &p_name) const {
	MutexLock lock(mutex);
	return (int64_t)_derive(global_seed, p_name);
}

void SeedServer::set_seed_global_functions(bool p_enabled) {
	MutexLock lock(mutex);
	seed_global_functions = p_enabled;
	if (seed_global_functions) {
		Math::seed(global_seed);
	}
}

bool SeedServer::is_seed_global_functions() const {
	MutexLock lock(mutex);
	return seed_global_functions;
}

Ref<RandomNumberGenerator> SeedServer::get_rng(const String &p_name) {
	MutexLock lock(mutex);
	Ref<RandomNumberGenerator> *existing = rngs.getptr(p_name);
	if (existing) {
		return *existing;
	}
	Ref<RandomNumberGenerator> rng = _create_rng(global_seed, p_name);
	rngs.insert(p_name, rng);
	return rng;
}

bool SeedServer::has_rng(const String &p_name) const {
	MutexLock lock(mutex);
	return rngs.has(p_name);
}

void SeedServer::remove_rng(const String &p_name) {
	MutexLock lock(mutex);
	rngs.erase(p_name);
}

PackedStringArray SeedServer::get_rng_names() const {
	MutexLock lock(mutex);
	LocalVector<String> names;
	for (const KeyValue<String, Ref<RandomNumberGenerator>> &E : rngs) {
		names.push_back(E.key);
	}
	return _sorted_names(names);
}

Ref<Resource> SeedServer::get_noise(const String &p_name) {
	MutexLock lock(mutex);
	Ref<Resource> *existing = noises.getptr(p_name);
	if (existing) {
		return *existing;
	}
	// Looked up by name at runtime: core does not depend on the noise module.
	const StringName noise_class = SNAME("FastNoiseLite");
	ERR_FAIL_COND_V_MSG(!ClassDB::class_exists(noise_class) || !ClassDB::can_instantiate(noise_class), Ref<Resource>(), "FastNoiseLite is not available in this build (module_noise_enabled=no).");
	Ref<Resource> noise = Object::cast_to<Resource>(ClassDB::instantiate(noise_class));
	ERR_FAIL_COND_V(noise.is_null(), Ref<Resource>());
	_apply_noise_seed(noise, global_seed, p_name);
	noises.insert(p_name, noise);
	return noise;
}

void SeedServer::register_noise(const String &p_name, const Ref<Resource> &p_noise) {
	ERR_FAIL_COND_MSG(p_noise.is_null(), "The noise generator is null.");
	MutexLock lock(mutex);
	ERR_FAIL_COND_MSG(!_apply_noise_seed(p_noise, global_seed, p_name), vformat("%s has no integer property \"seed\".", p_noise->get_class()));
	noises.insert(p_name, p_noise);
}

bool SeedServer::has_noise(const String &p_name) const {
	MutexLock lock(mutex);
	return noises.has(p_name);
}

void SeedServer::remove_noise(const String &p_name) {
	MutexLock lock(mutex);
	noises.erase(p_name);
}

PackedStringArray SeedServer::get_noise_names() const {
	MutexLock lock(mutex);
	LocalVector<String> names;
	for (const KeyValue<String, Ref<Resource>> &E : noises) {
		names.push_back(E.key);
	}
	return _sorted_names(names);
}

Dictionary SeedServer::get_state() const {
	MutexLock lock(mutex);
	Dictionary streams;
	for (const KeyValue<String, Ref<RandomNumberGenerator>> &E : rngs) {
		streams[E.key] = (int64_t)E.value->get_state();
	}
	Dictionary state;
	state["global_seed"] = (int64_t)global_seed;
	state["streams"] = streams;
	return state;
}

void SeedServer::set_state(const Dictionary &p_state) {
	ERR_FAIL_COND_MSG(!p_state.has("global_seed") || p_state["global_seed"].get_type() != Variant::INT, "The state has no integer \"global_seed\".");
	const int64_t seed = p_state["global_seed"];
	{
		MutexLock lock(mutex);
		global_seed = (uint64_t)seed;
		_reseed_locked();
		const Dictionary streams = p_state.get("streams", Dictionary());
		for (const KeyValue<Variant, Variant> &E : streams) {
			ERR_CONTINUE_MSG(E.value.get_type() != Variant::INT, vformat("The state of stream \"%s\" is not an integer.", E.key));
			const String name = E.key;
			Ref<RandomNumberGenerator> *existing = rngs.getptr(name);
			Ref<RandomNumberGenerator> rng = existing ? *existing : _create_rng(global_seed, name);
			if (!existing) {
				rngs.insert(name, rng);
			}
			rng->set_state((uint64_t)(int64_t)E.value);
		}
	}
	_emit_seed_changed(seed);
}

void SeedServer::clear() {
	MutexLock lock(mutex);
	rngs.clear();
	noises.clear();
}

void SeedServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_global_seed", "seed"), &SeedServer::set_global_seed);
	ClassDB::bind_method(D_METHOD("get_global_seed"), &SeedServer::get_global_seed);
	ClassDB::bind_method(D_METHOD("text_to_seed", "text"), &SeedServer::text_to_seed);
	ClassDB::bind_method(D_METHOD("set_seed_from_text", "text"), &SeedServer::set_seed_from_text);
	ClassDB::bind_method(D_METHOD("randomize_seed"), &SeedServer::randomize_seed);
	ClassDB::bind_method(D_METHOD("get_derived_seed", "name"), &SeedServer::get_derived_seed);
	ClassDB::bind_method(D_METHOD("set_seed_global_functions", "enabled"), &SeedServer::set_seed_global_functions);
	ClassDB::bind_method(D_METHOD("is_seed_global_functions"), &SeedServer::is_seed_global_functions);

	ClassDB::bind_method(D_METHOD("get_rng", "name"), &SeedServer::get_rng);
	ClassDB::bind_method(D_METHOD("has_rng", "name"), &SeedServer::has_rng);
	ClassDB::bind_method(D_METHOD("remove_rng", "name"), &SeedServer::remove_rng);
	ClassDB::bind_method(D_METHOD("get_rng_names"), &SeedServer::get_rng_names);

	ClassDB::bind_method(D_METHOD("get_noise", "name"), &SeedServer::get_noise);
	ClassDB::bind_method(D_METHOD("register_noise", "name", "noise"), &SeedServer::register_noise);
	ClassDB::bind_method(D_METHOD("has_noise", "name"), &SeedServer::has_noise);
	ClassDB::bind_method(D_METHOD("remove_noise", "name"), &SeedServer::remove_noise);
	ClassDB::bind_method(D_METHOD("get_noise_names"), &SeedServer::get_noise_names);

	ClassDB::bind_method(D_METHOD("get_state"), &SeedServer::get_state);
	ClassDB::bind_method(D_METHOD("set_state", "state"), &SeedServer::set_state);
	ClassDB::bind_method(D_METHOD("clear"), &SeedServer::clear);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "global_seed"), "set_global_seed", "get_global_seed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seed_global_functions"), "set_seed_global_functions", "is_seed_global_functions");

	ADD_SIGNAL(MethodInfo("seed_changed", PropertyInfo(Variant::INT, "global_seed")));
}

SeedServer::SeedServer() {
	// The global seed starts at 0, so a run is reproducible until randomize_seed() or a fixed seed is set.
	singleton = this;
}

SeedServer::~SeedServer() {
	rngs.clear();
	noises.clear();
	singleton = nullptr;
}
