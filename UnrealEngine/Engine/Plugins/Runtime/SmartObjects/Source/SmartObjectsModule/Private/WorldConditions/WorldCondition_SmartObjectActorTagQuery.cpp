// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditions/WorldCondition_SmartObjectActorTagQuery.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "SmartObjectTypes.h"
#include "WorldConditionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "WorldConditions/SmartObjectWorldConditionSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldCondition_SmartObjectActorTagQuery)

#define LOCTEXT_NAMESPACE "SmartObjects"

#if WITH_EDITOR
FText FWorldCondition_SmartObjectActorTagQuery::GetDescription() const
{
	return LOCTEXT("ActorTagQueryDesc", "Match SmartObject Actor Tags");
}
#endif // WITH_EDITOR

bool FWorldCondition_SmartObjectActorTagQuery::Initialize(const UWorldConditionSchema& Schema)
{
	const USmartObjectWorldConditionSchema* SmartObjectSchema = Cast<USmartObjectWorldConditionSchema>(&Schema);
	if (SmartObjectSchema == nullptr)
	{
		UE_LOG(LogSmartObject, Error, TEXT("[%hs] Expecting schema based on %s."), __FUNCTION__, *USmartObjectWorldConditionSchema::StaticClass()->GetName());
		return false;
	}

	SmartObjectActorRef = SmartObjectSchema->GetSmartObjectActorRef();

	bCanCacheResult = Schema.GetContextDataTypeByRef(SmartObjectActorRef) == EWorldConditionContextDataType::Persistent;

	return true;
}

bool FWorldCondition_SmartObjectActorTagQuery::Activate(const FWorldConditionContext& Context) const
{
	if (!SmartObjectActorRef.IsValid())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogWorldCondition, Error, TEXT("[%s] The provided 'SmartObjectActorRef' is not set! Owner: %s ; SmartObjectActorRef: %s"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Context.GetOwner()), *SmartObjectActorRef.GetName().ToString());
		
		return false;
	}
	
	const AActor* const SmartObjectActor = Context.GetContextDataPtr<AActor>(SmartObjectActorRef);

	if (TagQuery.IsEmpty())
	{
		UE_VLOG_UELOG(Context.GetOwner(), LogWorldCondition, Error, TEXT("[%s] The provided 'TagQuery' is empty! Owner: %s ; SmartObjectActor: %s"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Context.GetOwner()), *GetNameSafe(SmartObjectActor));
		
		return false;
	}

	if (bCanCacheResult)
	{
		if (UAbilitySystemComponent* const AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SmartObjectActor))
		{
			FStateType& State = Context.GetState(*this);
			ensure(!State.DelegateHandle.IsValid());
			
			State.DelegateHandle = AbilitySystemComponent->RegisterGenericGameplayTagEvent().AddLambda([this, InvalidationHandle = Context.GetInvalidationHandle(*this)](const FGameplayTag InTag, int32)
			{
				// Get the list of all unique gameplay tags referenced by the query so we can invalidate our result if the added/removed tag is one of them.
				TArray<FGameplayTag> QueryTags;
				TagQuery.GetGameplayTagArray(QueryTags);
				if (QueryTags.Contains(InTag))
				{
					InvalidationHandle.InvalidateResult();
				}
			});
		}
		else if (Cast<IGameplayTagAssetInterface>(SmartObjectActor) == nullptr)
		{
			UE_VLOG_UELOG(Context.GetOwner(), LogWorldCondition, Error,
				TEXT("[%s] The provided 'SmartObjectActor' does not implement IGameplayTagAssetInterface or does not have an AbilitySystemComponent. Owner: %s ; SmartObjectActor: %s"),
				ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Context.GetOwner()), *GetNameSafe(SmartObjectActor));
			
			return false;
		}
	}

	return true;
}

FWorldConditionResult FWorldCondition_SmartObjectActorTagQuery::IsTrue(const FWorldConditionContext& Context) const
{
	const AActor* const SmartObjectActor = Context.GetContextDataPtr<AActor>(SmartObjectActorRef);
	const IGameplayTagAssetInterface* GameplayTagAssetInterface = Cast<IGameplayTagAssetInterface>(SmartObjectActor);
	if (GameplayTagAssetInterface == nullptr)
	{
		if (const UAbilitySystemComponent* const AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SmartObjectActor))
		{
			GameplayTagAssetInterface = AbilitySystemComponent;
		}
	}

	FStateType& State = Context.GetState(*this);
	const bool bResultCanBeCached = State.DelegateHandle.IsValid();
	FWorldConditionResult Result(EWorldConditionResultValue::IsFalse, bResultCanBeCached);
	if (GameplayTagAssetInterface != nullptr)
	{
		FGameplayTagContainer Tags;
		GameplayTagAssetInterface->GetOwnedGameplayTags(Tags);
		if (TagQuery.Matches(Tags))
		{
			Result.Value = EWorldConditionResultValue::IsTrue;
		}
	}

	return Result;
}

void FWorldCondition_SmartObjectActorTagQuery::Deactivate(const FWorldConditionContext& Context) const
{
	FStateType& State = Context.GetState(*this);

	if (State.DelegateHandle.IsValid())
	{
		const AActor* const SmartObjectActor = Context.GetContextDataPtr<AActor>(SmartObjectActorRef);
		if (UAbilitySystemComponent* const AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SmartObjectActor))
		{
			AbilitySystemComponent->RegisterGenericGameplayTagEvent().Remove(State.DelegateHandle);
		}
		
		State.DelegateHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
