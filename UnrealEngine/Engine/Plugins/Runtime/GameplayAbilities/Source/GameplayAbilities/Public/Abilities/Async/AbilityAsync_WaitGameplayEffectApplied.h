// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilityAsync.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "GameplayEffectTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "AbilityAsync_WaitGameplayEffectApplied.generated.h"

class UAbilitySystemComponent;

/**
 * This action listens for specific gameplay effect applications based off specified tags. 
 * Effects themselves are not replicated; rather the tags they grant, the attributes they 
 * change, and the gameplay cues they emit are replicated.
 * This will only listen for local server or predicted client effects.
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityAsync_WaitGameplayEffectApplied : public UAbilityAsync
{
	GENERATED_BODY()

public:
	/**
	 * Wait until a GameplayEffect is applied to a target actor
	 * If TriggerOnce is true, this action will only activate one time. Otherwise it will return every time a GE is applied that meets the requirements over the life of the ability
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Async", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityAsync_WaitGameplayEffectApplied* WaitGameplayEffectAppliedToActor(AActor* TargetActor, const FGameplayTargetDataFilterHandle SourceFilter, FGameplayTagRequirements SourceTagRequirements, FGameplayTagRequirements TargetTagRequirements, bool TriggerOnce = false, bool ListenForPeriodicEffect = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAppliedDelegate, AActor*, Source, FGameplayEffectSpecHandle, SpecHandle, FActiveGameplayEffectHandle, ActiveHandle);
	UPROPERTY(BlueprintAssignable)
	FOnAppliedDelegate OnApplied;

protected:
	virtual void Activate() override;
	virtual void EndAction() override;

	void OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	FGameplayTargetDataFilterHandle Filter;
	FGameplayTagRequirements SourceTagRequirements;
	FGameplayTagRequirements TargetTagRequirements;

	bool TriggerOnce = false;
	bool ListenForPeriodicEffects = false;

	FDelegateHandle OnApplyGameplayEffectCallbackDelegateHandle;
	FDelegateHandle OnPeriodicGameplayEffectExecuteCallbackDelegateHandle;
	bool bLocked = false;
};
