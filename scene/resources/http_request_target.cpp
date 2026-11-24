/**************************************************************************/
/*  http_request_target.cpp                                               */
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

#include "http_request_target.h"

#include "core/io/json.h"
#include "core/object/class_db.h"

String HTTPRequestTarget::fill_placeholders(const String &p_text, const Dictionary &p_variables) {
	if (p_variables.is_empty() || !p_text.contains_char('{')) {
		return p_text;
	}
	String text = p_text;
	for (const KeyValue<Variant, Variant> &kv : p_variables) {
		text = text.replace("{" + String(kv.key) + "}", String(kv.value));
	}
	return text;
}

Variant HTTPRequestTarget::_fill_variant(const Variant &p_value, const Dictionary &p_variables) {
	switch (p_value.get_type()) {
		case Variant::STRING: {
			return fill_placeholders(String(p_value), p_variables);
		}
		case Variant::STRING_NAME: {
			return StringName(fill_placeholders(String(p_value), p_variables));
		}
		case Variant::ARRAY: {
			const Array source = p_value;
			Array filled;
			filled.resize(source.size());
			for (int i = 0; i < source.size(); i++) {
				filled[i] = _fill_variant(source[i], p_variables);
			}
			return filled;
		}
		case Variant::DICTIONARY: {
			const Dictionary source = p_value;
			Dictionary filled;
			for (const KeyValue<Variant, Variant> &kv : source) {
				filled[_fill_variant(kv.key, p_variables)] = _fill_variant(kv.value, p_variables);
			}
			return filled;
		}
		default: {
			return p_value;
		}
	}
}

String HTTPRequestTarget::encode_query(const Dictionary &p_fields) {
	// Same rules as `HTTPClient::query_string_from_dict()`, which needs an instance.
	String query;
	for (const KeyValue<Variant, Variant> &kv : p_fields) {
		const String encoded_key = String(kv.key).uri_encode();
		const Variant &value = kv.value;
		switch (value.get_type()) {
			case Variant::ARRAY: {
				// Repeat the key for every value.
				const Array values = value;
				for (int i = 0; i < values.size(); i++) {
					query += "&" + encoded_key + "=" + String(values[i]).uri_encode();
				}
			} break;
			case Variant::NIL: {
				// Add the key without a value.
				query += "&" + encoded_key;
			} break;
			default: {
				query += "&" + encoded_key + "=" + String(value).uri_encode();
			} break;
		}
	}
	return query.substr(1);
}

String HTTPRequestTarget::get_content_type_for(BodyType p_body_type) {
	switch (p_body_type) {
		case BODY_TYPE_TEXT:
			return "text/plain; charset=utf-8";
		case BODY_TYPE_JSON:
			return "application/json";
		case BODY_TYPE_FORM_URLENCODED:
			return "application/x-www-form-urlencoded";
		case BODY_TYPE_BINARY:
			return "application/octet-stream";
		case BODY_TYPE_NONE:
		default:
			return String();
	}
}

bool HTTPRequestTarget::_has_header(const String &p_name) const {
	for (const KeyValue<Variant, Variant> &kv : headers) {
		if (String(kv.key).strip_edges().nocasecmp_to(p_name) == 0) {
			return true;
		}
	}
	return false;
}

void HTTPRequestTarget::set_url(const String &p_url) {
	if (url == p_url) {
		return;
	}
	url = p_url;
	emit_changed();
}

String HTTPRequestTarget::get_url() const {
	return url;
}

void HTTPRequestTarget::set_method(HTTPClient::Method p_method) {
	ERR_FAIL_INDEX(int(p_method), int(HTTPClient::METHOD_MAX));
	if (method == p_method) {
		return;
	}
	method = p_method;
	emit_changed();
}

HTTPClient::Method HTTPRequestTarget::get_method() const {
	return method;
}

void HTTPRequestTarget::set_query(const Dictionary &p_query) {
	query = p_query;
	emit_changed();
}

Dictionary HTTPRequestTarget::get_query() const {
	return query;
}

void HTTPRequestTarget::set_headers(const Dictionary &p_headers) {
	headers = p_headers;
	emit_changed();
}

Dictionary HTTPRequestTarget::get_headers() const {
	return headers;
}

void HTTPRequestTarget::set_body_type(BodyType p_body_type) {
	ERR_FAIL_INDEX(int(p_body_type), int(BODY_TYPE_BINARY) + 1);
	if (body_type == p_body_type) {
		return;
	}
	body_type = p_body_type;
	notify_property_list_changed();
	emit_changed();
}

HTTPRequestTarget::BodyType HTTPRequestTarget::get_body_type() const {
	return body_type;
}

void HTTPRequestTarget::set_body_text(const String &p_body_text) {
	if (body_text == p_body_text) {
		return;
	}
	body_text = p_body_text;
	emit_changed();
}

