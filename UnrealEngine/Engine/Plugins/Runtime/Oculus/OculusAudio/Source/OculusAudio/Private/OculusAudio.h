// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"
#include "OculusAudioMixer.h"
#include "OculusAudioSourceSettings.h"
#include "OculusAmbisonicSpatializer.h"

/************************************************************************/
/* FOculusSpatializationPluginFactory								   */
/* Handles initialization of the required Oculus Audio Spatialization   */
/* plugin.															  */
/************************************************************************/
class FOculusSpatializationPluginFactory : public IAudioSpatializationFactory
{
public:
	//~ Begin IAudioSpatializationFactory
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Oculus Audio"));
		return DisplayName;
	}

	virtual bool SupportsPlatform(const FString& PlatformName) override
	{
		return	PlatformName == FString(TEXT("Windows")) || PlatformName == FString(TEXT("Android"));
	}

	virtual UClass* GetCustomSpatializationSettingsClass() const override { return UOculusAudioSourceSettings::StaticClass(); }

	virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) override;
	//~ End IAudioSpatializationFactory

};

class FOculusReverbPluginFactory : public IAudioReverbFactory
{
public:
	//~ Begin IAudioReverbFactory
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Oculus Audio"));
		return DisplayName;
	}

	virtual bool SupportsPlatform(const FString& PlatformName) override
	{
		return	PlatformName == FString(TEXT("Windows")) || PlatformName == FString(TEXT("Android"));
	}

	virtual TAudioReverbPtr CreateNewReverbPlugin(FAudioDevice* OwningDevice) override;
	//~ End IAudioReverbFactory
};

