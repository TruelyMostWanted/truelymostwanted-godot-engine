/**************************************************************************/
/*  flow_graph_player.h                                                   */
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

#include "scene/main/node.h"

// Plays a FlowGraph in the scene tree: a thin front end for a FlowGraphPlayback (owner exception like AnimationPlayer).
// Processing is on only while the current node waits through _process_waiting().
class FlowGraphPlayer : public Node {
	GDCLASS(FlowGraphPlayer, Node);

public:
	enum CallbackModeProcess {
		CALLBACK_MODE_PROCESS_PHYSICS,
		CALLBACK_MODE_PROCESS_IDLE,
		CALLBACK_MODE_PROCESS_MANUAL,
		CALLBACK_MODE_PROCESS_MAX,
	};

private:
	Ref<FlowGraphPlayback> playback;
	Ref<FlowGraph> graph;
	Ref<FlowBlackboard> blackboard;
	bool autostart = false;
	CallbackModeProcess callback_mode_process = CALLBACK_MODE_PROCESS_IDLE;

	void _update_processing();
	void _forward_0(const StringName &p_signal);
	void _forward_1(const Variant &p_arg1, const StringName &p_signal);
	void _forward_2(const Variant &p_arg1, const Variant &p_arg2, const StringName &p_signal);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_graph(const Ref<FlowGraph> &p_graph);
	Ref<FlowGraph> get_graph() const;
	void set_blackboard(const Ref<FlowBlackboard> &p_blackboard);
	Ref<FlowBlackboard> get_blackboard() const;
	void set_autostart(bool p_enabled) { autostart = p_enabled; }
	bool is_autostart() const { return autostart; }
	void set_callback_mode_process(CallbackModeProcess p_mode);
	CallbackModeProcess get_callback_mode_process() const { return callback_mode_process; }

	Error play(int p_from_id = FlowGraphPlayback::START_NODE_DEFAULT);
	void stop();
	bool is_playing() const;
	Ref<FlowGraphPlayback> get_playback() const;

	FlowGraphPlayer();
	~FlowGraphPlayer();
};

VARIANT_ENUM_CAST(FlowGraphPlayer::CallbackModeProcess);