String HTTPRequestTarget::get_body_text() const {
	return body_text;
}

void HTTPRequestTarget::set_body_fields(const Dictionary &p_body_fields) {
	body_fields = p_body_fields;
	emit_changed();
}

Dictionary HTTPRequestTarget::get_body_fields() const {
	return body_fields;
}

void HTTPRequestTarget::set_body_data(const PackedByteArray &p_body_data) {
	body_data = p_body_data;
	emit_changed();
}

PackedByteArray HTTPRequestTarget::get_body_data() const {
	return body_data;
}

void HTTPRequestTarget::set_timeout(double p_timeout) {
	ERR_FAIL_COND(p_timeout < 0);
	timeout = p_timeout;
	emit_changed();
}

double HTTPRequestTarget::get_timeout() const {
	return timeout;
}

void HTTPRequestTarget::set_accept_gzip(bool p_accept_gzip) {
	accept_gzip = p_accept_gzip;
	emit_changed();
}

bool HTTPRequestTarget::is_accepting_gzip() const {
	return accept_gzip;
}

void HTTPRequestTarget::set_max_redirects(int p_max_redirects) {
	max_redirects = p_max_redirects;
	emit_changed();
}

int HTTPRequestTarget::get_max_redirects() const {
	return max_redirects;
}

String HTTPRequestTarget::get_content_type() const {
	return get_content_type_for(body_type);
}

String HTTPRequestTarget::build_url(const Dictionary &p_variables) const {
	String full_url = fill_placeholders(url, p_variables);
	if (query.is_empty()) {
		return full_url;
	}
	const Dictionary filled_query = _fill_variant(query, p_variables);
	const String query_string = encode_query(filled_query);
	if (query_string.is_empty()) {
		return full_url;
	}
	// Keep a fragment at the end of the URL.
	String fragment;
	const int hash_pos = full_url.find_char('#');
	if (hash_pos != -1) {
		fragment = full_url.substr(hash_pos);
		full_url = full_url.substr(0, hash_pos);
	}
	if (!full_url.contains_char('?')) {
		full_url += "?";
	} else if (!full_url.ends_with("?") && !full_url.ends_with("&")) {
		full_url += "&";
	}
	return full_url + query_string + fragment;
}

PackedStringArray HTTPRequestTarget::build_headers(const Dictionary &p_variables) const {
	PackedStringArray result;
	for (const KeyValue<Variant, Variant> &kv : headers) {
		const String name = fill_placeholders(String(kv.key), p_variables).strip_edges();
		if (name.is_empty()) {
			continue;
		}
		result.push_back(name + ": " + fill_placeholders(String(kv.value), p_variables).strip_edges());
	}
	const String content_type = get_content_type();
	if (!content_type.is_empty() && !_has_header("Content-Type")) {
		result.push_back("Content-Type: " + content_type);
	}
	return result;
}

PackedByteArray HTTPRequestTarget::build_body(const Dictionary &p_variables) const {
	switch (body_type) {
		case BODY_TYPE_TEXT: {
			return fill_placeholders(body_text, p_variables).to_utf8_buffer();
		}
		case BODY_TYPE_JSON: {
			if (body_fields.is_empty()) {
				return fill_placeholders(body_text, p_variables).to_utf8_buffer();
			}
			return JSON::stringify(_fill_variant(body_fields, p_variables), "", false).to_utf8_buffer();
		}
		case BODY_TYPE_FORM_URLENCODED: {
			if (body_fields.is_empty()) {
				return fill_placeholders(body_text, p_variables).to_utf8_buffer();
			}
			const Dictionary filled_fields = _fill_variant(body_fields, p_variables);
			return encode_query(filled_fields).to_utf8_buffer();
		}
		case BODY_TYPE_BINARY: {
			return body_data;
		}
		case BODY_TYPE_NONE:
		default: {
			return PackedByteArray();
		}
	}
}

