// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSyncSlotTagTransition.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSyncSlotTagTransition)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSyncSlotTagTransitionTask::FGameplayInteractionSyncSlotTagTransitionTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state, we assume the slot does not change.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionSyncSlotTagTransitionTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

EDataValidationResult FGameplayInteractionSyncSlotTagTransitionTask::Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!TransitionFromTag.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingTransitionFromTag", "TransitionFromTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!TransitionToTag.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingTransitionToTag", "TransitionToTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	if (!TransitionEventTag.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingTransitionEventTag", "TransitionEventTag property is empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

EStateTreeRunStatus FGameplayInteractionSyncSlotTagTransitionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.OnEventHandle.Reset();
	
	if (!InstanceData.TargetSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected valid TargetSlot handle."));
		return EStateTreeRunStatus::Failed;
	}

	FOnSmartObjectEvent* OnEventDelegate = SmartObjectSubsystem.GetSlotEventDelegate(InstanceData.TargetSlot);
	if (OnEventDelegate == nullptr)
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected to find event delegate for the slot."));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeEventQueue& EventQueue = Context.GetMutableEventQueue();

	// Check initial state
	const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(InstanceData.TargetSlot);
	if (!SlotView.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Expected valid slot view."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.State = EGameplayInteractionSyncSlotTransitionState::WaitingForFromTag;

	// Check initial state.
	if (SlotView.GetTags().HasTag(TransitionToTag))
	{
		// Signal the other slot to change.
		EventQueue.SendEvent(Context.GetOwner(), TransitionEventTag);
		InstanceData.State = EGameplayInteractionSyncSlotTransitionState::Completed;

		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition (initial): (%s) WaitingForToTag match [%s] -> Event %s"),
			*LexToString(InstanceData.TargetSlot), *TransitionToTag.ToString(), *TransitionEventTag.ToString());
	}
	else if (SlotView.GetTags().HasTag(TransitionFromTag))
	{
		// Signal the other slot to change.
		SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, TransitionEventTag);
		InstanceData.State = EGameplayInteractionSyncSlotTransitionState::WaitingForToTag;

		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition (initial): (%s) WaitingForFromTag match [%s] -> SOEvent %s"),
			*LexToString(InstanceData.TargetSlot), *TransitionFromTag.ToString(), *TransitionEventTag.ToString());
	}

	// Event queue and the node are safe to access in the delegate.
	// InstanceData can be moved in memory, so we need to capture what we need by value.
	if (InstanceData.State != EGameplayInteractionSyncSlotTransitionState::Completed)
	{
		InstanceData.OnEventHandle = OnEventDelegate->AddLambda(
			[this, TargetSlot = InstanceData.TargetSlot, &EventQueue, InstanceDataRef = Context.GetInstanceDataStructRef(*this), SmartObjectSubsystem = &SmartObjectSubsystem, Owner = Context.GetOwner()](const FSmartObjectEventData& Data) mutable
			{
				if (TargetSlot == Data.SlotHandle
					&& Data.Reason == ESmartObjectChangeReason::OnTagAdded)
				{
					if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
					{
						UE_VLOG_UELOG(Owner, LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) Tag %s added"),
							*LexToString(InstanceData->TargetSlot), *Data.Tag.ToString());

						if (InstanceData->State == EGameplayInteractionSyncSlotTransitionState::WaitingForFromTag)
						{
							if (Data.Tag.MatchesTag(TransitionFromTag))
							{
								SmartObjectSubsystem->SendSlotEvent(InstanceData->TargetSlot, TransitionEventTag);
								InstanceData->State = EGameplayInteractionSyncSlotTransitionState::WaitingForToTag;

								UE_VLOG_UELOG(Owner, LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) WaitingForFromTag match [%s] -> SOEvent %s"),
									*LexToString(InstanceData->TargetSlot), *TransitionFromTag.ToString(), *TransitionEventTag.ToString());
							}
						}
						else if (InstanceData->State == EGameplayInteractionSyncSlotTransitionState::WaitingForToTag)
						{
							if (Data.Tag.MatchesTag(TransitionToTag))
							{
								EventQueue.SendEvent(Owner, TransitionEventTag);
								InstanceData->State = EGameplayInteractionSyncSlotTransitionState::Completed;

								UE_VLOG_UELOG(Owner, LogStateTree, VeryVerbose, TEXT("[GameplayInteractionSyncSlotTagTransitionTask] Sync transition: (%s) WaitingForToTag match [%s] -> Event %s"),
									*LexToString(InstanceData->TargetSlot), *TransitionToTag.ToString(), *TransitionEventTag.ToString());
							}
						}
					}
				}
			});
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSyncSlotTagTransitionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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
}

#undef LOCTEXT_NAMESPACE
