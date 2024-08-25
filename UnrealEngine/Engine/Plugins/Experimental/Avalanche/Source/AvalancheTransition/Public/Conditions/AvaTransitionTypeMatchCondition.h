// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.h"
#include "Conditions/AvaTransitionCondition.h"
#include "AvaTransitionTypeMatchCondition.generated.h"

USTRUCT()
struct FAvaTransitionTypeMatchConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="My Transition Type is", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionTypeMatchCondition : public FAvaTransitionCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionTypeMatchConditionInstanceData;

	FAvaTransitionTypeMatchCondition() = default;

	//~ Begin FAvaTransitionCondition
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionCondition

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeConditionBase
	virtual bool TestCondition(FStateTreeExecutionContext& InContext) const override;
	//~ End FStateTreeConditionBase

	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaTransitionType TransitionType = EAvaTransitionType::In;
};
