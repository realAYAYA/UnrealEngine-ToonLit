// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionLayerTask.h"
#include "AvaTransitionWaitForLayerTask.generated.h"

USTRUCT()
struct FAvaTransitionWaitForLayerTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName = "Wait for other Scenes in Layer to Finish", Category="Transition Logic")
struct AVALANCHETRANSITION_API FAvaTransitionWaitForLayerTask : public FAvaTransitionLayerTask
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaTransitionWaitForLayerTaskInstanceData;

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

	EStateTreeRunStatus QueryStatus(FStateTreeExecutionContext& InContext) const;
};
