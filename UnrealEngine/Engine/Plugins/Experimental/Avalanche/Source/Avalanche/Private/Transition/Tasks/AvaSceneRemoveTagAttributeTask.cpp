// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Tasks/AvaSceneRemoveTagAttributeTask.h"
#include "AvaSceneState.h"
#include "IAvaSceneInterface.h"

#define LOCTEXT_NAMESPACE "AvaSceneRemoveTagAttributeTask"

FText FAvaSceneRemoveTagAttributeTask::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("TaskDescription", "Remove '{0}' tag attribute from this scene"), FText::FromName(TagAttribute.ToName()));
}

EStateTreeRunStatus FAvaSceneRemoveTagAttributeTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	IAvaSceneInterface* Scene = GetScene(InContext);
	if (!Scene)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAvaSceneState* SceneState = Scene->GetSceneState();
	if (!SceneState)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (SceneState->RemoveTagAttribute(TagAttribute))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Failed;
}

#undef LOCTEXT_NAMESPACE
