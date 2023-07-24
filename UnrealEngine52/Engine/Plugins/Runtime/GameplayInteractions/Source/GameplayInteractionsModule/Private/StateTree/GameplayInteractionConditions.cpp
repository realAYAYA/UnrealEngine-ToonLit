// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTree/GameplayInteractionConditions.h"
#include "StateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionConditions)

#define ST_INTERACTION_LOG(Verbosity, Format, ...) UE_VLOG_UELOG(Context.GetOwner(), LogStateTree, Verbosity, TEXT("[%s] ") Format, *StaticStruct()->GetName(), ##__VA_ARGS__)
#define ST_INTERACTION_CLOG(Condition, Verbosity, Format, ...) UE_CVLOG_UELOG((Condition), Context.GetOwner(), LogStateTree, Verbosity, TEXT("[%s] ") Format, *StaticStruct()->GetName(), ##__VA_ARGS__)

namespace UE::GameplayInteraction
{

const FGameplayTagContainer* GetSlotTags(const USmartObjectSubsystem& SmartObjectSubsystem, const FSmartObjectSlotHandle Slot, const EGameplayInteractionMatchSlotTagSource Source)
{
	const FSmartObjectSlotView SlotView = SmartObjectSubsystem.GetSlotView(Slot);
	if (!SlotView.IsValid())
	{
		return nullptr;
	}

	if (Source == EGameplayInteractionMatchSlotTagSource::RuntimeTags)
	{
		return &SlotView.GetTags();
	}
	else if (Source == EGameplayInteractionMatchSlotTagSource::ActivityTags)
	{
		const FSmartObjectSlotDefinition& SlotDefinition = SlotView.GetDefinition();
		return &SlotDefinition.ActivityTags;
	}

	return nullptr;
}



};

//----------------------------------------------------------------------//
//  FGameplayInteractionSlotTagsMatchCondition
//----------------------------------------------------------------------//

bool FGameplayInteractionSlotTagsMatchCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FGameplayInteractionSlotTagsMatchCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FGameplayTagContainer* Container = UE::GameplayInteraction::GetSlotTags(SmartObjectSubsystem, InstanceData.Slot, Source);
	if (Container == nullptr)
	{
		return false;
	}

	bool bResult = false;
	switch (MatchType)
	{
	case EGameplayContainerMatchType::Any:
		bResult = bExactMatch ? Container->HasAnyExact(InstanceData.TagsToMatch) : Container->HasAny(InstanceData.TagsToMatch);
		break;
	case EGameplayContainerMatchType::All:
		bResult = bExactMatch ? Container->HasAllExact(InstanceData.TagsToMatch) : Container->HasAll(InstanceData.TagsToMatch);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}
	
	return bResult ^ bInvert;
}

//----------------------------------------------------------------------//
//  FGameplayInteractionQuerySlotTagCondition
//----------------------------------------------------------------------//

bool FGameplayInteractionQuerySlotTagCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FGameplayInteractionQuerySlotTagCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FGameplayTagContainer* Container = UE::GameplayInteraction::GetSlotTags(SmartObjectSubsystem, InstanceData.Slot, Source);
	if (Container == nullptr)
	{
		return false;
	}

	return TagQuery.Matches(*Container) ^ bInvert;
}

//----------------------------------------------------------------------//
//  FGameplayInteractionIsSlotHandleValidCondition
//----------------------------------------------------------------------//

bool FGameplayInteractionIsSlotHandleValidCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	return true;
}

bool FGameplayInteractionIsSlotHandleValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return (InstanceData.Slot.IsValid() && SmartObjectSubsystem.IsSmartObjectSlotValid(InstanceData.Slot)) ^ bInvert;
}
