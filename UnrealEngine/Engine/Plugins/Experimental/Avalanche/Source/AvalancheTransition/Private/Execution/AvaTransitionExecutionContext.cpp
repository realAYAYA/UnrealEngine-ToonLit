// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionExecutionContext.h"

FAvaTransitionExecutionContext::FAvaTransitionExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
	: FStateTreeExecutionContext(InOwner, InStateTree, InInstanceData)
{
}

void FAvaTransitionExecutionContext::SetSceneDescription(FString&& InSceneDescription)
{
	SceneDescription = MoveTemp(InSceneDescription);
}

FString FAvaTransitionExecutionContext::GetInstanceDescription() const
{
	if (!SceneDescription.IsEmpty())
	{
		return SceneDescription;
	}
	return FStateTreeExecutionContext::GetInstanceDescription();
}
