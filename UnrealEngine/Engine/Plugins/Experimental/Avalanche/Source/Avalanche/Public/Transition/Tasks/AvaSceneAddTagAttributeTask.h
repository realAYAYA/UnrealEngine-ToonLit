// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSceneTask.h"
#include "AvaSceneAddTagAttributeTask.generated.h"

USTRUCT(DisplayName="Add tag attribute to this scene", Category="Scene Attributes")
struct AVALANCHE_API FAvaSceneAddTagAttributeTask : public FAvaSceneTask
{
	GENERATED_BODY()

	//~ Begin FAvaTransitionTask
	virtual FText GenerateDescription(const FAvaTransitionNodeContext& InContext) const override;
	//~ End FAvaTransitionTask

	//~ Begin FStateTreeTaskBase
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const override;
	//~ End FStateTreeTaskBase
};
