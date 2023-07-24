// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectUIData.h"
#include "UObject/ObjectPtr.h"

#include "TopDownArenaPickupUIData.generated.h"

class UNiagaraSystem;
class UObject;
class USoundBase;
class UTexture2D;

// Icon and display name for pickups in the top-down arena game
UCLASS(BlueprintType)
class UTopDownArenaPickupUIData : public UGameplayEffectUIData
{
	GENERATED_BODY()

public:

	// The full description of the pickup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data, meta=(MultiLine="true"))
	FText Description;

	// The short description of the pickup (displayed by the player name when picked up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data, meta=(MultiLine="true"))
	FText ShortDescriptionForToast;
	
	// The icon material used to show the pickup in the world
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data)
	TObjectPtr<UTexture2D> IconTexture;

	// The pickup VFX override
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	TObjectPtr<UNiagaraSystem> PickupVFX;

	// The pickup SFX override (if not set, a default will play)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	TObjectPtr<USoundBase> PickupSFX;
};
