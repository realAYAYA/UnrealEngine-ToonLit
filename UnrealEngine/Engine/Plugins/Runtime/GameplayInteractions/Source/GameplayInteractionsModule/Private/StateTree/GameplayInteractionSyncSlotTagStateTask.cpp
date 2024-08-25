// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSyncSlotTagStateTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSyncSlotTagStateTask)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSyncSlotTagStateTask::FGameplayInteractionSyncSlotTagStateTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state, we assume the slot does not change.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionSyncSlotTagStateTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EDataValidationResult FGameplayInteractionSyncSlotTagStateTask::Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!TagToMonitor.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingTagToMonitor", "TagToMonitor property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!BreakEventTag.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingBreakEventTag", "BreakEventTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

EStateTreeRunStatus FGameplayInteractionSyncSlotTagStateTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.OnEventHandle.Reset();

	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot);
	if (OnEventDelegate == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected to find event delegate for the slot."));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeEventQueue& EventQueue = Context.GetMutableEventQueue();
	
	// Check initial state
	const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(InstanceData.TargetSlot);
	if (!SlotView.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagStateTask] Expected valid slot view."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bBreakSignalled = false;

	// Check initial state.
	if (!SlotView.GetTags().HasTag(TagToMonitor))
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagStateTask] Sync state (initial): [%s] -> Event %s"), *TagToMonitor.ToString(), *BreakEventTag.ToString());

		// Signal the other slot to change.
		EventQueue.SendEvent(Context.GetOwner(), BreakEventTag);
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, BreakEventTag);
		InstanceData.bBreakSignalled = true;
	}

	if (!InstanceData.bBreakSignalled)
	{
		InstanceData.OnEventHandle = OnEventDelegate->AddLambda([TargetSlot = InstanceData.TargetSlot, this, InstanceDataRef = Context.GetInstanceDataStructRef(*this), &EventQueue, SmartObjectSubsystem = &SmartObjectSubsystem, Owner = Context.GetOwner()](const FSmartObjectEventData& Data) mutable
		{
			if (TargetSlot == Data.SlotHandle
				&& Data.Reason == ESmartObjectChangeReason::OnTagRemoved)
			{
				check(InstanceDataRef.IsValid());
				if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
				{
					if (!InstanceData->bBreakSignalled && Data.Tag.MatchesTag(TagToMonitor))
					{
						UE_VLOG_UELOG(Owner, LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagStateTask] Sync state: [%s] -> Event %s"), *TagToMonitor.ToString(), *BreakEventTag.ToString());

						SmartObjectSubsystem->SendSlotEvent(InstanceData->TargetSlot, BreakEventTag);
						EventQueue.SendEvent(Owner, BreakEventTag);
						InstanceData->bBreakSignalled = true;
					}
				}
			}
		});
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSyncSlotTagStateTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.OnEventHandle.IsValid())
	{
		if (FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot))
		{
			OnEventDelegate->Remove(InstanceData.OnEventHandle);
		}
	}
	InstanceData.OnEventHandle.Reset();

	if (!InstanceData.bBreakSignalled)
	{
		Context.SendEvent(BreakEventTag);
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, BreakEventTag);
		InstanceData.bBreakSignalled = true;
	}
}

#undef LOCTEXT_NAMESPACE
