/**************************************************************************/
/*  test_http_request_target.cpp                                          */
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

TEST_FORCE_LINK(test_http_request_target)

#include "core/io/json.h"
#include "scene/main/http_response.h"
#include "scene/resources/http_request_target.h"

namespace TestHTTPRequestTarget {

static String body_string(const PackedByteArray &p_body) {
	return String::utf8(reinterpret_cast<const char *>(p_body.ptr()), p_body.size());
}

static bool has_line(const PackedStringArray &p_lines, const String &p_line) {
	return p_lines.has(p_line);
}

TEST_CASE("[HTTPRequestTarget] Defaults match HTTPRequest") {
	Ref<HTTPRequestTarget> target;
	target.instantiate();
	CHECK(target->get_method() == HTTPClient::METHOD_GET);
	CHECK(target->get_body_type() == HTTPRequestTarget::BODY_TYPE_NONE);
	CHECK(target->get_timeout() == 0.0);
	CHECK(target->is_accepting_gzip());
	CHECK(target->get_max_redirects() == 8);
	CHECK(target->build_body().is_empty());
	CHECK(target->build_headers().is_empty());
}

TEST_CASE("[HTTPRequestTarget] Placeholders") {
	Dictionary variables;
	variables["id"] = 42;
	variables["name"] = "Ada";

	CHECK(HTTPRequestTarget::fill_placeholders("/users/{id}/{name}", variables) == "/users/42/Ada");
	CHECK(HTTPRequestTarget::fill_placeholders("/users/{unknown}", variables) == "/users/{unknown}");
	CHECK(HTTPRequestTarget::fill_placeholders("/users/{id}", Dictionary()) == "/users/{id}");
	CHECK(HTTPRequestTarget::fill_placeholders("no placeholders", variables) == "no placeholders");
}

TEST_CASE("[HTTPRequestTarget] URL and query") {
	Ref<HTTPRequestTarget> target;
	target.instantiate();
	target->set_url("http://127.0.0.1:8080/{section}/items");
	CHECK(target->build_url() == "http://127.0.0.1:8080/{section}/items");

	Dictionary variables;
	variables["section"] = "shop";
	variables["term"] = "red apple";
	CHECK(target->build_url(variables) == "http://127.0.0.1:8080/shop/items");

	Dictionary query;
	query["q"] = "{term}";
	query["page"] = 2;
	Array tags;
	tags.push_back("a");
	tags.push_back("b&c");
	query["tag"] = tags;
	query["flag"] = Variant();
	target->set_query(query);
	CHECK(target->build_url(variables) == "http://127.0.0.1:8080/shop/items?q=red%20apple&page=2&tag=a&tag=b%26c&flag");

	SUBCASE("Existing query and fragment") {
		target->set_url("http://host/path?x=1#top");
		Dictionary single;
		single["y"] = 2;
		target->set_query(single);
		CHECK(target->build_url() == "http://host/path?x=1&y=2#top");
	}

	SUBCASE("Same rules as HTTPClient") {
		CHECK(HTTPRequestTarget::encode_query(query) == "q=%7Bterm%7D&page=2&tag=a&tag=b%26c&flag");
		CHECK(HTTPRequestTarget::encode_query(Dictionary()) == "");
	}
}

TEST_CASE("[HTTPRequestTarget] Headers and Content-Type") {
	Ref<HTTPRequestTarget> target;
	target.instantiate();

	Dictionary headers;
	headers["Authorization"] = "Bearer {token}";
	headers["X-Game"] = "demo";
	target->set_headers(headers);

	Dictionary variables;
	variables["token"] = "abc";

	PackedStringArray built = target->build_headers(variables);
	CHECK(built.size() == 2);
	CHECK(has_line(built, "Authorization: Bearer abc"));
	CHECK(has_line(built, "X-Game: demo"));

	target->set_body_type(HTTPRequestTarget::BODY_TYPE_JSON);
	built = target->build_headers(variables);
	CHECK(built.size() == 3);
	CHECK(has_line(built, "Content-Type: application/json"));

	// A Content-Type in the headers wins, compared without case.
	headers["content-type"] = "application/vnd.api+json";
	target->set_headers(headers);
	built = target->build_headers(variables);
	CHECK(built.size() == 3);
	CHECK(has_line(built, "content-type: application/vnd.api+json"));
	CHECK_FALSE(has_line(built, "Content-Type: application/json"));

	CHECK(HTTPRequestTarget::get_content_type_for(HTTPRequestTarget::BODY_TYPE_NONE) == "");
	CHECK(HTTPRequestTarget::get_content_type_for(HTTPRequestTarget::BODY_TYPE_TEXT) == "text/plain; charset=utf-8");
	CHECK(HTTPRequestTarget::get_content_type_for(HTTPRequestTarget::BODY_TYPE_FORM_URLENCODED) == "application/x-www-form-urlencoded");
	CHECK(HTTPRequestTarget::get_content_type_for(HTTPRequestTarget::BODY_TYPE_BINARY) == "application/octet-stream");
}

TEST_CASE("[HTTPRequestTarget] Bodies") {
	Ref<HTTPRequestTarget> target;
	target.instantiate();

	Dictionary variables;
	variables["player"] = "Ada \"the\" first";
	variables["score"] = 9001;

	SUBCASE("None ignores body fields") {
		target->set_body_text("ignored");
		CHECK(target->build_body(variables).is_empty());
	}

	SUBCASE("Text") {
		target->set_body_type(HTTPRequestTarget::BODY_TYPE_TEXT);
		target->set_body_text("Hello {player}!");
		CHECK(body_string(target->build_body(variables)) == "Hello Ada \"the\" first!");
	}

	SUBCASE("JSON from fields") {
		target->set_body_type(HTTPRequestTarget::BODY_TYPE_JSON);
		Dictionary fields;
		fields["name"] = "{player}";
		fields["level"] = 3;
		Array items;
		items.push_back("{score}");
		fields["items"] = items;
		target->set_body_fields(fields);

		const String json_text = body_string(target->build_body(variables));
		// Insertion order is kept, quotes from placeholder values are escaped.
		CHECK(json_text == "{\"name\":\"Ada \\\"the\\\" first\",\"level\":3,\"items\":[\"9001\"]}");
		const Dictionary parsed = JSON::parse_string(json_text);
		CHECK(String(parsed["name"]) == "Ada \"the\" first");
	}

	SUBCASE("JSON from text") {
		target->set_body_type(HTTPRequestTarget::BODY_TYPE_JSON);
		target->set_body_text("{\"score\": {score}}");
		CHECK(body_string(target->build_body(variables)) == "{\"score\": 9001}");
	}

	SUBCASE("Form from fields") {
		target->set_body_type(HTTPRequestTarget::BODY_TYPE_FORM_URLENCODED);
		Dictionary fields;
		fields["user"] = "{player}";
		fields["score"] = "{score}";
		target->set_body_fields(fields);
		CHECK(body_string(target->build_body(variables)) == "user=Ada%20%22the%22%20first&score=9001");
	}

	SUBCASE("Binary") {
		target->set_body_type(HTTPRequestTarget::BODY_TYPE_BINARY);
		PackedByteArray data;
		data.push_back(0);
		data.push_back(255);
		data.push_back(123);
		target->set_body_data(data);
		CHECK(target->build_body(variables) == data);
	}
}

TEST_CASE("[HTTPRequestTarget] Placeholders in keys") {
	Ref<HTTPRequestTarget> target;
	target.instantiate();
	Dictionary query;
	query["{key}"] = "{value}";
	target->set_query(query);
	target->set_url("http://host/");

	Dictionary variables;
	variables["key"] = "lang";
	variables["value"] = "de";
	CHECK(target->build_url(variables) == "http://host/?lang=de");
}

TEST_CASE("[HTTPResponse] Headers, text, JSON and success") {
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Set-Cookie: a=1");
	headers.push_back("set-cookie: b=2");
	const PackedByteArray body = String("{\"ok\":true}").to_utf8_buffer();

	Ref<HTTPRequestTarget> target;
	target.instantiate();

	Ref<HTTPResponse> response = HTTPResponse::create(HTTPRequest::RESULT_SUCCESS, 200, headers, body, target);
	CHECK(response->is_success());
	CHECK(response->get_status_code() == 200);
	CHECK(response->get_result() == HTTPRequest::RESULT_SUCCESS);
	CHECK(response->get_target() == target);
	CHECK(response->get_raw_headers() == headers);
	CHECK(response->get_header("content-type") == "application/json");
	CHECK(response->get_header("CONTENT-TYPE") == "application/json");
	CHECK(response->get_header("Set-Cookie") == "a=1, b=2");
	CHECK(response->get_header("X-Missing", "none") == "none");
	CHECK(response->get_headers().size() == 2);
	CHECK(response->get_text() == "{\"ok\":true}");
	const Dictionary json = response->get_json();
	CHECK(bool(json["ok"]));

	Ref<HTTPResponse> not_found = HTTPResponse::create(HTTPRequest::RESULT_SUCCESS, 404, PackedStringArray(), String("not json").to_utf8_buffer(), Ref<HTTPRequestTarget>());
	CHECK_FALSE(not_found->is_success());
	CHECK(not_found->get_json().get_type() == Variant::NIL);
	CHECK(not_found->get_target().is_null());

	Ref<HTTPResponse> failed = HTTPResponse::create(HTTPRequest::RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray(), Ref<HTTPRequestTarget>());
	CHECK_FALSE(failed->is_success());
	CHECK(failed->get_text() == "");
}

} // namespace TestHTTPRequestTarget
