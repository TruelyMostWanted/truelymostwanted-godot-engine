def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "FlowBlackboard",
        "FlowBranchNode",
        "FlowConnection",
        "FlowEmitEventNode",
        "FlowEndNode",
        "FlowExit",
        "FlowExpressionNode",
        "FlowFrame",
        "FlowGetVariableNode",
        "FlowGraph",
        "FlowGraphPlayback",
        "FlowGraphPlayer",
        "FlowGraphState",
        "FlowGraphValidation",
        "FlowNode",
        "FlowPort",
        "FlowSetVariableNode",
        "FlowStartNode",
        "FlowSubgraphNode",
        "FlowVariable",
        "FlowVariableNode",
        "FlowWaitNode",
    ]


def get_doc_path():
    return "doc_classes"
