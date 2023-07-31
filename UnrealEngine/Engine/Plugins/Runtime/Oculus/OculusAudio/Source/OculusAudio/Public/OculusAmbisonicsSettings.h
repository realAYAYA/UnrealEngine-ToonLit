// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"

#include "OculusAmbisonicsSettings.generated.h"


UENUM()
enum class EOculusAudioAmbisonicsMode : uint8
{
	// High quality ambisonic spatialization method
	SphericalHarmonics,

	// Alternative ambisonic spatialization method
	VirtualSpeakers,
};

USTRUCT(BlueprintType)
struct OCULUSAUDIO_API FSubmixEffectOculusAmbisonicSpatializerSettings
{
	GENERATED_USTRUCT_BODY()

		// Ambisonic spatialization mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Realtime)
	EOculusAudioAmbisonicsMode AmbisonicMode;

	FSubmixEffectOculusAmbisonicSpatializerSettings()
		: AmbisonicMode(EOculusAudioAmbisonicsMode::SphericalHarmonics)
	{
	}
};

class FOculusAudioSoundfieldSettingsProxy : public ISoundfieldEncodingSettingsProxy
{

public:
	const EOculusAudioAmbisonicsMode SpatailizationMode;

	FOculusAudioSoundfieldSettingsProxy(EOculusAudioAmbisonicsMode InSpatializationMode)
		: SpatailizationMode(InSpatializationMode)
	{}

	virtual uint32 GetUniqueId() const override
	{
		return (uint32)SpatailizationMode;
	}


	virtual TUniquePtr<ISoundfieldEncodingSettingsProxy> Duplicate() const override
	{
		return TUniquePtr<ISoundfieldEncodingSettingsProxy>(new FOculusAudioSoundfieldSettingsProxy(SpatailizationMode));
	}

};

UCLASS()
class OCULUSAUDIO_API UOculusAudioSoundfieldSettings : public USoundfieldEncodingSettingsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ambisonics)
	EOculusAudioAmbisonicsMode SpatializationMode;

	virtual TUniquePtr<ISoundfieldEncodingSettingsProxy> GetProxy() const override
	{
		return TUniquePtr<ISoundfieldEncodingSettingsProxy>(new FOculusAudioSoundfieldSettingsProxy(SpatializationMode));
	}
};

