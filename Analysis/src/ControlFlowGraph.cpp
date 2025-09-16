// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/ControlFlowGraph.h"

#include "Luau/Ast.h"
#include "Luau/Common.h"
#include "Luau/ControlFlow.h"
#include "Luau/Error.h"
#include "Luau/NotNull.h"

#include <optional>

namespace Luau
{

CfgNode* CfgNodeArena::freshNode()
{
    return nodes.allocate(CfgNode{});
}

void CfgContext::enterFunction(CfgNode* returnNode)
{
    functionStack.push_back(FunctionContext{returnNode});
}

void CfgContext::exitFunction()
{
    functionStack.pop_back();
}

void CfgContext::enterLoop(CfgNode* entryNode, CfgNode* exitNode)
{
    loopStack.push_back(LoopContext{entryNode, exitNode});
}

void CfgContext::exitLoop()
{
    loopStack.pop_back();
}

CfgContext::FunctionContext CfgContext::currentFunctionContext() const
{
    LUAU_ASSERT(!functionStack.empty());
    return functionStack.back();
}

std::optional<CfgContext::LoopContext> CfgContext::currentLoopContext() const
{
    if (loopStack.empty())
        return std::nullopt;

    return loopStack.back();
}

ControlFlowGraph::ControlFlowGraph()
{
    sinkNode = arena.freshNode();
}

const CfgNode* ControlFlowGraph::getNode(AstStat* stat) const
{
    auto node = stats.find(stat);
    LUAU_ASSERT(node);
    return *node;
}

const CfgNode* ControlFlowGraph::getNode(AstExpr* expr) const
{
    auto node = exprs.find(expr);
    LUAU_ASSERT(node);
    return *node;
}

CfgNode* ControlFlowGraph::freshNode()
{
    return arena.freshNode();
}

void ControlFlowGraph::bind(CfgNode* node, ControlFlow controlFlow, AstStat* stat)
{
    return bind(node, controlFlow, Instruction{stat});
}

void ControlFlowGraph::bind(CfgNode* node, ControlFlow controlFlow, AstExpr* expr)
{
    return bind(node, controlFlow, Instruction{expr});
}

void ControlFlowGraph::bind(CfgNode* node, ControlFlow controlFlow, Instruction instruction)
{
    // The statement S trivially flows to S'.
    //
    // N0:
    // { -- start of instructions for N0
    //   local x = 5
    // } -- end of instructions for N0
    //   local y = 7
    //
    // We simply expand the node with this new instruction.
    //
    // N0:
    // { -- start of instructions for N0
    //   local x = 5
    //   local y = 7
    // } -- end of instructions for N0
    node->addInstruction(instruction);

    switch (controlFlow)
    {
    case ControlFlow::None:
        return;
    case ControlFlow::Returns:
    {
        // The statement S returns to the function prologue.

        auto functionContext = context.currentFunctionContext();
        return node->addConsequent(functionContext.returnNode);
    }
    case ControlFlow::Throws:
    {
        // The statement S throws an exception, terminating the current
        // protected call environment.

        return node->addConsequent(sinkNode);
    }
    case ControlFlow::Breaks:
    {
        auto loopContext = context.currentLoopContext();
        LUAU_ASSERT(loopContext);
        return node->addConsequent(loopContext->exitNode);
    }
    case ControlFlow::Continues:
    {
        auto loopContext = context.currentLoopContext();
        LUAU_ASSERT(loopContext);
        return node->addConsequent(loopContext->entryNode);
    }
    default:
        LUAU_UNREACHABLE();
        // TODO: handle->ice() in ControlFlowGraph.
    }
}

void CfgNode::addAntecedent(CfgNode* antecedent)
{
    // Symmetry.
    antecedent->addConsequent(this);
}

void CfgNode::addConsequent(CfgNode* consequent)
{
    consequentNodes.push_back(consequent);
    consequent->antecedentNodes.push_back(this);
}

ControlFlowGraphBuilder::ControlFlowGraphBuilder(NotNull<struct InternalErrorReporter> handle)
    : handle(handle)
{
}

ControlFlowGraph ControlFlowGraphBuilder::build(AstStatBlock* body, NotNull<struct InternalErrorReporter> handle)
{
    ControlFlowGraphBuilder builder(handle);

    CfgNode* node = builder.cfg.freshNode();
    builder.visitStat(node, body);

    return std::move(builder.cfg);
}

CfgNode* ControlFlowGraphBuilder::visitStat(CfgNode* node, AstStat* stat)
{
    cfg.stats[stat] = node;

    CfgNode* consequent = visitStatImpl(node, stat);

    return consequent;
}

CfgNode* ControlFlowGraphBuilder::visitExpr(CfgNode* node, AstExpr* expr)
{
    cfg.exprs[expr] = node;

    CfgNode* consequent = visitExprImpl(node, expr);

    return consequent;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStat* stat)
{
    if (auto block = stat->as<AstStatBlock>())
        return visitStatImpl(node, block);
    else if (auto branch = stat->as<AstStatIf>())
        return visitStatImpl(node, branch);
    else if (auto loop = stat->as<AstStatWhile>())
        return visitStatImpl(node, loop);
    else if (auto repeat = stat->as<AstStatRepeat>())
        return visitStatImpl(node, repeat);
    else if (auto brk = stat->as<AstStatBreak>())
        return visitStatImpl(node, brk);
    else if (auto cont = stat->as<AstStatContinue>())
        return visitStatImpl(node, cont);
    else if (auto ret = stat->as<AstStatReturn>())
        return visitStatImpl(node, ret);
    else if (auto expr = stat->as<AstStatExpr>())
        return visitStatImpl(node, expr);
    else if (auto local = stat->as<AstStatLocal>())
        return visitStatImpl(node, local);
    else if (auto forRange = stat->as<AstStatFor>())
        return visitStatImpl(node, forRange);
    else if (auto forIter = stat->as<AstStatForIn>())
        return visitStatImpl(node, forIter);
    else if (auto assign = stat->as<AstStatAssign>())
        return visitStatImpl(node, assign);
    else if (auto compound = stat->as<AstStatCompoundAssign>())
        return visitStatImpl(node, compound);
    else if (auto function = stat->as<AstStatFunction>())
        return visitStatImpl(node, function);
    else if (auto function = stat->as<AstStatLocalFunction>())
        return visitStatImpl(node, function);
    else if (auto alias = stat->as<AstStatTypeAlias>())
        return visitStatImpl(node, alias);
    else if (auto function = stat->as<AstStatTypeFunction>())
        return visitStatImpl(node, function);
    else if (auto ambient = stat->as<AstStatDeclareGlobal>())
        return visitStatImpl(node, ambient);
    else if (auto ambient = stat->as<AstStatDeclareFunction>())
        return visitStatImpl(node, ambient);
    else if (auto ambient = stat->as<AstStatDeclareExternType>())
        return visitStatImpl(node, ambient);
    else if (auto error = stat->as<AstStatError>())
        return visitStatImpl(node, error);
    else
        handle->ice("Unknown AstStat in ControlFlowGraphBuilder::visitStatImpl");
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatBlock* block)
{
    for (AstStat* stat : block->body)
        node = visitStat(node, stat);

    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatIf* branch) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatWhile* loop) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatRepeat* repeat) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatBreak* brk)
{
    auto loopContext = cfg.context.currentLoopContext();
    LUAU_ASSERT(loopContext);
    node->addConsequent(loopContext->exitNode);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatContinue* cont)
{
    auto loopContext = cfg.context.currentLoopContext();
    LUAU_ASSERT(loopContext);
    node->addConsequent(loopContext->entryNode);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatReturn* ret) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatExpr* expr)
{
    return visitExpr(node, expr->expr);
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatLocal* local) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatFor* forRange) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatForIn* forIter) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatAssign* assign) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatCompoundAssign* compound) {}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatFunction* function)
{
    visitExpr(node, function->name);
    visitExpr(node, function->func);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatLocalFunction* function)
{
    visitExpr(node, function->func);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatTypeAlias* alias)
{
    visitGenerics(alias->generics);
    visitGenericPacks(alias->genericPacks);
    visitType(alias->type);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatTypeFunction* function)
{
    visitExpr(node, function->body);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatDeclareGlobal* ambient)
{
    visitType(ambient->type);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatDeclareFunction* ambient)
{
    visitGenerics(ambient->generics);
    visitGenericPacks(ambient->genericPacks);
    visitTypeList(ambient->params);
    visitTypePack(ambient->retTypes);
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatDeclareExternType* ambient)
{
    for (const AstDeclaredExternTypeProperty& prop : ambient->props)
        visitType(prop.ty);

    if (ambient->indexer)
    {
        visitType(ambient->indexer->indexType);
        visitType(ambient->indexer->resultType);
    }

    return node;
}

