// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/AudioEngineSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioEngineSubsystem)

UAudioEngineSubsystem::UAudioEngineSubsystem()
	: UDynamicSubsystem()
{
}

FAudioDeviceHandle UAudioEngineSubsystem::GetAudioDeviceHandle() const
{
	const UAudioSubsystemCollectionRoot* SubsystemRoot = Cast<UAudioSubsystemCollectionRoot>(GetOuter());
	check(SubsystemRoot);

	if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
	{
		return AudioDeviceManager->GetAudioDevice(SubsystemRoot->GetAudioDeviceID());
	}

	return FAudioDeviceHandle();
}

