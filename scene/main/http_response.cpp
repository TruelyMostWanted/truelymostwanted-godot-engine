/**************************************************************************/
/*  http_response.cpp                                                     */
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

#include "http_response.h"

#include "core/io/json.h"
#include "core/object/class_db.h"

Ref<HTTPResponse> HTTPResponse::create(HTTPRequest::Result p_result, int p_status_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, const Ref<HTTPRequestTarget> &p_target) {
	Ref<HTTPResponse> response;
	response.instantiate();
	response->result = p_result;
	response->status_code = p_status_code;
	response->raw_headers = p_headers;
	response->body = p_body;
	response->target = p_target;

	for (const String &line : p_headers) {
		const int colon = line.find_char(':');
		if (colon < 1) {
			continue;
		}
		const String name = line.substr(0, colon).strip_edges().to_lower();
		const String value = line.substr(colon + 1).strip_edges();
		if (response->headers.has(name)) {
			// Repeated headers are joined as defined by RFC 9110, section 5.3.
			response->headers[name] = String(response->headers[name]) + ", " + value;
		} else {
			response->headers[name] = value;
		}
	}
	return response;
}

HTTPRequest::Result HTTPResponse::get_result() const {
	return result;
}

int HTTPResponse::get_status_code() const {
	return status_code;
}

Dictionary HTTPResponse::get_headers() const {
	return headers.duplicate();
}

PackedStringArray HTTPResponse::get_raw_headers() const {
	return raw_headers;
}

PackedByteArray HTTPResponse::get_body() const {
	return body;
}

Ref<HTTPRequestTarget> HTTPResponse::get_target() const {
	return target;
}

String HTTPResponse::get_header(const String &p_name, const String &p_default) const {
	const String key = p_name.strip_edges().to_lower();
	if (!headers.has(key)) {
		return p_default;
	}
	return headers[key];
}

String HTTPResponse::get_text() const {
	if (body.is_empty()) {
		return String();
	}
	return String::utf8(reinterpret_cast<const char *>(body.ptr()), body.size());
}

Variant HTTPResponse::get_json() const {
	const String text = get_text();
	if (text.is_empty()) {
		return Variant();
	}
	// Parse without printing an error: a non-JSON body is a valid server answer.
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(text) != OK) {
		return Variant();
	}
	return json->get_data();
}

bool HTTPResponse::is_success() const {
	return result == HTTPRequest::RESULT_SUCCESS && status_code >= 200 && status_code < 300;
}

void HTTPResponse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_result"), &HTTPResponse::get_result);
	ClassDB::bind_method(D_METHOD("get_status_code"), &HTTPResponse::get_status_code);
	ClassDB::bind_method(D_METHOD("get_headers"), &HTTPResponse::get_headers);
	ClassDB::bind_method(D_METHOD("get_raw_headers"), &HTTPResponse::get_raw_headers);
	ClassDB::bind_method(D_METHOD("get_body"), &HTTPResponse::get_body);
	ClassDB::bind_method(D_METHOD("get_target"), &HTTPResponse::get_target);

	ClassDB::bind_method(D_METHOD("get_header", "name", "default"), &HTTPResponse::get_header, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_text"), &HTTPResponse::get_text);
	ClassDB::bind_method(D_METHOD("get_json"), &HTTPResponse::get_json);
	ClassDB::bind_method(D_METHOD("is_success"), &HTTPResponse::is_success);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "result", PROPERTY_HINT_ENUM, "Success,Chunked Body Size Mismatch,Can't Connect,Can't Resolve,Connection Error,TLS Handshake Error,No Response,Body Size Limit Exceeded,Body Decompress Failed,Request Failed,Download File Can't Open,Download File Write Error,Redirect Limit Reached,Timeout", PROPERTY_USAGE_NONE), "", "get_result");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "status_code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_code");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_headers");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "raw_headers", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_raw_headers");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "body", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_body");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, HTTPRequestTarget::get_class_static(), PROPERTY_USAGE_NONE), "", "get_target");
}
