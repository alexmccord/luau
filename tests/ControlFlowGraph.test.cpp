// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details

#include "Luau/Cfg/Node.h"

#include "Fixture.h"
#include "doctest.h"

using namespace Luau;

TEST_SUITE_BEGIN("ControlFlowGraph");

TEST_CASE("cfg_nodes_correctly_add_antecedents_and_consequents")
{
    CfgNode n1;
    CfgNode n2;
    CfgNode n3;

    n1.addConsequent(&n2);
    n2.addAntecedent(&n3);

    CHECK(n1.antecedents().size() == 0);
    CHECK(n1.consequents().size() == 1);
    CHECK(n2.antecedents().size() == 2);
    CHECK(n2.consequents().size() == 0);
    CHECK(n3.antecedents().size() == 0);
    CHECK(n3.consequents().size() == 1);

    auto contains = [](auto range, auto value) -> bool
    {
        return std::find(range.begin(), range.end(), value) != range.end();
    };

    CHECK(contains(n1.consequents(), &n2));
    CHECK(contains(n2.antecedents(), &n1));
    CHECK(contains(n2.antecedents(), &n3));
    CHECK(contains(n3.consequents(), &n2));
}

TEST_SUITE_END();
