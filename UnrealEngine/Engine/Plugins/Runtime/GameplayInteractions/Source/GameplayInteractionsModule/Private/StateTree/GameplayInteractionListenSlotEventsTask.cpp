// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionListenSlotEventsTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionListenSlotEventsTask)

FGameplayInteractionListenSlotEventsTask::FGameplayInteractionListenSlotEventsTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionListenSlotEventsTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EStateTreeRunStatus FGameplayInteractionListenSlotEventsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionListenSlotEventsTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.OnEventHandle.Reset();

	FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot);
	if (OnEventDelegate == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionListenSlotEventsTask] Expected to find event delegate for the slot."));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeEventQueue& EventQueue = Context.GetMutableEventQueue();

	// Start piping Smart Object slot events into State Tree.
	InstanceData.OnEventHandle = OnEventDelegate->AddLambda([TargetSlot = InstanceData.TargetSlot, &EventQueue, Owner = Context.GetOwner()](const FSmartObjectEventData& Data)
	{
		if (Data.SlotHandle == TargetSlot && Data.Reason == ESmartObjectChangeReason::OnEvent)
		{
			UE_VLOG_UELOG(Owner, LogStateTree, VeryVerbose, TEXT("Listen Slot Events: received %s"), *Data.Tag.ToString());

			EventQueue.SendEvent(Owner, Data.Tag, Data.EventPayload);
		}
	});

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionListenSlotEventsTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.OnEventHandle.IsValid())
	{
		// Stop listening.
		if (FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot))
		{
			OnEventDelegate->Remove(InstanceData.OnEventHandle);
		}
	}

	InstanceData.OnEventHandle.Reset();
}
