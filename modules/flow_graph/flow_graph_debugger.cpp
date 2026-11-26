/**************************************************************************/
/*  flow_graph_debugger.cpp                                               */
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

#include "flow_graph_debugger.h"

#ifdef DEBUG_ENABLED

#include "flow_graph.h"
#include "flow_graph_playback.h"

#include "core/debugger/engine_debugger.h"
#include "core/object/callable_mp.h"
#include "core/templates/local_vector.h"

Array FlowGraphDebugger::encode(const FlowGraphPlayback *p_playback) {
	const Ref<FlowGraph> graph = p_playback->get_graph();
	const Ref<FlowGraph> current = p_playback->get_current_graph();
	Array data;
	data.resize(FIELD_MAX);
	data[FIELD_ID] = (int64_t)(uint64_t)p_playback->get_instance_id();
	data[FIELD_GRAPH_PATH] = graph.is_valid() ? graph->get_path() : String();
	data[FIELD_CURRENT_GRAPH_PATH] = current.is_valid() ? current->get_path() : String();
	data[FIELD_NODE_ID] = p_playback->get_current_node_id();
	data[FIELD_PLAYING] = p_playback->is_playing();
	data[FIELD_WAITING] = p_playback->is_waiting();
	return data;
}

void FlowGraphDebugger::_playback_changed(FlowGraphPlayback *p_playback) {
	const Array data = encode(p_playback);
	bool queue_flush = false;
	{
		MutexLock lock(mutex);
		pending[p_playback->get_instance_id()] = data;
		queue_flush = !flush_queued;
		flush_queued = true;
	}
	if (queue_flush) {
		callable_mp_static(&FlowGraphDebugger::_flush).call_deferred();
	}
}

void FlowGraphDebugger::_flush() {
	LocalVector<Array> messages;
	{
		MutexLock lock(mutex);
		for (const KeyValue<ObjectID, Array> &E : pending) {
			messages.push_back(E.value);
		}
		pending.clear();
		flush_queued = false;
	}
	if (EngineDebugger::is_active()) {
		for (const Array &data : messages) {
			EngineDebugger::get_singleton()->send_message(MESSAGE, data);
		}
	}
}

void FlowGraphDebugger::initialize() {
	if (EngineDebugger::is_active()) {
		FlowGraphPlayback::debug_hook = &FlowGraphDebugger::_playback_changed;
	}
}

void FlowGraphDebugger::finalize() {
	FlowGraphPlayback::debug_hook = nullptr;
	MutexLock lock(mutex);
	pending.clear();
}

#endif // DEBUG_ENABLED
