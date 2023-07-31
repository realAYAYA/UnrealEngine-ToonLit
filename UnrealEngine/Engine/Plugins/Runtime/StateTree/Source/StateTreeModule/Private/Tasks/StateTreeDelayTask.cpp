// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeDelayTask.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeDelayTask)

EStateTreeRunStatus FStateTreeDelayTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	InstanceDataType& InstanceData = Context.GetInstanceData<InstanceDataType>(*this);

	if (!InstanceData.bRunForever)
	{
		InstanceData.RemainingTime = FMath::FRandRange(
			FMath::Max(0.0f, InstanceData.Duration - InstanceData.RandomDeviation), (InstanceData.Duration + InstanceData.RandomDeviation));
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeDelayTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	InstanceDataType& InstanceData = Context.GetInstanceData<InstanceDataType>(*this);

	if (!InstanceData.bRunForever)
	{
		InstanceData.RemainingTime -= DeltaTime;

		if (InstanceData.RemainingTime <= 0.f)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}
	
	return EStateTreeRunStatus::Running;
}

