// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OculusAudioSettings.generated.h"


UCLASS(config = Engine, defaultconfig)
class OCULUSAUDIO_API UOculusAudioSettings : public UObject
{
	GENERATED_BODY()

public:

	UOculusAudioSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb", meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
	FSoftObjectPath OutputSubmix;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Reverb", meta = (ClampMin = "-60.0", ClampMax = "20.0", UIMin = "-60.0", UIMax = "20.0"))
	float ReverbWetLevel;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling")
	bool EarlyReflections;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling")
	bool LateReverberation;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Propagation Quality", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float PropagationQuality;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "200.0", UIMin = "0.0", UIMax = "200.0"))
	float Width;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "200.0", UIMin = "0.0", UIMax = "200.0"))
	float Height;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "200.0", UIMin = "0.0", UIMax = "200.0"))
	float Depth;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefRight;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefLeft;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefUp;
	
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefDown;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefBack;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Room Modeling", meta = (ClampMin = "0.0", ClampMax = "0.97", UIMin = "0.0", UIMax = "0.97"))
	float ReflectionCoefFront;
};