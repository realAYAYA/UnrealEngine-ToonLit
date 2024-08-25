// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/Tasks/AvaSceneTask.h"
#include "AvaSceneSubsystem.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FAvaSceneTask::Link(FStateTreeLinker& InLinker)
{
	FAvaTransitionTask::Link(InLinker);
	InLinker.LinkExternalData(SceneSubsystemHandle);
	return true;
}

IAvaSceneInterface* FAvaSceneTask::GetScene(FStateTreeExecutionContext& InContext) const
{
	const UAvaSceneSubsystem& SceneSubsystem       = InContext.GetExternalData(SceneSubsystemHandle);
	const FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);
	const FAvaTransitionScene* TransitionScene     = TransitionContext.GetTransitionScene();

	return TransitionScene ? SceneSubsystem.GetSceneInterface(TransitionScene->GetLevel()) : nullptr;
}
