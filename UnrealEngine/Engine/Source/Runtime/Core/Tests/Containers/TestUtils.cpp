// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestUtils.h"

#include <numeric>

namespace Test
{
	constexpr int32 DefaultRandomSeed = 283281277;
}

void Test::ResetRandomSeed()
{
	FMath::SRandInit(DefaultRandomSeed);
}

TArray<int32> Test::MakeIndexArray(int32 Size)
{
	TArray<int32> Indices;
	Indices.SetNum(Size);
	std::iota(Indices.begin(), Indices.end(), 0);
	return Indices;
}
