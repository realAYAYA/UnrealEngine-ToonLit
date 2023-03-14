// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAudioExtensionPlugin.h"
#include "AudioPluginUtilities.h"
#include "AudioCompressionSettings.h"
#include "AudioStreamingCache.h"

class ENGINE_API FPlatformCompressionUtilities
{
public:
	// Returns the Duration Threshold for the current platform if it is overridden, -1.0f otherwise.
	static float GetCompressionDurationForCurrentPlatform();

	// Returns the sample rate for a given platform,
	static float GetTargetSampleRateForPlatform(ESoundwaveSampleRateSettings InSampleRateLevel = ESoundwaveSampleRateSettings::High);

	static int32 GetMaxPreloadedBranchesForCurrentPlatform();

	static int32 GetQualityIndexOverrideForCurrentPlatform();

	static void RecacheCookOverrides();

	// null platformname means to use current platform
	static const FPlatformAudioCookOverrides* GetCookOverrides(const TCHAR* PlatformName=nullptr, bool bForceRecache = false);

	static bool IsCurrentPlatformUsingStreamCaching();

	// null platformname means to use current platform
	static const FAudioStreamCachingSettings& GetStreamCachingSettingsForCurrentPlatform();

	/** This is used at runtime to initialize FCachedAudioStreamingManager. */
	static FCachedAudioStreamingManagerParams BuildCachedStreamingManagerParams();

	/** This is used at runtime in BuildCachedStreamingManagerParams, as well as cooktime in FStreamedAudioCacheDerivedDataWorker::BuildStreamedAudio to split compressed audio.  */
	static uint32 GetMaxChunkSizeForCookOverrides(const FPlatformAudioCookOverrides* InCompressionOverrides);

private:
	static const FPlatformRuntimeAudioCompressionOverrides* GetRuntimeCompressionOverridesForCurrentPlatform();
	
};
