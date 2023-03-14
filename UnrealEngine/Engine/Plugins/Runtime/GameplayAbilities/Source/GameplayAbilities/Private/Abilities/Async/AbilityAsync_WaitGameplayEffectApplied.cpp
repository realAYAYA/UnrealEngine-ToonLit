// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/Async/AbilityAsync_WaitGameplayEffectApplied.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityAsync_WaitGameplayEffectApplied)

UAbilityAsync_WaitGameplayEffectApplied* UAbilityAsync_WaitGameplayEffectApplied::WaitGameplayEffectAppliedToActor(AActor* TargetActor, const FGameplayTargetDataFilterHandle SourceFilter, FGameplayTagRequirements SourceTagRequirements, FGameplayTagRequirements TargetTagRequirements, bool TriggerOnce, bool ListenForPeriodicEffect)
{
	UAbilityAsync_WaitGameplayEffectApplied* MyObj = NewObject<UAbilityAsync_WaitGameplayEffectApplied>();
	MyObj->SetAbilityActor(TargetActor);
	MyObj->Filter = SourceFilter;
	MyObj->SourceTagRequirements = SourceTagRequirements;
	MyObj->TargetTagRequirements = TargetTagRequirements;
	MyObj->TriggerOnce = TriggerOnce;
	MyObj->ListenForPeriodicEffects = ListenForPeriodicEffect;
	return MyObj;
}

void UAbilityAsync_WaitGameplayEffectApplied::Activate()
{
	Super::Activate();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		OnApplyGameplayEffectCallbackDelegateHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UAbilityAsync_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback);
		if (ListenForPeriodicEffects)
		{
			OnPeriodicGameplayEffectExecuteCallbackDelegateHandle = ASC->OnPeriodicGameplayEffectExecuteDelegateOnSelf.AddUObject(this, &UAbilityAsync_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback);
		}
	}
	else
	{
		EndAction();
	}
}

void UAbilityAsync_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	AActor* AvatarActor = Target ? Target->GetAvatarActor_Direct() : nullptr;

	if (!Filter.FilterPassesForActor(AvatarActor))
	{
		return;
	}
	if (!SourceTagRequirements.RequirementsMet(*SpecApplied.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*SpecApplied.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}

	if (bLocked)
	{
		ABILITY_LOG(Error, TEXT("WaitGameplayEffectApplied recursion detected. Action: %s. Applied Spec: %s. This could cause an infinite loop! Ignoring"), *GetPathName(), *SpecApplied.ToSimpleString());
		return;
	}

	FGameplayEffectSpecHandle SpecHandle(new FGameplayEffectSpec(SpecApplied));

	if (ShouldBroadcastDelegates())
	{
		TGuardValue<bool> GuardValue(bLocked, true);
		OnApplied.Broadcast(AvatarActor, SpecHandle, ActiveHandle);

		if (TriggerOnce)
		{
			EndAction();
		}
	}
	else
	{
		EndAction();
	}
}

void UAbilityAsync_WaitGameplayEffectApplied::EndAction()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (OnPeriodicGameplayEffectExecuteCallbackDelegateHandle.IsValid())
		{
			ASC->OnPeriodicGameplayEffectExecuteDelegateOnSelf.Remove(OnPeriodicGameplayEffectExecuteCallbackDelegateHandle);
		}
		ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(OnApplyGameplayEffectCallbackDelegateHandle);
	}
	Super::EndAction();
}

