// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudio.h"
#include "OculusAmbisonicSpatializer.h"
#include "OculusAudioReverb.h"
#include "IOculusAudioPlugin.h"
#include "Features/IModularFeatures.h"

TAudioSpatializationPtr FOculusSpatializationPluginFactory::CreateNewSpatializationPlugin(FAudioDevice* OwningDevice)
{
	FOculusAudioPlugin* Plugin = &FModuleManager::GetModuleChecked<FOculusAudioPlugin>("OculusAudio");
	check(Plugin != nullptr);
	Plugin->RegisterAudioDevice(OwningDevice);

	return TAudioSpatializationPtr(new OculusAudioSpatializationAudioMixer());
}


TAudioReverbPtr FOculusReverbPluginFactory::CreateNewReverbPlugin(FAudioDevice* OwningDevice)
{
	FOculusAudioPlugin* Plugin = &FModuleManager::GetModuleChecked<FOculusAudioPlugin>("OculusAudio");
	check(Plugin != nullptr);
	Plugin->RegisterAudioDevice(OwningDevice);

	return TAudioReverbPtr(new OculusAudioReverb());
}