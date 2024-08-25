// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "AvaTransitionCondition.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionLayerCondition.generated.h"

struct FAvaTransitionContext;

USTRUCT(Category="Transition Logic", meta=(Hidden))
struct AVALANCHETRANSITION_API FAvaTransitionLayerCondition : public FAvaTransitionCondition
{
	GENERATED_BODY()

	/** Gets all the Behavior Instances that match the Layer Query. Always excludes the Instance belonging to this Transition */
	TArray<const FAvaTransitionBehaviorInstance*> QueryBehaviorInstances(const FStateTreeExecutionContext& InContext) const;

	FText GetLayerQueryText() const;

	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaTransitionLayerCompareType LayerType = EAvaTransitionLayerCompareType::Same;

	UPROPERTY(EditAnywhere, Category="Parameter", meta=(EditCondition="LayerType==EAvaTransitionLayerCompareType::MatchingTag", EditConditionHides))
	FAvaTagHandle SpecificLayer;
};
