/**************************************************************************/
/*  test_tts_server.cpp                                                   */
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

TEST_FORCE_LINK(test_tts_server)

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "servers/display/display_server.h"
#include "servers/tts/tts_persona.h"
#include "servers/tts/tts_server.h"
#include "tests/signal_watcher.h"
#include "tests/test_utils.h"

namespace TestTtsServer {

static Ref<TtsPersona> make_persona(const String &p_name, const String &p_voice, const String &p_language) {
	Ref<TtsPersona> persona;
	persona.instantiate();
	persona->set_persona_name(p_name);
	persona->set_voice(p_voice);
	persona->set_language(p_language);
	return persona;
}

TEST_CASE("[TtsServer] Singleton is registered") {
	TtsServer *server = TtsServer::get_singleton();
	REQUIRE(server != nullptr);
	CHECK(Engine::get_singleton()->has_singleton("TtsServer"));
}

TEST_CASE("[TtsServer] TtsPersona defaults and value ranges") {
	Ref<TtsPersona> persona;
	persona.instantiate();
	CHECK(persona->get_persona_name().is_empty());
	CHECK(persona->get_voice().is_empty());
	CHECK(persona->get_language().is_empty());
	CHECK(persona->get_volume() == 50);
	CHECK(persona->get_pitch() == doctest::Approx(1.0));
	CHECK(persona->get_rate() == doctest::Approx(1.0));
	CHECK_FALSE(persona->is_interrupt());

	persona->set_volume(250);
	CHECK(persona->get_volume() == 100);
	persona->set_volume(-5);
	CHECK(persona->get_volume() == 0);
	persona->set_pitch(5.0f);
	CHECK(persona->get_pitch() == doctest::Approx(2.0));
	persona->set_pitch(-1.0f);
	CHECK(persona->get_pitch() == doctest::Approx(0.0));
	persona->set_rate(0.0f);
	CHECK(persona->get_rate() == doctest::Approx(0.1));
	persona->set_rate(50.0f);
	CHECK(persona->get_rate() == doctest::Approx(10.0));
}

TEST_CASE("[TtsServer] TtsPersona saves and loads as .tres") {
	Ref<TtsPersona> persona = make_persona("narrator", "Some Voice", "en_US");
	persona->set_volume(80);
	persona->set_pitch(1.25f);
	persona->set_rate(0.75f);
	persona->set_interrupt(true);

	const String path = TestUtils::get_temp_path("tts_persona.tres");
	REQUIRE(ResourceSaver::save(persona, path) == OK);

	Ref<TtsPersona> loaded = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_IGNORE);
	REQUIRE(loaded.is_valid());
	CHECK(loaded != persona);
	CHECK(loaded->get_persona_name() == "narrator");
	CHECK(loaded->get_voice() == "Some Voice");
	CHECK(loaded->get_language() == "en_US");
	CHECK(loaded->get_volume() == 80);
	CHECK(loaded->get_pitch() == doctest::Approx(1.25));
	CHECK(loaded->get_rate() == doctest::Approx(0.75));
	CHECK(loaded->is_interrupt());
}

TEST_CASE("[TtsServer] Persona list and default persona") {
	TtsServer *server = TtsServer::get_singleton();
	REQUIRE(server != nullptr);
	server->clear_personas();

	// Without own personas, the built-in default persona is used.
	Ref<TtsPersona> builtin = server->get_default_persona();
	REQUIRE(builtin.is_valid());
	CHECK(server->get_default_persona_name().is_empty());
	CHECK(builtin->get_volume() == 50);

	Ref<TtsPersona> narrator = make_persona("narrator", "", "en");
	Ref<TtsPersona> guard = make_persona("guard", "", "de");
	server->add_persona(narrator);
	server->add_persona(guard);
	CHECK(server->has_persona("narrator"));
	CHECK(server->has_persona("guard"));
	CHECK_FALSE(server->has_persona("king"));
	CHECK(server->get_persona("narrator") == narrator);
	CHECK(server->get_persona("king").is_null());
	CHECK(server->get_personas().size() == 2);
	CHECK(server->get_persona_names().size() == 2);

	ERR_PRINT_OFF;
	server->add_persona(Ref<TtsPersona>());
	server->add_persona(make_persona("", "", ""));
	server->set_default_persona("king");
	ERR_PRINT_ON;
	CHECK(server->get_personas().size() == 2);
	CHECK(server->get_default_persona_name().is_empty());

	server->set_default_persona("guard");
	CHECK(server->get_default_persona_name() == "guard");
	CHECK(server->get_default_persona() == guard);

	// Adding a persona with the same name replaces it.
	Ref<TtsPersona> guard2 = make_persona("guard", "", "fr");
	server->add_persona(guard2);
	CHECK(server->get_personas().size() == 2);
	CHECK(server->get_default_persona() == guard2);

	// Removing the default persona falls back to the built-in one.
	server->remove_persona("guard");
	CHECK_FALSE(server->has_persona("guard"));
	CHECK(server->get_default_persona_name().is_empty());
	CHECK(server->get_default_persona() == builtin);
	server->remove_persona("does_not_exist");

	server->clear_personas();
	CHECK(server->get_personas().is_empty());
}

