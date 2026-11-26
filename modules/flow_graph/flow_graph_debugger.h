/**************************************************************************/
/*  flow_graph_debugger.h                                                 */
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

#ifdef DEBUG_ENABLED

#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"

class FlowGraphPlayback;

// Game side of the FlowGraph debugger: sends the state of every FlowGraphPlayback to the editor,
// at most once per playback and frame. Debug builds only; installed only when a debugger is attached.
class FlowGraphDebugger {
public:
	static constexpr const char *CAPTURE = "flow_graph";
	static constexpr const char *MESSAGE = "flow_graph:playback";

	// Fields of the MESSAGE data array.
	enum Field {
		FIELD_ID, // int: instance id of the FlowGraphPlayback.
		FIELD_GRAPH_PATH, // String: path of the started graph ("res://scene.tscn::id" for built-in graphs).
		FIELD_CURRENT_GRAPH_PATH, // String: path of the graph the current node is in (differs inside subgraphs).
		FIELD_NODE_ID, // int: current node id, or FlowGraph.NODE_ID_NONE.
		FIELD_PLAYING, // bool: false once the playback has finished, stopped or been freed.
		FIELD_WAITING, // bool
		FIELD_MAX,
	};

private:
	static inline BinaryMutex mutex;
	static inline HashMap<ObjectID, Array> pending;
	static inline bool flush_queued = false;

	static void _playback_changed(FlowGraphPlayback *p_playback);
	static void _flush();

public:
	static Array encode(const FlowGraphPlayback *p_playback);

	static void initialize();
	static void finalize();
};

#endif // DEBUG_ENABLED