CfgNode* ControlFlowGraphBuilder::visitStatImpl(CfgNode* node, AstStatError* error)
{
    CfgNode* unreachable = cfg.freshNode();
    node->addConsequent(unreachable);

    for (AstStat* stat : error->statements)
        visitStat(unreachable, stat);

    for (AstExpr* expr : error->expressions)
        visitExpr(unreachable, expr);

    return node;
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExpr* expr)
{
    if (auto group = expr->as<AstExprGroup>())
        return visitExprImpl(node, group);
    else if (expr->is<AstExprConstantNil>())
        return node;
    else if (expr->is<AstExprConstantBool>())
        return node;
    else if (expr->is<AstExprConstantNumber>())
        return node;
    else if (expr->is<AstExprConstantString>())
        return node;
    else if (expr->is<AstExprLocal>())
        return node;
    else if (expr->is<AstExprGlobal>())
        return node;
    else if (expr->is<AstExprVarargs>())
        return node;
    else if (auto call = expr->as<AstExprCall>())
        return visitExprImpl(node, call);
    else if (auto index = expr->as<AstExprIndexName>())
        return visitExprImpl(node, index);
    else if (auto subscript = expr->as<AstExprIndexExpr>())
        return visitExprImpl(node, subscript);
    else if (auto function = expr->as<AstExprFunction>())
        return visitExprImpl(node, function);
    else if (auto table = expr->as<AstExprTable>())
        return visitExprImpl(node, table);
    else if (auto unary = expr->as<AstExprUnary>())
        return visitExprImpl(node, unary);
    else if (auto binary = expr->as<AstExprBinary>())
        return visitExprImpl(node, binary);
    else if (auto typeAssertion = expr->as<AstExprTypeAssertion>())
        return visitExprImpl(node, typeAssertion);
    else if (auto branch = expr->as<AstExprIfElse>())
        return visitExprImpl(node, branch);
    else if (auto interpolation = expr->as<AstExprInterpString>())
        return visitExprImpl(node, interpolation);
    else if (auto error = expr->as<AstExprError>())
        return visitExprImpl(node, error);
    else
        handle->ice("Unknown AstExpr in ControlFlowGraphBuilder::visitExprImpl");
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprGroup* group)
{
    return visitExpr(node, group->expr);
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprCall* call) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprIndexName* index) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprIndexExpr* subscript) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprFunction* function)
{
    return node;
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprTable* table) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprUnary* unary)
{
    return visitExpr(node, unary->expr);
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprBinary* binary)
{
    // `and`/`or` are short-circuiting, which makes it non-linear. That requires
    // a fresh node.
    //
    // For all other operators, they are not short-circuiting, making them linear
    // and can be folded into the same node.
    switch (binary->op)
    {
    case AstExprBinary::And:
    case AstExprBinary::Or:
    {

        break;
    }
    case AstExprBinary::Add:
    case AstExprBinary::Sub:
    case AstExprBinary::Mul:
    case AstExprBinary::Div:
    case AstExprBinary::FloorDiv:
    case AstExprBinary::Mod:
    case AstExprBinary::Pow:
    case AstExprBinary::Concat:
    case AstExprBinary::CompareNe:
    case AstExprBinary::CompareEq:
    case AstExprBinary::CompareLt:
    case AstExprBinary::CompareLe:
    case AstExprBinary::CompareGt:
    case AstExprBinary::CompareGe:
    {
        CfgNode* leftNode = visitExpr(node, binary->left);
        return visitExpr(leftNode, binary->right);
    }
    case AstExprBinary::Op__Count:
        handle->ice("Op__Count should never be generated in an AST.");
    }
}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprTypeAssertion* typeAssertion) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprIfElse* branch) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprInterpString* interpolation) {}

