// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionGetSlotActorTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionGetSlotActorTask)

FGameplayInteractionGetSlotActorTask::FGameplayInteractionGetSlotActorTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionGetSlotActorTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EStateTreeRunStatus FGameplayInteractionGetSlotActorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionGetSlotActorTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ResultActor = nullptr;
	
	const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(InstanceData.TargetSlot);
	if (SlotView.IsValid())
	{
		if (const FGameplayInteractionSlotUserData* UserData = SlotView.GetStateDataPtr<FGameplayInteractionSlotUserData>())
		{
			InstanceData.ResultActor = UserData->UserActor.Get();
		}
	}
	
	if (bFailIfNotFound && !IsValid(InstanceData.ResultActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;  
}
