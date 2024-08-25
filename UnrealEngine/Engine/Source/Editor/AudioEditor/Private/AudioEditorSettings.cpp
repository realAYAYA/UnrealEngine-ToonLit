// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioEditorSettings.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"

struct FPropertyChangedEvent;

void UAudioEditorSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplyAttenuationForAllAudioDevices();
	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddUObject(this, &UAudioEditorSettings::ApplyAttenuationForAudioDevice);
}

void UAudioEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAudioEditorSettings, bUseAudioAttenuation))
	{
		ApplyAttenuationForAllAudioDevices();
	}
}

void UAudioEditorSettings::SetUseAudioAttenuation(bool bInUseAudioAttenuation) 
{
	bUseAudioAttenuation = bInUseAudioAttenuation;
	SaveConfig();
	ApplyAttenuationForAllAudioDevices();
}

void UAudioEditorSettings::ApplyAttenuationForAllAudioDevices()
{
	if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
	{
		TArray<FAudioDevice*> AudioDevices = AudioDeviceManager->GetAudioDevices();
		for (FAudioDevice* Device : AudioDevices)
		{
			if (Device)
			{
				Device->SetUseAttenuationForNonGameWorlds(bUseAudioAttenuation);
			}
		}
	}
}

void UAudioEditorSettings::ApplyAttenuationForAudioDevice(Audio::FDeviceId InDeviceID)
{
	if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
	{
		FAudioDeviceHandle Device = AudioDeviceManager->GetAudioDevice(InDeviceID);
		if (Device.IsValid())
		{
			Device->SetUseAttenuationForNonGameWorlds(bUseAudioAttenuation);
		}
	}	
}
