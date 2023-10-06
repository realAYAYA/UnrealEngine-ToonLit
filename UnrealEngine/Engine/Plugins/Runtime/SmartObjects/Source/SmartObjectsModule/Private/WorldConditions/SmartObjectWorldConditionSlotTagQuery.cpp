// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditions/SmartObjectWorldConditionSlotTagQuery.h"
#include "SmartObjectSubsystem.h"
#include "WorldConditionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectWorldConditionSlotTagQuery)

#define LOCTEXT_NAMESPACE "SmartObjects"

#if WITH_EDITOR
FText FSmartObjectWorldConditionSlotTagQuery::GetDescription() const
{
	return LOCTEXT("SlotTagQueryDesc", "Match Runtime Slot Tags");
}
#endif // WITH_EDITOR

bool FSmartObjectWorldConditionSlotTagQuery::Initialize(const UWorldConditionSchema& Schema)
{
	const USmartObjectWorldConditionSchema* SmartObjectSchema = Cast<USmartObjectWorldConditionSchema>(&Schema);
	if (!SmartObjectSchema)
	{
		UE_LOG(LogSmartObject, Error, TEXT("SmartObjectWorldConditionSlotTagQuery: Expecting schema based on SmartObjectWorldConditionSchema."));
		return false;
	}

	SubsystemRef = SmartObjectSchema->GetSubsystemRef();
	SlotHandleRef = SmartObjectSchema->GetSlotHandleRef();
	bCanCacheResult	= Schema.GetContextDataTypeByRef(SlotHandleRef) == EWorldConditionContextDataType::Persistent;
	
	return true;
}

bool FSmartObjectWorldConditionSlotTagQuery::Activate(const FWorldConditionContext& Context) const
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	// Use a callback to listen changed to persistent data.
	if (Context.GetContextDataType(SlotHandleRef) == EWorldConditionContextDataType::Persistent)
	{
		if (const FSmartObjectSlotHandle* SlotHandle = Context.GetContextDataPtr<FSmartObjectSlotHandle>(SlotHandleRef))
		{
			if (FOnSmartObjectEvent* SlotDelegate = SmartObjectSubsystem->GetSlotEventDelegate(*SlotHandle))
			{
				FStateType& State = Context.GetState(*this);
				State.SlotHandle = *SlotHandle;
				State.DelegateHandle = SlotDelegate->AddLambda([InvalidationHandle = Context.GetInvalidationHandle(*this), TargetSlot = *SlotHandle](const FSmartObjectEventData& Event)
					{
						if (TargetSlot == Event.SlotHandle
							&& (Event.Reason == ESmartObjectChangeReason::OnTagAdded
								|| Event.Reason == ESmartObjectChangeReason::OnTagRemoved))
						{
							InvalidationHandle.InvalidateResult();
						}
					});
				
				return true;
			}
		}
		// Failed to find the data.
		return false;
	}

	// Dynamic data, do not expect input to be valid on Activate().
	return true;
}

FWorldConditionResult FSmartObjectWorldConditionSlotTagQuery::IsTrue(const FWorldConditionContext& Context) const
{
	const USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	FWorldConditionResult Result(EWorldConditionResultValue::IsFalse, bCanCacheResult);
	if (const FSmartObjectSlotHandle* SlotHandle = Context.GetContextDataPtr<FSmartObjectSlotHandle>(SlotHandleRef))
	{
		const FGameplayTagContainer& SlotTags = SmartObjectSubsystem->GetSlotTags(*SlotHandle);
		if (TagQuery.Matches(SlotTags))
		{
			Result.Value = EWorldConditionResultValue::IsTrue;
		}
	}

	return Result;
}

void FSmartObjectWorldConditionSlotTagQuery::Deactivate(const FWorldConditionContext& Context) const
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	FStateType& State = Context.GetState(*this);

	if (State.DelegateHandle.IsValid() && State.SlotHandle.IsValid())
	{
		if (FOnSmartObjectEvent* SlotDelegate = SmartObjectSubsystem->GetSlotEventDelegate(State.SlotHandle))
		{
			SlotDelegate->Remove(State.DelegateHandle);
		}
		State.DelegateHandle.Reset();
		State.SlotHandle = FSmartObjectSlotHandle();
	}
}

#undef LOCTEXT_NAMESPACE
