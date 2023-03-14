// Copyright Epic Games, Inc. All Rights Reserved.


#include "Sound/AudioBus.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioBus)

FAudioBusProxy::FAudioBusProxy(UAudioBus* InAudioBus)
{
	if (InAudioBus)
	{
		AudioBusId = InAudioBus->GetUniqueID();
		NumChannels = InAudioBus->GetNumChannels();
	}
}


UAudioBus::UAudioBus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAudioBus::BeginDestroy()
{
	Super::BeginDestroy();

	if (!GEngine)
	{
		return;
	}

	// Make sure we stop all audio bus instances on all devices if this object is getting destroyed
	uint32 AudioBusId = GetUniqueID();

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	if (AudioDeviceManager)
	{
		TArray<FAudioDevice*> AudioDevices = AudioDeviceManager->GetAudioDevices();
		for (FAudioDevice* AudioDevice : AudioDevices)
		{
			if (AudioDevice->IsAudioMixerEnabled())
			{
				Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(AudioDevice);
				MixerDevice->StopAudioBus(AudioBusId);
			}
		}
	}
}

#if WITH_EDITOR
void UAudioBus::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAudioBus, AudioBusChannels))
	{
		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			DeviceManager->IterateOverAllDevices([BusId = GetUniqueID(), NumChannels = AudioBusChannels](Audio::FDeviceId, FAudioDevice* InDevice)
			{
				if (Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(InDevice))
				{
					MixerDevice->StopAudioBus(BusId);
					MixerDevice->StartAudioBus(BusId, (int32)NumChannels + 1, false /* bInIsAutomatic */);
				}
			});
		}
	}
}
#endif // WITH_EDITOR

 TUniquePtr<Audio::IProxyData> UAudioBus::CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	return MakeUnique<FAudioBusProxy>(this);
}

