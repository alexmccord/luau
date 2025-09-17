// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Ast.h"
#include "Luau/Cfg/Node.h"
#include "Luau/DenseHash.h"

namespace Luau
{

/// A control flow graph is a disconnected directed graph that may contain cycles.
///
/// The graph can be disconnected when crossing function boundaries and modules.
///
/// In typical cases, any cycles are of length >=2. Degenerate cases could happen
/// that causes the cycle length to be 1, such as this program:
/// ```luau
/// -- N0
/// while true do -- N1
/// end
/// -- N2
/// ```
/// We obtain this CFG:
/// ```
/// N0 -> {N1, N2}
/// N1 -> {N1, N2}
/// ```
///
/// It is my hope that, eventually, you can say `cfg->markUnreachable(expr)` while
/// the constraint solver is running. For instance, given this program:
/// ```
/// local x = if math.random() > 0.5 then 5 else "hello"
///
/// local y
/// if typeof(x) == "number" and typeof(x) == "string" then
///     y = 7
/// else
///     y = "world!"
/// end
/// ```
/// Syntactically, it would appear that `y` is definitely assigned to, which makes
/// it look like it's `y : number | string`, but later by reductio ad absurdum, we
/// would refine to simply just `y : string`.
struct ControlFlowGraph
{
    ControlFlowGraph() = default;

    ControlFlowGraph(ControlFlowGraph&&) = default;
    ControlFlowGraph& operator=(ControlFlowGraph&&) = default;

    ControlFlowGraph(const ControlFlowGraph&) = delete;
    ControlFlowGraph& operator=(const ControlFlowGraph&) = delete;

public:
    const CfgNode* getNode(AstStat* stat) const;
    const CfgNode* getNode(AstExpr* expr) const;

private:
    DenseHashMap<AstNode*, CfgNode*> nodes{nullptr};
};

} // namespace Luau