TEST_CASE("[TtsServer] Graceful behavior without text to speech") {
	TtsServer *server = TtsServer::get_singleton();
	REQUIRE(server != nullptr);
	// Unit tests run without a DisplayServer that supports TTS, and with "audio/general/text_to_speech" off.
	CHECK_FALSE(server->is_available());
	CHECK(server->get_voices().is_empty());
	CHECK(server->get_voices("en").is_empty());
	CHECK_FALSE(server->is_speaking());
	CHECK_FALSE(server->is_paused());
	server->pause();
	server->resume();
	server->stop();

	ERR_PRINT_OFF;
	CHECK(server->speak("Hello") == -1);
	CHECK(server->speak("Hello", make_persona("x", "Voice", "en")) == -1);
	CHECK(server->speak("") == -1);
	ERR_PRINT_ON;
	CHECK(server->get_pending_utterances().is_empty());

	// Without a voice list, the voice of the persona is passed on as it is.
	CHECK(server->resolve_voice(make_persona("x", "Some Voice", "en")) == "Some Voice");
	CHECK(server->resolve_voice(make_persona("x", "", "en")).is_empty());
	CHECK(server->resolve_voice(Ref<TtsPersona>()).is_empty());
}

TEST_CASE("[TtsServer] Utterance events become signals on the main thread") {
	TtsServer *server = TtsServer::get_singleton();
	REQUIRE(server != nullptr);
	MessageQueue::get_singleton()->flush();

	SIGNAL_WATCH(server, "utterance_started");
	SIGNAL_WATCH(server, "utterance_ended");
	SIGNAL_WATCH(server, "utterance_canceled");
	SIGNAL_WATCH(server, "utterance_boundary");

	server->post_utterance_event(DisplayServerEnums::TTS_UTTERANCE_STARTED, 7, 0);
	server->post_utterance_event(DisplayServerEnums::TTS_UTTERANCE_BOUNDARY, 7, 5);
	server->post_utterance_event(DisplayServerEnums::TTS_UTTERANCE_ENDED, 7, 0);
	server->post_utterance_event(DisplayServerEnums::TTS_UTTERANCE_CANCELED, 8, 0);

	// Deferred: nothing arrives before the message queue is flushed.
	SIGNAL_CHECK_FALSE("utterance_started");
	SIGNAL_CHECK_FALSE("utterance_ended");

	MessageQueue::get_singleton()->flush();

	const Array started_args = { { 7 } };
	const Array boundary_args = { { 7, 5 } };
	const Array canceled_args = { { 8 } };
	SIGNAL_CHECK("utterance_started", started_args);
	SIGNAL_CHECK("utterance_boundary", boundary_args);
	SIGNAL_CHECK("utterance_ended", started_args);
	SIGNAL_CHECK("utterance_canceled", canceled_args);

	ERR_PRINT_OFF;
	server->post_utterance_event(DisplayServerEnums::TTS_UTTERANCE_MAX, 9, 0);
	ERR_PRINT_ON;
	MessageQueue::get_singleton()->flush();
	SIGNAL_CHECK_FALSE("utterance_started");

	SIGNAL_UNWATCH(server, "utterance_started");
	SIGNAL_UNWATCH(server, "utterance_ended");
	SIGNAL_UNWATCH(server, "utterance_canceled");
	SIGNAL_UNWATCH(server, "utterance_boundary");
}

} // namespace TestTtsServer
