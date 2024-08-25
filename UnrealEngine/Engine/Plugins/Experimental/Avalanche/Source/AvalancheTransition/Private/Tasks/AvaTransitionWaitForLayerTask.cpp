// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionWaitForLayerTask.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionWaitForLayerTask"

FText FAvaTransitionWaitForLayerTask::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("TaskDescription", "Wait for others in {0} to finish")
		, GetLayerQueryText());
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	return QueryStatus(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const
{
	return QueryStatus(InContext);
}

EStateTreeRunStatus FAvaTransitionWaitForLayerTask::QueryStatus(FStateTreeExecutionContext& InContext) const
{
	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = QueryBehaviorInstances(InContext);

	bool bIsLayerRunning = BehaviorInstances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance* InInstance)
		{
			check(InInstance);
			return InInstance->IsRunning();
		});

	if (bIsLayerRunning)
	{
		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Succeeded;
}

#undef LOCTEXT_NAMESPACE
