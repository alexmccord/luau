// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Cfg/Analysis.h"

#include "Luau/Ast.h"
#include "Luau/Cfg/Graph.h"
#include "Luau/Cfg/Instruction.h"
#include "Luau/Common.h"
#include "Luau/Error.h"

namespace Luau
{

struct CfgBuilderState
{
    NotNull<CfgNodeArena> arena;
    NotNull<std::vector<AstExprFunction*>> pendingFunctions;
    NotNull<struct InternalErrorReporter> handle;
};

struct CfgBuilder
{
    NotNull<ControlFlowGraph> cfg;
    NotNull<CfgBuilderState> state;

    CfgBuilder(CfgBuilder&&) = default;
    CfgBuilder& operator=(CfgBuilder&&) = default;

    CfgBuilder(const CfgBuilder&) = delete;
    CfgBuilder& operator=(const CfgBuilder&) = delete;

    void traverse(CfgNode* entry, AstStatBlock* block)
    {
        visitStat(entry, block);
    }

    void traverse(AstExprFunction* function)
    {
        CfgNode* entry = state->arena->freshNode();
        visitGenerics(entry, function->generics);
        visitGenericPacks(entry, function->genericPacks);

        for (AstLocal* param : function->args)
        {
            if (param->annotation)
                visitType(entry, param->annotation);
        }

        if (function->returnAnnotation)
            visitTypePack(entry, function->returnAnnotation);

        traverse(entry, function->body);
    }

private:
    CfgNode* visitStat(CfgNode* entry, AstStat* stat)
    {
        if (auto block = stat->as<AstStatBlock>())
            return visitStatImpl(entry, block);
        else if (auto branch = stat->as<AstStatIf>())
            return visitStatImpl(entry, branch);
        else if (auto loop = stat->as<AstStatWhile>())
            return visitStatImpl(entry, loop);
        else if (auto repeat = stat->as<AstStatRepeat>())
            return visitStatImpl(entry, repeat);
        else if (auto brk = stat->as<AstStatBreak>())
            return visitStatImpl(entry, brk);
        else if (auto cont = stat->as<AstStatContinue>())
            return visitStatImpl(entry, cont);
        else if (auto ret = stat->as<AstStatReturn>())
            return visitStatImpl(entry, ret);
        else if (auto expr = stat->as<AstStatExpr>())
            return visitStatImpl(entry, expr);
        else if (auto local = stat->as<AstStatLocal>())
            return visitStatImpl(entry, local);
        else if (auto forRange = stat->as<AstStatFor>())
            return visitStatImpl(entry, forRange);
        else if (auto forIter = stat->as<AstStatForIn>())
            return visitStatImpl(entry, forIter);
        else if (auto assign = stat->as<AstStatAssign>())
            return visitStatImpl(entry, assign);
        else if (auto compound = stat->as<AstStatCompoundAssign>())
            return visitStatImpl(entry, compound);
        else if (auto function = stat->as<AstStatFunction>())
            return visitStatImpl(entry, function);
        else if (auto function = stat->as<AstStatLocalFunction>())
            return visitStatImpl(entry, function);
        else if (auto alias = stat->as<AstStatTypeAlias>())
            return visitStatImpl(entry, alias);
        else if (auto function = stat->as<AstStatTypeFunction>())
            return visitStatImpl(entry, function);
        else if (auto ambient = stat->as<AstStatDeclareGlobal>())
            return visitStatImpl(entry, ambient);
        else if (auto ambient = stat->as<AstStatDeclareFunction>())
            return visitStatImpl(entry, ambient);
        else if (auto ambient = stat->as<AstStatDeclareExternType>())
            return visitStatImpl(entry, ambient);
        else if (auto error = stat->as<AstStatError>())
            return visitStatImpl(entry, error);
        else
            state->handle->ice("Unknown AstStat in CfgBuilder::visitStat");
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatBlock* block)
    {
        // We do not add the `do` block itself as an instruction. It is not one.

        CfgNode* exit = entry;
        for (AstStat* stat : block->body)
            exit = visitStat(exit, stat);

        return exit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatIf* branch)
    {
        // entry -> {then, else}
        // then -> {exit}
        // else -> {exit}

        entry = visitExpr(entry, branch->condition);

        CfgNode* exit = state->arena->freshNode();

        CfgNode* thenEntry = state->arena->freshNode();
        entry->addConsequent(thenEntry);

        CfgNode* thenExit = visitStat(thenEntry, branch->thenbody);
        exit->addAntecedent(thenExit);

        if (branch->elsebody)
        {
            CfgNode* elseEntry = state->arena->freshNode();
            entry->addConsequent(elseEntry);

            CfgNode* elseExit = visitStat(elseEntry, branch->elsebody);
            exit->addAntecedent(elseExit);
        }
        else
            exit->addAntecedent(entry);

        return exit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatWhile* loop)
    {
        // entry -> {body, exit}
        // body  -> {entry}

        entry = visitExpr(entry, loop->condition);

        CfgNode* body = visitStat(entry, loop->body);

        CfgNode* exit = state->arena->freshNode();
        entry->addConsequent(body);
        entry->addConsequent(exit);

        return exit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatRepeat* repeat)
    {
        // entry -> {body}
        // body  -> {cond}
        // cond  -> {body, exit}

        CfgNode* loopEntry = state->arena->freshNode();
        loopEntry->addAntecedent(entry);

        CfgNode* bodyExit = visitStat(loopEntry, repeat->body);
        CfgNode* loopExit = visitExpr(bodyExit, repeat->condition);
        loopExit->addAntecedent(bodyExit);
        loopExit->addConsequent(entry);

        CfgNode* exitNode = state->arena->freshNode();
        exitNode->addAntecedent(loopExit);

        return exitNode;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatBreak* brk)
    {
        entry->addInstruction(Instruction::Stat{brk});
        return nullptr; // TODO: A stack of loops
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatContinue* cont)
    {
        entry->addInstruction(Instruction::Stat{cont});
        return nullptr; // TODO: A stack of loops
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatReturn* ret)
    {
        entry->addInstruction(Instruction::Stat{ret});
        return nullptr; // TODO: A way to get the
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatExpr* expr)
    {
        // This is an instruction because we have to clean up the stack garbage
        // left behind the function call.

        CfgNode* exit = visitExpr(entry, expr->expr);
        exit->addInstruction(Instruction::Stat{expr});
        return exit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatLocal* local)
    {
        // This is an instruction because we have to right-pad with nil or trim
        // the excess fat by the tail expression.

        for (AstLocal* binding : local->vars)
        {
            if (binding->annotation)
                visitType(entry, binding->annotation);
        }

        CfgNode* exit = entry;
        for (AstExpr* expr : local->values)
            exit = visitExpr(exit, expr);

        entry->addInstruction(Instruction::Stat{local});
        return exit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatFor* forRange)
    {
        // The from/to/step are evaluated only once, so they actually belong to
        // the entry node. The loop then increments by the given step (or 1) and
        // enters the body. This is another use case for the IR: to insert an
        // explicit "step" node that always exists, and makes the assignment to
        // the loop variable for type states. This program is well-typed because
        // the loop literally overwrites `i` with the new value before entering
        // the loop body.
        //
        // for i = 1, 10 do
        //   local x: number = i
        //   i = "a"
        // end

        if (forRange->var->annotation)
            visitType(entry, forRange->var->annotation);

        CfgNode* from = visitExpr(entry, forRange->from);
        CfgNode* to = visitExpr(from, forRange->to);
        CfgNode* step = nullptr;
        if (forRange->step)
            step = visitExpr(to, forRange->step);

        CfgNode* loopEntry = state->arena->freshNode();
        CfgNode* loopExit = state->arena->freshNode();

        loopEntry->addAntecedent(step ? step : to);
        loopEntry->addConsequent(loopExit);

        CfgNode* body = visitStat(loopEntry, forRange->body);
        body->addConsequent(loopEntry);

        return loopExit;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatForIn* forIter)
    {
        // TODO: Think about the instruction-ness of this statement. It is
        // nontrivial. We will stub this out for now.

        return nullptr;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatAssign* assign)
    {
        // This is an instruction because we have to assign.

        entry->addInstruction(Instruction::Stat{assign});
        return nullptr;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatCompoundAssign* compound)
    {
        // This is an instruction because we have to assign.

        entry->addInstruction(Instruction::Stat{compound});
        return nullptr;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatFunction* function)
    {
        // This is an instruction because we have to assign.

        entry->addInstruction(Instruction::Stat{function});

        CfgNode* name = visitExpr(entry, function->name);
        return visitExpr(name, function->func);
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatLocalFunction* function)
    {
        // This is an instruction because we have to assign.

        entry->addInstruction(Instruction::Stat{function});

        return visitExpr(entry, function->func);
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatTypeAlias* alias)
    {
        visitGenerics(entry, alias->generics);
        visitGenericPacks(entry, alias->genericPacks);
        visitType(entry, alias->type);
        return entry;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatTypeFunction* function)
    {
        visitExpr(entry, function->body);
        return entry;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatDeclareGlobal* ambient)
    {
        visitType(entry, ambient->type);
        return entry;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatDeclareFunction* ambient)
    {
        visitGenerics(entry, ambient->generics);
        visitGenericPacks(entry, ambient->genericPacks);
        visitTypeList(entry, ambient->params);
        visitTypePack(entry, ambient->retTypes);
        return entry;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatDeclareExternType* ambient)
    {
        for (const AstDeclaredExternTypeProperty& prop : ambient->props)
            visitType(entry, prop.ty);

        if (ambient->indexer)
        {
            visitType(entry, ambient->indexer->indexType);
            visitType(entry, ambient->indexer->resultType);
        }

        return entry;
    }

    CfgNode* visitStatImpl(CfgNode* entry, AstStatError* error)
    {
        CfgNode* unreachable = state->arena->freshNode();
        unreachable->addAntecedent(entry);
        unreachable->addInstruction(Instruction::Unreachable{});

        for (AstStat* stat : error->statements)
            visitStat(unreachable, stat);

        for (AstExpr* expr : error->expressions)
            visitExpr(unreachable, expr);

        return entry;
    }

    CfgNode* visitExpr(CfgNode* entry, AstExpr* expr)
    {
        if (auto group = expr->as<AstExprGroup>())
            return visitExprImpl(entry, group);
        else if (expr->is<AstExprConstantNil>())
            return entry;
        else if (expr->is<AstExprConstantBool>())
            return entry;
        else if (expr->is<AstExprConstantNumber>())
            return entry;
        else if (expr->is<AstExprConstantString>())
            return entry;
        else if (expr->is<AstExprLocal>())
            return entry;
        else if (expr->is<AstExprGlobal>())
        {
            // Technically, globals are always _potentially divergent_ or should
            // be encoded as if going through `_G`'s `__index` metamethod, but
            // if we did that, it would make analysis intractible if we applied
            // that worst-case scenario to everything. So we have to assume pure
            // sequential computation.

            return entry;
        }
        else if (expr->is<AstExprVarargs>())
            return entry;
        else if (auto call = expr->as<AstExprCall>())
            return visitExprImpl(entry, call);
        else if (auto index = expr->as<AstExprIndexName>())
            return visitExprImpl(entry, index);
        else if (auto subscript = expr->as<AstExprIndexExpr>())
            return visitExprImpl(entry, subscript);
        else if (auto function = expr->as<AstExprFunction>())
            return visitExprImpl(entry, function);
        else if (auto table = expr->as<AstExprTable>())
            return visitExprImpl(entry, table);
        else if (auto unary = expr->as<AstExprUnary>())
            return visitExprImpl(entry, unary);
        else if (auto binary = expr->as<AstExprBinary>())
            return visitExprImpl(entry, binary);
        else if (auto typeAssertion = expr->as<AstExprTypeAssertion>())
            return visitExprImpl(entry, typeAssertion);
        else if (auto branch = expr->as<AstExprIfElse>())
            return visitExprImpl(entry, branch);
        else if (auto interpolation = expr->as<AstExprInterpString>())
            return visitExprImpl(entry, interpolation);
        else if (auto error = expr->as<AstExprError>())
            return visitExprImpl(entry, error);
        else
            state->handle->ice("Unknown AstExpr in CfgBuilder::visitExpr");
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprGroup* group)
    {
        // This is an instruction because we need to resize the stack to 1 value

        CfgNode* exit = visitExpr(entry, group->expr);
        exit->addInstruction(Instruction::Expr{group});
        return exit;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprCall* call)
    {
        // So... this is a long one and pretty complicated. It interacts with
        // calling conventions, edge cases with `__call`, and some edge case
        // where the argument order necessarily not matching with evaluation.
        //
        // Even so, we still have a few other things to think about: argument
        // count and the control flow diagram with exceptional control flow.
        //
        // 1: The calling convention.
        //
        // `<self>:<method>(<args>)`
        //
        // Is not actually isomorphic to
        //
        // `<self>.<method>(<self>, <args>)`
        //
        // But rather, it is isomorphic to _two_ distinct instructions:
        //
        // ```
        // local _tmp = <self>
        // _tmp.method(_tmp, <args>)
        // ```
        //
        // Case in point: `self:m(x, y)` vs `self.foo:m(x, y)`. This doesn't get
        // into the nasty edge case where the argument ordering is totally
        // different depending on whether the self argument is a global.
        //
        // 2: The argument count v.s. the count of unique expression addresses.
        //
        // This is where we might need another instruction type that "patches"
        // the actual stack size, because really, the expression:
        //
        // Same as above, but in the expression `self:m()`, we have to "load"
        // the value at the temporary binding twice. First to read `m`, and
        // again to load it into the corresponding argument slot. But `self`
        // uses the same pointer address to the AST node, rendering whatever
        // memoization to be inaccurate unless we store a separate map for it,
        // but that requires the caller to correctly look up with the correct
        // function.
        //
        // 3: Exceptional control flow is disjunctive in a function call.
        //
        // ... sometimes. Sort of. If in the _same_ argument slot, that
        // expression _always_ diverges, then the expressions that follows are
        // unreachable, as is the function call, and everything after that.
        //
        // But if one argument slot is partially divergent, and _another_
        // argument slot is also partially divergent, then we have two possible
        // divergences that are unrelated, which forms at least two entry nodes
        // to the function application.
        //
        // To put it simply: calling a function is an instruction, _but not_
        // representable using the AST structure. We will need an `Instruction`
        // that can model a `Call` node correctly for any fixpoint iteration.

        // Due to the complexity, we will simply leave it stubbed for now.
        return nullptr;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprIndexName* index)
    {
        CfgNode* exit = visitExpr(entry, index->expr);
        exit->addInstruction(Instruction::Expr{index});
        return exit;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprIndexExpr* subscript)
    {
        // This has the same problem as function applications because this is a
        // composition of two expressions that both must succeed to evalaute to
        // some value. It's the simpler form of `f(x)` where either `f` or `x`
        // diverges, giving a disjunctive form. But the AST structure models the
        // problem correctly, so we can reuse it.

        CfgNode* exprExit = visitExpr(entry, subscript->expr);
        CfgNode* indexExit = visitExpr(exprExit, subscript->index);
        indexExit->addInstruction(Instruction::Expr{subscript});
        return indexExit;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprFunction* function)
    {
        // The CFG of a function is disjoint from the CFG of the block that contains
        // it, so to keep it disjoint, we return the same node back.

        state->pendingFunctions->push_back(function);
        return entry;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprTable* table)
    {
        return nullptr;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprUnary* unary)
    {
        return visitExpr(entry, unary->expr);
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprBinary* binary)
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
            CfgNode* exitNode = state->arena->freshNode();

            CfgNode* leftNode = visitExpr(entry, binary->left);
            CfgNode* rightNode = state->arena->freshNode();
            leftNode->addConsequent(rightNode);
            leftNode->addConsequent(exitNode);

            exitNode->addAntecedent(visitExpr(rightNode, binary->right));
            exitNode->addInstruction(Instruction::Expr{binary});
            return exitNode;
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
            CfgNode* leftNode = visitExpr(entry, binary->left);
            CfgNode* rightNode = visitExpr(leftNode, binary->right);
            rightNode->addInstruction(Instruction::Expr{binary});
            return rightNode;
        }
        case AstExprBinary::Op__Count:
            state->handle->ice("Op__Count should never be generated in an AST.");
        default:
            return nullptr;
        }
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprTypeAssertion* typeAssertion)
    {
        // We want to visit the annotation first before the expression in this
        // case. For instance, `f(x) :: typeof(x)`. Certainly, we'd like to have
        // the `x` see whatever come before the the use of `x` in the left side.

        visitType(entry, typeAssertion->annotation);
        return visitExpr(entry, typeAssertion->expr);
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprIfElse* branch)
    {
        CfgNode* postCondition = visitExpr(entry, branch->condition);
        postCondition->addInstruction(Instruction::Expr{branch});

        CfgNode* thenNode = state->arena->freshNode();
        CfgNode* elseNode = state->arena->freshNode();
        thenNode->addAntecedent(postCondition);
        elseNode->addAntecedent(postCondition);

        CfgNode* exit = state->arena->freshNode();
        exit->addAntecedent(visitExpr(thenNode, branch->trueExpr));
        exit->addAntecedent(visitExpr(elseNode, branch->falseExpr));
        return exit;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprInterpString* interpolation)
    {
        CfgNode* exit = entry;
        for (AstExpr* expr : interpolation->expressions)
            exit = visitExpr(exit, expr);

        return exit;
    }

    CfgNode* visitExprImpl(CfgNode* entry, AstExprError* error)
    {
        CfgNode* unreachable = state->arena->freshNode();
        unreachable->addInstruction(Instruction::Unreachable{});
        unreachable->addAntecedent(entry);

        for (AstExpr* expr : error->expressions)
            visitExpr(unreachable, expr);

        return entry;
    }

    void visitType(CfgNode* entry, AstType* type)
    {
        if (auto ref = type->as<AstTypeReference>())
            return visitType(entry, ref);
        else if (auto table = type->as<AstTypeTable>())
            return visitType(entry, table);
        else if (auto function = type->as<AstTypeFunction>())
            return visitType(entry, function);
        else if (auto typeofTerm = type->as<AstTypeTypeof>())
            return visitType(entry, typeofTerm);
        else if (auto disjunction = type->as<AstTypeUnion>())
            return visitType(entry, disjunction);
        else if (auto conjunction = type->as<AstTypeIntersection>())
            return visitType(entry, conjunction);
        else if (auto error = type->as<AstTypeError>())
            return visitType(entry, error);
        else
            state->handle->ice("Unknown AstType in CfgBuilder::visitType");
    }

    void visitTypeImpl(CfgNode* entry, AstTypeReference* ref)
    {
        for (AstTypeOrPack parameter : ref->parameters)
        {
            if (parameter.type)
                visitType(entry, parameter.type);
            else
                visitTypePack(entry, parameter.typePack);
        }
    }

    void visitTypeImpl(CfgNode* entry, AstTypeTable* table)
    {
        for (const AstTableProp& prop : table->props)
            visitType(entry, prop.type);

        if (table->indexer)
        {
            visitType(entry, table->indexer->indexType);
            visitType(entry, table->indexer->resultType);
        }
    }

    void visitTypeImpl(CfgNode* entry, AstTypeFunction* function)
    {
        visitGenerics(entry, function->generics);
        visitGenericPacks(entry, function->genericPacks);
        visitTypeList(entry, function->argTypes);

        if (function->returnTypes)
            visitTypePack(entry, function->returnTypes);
    }

    void visitTypeImpl(CfgNode* entry, AstTypeTypeof* typeofTerm)
    {
        CfgNode* unreachable = state->arena->freshNode();
        unreachable->addInstruction(Instruction::Unreachable{});
        unreachable->addInstruction(Instruction::Expr{typeofTerm->expr});

        visitExpr(unreachable, typeofTerm->expr);
    }

    void visitTypeImpl(CfgNode* entry, AstTypeUnion* disjunction)
    {
        for (AstType* t : disjunction->types)
            visitType(entry, t);
    }

    void visitTypeImpl(CfgNode* entry, AstTypeIntersection* conjunction)
    {
        for (AstType* t : conjunction->types)
            visitType(entry, t);
    }

    void visitTypeImpl(CfgNode* entry, AstTypeError* error)
    {
        for (AstType* type : error->types)
            visitType(entry, type);
    }

    void visitTypePack(CfgNode* entry, AstTypePack* pack)
    {
        if (auto stack = pack->as<AstTypePackExplicit>())
            return visitTypePack(entry, stack);
        else if (auto homogeneous = pack->as<AstTypePackVariadic>())
            return visitTypePack(entry, homogeneous);
        else if (pack->is<AstTypePackGeneric>())
            return;
        else
            state->handle->ice("Unknown AstTypePack in CfgBuilder::visitTypePack");
    }

    void visitTypePackImpl(CfgNode* entry, AstTypePackExplicit* stack)
    {
        visitTypeList(entry, stack->typeList);
    }

    void visitTypePackImpl(CfgNode* entry, AstTypePackVariadic* homogeneous)
    {
        visitType(entry, homogeneous->variadicType);
    }

    void visitTypeList(CfgNode* entry, AstTypeList list)
    {
        for (AstType* t : list.types)
            visitType(entry, t);

        if (list.tailType)
            visitTypePack(entry, list.tailType);
    }

    void visitGenerics(CfgNode* entry, AstArray<AstGenericType*> params)
    {
        for (AstGenericType* param : params)
        {
            if (param->defaultValue)
                visitType(entry, param->defaultValue);
        }
    }

    void visitGenericPacks(CfgNode* entry, AstArray<AstGenericTypePack*> params)
    {
        for (AstGenericTypePack* param : params)
        {
            if (param->defaultValue)
                visitTypePack(entry, param->defaultValue);
        }
    }
};

ControlFlowAnalysis::ControlFlowAnalysis(NotNull<struct InternalErrorReporter> handle)
    : handle(handle)
{
}

NotNull<ControlFlowGraph> ControlFlowAnalysis::build(AstStatBlock* root)
{
    std::vector<AstExprFunction*> pendingFunctions;
    CfgBuilderState state{NotNull{&arena}, NotNull{&pendingFunctions}, handle};

    NotNull<ControlFlowGraph> moduleCfg = prototype(root);
    CfgBuilder builder{moduleCfg, NotNull{&state}};
    builder.traverse(arena.freshNode(), root);

    while (!pendingFunctions.empty())
    {
        AstExprFunction* function = pendingFunctions.back();
        pendingFunctions.pop_back();

        NotNull<ControlFlowGraph> functionCfg = prototype(function);
        CfgBuilder builder{functionCfg, NotNull{&state}};
        builder.traverse(function);
    }

    return moduleCfg;
}

ControlFlowGraph* ControlFlowAnalysis::get(AstStat* stat) const
{
    if (auto cfg = functionCfgs.find(stat))
        return *cfg;
    return nullptr;
}

ControlFlowGraph* ControlFlowAnalysis::get(AstExpr* expr) const
{
    if (auto cfg = functionCfgs.find(expr))
        return *cfg;
    return nullptr;
}

NotNull<ControlFlowGraph> ControlFlowAnalysis::prototype(AstNode* node)
{
    // Nothing I can do about AstStatBlock that isn't the module root without
    // the ancestry tree of the AST. This function is private anyhow, but still.
    LUAU_ASSERT(node->is<AstStatBlock>() || node->is<AstExprFunction>());

    NotNull<ControlFlowGraph> cfg{cfgs.emplace_back(new ControlFlowGraph{}).get()};
    functionCfgs[node] = cfg;
    return cfg;
}

} // namespace Luau
