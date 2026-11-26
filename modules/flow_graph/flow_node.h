/**************************************************************************/
/*  flow_node.h                                                           */
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

#include "flow_graph_playback.h"
#include "flow_port.h"

#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

// Base of all node types of a FlowGraph. Node types are subclasses, in C++ or as scripts (GDScript, C#).
// The graph owns the node ids; a node belongs to at most one graph at a time.
class FlowNode : public Resource {
	GDCLASS(FlowNode, Resource);

	friend class FlowGraph;

	String title_override;
	Vector2 editor_position;
	Vector2 editor_size;
	String editor_comment;

	ObjectID graph_id; // The graph this node belongs to, not stored.

	mutable LocalVector<Ref<FlowPort>> input_ports;
	mutable LocalVector<Ref<FlowPort>> output_ports;
	mutable bool ports_dirty = true;

	void _update_ports() const;
	static Ref<FlowPort> _find_port(const LocalVector<Ref<FlowPort>> &p_ports, const StringName &p_name);
	static TypedArray<FlowPort> _to_array(const LocalVector<Ref<FlowPort>> &p_ports);

protected:
	static void _bind_methods();

	// Defaults of built-in node types; a script override of the matching virtual takes precedence.
	virtual TypedArray<FlowPort> _create_input_ports() const { return TypedArray<FlowPort>(); }
	virtual TypedArray<FlowPort> _create_output_ports() const { return TypedArray<FlowPort>(); }
	virtual String _get_default_category() const { return String(); }
	virtual PackedStringArray _get_default_inline_properties() const { return PackedStringArray(); }
	virtual PackedStringArray _get_default_configuration_warnings() const { return PackedStringArray(); }
	virtual TypedArray<PackedStringArray> _get_default_translatable_strings() const { return TypedArray<PackedStringArray>(); }

	// Runtime defaults of built-in node types; a script override of the matching virtual takes precedence.
	virtual FlowGraphPlayback::EnterResult _default_enter(FlowGraphPlayback *p_playback) const { return FlowGraphPlayback::ENTER_RESULT_CONTINUE; }
	// Runs before a script's _exit(); p_port is the flow output the playback leaves through.
	virtual void _default_exit(FlowGraphPlayback *p_playback, const StringName &p_port) const {}
	virtual StringName _default_choose_exit(FlowGraphPlayback *p_playback) const;
	virtual TypedArray<FlowExit> _default_get_available_exits(FlowGraphPlayback *p_playback) const;
	virtual Variant _default_evaluate_output(FlowGraphPlayback *p_playback, const StringName &p_port) const;
	virtual bool _default_process_waiting(FlowGraphPlayback *p_playback, double p_delta) const { return false; }
	virtual bool _has_default_process_waiting() const { return false; }

	GDVIRTUAL0RC(TypedArray<FlowPort>, _get_input_ports)
	GDVIRTUAL0RC(TypedArray<FlowPort>, _get_output_ports)
	GDVIRTUAL0RC(String, _get_title)
	GDVIRTUAL0RC(String, _get_category)
	GDVIRTUAL0RC(TypedArray<PackedStringArray>, _get_translatable_strings)
	GDVIRTUAL0RC(PackedStringArray, _get_configuration_warnings)
	GDVIRTUAL0RC(PackedStringArray, _get_inline_properties)

	GDVIRTUAL1RC(FlowGraphPlayback::EnterResult, _enter, Ref<FlowGraphPlayback>)
	GDVIRTUAL1C(_exit, Ref<FlowGraphPlayback>)
	GDVIRTUAL1RC(StringName, _choose_exit, Ref<FlowGraphPlayback>)
	GDVIRTUAL1RC(TypedArray<FlowExit>, _get_available_exits, Ref<FlowGraphPlayback>)
	GDVIRTUAL2RC(Variant, _evaluate_output, Ref<FlowGraphPlayback>, StringName)
	GDVIRTUAL2RC(bool, _process_waiting, Ref<FlowGraphPlayback>, double)

public:
	void set_title_override(const String &p_title);
	String get_title_override() const { return title_override; }
	void set_editor_position(const Vector2 &p_position);
	Vector2 get_editor_position() const { return editor_position; }
	void set_editor_size(const Vector2 &p_size);
	Vector2 get_editor_size() const { return editor_size; }
	void set_editor_comment(const String &p_comment);
	String get_editor_comment() const { return editor_comment; }

	// Ports are cached; call notify_ports_changed() when the result of _get_input_ports()/_get_output_ports() changes.
	TypedArray<FlowPort> get_input_ports() const;
	TypedArray<FlowPort> get_output_ports() const;
	Ref<FlowPort> find_input_port(const StringName &p_name) const;
	Ref<FlowPort> find_output_port(const StringName &p_name) const;
	void notify_ports_changed();

	String get_title() const;
	String get_category() const;
	TypedArray<PackedStringArray> get_translatable_strings() const;
	PackedStringArray get_configuration_warnings() const;
	PackedStringArray get_inline_properties() const;

	bool is_in_graph() const;

	// Runtime, called by FlowGraphPlayback.
	FlowGraphPlayback::EnterResult enter(FlowGraphPlayback *p_playback) const;
	// p_port: the flow output the playback leaves through, empty when it travels or stops.
	void exit(FlowGraphPlayback *p_playback, const StringName &p_port = StringName()) const;
	StringName choose_exit(FlowGraphPlayback *p_playback) const;
	TypedArray<FlowExit> get_available_exits(FlowGraphPlayback *p_playback) const;
	Variant evaluate_output(FlowGraphPlayback *p_playback, const StringName &p_port) const;
	bool process_waiting(FlowGraphPlayback *p_playback, double p_delta) const;
	bool uses_process_waiting() const;
};
