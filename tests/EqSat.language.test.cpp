// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include <doctest.h>

#include "Luau/Bump.h"
#include "Luau/Id.h"
#include "Luau/Language.h"

#include <string>
#include <unordered_map>

LUAU_EQSAT_ATOM(I32, int);
LUAU_EQSAT_ATOM(Bool, bool);
LUAU_EQSAT_ATOM(Str, std::string);

LUAU_EQSAT_FIELD(Left);
LUAU_EQSAT_FIELD(Right);
LUAU_EQSAT_NODE_FIELDS(Add, Left, Right);

using namespace Luau;

using Value = EqSat::Language<I32, Bool, Str, Add>;

TEST_SUITE_BEGIN("EqSatLanguage");

TEST_CASE("atom_equality")
{
    CHECK(I32{0} == I32{0});
    CHECK(I32{0} != I32{1});
}

TEST_CASE("node_equality")
{
    CHECK(Add{EqSat::Id{0}, EqSat::Id{0}} == Add{EqSat::Id{0}, EqSat::Id{0}});
    CHECK(Add{EqSat::Id{1}, EqSat::Id{0}} != Add{EqSat::Id{0}, EqSat::Id{0}});
}

TEST_CASE("language_get")
{
    EqSat::BumpAllocator bump;

    Value v{bump.allocate<I32>(5)};

    auto i = v.get<I32>();
    REQUIRE(i);
    CHECK(i->value());

    CHECK(!v.get<Bool>());
}

TEST_CASE("language_copy_ctor")
{
    EqSat::BumpAllocator bump;

    Value v1{bump.allocate<I32>(5)};
    Value v2 = v1;

    auto i1 = v1.get<I32>();
    auto i2 = v2.get<I32>();
    REQUIRE(i1);
    REQUIRE(i2);
    CHECK(i1->value() == i2->value());
}

TEST_CASE("language_move_ctor")
{
    EqSat::BumpAllocator bump;

    Value v1{bump.allocate<Str>("hello")};
    {
        auto s1 = v1.get<Str>();
        REQUIRE(s1);
        CHECK(s1->value() == "hello");
    }

    Value v2 = std::move(v1);

    auto s1 = v1.get<Str>();
    REQUIRE(s1);
    CHECK(s1->value() == ""); // this also tests the dtor.

    auto s2 = v2.get<Str>();
    REQUIRE(s2);
    CHECK(s2->value() == "hello");
}

TEST_CASE("language_equality")
{
    EqSat::BumpAllocator bump;

    Value v1{bump.allocate<I32>(0)};
    Value v2{bump.allocate<I32>(0)};
    Value v3{bump.allocate<I32>(1)};
    Value v4{bump.allocate<Bool>(true)};
    Value v5{bump.allocate<Add>(EqSat::Id{0}, EqSat::Id{1})};

    CHECK(v1 == v2);
    CHECK(v2 != v3);
    CHECK(v3 != v4);
    CHECK(v4 != v5);
}

TEST_CASE("language_is_mappable")
{
    EqSat::BumpAllocator bump;

    std::unordered_map<Value, int, Value::Hash> map;

    Value v1{bump.allocate<I32>(5)};
    Value v2{bump.allocate<I32>(5)};
    Value v3{bump.allocate<Bool>(true)};
    Value v4{bump.allocate<Add>(EqSat::Id{0}, EqSat::Id{1})};

    map[v1] = 1;
    map[v2] = 2;
    map[v3] = 42;
    map[v4] = 37;

    CHECK(map[v1] == 2);
    CHECK(map[v2] == 2);
    CHECK(map[v3] == 42);
    CHECK(map[v4] == 37);
}

TEST_CASE("node_field")
{
    EqSat::Id left{0};
    EqSat::Id right{1};

    Add add{left, right};

    EqSat::Id left2 = add.field<Left>();
    EqSat::Id right2 = add.field<Right>();

    CHECK(left == left2);
    CHECK(left != right2);
    CHECK(right == right2);
    CHECK(right != left2);
}

TEST_CASE("language_operands")
{
    EqSat::BumpAllocator bump;

    Value v1{bump.allocate<I32>(0)};
    CHECK(v1.operands().empty());

    Value v2{bump.allocate<Add>(EqSat::Id{0}, EqSat::Id{1})};
    const Add* add = v2.get<Add>();
    REQUIRE(add);

    EqSat::Slice<EqSat::Id> actual = v2.operands();
    CHECK(actual.size() == 2);
    CHECK(actual[0] == add->field<Left>());
    CHECK(actual[1] == add->field<Right>());
}

TEST_SUITE_END();
