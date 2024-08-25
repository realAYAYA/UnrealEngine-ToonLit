// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionLayerCondition.h"
#include "AvaTransitionLayerMatchCondition.generated.h"

USTRUCT()
struct FAvaTransitionLayerMatchConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Layer is Transitioning", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionLayerMatchCondition : public FAvaTransitionLayerCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionLayerMatchConditionInstanceData;

	//~ Begin FAvaTransitionCondition
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionCondition

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeConditionBase
	virtual bool TestCondition(FStateTreeExecutionContext& InContext) const override;
	//~ End FStateTreeConditionBase
};
