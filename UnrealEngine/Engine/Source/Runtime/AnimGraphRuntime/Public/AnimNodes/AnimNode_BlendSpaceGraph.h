// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_BlendSpaceGraphBase.h"
#include "AnimNode_BlendSpaceGraph.generated.h"

// Allows multiple animations to be blended between based on input parameters
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendSpaceGraph : public FAnimNode_BlendSpaceGraphBase
{
	GENERATED_BODY()

	// @return the sync group that this blendspace uses
	FName GetGroupName() const { return GroupName; }
};
