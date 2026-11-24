/**************************************************************************/
/*  http_response.h                                                       */
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

#include "core/object/ref_counted.h"
#include "scene/main/http_request.h"
#include "scene/resources/http_request_target.h"

// Typed result of one request sent by `HTTPRequest`.
class HTTPResponse : public RefCounted {
	GDCLASS(HTTPResponse, RefCounted);

	HTTPRequest::Result result = HTTPRequest::RESULT_SUCCESS;
	int status_code = 0;
	Dictionary headers;
	PackedStringArray raw_headers;
	PackedByteArray body;
	Ref<HTTPRequestTarget> target;

protected:
	static void _bind_methods();

public:
	static Ref<HTTPResponse> create(HTTPRequest::Result p_result, int p_status_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, const Ref<HTTPRequestTarget> &p_target);

	HTTPRequest::Result get_result() const;
	int get_status_code() const;
	Dictionary get_headers() const;
	PackedStringArray get_raw_headers() const;
	PackedByteArray get_body() const;
	Ref<HTTPRequestTarget> get_target() const;

	String get_header(const String &p_name, const String &p_default = String()) const;
	String get_text() const;
	Variant get_json() const;
	bool is_success() const;
};
