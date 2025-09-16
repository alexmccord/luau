// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/ControlFlow.h"
#include "Luau/Ast.h"
#include "Luau/DenseHash.h"
#include "Luau/NotNull.h"
#include "Luau/TypedAllocator.h"
#include "Luau/Variant.h"

#include <vector>

namespace Luau
{

struct CfgNode;

struct CfgNodeArena
{
    CfgNodeArena() = default;

    CfgNodeArena(CfgNodeArena&&) = default;
    CfgNodeArena& operator=(CfgNodeArena&&) = default;

    CfgNode* freshNode();

private:
    CfgNodeArena(const CfgNodeArena&) = delete;
    CfgNodeArena& operator=(const CfgNodeArena&) = delete;

    TypedAllocator<CfgNode> nodes;
};

struct Instruction
{
    template<typename T>
    Instruction(T&& node)
        : variant(std::forward<T>(node))
    {
    }

    template<typename T>
    const T* get() const
    {
        return variant.get_if<T>();
    }

private:
    using V = Variant<AstStat*, AstExpr*>;

    V variant;
};

struct CfgContext
{
    CfgContext() = default;

    CfgContext(CfgContext&&) = default;
    CfgContext& operator=(CfgContext&&) = default;

    CfgContext(const CfgContext&) = delete;
    CfgContext& operator=(const CfgContext&) = delete;

public:
    struct FunctionContext
    {
        CfgNode* returnNode;
    };

    struct LoopContext
    {
        CfgNode* entryNode;
        CfgNode* exitNode;
    };

    void enterFunction(CfgNode* returnNode);
    void exitFunction();

    void enterLoop(CfgNode* entryNode, CfgNode* exitNode);
    void exitLoop();

    FunctionContext currentFunctionContext() const;
    std::optional<LoopContext> currentLoopContext() const;

private:
    std::vector<FunctionContext> functionStack;
    std::vector<LoopContext> loopStack;
};

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
struct ControlFlowGraph
{
    ControlFlowGraph(ControlFlowGraph&&) = default;
    ControlFlowGraph& operator=(ControlFlowGraph&&) = default;

    ControlFlowGraph(const ControlFlowGraph&) = delete;
    ControlFlowGraph& operator=(const ControlFlowGraph&) = delete;

public:
    const CfgNode* getNode(AstStat* stat) const;
    const CfgNode* getNode(AstExpr* expr) const;

private:
    ControlFlowGraph();

    friend struct ControlFlowGraphBuilder;

    CfgNodeArena arena;

    /// Any CfgNode that terminates with an exceptional control flow have an
    /// edge to this special sink. If we wanted to handle `pcall()`, we would
    /// just move this into the `CfgContext` with a stack of `sinkNodes`. For
    /// now though, exceptional control flow is global to the entire module.
    CfgNode* sinkNode;
    CfgContext context;

    DenseHashMap<AstStat*, CfgNode*> stats{nullptr};
    DenseHashMap<AstExpr*, CfgNode*> exprs{nullptr};

    CfgNode* freshNode();

    void bind(CfgNode* node, ControlFlow controlFlow, AstStat* stat);
    void bind(CfgNode* node, ControlFlow controlFlow, AstExpr* expr);
    void bind(CfgNode* node, ControlFlow controlFlow, Instruction instruction);
};

/// In compilers with a traditional IR, they can implement a control flow graph
/// and retain the name `BasicBlock` because the IR just contains a sequence of
/// instructions, not statements _and_ expressions.
///
/// In Luau, we don't lower the AST into an IR before doing any analysis. On top
/// of that, since expressions can induce abnormal control flow, we really would
/// like to avoid the connotation of a statement-oriented world view.
///
/// A trivial example:
///
/// ```luau
/// local result = f() -- Result<number, string>
/// local value = if result.type == "ok"
///     then result.value
///     else error("...") -- this implies `result : Ok<number>`
///
/// -- due to the control flow of the previous expression, this is well-typed
/// local again = result.value
/// ```
///
/// Beyond the name, everything about it is the same as if it were called
/// `BasicBlock`. No fundamental difference.
struct CfgNode
{
    explicit CfgNode() = default;

    CfgNode(const CfgNode&) = delete;
    CfgNode& operator=(const CfgNode&) = delete;

public:
    void addAntecedent(CfgNode* antecedent);
    void addConsequent(CfgNode* consequent);
    void addInstruction(Instruction instruction);

private:
    std::vector<CfgNode*> antecedentNodes;
    std::vector<CfgNode*> consequentNodes;
    std::vector<Instruction> instructions;
};

struct ControlFlowGraphBuilder
{
    static ControlFlowGraph build(AstStatBlock* body, NotNull<struct InternalErrorReporter> handle);

private:
    ControlFlowGraphBuilder(NotNull<struct InternalErrorReporter> handle);

