/**************************************************************************/
/*  resource_error_capture.h                                              */
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

#include "core/error/error_macros.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"

// Collects the errors printed on the current thread while the scope is alive, so a load or
// save can report the loader's or saver's own message instead of only an error code.
//
// One error handler is registered for the whole engine (`initialize()`); it only reads a
// thread-local pointer and returns when no scope is active, so the cost when nothing fails is
// one thread-local read per printed error and two pointer writes per scope. The handler runs
// while the global print lock is held: it touches thread-local data only, takes no other lock
// and never prints (see #60).
//
// Scopes nest: when an inner scope ends, its lines are appended to the enclosing scope on the
// same thread, so a failed dependency shows up in the message of the resource that needed it.
// Errors printed on other threads (worker threads of a threaded load) are captured by the scope
// that runs on that thread; `ResourceLoader` stores them in its load task for the caller.
class ResourceErrorCapture {
	static constexpr uint32_t MAX_LINES = 16;

	static inline thread_local ResourceErrorCapture *current = nullptr;

	ResourceErrorCapture *parent = nullptr;
	LocalVector<String> lines;
	uint32_t dropped = 0;

	void _add_line(const String &p_line);
	static void _error_handler(void *p_userdata, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type);

public:
	static void initialize();
	static void finalize();

	bool has_errors() const { return !lines.is_empty(); }
	// The captured errors, one per line, oldest first. Empty when nothing was printed.
	String get_message() const;

	ResourceErrorCapture();
	~ResourceErrorCapture();

	ResourceErrorCapture(const ResourceErrorCapture &) = delete;
	ResourceErrorCapture &operator=(const ResourceErrorCapture &) = delete;
};
