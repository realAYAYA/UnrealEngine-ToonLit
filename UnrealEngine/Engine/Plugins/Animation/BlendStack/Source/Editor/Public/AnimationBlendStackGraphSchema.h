// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationGraphSchema.h"
#include "AnimationBlendStackGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimationBlendStackGraphSchema : public UAnimationGraphSchema
{
	GENERATED_BODY()

	//~ Begin UEdGraphSchema Interface.
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	//~ End UEdGraphSchema Interface.
};
