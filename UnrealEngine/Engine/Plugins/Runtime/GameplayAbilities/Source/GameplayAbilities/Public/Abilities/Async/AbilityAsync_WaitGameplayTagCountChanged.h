// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityAsync.h"
#include "AbilityAsync_WaitGameplayTagCountChanged.generated.h"

UCLASS()
class GAMEPLAYABILITIES_API UAbilityAsync_WaitGameplayTagCountChanged : public UAbilityAsync
{
	GENERATED_BODY()
protected:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncWaitGameplayTagCountDelegate, int32, TagCount);

	virtual void Activate() override;
	virtual void EndAction() override;

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);

	FGameplayTag Tag;
	FDelegateHandle GameplayTagCountChangedHandle;

public:
	UPROPERTY(BlueprintAssignable)
	FAsyncWaitGameplayTagCountDelegate TagCountChanged;

	/**
	 * Wait until the specified gameplay tag count changes on Target Actor's ability component
	 * If used in an ability graph, this async action will wait even after activation ends. It's recommended to use WaitGameplayTagCountChange instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityAsync_WaitGameplayTagCountChanged* WaitGameplayTagCountChangedOnActor(AActor* TargetActor, FGameplayTag Tag);
};