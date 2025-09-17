// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/TypedAllocator.h"
#include "Luau/Cfg/Instruction.h"

#include <iterator>

namespace Luau
{

template<typename T>
struct CfgArray
{
    CfgArray(const std::vector<T>* vec)
        : vec(vec)
    {
    }

    const T* begin() const
    {
        return vec->data();
    }

    const T* end() const
    {
        return vec->data() + vec->size();
    };

    std::reverse_iterator<const T*> rbegin() const
    {
        return std::make_reverse_iterator(end());
    }

    std::reverse_iterator<const T*> rend() const
    {
        return std::make_reverse_iterator(begin());
    }

    bool empty() const
    {
        return size() == 0;
    }

    size_t size() const
    {
        return vec->size();
    }

private:
    const std::vector<T>* vec;
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

    CfgArray<CfgNode*> antecedents() const;
    CfgArray<CfgNode*> consequents() const;
    CfgArray<Instruction> instructions() const;

private:
    std::vector<CfgNode*> antecedentNodes;
    std::vector<CfgNode*> consequentNodes;
    std::vector<Instruction> insns;
    bool dirty = false;
};

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

} // namespace Luau
