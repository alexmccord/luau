// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include <doctest.h>

#include <type_traits>

#include "Luau/Bump.h"

using namespace Luau;

struct DtorTest
{
    bool* b;

    DtorTest(bool* b)
        : b(b)
    {
    }

    ~DtorTest()
    {
        *b = false;
    }
};

TEST_SUITE_BEGIN("EqSatBump");

TEST_CASE("allocate_a_couple_of_things")
{
    EqSat::BumpAllocator bump;
    int* x = bump.allocate<int>(5);
    REQUIRE(x);
    CHECK(*x == 5);
}

TEST_CASE("dtor_works")
{
    static_assert(!std::is_trivially_destructible_v<DtorTest>);

    bool b = true;

    {
        EqSat::BumpAllocator bump;
        DtorTest* dtor = bump.allocate<DtorTest>(&b);

        REQUIRE(dtor);
        REQUIRE(dtor->b);
        CHECK(b);
    }

    CHECK(!b);
}

TEST_SUITE_END();
