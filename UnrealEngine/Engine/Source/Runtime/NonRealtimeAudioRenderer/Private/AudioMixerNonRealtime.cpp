// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformNonRealtime.h"
#include "Modules/ModuleManager.h"

class FAudioMixerModuleNonRealtime : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformNonRealtime());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleNonRealtime, NonRealtimeAudioRenderer);
