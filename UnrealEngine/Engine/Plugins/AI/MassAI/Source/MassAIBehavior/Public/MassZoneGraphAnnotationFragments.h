// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "MassZoneGraphAnnotationFragments.generated.h"

struct FMassExecutionContext;

USTRUCT()
struct MASSAIBEHAVIOR_API FMassZoneGraphAnnotationFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Behavior tags for current lane */
	UPROPERTY()
	FZoneGraphTagMask Tags;
};

USTRUCT()
struct FMassZoneGraphAnnotationVariableTickChunkFragment : public FMassChunkFragment
{
	GENERATED_BODY();

	/** Update the ticking frequency of the chunk and return if this chunk should be process this frame */
	static bool UpdateChunk(FMassExecutionContext& Context);

	float TimeUntilNextTick = 0.0f;
	bool bInitialized = false;
};