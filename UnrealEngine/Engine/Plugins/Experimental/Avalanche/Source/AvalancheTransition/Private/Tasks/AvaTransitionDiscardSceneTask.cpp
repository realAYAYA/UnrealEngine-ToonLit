// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionDiscardSceneTask.h"
#include "AvaTransitionContext.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FAvaTransitionDiscardSceneTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	FAvaTransitionContext& TransitionContext = InContext.GetExternalData(TransitionContextHandle);

	if (FAvaTransitionScene* TransitionScene = TransitionContext.GetTransitionScene())
	{
		TransitionScene->SetFlags(EAvaTransitionSceneFlags::NeedsDiscard);
	}

	return EStateTreeRunStatus::Failed;
}
