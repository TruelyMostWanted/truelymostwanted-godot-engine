/**************************************************************************/
/*  flow_graph_player.cpp                                                 */
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

#include "flow_graph_player.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void FlowGraphPlayer::_update_processing() {
	const bool needed = playback->is_processing_needed() && !Engine::get_singleton()->is_editor_hint();
	set_process_internal(needed && callback_mode_process == CALLBACK_MODE_PROCESS_IDLE);
	set_physics_process_internal(needed && callback_mode_process == CALLBACK_MODE_PROCESS_PHYSICS);
}

void FlowGraphPlayer::_forward_0(const StringName &p_signal) {
	_update_processing();
	emit_signal(p_signal);
}

void FlowGraphPlayer::_forward_1(const Variant &p_arg1, const StringName &p_signal) {
	_update_processing();
	emit_signal(p_signal, p_arg1);
}

void FlowGraphPlayer::_forward_2(const Variant &p_arg1, const Variant &p_arg2, const StringName &p_signal) {
	_update_processing();
	emit_signal(p_signal, p_arg1, p_arg2);
}

void FlowGraphPlayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (autostart && graph.is_valid() && !Engine::get_singleton()->is_editor_hint()) {
				play();
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			playback->process(get_process_delta_time());
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			playback->process(get_physics_process_delta_time());
		} break;
	}
}

void FlowGraphPlayer::set_graph(const Ref<FlowGraph> &p_graph) {
	graph = p_graph;
}

Ref<FlowGraph> FlowGraphPlayer::get_graph() const {
	return graph;
}

void FlowGraphPlayer::set_blackboard(const Ref<FlowBlackboard> &p_blackboard) {
	blackboard = p_blackboard;
	playback->set_blackboard(p_blackboard);
}

Ref<FlowBlackboard> FlowGraphPlayer::get_blackboard() const {
	return blackboard;
}

void FlowGraphPlayer::set_callback_mode_process(CallbackModeProcess p_mode) {
	ERR_FAIL_INDEX(p_mode, CALLBACK_MODE_PROCESS_MAX);
	callback_mode_process = p_mode;
	_update_processing();
}

Error FlowGraphPlayer::play(int p_from_id) {
	ERR_FAIL_COND_V_MSG(graph.is_null(), ERR_UNCONFIGURED, "FlowGraphPlayer: no graph to play.");
	return playback->start(graph, p_from_id);
}

void FlowGraphPlayer::stop() {
	playback->stop();
}

bool FlowGraphPlayer::is_playing() const {
	return playback->is_playing();
}

Ref<FlowGraphPlayback> FlowGraphPlayer::get_playback() const {
	return playback;
}

void FlowGraphPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_graph", "graph"), &FlowGraphPlayer::set_graph);
	ClassDB::bind_method(D_METHOD("get_graph"), &FlowGraphPlayer::get_graph);
	ClassDB::bind_method(D_METHOD("set_blackboard", "blackboard"), &FlowGraphPlayer::set_blackboard);
	ClassDB::bind_method(D_METHOD("get_blackboard"), &FlowGraphPlayer::get_blackboard);
	ClassDB::bind_method(D_METHOD("set_autostart", "enabled"), &FlowGraphPlayer::set_autostart);
	ClassDB::bind_method(D_METHOD("is_autostart"), &FlowGraphPlayer::is_autostart);
	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "mode"), &FlowGraphPlayer::set_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &FlowGraphPlayer::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("play", "from_id"), &FlowGraphPlayer::play, DEFVAL(FlowGraphPlayback::START_NODE_DEFAULT));
	ClassDB::bind_method(D_METHOD("stop"), &FlowGraphPlayer::stop);
	ClassDB::bind_method(D_METHOD("is_playing"), &FlowGraphPlayer::is_playing);
	ClassDB::bind_method(D_METHOD("get_playback"), &FlowGraphPlayer::get_playback);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "graph", PROPERTY_HINT_RESOURCE_TYPE, "FlowGraph"), "set_graph", "get_graph");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "blackboard", PROPERTY_HINT_RESOURCE_TYPE, "FlowBlackboard"), "set_blackboard", "get_blackboard");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autostart"), "set_autostart", "is_autostart");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_callback_mode_process", "get_callback_mode_process");

	// Forwarded from the playback.
	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("node_entered", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("node_exited", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("node_waiting", PropertyInfo(Variant::INT, "node_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "FlowNode")));
	ADD_SIGNAL(MethodInfo("event_emitted", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::ARRAY, "arguments")));
	ADD_SIGNAL(MethodInfo("variable_changed", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::INT, "end_node_id")));
	ADD_SIGNAL(MethodInfo("stopped"));

	BIND_ENUM_CONSTANT(CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(CALLBACK_MODE_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(CALLBACK_MODE_PROCESS_MANUAL);
	BIND_ENUM_CONSTANT(CALLBACK_MODE_PROCESS_MAX);
}

FlowGraphPlayer::FlowGraphPlayer() {
	playback.instantiate();
	for (const char *signal_name : { "started", "stopped" }) {
		playback->connect(signal_name, callable_mp(this, &FlowGraphPlayer::_forward_0).bind(StringName(signal_name)));
	}
	playback->connect(SNAME("finished"), callable_mp(this, &FlowGraphPlayer::_forward_1).bind(SNAME("finished")));
	for (const char *signal_name : { "node_entered", "node_exited", "node_waiting", "event_emitted", "variable_changed" }) {
		playback->connect(signal_name, callable_mp(this, &FlowGraphPlayer::_forward_2).bind(StringName(signal_name)));
	}
}

FlowGraphPlayer::~FlowGraphPlayer() {
}