void HTTPRequestTarget::_validate_property(PropertyInfo &p_property) const {
	// Show only the body fields that the selected body type uses.
	if (p_property.name == "body_text") {
		if (body_type == BODY_TYPE_NONE || body_type == BODY_TYPE_BINARY) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else if (p_property.name == "body_fields") {
		if (body_type != BODY_TYPE_JSON && body_type != BODY_TYPE_FORM_URLENCODED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else if (p_property.name == "body_data") {
		if (body_type != BODY_TYPE_BINARY) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}
}

void HTTPRequestTarget::_bind_methods() {
	ClassDB::bind_static_method("HTTPRequestTarget", D_METHOD("fill_placeholders", "text", "variables"), &HTTPRequestTarget::fill_placeholders);
	ClassDB::bind_static_method("HTTPRequestTarget", D_METHOD("encode_query", "fields"), &HTTPRequestTarget::encode_query);
	ClassDB::bind_static_method("HTTPRequestTarget", D_METHOD("get_content_type_for", "body_type"), &HTTPRequestTarget::get_content_type_for);

	ClassDB::bind_method(D_METHOD("set_url", "url"), &HTTPRequestTarget::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &HTTPRequestTarget::get_url);
	ClassDB::bind_method(D_METHOD("set_method", "method"), &HTTPRequestTarget::set_method);
	ClassDB::bind_method(D_METHOD("get_method"), &HTTPRequestTarget::get_method);
	ClassDB::bind_method(D_METHOD("set_query", "query"), &HTTPRequestTarget::set_query);
	ClassDB::bind_method(D_METHOD("get_query"), &HTTPRequestTarget::get_query);
	ClassDB::bind_method(D_METHOD("set_headers", "headers"), &HTTPRequestTarget::set_headers);
	ClassDB::bind_method(D_METHOD("get_headers"), &HTTPRequestTarget::get_headers);
	ClassDB::bind_method(D_METHOD("set_body_type", "body_type"), &HTTPRequestTarget::set_body_type);
	ClassDB::bind_method(D_METHOD("get_body_type"), &HTTPRequestTarget::get_body_type);
	ClassDB::bind_method(D_METHOD("set_body_text", "body_text"), &HTTPRequestTarget::set_body_text);
	ClassDB::bind_method(D_METHOD("get_body_text"), &HTTPRequestTarget::get_body_text);
	ClassDB::bind_method(D_METHOD("set_body_fields", "body_fields"), &HTTPRequestTarget::set_body_fields);
	ClassDB::bind_method(D_METHOD("get_body_fields"), &HTTPRequestTarget::get_body_fields);
	ClassDB::bind_method(D_METHOD("set_body_data", "body_data"), &HTTPRequestTarget::set_body_data);
	ClassDB::bind_method(D_METHOD("get_body_data"), &HTTPRequestTarget::get_body_data);
	ClassDB::bind_method(D_METHOD("set_timeout", "timeout"), &HTTPRequestTarget::set_timeout);
	ClassDB::bind_method(D_METHOD("get_timeout"), &HTTPRequestTarget::get_timeout);
	ClassDB::bind_method(D_METHOD("set_accept_gzip", "enable"), &HTTPRequestTarget::set_accept_gzip);
	ClassDB::bind_method(D_METHOD("is_accepting_gzip"), &HTTPRequestTarget::is_accepting_gzip);
	ClassDB::bind_method(D_METHOD("set_max_redirects", "amount"), &HTTPRequestTarget::set_max_redirects);
	ClassDB::bind_method(D_METHOD("get_max_redirects"), &HTTPRequestTarget::get_max_redirects);

	ClassDB::bind_method(D_METHOD("get_content_type"), &HTTPRequestTarget::get_content_type);
	ClassDB::bind_method(D_METHOD("build_url", "variables"), &HTTPRequestTarget::build_url, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("build_headers", "variables"), &HTTPRequestTarget::build_headers, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("build_body", "variables"), &HTTPRequestTarget::build_body, DEFVAL(Dictionary()));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url", PROPERTY_HINT_PLACEHOLDER_TEXT, "https://example.com/api/{id}"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "method", PROPERTY_HINT_ENUM, "GET,HEAD,POST,PUT,DELETE,OPTIONS,TRACE,CONNECT,PATCH"), "set_method", "get_method");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "query"), "set_query", "get_query");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers"), "set_headers", "get_headers");

	ADD_GROUP("Body", "body_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "body_type", PROPERTY_HINT_ENUM, "None,Text,JSON,Form URL-Encoded,Binary"), "set_body_type", "get_body_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "body_text", PROPERTY_HINT_MULTILINE_TEXT), "set_body_text", "get_body_text");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "body_fields"), "set_body_fields", "get_body_fields");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "body_data"), "set_body_data", "get_body_data");

	ADD_GROUP("Options", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "timeout", PROPERTY_HINT_RANGE, "0,3600,0.1,or_greater,suffix:s"), "set_timeout", "get_timeout");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "accept_gzip"), "set_accept_gzip", "is_accepting_gzip");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_redirects", PROPERTY_HINT_RANGE, "-1,64"), "set_max_redirects", "get_max_redirects");

	BIND_ENUM_CONSTANT(BODY_TYPE_NONE);
	BIND_ENUM_CONSTANT(BODY_TYPE_TEXT);
	BIND_ENUM_CONSTANT(BODY_TYPE_JSON);
	BIND_ENUM_CONSTANT(BODY_TYPE_FORM_URLENCODED);
	BIND_ENUM_CONSTANT(BODY_TYPE_BINARY);
}
