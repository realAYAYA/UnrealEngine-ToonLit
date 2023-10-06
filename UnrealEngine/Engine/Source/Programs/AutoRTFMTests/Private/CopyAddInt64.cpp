// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>

TEST_CASE("CopyAddInt64")
{
    uint64_t X = 0x123456789abcdef0llu;
    AutoRTFM::Commit([&] () { X += 0xa1b2c3d4e5f60789llu; });
    REQUIRE(X == 0x123456789abcdef0llu + 0xa1b2c3d4e5f60789llu);
}

