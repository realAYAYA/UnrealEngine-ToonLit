// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionTask.h"
#include "AvaTransitionDelayTask.generated.h"

USTRUCT()
struct FAvaTransitionDelayTaskInstanceData
{
	GENERATED_BODY()

	/** Internal countdown in seconds. */
	float RemainingTime = 0.f;
};

USTRUCT(DisplayName="Delay", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionDelayTask : public FAvaTransitionTask
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionDelayTaskInstanceData;

	//~ Begin FAvaTransitionTask
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionTask

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeTaskBase
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const override;
	//~ End FStateTreeTaskBase

	/** Delay in seconds before the task ends. */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(ClampMin="0.0"))
	float Duration = 0.5f;
};
