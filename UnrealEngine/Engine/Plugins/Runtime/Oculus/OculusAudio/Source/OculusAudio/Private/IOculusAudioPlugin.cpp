// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOculusAudioPlugin.h"
#include "OculusAudioContextManager.h"
#include "AudioDevice.h"
#include "Features/IModularFeatures.h"

void FOculusAudioPlugin::StartupModule()
{
	FOculusAudioLibraryManager::Get().Initialize();
	IModularFeatures::Get().RegisterModularFeature(FOculusSpatializationPluginFactory::GetModularFeatureName(), &PluginFactory);
	IModularFeatures::Get().RegisterModularFeature(FOculusReverbPluginFactory::GetModularFeatureName(), &ReverbPluginFactory);
};

void FOculusAudioPlugin::ShutdownModule()
{
	FOculusAudioLibraryManager::Get().Shutdown();
}

void FOculusAudioPlugin::RegisterAudioDevice(FAudioDevice* AudioDeviceHandle)
{
	// Inject the Context into the spatailizer (and reverb) if they're enabled
	if (!AudioDeviceHandle->IsAudioMixerEnabled())
	{
		return; // Not supported in old audio engine
	}

	if (!RegisteredAudioDevices.Contains(AudioDeviceHandle))
	{
		TAudioPluginListenerPtr ContextManager = TAudioPluginListenerPtr(new FOculusAudioContextManager());
		AudioDeviceHandle->RegisterPluginListener(ContextManager);
		RegisteredAudioDevices.Add(AudioDeviceHandle);
	}
}

void FOculusAudioPlugin::UnregisterAudioDevice(FAudioDevice* AudioDeviceHandle)
{
	RegisteredAudioDevices.Remove(AudioDeviceHandle);
}

IMPLEMENT_MODULE(FOculusAudioPlugin, OculusAudio)