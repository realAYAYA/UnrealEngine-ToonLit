// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Tasks/AvaSceneAddTagAttributeTask.h"
#include "AvaSceneState.h"
#include "IAvaSceneInterface.h"

#define LOCTEXT_NAMESPACE "AvaSceneAddTagAttributeTask"

FText FAvaSceneAddTagAttributeTask::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("TaskDescription", "Add '{0}' tag attribute to this scene"), FText::FromName(TagAttribute.ToName()));
}

EStateTreeRunStatus FAvaSceneAddTagAttributeTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
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

	if (SceneState->AddTagAttribute(TagAttribute))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Failed;
}

#undef LOCTEXT_NAMESPACE