CfgNode* ControlFlowGraphBuilder::visitExprImpl(CfgNode* node, AstExprError* error)
{
    CfgNode* unreachable = cfg.freshNode();
    node->addConsequent(unreachable);

    for (AstExpr* expr : error->expressions)
        visitExpr(unreachable, expr);

    return node;
}

void ControlFlowGraphBuilder::visitType(AstType* type)
{
    if (auto ref = type->as<AstTypeReference>())
        return visitType(ref);
    else if (auto table = type->as<AstTypeTable>())
        return visitType(table);
    else if (auto function = type->as<AstTypeFunction>())
        return visitType(function);
    else if (auto typeofTerm = type->as<AstTypeTypeof>())
        return visitType(typeofTerm);
    else if (auto disjunction = type->as<AstTypeUnion>())
        return visitType(disjunction);
    else if (auto conjunction = type->as<AstTypeIntersection>())
        return visitType(conjunction);
    else if (auto error = type->as<AstTypeError>())
        return visitType(error);
    else
        handle->ice("Unknown AstType in ControlFlowGraphBuilder::visitType");
}

void ControlFlowGraphBuilder::visitType(AstTypeReference* ref)
{
    for (AstTypeOrPack parameter : ref->parameters)
    {
        if (parameter.type)
            visitType(parameter.type);
        else
            visitTypePack(parameter.typePack);
    }
}

