// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAudioModulation.h"
#include "SoundModulationValue.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

#include "SoundControlBusMix.generated.h"

// Forward Declarations
class USoundControlBus;


USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundControlBusMixStage
{
	GENERATED_USTRUCT_BODY()

	FSoundControlBusMixStage();
	FSoundControlBusMixStage(USoundControlBus* InBus, const float TargetValue);

	/* Bus controlled by stage. */
	UPROPERTY(EditAnywhere, Category = Stage, BlueprintReadWrite)
	TObjectPtr<USoundControlBus> Bus;

	/* Value mix is set to. */
	UPROPERTY(EditAnywhere, Category = Stage, BlueprintReadWrite)
	FSoundModulationMixValue Value;
};

UCLASS(config = Engine, autoexpandcategories = (Stage, Mix), editinlinenew, BlueprintType, MinimalAPI)
class USoundControlBusMix : public UObject
{
	GENERATED_UCLASS_BODY()

protected:
	// Loads the mix from the provided profile index
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void LoadMixFromProfile();

	// Saves the mix to the provided profile index
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void SaveMixToProfile();

	// Solos this mix, deactivating all others and activating this
	// (if its not already active), while testing in-editor in all
	// active worlds
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void SoloMix();

	// Activates this mix in all active worlds
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void ActivateMix();

	// Deactivates this mix in all active worlds
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void DeactivateMix();

	// Deactivates all mixes in all active worlds
	UFUNCTION(Category = Mix, meta = (CallInEditor = "true"))
	void DeactivateAllMixes();

public:
	UPROPERTY(EditAnywhere, Transient, Category = Mix)
	uint32 ProfileIndex;

	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
	virtual void OnPropertyChanged(FProperty* Property, EPropertyChangeType::Type ChangeType);
#endif // WITH_EDITOR

	/* Array of stages controlled by mix. */
	UPROPERTY(EditAnywhere, Category = Mix, BlueprintReadOnly)
	TArray<FSoundControlBusMixStage> MixStages;
};
