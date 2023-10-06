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
	UE_LOG(LogAudioMixer, Log, TEXT("Deinitializing Audio Bus Subsystem for audio device with ID %d"), GetMixerDevice()->DeviceID);
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
	using namespace Audio;
	// This function is supporting adding audio bus patches from multiple threads (AT, ART, GT, and tasks) and is currently
	// depending on a number of places where data lives, which accounts for the complexity here.
	// This code needs a clean up to refactor everything into a true MPSC model, along with an MPSC refactor of the source manager
	// and our command queues. Once we do that we can remove the code which branches based on the thread the request is coming from. 

	Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager();
	check(SourceManager);

	Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice();
	if (!MixerDevice)
	{
		return FPatchInput();
	}

	if (IsInGameThread())
	{
		if (ActiveAudioBuses_GameThread.Find(InAudioBusKey))
		{
			FPatchInput PatchInput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
			FAudioThread::RunCommandOnAudioThread([this, PatchInput, InAudioBusKey]()
			{
				if (Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager())
				{
					SourceManager->AddPatchInputForAudioBus_AudioThread(InAudioBusKey, PatchInput);
				}
			});
			return PatchInput;
		}
		UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus with object id '%u' and instance id '%u' is not active."), InAudioBusKey.ObjectId, InAudioBusKey.InstanceId);
		return FPatchInput();
	}
	else if (IsInAudioThread())
	{
		FPatchInput PatchInput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
		SourceManager->AddPatchInputForAudioBus_AudioThread(InAudioBusKey, PatchInput);
		return PatchInput;
	}
	else if (GetMixerDevice()->IsAudioRenderingThread())
	{
		check(SourceManager);

		const int32 NumChannels = SourceManager->GetAudioBusNumChannels(InAudioBusKey);
		if (NumChannels > 0)
		{
			FPatchInput PatchInput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
			SourceManager->AddPatchInputForAudioBus(InAudioBusKey, PatchInput);
			return PatchInput;
		}

		return FPatchInput();
	}
	else
	{
		FPatchInput PatchInput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
		MixerDevice->GameThreadMPSCCommand([this, PatchInput, InAudioBusKey]() mutable
		{
			if (ActiveAudioBuses_GameThread.Find(InAudioBusKey))
			{
				FAudioThread::RunCommandOnAudioThread([this, InAudioBusKey, PatchInput = MoveTemp(PatchInput)]()
				{
					if (Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager())
					{
						SourceManager->AddPatchInputForAudioBus_AudioThread(InAudioBusKey, PatchInput);
					}
				});
			}
			else
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus with object id '%u' and instance id '%u' is not active."), InAudioBusKey.ObjectId, InAudioBusKey.InstanceId);
			}
		});
		return PatchInput;
	}
}

Audio::FPatchOutputStrongPtr UAudioBusSubsystem::AddPatchOutputForAudioBus(Audio::FAudioBusKey InAudioBusKey, int32 InFrames, int32 InChannels, float InGain)
{
	// This function is supporting adding audio bus patches from multiple threads (AT, ART, GT, and tasks) and is currently
	// depending on a number of places where data lives, which accounts for the complexity here.
	// This code needs a clean up to refactor everything into a true MPSC model, along with an MPSC refactor of the source manager
	// and our command queues. Once we do that we can remove the code which branches based on the thread the request is coming from. 

	Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager();
	check(SourceManager);

	Audio::FMixerDevice* MixerDevice = GetMutableMixerDevice();
	if (!MixerDevice)
	{
		return nullptr;
	}

	if (IsInGameThread())
	{
		if (ActiveAudioBuses_GameThread.Find(InAudioBusKey))
		{
			Audio::FPatchOutputStrongPtr PatchOutput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
			FAudioThread::RunCommandOnAudioThread([this, PatchOutput, InAudioBusKey]()
			{
				if (Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager())
				{
					SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusKey, PatchOutput);
				}
			});
			return PatchOutput;
		}
		UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus with object id '%u' and instance id '%u' is not active."), InAudioBusKey.ObjectId, InAudioBusKey.InstanceId);
		return nullptr;
	}
	else if (IsInAudioThread())
	{
		Audio::FPatchOutputStrongPtr PatchOutput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
		SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusKey, PatchOutput);
		return PatchOutput;
	}
	else if (MixerDevice->IsAudioRenderingThread())
	{
		check(SourceManager);

		const int32 NumChannels = SourceManager->GetAudioBusNumChannels(InAudioBusKey);
		if (NumChannels > 0)
		{
			Audio::FPatchOutputStrongPtr PatchOutput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
			SourceManager->AddPatchOutputForAudioBus(InAudioBusKey, PatchOutput);
			return PatchOutput;
		}

		return nullptr;
	}
	else
	{
		Audio::FPatchOutputStrongPtr PatchOutput = MixerDevice->MakePatch(InFrames, InChannels, InGain);
		MixerDevice->GameThreadMPSCCommand([this, PatchOutput, InAudioBusKey]() mutable
		{
			if (ActiveAudioBuses_GameThread.Find(InAudioBusKey))
			{
				FAudioThread::RunCommandOnAudioThread([this, InAudioBusKey, PatchOutput = MoveTemp(PatchOutput)]()
				{
					if (Audio::FMixerSourceManager* SourceManager = GetMutableSourceManager())
					{
						SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusKey, PatchOutput);
					}
				});
			}
			else
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus with object id '%u' and instance id '%u' is not active."), InAudioBusKey.ObjectId, InAudioBusKey.InstanceId);
			}
		});
		return PatchOutput;
	}
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
