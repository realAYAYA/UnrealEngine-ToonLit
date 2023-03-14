// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilityAsync.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityAsync_WaitGameplayTag.generated.h"

class UAbilitySystemComponent;

UCLASS(Abstract)
class GAMEPLAYABILITIES_API UAbilityAsync_WaitGameplayTag : public UAbilityAsync
{
	GENERATED_BODY()
protected:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAsyncWaitGameplayTagDelegate);

	virtual void Activate() override;
	virtual void EndAction() override;

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);
	virtual void BroadcastDelegate();

	int32 TargetCount = -1;
	FGameplayTag Tag;
	bool OnlyTriggerOnce = false;

	FDelegateHandle MyHandle;
};

UCLASS()
class GAMEPLAYABILITIES_API UAbilityAsync_WaitGameplayTagAdded : public UAbilityAsync_WaitGameplayTag
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintAssignable)
	FAsyncWaitGameplayTagDelegate Added;

	/**
	 * Wait until the specified gameplay tag is Added to Target Actor's ability component
	 * If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityAsync_WaitGameplayTagAdded* WaitGameplayTagAddToActor(AActor* TargetActor, FGameplayTag Tag, bool OnlyTriggerOnce=false);

	virtual void BroadcastDelegate() override;
};

UCLASS()
class GAMEPLAYABILITIES_API UAbilityAsync_WaitGameplayTagRemoved : public UAbilityAsync_WaitGameplayTag
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintAssignable)
	FAsyncWaitGameplayTagDelegate Removed;

	/**
	 * Wait until the specified gameplay tag is Removed from Target Actor's ability component
	 * If the tag is not present when this task is started, it will immediately broadcast the Removed event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityAsync_WaitGameplayTagRemoved* WaitGameplayTagRemoveFromActor(AActor* TargetActor, FGameplayTag Tag, bool OnlyTriggerOnce=false);

	virtual void BroadcastDelegate() override;
};