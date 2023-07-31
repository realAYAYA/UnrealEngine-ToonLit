// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimationGraph.h"
#include "AnimationBlendSpaceSampleGraph.generated.h"

class UAnimGraphNode_BlendSpaceSampleResult;

UCLASS(MinimalAPI)
class UAnimationBlendSpaceSampleGraph : public UAnimationGraph
{
	GENERATED_BODY()

public:
	// Result node within the state's animation graph
	UPROPERTY()
	TObjectPtr<UAnimGraphNode_BlendSpaceSampleResult> ResultNode;
};
