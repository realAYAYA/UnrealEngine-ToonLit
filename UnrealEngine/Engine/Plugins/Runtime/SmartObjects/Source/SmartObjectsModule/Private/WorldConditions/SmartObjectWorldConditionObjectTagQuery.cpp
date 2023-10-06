// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditions/SmartObjectWorldConditionObjectTagQuery.h"
#include "SmartObjectSubsystem.h"
#include "WorldConditionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectWorldConditionObjectTagQuery)

#define LOCTEXT_NAMESPACE "SmartObjects"

#if WITH_EDITOR
FText FSmartObjectWorldConditionObjectTagQuery::GetDescription() const
{
	return LOCTEXT("ObjectTagQueryDesc", "Match Runtime Object Tags");
}
#endif // WITH_EDITOR

bool FSmartObjectWorldConditionObjectTagQuery::Initialize(const UWorldConditionSchema& Schema)
{
	const USmartObjectWorldConditionSchema* SmartObjectSchema = Cast<USmartObjectWorldConditionSchema>(&Schema);
	if (!SmartObjectSchema)
	{
		UE_LOG(LogSmartObject, Error, TEXT("SmartObjectWorldConditionObjectTagQuery: Expecting schema based on SmartObjectWorldConditionSchema."));
		return false;
	}

	SubsystemRef = SmartObjectSchema->GetSubsystemRef();
	ObjectHandleRef = SmartObjectSchema->GetSmartObjectHandleRef();
	bCanCacheResult = Schema.GetContextDataTypeByRef(ObjectHandleRef) == EWorldConditionContextDataType::Persistent;

	return true;
}

bool FSmartObjectWorldConditionObjectTagQuery::Activate(const FWorldConditionContext& Context) const
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	// Use a callback to listen changes to persistent data.
	if (Context.GetContextDataType(ObjectHandleRef) == EWorldConditionContextDataType::Persistent)
	{
		if (const FSmartObjectHandle* ObjectHandle = Context.GetContextDataPtr<FSmartObjectHandle>(ObjectHandleRef))
		{
			if (FOnSmartObjectEvent* Delegate = SmartObjectSubsystem->GetEventDelegate(*ObjectHandle))
			{
				FStateType& State = Context.GetState(*this);
				State.DelegateHandle = Delegate->AddLambda([InvalidationHandle = Context.GetInvalidationHandle(*this)](const FSmartObjectEventData& Event)
				{
					if (Event.SlotHandle.IsValid() == false
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

FWorldConditionResult FSmartObjectWorldConditionObjectTagQuery::IsTrue(const FWorldConditionContext& Context) const
{
	const USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	FWorldConditionResult Result(EWorldConditionResultValue::IsFalse, bCanCacheResult);
	if (const FSmartObjectHandle* ObjectHandle = Context.GetContextDataPtr<FSmartObjectHandle>(ObjectHandleRef))
	{
		const FGameplayTagContainer& ObjectTags = SmartObjectSubsystem->GetInstanceTags(*ObjectHandle);
		if (TagQuery.Matches(ObjectTags))
		{
			Result.Value =EWorldConditionResultValue::IsTrue; 
		}
	}

	return Result;
}

void FSmartObjectWorldConditionObjectTagQuery::Deactivate(const FWorldConditionContext& Context) const
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetContextDataPtr<USmartObjectSubsystem>(SubsystemRef);
	check(SmartObjectSubsystem);

	FStateType& State = Context.GetState(*this);

	if (State.DelegateHandle.IsValid())
	{
		if (const FSmartObjectHandle* ObjectHandle = Context.GetContextDataPtr<FSmartObjectHandle>(ObjectHandleRef))
		{
			if (FOnSmartObjectEvent* Delegate = SmartObjectSubsystem->GetEventDelegate(*ObjectHandle))
			{
				Delegate->Remove(State.DelegateHandle);
			}
		}
		State.DelegateHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
