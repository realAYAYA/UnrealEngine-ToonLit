// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <atomic>
#include <map>
#include <vector>

TEST_CASE("Open")
{
    bool bDidRun = false;
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&] ()
        {
            AutoRTFM::Open([&] () { bDidRun = true; });
            AutoRTFM::Open([&] () { REQUIRE(bDidRun); });
        }));
    REQUIRE(bDidRun);
}

TEST_CASE("Open.Large")
{
    int x = 42;
    std::vector<int> v;
    std::map<int, std::vector<int>> m;
    bool bRanOpen = false;
    v.push_back(100);
    m[1].push_back(2);
    m[1].push_back(3);
    m[4].push_back(5);
    m[6].push_back(7);
    m[6].push_back(8);
    m[6].push_back(9);
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&] () {
            x = 5;

            for (size_t n = 10; n--;)
            {
                v.push_back(2 * n);
            }

            m.clear();
            m[10].push_back(11);
            m[12].push_back(13);
            m[12].push_back(14);

            AutoRTFM::Open([&] () {
#if 0
                // These checks are UB, because the open is interacting with transactional data!
                REQUIRE(x == 42);
                REQUIRE(v.size() == 1);
                REQUIRE(v[0] == 100);
                REQUIRE(m.size() == 3);
                REQUIRE(m[1].size() == 2);
                REQUIRE(m[1][0] == 2);
                REQUIRE(m[1][1] == 3);
                REQUIRE(m[4].size() == 1);
                REQUIRE(m[4][0] == 5);
                REQUIRE(m[6].size() == 3);
                REQUIRE(m[6][0] == 7);
                REQUIRE(m[6][1] == 8);
                REQUIRE(m[6][2] == 9);
#endif
                bRanOpen = true;
            });
        }));
    
    REQUIRE(bRanOpen);
    REQUIRE(x == 5);
    REQUIRE(v.size() == 11);
    REQUIRE(v[0] == 100);
    REQUIRE(v[1] == 18);
    REQUIRE(v[2] == 16);
    REQUIRE(v[3] == 14);
    REQUIRE(v[4] == 12);
    REQUIRE(v[5] == 10);
    REQUIRE(v[6] == 8);
    REQUIRE(v[7] == 6);
    REQUIRE(v[8] == 4);
    REQUIRE(v[9] == 2);
    REQUIRE(v[10] == 0);
    REQUIRE(m.size() == 2);
    REQUIRE(m[10].size() == 1);
    REQUIRE(m[10][0] == 11);
    REQUIRE(m[12].size() == 2);
    REQUIRE(m[12][0] == 13);
    REQUIRE(m[12][1] == 14);
}

TEST_CASE("Open.Atomics")
{
    std::atomic<bool> bDidRun = false;
    REQUIRE(
        AutoRTFM::ETransactionResult::Committed ==
        AutoRTFM::Transact([&] ()
        {
            AutoRTFM::Open([&] () { bDidRun = true; });
            AutoRTFM::Open([&] () { REQUIRE(bDidRun); });
        }));
    REQUIRE(bDidRun);
}
