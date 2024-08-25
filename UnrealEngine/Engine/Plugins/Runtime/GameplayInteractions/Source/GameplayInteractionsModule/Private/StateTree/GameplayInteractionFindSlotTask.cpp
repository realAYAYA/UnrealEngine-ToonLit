// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionFindSlotTask.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "Annotations/SmartObjectSlotLinkAnnotation.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionFindSlotTask)

FGameplayInteractionFindSlotTask::FGameplayInteractionFindSlotTask()
{
	// No tick needed.
	bShouldCallTick = false;
	// No need to update bound properties after enter state.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FGameplayInteractionFindSlotTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FGameplayInteractionFindSlotTask::UpdateResult(const FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.ReferenceSlot.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Error, TEXT("[GameplayInteractionFindSlotTask] Expected valid ReferenceSlot handle."));
		return false;
	}

	InstanceData.ResultSlot = FSmartObjectSlotHandle();

	if (ReferenceType == EGameplayInteractionSlotReferenceType::ByLinkTag)
	{
		// Acquire the target slot based on a link
		InstanceData.ResultSlot = FSmartObjectSlotHandle();
		
		const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(InstanceData.ReferenceSlot);
		const FSmartObjectSlotDefinition& SlotDefinition = SlotView.GetDefinition();

		for (const FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
		{
			if (const FSmartObjectSlotLinkAnnotation* Link = DataProxy.Data.GetPtr<FSmartObjectSlotLinkAnnotation>())
			{
				if (Link->Tag.MatchesTag(FindByTag))
				{
					TArray<FSmartObjectSlotHandle> Slots;
					SmartObjectSubsystem.GetAllSlots(SlotView.GetOwnerRuntimeObject(), Slots);

					const int32 SlotIndex = Link->LinkedSlot.GetIndex();
					if (Slots.IsValidIndex(SlotIndex))
					{
						InstanceData.ResultSlot = Slots[SlotIndex];
						break;
					}
				}
			}
		}
		
	}
	else if (ReferenceType == EGameplayInteractionSlotReferenceType::ByActivityTag)
	{
		// Acquire the target slot based on activity tags
		InstanceData.ResultSlot = FSmartObjectSlotHandle();
		
		const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(InstanceData.ReferenceSlot);
		const USmartObjectDefinition& Definition = SlotView.GetSmartObjectDefinition();
		
		int32 SlotIndex = 0;
		for (const FSmartObjectSlotDefinition& SlotDefinition : Definition.GetSlots())
		{
			if (SlotDefinition.ActivityTags.HasTag(FindByTag))
			{
				TArray<FSmartObjectSlotHandle> Slots;
				SmartObjectSubsystem.GetAllSlots(SlotView.GetOwnerRuntimeObject(), Slots);

				if (Slots.IsValidIndex(SlotIndex))
				{
					InstanceData.ResultSlot = Slots[SlotIndex];
					break;
				}
			}
			SlotIndex++;
		}
	}

	return InstanceData.ResultSlot.IsValid();
}

EStateTreeRunStatus FGameplayInteractionFindSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!UpdateResult(Context))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}
