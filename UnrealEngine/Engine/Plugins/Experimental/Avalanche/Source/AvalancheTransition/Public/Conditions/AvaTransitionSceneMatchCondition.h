// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.h"
#include "AvaTransitionLayerCondition.h"
#include "AvaTransitionSceneMatchCondition.generated.h"

USTRUCT()
struct FAvaTransitionSceneMatchConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Compare other Scene in Layer", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionSceneMatchCondition : public FAvaTransitionLayerCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionSceneMatchConditionInstanceData;

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
	EAvaTransitionComparisonResult SceneComparisonType = EAvaTransitionComparisonResult::None;
};
