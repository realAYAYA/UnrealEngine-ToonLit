// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsMisc.h"

struct FUnrealHeaderToolStats
{
	TMap<FName, double> Counters;

	static FUnrealHeaderToolStats& Get();
	static double& GetCounter(const FName& Key);
};

#if STATS
#define SCOPE_SECONDS_COUNTER_UHT(Name) \
	FSimpleScopeSecondsCounter ANONYMOUS_VARIABLE(Name)(FUnrealHeaderToolStats::GetCounter(TEXT(#Name)))
#else
#define SCOPE_SECONDS_COUNTER_UHT(Name)
#endif
