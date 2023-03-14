// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "MotoSynthSourceAsset.h"
#include "SynthComponentMoto.generated.h"


UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class MOTOSYNTH_API USynthComponentMoto : public USynthComponent
{
	GENERATED_BODY()
public:

	USynthComponentMoto(const FObjectInitializer& ObjInitializer);
	virtual ~USynthComponentMoto();

	/* The moto synth preset to use for the moto synth component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotoSynth")
	TObjectPtr<UMotoSynthPreset> MotoSynthPreset;

	/* Sets the starting RPM of the engine */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotoSynth", meta = (ClampMin = "500.0", ClampMax = "20000.0", UIMin = "500.0", UIMax = "20000.0"))
	float RPM = 1000.0f;

	/** Sets the RPM of the granular engine directly. */
	UFUNCTION(BlueprintCallable, Category = "MotoSynth")
	void SetRPM(float InRPM, float InTimeSec);

	/** Sets a moto synth settings dynamically. */
	UFUNCTION(BlueprintCallable, Category = "MotoSynth")
	void SetSettings(const FMotoSynthRuntimeSettings& InSettings);

	/** Retrieves RPM range of the moto synth, taking into account the acceleration and deceleration sources. The min RPM is the largest of the min RPms of either and the max RPM is min of the max RPMs of either. */
	UFUNCTION(BlueprintCallable, Category = "MotoSynth")
	void GetRPMRange(float& OutMinRPM, float& OutMaxRPM);

	/** Returns if the moto synth is enabled. */
	UFUNCTION(BlueprintCallable, Category = "MotoSynth")
	bool IsEnabled() const;

	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	FMotoSynthRuntimeSettings* GetSettingsToUse();

	FCriticalSection SettingsCriticalSection;
	FVector2D RPMRange;
	ISoundGeneratorPtr MotoSynthEngine;
	FMotoSynthRuntimeSettings OverrideSettings;
	bool bSettingsOverridden = false;
};
