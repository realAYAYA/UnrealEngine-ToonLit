// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioBusSubsystem.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceManager.h"
#include "DSP/MultithreadedPatching.h"
#include "UObject/UObjectIterator.h"

std::atomic<uint32> Audio::FAudioBusKey::InstanceIdCounter = 0;

UAudioBusSubsystem::UAudioBusSubsystem()
{
}

bool UAudioBusSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void UAudioBusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogAudioMixer, Log, TEXT("Initializing Audio Bus Subsystem for audio device with ID %d"), GetMixerDevice()->DeviceID);
	InitDefaultAudioBuses();
}

void UAudioBusSubsystem::Deinitialize()
{
	UE_LOG(LogAudioMixer, Log, TEXT("Deinitializing Audio Bus Subsystem for audio device with ID %d"), GetMixerDevice() ? GetMixerDevice()->DeviceID : -1);
	ShutdownDefaultAudioBuses();
}

void UAudioBusSubsystem::StartAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InNumChannels, bool bInIsAutomatic)
{
	if (IsInGameThread())
	{
		if (ActiveAudioBuses_GameThread.Contains(InAudioBusKey))
		{
			return;
		}

		FActiveBusData BusData;
		BusData.BusKey = InAudioBusKey;
		BusData.NumChannels = InNumChannels;
		BusData.bIsAutomatic = bInIsAutomatic;

		ActiveAudioBuses_GameThread.Add(InAudioBusKey, BusData);

		FAudioThread::RunCommandOnAudioThread([this, InAudioBusKey, InNumChannels, bInIsAutomatic]()
		{
			if (Audio::FMixerSourceManager* MixerSourceManager = GetMutableSourceManager())
			{
				MixerSourceManager->StartAudioBus(InAudioBusKey, InNumChannels, bInIsAutomatic);
			}
		});
	}
	else
	{
		// If we're not the game thread, this needs to be on the game thread, so queue up a command to execute it on the game thread
		if (Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice())
		{
			MixerDevice->GameThreadMPSCCommand([this, InAudioBusKey, InNumChannels, bInIsAutomatic]
			{
				StartAudioBus(InAudioBusKey, InNumChannels, bInIsAutomatic);
			});
		}
	}
}

void UAudioBusSubsystem::StopAudioBus(Audio::FAudioBusKey InAudioBusKey)
{
	if (IsInGameThread())
	{
		if (!ActiveAudioBuses_GameThread.Contains(InAudioBusKey))
		{
			return;
		}

		ActiveAudioBuses_GameThread.Remove(InAudioBusKey);

		FAudioThread::RunCommandOnAudioThread([this, InAudioBusKey]()
		{
			if (Audio::FMixerSourceManager* MixerSourceManager = GetMutableSourceManager())
			{
				MixerSourceManager->StopAudioBus(InAudioBusKey);
			}
		});
	}
	else
	{
		// If we're not the game thread, this needs to be on the game thread, so queue up a command to execute it on the game thread
		if (Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice())
		{
			MixerDevice->GameThreadMPSCCommand([this, InAudioBusKey]
			{
				StopAudioBus(InAudioBusKey);
			});
		}
	}
}

bool UAudioBusSubsystem::IsAudioBusActive(Audio::FAudioBusKey InAudioBusKey) const
{
	if (IsInGameThread())
	{
		return ActiveAudioBuses_GameThread.Contains(InAudioBusKey);
	}

	check(IsInAudioThread());
	if (const Audio::FMixerSourceManager* MixerSourceManager = GetSourceManager())
	{
		return MixerSourceManager->IsAudioBusActive(InAudioBusKey);
	}
	return false;
}

Audio::FPatchInput UAudioBusSubsystem::AddPatchInputForAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InFrames, int32 InChannels, float InGain)
{
	Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager();
	check(SourceManager);

	Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice();
	if (!MixerDevice)
	{
		return Audio::FPatchInput();
	}

	Audio::FPatchInput PatchInput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
	SourceManager->AddPendingAudioBusConnection(InAudioBusKey, InChannels, false, PatchInput);
	return PatchInput;
}

Audio::FPatchOutputStrongPtr UAudioBusSubsystem::AddPatchOutputForAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InFrames, int32 InChannels, float InGain)
{
	Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager();
	check(SourceManager);

	Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice();
	if (!MixerDevice)
	{
		return nullptr;
	}

	Audio::FPatchOutputStrongPtr PatchOutput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
	SourceManager->AddPendingAudioBusConnection(InAudioBusKey, InChannels, false, PatchOutput);
	return PatchOutput;
}

void UAudioBusSubsystem::InitDefaultAudioBuses()
{
	if (!ensure(IsInGameThread()))
	{
		return;
	}

	if (const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>())
	{
		TArray<TStrongObjectPtr<UAudioBus>> StaleBuses = DefaultAudioBuses;
		DefaultAudioBuses.Reset();

		for (const FDefaultAudioBusSettings& BusSettings : AudioSettings->DefaultAudioBuses)
		{
			if (UObject* BusObject = BusSettings.AudioBus.TryLoad())
			{
				if (UAudioBus* AudioBus = Cast<UAudioBus>(BusObject))
				{
					const int32 NumChannels = static_cast<int32>(AudioBus->AudioBusChannels) + 1;
					StartAudioBus(Audio::FAudioBusKey(AudioBus->GetUniqueID()), NumChannels, false /* bInIsAutomatic */);

					TStrongObjectPtr<UAudioBus>AddedBus(AudioBus);
					DefaultAudioBuses.AddUnique(AddedBus);
					StaleBuses.Remove(AddedBus);
				}
			}
		}

		for (TStrongObjectPtr<UAudioBus>& Bus : StaleBuses)
		{
			if (Bus.IsValid())
			{
				StopAudioBus(Audio::FAudioBusKey(Bus->GetUniqueID()));
			}
		}
	}
	else
	{
		UE_LOG(LogAudioMixer, Error, TEXT("Failed to initialize Default Audio Buses. Audio Settings not found."));
	}
}

void UAudioBusSubsystem::ShutdownDefaultAudioBuses()
{
	if (!ensure(IsInGameThread()))
	{
		return;
	}

	for (TObjectIterator<UAudioBus> It; It; ++It)
	{
		UAudioBus* AudioBus = *It;
		if (AudioBus)
		{
			StopAudioBus(Audio::FAudioBusKey(AudioBus->GetUniqueID()));
		}
	}

	DefaultAudioBuses.Reset();
}
