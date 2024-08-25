// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.h"
#include "AvaTransitionLayerCondition.h"
#include "AvaTransitionStateMatchCondition.generated.h"

USTRUCT()
struct FAvaTransitionStateMatchConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Check State of other Scene in Layer", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionStateMatchCondition : public FAvaTransitionLayerCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionStateMatchConditionInstanceData;

	//~ Begin FAvaTransitionCondition
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionCondition

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FAvaTransitionStateMatchConditionInstanceData::StaticStruct(); }
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeConditionBase
	virtual bool TestCondition(FStateTreeExecutionContext& InContext) const override;
	//~ End FStateTreeConditionBase

	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaTransitionRunState TransitionState = EAvaTransitionRunState::Running;
};
