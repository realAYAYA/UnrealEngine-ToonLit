// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/StateTreeGameplayTagConditions.h"
#include "GameplayTagContainer.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeGameplayTagConditions)

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Conditions
{
	FText GetContainerAsText(const FGameplayTagContainer& TagContainer, const int ApproxMaxLength = 60)
	{
		FString Combined;
		for (const FGameplayTag& Tag : TagContainer)
		{
			FString TagString = Tag.ToString();

			if (Combined.Len() > 0)
			{
				Combined += TEXT(", ");
			}
			
			if (Combined.Len() + TagString.Len() > ApproxMaxLength)
			{
				// Overflow
				if (Combined.Len() == 0)
				{
					Combined += TagString.Left(ApproxMaxLength);
				}
				Combined += TEXT("...");
				break;
			}

			Combined += TagString;
		}

		return FText::FromString(Combined);
	}
	
}

#endif// WITH_EDITOR


//----------------------------------------------------------------------//
//  FGameplayTagMatchCondition
//----------------------------------------------------------------------//

bool FGameplayTagMatchCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	return (bExactMatch ? InstanceData.TagContainer.HasTagExact(InstanceData.Tag) : InstanceData.TagContainer.HasTag(InstanceData.Tag)) ^ bInvert;
}

//----------------------------------------------------------------------//
//  FGameplayTagContainerMatchCondition
//----------------------------------------------------------------------//

bool FGameplayTagContainerMatchCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bResult = false;
	switch (MatchType)
	{
	case EGameplayContainerMatchType::Any:
		bResult = bExactMatch ? InstanceData.TagContainer.HasAnyExact(InstanceData.OtherContainer) : InstanceData.TagContainer.HasAny(InstanceData.OtherContainer);
		break;
	case EGameplayContainerMatchType::All:
		bResult = bExactMatch ? InstanceData.TagContainer.HasAllExact(InstanceData.OtherContainer) : InstanceData.TagContainer.HasAll(InstanceData.OtherContainer);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}
	
	return bResult ^ bInvert;
}


//----------------------------------------------------------------------//
//  FGameplayTagQueryCondition
//----------------------------------------------------------------------//

bool FGameplayTagQueryCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return TagQuery.Matches(InstanceData.TagContainer) ^ bInvert;
}


#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR

