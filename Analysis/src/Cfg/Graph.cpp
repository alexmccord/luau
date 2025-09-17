// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Cfg/Graph.h"

#include "Luau/Ast.h"
#include "Luau/Common.h"

namespace Luau
{

const CfgNode* ControlFlowGraph::getNode(AstStat* stat) const
{
    auto node = nodes.find(stat);
    LUAU_ASSERT(node);
    return *node;
}

const CfgNode* ControlFlowGraph::getNode(AstExpr* expr) const
{
    auto node = nodes.find(expr);
    LUAU_ASSERT(node);
    return *node;
}

} // namespace Luau
