// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <unordered_set>

using namespace std;

TEST_CASE("UnorderedSet")
{
    constexpr unsigned Count = 100;
    unordered_set<unsigned> MySet;
    AutoRTFM::Commit([&] ()
    {
        unordered_set<unsigned> MySetInner;
        for (unsigned Index = Count; Index--;)
        {
            MySetInner.insert(Index);
        }

        MySet = MySetInner;
    });

    REQUIRE(MySet.size() == Count);
    for (unsigned Index = Count; Index--;)
    {
        REQUIRE(MySet.count(Index) == 1);
    }
}
