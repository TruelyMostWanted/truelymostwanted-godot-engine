/**************************************************************************/
/*  flow_graph_mermaid.h                                                  */
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

#include "flow_graph.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/variant/binder_common.h"

// Options of FlowGraphMermaid.serialize().
class FlowGraphMermaidOptions : public Resource {
	GDCLASS(FlowGraphMermaidOptions, Resource);

public:
	enum Direction {
		DIRECTION_TOP_DOWN,
		DIRECTION_LEFT_RIGHT,
		DIRECTION_BOTTOM_UP,
		DIRECTION_RIGHT_LEFT,
		DIRECTION_MAX,
	};

private:
	Direction direction = DIRECTION_TOP_DOWN;
	bool include_data_comments = true;
	bool include_editor_data = true;
	bool draw_data_connections = true;

protected:
	static void _bind_methods();

public:
	void set_direction(Direction p_direction);
	Direction get_direction() const { return direction; }
	void set_include_data_comments(bool p_enabled);
	bool is_including_data_comments() const { return include_data_comments; }
	void set_include_editor_data(bool p_enabled);
	bool is_including_editor_data() const { return include_editor_data; }
	void set_draw_data_connections(bool p_enabled);
	bool is_drawing_data_connections() const { return draw_data_connections; }
};

// Reads and writes a FlowGraph as a Mermaid flowchart (`.mmd`). The structure is plain Mermaid; types and properties
// are kept in `%% flow:` comment lines, so the round trip is lossless. Errors are kept per instance (safe on loader threads).
class FlowGraphMermaid : public RefCounted {
	GDCLASS(FlowGraphMermaid, RefCounted);

	Ref<FlowGraph> graph;
	int error_line = 0;
	String error_message;

	Error _fail(int p_line, const String &p_message);

protected:
	static void _bind_methods();

public:
	Error parse(const String &p_text);
	Ref<FlowGraph> get_graph() const { return graph; }
	int get_error_line() const { return error_line; }
	String get_error_message() const { return error_message; }

	static String serialize(const Ref<FlowGraph> &p_graph, const Ref<FlowGraphMermaidOptions> &p_options = Ref<FlowGraphMermaidOptions>());
	static Ref<FlowGraph> deserialize(const String &p_text);
};

class ResourceFormatLoaderFlowGraphMermaid : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderFlowGraphMermaid, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverFlowGraphMermaid : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverFlowGraphMermaid, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
};

VARIANT_ENUM_CAST(FlowGraphMermaidOptions::Direction);