    ControlFlowGraphBuilder(ControlFlowGraphBuilder&&) = default;
    ControlFlowGraphBuilder& operator=(ControlFlowGraphBuilder&&) = default;

    ControlFlowGraphBuilder(const ControlFlowGraphBuilder&) = delete;
    ControlFlowGraphBuilder& operator=(const ControlFlowGraphBuilder&) = delete;

    ControlFlowGraph cfg;
    NotNull<struct InternalErrorReporter> handle;

private:
    CfgNode* visitStat(CfgNode* node, AstStat* stat);
    CfgNode* visitExpr(CfgNode* node, AstExpr* expr);

    CfgNode* visitStatImpl(CfgNode* node, AstStat* stat);
    CfgNode* visitStatImpl(CfgNode* node, AstStatBlock* block);
    CfgNode* visitStatImpl(CfgNode* node, AstStatIf* branch);
    CfgNode* visitStatImpl(CfgNode* node, AstStatWhile* loop);
    CfgNode* visitStatImpl(CfgNode* node, AstStatRepeat* repeat);
    CfgNode* visitStatImpl(CfgNode* node, AstStatBreak* brk);
    CfgNode* visitStatImpl(CfgNode* node, AstStatContinue* cont);
    CfgNode* visitStatImpl(CfgNode* node, AstStatReturn* ret);
    CfgNode* visitStatImpl(CfgNode* node, AstStatExpr* expr);
    CfgNode* visitStatImpl(CfgNode* node, AstStatLocal* local);
    CfgNode* visitStatImpl(CfgNode* node, AstStatFor* forRange);
    CfgNode* visitStatImpl(CfgNode* node, AstStatForIn* forIter);
    CfgNode* visitStatImpl(CfgNode* node, AstStatAssign* assign);
    CfgNode* visitStatImpl(CfgNode* node, AstStatCompoundAssign* compound);
    CfgNode* visitStatImpl(CfgNode* node, AstStatFunction* function);
    CfgNode* visitStatImpl(CfgNode* node, AstStatLocalFunction* function);
    CfgNode* visitStatImpl(CfgNode* node, AstStatTypeAlias* alias);
    CfgNode* visitStatImpl(CfgNode* node, AstStatTypeFunction* function);
    CfgNode* visitStatImpl(CfgNode* node, AstStatDeclareGlobal* ambient);
    CfgNode* visitStatImpl(CfgNode* node, AstStatDeclareFunction* ambient);
    CfgNode* visitStatImpl(CfgNode* node, AstStatDeclareExternType* ambient);
    CfgNode* visitStatImpl(CfgNode* node, AstStatError* error);

    CfgNode* visitExprImpl(CfgNode* node, AstExpr* expr);
    CfgNode* visitExprImpl(CfgNode* node, AstExprGroup* group);
    CfgNode* visitExprImpl(CfgNode* node, AstExprCall* call);
    CfgNode* visitExprImpl(CfgNode* node, AstExprIndexName* index);
    CfgNode* visitExprImpl(CfgNode* node, AstExprIndexExpr* subscript);
    CfgNode* visitExprImpl(CfgNode* node, AstExprFunction* function);
    CfgNode* visitExprImpl(CfgNode* node, AstExprTable* table);
    CfgNode* visitExprImpl(CfgNode* node, AstExprUnary* unary);
    CfgNode* visitExprImpl(CfgNode* node, AstExprBinary* binary);
    CfgNode* visitExprImpl(CfgNode* node, AstExprTypeAssertion* typeAssertion);
    CfgNode* visitExprImpl(CfgNode* node, AstExprIfElse* ifElse);
    CfgNode* visitExprImpl(CfgNode* node, AstExprInterpString* interpolation);
    CfgNode* visitExprImpl(CfgNode* node, AstExprError* error);

    void visitType(AstType* type);
    void visitType(AstTypeReference* ref);
    void visitType(AstTypeTable* table);
    void visitType(AstTypeFunction* function);
    void visitType(AstTypeTypeof* typeofTerm);
    void visitType(AstTypeUnion* disjunction);
    void visitType(AstTypeIntersection* conjunction);
    void visitType(AstTypeError* error);

    void visitTypePack(AstTypePack* pack);
    void visitTypePack(AstTypePackExplicit* stack);
    void visitTypePack(AstTypePackVariadic* homogeneous);

    void visitTypeList(AstTypeList list);

    void visitGenerics(AstArray<AstGenericType*> params);
    void visitGenericPacks(AstArray<AstGenericTypePack*> params);
};

} // namespace Luau
