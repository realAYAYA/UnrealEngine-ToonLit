// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectComponent.h"
#include "GameplayEffectCustomApplicationRequirement.h"

#include "CustomCanApplyGameplayEffectComponent.generated.h"

/** Handles configuration of a CustomApplicationRequirement function to see if this GameplayEffect should apply */
UCLASS(DisplayName="Custom Can Apply This Effect")
class GAMEPLAYABILITIES_API UCustomCanApplyGameplayEffectComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()
	
public:
	UCustomCanApplyGameplayEffectComponent();

	/** Determine if we can apply this GameplayEffect or not */
	virtual bool CanGameplayEffectApply(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& GESpec) const override;

public:
	/** Custom application requirements */
	UPROPERTY(EditDefaultsOnly, Category = Application, meta = (DisplayName = "Custom Application Requirement"))
	TArray<TSubclassOf<UGameplayEffectCustomApplicationRequirement>> ApplicationRequirements;
};
