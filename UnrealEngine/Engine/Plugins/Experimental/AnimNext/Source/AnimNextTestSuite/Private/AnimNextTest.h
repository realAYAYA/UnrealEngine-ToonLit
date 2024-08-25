// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextTest.generated.h"

//// --- Raw type ---
USTRUCT()
struct FAnimNextTestData
{
	GENERATED_BODY()

	float A = 0.f;
	float B = 0.f;
};

namespace UE::AnimNext::Tests
{

struct FUtils
{
	// Clean up after tests. Clears transaction buffer, collects garbage
	static void CleanupAfterTests();
};

}
