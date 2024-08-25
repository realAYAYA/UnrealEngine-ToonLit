// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AvaTransitionDelayTask.h"
#include "StateTreeExecutionContext.h"

#define LOCTEXT_NAMESPACE "AvaTransitionDelayTask"

FText FAvaTransitionDelayTask::GenerateDescription(const FAvaTransitionNodeContext& InContext) const
{
	return FText::Format(LOCTEXT("TaskDescription", "Delay {0} seconds"), FText::AsNumber(Duration));
}

EStateTreeRunStatus FAvaTransitionDelayTask::EnterState(FStateTreeExecutionContext& InContext, const FStateTreeTransitionResult& InTransition) const
{
	FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	InstanceData.RemainingTime = Duration;

	if (InstanceData.RemainingTime <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAvaTransitionDelayTask::Tick(FStateTreeExecutionContext& InContext, const float InDeltaTime) const
{
	FInstanceDataType& InstanceData = InContext.GetInstanceData(*this);

	InstanceData.RemainingTime -= InDeltaTime;

	if (InstanceData.RemainingTime <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

#undef LOCTEXT_NAMESPACE