void ControlFlowGraphBuilder::visitType(AstTypeTable* table)
{
    for (const AstTableProp& prop : table->props)
        visitType(prop.type);

    if (table->indexer)
    {
        visitType(table->indexer->indexType);
        visitType(table->indexer->resultType);
    }
}

void ControlFlowGraphBuilder::visitType(AstTypeFunction* function)
{
    visitGenerics(function->generics);
    visitGenericPacks(function->genericPacks);
    visitTypeList(function->argTypes);

    if (function->returnTypes)
        visitTypePack(function->returnTypes);
}

void ControlFlowGraphBuilder::visitType(AstTypeTypeof* typeofTerm)
{
    // Currently, it is intended that there is no incoming edge to this node. It
    // is unclear what we'd gain or lose in threading at least the antecendent
    // node. It's trivial to retrofit anyhow.
    //
    // One thing is clear, though: it must not have any outgoing edges. Period.
    // We probably would model this with an explicit unreachable instruction or
    // something. Still working out the design.

    visitExpr(cfg.freshNode(), typeofTerm->expr);
}

void ControlFlowGraphBuilder::visitType(AstTypeUnion* disjunction)
{
    for (AstType* t : disjunction->types)
        visitType(t);
}

void ControlFlowGraphBuilder::visitType(AstTypeIntersection* conjunction)
{
    for (AstType* t : conjunction->types)
        visitType(t);
}

void ControlFlowGraphBuilder::visitType(AstTypeError* error)
{
    for (AstType* type : error->types)
        visitType(type);
}

void ControlFlowGraphBuilder::visitTypePack(AstTypePack* pack)
{
    if (auto stack = pack->as<AstTypePackExplicit>())
        return visitTypePack(stack);
    else if (auto homogeneous = pack->as<AstTypePackVariadic>())
        return visitTypePack(homogeneous);
    else if (pack->is<AstTypePackGeneric>())
        return;
    else
        handle->ice("Unknown AstTypePack in ControlFlowGraphBuilder::visitTypePack");
}

void ControlFlowGraphBuilder::visitTypePack(AstTypePackExplicit* stack)
{
    visitTypeList(stack->typeList);
}

void ControlFlowGraphBuilder::visitTypePack(AstTypePackVariadic* homogeneous)
{
    visitType(homogeneous->variadicType);
}

void ControlFlowGraphBuilder::visitTypeList(AstTypeList list)
{
    for (AstType* t : list.types)
        visitType(t);

    if (list.tailType)
        visitTypePack(list.tailType);
}

void ControlFlowGraphBuilder::visitGenerics(AstArray<AstGenericType*> params)
{
    for (AstGenericType* param : params)
    {
        if (param->defaultValue)
            visitType(param->defaultValue);
    }
}

void ControlFlowGraphBuilder::visitGenericPacks(AstArray<AstGenericTypePack*> params)
{
    for (AstGenericTypePack* param : params)
    {
        if (param->defaultValue)
            visitTypePack(param->defaultValue);
    }
}

} // namespace Luau
