// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Cfg/Node.h"

#include "Luau/Cfg/Instruction.h"

namespace Luau
{

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

void CfgNode::addInstruction(Instruction instruction)
{
    insns.push_back(instruction);
}

CfgArray<CfgNode*> CfgNode::antecedents() const
{
    return CfgArray{&antecedentNodes};
}

CfgArray<CfgNode*> CfgNode::consequents() const
{
    return CfgArray{&consequentNodes};
}

CfgArray<Instruction> CfgNode::instructions() const
{
    return CfgArray{&insns};
}

CfgNode* CfgNodeArena::freshNode()
{
    return nodes.allocate();
}

} // namespace Luau
