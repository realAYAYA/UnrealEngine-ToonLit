// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "Misc/Build.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#include "AudioModulationSettings.generated.h"


UCLASS(config=AudioModulation, defaultconfig, meta = (DisplayName = "Audio Modulation"))
class AUDIOMODULATION_API UAudioModulationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Array of Modulation Parameters that are loaded on plugin startup
	UPROPERTY(config, EditAnywhere, Category = "Parameters", meta = (AllowedClasses = "/Script/AudioModulation.SoundModulationParameter"))
	TArray<FSoftObjectPath> Parameters;

	void RegisterParameters() const;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};