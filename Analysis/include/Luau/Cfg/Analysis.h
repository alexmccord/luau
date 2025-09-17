// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Graph.h"
#include "Luau/Ast.h"
#include "Luau/Cfg/Node.h"
#include "Luau/Cfg/Graph.h"
#include "Luau/NotNull.h"

namespace Luau
{

struct ControlFlowAnalysis
{
    ControlFlowAnalysis(NotNull<struct InternalErrorReporter> handle);

    ControlFlowAnalysis(ControlFlowAnalysis&&) = default;
    ControlFlowAnalysis& operator=(ControlFlowAnalysis&&) = default;

    ControlFlowAnalysis(const ControlFlowAnalysis&) = delete;
    ControlFlowAnalysis& operator=(const ControlFlowAnalysis&) = delete;

public:
    NotNull<ControlFlowGraph> build(AstStatBlock* root);

    ControlFlowGraph* get(AstStat* stat) const;
    ControlFlowGraph* get(AstExpr* expr) const;

private:
    NotNull<ControlFlowGraph> prototype(AstNode* node);

private:
    NotNull<struct InternalErrorReporter> handle;

    CfgNodeArena arena;
    std::vector<std::unique_ptr<ControlFlowGraph>> cfgs;
    DenseHashMap<AstNode*, ControlFlowGraph*> functionCfgs{nullptr};
};

} // namespace Luau
