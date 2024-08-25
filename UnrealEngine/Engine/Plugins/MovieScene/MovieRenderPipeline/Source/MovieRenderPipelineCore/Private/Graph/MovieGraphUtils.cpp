// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphUtils.h"

#include "AudioMixerDevice.h"
#include "AudioThread.h"
#include "Engine/Engine.h"

namespace UE::MovieGraph
{
	FString GetUniqueName(const TArray<FString>& InExistingNames, const FString& InBaseName)
	{
		int32 Postfix = 0;
		FString NewName = InBaseName;

		while (InExistingNames.Contains(NewName))
		{
			Postfix++;
			NewName = FString::Format(TEXT("{0} {1}"), {InBaseName, Postfix});
		}

		return NewName;
	}

	namespace Audio
	{
		FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* InWorldContextObject)
		{
			const UWorld* ThisWorld = GEngine->GetWorldFromContextObject(InWorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
			if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || (ThisWorld->GetNetMode() == NM_DedicatedServer))
			{
				return nullptr;
			}

			return ThisWorld->GetAudioDeviceRaw();
		}

		::Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* InWorldContextObject)
		{
			if (FAudioDevice* AudioDevice = GetAudioDeviceFromWorldContext(InWorldContextObject))
			{
				return static_cast<::Audio::FMixerDevice*>(AudioDevice);
			}
	
			return nullptr;
		}

		bool IsMoviePipelineAudioOutputSupported(const UObject* InWorldContextObject)
		{
			const ::Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(InWorldContextObject);
			const ::Audio::IAudioMixerPlatformInterface* AudioMixerPlatform = MixerDevice ? MixerDevice->GetAudioMixerPlatform() : nullptr;

			// If the current audio mixer is non-realtime, audio output is supported
			if (AudioMixerPlatform && AudioMixerPlatform->IsNonRealtime())
			{
				return true;
			}

			// If there is no async audio processing (e.g. we're in the editor), it's possible to create a new non-realtime audio mixer
			if (!FAudioThread::IsUsingThreadedAudio())
			{
				return true;
			}

			// Otherwise, we can't support audio output
			return false;
		}
	}
}
