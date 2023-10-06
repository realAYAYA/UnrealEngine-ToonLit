// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionSendSlotEventTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionSendSlotEventTask)

#define LOCTEXT_NAMESPACE "GameplayInteractions"

FGameplayInteractionSendSlotEventTask::FGameplayInteractionSendSlotEventTask()
{
	// No tick needed.
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

bool FGameplayInteractionSendSlotEventTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);

	bShouldStateChangeOnReselect = bShouldTriggerOnReselect;
	// Copy properties on exit state if the event is sent then.
	bShouldCopyBoundPropertiesOnExitState = (Trigger == EGameplayInteractionTaskTrigger::OnExitState);

	return true;
}

EDataValidationResult FGameplayInteractionSendSlotEventTask::Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages)
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	
	if (!EventTag.IsValid() && !Payload.IsValid())
	{
		ValidationMessages.Add(LOCTEXT("MissingEventData", "EventTag and Payload properties are empty, expecting valid tag."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

EStateTreeRunStatus FGameplayInteractionSendSlotEventTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (Trigger == EGameplayInteractionTaskTrigger::OnEnterState)
	{
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

		if (InstanceData.TargetSlot.IsValid())
		{
			// Send the event
			SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, EventTag, Payload);
		}
		else
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSendSlotEventTask] Expected valid TargetSlot handle."));
		}
	}

	return EStateTreeRunStatus::Running;
}

void FGameplayInteractionSendSlotEventTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const bool bLastStateFailed = Transition.CurrentRunStatus == EStateTreeRunStatus::Failed
								|| (bHandleExternalStopAsFailure &&  Transition.CurrentRunStatus == EStateTreeRunStatus::Stopped);
	
	if (Trigger == EGameplayInteractionTaskTrigger::OnExitState
		|| (bLastStateFailed && Trigger == EGameplayInteractionTaskTrigger::OnExitStateFailed)
		|| (!bLastStateFailed && Trigger == EGameplayInteractionTaskTrigger::OnExitStateSucceeded))
	{
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

		if (InstanceData.TargetSlot.IsValid())
		{
			// Send the event
			SmartObjectSubsystem.SendSlotEvent(InstanceData.TargetSlot, EventTag, Payload);
		}
		else
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionSendSlotEventTask] Expected valid TargetSlot handle."));
		}
	}
}

#undef LOCTEXT_NAMESPACE
