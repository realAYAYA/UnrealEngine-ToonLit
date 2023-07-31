// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"

#include "OculusAudioSourceSettings.generated.h"

UCLASS()
class OCULUSAUDIO_API UOculusAudioSourceSettings : public USpatializationPluginSourceSettingsBase
{
	GENERATED_BODY()

public:
	UOculusAudioSourceSettings();

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings")
	bool EarlyReflectionsEnabled;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings")
	bool AttenuationEnabled;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings")
	float AttenuationRangeMinimum;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings")
	float AttenuationRangeMaximum;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings")
	float VolumetricRadius;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SpatializationSettings", meta = (ClampMin = "-60.0", ClampMax = "20.0", UIMin = "-60.0", UIMax = "20.0"))
	float ReverbSendLevel;
};

