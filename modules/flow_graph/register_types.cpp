/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "dialogue/dialogue_nodes.h"
#include "flow_graph.h"
#include "flow_graph_data.h"
#include "flow_graph_debugger.h"
#include "flow_graph_playback.h"
#include "flow_graph_player.h"
#include "flow_node.h"
#include "flow_nodes.h"
#include "flow_port.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/flow_graph_editor_plugin.h"
#endif

void initialize_flow_graph_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<FlowGraphEditorPlugin>();
	}
#endif
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(FlowPort);
	GDREGISTER_CLASS(FlowConnection);
	GDREGISTER_CLASS(FlowFrame);
	GDREGISTER_CLASS(FlowVariable);
	GDREGISTER_CLASS(FlowBlackboard);
	GDREGISTER_CLASS(FlowGraphValidation);
	GDREGISTER_CLASS(FlowNode);
	GDREGISTER_CLASS(FlowGraph);
	GDREGISTER_CLASS(FlowStartNode);
	GDREGISTER_CLASS(FlowEndNode);
	GDREGISTER_CLASS(FlowBranchNode);
	GDREGISTER_CLASS(FlowExpressionNode);
	GDREGISTER_ABSTRACT_CLASS(FlowVariableNode);
	GDREGISTER_CLASS(FlowGetVariableNode);
	GDREGISTER_CLASS(FlowSetVariableNode);
	GDREGISTER_CLASS(FlowEmitEventNode);
	GDREGISTER_CLASS(FlowSubgraphNode);
	GDREGISTER_CLASS(FlowWaitNode);
	GDREGISTER_CLASS(FlowExit);
	GDREGISTER_CLASS(FlowGraphState);
	GDREGISTER_CLASS(FlowGraphPlayback);
	GDREGISTER_CLASS(FlowGraphPlayer);
	GDREGISTER_CLASS(DialogueOption);
	GDREGISTER_CLASS(DialogueLineNode);
	GDREGISTER_CLASS(DialogueChoiceNode);
#ifdef DEBUG_ENABLED
	FlowGraphDebugger::initialize();
#endif
}

void uninitialize_flow_graph_module(ModuleInitializationLevel p_level) {
#ifdef DEBUG_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		FlowGraphDebugger::finalize();
	}
#endif
}
