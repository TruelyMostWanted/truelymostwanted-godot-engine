/**************************************************************************/
/*  resource_error_capture.cpp                                            */
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

#include "resource_error_capture.h"

static ErrorHandlerList resource_error_capture_handler;
static bool resource_error_capture_registered = false;

void ResourceErrorCapture::_add_line(const String &p_line) {
	if (lines.size() >= MAX_LINES) {
		dropped++;
		return;
	}
	lines.push_back(p_line);
}

// Called by `_err_print_error()` with the global print lock held. Thread-local data only, no
// other lock and no printing.
void ResourceErrorCapture::_error_handler(void *p_userdata, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, bool p_editor_notify, ErrorHandlerType p_type) {
	ResourceErrorCapture *capture = current;
	if (likely(capture == nullptr)) {
		return;
	}
	if (p_type != ERR_HANDLER_ERROR && p_type != ERR_HANDLER_SCRIPT) {
		return; // Warnings and shader errors do not explain a failed load or save.
	}
	const char *text = (p_message && *p_message) ? p_message : p_error;
	if (text == nullptr || *text == '\0') {
		return;
	}
	capture->_add_line(String::utf8(text));
}

void ResourceErrorCapture::initialize() {
	if (resource_error_capture_registered) {
		return;
	}
	resource_error_capture_handler.errfunc = &ResourceErrorCapture::_error_handler;
	resource_error_capture_handler.userdata = nullptr;
	add_error_handler(&resource_error_capture_handler);
	resource_error_capture_registered = true;
}

void ResourceErrorCapture::finalize() {
	if (!resource_error_capture_registered) {
		return;
	}
	remove_error_handler(&resource_error_capture_handler);
	resource_error_capture_registered = false;
}

String ResourceErrorCapture::get_message() const {
	String message;
	for (uint32_t i = 0; i < lines.size(); i++) {
		if (i > 0) {
			message += "\n";
		}
		message += lines[i];
	}
	if (dropped > 0) {
		message += "\n(" + itos(dropped) + " more errors)";
	}
	return message;
}

ResourceErrorCapture::ResourceErrorCapture() {
	parent = current;
	current = this;
}

ResourceErrorCapture::~ResourceErrorCapture() {
	current = parent;
	if (parent) {
		for (const String &line : lines) {
			parent->_add_line(line);
		}
		parent->dropped += dropped;
	}
}
