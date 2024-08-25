// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StateTreeMoveToTask.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Tasks/AITask_MoveTo.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeMoveToTask)

EStateTreeRunStatus FStateTreeMoveToTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FStateTreeMoveToTask failed since AIController is missing."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TaskOwner = TScriptInterface<IGameplayTaskOwnerInterface>(InstanceData.AIController->FindComponentByInterface(UGameplayTaskOwnerInterface::StaticClass()));
	if (!InstanceData.TaskOwner)
	{
		InstanceData.TaskOwner = InstanceData.AIController;
	}

	return PerformMoveTask(Context, *InstanceData.AIController);
}

EStateTreeRunStatus FStateTreeMoveToTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.MoveToTask)
	{
		if (InstanceData.MoveToTask->GetState() == EGameplayTaskState::Finished)
		{
			return InstanceData.MoveToTask->WasMoveSuccessful() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
		}

		if (InstanceData.bTrackMovingGoal && !InstanceData.TargetActor)
		{
			const FVector CurrentDestination = InstanceData.MoveToTask->GetMoveRequestRef().GetDestination();
			if (FVector::DistSquared(CurrentDestination, InstanceData.Destination) > (InstanceData.DestinationMoveTolerance * InstanceData.DestinationMoveTolerance))
			{
				UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FStateTreeMoveToTask destination has moved enough. Restarting task."));
				return PerformMoveTask(Context, *InstanceData.AIController);
			}
		}
		return EStateTreeRunStatus::Running;
	}
	return EStateTreeRunStatus::Failed;
}

void FStateTreeMoveToTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.MoveToTask && InstanceData.MoveToTask->GetState() != EGameplayTaskState::Finished)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FStateTreeMoveToTask aborting move to because state finished."));
		InstanceData.MoveToTask->ExternalCancel();
	}
}

UAITask_MoveTo* FStateTreeMoveToTask::PrepareMoveToTask(FStateTreeExecutionContext& Context, AAIController& Controller, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAITask_MoveTo* MoveTask = ExistingTask ? ExistingTask : UAITask::NewAITask<UAITask_MoveTo>(Controller, *InstanceData.TaskOwner);
	if (MoveTask)
	{
		MoveTask->SetUp(&Controller, MoveRequest);
	}

	return MoveTask;
}

EStateTreeRunStatus FStateTreeMoveToTask::PerformMoveTask(FStateTreeExecutionContext& Context, AAIController& Controller) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FAIMoveRequest MoveReq;
	MoveReq.SetNavigationFilter(InstanceData.FilterClass ? InstanceData.FilterClass : Controller.GetDefaultNavigationFilterClass())
		.SetAllowPartialPath(InstanceData.bAllowPartialPath)
		.SetAcceptanceRadius(InstanceData.AcceptableRadius)
		.SetCanStrafe(InstanceData.bAllowStrafe)
		.SetReachTestIncludesAgentRadius(InstanceData.bReachTestIncludesAgentRadius)
		.SetReachTestIncludesGoalRadius(InstanceData.bReachTestIncludesGoalRadius)
		.SetRequireNavigableEndLocation(InstanceData.bRequireNavigableEndLocation)
		.SetProjectGoalLocation(InstanceData.bProjectGoalLocation)
		.SetUsePathfinding(true);

	if (InstanceData.TargetActor)
	{
		if (InstanceData.bTrackMovingGoal)
		{
			MoveReq.SetGoalActor(InstanceData.TargetActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InstanceData.TargetActor->GetActorLocation());
		}
	}
	else
	{
		MoveReq.SetGoalLocation(InstanceData.Destination);
	}

	if (MoveReq.IsValid())
	{	
		InstanceData.MoveToTask = PrepareMoveToTask(Context, Controller, InstanceData.MoveToTask, MoveReq);
		if (InstanceData.MoveToTask)
		{
			if (InstanceData.MoveToTask->IsActive())
			{
				InstanceData.MoveToTask->ConditionalPerformMove();
			}
			else
			{
				InstanceData.MoveToTask->ReadyForActivation();
			}

			if (InstanceData.MoveToTask->GetState() == EGameplayTaskState::Finished)
			{
				return InstanceData.MoveToTask->WasMoveSuccessful() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
			}

			return EStateTreeRunStatus::Running;
		}
	}

	UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FStateTreeMoveToTask failed because it doesn't have a destination."));
	return EStateTreeRunStatus::Failed;
}