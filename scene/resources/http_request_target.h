/**************************************************************************/
/*  http_request_target.h                                                 */
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

#include "core/io/http_client.h"
#include "core/io/resource.h"

// Describes one HTTP request (URL, method, query, headers and body) as a reusable resource.
// `HTTPRequest::request_target()` sends it; `{name}` placeholders are filled per call.
class HTTPRequestTarget : public Resource {
	GDCLASS(HTTPRequestTarget, Resource);

public:
	enum BodyType {
		BODY_TYPE_NONE,
		BODY_TYPE_TEXT,
		BODY_TYPE_JSON,
		BODY_TYPE_FORM_URLENCODED,
		BODY_TYPE_BINARY,
	};

private:
	String url;
	HTTPClient::Method method = HTTPClient::METHOD_GET;
	Dictionary query;
	Dictionary headers;
	BodyType body_type = BODY_TYPE_NONE;
	String body_text;
	Dictionary body_fields;
	PackedByteArray body_data;
	double timeout = 0.0;
	bool accept_gzip = true;
	int max_redirects = 8;

	static Variant _fill_variant(const Variant &p_value, const Dictionary &p_variables);
	bool _has_header(const String &p_name) const;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static String fill_placeholders(const String &p_text, const Dictionary &p_variables);
	static String encode_query(const Dictionary &p_fields);
	static String get_content_type_for(BodyType p_body_type);

	void set_url(const String &p_url);
	String get_url() const;

	void set_method(HTTPClient::Method p_method);
	HTTPClient::Method get_method() const;

	void set_query(const Dictionary &p_query);
	Dictionary get_query() const;

	void set_headers(const Dictionary &p_headers);
	Dictionary get_headers() const;

	void set_body_type(BodyType p_body_type);
	BodyType get_body_type() const;

	void set_body_text(const String &p_body_text);
	String get_body_text() const;

	void set_body_fields(const Dictionary &p_body_fields);
	Dictionary get_body_fields() const;

	void set_body_data(const PackedByteArray &p_body_data);
	PackedByteArray get_body_data() const;

	void set_timeout(double p_timeout);
	double get_timeout() const;

	void set_accept_gzip(bool p_accept_gzip);
	bool is_accepting_gzip() const;

	void set_max_redirects(int p_max_redirects);
	int get_max_redirects() const;

	String get_content_type() const;
	String build_url(const Dictionary &p_variables = Dictionary()) const;
	PackedStringArray build_headers(const Dictionary &p_variables = Dictionary()) const;
	PackedByteArray build_body(const Dictionary &p_variables = Dictionary()) const;
};

VARIANT_ENUM_CAST(HTTPRequestTarget::BodyType);
