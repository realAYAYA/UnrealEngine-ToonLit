// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCompressionSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioCompressionSettings)

FPlatformRuntimeAudioCompressionOverrides::FPlatformRuntimeAudioCompressionOverrides()
	: bOverrideCompressionTimes(false)
	, DurationThreshold(5.0f)
	, MaxNumRandomBranches(0)
	, SoundCueQualityIndex(0)
{
	
}

FPlatformRuntimeAudioCompressionOverrides* FPlatformRuntimeAudioCompressionOverrides::DefaultCompressionOverrides = nullptr;

// Increment this return value to force a recook on all Stream Caching assets.
// For testing, it's useful to set this to either a negative number or
// absurdly large number, to ensure you do not pollute the DDC.
int32 FPlatformAudioCookOverrides::GetStreamCachingVersion()
{
	return 5027;
}
