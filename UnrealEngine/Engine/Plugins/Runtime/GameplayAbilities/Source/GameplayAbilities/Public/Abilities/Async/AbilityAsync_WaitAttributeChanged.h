// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilityAsync.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityAsync_WaitAttributeChanged.generated.h"

class UAbilitySystemComponent;

UCLASS()
class GAMEPLAYABILITIES_API UAbilityAsync_WaitAttributeChanged : public UAbilityAsync
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityAsync_WaitAttributeChanged* WaitForAttributeChanged(AActor* TargetActor, FGameplayAttribute Attribute, bool OnlyTriggerOnce = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAsyncWaitAttributeChangedDelegate, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);
	UPROPERTY(BlueprintAssignable)
	FAsyncWaitAttributeChangedDelegate Changed;

protected:

	virtual void Activate() override;
	virtual void EndAction() override;

	void OnAttributeChanged(const FOnAttributeChangeData& ChangeData);

	FGameplayAttribute Attribute;
	bool OnlyTriggerOnce = false;

	FDelegateHandle MyHandle;
};
