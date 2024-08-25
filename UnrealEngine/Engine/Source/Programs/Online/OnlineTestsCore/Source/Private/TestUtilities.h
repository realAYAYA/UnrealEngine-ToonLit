// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TestHarness.h"

class FTestUtilities
{
public:
	static FString GetUniqueTestString()
	{
		FString UniqueString = FGuid().ToString();

		return UniqueString;
	}

	static int32 GetUniqueTestInteger()
	{
		int32 Rand = FMath::RandHelper(INT_MAX);
		
		return Rand;
	}
};
