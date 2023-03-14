// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

namespace Audio {

	namespace EAudioMixerStreamDataFormat
	{
		enum Type
		{
			Unknown,
			Float,
			Int16,
			Unsupported
		};
	}

	/**
	 * EAudioOutputStreamState
	 * Specifies the state of the output audio stream.
	 */
	namespace EAudioOutputStreamState
	{
		enum Type
		{
			/* The audio stream is shutdown or not uninitialized. */
			Closed,
		
			/* The audio stream is open but not running. */
			Open,

			/** The audio stream is open but stopped. */
			Stopped,
		
			/** The audio output stream is stopping. */
			Stopping,

			/** The audio output stream is open and running. */
			Running,
		};
	}

	// Indicates a platform-specific format
	static FName NAME_PLATFORM_SPECIFIC(TEXT("PLATFORM_SPECIFIC"));
	static FName NAME_PROJECT_DEFINED(TEXT("PROJECT_DEFINED"));

	// Supported on all platforms:
	static FName NAME_BINKA(TEXT("BINKA"));
	static FName NAME_ADPCM(TEXT("ADPCM"));
	static FName NAME_PCM(TEXT("PCM"));

	// Not yet supported on all platforms as a selectable option so is included under "platform specific" enumeration for now. 
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_OPUS(TEXT("OPUS"));
}

struct AUDIOMIXERCORE_API FAudioPlatformSettings
{
	/** Sample rate to use on the platform for the mixing engine. Higher sample rates will incur more CPU cost. */
	int32 SampleRate;

	/** The amount of audio to compute each callback block. Lower values decrease latency but may increase CPU cost. */
	int32 CallbackBufferFrameSize;

	/** The number of buffers to keep enqueued. More buffers increases latency, but can compensate for variable compute availability in audio callbacks on some platforms. */
	int32 NumBuffers;

	/** The max number of channels (simultaneous voices) to use as the limit for this platform. If given a value of 0, it will use the value from the active Global Audio Quality Settings */
	int32 MaxChannels;

	/** The number of workers to use to compute source audio. Will only use up to the max number of sources. Will evenly divide sources to each source worker. */
	int32 NumSourceWorkers;

	static FAudioPlatformSettings GetPlatformSettings(const TCHAR* PlatformSettingsConfigFile)
	{
		FAudioPlatformSettings Settings;

		FString TempString;

		if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioSampleRate"), TempString, GEngineIni))
		{
			Settings.SampleRate = FMath::Max(FCString::Atoi(*TempString), 8000);
		}

		if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioCallbackBufferFrameSize"), TempString, GEngineIni))
		{
			Settings.CallbackBufferFrameSize = FMath::Max(FCString::Atoi(*TempString), 256);
		}

		if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumBuffersToEnqueue"), TempString, GEngineIni))
		{
			Settings.NumBuffers = FMath::Max(FCString::Atoi(*TempString), 1);
		}

		if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioMaxChannels"), TempString, GEngineIni))
		{
			Settings.MaxChannels = FMath::Max(FCString::Atoi(*TempString), 0);
		}

		if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumSourceWorkers"), TempString, GEngineIni))
		{
			Settings.NumSourceWorkers = FMath::Max(FCString::Atoi(*TempString), 0);
		}

		return Settings;
	}

	FAudioPlatformSettings()
		: SampleRate(48000)
		, CallbackBufferFrameSize(1024)
		, NumBuffers(2)
		, MaxChannels(0) // This needs to be 0 to indicate it's not overridden from the audio settings object, which is the default used on all platforms
		, NumSourceWorkers(0)
	{
	}
};
