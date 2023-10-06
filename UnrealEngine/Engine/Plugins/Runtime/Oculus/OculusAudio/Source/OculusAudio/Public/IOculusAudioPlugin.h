// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "OculusAudio.h"
#include "OculusAudioDllManager.h"
#include "OculusAmbisonicSpatializer.h"

/************************************************************************/
/* FOculusAudioPlugin												   */
/* Module interface. Also handles loading and unloading the Oculus Audio*/
/* DLL using FOculusAudioDllManager.									*/
/************************************************************************/
class FOculusAudioPlugin : public IModuleInterface
{
public:
	//~ Begin IModuleInterface 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	void RegisterAudioDevice(FAudioDevice* AudioDeviceHandle);
	void UnregisterAudioDevice(FAudioDevice* AudioDeviceHandle);

	FOculusSpatializationPluginFactory* GetSpatializationPluginFactory() { return &PluginFactory; }
	FOculusReverbPluginFactory* GetReverbPluginFactory() { return &ReverbPluginFactory; }

private:
	TArray<FAudioDevice*> RegisteredAudioDevices;
	FOculusSpatializationPluginFactory PluginFactory;
	FOculusReverbPluginFactory ReverbPluginFactory;
	FOculusAmbisonicsFactory AmbisonicsFactory;
};
